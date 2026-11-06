#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Economy/DigetherQuotaStateComponent.h"
#include "Game/Elevator/DigetherElevatorActor.h"
#include "Game/Elevator/DigetherElevatorComponent.h"
#include "Game/Elevator/DigetherElevatorLeverActor.h"
#include "Game/Elevator/DigetherElevatorLeverComponent.h"
#include "Game/Framework/DigetherGameplayGameMode.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Framework/DigetherTestPawn.h"
#include "Game/Hub/DigetherHubCatalog.h"
#include "Game/Hub/DigetherHubFacilityActor.h"
#include "Game/Hub/DigetherHubRules.h"
#include "Game/Hub/DigetherHubStageAnchor.h"
#include "Game/Hub/DigetherHubStateActor.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Run/DigetherRunCoordinatorComponent.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

// 준비만 노출, 거래·점유·레버 시작/유지는 제품 서버 함수 직접 호출
class FDigetherHubWorldTestAccess
{
public:
	static bool Prepare(ADigetherGameplayGameMode& Mode, const FDigetherRunTuning& Tuning)
	{
		Mode.RunTuning = Tuning;
		Mode.GameId = FGuid::NewGuid();
		Mode.CurrentRunId = FGuid::NewGuid();
		Mode.bRunReady = true;
		Mode.bHasSaveStartupAttempted = true;
		const ADigetherGameplayGameState* State = Mode.GetGameState<ADigetherGameplayGameState>();
		return IsValid(State)
			&& State->GetQuotaComponent()->SetQuotaState(FDigetherEconomyRules::MakeInitialQuotaState(Tuning), false)
				== EDigetherEconomyResult::Success
			&& Mode.InitializeRunActors();
	}
#if !UE_BUILD_SHIPPING
	static void SetSmokeRequester(ADigetherGameplayGameMode& Mode, ADigetherGameplayPlayerController* Controller)
	{
		Mode.bSmokeActive = Controller != nullptr;
		Mode.SmokeController = Controller;
	}
#endif
};

namespace
{
	FDigetherRunTuning MakeHubWorldTuning()
	{
		FDigetherRunTuning Tuning;
		Tuning.SourceProfile = FPrimaryAssetId(TEXT("DigetherTuningProfile:DA_HubWorldFixture"));
		Tuning.ProfileVersion = 1;
		Tuning.InitialQuotaCredits = 100;
		Tuning.RunSeconds = 120.f;
		Tuning.InteractionDistanceCm = 200.f;
		Tuning.ElevatorCapacityKg = 500.f;
		Tuning.ElevatorTripLimit = 2;
		Tuning.AscentPenaltySeconds = 10.f;
		Tuning.ElevatorTravelSeconds = 2.f;
		Tuning.DepositIntervalSeconds = .5f;
		Tuning.EmergencyWeightLimitKg = 3.f;
		Tuning.DeathLossPercent = 50;
		Tuning.CorpseRefundPercent = 50;
		return Tuning;
	}

	struct FHubWorldPlayer
	{
		ADigetherGameplayPlayerController* Controller = nullptr;
		ADigetherGameplayPlayerState* State = nullptr;
		APawn* Pawn = nullptr;
		bool IsReady() const { return IsValid(Controller) && IsValid(State) && IsValid(Pawn); }
	};

	// 새 GameInstance의 임시 월드만 사용, 로그인·자동 저장·세션 생성 없음
	class FHubFacilityWorld
	{
	public:
		FHubFacilityWorld()
		{
			if (!GEngine) return;
			Instance = NewObject<UGameInstance>(GEngine);
			Instance->AddToRoot();
			Instance->InitializeStandalone();
			World = Instance->GetWorld();
			const FURL URL(nullptr, TEXT("/Engine/Maps/Entry?game=/Script/DIGETHER.DigetherGameplayGameMode"), TRAVEL_Absolute);
			if (!World || !World->SetGameMode(URL)) return;
			World->InitializeActorsForPlay(URL);
			Mode = World->GetAuthGameMode<ADigetherGameplayGameMode>();
			State = World->GetGameState<ADigetherGameplayGameState>();
			if (!IsValid(Mode) || !IsValid(State)) return;
			Elevator = World->SpawnActor<ADigetherElevatorActor>();
			Departure = AddLever(EDigetherElevatorLeverKind::Departure, FVector(-100, 0, 0));
			Ascent = AddLever(EDigetherElevatorLeverKind::Ascent, FVector(100, 0, 0));
			// 기획 S15: 갱도 상승 레버는 안·밖 하나씩, 밖 레버는 탑승 영역(반폭 250cm) 밖
			AscentOutside = AddLever(EDigetherElevatorLeverKind::Ascent, FVector(400, 0, 0),
				EDigetherElevatorLeverPlacement::Outside);
			if (!IsValid(Elevator) || !IsValid(Departure) || !IsValid(Ascent) || !IsValid(AscentOutside)
				|| !FDigetherHubWorldTestAccess::Prepare(*Mode, MakeHubWorldTuning())) return;
			Hub = World->SpawnActor<ADigetherHubStateActor>();
			if (!IsValid(Hub)) return;
			Catalog = NewObject<UDigetherHubCatalog>(Hub);
			Catalog->Offers = {{FName(TEXT("Wood")), FText::FromString(TEXT("Fixture wood")), 10, 1}};
			Catalog->Stages = {{FName(TEXT("Mine")), FText::FromString(TEXT("Fixture mine")), 0, true},
				{FName(TEXT("Ruins")), FText::FromString(TEXT("Fixture ruins")), 100, false}};
			Catalog->TrialStarterCredits = 0;
			Hub->Catalog = Catalog;
			if (!Hub->RestoreProgress(FDigetherHubProgress())) return;
			AddAnchor(TEXT("Mine"), FVector(2000, 0, 0));
			AddAnchor(TEXT("Ruins"), FVector(4000, 0, 0));
			bReady = true;
		}
		~FHubFacilityWorld()
		{
			if (Instance) Instance->Shutdown();
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine) GEngine->DestroyWorldContext(World);
			}
			if (Instance) Instance->RemoveFromRoot();
		}
		FHubFacilityWorld(const FHubFacilityWorld&) = delete;
		FHubFacilityWorld& operator=(const FHubFacilityWorld&) = delete;
		bool IsReady() const { return bReady; }
		UWorld* GetWorld() const { return World; }
		ADigetherGameplayGameMode* GetMode() const { return Mode; }
		ADigetherHubStateActor* GetHub() const { return Hub; }
		ADigetherElevatorLeverActor* GetDeparture() const { return Departure; }
		ADigetherElevatorLeverActor* GetAscent() const { return Ascent; }
		ADigetherElevatorLeverActor* GetAscentOutside() const { return AscentOutside; }

		FHubWorldPlayer AddPlayer(const FVector& Location = FVector(0, 0, 100), int32 Credits = 200)
		{
			FHubWorldPlayer Player;
			Player.Controller = World->SpawnActor<ADigetherGameplayPlayerController>();
			if (!IsValid(Player.Controller)) return Player;
			Player.State = Player.Controller->GetPlayerState<ADigetherGameplayPlayerState>();
			if (!IsValid(Player.State)) Player.State = World->SpawnActor<ADigetherGameplayPlayerState>();
			if (!IsValid(Player.State)) return Player;
			Player.State->SetOwner(Player.Controller);
			Player.Controller->SetPlayerState(Player.State);
			State->AddPlayerState(Player.State);
			Player.State->InitializeGameplayIdentity();
			Player.State->MarkGameplayInitialized();
			FDigetherWalletState Wallet;
			Wallet.BalanceCredits = Credits;
			Player.State->RestoreState(Wallet, false);
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player.Pawn = World->SpawnActor<ADigetherTestPawn>(Location, FRotator::ZeroRotator, Spawn);
			if (IsValid(Player.Pawn))
			{
				Player.Controller->Possess(Player.Pawn);
				Player.Controller->SetViewTarget(Player.Pawn);
			}
			return Player;
		}

		ADigetherHubFacilityActor* AddFacility(const TCHAR* Id, EDigetherHubFacilityKind Kind,
			const FVector& Location = FVector(150, 0, 100))
		{
			ADigetherHubFacilityActor* Facility = World->SpawnActor<ADigetherHubFacilityActor>();
			Facility->FacilityId = Id;
			Facility->Kind = Kind;
			Facility->SetActorLocation(Location);
			return Facility;
		}

		ADigetherHubStageAnchor* AddAnchor(const TCHAR* Id, const FVector& Location)
		{
			ADigetherHubStageAnchor* Anchor = World->SpawnActor<ADigetherHubStageAnchor>();
			Anchor->StageId = Id;
			Anchor->SetActorLocation(Location);
			return Anchor;
		}

		UBoxComponent* AddWall(const FVector& Center, const FVector& Extent)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
			Actor->AddInstanceComponent(Box);
			Actor->SetRootComponent(Box);
			Box->SetBoxExtent(Extent);
			Box->SetCollisionProfileName(TEXT("BlockAll"));
			Box->SetCollisionObjectType(ECC_WorldStatic);
			Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetWorldLocation(Center);
			Box->RegisterComponent();
			return Box;
		}

		void UpdateBoarding(const FHubWorldPlayer& Player)
		{
			if (!Elevator->HasActorBegunPlay()) Elevator->DispatchBeginPlay();
			if (!Player.Pawn->HasActorBegunPlay()) Player.Pawn->DispatchBeginPlay();
			CastChecked<UPrimitiveComponent>(Player.Pawn->GetRootComponent())->UpdateOverlaps();
			Elevator->GetBoardingVolume()->UpdateOverlaps();
		}

	private:
		ADigetherElevatorLeverActor* AddLever(EDigetherElevatorLeverKind Kind, const FVector& Location,
			EDigetherElevatorLeverPlacement Placement = EDigetherElevatorLeverPlacement::Inside)
		{
			ADigetherElevatorLeverActor* Lever = World->SpawnActor<ADigetherElevatorLeverActor>();
			if (!IsValid(Lever)) return nullptr;
			FEnumProperty* KindProperty = FindFProperty<FEnumProperty>(Lever->GetClass(), TEXT("LeverKind"));
			FEnumProperty* PlacementProperty = FindFProperty<FEnumProperty>(Lever->GetClass(), TEXT("Placement"));
			FObjectPropertyBase* ElevatorProperty = FindFProperty<FObjectPropertyBase>(Lever->GetClass(), TEXT("ElevatorActor"));
			if (!KindProperty || !PlacementProperty || !ElevatorProperty) return nullptr;
			*KindProperty->ContainerPtrToValuePtr<EDigetherElevatorLeverKind>(Lever) = Kind;
			*PlacementProperty->ContainerPtrToValuePtr<EDigetherElevatorLeverPlacement>(Lever) = Placement;
			ElevatorProperty->SetObjectPropertyValue_InContainer(Lever, Elevator);
			Lever->SetActorLocation(Location);
			return Lever;
		}
		UGameInstance* Instance = nullptr;
		UWorld* World = nullptr;
		ADigetherGameplayGameMode* Mode = nullptr;
		ADigetherGameplayGameState* State = nullptr;
		ADigetherElevatorActor* Elevator = nullptr;
		ADigetherElevatorLeverActor* Departure = nullptr;
		ADigetherElevatorLeverActor* Ascent = nullptr;
		ADigetherElevatorLeverActor* AscentOutside = nullptr;
		ADigetherHubStateActor* Hub = nullptr;
		UDigetherHubCatalog* Catalog = nullptr;
		bool bReady = false;
	};

	FDigetherHubView Inspect(FHubFacilityWorld& Fixture, const FHubWorldPlayer& Player, ADigetherHubFacilityActor* Facility)
	{
		return Fixture.GetHub()->Execute(Player.Controller, Facility, EDigetherHubAction::Inspect, NAME_None, FGuid::NewGuid());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherHubWorldAuthorizationTest,
	"DIGETHER.Game.Hub.World.Authorization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherHubWorldAuthorizationTest::RunTest(const FString& Parameters)
{
	FHubFacilityWorld Fixture;
	if (!TestTrue(TEXT("서버 시설 월드 준비"), Fixture.IsReady())) return false;
	FHubWorldPlayer Buyer = Fixture.AddPlayer();
	FHubWorldPlayer Other = Fixture.AddPlayer(FVector(0, 100, 100));
	if (!TestTrue(TEXT("소유 Controller·PlayerState·Pawn 준비"), Buyer.IsReady() && Other.IsReady())) return false;
	ADigetherHubFacilityActor* Shop = Fixture.AddFacility(TEXT("Shop"), EDigetherHubFacilityKind::Shop);
	TestTrue(TEXT("정상 소유자 열기"), Inspect(Fixture, Buyer, Shop).bInteractionAllowed);
	const int32 Balance = Buyer.State->CaptureState().BalanceCredits;
	auto RejectPurchase = [&]()
	{
		const FDigetherHubView View = Fixture.GetHub()->Execute(Buyer.Controller, Shop,
			EDigetherHubAction::Purchase, TEXT("Wood"), FGuid::NewGuid());
		TestFalse(TEXT("잘못된 상호작용은 서버에서 거절"), View.bInteractionAllowed);
		TestEqual(TEXT("거절은 지갑을 보존"), Buyer.State->CaptureState().BalanceCredits, Balance);
	};
	Buyer.Pawn->SetActorLocation(FVector(1000, 0, 100));
	RejectPurchase();
	Buyer.Pawn->SetActorLocation(FVector(0, 0, 100));
	UBoxComponent* Wall = Fixture.AddWall(FVector(75, 0, 100), FVector(10, 100, 150));
	RejectPurchase();
	Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Buyer.State->GetLifeComponent()->ApplyDeath(EDigetherDeathCause::Killed, false);
	RejectPurchase();
	Buyer.State->GetLifeComponent()->Revive(false);
	Buyer.State->SetOwner(Other.Controller);
	RejectPurchase();
	Buyer.State->SetOwner(Buyer.Controller);
	ADigetherHubFacilityActor* Duplicate = Fixture.AddFacility(TEXT("Shop"), EDigetherHubFacilityKind::Shop);
	RejectPurchase();
	Duplicate->Destroy();
	Fixture.GetHub()->SetRole(ROLE_SimulatedProxy);
	RejectPurchase();
	Fixture.GetHub()->SetRole(ROLE_Authority);
	TestTrue(TEXT("원상복구 후 정상 상호작용"), Inspect(Fixture, Buyer, Shop).bInteractionAllowed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherHubWorldPurchaseDeliveryTest,
	"DIGETHER.Game.Hub.World.PurchaseDelivery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherHubWorldPurchaseDeliveryTest::RunTest(const FString& Parameters)
{
	FHubFacilityWorld Fixture;
	if (!TestTrue(TEXT("서버 시설 월드 준비"), Fixture.IsReady())) return false;
	FHubWorldPlayer Buyer = Fixture.AddPlayer();
	FHubWorldPlayer Other = Fixture.AddPlayer(FVector(0, 100, 100));
	if (!TestTrue(TEXT("두 플레이어 준비"), Buyer.IsReady() && Other.IsReady())) return false;
	ADigetherHubFacilityActor* Shop = Fixture.AddFacility(TEXT("Shop"), EDigetherHubFacilityKind::Shop);
	ADigetherHubFacilityActor* Delivery = Fixture.AddFacility(TEXT("Delivery"), EDigetherHubFacilityKind::Delivery, FVector(150, 100, 100));
	Inspect(Fixture, Buyer, Shop);
	Inspect(Fixture, Other, Shop);
	const FGuid Request = FGuid::NewGuid();
	TestTrue(TEXT("실제 액터의 서버 구매"), Fixture.GetHub()->Execute(Buyer.Controller, Shop,
		EDigetherHubAction::Purchase, TEXT("Wood"), Request).bSuccess);
	Fixture.GetHub()->Execute(Buyer.Controller, Shop, EDigetherHubAction::Purchase, TEXT("Wood"), Request);
	TestEqual(TEXT("같은 요청 재전송은 한 번만 차감"), Buyer.State->CaptureState().BalanceCredits, 190);
	FDigetherHubProgress Progress = Fixture.GetHub()->CaptureProgress();
	const FDigetherHubPlayerProgress* Record = DigetherHubRules::FindPlayer(Progress, Buyer.State->GetGameplayPlayerId());
	if (!TestNotNull(TEXT("구매자의 비공개 배송 기록"), Record)) return false;
	TestEqual(TEXT("재전송으로 배송을 복제하지 않음"), DigetherHubRules::GetItemCount(Record->PendingDelivery, TEXT("Wood")), 1);
	TestFalse(TEXT("타인이 구매자의 배송을 받을 수 없음"), Fixture.GetHub()->Execute(Other.Controller, Delivery,
		EDigetherHubAction::ClaimDelivery, NAME_None, FGuid::NewGuid()).bSuccess);
	TestTrue(TEXT("구매자 본인이 수령"), Fixture.GetHub()->Execute(Buyer.Controller, Delivery,
		EDigetherHubAction::ClaimDelivery, NAME_None, FGuid::NewGuid()).bSuccess);
	Progress = Fixture.GetHub()->CaptureProgress();
	Record = DigetherHubRules::FindPlayer(Progress, Buyer.State->GetGameplayPlayerId());
	TestEqual(TEXT("본인 보유 수량에 지급"), DigetherHubRules::GetItemCount(Record->Inventory, TEXT("Wood")), 1);
	TestTrue(TEXT("배송 대기열은 비움"), Record->PendingDelivery.IsEmpty());
	const FDigetherHubPlayerProgress* OtherRecord = DigetherHubRules::FindPlayer(Progress, Other.State->GetGameplayPlayerId());
	TestTrue(TEXT("타인 보유 수량에는 노출·지급하지 않음"), OtherRecord && OtherRecord->Inventory.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherHubWorldBedCleanupTest,
	"DIGETHER.Game.Hub.World.BedRelease", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherHubWorldBedCleanupTest::RunTest(const FString& Parameters)
{
	FHubFacilityWorld Fixture;
	if (!TestTrue(TEXT("서버 시설 월드 준비"), Fixture.IsReady())) return false;
	FHubWorldPlayer First = Fixture.AddPlayer();
	FHubWorldPlayer Second = Fixture.AddPlayer(FVector(0, 100, 100));
	if (!TestTrue(TEXT("두 플레이어 준비"), First.IsReady() && Second.IsReady())) return false;
	ADigetherHubFacilityActor* Bed = Fixture.AddFacility(TEXT("Bed"), EDigetherHubFacilityKind::Bed);
	Inspect(Fixture, First, Bed);
	Inspect(Fixture, Second, Bed);
	const FTransform Before = First.Pawn->GetActorTransform();
	TestTrue(TEXT("빈 침대에서 실제 휴식 시작"), Fixture.GetHub()->Execute(First.Controller, Bed,
		EDigetherHubAction::Rest, NAME_None, FGuid::NewGuid()).bResting);
	TestFalse(TEXT("점유 중 침대의 두 번째 휴식 거절"), Fixture.GetHub()->Execute(Second.Controller, Bed,
		EDigetherHubAction::Rest, NAME_None, FGuid::NewGuid()).bSuccess);
	TestFalse(TEXT("휴식 위치로 실제 Pawn 이동"), First.Pawn->GetActorTransform().Equals(Before));
	First.State->GetLifeComponent()->ApplyDeath(EDigetherDeathCause::Killed, false);
	TestTrue(TEXT("사망 이후에도 자기 휴식 해제 요청 수락"), Fixture.GetHub()->Execute(First.Controller, Bed,
		EDigetherHubAction::Stand, NAME_None, FGuid::NewGuid()).bSuccess);
	TestTrue(TEXT("해제 후 원래 위치 복귀"), First.Pawn->GetActorTransform().Equals(Before));
	TestTrue(TEXT("점유 해제 후 다른 플레이어가 휴식"), Fixture.GetHub()->Execute(Second.Controller, Bed,
		EDigetherHubAction::Rest, NAME_None, FGuid::NewGuid()).bResting);
	Fixture.GetHub()->EndAllRest();
	TestFalse(TEXT("출발 전 전체 휴식 정리가 점유를 해제"), Fixture.GetHub()->BuildView(Second.Controller, Bed).bResting);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherHubWorldStageTest,
	"DIGETHER.Game.Hub.World.SharedStage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherHubWorldStageTest::RunTest(const FString& Parameters)
{
	FHubFacilityWorld Fixture;
	if (!TestTrue(TEXT("서버 시설 월드 준비"), Fixture.IsReady())) return false;
	FHubWorldPlayer First = Fixture.AddPlayer();
	FHubWorldPlayer Second = Fixture.AddPlayer(FVector(0, 100, 100));
	if (!TestTrue(TEXT("두 플레이어 준비"), First.IsReady() && Second.IsReady())) return false;
	ADigetherHubFacilityActor* Panel = Fixture.AddFacility(TEXT("Stage"), EDigetherHubFacilityKind::Stage);
	Inspect(Fixture, First, Panel);
	Inspect(Fixture, Second, Panel);
	TestTrue(TEXT("첫 해금 요청 확정"), Fixture.GetHub()->Execute(First.Controller, Panel,
		EDigetherHubAction::UnlockStage, TEXT("Ruins"), FGuid::NewGuid()).bSuccess);
	TestFalse(TEXT("다른 소유자의 동일 해금은 중복 차감하지 않음"), Fixture.GetHub()->Execute(Second.Controller, Panel,
		EDigetherHubAction::UnlockStage, TEXT("Ruins"), FGuid::NewGuid()).bSuccess);
	TestEqual(TEXT("최초 요청자만 비용 차감"), First.State->CaptureState().BalanceCredits, 100);
	TestEqual(TEXT("후속 요청자의 잔고 보존"), Second.State->CaptureState().BalanceCredits, 200);
	TestTrue(TEXT("누구나 팀이 해금한 목적지 선택"), Fixture.GetHub()->Execute(Second.Controller, Panel,
		EDigetherHubAction::SelectStage, TEXT("Ruins"), FGuid::NewGuid()).bSuccess);
	Fixture.GetHub()->LockDestination(true);
	TestFalse(TEXT("레버 점유 이후 목적지 변경 거절"), Fixture.GetHub()->Execute(First.Controller, Panel,
		EDigetherHubAction::SelectStage, TEXT("Mine"), FGuid::NewGuid()).bSuccess);
	TestEqual(TEXT("고정한 목적지 유지"), Fixture.GetHub()->GetSelectedStageId(), FName(TEXT("Ruins")));
	FTransform Arrival;
	TestTrue(TEXT("선택한 실제 앵커를 조회"), Fixture.GetHub()->GetSelectedStageTransform(Arrival));
	TestTrue(TEXT("선택 앵커 좌표 일치"), Arrival.GetLocation().Equals(FVector(4000, 0, 0)));
	ADigetherHubStageAnchor* Duplicate = Fixture.AddAnchor(TEXT("Ruins"), FVector(6000, 0, 0));
	TestFalse(TEXT("중복 앵커는 목적지 확정을 거절"), Fixture.GetHub()->GetSelectedStageTransform(Arrival));
	Duplicate->Destroy();
	TestTrue(TEXT("실제 엘리베이터·레버 이동"), Fixture.GetHub()->MoveElevatorToSelectedStage());
	TestTrue(TEXT("출발 레버도 선택 앵커로 이동"), Fixture.GetDeparture()->GetActorLocation().Equals(FVector(3900, 0, 0)));
	TestTrue(TEXT("안 상승 레버도 선택 앵커로 이동"), Fixture.GetAscent()->GetActorLocation().Equals(FVector(4100, 0, 0)));
	TestTrue(TEXT("밖 상승 레버도 선택 앵커로 이동"), Fixture.GetAscentOutside()->GetActorLocation().Equals(FVector(4400, 0, 0)));
	Fixture.GetHub()->ReturnElevatorToHub();
	TestTrue(TEXT("거점 복귀 시 출발 레버 원위치"), Fixture.GetDeparture()->GetActorLocation().Equals(FVector(-100, 0, 0)));
	TestTrue(TEXT("거점 복귀 시 밖 상승 레버 원위치"), Fixture.GetAscentOutside()->GetActorLocation().Equals(FVector(400, 0, 0)));
	TestFalse(TEXT("거점 복귀 시 목적지 고정 해제"), Fixture.GetHub()->bDestinationLocked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherHubWorldLeverRetentionTest,
	"DIGETHER.Game.Hub.World.LeverRetention", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherHubWorldLeverRetentionTest::RunTest(const FString& Parameters)
{
	FHubFacilityWorld Fixture;
	if (!TestTrue(TEXT("서버 시설 월드 준비"), Fixture.IsReady())) return false;
	FHubWorldPlayer Operator = Fixture.AddPlayer();
	FHubWorldPlayer Other = Fixture.AddPlayer(FVector(1000, 0, 100));
	if (!TestTrue(TEXT("두 플레이어 준비"), Operator.IsReady() && Other.IsReady())) return false;
	Fixture.UpdateBoarding(Operator);
	ADigetherElevatorLeverActor* Lever = Fixture.GetDeparture();
#if !UE_BUILD_SHIPPING
	FDigetherHubWorldTestAccess::SetSmokeRequester(*Fixture.GetMode(), Operator.Controller);
	TestFalse(TEXT("호스트 smoke는 먼 타인의 레버 요청 거리 검사를 끄지 않음"),
		Fixture.GetMode()->BeginLeverPull(Other.Controller, Lever));
	FDigetherHubWorldTestAccess::SetSmokeRequester(*Fixture.GetMode(), nullptr);
#endif
	if (!TestTrue(TEXT("가까운 활성 플레이어가 레버 잡기"), Fixture.GetMode()->BeginLeverPull(Operator.Controller, Lever))) return false;
	Fixture.GetMode()->Tick(.5f);
	TestTrue(TEXT("정상 유지 중 게이지 진행"), Lever->GetLeverComponent()->GetPublicState().Progress > 0);
	Operator.Pawn->SetActorLocation(FVector(1000, 0, 100));
	Fixture.GetMode()->Tick(.2f);
	TestFalse(TEXT("유지 중 거리 이탈은 실제 점유 해제"), Lever->GetLeverComponent()->GetActivePullerId().IsValid());
	TestEqual(TEXT("이탈 후 게이지 초기화"), Lever->GetLeverComponent()->GetPublicState().Progress, 0.f);
	TestFalse(TEXT("이탈 후 목적지 변경 가능"), Fixture.GetHub()->bDestinationLocked);
	Operator.Pawn->SetActorLocation(FVector(0, 0, 100));
	Fixture.UpdateBoarding(Operator);
	if (!TestTrue(TEXT("복귀 후 다시 레버 잡기"), Fixture.GetMode()->BeginLeverPull(Operator.Controller, Lever))) return false;
	Operator.State->GetLifeComponent()->ApplyDeath(EDigetherDeathCause::Killed, false);
	Fixture.GetMode()->Tick(.2f);
	TestFalse(TEXT("잡기 도중 사망하면 실제 점유 해제"), Lever->GetLeverComponent()->GetActivePullerId().IsValid());
	TestEqual(TEXT("사망 후 게이지 초기화"), Lever->GetLeverComponent()->GetPublicState().Progress, 0.f);
	TestFalse(TEXT("사망 후 목적지 고정 해제"), Fixture.GetHub()->bDestinationLocked);
	return true;
}

#endif
