// Development-only out-of-process network probe. Active only when -DigetherPrototypeProbe and -DigetherPrototypeProbeDir are given.

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "InputKeyEventArgs.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Components/CapsuleComponent.h"
#include "Prototype/DigetherPrototypeGameState.h"
#include "Prototype/DigetherPrototypePlayerState.h"
#include "Prototype/DigetherPrototypeTypes.h"
#include "Prototype/World/DigetherPrototypeWorld.h"
#include "Prototype/World/DigetherPrototypeMineScene.h"
#include "Prototype/World/DigetherPrototypePortal.h"
#include "Prototype/World/DigetherExcavationTerrain.h"
#include "Prototype/Player/DigetherPrototypePlayerController.h"
#include "Prototype/Player/DigetherPrototypeCharacter.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/DateTime.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogDigetherPrototypeProbe, Log, All);

namespace DigetherPrototypeProbe
{
	constexpr double DefaultMaxSeconds = 180.0;
	constexpr double MaxAllowedSeconds = 600.0;
	constexpr double ControlReadInterval = 0.1;
	constexpr double StateWriteInterval = 0.2;
	constexpr double MaxMoveSeconds = 2.0;

	const TCHAR* const AllowedRoles[] = { TEXT("Host"), TEXT("Client1"), TEXT("Client2"), TEXT("Client3"), TEXT("Client3Reconnect"), TEXT("NewJoin") };

	const TCHAR* RoleToString(ENetRole Role)
	{
		switch (Role)
		{
		case ROLE_Authority: return TEXT("Authority");
		case ROLE_AutonomousProxy: return TEXT("AutonomousProxy");
		case ROLE_SimulatedProxy: return TEXT("SimulatedProxy");
		default: return TEXT("None");
		}
	}

	const TCHAR* NetModeToString(ENetMode Mode)
	{
		switch (Mode)
		{
		case NM_Standalone: return TEXT("Standalone");
		case NM_DedicatedServer: return TEXT("DedicatedServer");
		case NM_ListenServer: return TEXT("ListenServer");
		case NM_Client: return TEXT("Client");
		default: return TEXT("Unknown");
		}
	}

	UWorld* FindGameWorld()
	{
		if (GEngine)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::Game && Context.World())
				{
					return Context.World();
				}
			}
		}
		return nullptr;
	}

	ACharacter* FindLocalCharacter(UWorld* World)
	{
		APlayerController* Controller = (GEngine && World) ? GEngine->GetFirstLocalPlayerController(World) : nullptr;
		ACharacter* Character = Controller ? Cast<ACharacter>(Controller->GetPawn()) : nullptr;
		return (Character && Character->IsLocallyControlled()) ? Character : nullptr;
	}

	double UtcMilliseconds()
	{
		const FDateTime Now = FDateTime::UtcNow();
		return static_cast<double>(Now.ToUnixTimestamp()) * 1000.0 + Now.GetMillisecond();
	}

	bool SerializeCondensed(const TSharedRef<FJsonObject>& Object, FString& OutJson)
	{
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJson);
		const bool bOk = FJsonSerializer::Serialize(Object, Writer);
		Writer->Close();
		return bOk;
	}

	class FProbe
	{
	public:
		struct FHeldInputKey
		{
			TWeakObjectPtr<APlayerController> Controller;
			FInputDeviceId InputDevice = INPUTDEVICEID_NONE;
		};

		~FProbe()
		{
			ReleaseHeldKeys();
		}

		FString Role = TEXT("Invalid");
		FString StatePath;
		FString HistoryPath;
		FString ControlPath;
		FString StateTmpPath;
		double MaxSeconds = DefaultMaxSeconds;
		double StartTime = 0.0;
		double NextControlRead = 0.0;
		double NextStateWrite = 0.0;
		int64 SampleSequence = 0;
		int64 LastCommandSequence = 0;
		FString LastCommandStatus = TEXT("None");
		FString LastCommandReason;
		FString ControlError;
		FString LastControlText;
		FString Status = TEXT("Running");
		FString FailureReason;
		bool bMoveActive = false;
		double StopJumpAt = 0.0;
		double MoveEndTime = 0.0;
		FVector MoveDirection = FVector::ZeroVector;
		int32 InjectedFrames = 0;
		FString MoveTarget;
		TWeakObjectPtr<AActor> ScheduledTarget;
		TWeakObjectPtr<ADigetherPrototypePlayerController> ScheduledController;
		int64 ScheduledSequence = 0;
		double ScheduledAtUtcMs = 0.0;
		double ScheduledFiredUtcMs = 0.0;
		FString ScheduledStatus = TEXT("None");
		FString ScheduledReason;
		FString ScheduledTargetName;
		bool bScheduledPending = false;
		bool bScheduledAlternate = false;
		bool bScheduledDig = false;
		bool bPendingQuit = false;
		bool bExitRequested = false;
		bool bFailed = false;
		FTSTicker::FDelegateHandle TickHandle;
		FDelegateHandle PreExitHandle;
		TMap<FKey, FHeldInputKey> HeldKeys;

		bool Configure(const FString& InRole, const FString& InDir, const FString& MaxSecondsText, bool bHasMaxSeconds, FString& OutError)
		{
			StartTime = FPlatformTime::Seconds();

			FString Dir = InDir;
			FPaths::NormalizeDirectoryName(Dir);
			if (FPaths::IsRelative(Dir) || !FPaths::CollapseRelativeDirectories(Dir) || Dir.Contains(TEXT("..")))
			{
				OutError = TEXT("DigetherPrototypeProbeDir must be an absolute path without '..'");
				return false;
			}
			StatePath = FPaths::Combine(Dir, TEXT("state.json"));
			StateTmpPath = FPaths::Combine(Dir, TEXT("state.json.tmp"));
			HistoryPath = FPaths::Combine(Dir, TEXT("history.jsonl"));
			ControlPath = FPaths::Combine(Dir, TEXT("control.json"));

			bool bRoleOk = false;
			for (const TCHAR* Allowed : AllowedRoles)
			{
				bRoleOk |= InRole.Equals(Allowed, ESearchCase::CaseSensitive);
			}
			if (!bRoleOk)
			{
				OutError = FString::Printf(TEXT("unsupported probe role '%s'"), *InRole);
				return false;
			}
			Role = InRole;

			if (bHasMaxSeconds)
			{
				if (!FCString::IsNumeric(*MaxSecondsText))
				{
					OutError = TEXT("DigetherPrototypeProbeMaxSeconds is not numeric");
					return false;
				}
				MaxSeconds = FCString::Atod(*MaxSecondsText);
				if (!FMath::IsFinite(MaxSeconds) || MaxSeconds <= 0.0 || MaxSeconds > MaxAllowedSeconds)
				{
					OutError = FString::Printf(TEXT("DigetherPrototypeProbeMaxSeconds must be in (0, %.0f]"), MaxAllowedSeconds);
					return false;
				}
			}

			if (!IFileManager::Get().MakeDirectory(*Dir, true))
			{
				OutError = TEXT("cannot create probe directory");
				return false;
			}
			return true;
		}

		bool Tick()
		{
			if (bFailed)
			{
				return false;
			}

			const double Now = FPlatformTime::Seconds();
			UWorld* World = FindGameWorld();
			if (StopJumpAt > 0.0 && Now >= StopJumpAt) { if (ACharacter* Character = FindLocalCharacter(World)) Character->StopJumping(); StopJumpAt = 0.0; }

			if (!bExitRequested && Now - StartTime >= MaxSeconds)
			{
				ReleaseHeldKeys();
				Status = TEXT("Expired");
				WriteState(World);
				RequestExitOnce();
				return false;
			}

			if (!bExitRequested && Now >= NextControlRead)
			{
				NextControlRead = Now + ControlReadInterval;
				ReadControl(World, Now);
			}

			if (bScheduledPending && !bExitRequested && UtcMilliseconds() >= ScheduledAtUtcMs)
			{
				bScheduledPending = false;
				ScheduledFiredUtcMs = UtcMilliseconds();
				AActor* Target = ScheduledTarget.Get();
				ADigetherPrototypePlayerController* Controller = ScheduledController.Get();
				if ((!bScheduledDig && !Target) || !Controller || !Controller->IsLocalController() || Controller->GetWorld() != World)
				{
					ScheduledStatus = TEXT("Rejected");
					ScheduledReason = TEXT("prepared target or local controller disappeared before scheduled RPC");
				}
				else
				{
					if (bScheduledDig) Controller->ServerDig();
					else Controller->ServerInteract(Target, bScheduledAlternate);
					ScheduledStatus = TEXT("Sent");
					ScheduledReason = TEXT("RPC invoked; server outcome is checked separately");
				}
				ScheduledTarget.Reset();
				ScheduledController.Reset();
				NextStateWrite = 0.0;
			}

			if (bMoveActive && !MoveTarget.IsEmpty() && !bExitRequested)
			{
				ACharacter* Character = FindLocalCharacter(World);
				AActor* Target = FindTarget(World, MoveTarget);
				if (!Character || !Target || Now >= MoveEndTime)
				{
					bMoveActive = false;
					MoveTarget.Empty();
				}
				else
				{
					const FVector Delta = Target->GetActorLocation() - Character->GetActorLocation();
					if (Delta.Size2D() <= 180.f) { bMoveActive = false; MoveTarget.Empty(); }
					else { Character->AddMovementInput(FVector(Delta.X, Delta.Y, 0.f).GetSafeNormal(), 1.f); ++InjectedFrames; }
				}
			}
			else if (bMoveActive && !bExitRequested)
			{
				if (Now >= MoveEndTime)
				{
					bMoveActive = false;
				}
				else if (ACharacter* Character = FindLocalCharacter(World))
				{
					Character->AddMovementInput(MoveDirection, 1.0f);
					++InjectedFrames;
				}
			}

			if (Now >= NextStateWrite)
			{
				NextStateWrite = Now + StateWriteInterval;
				if (!WriteState(World))
				{
					return false;
				}
			}

			if (bPendingQuit)
			{
				RequestExitOnce();
			}
			return !bFailed;
		}

		void Fail(const FString& Reason)
		{
			if (bFailed)
			{
				return;
			}
			bFailed = true;
			ReleaseHeldKeys();
			Status = TEXT("Failed");
			FailureReason = Reason;
			UE_LOG(LogDigetherPrototypeProbe, Error, TEXT("Network probe failed: %s"), *Reason);
			FString Ignored;
			TryWriteState(FindGameWorld(), Ignored);
			RequestExitOnce();
		}

		void OnPreExit()
		{
			ReleaseHeldKeys();
			FTSTicker::RemoveTicker(TickHandle);
			TickHandle.Reset();
			FCoreDelegates::OnPreExit.Remove(PreExitHandle);
			PreExitHandle.Reset();

			if (!bFailed)
			{
				if (Status == TEXT("Running"))
				{
					Status = TEXT("Exiting");
				}
				FString Error;
				if (!TryWriteState(FindGameWorld(), Error))
				{
					UE_LOG(LogDigetherPrototypeProbe, Error, TEXT("Final probe state write failed: %s"), *Error);
				}
			}
		}

	private:
		void ReleaseHeldKeys()
		{
			// Release only keys pressed by this probe. Clear ownership first because
			// mouse releases can dispatch gameplay callbacks through InputKey.
			TMap<FKey, FHeldInputKey> KeysToRelease = MoveTemp(HeldKeys);
			HeldKeys.Empty();
			for (const TPair<FKey, FHeldInputKey>& Entry : KeysToRelease)
			{
				APlayerController* Controller = Entry.Value.Controller.Get();
				if (Controller && Controller->IsLocalController() && Controller->GetLocalPlayer())
				{
					Controller->InputKey(FInputKeyEventArgs::CreateSimulated(
						Entry.Key, IE_Released, 0.f, -1, Entry.Value.InputDevice));
				}
			}
		}

		void RequestExitOnce()
		{
			if (!bExitRequested)
			{
				ReleaseHeldKeys();
				bExitRequested = true;
				FPlatformMisc::RequestExit(false, TEXT("DigetherPrototypeProbe"));
			}
		}

		static AActor* FindTarget(UWorld* World, const FString& Name)
		{
			if (!World) return nullptr;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->GetName() == Name && It->GetClass()->ImplementsInterface(UDigetherPrototypeInteractable::StaticClass())) return *It;
			}
			return nullptr;
		}

		bool WriteState(UWorld* World)
		{
			FString Error;
			if (!TryWriteState(World, Error))
			{
				Fail(Error);
				return false;
			}
			return true;
		}

		bool TryWriteState(UWorld* World, FString& OutError)
		{
			if (StatePath.IsEmpty())
			{
				OutError = TEXT("probe directory is not configured");
				return false;
			}

			++SampleSequence;
			FString Json;
			if (!SerializeCondensed(BuildState(World), Json))
			{
				OutError = TEXT("state JSON serialization failed");
				return false;
			}

			if (!FFileHelper::SaveStringToFile(Json + TEXT("\n"), *HistoryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append | FILEWRITE_AllowRead))
			{
				OutError = TEXT("history.jsonl append failed");
				return false;
			}
			if (!FFileHelper::SaveStringToFile(Json, *StateTmpPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_AllowRead))
			{
				OutError = TEXT("state.json.tmp write failed");
				return false;
			}

			// A concurrent reader can briefly block replacing the destination on Windows.
			bool bMoved = false;
			for (int32 Attempt = 0; Attempt < 3 && !bMoved; ++Attempt)
			{
				bMoved = IFileManager::Get().Move(*StatePath, *StateTmpPath, true, true, false, true);
				if (!bMoved)
				{
					FPlatformProcess::Sleep(0.01f);
				}
			}
			if (!bMoved)
			{
				OutError = TEXT("state.json atomic replace failed");
				return false;
			}
			return true;
		}

		TSharedRef<FJsonObject> BuildState(UWorld* World) const
		{
			TSharedRef<FJsonObject> State = MakeShared<FJsonObject>();
			State->SetStringField(TEXT("role"), Role);
			TArray<TSharedPtr<FJsonValue>> HeldInputNames;
			for (const TPair<FKey, FHeldInputKey>& Entry : HeldKeys)
			{
				HeldInputNames.Add(MakeShared<FJsonValueString>(Entry.Key.GetFName().ToString()));
			}
			State->SetArrayField(TEXT("heldKeys"), HeldInputNames);
			State->SetNumberField(TEXT("sampleSequence"), static_cast<double>(SampleSequence));
			State->SetStringField(TEXT("utc"), FDateTime::UtcNow().ToIso8601());
			State->SetStringField(TEXT("probeStatus"), Status);
			if (FailureReason.IsEmpty())
			{
				State->SetField(TEXT("probeFailure"), MakeShared<FJsonValueNull>());
			}
			else
			{
				State->SetStringField(TEXT("probeFailure"), FailureReason);
			}

			APlayerController* LocalController = (GEngine && World) ? GEngine->GetFirstLocalPlayerController(World) : nullptr;
			APlayerState* LocalState = LocalController ? LocalController->PlayerState.Get() : nullptr;
			if (const ADigetherPrototypePlayerController* PrototypeController = Cast<ADigetherPrototypePlayerController>(LocalController))
			{
				TSharedRef<FJsonObject> DepositInput = MakeShared<FJsonObject>();
				DepositInput->SetBoolField(TEXT("open"), PrototypeController->IsDepositInputOpen());
				DepositInput->SetStringField(TEXT("text"), PrototypeController->GetDepositInputText());
				DepositInput->SetStringField(TEXT("error"), PrototypeController->GetDepositInputError());
				DepositInput->SetNumberField(TEXT("sessionId"), PrototypeController->GetDepositSessionId());
				DepositInput->SetBoolField(TEXT("moveInputIgnored"), PrototypeController->IsMoveInputIgnored());
				State->SetObjectField(TEXT("depositInput"), DepositInput);
				State->SetStringField(TEXT("lastServerMessage"), PrototypeController->GetLastServerMessage());
			}

			State->SetBoolField(TEXT("hasWorld"), World != nullptr);
			State->SetBoolField(TEXT("hasBegunPlay"), World && World->HasBegunPlay());
			if (World)
			{
				State->SetStringField(TEXT("world"), World->GetOutermost()->GetName());
				State->SetStringField(TEXT("netMode"), NetModeToString(World->GetNetMode()));
				int32 GameModeCount = 0;
				for (TActorIterator<AGameModeBase> It(World); It; ++It)
				{
					++GameModeCount;
				}
				State->SetNumberField(TEXT("authGameModeCount"), GameModeCount);
			}
			else
			{
				State->SetField(TEXT("world"), MakeShared<FJsonValueNull>());
				State->SetField(TEXT("netMode"), MakeShared<FJsonValueNull>());
				State->SetNumberField(TEXT("authGameModeCount"), 0);
			}

			if (LocalState)
			{
				State->SetNumberField(TEXT("localPlayerId"), LocalState->GetPlayerId());
			}
			else
			{
				State->SetField(TEXT("localPlayerId"), MakeShared<FJsonValueNull>());
			}

			const ADigetherPrototypeElevatorCab* ObservedCab = nullptr;
			if (World) for (TActorIterator<ADigetherPrototypeElevatorCab> It(World); It; ++It) { ObservedCab = *It; break; }
			const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
			State->SetBoolField(TEXT("hasGameState"), GameState != nullptr);
			State->SetNumberField(TEXT("playerCount"), GameState ? GameState->PlayerArray.Num() : 0);

			TArray<TSharedPtr<FJsonValue>> Players;
			if (GameState)
			{
				for (APlayerState* PlayerState : GameState->PlayerArray)
				{
					if (!PlayerState)
					{
						continue;
					}
					const APawn* Pawn = PlayerState->GetPawn();
					TSharedRef<FJsonObject> Player = MakeShared<FJsonObject>();
					Player->SetNumberField(TEXT("playerId"), PlayerState->GetPlayerId());
					Player->SetBoolField(TEXT("hasPawn"), Pawn != nullptr);
					if (Pawn)
					{
						Player->SetStringField(TEXT("pawnName"), Pawn->GetName());
						Player->SetBoolField(TEXT("inCab"), ObservedCab && ObservedCab->ContainsLocation(Pawn->GetActorLocation()));
						const AActor* Parent = Pawn->GetAttachParentActor();
						Player->SetStringField(TEXT("attachParentClass"), Parent ? Parent->GetClass()->GetName() : TEXT(""));
						Player->SetBoolField(TEXT("attachParentIsCab"), Parent && Parent->IsA<ADigetherPrototypeElevatorCab>());
					}
					Player->SetBoolField(TEXT("isLocal"), PlayerState == LocalState);
					if (Pawn)
					{
						const FVector Location = Pawn->GetActorLocation();
						Player->SetStringField(TEXT("localRole"), RoleToString(Pawn->GetLocalRole()));
						Player->SetStringField(TEXT("remoteRole"), RoleToString(Pawn->GetRemoteRole()));
						Player->SetNumberField(TEXT("x"), Location.X);
						Player->SetNumberField(TEXT("y"), Location.Y);
						Player->SetNumberField(TEXT("z"), Location.Z);
					}
					else
					{
						Player->SetField(TEXT("localRole"), MakeShared<FJsonValueNull>());
						Player->SetField(TEXT("remoteRole"), MakeShared<FJsonValueNull>());
						Player->SetField(TEXT("x"), MakeShared<FJsonValueNull>());
						Player->SetField(TEXT("y"), MakeShared<FJsonValueNull>());
						Player->SetField(TEXT("z"), MakeShared<FJsonValueNull>());
					}
					Players.Add(MakeShared<FJsonValueObject>(Player));
				}
			}
			State->SetArrayField(TEXT("players"), Players);

			const ADigetherPrototypeGameState* Prototype = World ? World->GetGameState<ADigetherPrototypeGameState>() : nullptr;
			State->SetBoolField(TEXT("hasPrototype"), Prototype != nullptr);
			if (Prototype)
			{
				const FPrototypePublicState& P = Prototype->PublicState;
				TSharedRef<FJsonObject> Public = MakeShared<FJsonObject>();
				Public->SetNumberField(TEXT("phase"), static_cast<int32>(P.Phase));
				Public->SetNumberField(TEXT("day"), P.Day);
				Public->SetNumberField(TEXT("quotaTarget"), P.QuotaTarget);
				Public->SetNumberField(TEXT("quotaProgress"), P.QuotaProgress);
				Public->SetNumberField(TEXT("runNumber"), P.RunNumber);
				Public->SetStringField(TEXT("runId"), P.RunId.ToString());
				Public->SetNumberField(TEXT("elevatorTrips"), P.ElevatorTrips);
				Public->SetNumberField(TEXT("elevatorState"), static_cast<int32>(P.ElevatorState));
				Public->SetNumberField(TEXT("elevatorProgress"), P.ElevatorProgress);
				Public->SetNumberField(TEXT("elevatorWeightStage"), P.ElevatorWeightStage);
				Public->SetNumberField(TEXT("remainingSeconds"), P.RemainingSeconds);
				State->SetObjectField(TEXT("public"), Public);
			}
			TArray<TSharedPtr<FJsonValue>> PrivateViews;
			if (GameState) for (const APlayerState* BasePlayer : GameState->PlayerArray)
			{
				const ADigetherPrototypePlayerState* P = Cast<ADigetherPrototypePlayerState>(BasePlayer);
				if (!P) continue;
				TSharedRef<FJsonObject> View = MakeShared<FJsonObject>();
				View->SetNumberField(TEXT("playerId"), P->GetPlayerId());
				View->SetBoolField(TEXT("isLocal"), P == LocalState);
				View->SetStringField(TEXT("fixtureId"), P->FixtureId);
				View->SetNumberField(TEXT("money"), P->PrivateState.Money);
				View->SetNumberField(TEXT("health"), P->PrivateState.Health);
				View->SetNumberField(TEXT("woodCount"), P->PrivateState.WoodCount);
				View->SetNumberField(TEXT("burialLevel"), P->PrivateState.BurialLevel);
				View->SetNumberField(TEXT("holdProgress"), P->PrivateState.HoldProgress);
				View->SetStringField(TEXT("message"), P->PrivateState.Message);
				View->SetNumberField(TEXT("life"), static_cast<int32>(P->PrivateState.Life));
				View->SetNumberField(TEXT("lastPayout"), P->PrivateState.LastPayout);
				TArray<TSharedPtr<FJsonValue>> Items;
				for (const FPrototypeItem& Item : P->PrivateState.Items)
				{
					TSharedRef<FJsonObject> I = MakeShared<FJsonObject>();
					I->SetStringField(TEXT("id"), Item.Id.ToString());
					I->SetNumberField(TEXT("value"), Item.Value);
					I->SetNumberField(TEXT("weightKg"), Item.WeightKg);
					I->SetBoolField(TEXT("twoHanded"), Item.bTwoHanded);
					I->SetBoolField(TEXT("equipment"), Item.bEquipment);
					I->SetBoolField(TEXT("emergencyDevice"), Item.bEmergencyDevice);
					I->SetBoolField(TEXT("hidden"), Item.bHidden);
					Items.Add(MakeShared<FJsonValueObject>(I));
				}
				View->SetArrayField(TEXT("items"), Items);
				PrivateViews.Add(MakeShared<FJsonValueObject>(View));
			}
			State->SetArrayField(TEXT("privateViews"), PrivateViews);
			TArray<TSharedPtr<FJsonValue>> WorldPawns;
			if (World) for (TActorIterator<APawn> It(World); It; ++It)
			{
				TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
				P->SetStringField(TEXT("name"), It->GetName());
				P->SetBoolField(TEXT("hasController"), It->GetController() != nullptr);
				const FVector Loc = It->GetActorLocation();
				P->SetNumberField(TEXT("x"), Loc.X);
				P->SetNumberField(TEXT("y"), Loc.Y);
				P->SetNumberField(TEXT("z"), Loc.Z);
				WorldPawns.Add(MakeShared<FJsonValueObject>(P));
			}
			State->SetArrayField(TEXT("worldPawns"), WorldPawns);
			TArray<TSharedPtr<FJsonValue>> Targets;
			if (World) for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (!It->GetClass()->ImplementsInterface(UDigetherPrototypeInteractable::StaticClass())) continue;
				TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
				Target->SetStringField(TEXT("name"), It->GetName());
				Target->SetStringField(TEXT("class"), It->GetClass()->GetName());
				const TCHAR* SemanticType = It->IsA<ADigetherPrototypeStation>() ? TEXT("Station")
					: It->IsA<ADigetherPrototypeDigCell>() ? TEXT("DigCell")
					: It->IsA<ADigetherPrototypePickup>() ? TEXT("Pickup")
					: It->IsA<ADigetherPrototypePortal>() ? TEXT("Portal") : TEXT("Other");
				Target->SetStringField(TEXT("semanticType"), SemanticType);
				TArray<TSharedPtr<FJsonValue>> Tags;
				for (const FName& Tag : It->Tags) Tags.Add(MakeShared<FJsonValueString>(Tag.ToString()));
				Target->SetArrayField(TEXT("tags"), Tags);
				if (const ADigetherPrototypeStation* Station = Cast<ADigetherPrototypeStation>(*It))
					Target->SetNumberField(TEXT("kind"), static_cast<int32>(Station->Kind));
				if (const ADigetherPrototypePickup* Pickup = Cast<ADigetherPrototypePickup>(*It))
				{
					Target->SetBoolField(TEXT("reserved"), Pickup->bReserved);
					Target->SetBoolField(TEXT("inCab"), ObservedCab && ObservedCab->ContainsLocation(Pickup->GetActorLocation()));
					const AActor* Parent = Pickup->GetAttachParentActor();
					Target->SetStringField(TEXT("attachParentClass"), Parent ? Parent->GetClass()->GetName() : TEXT(""));
					Target->SetBoolField(TEXT("attachParentIsCab"), Parent && Parent->IsA<ADigetherPrototypeElevatorCab>());
					if (World->GetNetMode() != NM_Client)
					{
						Target->SetStringField(TEXT("authorityItemId"), Pickup->StoredItem.Id.ToString());
						Target->SetNumberField(TEXT("authorityValue"), Pickup->StoredItem.Value);
					}
				}
				if (const ADigetherPrototypeDigCell* Cell = Cast<ADigetherPrototypeDigCell>(*It))
				{
					Target->SetBoolField(TEXT("excavated"), Cell->IsExcavated());
					Target->SetNumberField(TEXT("supportStrength"), Cell->GetSupportStrength());
				}
				const FVector Position = It->GetActorLocation();
				Target->SetNumberField(TEXT("x"), Position.X);
				Target->SetNumberField(TEXT("y"), Position.Y);
				Target->SetNumberField(TEXT("z"), Position.Z);
				Targets.Add(MakeShared<FJsonValueObject>(Target));
			}
			State->SetArrayField(TEXT("targets"), Targets);
			TArray<TSharedPtr<FJsonValue>> Terrains;
			if (World) for (TActorIterator<ADigetherExcavationTerrain> It(World); It; ++It)
			{
				TSharedRef<FJsonObject> Terrain = MakeShared<FJsonObject>();
				Terrain->SetStringField(TEXT("name"), It->GetName());
				TArray<TSharedPtr<FJsonValue>> Tags;
				for (const FName& Tag : It->Tags) Tags.Add(MakeShared<FJsonValueString>(Tag.ToString()));
				Terrain->SetArrayField(TEXT("tags"), Tags);
				const FVector Location = It->GetActorLocation();
				const FRotator Rotation = It->GetActorRotation();
				const FVector Scale = It->GetActorScale3D();
				Terrain->SetNumberField(TEXT("x"), Location.X); Terrain->SetNumberField(TEXT("y"), Location.Y); Terrain->SetNumberField(TEXT("z"), Location.Z);
				Terrain->SetNumberField(TEXT("pitch"), Rotation.Pitch); Terrain->SetNumberField(TEXT("yaw"), Rotation.Yaw); Terrain->SetNumberField(TEXT("roll"), Rotation.Roll);
				Terrain->SetNumberField(TEXT("scaleX"), Scale.X); Terrain->SetNumberField(TEXT("scaleY"), Scale.Y); Terrain->SetNumberField(TEXT("scaleZ"), Scale.Z);
				Terrain->SetNumberField(TEXT("hash"), It->GetTerrainHash());
				Terrain->SetNumberField(TEXT("revision"), It->GetTerrainRevision());
				Terrain->SetBoolField(TEXT("hasSupport"), It->HasSupport());
				Terrain->SetNumberField(TEXT("supportStrength"), It->GetSupportStrength());
				Terrains.Add(MakeShared<FJsonValueObject>(Terrain));
			}
			State->SetArrayField(TEXT("terrains"), Terrains);
			TArray<TSharedPtr<FJsonValue>> Traces;
			if (World)
			{
				const FVector Starts[] = { FVector(4200, 0, 200), FVector(5000, 0, 200) };
				const FVector Ends[] = { FVector(4200, 0, -300), FVector(5900, 0, 200) };
				const TCHAR* Names[] = { TEXT("floor"), TEXT("wall") };
				for (int32 Index = 0; Index < 2; ++Index)
				{
					FHitResult Hit;
					FCollisionQueryParams Params(SCENE_QUERY_STAT(PrototypeProbeTerrain), false);
					// Exclude pawns so movement through a sample does not masquerade as terrain.
					for (TActorIterator<APawn> Pawn(World); Pawn; ++Pawn) Params.AddIgnoredActor(*Pawn);
					const bool bHit = World->LineTraceSingleByChannel(Hit, Starts[Index], Ends[Index], ECC_Visibility, Params);
					TSharedRef<FJsonObject> Trace = MakeShared<FJsonObject>();
					Trace->SetStringField(TEXT("name"), Names[Index]); Trace->SetBoolField(TEXT("hit"), bHit);
					Trace->SetStringField(TEXT("actor"), bHit && Hit.GetActor() ? Hit.GetActor()->GetName() : TEXT(""));
					Trace->SetBoolField(TEXT("terrain"), bHit && Hit.GetActor() && Hit.GetActor()->IsA<ADigetherExcavationTerrain>());
					if (bHit) { Trace->SetNumberField(TEXT("x"), Hit.ImpactPoint.X); Trace->SetNumberField(TEXT("y"), Hit.ImpactPoint.Y); Trace->SetNumberField(TEXT("z"), Hit.ImpactPoint.Z); Trace->SetNumberField(TEXT("distance"), Hit.Distance); Trace->SetNumberField(TEXT("normalX"), Hit.ImpactNormal.X); Trace->SetNumberField(TEXT("normalY"), Hit.ImpactNormal.Y); Trace->SetNumberField(TEXT("normalZ"), Hit.ImpactNormal.Z); }
					Traces.Add(MakeShared<FJsonValueObject>(Trace));
				}
			}
			State->SetArrayField(TEXT("terrainTraces"), Traces);
			if (ACharacter* Character = FindLocalCharacter(World))
			{
				FVector Eye; FRotator EyeRotation; Character->GetActorEyesViewPoint(Eye, EyeRotation);
				TSharedRef<FJsonObject> View = MakeShared<FJsonObject>();
				View->SetNumberField(TEXT("x"), Eye.X); View->SetNumberField(TEXT("y"), Eye.Y); View->SetNumberField(TEXT("z"), Eye.Z);
				if (LocalController) { const FRotator Aim = LocalController->GetControlRotation(); View->SetNumberField(TEXT("pitch"), Aim.Pitch); View->SetNumberField(TEXT("yaw"), Aim.Yaw); }
				State->SetObjectField(TEXT("localView"), View);
				const auto DescribeHit = [](const FHitResult& Hit, bool bHit)
				{
					TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
					Result->SetBoolField(TEXT("hit"), bHit);
					if (bHit)
					{
						Result->SetStringField(TEXT("actor"), Hit.GetActor() ? Hit.GetActor()->GetName() : TEXT(""));
						Result->SetStringField(TEXT("actorClass"), Hit.GetActor() ? Hit.GetActor()->GetClass()->GetName() : TEXT(""));
						Result->SetStringField(TEXT("component"), Hit.GetComponent() ? Hit.GetComponent()->GetName() : TEXT(""));
						Result->SetBoolField(TEXT("startPenetrating"), Hit.bStartPenetrating);
						Result->SetNumberField(TEXT("distance"), Hit.Distance);
						Result->SetNumberField(TEXT("x"), Hit.ImpactPoint.X); Result->SetNumberField(TEXT("y"), Hit.ImpactPoint.Y); Result->SetNumberField(TEXT("z"), Hit.ImpactPoint.Z);
						Result->SetNumberField(TEXT("normalX"), Hit.ImpactNormal.X); Result->SetNumberField(TEXT("normalY"), Hit.ImpactNormal.Y); Result->SetNumberField(TEXT("normalZ"), Hit.ImpactNormal.Z);
					}
					return Result;
				};
				FCollisionQueryParams Params(SCENE_QUERY_STAT(PrototypeProbeDiagnostics), false, Character);
				FHitResult FloorHit;
				const FVector PawnLocation = Character->GetActorLocation();
				const bool bFloorHit = World->LineTraceSingleByChannel(FloorHit, PawnLocation + FVector(0.f, 0.f, 20.f),
					PawnLocation - FVector(0.f, 0.f, 400.f), ECC_Visibility, Params);
				State->SetObjectField(TEXT("localFloor"), DescribeHit(FloorHit, bFloorHit));
				if (const ADigetherPrototypeCharacter* PrototypeCharacter = Cast<ADigetherPrototypeCharacter>(Character))
				{
					State->SetBoolField(TEXT("headlampOn"), PrototypeCharacter->IsHeadlampOn());
					FVector AimOrigin;
					if (LocalController && PrototypeCharacter->GetPrototypeAimOrigin(AimOrigin))
					{
						FHitResult Hit;
						const float Reach = FMath::Clamp(PrototypeCharacter->InteractTraceDistance, 1.f, 1000.f);
						const bool bHit = World->LineTraceSingleByChannel(Hit, AimOrigin, AimOrigin + LocalController->GetControlRotation().Vector() * Reach, ECC_Visibility, Params);
						TSharedRef<FJsonObject> AimTrace = DescribeHit(Hit, bHit);
						AimTrace->SetNumberField(TEXT("originX"), AimOrigin.X); AimTrace->SetNumberField(TEXT("originY"), AimOrigin.Y); AimTrace->SetNumberField(TEXT("originZ"), AimOrigin.Z); AimTrace->SetNumberField(TEXT("reach"), Reach);
						State->SetObjectField(TEXT("aimTrace"), AimTrace);
					}
				}
				if (bMoveActive && MoveTarget.IsEmpty() && !MoveDirection.IsNearlyZero() && Character->GetCapsuleComponent())
				{
					const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
					const FVector Start = Capsule->GetComponentLocation();
					FHitResult Hit;
					const bool bHit = World->SweepSingleByChannel(Hit, Start, Start + MoveDirection * 100.f, FQuat::Identity, ECC_Pawn,
						FCollisionShape::MakeCapsule(FMath::Max(1.f, Capsule->GetScaledCapsuleRadius() - 1.f), FMath::Max(1.f, Capsule->GetScaledCapsuleHalfHeight() - 2.f)), Params);
					TSharedRef<FJsonObject> Sweep = DescribeHit(Hit, bHit);
					Sweep->SetNumberField(TEXT("directionX"), MoveDirection.X); Sweep->SetNumberField(TEXT("directionY"), MoveDirection.Y);
					State->SetObjectField(TEXT("movementSweep"), Sweep);
				}
			}
			TArray<TSharedPtr<FJsonValue>> Cabs;
			if (World) for (TActorIterator<ADigetherPrototypeElevatorCab> It(World); It; ++It)
			{
				TSharedRef<FJsonObject> Cab = MakeShared<FJsonObject>();
				Cab->SetStringField(TEXT("name"), It->GetName());
				const FVector Pos = It->GetActorLocation();
				Cab->SetNumberField(TEXT("x"), Pos.X);
				Cab->SetNumberField(TEXT("y"), Pos.Y);
				Cab->SetNumberField(TEXT("z"), Pos.Z);
				Cabs.Add(MakeShared<FJsonValueObject>(Cab));
			}
			State->SetArrayField(TEXT("cabs"), Cabs);
			TArray<TSharedPtr<FJsonValue>> LayoutSeeds;
			if (World) for (TActorIterator<ADigetherPrototypeWorldBuilder> It(World); It; ++It)
				LayoutSeeds.Add(MakeShared<FJsonValueNumber>(It->TestSeed));
			State->SetArrayField(TEXT("layoutSeeds"), LayoutSeeds);
			int32 MineSceneCount = 0;
			if (World) for (TActorIterator<ADigetherPrototypeMineScene> It(World); It; ++It) ++MineSceneCount;
			State->SetNumberField(TEXT("mineSceneCount"), MineSceneCount);

			State->SetNumberField(TEXT("scheduledSequence"), static_cast<double>(ScheduledSequence));
			State->SetStringField(TEXT("scheduledStatus"), ScheduledStatus);
			State->SetStringField(TEXT("scheduledReason"), ScheduledReason);
			State->SetStringField(TEXT("scheduledTarget"), ScheduledTargetName);
			State->SetNumberField(TEXT("scheduledAtUtcMs"), ScheduledAtUtcMs);
			State->SetNumberField(TEXT("scheduledFiredUtcMs"), ScheduledFiredUtcMs);
			State->SetNumberField(TEXT("lastCommandSequence"), static_cast<double>(LastCommandSequence));
			State->SetStringField(TEXT("lastCommandStatus"), LastCommandStatus);
			State->SetStringField(TEXT("lastCommandReason"), LastCommandReason);
			State->SetStringField(TEXT("controlError"), ControlError);
			State->SetBoolField(TEXT("moveActive"), bMoveActive);
			State->SetNumberField(TEXT("moveInjectedFrames"), InjectedFrames);
			return State;
		}

		void Reject(const FString& Reason)
		{
			LastCommandStatus = TEXT("Rejected");
			LastCommandReason = Reason;
			UE_LOG(LogDigetherPrototypeProbe, Warning, TEXT("Command %lld rejected: %s"), LastCommandSequence, *Reason);
		}

		void ReadControl(UWorld* World, double Now)
		{
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *ControlPath) || Text == LastControlText)
			{
				return;
			}
			LastControlText = Text;

			TSharedPtr<FJsonObject> Command;
			const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Text);
			if (!FJsonSerializer::Deserialize(Reader, Command) || !Command.IsValid())
			{
				ControlError = TEXT("control.json is not parseable");
				UE_LOG(LogDigetherPrototypeProbe, Warning, TEXT("%s"), *ControlError);
				return;
			}

			double SequenceValue = 0.0;
			if (!Command->TryGetNumberField(TEXT("sequenceId"), SequenceValue) || SequenceValue < 1.0 || SequenceValue > 9.0e15 || SequenceValue != FMath::FloorToDouble(SequenceValue))
			{
				ControlError = TEXT("sequenceId must be a positive integer");
				UE_LOG(LogDigetherPrototypeProbe, Warning, TEXT("%s"), *ControlError);
				return;
			}
			const int64 Sequence = static_cast<int64>(SequenceValue);
			if (Sequence <= LastCommandSequence)
			{
				ControlError = TEXT("stale or duplicate sequenceId ignored");
				return;
			}

			ControlError.Empty();
			LastCommandSequence = Sequence;
			LastCommandReason.Empty();
			NextStateWrite = 0.0;

			FString Name;
			Command->TryGetStringField(TEXT("command"), Name);
			if (Name == TEXT("Stop"))
			{
				ReleaseHeldKeys();
				bMoveActive = false;
				MoveTarget.Empty();
				LastCommandStatus = TEXT("Applied");
			}
			else if (Name == TEXT("Quit"))
			{
				ReleaseHeldKeys();
				bMoveActive = false;
				if (bScheduledPending)
				{
					bScheduledPending = false;
					ScheduledStatus = TEXT("Canceled");
					ScheduledReason = TEXT("probe quitting before scheduled RPC");
					ScheduledTarget.Reset();
					ScheduledController.Reset();
				}
				bPendingQuit = true;
				Status = TEXT("Quitting");
				LastCommandStatus = TEXT("Applied");
			}
			else if (Name == TEXT("InputKey"))
			{
				FString KeyName;
				FString EventName;
				APlayerController* Controller = (GEngine && World) ? GEngine->GetFirstLocalPlayerController(World) : nullptr;
				if (!Command->TryGetStringField(TEXT("key"), KeyName) ||
					!Command->TryGetStringField(TEXT("event"), EventName) ||
					(EventName != TEXT("Pressed") && EventName != TEXT("Released")) ||
					!Controller || !Controller->IsLocalController() || !Controller->GetLocalPlayer())
				{
					Reject(TEXT("InputKey requires a local player, whitelisted key and event Pressed or Released"));
					return;
				}

				static const FKey AllowedKeys[] = {
					EKeys::W, EKeys::A, EKeys::S, EKeys::D, EKeys::LeftShift, EKeys::LeftControl,
					EKeys::SpaceBar, EKeys::E, EKeys::F, EKeys::B, EKeys::Q,
					EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five,
					EKeys::F1, EKeys::F5, EKeys::F9, EKeys::LeftMouseButton, EKeys::RightMouseButton
				};
				FKey Key;
				for (const FKey& Allowed : AllowedKeys)
				{
					if (KeyName.Equals(Allowed.GetFName().ToString(), ESearchCase::CaseSensitive))
					{
						Key = Allowed;
						break;
					}
				}
				if (!Key.IsValid())
				{
					Reject(TEXT("InputKey key is not in the probe whitelist"));
					return;
				}

				const bool bPressed = EventName == TEXT("Pressed");
				if (bPressed && HeldKeys.Contains(Key))
				{
					Reject(TEXT("InputKey key is already held; release it before pressing again"));
					return;
				}

				FInputDeviceId InputDevice = IPlatformInputDeviceMapper::Get().GetPrimaryInputDeviceForUser(Controller->GetPlatformUserId());
				if (!bPressed)
				{
					if (const FHeldInputKey* Held = HeldKeys.Find(Key))
					{
						Controller = Held->Controller.Get();
						InputDevice = Held->InputDevice;
					}
					HeldKeys.Remove(Key);
				}
				if (!Controller || !Controller->IsLocalController() || !Controller->GetLocalPlayer() || !InputDevice.IsValid())
				{
					Reject(TEXT("InputKey owning local controller or mapped input device is unavailable"));
					return;
				}
				if (bPressed)
				{
					FHeldInputKey Held;
					Held.Controller = Controller;
					Held.InputDevice = InputDevice;
					HeldKeys.Add(Key, Held);
				}
				Controller->InputKey(FInputKeyEventArgs::CreateSimulated(
					Key, bPressed ? IE_Pressed : IE_Released, bPressed ? 1.f : 0.f, -1, InputDevice));
				LastCommandStatus = TEXT("Applied");
				LastCommandReason = TEXT("real local key event injected; binding and gameplay outcome checked separately");
			}
			else if (Name == TEXT("MoveTo"))
			{
				FString TargetName;
				double Duration = 0.0;
				if (!Command->TryGetStringField(TEXT("target"), TargetName) || !FindTarget(World, TargetName)
					|| !FindLocalCharacter(World) || !Command->TryGetNumberField(TEXT("durationSeconds"), Duration)
					|| !FMath::IsFinite(Duration) || Duration <= 0 || Duration > 30)
				{
					Reject(TEXT("MoveTo requires existing target, local character and duration in (0,30]"));
				}
				else { MoveTarget = TargetName; MoveEndTime = Now + Duration; bMoveActive = true; LastCommandStatus = TEXT("Applied"); }
			}
			else if (Name == TEXT("Jump"))
			{
				if (ACharacter* Character = FindLocalCharacter(World)) { Character->Jump(); StopJumpAt = Now + 0.2; LastCommandStatus = TEXT("Applied"); }
				else Reject(TEXT("Jump requires a local character"));
			}
			else if (Name == TEXT("Screenshot"))
			{
				if (FParse::Param(FCommandLine::Get(), TEXT("NullRHI")) || !GEngine || !GEngine->GameViewport || !GEngine->GameViewport->Viewport)
				{
					Reject(TEXT("Screenshot requires a rendered game viewport; NullRHI is unsupported"));
				}
				else
				{
					const FString Filename = FPaths::Combine(FPaths::GetPath(StatePath), FString::Printf(TEXT("screenshot-%lld.png"), LastCommandSequence));
					// Include Slate/UMG as well as the scene and Canvas HUD so amount
					// entry is actually visible in rendered validation evidence.
					FScreenshotRequest::RequestScreenshot(Filename, true, false);
					LastCommandStatus = TEXT("Applied");
					LastCommandReason = TEXT("screenshot requested; file creation must be checked separately");
				}
			}
			else if (Name == TEXT("ToggleHeadlamp"))
			{
				APlayerController* PC = (GEngine && World) ? GEngine->GetFirstLocalPlayerController(World) : nullptr;
				if (!PC || !PC->IsLocalController() || !FindLocalCharacter(World)) Reject(TEXT("ToggleHeadlamp requires a local player and character"));
				else
				{
					PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::L, IE_Pressed, 1.f));
					PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::L, IE_Released, 0.f));
					LastCommandStatus = TEXT("Applied");
					LastCommandReason = TEXT("L key events injected; headlamp state checked separately");
				}
			}
			else if (Name == TEXT("ScheduleInteract"))
			{
				FString TargetName;
				bool bAlternate = false;
				double FireAt = 0.0;
				Command->TryGetStringField(TEXT("target"), TargetName);
				Command->TryGetBoolField(TEXT("alternate"), bAlternate);
				AActor* Target = FindTarget(World, TargetName);
				ADigetherPrototypePlayerController* PC = (GEngine && World)
					? Cast<ADigetherPrototypePlayerController>(GEngine->GetFirstLocalPlayerController(World)) : nullptr;
				const double UtcNow = UtcMilliseconds();
				if (bScheduledPending || !Target || !PC || !PC->IsLocalController()
					|| !Command->TryGetNumberField(TEXT("fireAtUtcMs"), FireAt) || !FMath::IsFinite(FireAt)
					|| FireAt != FMath::FloorToDouble(FireAt) || FireAt < UtcNow + 1000.0 || FireAt > UtcNow + 15000.0)
				{
					Reject(TEXT("ScheduleInteract requires a local controller, existing target, no pending RPC and UTC deadline 1-15 seconds ahead"));
				}
				else
				{
					ScheduledTarget = Target;
					ScheduledController = PC;
					ScheduledSequence = LastCommandSequence;
					ScheduledAtUtcMs = FireAt;
					ScheduledFiredUtcMs = 0.0;
					ScheduledTargetName = TargetName;
					ScheduledStatus = TEXT("Pending");
					ScheduledReason.Empty();
					bScheduledAlternate = bAlternate;
					bScheduledDig = false;
					bScheduledPending = true;
					LastCommandStatus = TEXT("Applied");
					LastCommandReason = TEXT("target prepared; RPC is not yet sent");
				}
			}
			else if (Name == TEXT("Look") || Name == TEXT("Dig") || Name == TEXT("SupportTerrain"))
			{
				ADigetherPrototypePlayerController* PC = (GEngine && World) ? Cast<ADigetherPrototypePlayerController>(GEngine->GetFirstLocalPlayerController(World)) : nullptr;
				if (!PC || !PC->IsLocalController() || !FindLocalCharacter(World)) Reject(TEXT("command requires local controller and character"));
				else if (Name == TEXT("Look"))
				{
					double Pitch = 0, Yaw = 0;
					if (!Command->TryGetNumberField(TEXT("pitch"), Pitch) || !Command->TryGetNumberField(TEXT("yaw"), Yaw) || !FMath::IsFinite(Pitch) || !FMath::IsFinite(Yaw) || Pitch < -89 || Pitch > 89) Reject(TEXT("Look requires finite pitch [-89,89] and yaw"));
					else { PC->SetControlRotation(FRotator(Pitch, Yaw, 0)); LastCommandStatus = TEXT("Applied"); }
				}
				else if (Name == TEXT("Dig") && Command->HasField(TEXT("fireAtUtcMs")))
				{
					double FireAt = 0; const double UtcNow = UtcMilliseconds();
					if (bScheduledPending || !Command->TryGetNumberField(TEXT("fireAtUtcMs"), FireAt) || !FMath::IsFinite(FireAt) || FireAt != FMath::FloorToDouble(FireAt) || FireAt < UtcNow + 1000 || FireAt > UtcNow + 15000) Reject(TEXT("scheduled Dig requires no pending RPC and UTC deadline 1-15 seconds ahead"));
					else { ScheduledController = PC; ScheduledTarget.Reset(); ScheduledSequence = LastCommandSequence; ScheduledAtUtcMs = FireAt; ScheduledFiredUtcMs = 0; ScheduledTargetName = TEXT("Dig"); ScheduledStatus = TEXT("Pending"); ScheduledReason.Empty(); bScheduledDig = true; bScheduledPending = true; LastCommandStatus = TEXT("Applied"); LastCommandReason = TEXT("RPC prepared; outcome checked separately"); }
				}
				else { if (Name == TEXT("Dig")) PC->ServerDig(); else PC->ServerSupportTerrain(); LastCommandStatus = TEXT("Applied"); LastCommandReason = TEXT("RPC sent; outcome checked separately"); }
			}
			else if (Name == TEXT("DepositInput") || Name == TEXT("DepositSubmit") || Name == TEXT("DepositClose"))
			{
				ADigetherPrototypePlayerController* PC = (GEngine && World)
					? Cast<ADigetherPrototypePlayerController>(GEngine->GetFirstLocalPlayerController(World)) : nullptr;
				FString InputText;
				if (!PC || !PC->IsLocalController()) Reject(TEXT("deposit UI command requires the owning local controller"));
				else if (Name == TEXT("DepositInput") && (!Command->TryGetStringField(TEXT("text"), InputText) || InputText.Len() > 128))
					Reject(TEXT("DepositInput requires a string of at most 128 characters"));
				else
				{
					// Use the same local editing/submission methods as the product UI.
					// Invalid text reaches that UI's validation rather than changing game state here.
					if (Name == TEXT("DepositInput")) PC->SetDepositInputText(InputText);
					else if (Name == TEXT("DepositSubmit")) PC->SubmitDepositInput();
					else PC->CloseDepositInput();
					LastCommandStatus = TEXT("Applied");
					LastCommandReason = TEXT("product UI entry point invoked; result checked separately");
				}
			}
			else if (Name == TEXT("DepositRpc"))
			{
				ADigetherPrototypePlayerController* PC = (GEngine && World)
					? Cast<ADigetherPrototypePlayerController>(GEngine->GetFirstLocalPlayerController(World)) : nullptr;
				FString TargetName;
				Command->TryGetStringField(TEXT("target"), TargetName);
				ADigetherPrototypeStation* Station = Cast<ADigetherPrototypeStation>(FindTarget(World, TargetName));
				double Amount = 0.0;
				double SessionId = PC ? PC->GetDepositSessionId() : 0;
				const bool bSessionValid = !Command->HasField(TEXT("sessionId")) || Command->TryGetNumberField(TEXT("sessionId"), SessionId);
				if (!PC || !PC->IsLocalController() || !Station || !Command->TryGetNumberField(TEXT("amount"), Amount)
					|| !bSessionValid || !FMath::IsFinite(Amount) || Amount != FMath::FloorToDouble(Amount) || Amount < MIN_int32 || Amount > MAX_int32
					|| !FMath::IsFinite(SessionId) || SessionId != FMath::FloorToDouble(SessionId) || SessionId < MIN_int32 || SessionId > MAX_int32)
				{
					Reject(TEXT("DepositRpc requires owning controller, station and int32 amount/sessionId"));
				}
				else
				{
					// Deliberately allow zero/negative/over-balance requests through the real RPC.
					// Only wire type validity is checked here; the server must reject game-rule violations.
					PC->ServerDepositAmount(Station, static_cast<int32>(Amount), static_cast<int32>(SessionId));
					LastCommandStatus = TEXT("Applied");
					LastCommandReason = TEXT("owning-controller deposit RPC sent; authoritative outcome checked separately");
				}
			}
			else if (Name == TEXT("Interact"))
			{
				FString TargetName;
				bool bAlternate = false;
				Command->TryGetStringField(TEXT("target"), TargetName);
				Command->TryGetBoolField(TEXT("alternate"), bAlternate);
				AActor* Target = FindTarget(World, TargetName);
				ADigetherPrototypePlayerController* PC = (GEngine && World)
					? Cast<ADigetherPrototypePlayerController>(GEngine->GetFirstLocalPlayerController(World)) : nullptr;
				if (!Target || !PC) Reject(TEXT("Interact requires replicated target and prototype player controller"));
				else
				{
					PC->ServerInteract(Target, bAlternate);
					LastCommandStatus = TEXT("Applied");
					LastCommandReason = TEXT("RPC sent; application result must be observed separately");
				}
			}
			else if (Name == TEXT("CancelInteraction") || Name == TEXT("Drop"))
			{
				ADigetherPrototypePlayerController* PC = (GEngine && World)
					? Cast<ADigetherPrototypePlayerController>(GEngine->GetFirstLocalPlayerController(World)) : nullptr;
				double Slot = 0;
				if (!PC) Reject(TEXT("command requires prototype player controller"));
				else if (Name == TEXT("Drop") && (!Command->TryGetNumberField(TEXT("slot"), Slot) || Slot < 1 || Slot > 5 || Slot != FMath::FloorToDouble(Slot)))
					Reject(TEXT("Drop slot must be an integer in [1,5]"));
				else
				{
					if (Name == TEXT("Drop")) PC->ServerDrop(static_cast<int32>(Slot));
					else PC->ServerCancelInteraction();
					LastCommandStatus = TEXT("Applied");
					LastCommandReason = TEXT("RPC sent; application result must be observed separately");
				}
			}
			else if (Name == TEXT("Move"))
			{
				ApplyMove(*Command, World, Now);
			}
			else
			{
				Reject(TEXT("unknown command"));
			}
		}

		void ApplyMove(const FJsonObject& Command, UWorld* World, double Now)
		{
			const TSharedPtr<FJsonObject>* Direction = nullptr;
			double X = 0.0;
			double Y = 0.0;
			double Z = 0.0;
			double Duration = 0.0;
			if (!Command.TryGetObjectField(TEXT("direction"), Direction) || !Direction
				|| !(*Direction)->TryGetNumberField(TEXT("x"), X) || !(*Direction)->TryGetNumberField(TEXT("y"), Y))
			{
				Reject(TEXT("Move requires direction.x and direction.y"));
				return;
			}
			(*Direction)->TryGetNumberField(TEXT("z"), Z);
			if (!FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z) || FMath::Square(X) + FMath::Square(Y) < 1.0e-8)
			{
				Reject(TEXT("direction XY must be finite and non-zero"));
				return;
			}
			if (!Command.TryGetNumberField(TEXT("durationSeconds"), Duration) || !FMath::IsFinite(Duration) || Duration <= 0.0 || Duration > MaxMoveSeconds)
			{
				Reject(TEXT("durationSeconds must be in (0, 2]"));
				return;
			}
			if (!FindLocalCharacter(World))
			{
				Reject(TEXT("no locally controlled Character"));
				return;
			}

			MoveTarget.Empty();
			MoveDirection = FVector(X, Y, 0.0).GetSafeNormal();
			MoveEndTime = Now + Duration;
			bMoveActive = true;
			LastCommandStatus = TEXT("Applied");
		}
	};

	TUniquePtr<FProbe> GProbe;

	void StartIfRequested()
	{
		FString Role;
		FString Dir;
		if (!FParse::Value(FCommandLine::Get(), TEXT("-DigetherPrototypeProbe="), Role)
			|| !FParse::Value(FCommandLine::Get(), TEXT("-DigetherPrototypeProbeDir="), Dir, false))
		{
			return;
		}
		FString MaxSecondsText;
		const bool bHasMaxSeconds = FParse::Value(FCommandLine::Get(), TEXT("-DigetherPrototypeProbeMaxSeconds="), MaxSecondsText);

		GProbe = MakeUnique<FProbe>();
		FString Error;
		if (!GProbe->Configure(Role, Dir, MaxSecondsText, bHasMaxSeconds, Error))
		{
			GProbe->Fail(Error);
			return;
		}

		GProbe->PreExitHandle = FCoreDelegates::OnPreExit.AddLambda([]()
		{
			if (GProbe.IsValid())
			{
				GProbe->OnPreExit();
			}
		});
		GProbe->TickHandle = FTSTicker::GetCoreTicker().AddTicker(TEXT("DigetherPrototypeProbe"), 0.0f, [](float)
		{
			return GProbe.IsValid() && GProbe->Tick();
		});
		UE_LOG(LogDigetherPrototypeProbe, Display, TEXT("Network probe started: role=%s maxSeconds=%.1f"), *GProbe->Role, GProbe->MaxSeconds);
	}

	FDelayedAutoRegisterHelper GRegistration(EDelayedRegisterRunPhase::EndOfEngineInit, []() { StartIfRequested(); });
}

#endif
