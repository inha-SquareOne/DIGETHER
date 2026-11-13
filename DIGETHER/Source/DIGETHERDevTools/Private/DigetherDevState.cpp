#include "DigetherDevState.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Economy/DigetherQuotaStateComponent.h"
#include "Game/Elevator/DigetherElevatorActor.h"
#include "Game/Elevator/DigetherElevatorComponent.h"
#include "Game/Framework/DigetherGameplayGameMode.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Monster/DigetherMonsterCharacter.h"
#include "Game/Monster/DigetherMonsterDirectorSubsystem.h"
#include "Game/Run/DigetherRunCoordinatorComponent.h"
#include "Game/Run/DigetherRunPublicStateComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace DigetherDev
{
	namespace
	{
		template <typename EnumType>
		FString EnumName(const EnumType Value)
		{
			const UEnum* Enum = StaticEnum<EnumType>();
			return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Value)) : FString();
		}

		FString NetModeName(const ENetMode Mode)
		{
			switch (Mode)
			{
			case NM_Standalone: return TEXT("단독");
			case NM_ListenServer: return TEXT("호스트");
			case NM_DedicatedServer: return TEXT("전용 서버");
			case NM_Client: return TEXT("참가자");
			default: return TEXT("?");
			}
		}
	}

	FStateSummary CaptureState(UWorld* World)
	{
		FStateSummary State;
		if (World == nullptr)
		{
			return State;
		}
		State.bHasWorld = true;
		State.bHost = World->GetNetMode() != NM_Client;
		State.MapName = World->GetMapName();
		State.MapName.RemoveFromStart(World->StreamingLevelsPrefix);
		State.NetMode = NetModeName(World->GetNetMode());

		const ADigetherGameplayGameState* GameState = World->GetGameState<ADigetherGameplayGameState>();
		State.bGameplay = GameState != nullptr;
		if (GameState == nullptr)
		{
			return State;
		}
		if (const UDigetherQuotaStateComponent* Quota = GameState->GetQuotaComponent())
		{
			const FDigetherQuotaPublicState Public = Quota->CaptureState();
			State.TotalDay = Public.Quota.TotalDay;
			State.PeriodDay = Public.Quota.Day;
			State.TargetCredits = Public.Quota.TargetCredits;
			State.DepositedCredits = State.bHost ? Public.Quota.DepositedCredits : -1;
		}
		const ADigetherGameplayGameMode* GameMode = World->GetAuthGameMode<ADigetherGameplayGameMode>();
		const UDigetherRunCoordinatorComponent* Coordinator = GameMode ? GameMode->GetRunCoordinatorComponent() : nullptr;
		FDigetherRunPublicState Run;
		if (Coordinator)
		{
			Run = Coordinator->CapturePublicState();
			if (Run.Phase == EDigetherRunPhase::InProgress)
			{
				State.RemainingSeconds = Coordinator->GetRemainingSeconds();
			}
		}
		else if (const UDigetherRunPublicStateComponent* RunPublic = GameState->GetRunPublicStateComponent())
		{
			Run = RunPublic->CaptureState();
		}
		State.RunPhase = EnumName(Run.Phase);
		State.AftershockStage = Run.AftershockStage;
		State.bPaymentDeadline = Run.bPaymentDeadline;
		State.bGameOver = Run.bGameOver || (GameMode != nullptr && GameMode->IsGameOverActive());

		for (TActorIterator<ADigetherElevatorActor> It(World); It; ++It)
		{
			if (const UDigetherElevatorComponent* Elevator = It->GetElevatorComponent())
			{
				const FDigetherElevatorPublicState Public = Elevator->GetPublicState();
				State.ElevatorPhase = EnumName(Public.Phase);
				State.CompletedAscents = Public.CompletedAscents;
				State.WeightStage = EnumName(Public.WeightStage);
				break;
			}
		}

		const APlayerController* LocalController = nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (It->Get() && It->Get()->IsLocalController())
			{
				LocalController = It->Get();
				break;
			}
		}
		const APlayerState* LocalPlayerState = LocalController ? LocalController->PlayerState.Get() : nullptr;
		for (APlayerState* Listed : GameState->PlayerArray)
		{
			const ADigetherGameplayPlayerState* Player = Cast<ADigetherGameplayPlayerState>(Listed);
			if (Player == nullptr)
			{
				continue;
			}
			FStateSummary::FPlayer Entry;
			Entry.PlayerId = Player->GetPlayerId();
			Entry.Name = Player->GetPlayerName();
			Entry.bSelf = Player == LocalPlayerState;
			Entry.Balance = State.bHost || Entry.bSelf ? Player->CaptureState().BalanceCredits : -1;
			if (const UDigetherLifeStateComponent* Life = Player->GetLifeComponent())
			{
				Entry.Life = EnumName(Life->CaptureState().PublicState.Life);
			}
			State.Players.Add(MoveTemp(Entry));
		}

		if (const UDigetherMonsterDirectorSubsystem* Director = World->GetSubsystem<UDigetherMonsterDirectorSubsystem>())
		{
			State.bMonsterDirector = Director->IsActive();
			State.bMonsterRun = Director->HasActiveRun();
			State.bSpawnTimerRunning = Director->IsSpawnTimerRunning();
			State.SpawnTimerSeconds = Director->GetSpawnTimerSeconds();
			State.SpawnChance = Director->GetCurrentChance();
			TArray<ADigetherMonsterCharacter*> Monsters;
			Director->GetLiveMonsters(Monsters);
			State.LiveMonsters = Monsters.Num();
		}
		return State;
	}

	FString StateToJson(const FStateSummary& State, const bool bCheatUsed, const FString& WebUrl,
		const TArray<TPair<FString, bool>>& Panels)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("build"), FApp::GetBuildVersion());
		Root->SetStringField(TEXT("url"), WebUrl);
		Root->SetBoolField(TEXT("cheatUsed"), bCheatUsed);
		Root->SetBoolField(TEXT("hasWorld"), State.bHasWorld);
		Root->SetBoolField(TEXT("host"), State.bHost);
		Root->SetBoolField(TEXT("gameplay"), State.bGameplay);
		Root->SetStringField(TEXT("map"), State.MapName);
		Root->SetStringField(TEXT("netMode"), State.NetMode);
		Root->SetNumberField(TEXT("totalDay"), State.TotalDay);
		Root->SetNumberField(TEXT("periodDay"), State.PeriodDay);
		Root->SetNumberField(TEXT("target"), State.TargetCredits);
		Root->SetNumberField(TEXT("deposited"), State.DepositedCredits);
		Root->SetNumberField(TEXT("remainingSeconds"), State.RemainingSeconds);
		Root->SetStringField(TEXT("runPhase"), State.RunPhase);
		Root->SetNumberField(TEXT("aftershockStage"), State.AftershockStage);
		Root->SetBoolField(TEXT("paymentDeadline"), State.bPaymentDeadline);
		Root->SetBoolField(TEXT("gameOver"), State.bGameOver);
		Root->SetStringField(TEXT("elevatorPhase"), State.ElevatorPhase);
		Root->SetNumberField(TEXT("ascents"), State.CompletedAscents);
		Root->SetStringField(TEXT("weightStage"), State.WeightStage);

		TArray<TSharedPtr<FJsonValue>> Players;
		for (const FStateSummary::FPlayer& Player : State.Players)
		{
			TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetNumberField(TEXT("id"), Player.PlayerId);
			Object->SetStringField(TEXT("name"), Player.Name);
			Object->SetBoolField(TEXT("self"), Player.bSelf);
			Object->SetNumberField(TEXT("balance"), Player.Balance);
			Object->SetStringField(TEXT("life"), Player.Life);
			Players.Add(MakeShared<FJsonValueObject>(Object));
		}
		Root->SetArrayField(TEXT("players"), Players);

		TSharedRef<FJsonObject> Monster = MakeShared<FJsonObject>();
		Monster->SetBoolField(TEXT("director"), State.bMonsterDirector);
		Monster->SetBoolField(TEXT("run"), State.bMonsterRun);
		Monster->SetBoolField(TEXT("timerRunning"), State.bSpawnTimerRunning);
		Monster->SetNumberField(TEXT("timerSeconds"), State.SpawnTimerSeconds);
		Monster->SetNumberField(TEXT("chance"), State.SpawnChance);
		Monster->SetNumberField(TEXT("alive"), State.LiveMonsters);
		Root->SetObjectField(TEXT("monster"), Monster);

		TArray<TSharedPtr<FJsonValue>> PanelValues;
		for (const TPair<FString, bool>& Panel : Panels)
		{
			TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetStringField(TEXT("id"), Panel.Key);
			Object->SetBoolField(TEXT("on"), Panel.Value);
			PanelValues.Add(MakeShared<FJsonValueObject>(Object));
		}
		Root->SetArrayField(TEXT("panels"), PanelValues);

		FString Output;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(Root, Writer);
		return Output;
	}

	void StateToLines(const FStateSummary& State, TArray<FString>& OutLines)
	{
		if (!State.bGameplay)
		{
			OutLines.Add(FString::Printf(TEXT("%s · %s (게임 맵 아님)"), *State.MapName, *State.NetMode));
			return;
		}
		OutLines.Add(FString::Printf(TEXT("%s · %s · 전체 %d일차 · 기간 %d일차"), *State.MapName, *State.NetMode,
			State.TotalDay, State.PeriodDay));
		OutLines.Add(State.DepositedCredits >= 0
			? FString::Printf(TEXT("목표 %d / 납부 %d"), State.TargetCredits, State.DepositedCredits)
			: FString::Printf(TEXT("목표 %d"), State.TargetCredits));
		FString Run = FString::Printf(TEXT("런 %s · 여진 %d단계"), *State.RunPhase, State.AftershockStage);
		if (State.RemainingSeconds >= 0.0)
		{
			Run += FString::Printf(TEXT(" · 남은 %.1f초"), State.RemainingSeconds);
		}
		if (State.bPaymentDeadline)
		{
			Run += TEXT(" · 마감");
		}
		if (State.bGameOver)
		{
			Run += TEXT(" · 게임 오버");
		}
		OutLines.Add(Run);
		OutLines.Add(FString::Printf(TEXT("엘리베이터 %s · 상승 %d회 · 무게 %s"), *State.ElevatorPhase, State.CompletedAscents,
			*State.WeightStage));
		for (const FStateSummary::FPlayer& Player : State.Players)
		{
			OutLines.Add(FString::Printf(TEXT("  #%d %s%s · %s · 잔고 %s"), Player.PlayerId, *Player.Name,
				Player.bSelf ? TEXT("(나)") : TEXT(""), *Player.Life,
				Player.Balance >= 0 ? *FString::FromInt(Player.Balance) : TEXT("비공개")));
		}
	}
}
