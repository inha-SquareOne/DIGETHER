#include "Game/Tests/DigetherVoiceEnvironmentTestTypes.h"

#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "GameFramework/PlayerController.h"

void ADigetherVoiceEnvironmentTestPawn::KeepOriginalPawn(APawn* Pawn)
{
	if (!HasAuthority() || !IsValid(Pawn))
	{
		return;
	}
	OriginalPawn = Pawn;
	bOriginalHidden = Pawn->IsHidden();
	bOriginalCollision = Pawn->GetActorEnableCollision();
	Pawn->SetActorHiddenInGame(true);
	Pawn->SetActorEnableCollision(false);
}

void ADigetherVoiceEnvironmentTestPawn::RestoreOriginalPawn()
{
	APlayerController* OwningController = Cast<APlayerController>(GetController());
	if (HasAuthority() && IsValid(OwningController) && IsValid(OriginalPawn))
	{
		OriginalPawn->SetActorHiddenInGame(bOriginalHidden);
		OriginalPawn->SetActorEnableCollision(bOriginalCollision);
		OwningController->Possess(OriginalPawn);
		if (ADigetherGameplayPlayerState* State = OwningController->GetPlayerState<ADigetherGameplayPlayerState>())
		{
			State->RetainPawn(OriginalPawn);
			State->RefreshVoiceEnvironment();
		}
		OriginalPawn = nullptr;
		Destroy();
	}
}

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BrushComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Framework/DigetherGameplayGameMode.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Monster/DigetherZoneVolume.h"
#include "Game/Voice/DigetherVoiceLog.h"
#include "Game/Voice/DigetherVoiceRoutingComponent.h"
#include "Game/Voice/DigetherVoiceRules.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/IConsoleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "Serialization/JsonSerializer.h"

namespace DigetherVoiceEnvironmentTest
{
	ADigetherZoneVolume* CreateDungeon(UWorld* World, const FVector& Location)
	{
		ADigetherZoneVolume* Zone = World != nullptr ? World->SpawnActor<ADigetherZoneVolume>() : nullptr;
		if (!IsValid(Zone))
		{
			return nullptr;
		}
		Zone->Zone = EDigetherMonsterZone::Dungeon;
		Zone->ZoneId = TEXT("VoiceTestDungeon");
		UBrushComponent* Brush = Zone->GetBrushComponent();
		Brush->SetMobility(EComponentMobility::Movable);
		Brush->UnregisterComponent();
		Brush->BrushBodySetup = NewObject<UBodySetup>(Brush);
		FKBoxElem Box;
		// 시험 볼륨 20m, 기획 구역 크기 아님
		Box.X = 2000.f;
		Box.Y = 2000.f;
		Box.Z = 2000.f;
		Brush->BrushBodySetup->AggGeom.BoxElems.Add(Box);
		Brush->BrushBodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		Zone->SetActorLocation(Location);
		Brush->RegisterComponent();
		return Zone;
	}
}

namespace
{
	const FVector VoiceTestMineLocation(100000.f, 0.f, 10000.f);
	const FVector VoiceTestDungeonLocation(110000.f, 0.f, 10000.f);

	void RefreshVoiceTestRoutes(UWorld* World)
	{
		if (AGameModeBase* Mode = World != nullptr ? World->GetAuthGameMode() : nullptr)
		{
			if (UDigetherVoiceRoutingComponent* Routes = Mode->FindComponentByClass<UDigetherVoiceRoutingComponent>())
			{
				Routes->Refresh();
			}
		}
	}

	void SnapshotVoiceTest(UWorld* World, const FString& Label)
	{
		AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
		if (!IsValid(GameState))
		{
			return;
		}
		const bool bAuthority = World->GetNetMode() != NM_Client;
		TSharedRef<FJsonObject> Snapshot = MakeShared<FJsonObject>();
		Snapshot->SetStringField(TEXT("label"), Label);
		Snapshot->SetBoolField(TEXT("authority"), bAuthority);
		TArray<FDigetherVoiceRouteEntry> Entries;
		EDigetherVoiceStage Stage = EDigetherVoiceStage::Separated;
		if (ADigetherGameplayGameMode* Mode = World->GetAuthGameMode<ADigetherGameplayGameMode>())
		{
			if (UDigetherVoiceRoutingComponent* Routes = Mode->FindComponentByClass<UDigetherVoiceRoutingComponent>())
			{
				Routes->Refresh();
				if (Routes->CollectParticipants.IsBound())
				{
					Routes->CollectParticipants.Execute(Entries, Stage);
				}
			}
		}
		TArray<TSharedPtr<FJsonValue>> Players;
		for (APlayerState* Entry : GameState->PlayerArray)
		{
			ADigetherGameplayPlayerState* State = Cast<ADigetherGameplayPlayerState>(Entry);
			if (!IsValid(State))
			{
				continue;
			}
			TSharedRef<FJsonObject> Player = MakeShared<FJsonObject>();
			Player->SetNumberField(TEXT("id"), State->GetPlayerId());
			Player->SetStringField(TEXT("name"), State->GetPlayerName());
			Player->SetBoolField(TEXT("buried"), State->IsBuriedForVoice());
			const FDigetherLifeComponentState Life = State->GetLifeComponent()->CaptureState();
			Player->SetStringField(TEXT("life"), StaticEnum<EDigetherLifeState>()->GetNameStringByValue(static_cast<int64>(Life.PublicState.Life)));
			Player->SetBoolField(TEXT("spectating"), Life.PublicState.bSpectating);
			if (bAuthority)
			{
				const APawn* Pawn = State->GetGameplayPawn();
				Player->SetNumberField(TEXT("zone"), IsValid(Pawn) && ADigetherZoneVolume::GetZoneKindAt(World,
					Pawn->GetActorLocation()) == EDigetherMonsterZone::Dungeon ? 1 : 0);
				Player->SetStringField(TEXT("target"), Life.PrivateState.SpectateTargetPlayerKey);
			}
			Players.Add(MakeShared<FJsonValueObject>(Player));
		}
		Snapshot->SetArrayField(TEXT("players"), Players);
		if (bAuthority)
		{
			Snapshot->SetStringField(TEXT("stage"), Stage == EDigetherVoiceStage::AllOpen ? TEXT("AllOpen") : TEXT("Separated"));
			TArray<FDigetherVoiceParticipant> Participants;
			for (const FDigetherVoiceRouteEntry& Entry : Entries)
			{
				Participants.Add(Entry.Participant);
			}
			TArray<TSharedPtr<FJsonValue>> Routes;
			for (const FDigetherVoiceRouteEntry& Entry : Entries)
			{
				TSharedRef<FJsonObject> Route = MakeShared<FJsonObject>();
				Route->SetNumberField(TEXT("listener"), FCString::Atoi(*Entry.Participant.PlayerKey));
				TArray<TSharedPtr<FJsonValue>> Blocked;
				TArray<TSharedPtr<FJsonValue>> Applied;
				const TSet<FString> Expected = FDigetherVoiceRules::CollectBlockedTalkers(Entry.Participant, Participants, Stage);
				for (const FDigetherVoiceRouteEntry& Talker : Entries)
				{
					if (Expected.Contains(Talker.Participant.PlayerKey))
					{
						Blocked.Add(MakeShared<FJsonValueNumber>(FCString::Atoi(*Talker.Participant.PlayerKey)));
					}
					if (Talker.UniqueId.IsValid() && Entry.Controller.IsValid() && Entry.Controller->IsPlayerMuted(*Talker.UniqueId))
					{
						Applied.Add(MakeShared<FJsonValueNumber>(FCString::Atoi(*Talker.Participant.PlayerKey)));
					}
				}
				Route->SetArrayField(TEXT("blocked"), Blocked);
				Route->SetArrayField(TEXT("applied_muted"), Applied);
				Routes.Add(MakeShared<FJsonValueObject>(Route));
			}
			Snapshot->SetArrayField(TEXT("routes"), Routes);
		}
		FString Json;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR,
			TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
		FJsonSerializer::Serialize(Snapshot, Writer);
		UE_LOG(LogDigetherVoice, Display, TEXT("VOICE_ENV_SNAPSHOT %s"), *Json);
	}

	FAutoConsoleCommandWithWorldAndArgs VoiceEnvironmentTestCommand(TEXT("Digether.VoiceTest"),
		TEXT("시험 전용: setup|cleanup|buried PlayerId 0/1|zone PlayerId 0/1|role PlayerId active/dead/escaped|snapshot Label"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (World == nullptr || Args.IsEmpty())
			{
				return;
			}
			if (Args[0] == TEXT("snapshot"))
			{
				SnapshotVoiceTest(World, Args.Num() > 1 ? Args[1] : TEXT("unnamed"));
				return;
			}
			if (World->GetNetMode() == NM_Client)
			{
				return;
			}
			if (Args[0] == TEXT("setup"))
			{
				bool bHasZone = false;
				for (TActorIterator<ADigetherZoneVolume> It(World); It; ++It)
				{
					bHasZone |= It->ZoneId == TEXT("VoiceTestDungeon");
				}
				if (!bHasZone)
				{
					DigetherVoiceEnvironmentTest::CreateDungeon(World, VoiceTestDungeonLocation);
				}
				for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
				{
					APlayerController* Controller = It->Get();
					ADigetherGameplayPlayerState* State = IsValid(Controller) ? Controller->GetPlayerState<ADigetherGameplayPlayerState>() : nullptr;
					APawn* Original = IsValid(Controller) ? Controller->GetPawn() : nullptr;
					if (!IsValid(State) || !IsValid(Original) || Cast<ADigetherVoiceEnvironmentTestPawn>(Original) != nullptr)
					{
						continue;
					}
					ADigetherVoiceEnvironmentTestPawn* Pawn = World->SpawnActor<ADigetherVoiceEnvironmentTestPawn>();
					if (IsValid(Pawn))
					{
						Pawn->SetReplicates(true);
						Pawn->KeepOriginalPawn(Original);
						Pawn->SetActorLocation(VoiceTestMineLocation + FVector(0.f, State->GetPlayerId() * 100.f, 0.f));
						Controller->Possess(Pawn);
						State->RetainPawn(Pawn);
					}
				}
			}
			else if (Args[0] == TEXT("cleanup"))
			{
				TArray<ADigetherVoiceEnvironmentTestPawn*> Pawns;
				for (TActorIterator<ADigetherVoiceEnvironmentTestPawn> It(World); It; ++It)
				{
					Pawns.Add(*It);
				}
				for (ADigetherVoiceEnvironmentTestPawn* Pawn : Pawns)
				{
					Pawn->RestoreOriginalPawn();
				}
				for (TActorIterator<ADigetherZoneVolume> It(World); It; ++It)
				{
					if (It->ZoneId == TEXT("VoiceTestDungeon"))
					{
						It->Destroy();
					}
				}
			}
			else if (Args.Num() == 3 && World->GetGameState() != nullptr)
			{
				for (APlayerState* Entry : World->GetGameState()->PlayerArray)
				{
					ADigetherGameplayPlayerState* State = Cast<ADigetherGameplayPlayerState>(Entry);
					if (!IsValid(State) || State->GetPlayerId() != FCString::Atoi(*Args[1]))
					{
						continue;
					}
					ADigetherVoiceEnvironmentTestPawn* Pawn = Cast<ADigetherVoiceEnvironmentTestPawn>(State->GetGameplayPawn());
					if (Args[0] == TEXT("buried") && IsValid(Pawn))
					{
						Pawn->SetTestBuried(Args[2] == TEXT("1"));
					}
					else if (Args[0] == TEXT("zone") && IsValid(Pawn))
					{
						Pawn->SetActorLocation(Args[2] == TEXT("1") ? VoiceTestDungeonLocation : VoiceTestMineLocation,
							false, nullptr, ETeleportType::TeleportPhysics);
						if (APlayerController* Controller = Cast<APlayerController>(State->GetOwner()))
						{
							Controller->ClientSetLocation(Pawn->GetActorLocation(), Pawn->GetActorRotation());
						}
					}
					else if (Args[0] == TEXT("role"))
					{
						if (Args[2] == TEXT("active"))
						{
							State->GetLifeComponent()->Revive();
						}
						else if (Args[2] == TEXT("dead"))
						{
							State->GetLifeComponent()->ApplyDeath(EDigetherDeathCause::Killed);
						}
						else if (Args[2] == TEXT("escaped"))
						{
							State->GetLifeComponent()->ApplyEscape(EDigetherEscapeKind::Emergency);
						}
					}
				}
			}
			RefreshVoiceTestRoutes(World);
		}));
}

#endif
