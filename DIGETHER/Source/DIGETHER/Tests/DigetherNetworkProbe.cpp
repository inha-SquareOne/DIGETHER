// Development-only out-of-process network probe. Active only when -DigetherNetworkProbe and -DigetherNetworkProbeDir are given.

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogDigetherNetworkProbe, Log, All);

namespace DigetherNetworkProbe
{
	constexpr double DefaultMaxSeconds = 180.0;
	constexpr double MaxAllowedSeconds = 600.0;
	constexpr double ControlReadInterval = 0.1;
	constexpr double StateWriteInterval = 0.2;
	constexpr double MaxMoveSeconds = 2.0;

	const TCHAR* const AllowedRoles[] = { TEXT("Host"), TEXT("Client1"), TEXT("Client2"), TEXT("Client3"), TEXT("Client3Reconnect") };

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
		double MoveEndTime = 0.0;
		FVector MoveDirection = FVector::ZeroVector;
		int32 InjectedFrames = 0;
		bool bPendingQuit = false;
		bool bExitRequested = false;
		bool bFailed = false;
		FTSTicker::FDelegateHandle TickHandle;
		FDelegateHandle PreExitHandle;

		bool Configure(const FString& InRole, const FString& InDir, const FString& MaxSecondsText, bool bHasMaxSeconds, FString& OutError)
		{
			StartTime = FPlatformTime::Seconds();

			FString Dir = InDir;
			FPaths::NormalizeDirectoryName(Dir);
			if (FPaths::IsRelative(Dir) || !FPaths::CollapseRelativeDirectories(Dir) || Dir.Contains(TEXT("..")))
			{
				OutError = TEXT("DigetherNetworkProbeDir must be an absolute path without '..'");
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
					OutError = TEXT("DigetherNetworkProbeMaxSeconds is not numeric");
					return false;
				}
				MaxSeconds = FCString::Atod(*MaxSecondsText);
				if (!FMath::IsFinite(MaxSeconds) || MaxSeconds <= 0.0 || MaxSeconds > MaxAllowedSeconds)
				{
					OutError = FString::Printf(TEXT("DigetherNetworkProbeMaxSeconds must be in (0, %.0f]"), MaxAllowedSeconds);
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

			if (!bExitRequested && Now - StartTime >= MaxSeconds)
			{
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

			if (bMoveActive && !bExitRequested)
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
			Status = TEXT("Failed");
			FailureReason = Reason;
			UE_LOG(LogDigetherNetworkProbe, Error, TEXT("Network probe failed: %s"), *Reason);
			FString Ignored;
			TryWriteState(FindGameWorld(), Ignored);
			RequestExitOnce();
		}

		void OnPreExit()
		{
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
					UE_LOG(LogDigetherNetworkProbe, Error, TEXT("Final probe state write failed: %s"), *Error);
				}
			}
		}

	private:
		void RequestExitOnce()
		{
			if (!bExitRequested)
			{
				bExitRequested = true;
				FPlatformMisc::RequestExit(false, TEXT("DigetherNetworkProbe"));
			}
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
			UE_LOG(LogDigetherNetworkProbe, Warning, TEXT("Command %lld rejected: %s"), LastCommandSequence, *Reason);
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
				UE_LOG(LogDigetherNetworkProbe, Warning, TEXT("%s"), *ControlError);
				return;
			}

			double SequenceValue = 0.0;
			if (!Command->TryGetNumberField(TEXT("sequenceId"), SequenceValue) || SequenceValue < 1.0 || SequenceValue > 9.0e15 || SequenceValue != FMath::FloorToDouble(SequenceValue))
			{
				ControlError = TEXT("sequenceId must be a positive integer");
				UE_LOG(LogDigetherNetworkProbe, Warning, TEXT("%s"), *ControlError);
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
				bMoveActive = false;
				LastCommandStatus = TEXT("Applied");
			}
			else if (Name == TEXT("Quit"))
			{
				bMoveActive = false;
				bPendingQuit = true;
				Status = TEXT("Quitting");
				LastCommandStatus = TEXT("Applied");
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
		if (!FParse::Value(FCommandLine::Get(), TEXT("-DigetherNetworkProbe="), Role)
			|| !FParse::Value(FCommandLine::Get(), TEXT("-DigetherNetworkProbeDir="), Dir, false))
		{
			return;
		}
		FString MaxSecondsText;
		const bool bHasMaxSeconds = FParse::Value(FCommandLine::Get(), TEXT("-DigetherNetworkProbeMaxSeconds="), MaxSecondsText);

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
		GProbe->TickHandle = FTSTicker::GetCoreTicker().AddTicker(TEXT("DigetherNetworkProbe"), 0.0f, [](float)
		{
			return GProbe.IsValid() && GProbe->Tick();
		});
		UE_LOG(LogDigetherNetworkProbe, Display, TEXT("Network probe started: role=%s maxSeconds=%.1f"), *GProbe->Role, GProbe->MaxSeconds);
	}

	FDelayedAutoRegisterHelper GRegistration(EDelayedRegisterRunPhase::EndOfEngineInit, []() { StartIfRequested(); });
}

#endif
