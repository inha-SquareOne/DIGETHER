#if WITH_DEV_AUTOMATION_TESTS

// 아래 수치·변환은 연출 수명 검사 픽스처, 게임 기획 값 아님

#include "Game/Hub/DigetherHubWellVisualComponent.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Run/DigetherGameOverPublicStateComponent.h"
#include "Game/Tests/DigetherTestValues.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

namespace
{
	class FDigetherWellVisualFixture
	{
	public:
		FDigetherWellVisualFixture()
		{
			UWorld::InitializationValues Values;
			Values.InitializeScenes(false).AllowAudioPlayback(false).CreatePhysicsScene(false)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false)
				.CreateFXSystem(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &Values);
			if (!World)
			{
				return;
			}
			Owner = World->SpawnActor<AActor>();
			if (!Owner)
			{
				return;
			}
			USceneComponent* Root = NewObject<USceneComponent>(Owner, TEXT("Root"));
			Owner->AddInstanceComponent(Root);
			Owner->SetRootComponent(Root);
			Root->RegisterComponent();
			Flood = AddScene<UStaticMeshComponent>(TEXT("Flood"), *Root);
			Suction = AddScene<UStaticMeshComponent>(TEXT("Suction"), *Root);
			Glow = AddScene<UPointLightComponent>(TEXT("Glow"), *Root);
			Flood->SetRelativeTransform(FTransform(FRotator(0.f, 25.f, 0.f), FVector(10.f, 20.f, 30.f), FVector(3.f, 2.f, 1.f)));
			Suction->SetRelativeTransform(FTransform(FRotator(0.f, 15.f, 0.f), FVector(4.f, 5.f, 60.f), FVector(4.f, 4.f, 6.f)));
			Glow->SetRelativeTransform(FTransform(FRotator::ZeroRotator, FVector(7.f, 8.f, 90.f)));
			Flood->SetVisibility(false);
			Flood->SetHiddenInGame(true);
			Suction->SetVisibility(true);
			Suction->SetHiddenInGame(false);
			Glow->SetVisibility(false);
			Glow->SetHiddenInGame(true);
			Glow->SetLightFColor(FColor(33, 77, 149, 255));
			Glow->SetIntensity(123.f);
			// 연출 컴포넌트가 판정 설정을 바꾸지 않는지 검사, 실제 BP는 NoCollision 사용
			Flood->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Flood->SetGenerateOverlapEvents(true);
			Flood->SetCanEverAffectNavigation(true);
			Visual = NewObject<UDigetherHubWellVisualComponent>(Owner);
			Owner->AddInstanceComponent(Visual);
			Visual->RegisterComponent();
		}

		~FDigetherWellVisualFixture()
		{
			if (World)
			{
				World->DestroyWorld(false);
			}
		}

		FDigetherWellVisualFixture(const FDigetherWellVisualFixture&) = delete;
		FDigetherWellVisualFixture& operator=(const FDigetherWellVisualFixture&) = delete;

		UWorld* World = nullptr;
		AActor* Owner = nullptr;
		UStaticMeshComponent* Flood = nullptr;
		UStaticMeshComponent* Suction = nullptr;
		UPointLightComponent* Glow = nullptr;
		UDigetherHubWellVisualComponent* Visual = nullptr;

	private:
		template <typename TComponent>
		TComponent* AddScene(const TCHAR* Name, USceneComponent& Parent)
		{
			TComponent* Component = NewObject<TComponent>(Owner, Name);
			Owner->AddInstanceComponent(Component);
			Component->SetMobility(EComponentMobility::Movable);
			Component->SetupAttachment(&Parent);
			Component->RegisterComponent();
			return Component;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherWellVisualLifecycleTest,
	"DIGETHER.Game.Hub.WellVisual.PlayPauseRepeatAndRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherWellVisualLifecycleTest::RunTest(const FString& Parameters)
{
	FDigetherWellVisualFixture Fixture;
	if (!TestNotNull(TEXT("연출 검사 컴포넌트"), Fixture.Visual))
	{
		return false;
	}
	const FTransform FloodBefore = Fixture.Flood->GetRelativeTransform();
	const FTransform SuctionBefore = Fixture.Suction->GetRelativeTransform();
	const FTransform GlowBefore = Fixture.Glow->GetRelativeTransform();
	const FColor GlowColorBefore = Fixture.Glow->LightColor;
	const float IntensityBefore = Fixture.Glow->Intensity;
	TestFalse(TEXT("기본 Tick 꺼짐"), Fixture.Visual->IsComponentTickEnabled());
	TestFalse(TEXT("연출 컴포넌트 복제 없음"), Fixture.Visual->GetIsReplicated());
	TestFalse(TEXT("시작 전 흡입 거절"), Fixture.Visual->StartSuction());
	TestTrue(TEXT("범람 시작"), Fixture.Visual->StartFlood(4.f, 8.f, Fixture.Flood, Fixture.Suction, Fixture.Glow));
	const FTransform FloodAtStart = Fixture.Flood->GetRelativeTransform();
	TestTrue(TEXT("활성 중 Tick 켜짐"), Fixture.Visual->IsComponentTickEnabled());
	TestTrue(TEXT("작은 수면 표시"), Fixture.Flood->GetVisibleFlag() && !Fixture.Flood->bHiddenInGame
		&& Fixture.Flood->GetRelativeScale3D().X < FloodBefore.GetScale3D().X);
	TestTrue(TEXT("흡입 형상은 아직 숨김"), !Fixture.Suction->GetVisibleFlag() && Fixture.Suction->bHiddenInGame);
	Fixture.Visual->TickComponent(2.f, LEVELTICK_All, nullptr);
	const FTransform FloodAtHalf = Fixture.Flood->GetRelativeTransform();
	TestTrue(TEXT("범람 중 확산·높이 상승"), FloodAtHalf.GetScale3D().X > FloodAtStart.GetScale3D().X
		&& FloodAtHalf.GetLocation().Z > FloodAtStart.GetLocation().Z);
	Fixture.Visual->TickComponent(2.f, LEVELTICK_PauseTick, nullptr);
	TestTrue(TEXT("일시정지 중 진행 없음"), Fixture.Flood->GetRelativeTransform().Equals(FloodAtHalf));
	Fixture.Visual->TickComponent(DigetherTestValues::NaN(), LEVELTICK_All, nullptr);
	TestTrue(TEXT("비유한 Delta는 변환 불변"), Fixture.Flood->GetRelativeTransform().Equals(FloodAtHalf));
	TestTrue(TEXT("서버 흡입 시작 이벤트"), Fixture.Visual->StartSuction());
	TestTrue(TEXT("흡입 형상 표시"), Fixture.Suction->GetVisibleFlag() && !Fixture.Suction->bHiddenInGame);
	const FTransform SuctionAtStart = Fixture.Suction->GetRelativeTransform();
	TestTrue(TEXT("대칭 Cone을 타원으로 표시"), SuctionAtStart.GetScale3D().Y < SuctionAtStart.GetScale3D().X);
	Fixture.Visual->TickComponent(0.25f, LEVELTICK_All, nullptr);
	const FTransform SuctionMoving = Fixture.Suction->GetRelativeTransform();
	TestFalse(TEXT("흡입 회전"), SuctionMoving.GetRotation().Equals(SuctionAtStart.GetRotation()));
	TestFalse(TEXT("흡입 맥동"), SuctionMoving.GetScale3D().Equals(SuctionAtStart.GetScale3D()));
	TestTrue(TEXT("흡입 중 중복 시작 허용"), Fixture.Visual->StartSuction());
	TestTrue(TEXT("중복 흡입은 진행 유지"), Fixture.Suction->GetRelativeTransform().Equals(SuctionMoving));
	TestTrue(TEXT("반복 범람 시작"), Fixture.Visual->StartFlood(4.f, 8.f, Fixture.Flood, Fixture.Suction, Fixture.Glow));
	TestTrue(TEXT("반복 시작 변환 누적 없음"), Fixture.Flood->GetRelativeTransform().Equals(FloodAtStart));
	Fixture.Visual->StopVisuals();
	TestTrue(TEXT("수면 변환 정확 복원"), Fixture.Flood->GetRelativeTransform().Equals(FloodBefore));
	TestTrue(TEXT("흡입 변환 정확 복원"), Fixture.Suction->GetRelativeTransform().Equals(SuctionBefore));
	TestTrue(TEXT("조명 변환 정확 복원"), Fixture.Glow->GetRelativeTransform().Equals(GlowBefore));
	TestTrue(TEXT("원래 수면 가시성 복원"), !Fixture.Flood->GetVisibleFlag() && Fixture.Flood->bHiddenInGame);
	TestTrue(TEXT("원래 흡입 가시성 복원"), Fixture.Suction->GetVisibleFlag() && !Fixture.Suction->bHiddenInGame);
	TestTrue(TEXT("원래 조명 가시성 복원"), !Fixture.Glow->GetVisibleFlag() && Fixture.Glow->bHiddenInGame);
	TestEqual(TEXT("FColor 감마 변환 없이 정확 복원"), Fixture.Glow->LightColor, GlowColorBefore);
	TestEqual(TEXT("조명 강도 복원"), Fixture.Glow->Intensity, IntensityBefore);
	TestFalse(TEXT("중지 뒤 Tick 꺼짐"), Fixture.Visual->IsComponentTickEnabled());
	Fixture.Visual->StopVisuals();
	TestTrue(TEXT("반복 중지는 멱등"), Fixture.Flood->GetRelativeTransform().Equals(FloodBefore));
	TestTrue(TEXT("충돌·overlap·nav 설정 불변"), Fixture.Flood->GetCollisionEnabled() == ECollisionEnabled::QueryOnly
		&& Fixture.Flood->GetGenerateOverlapEvents() && Fixture.Flood->CanEverAffectNavigation());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherWellVisualInvalidAndEndPlayTest,
	"DIGETHER.Game.Hub.WellVisual.InvalidInputsAndEndPlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherWellVisualInvalidAndEndPlayTest::RunTest(const FString& Parameters)
{
	FDigetherWellVisualFixture Fixture;
	if (!TestNotNull(TEXT("연출 검사 컴포넌트"), Fixture.Visual))
	{
		return false;
	}
	const FTransform FloodBefore = Fixture.Flood->GetRelativeTransform();
	TestFalse(TEXT("모든 컴포넌트 누락 거절"), Fixture.Visual->StartFlood(1.f, 2.f, nullptr, nullptr, nullptr));
	TestFalse(TEXT("음수 시간 거절"), Fixture.Visual->StartFlood(-1.f, 2.f, Fixture.Flood, nullptr, nullptr));
	TestFalse(TEXT("NaN 시간 거절"), Fixture.Visual->StartFlood(DigetherTestValues::NaN(), 2.f, Fixture.Flood, nullptr, nullptr));
	TestFalse(TEXT("무한 마감 시간 거절"), Fixture.Visual->StartFlood(1.f, DigetherTestValues::Infinity(), Fixture.Flood, nullptr, nullptr));
	TestFalse(TEXT("같은 메시를 수면·흡입에 동시 지정 거절"), Fixture.Visual->StartFlood(1.f, 2.f, Fixture.Flood, Fixture.Flood, nullptr));
	TestFalse(TEXT("거절 뒤 Tick 꺼짐"), Fixture.Visual->IsComponentTickEnabled());
	TestTrue(TEXT("0초 범람은 즉시 완료"), Fixture.Visual->StartFlood(0.f, 2.f, Fixture.Flood, nullptr, nullptr));
	TestTrue(TEXT("0초 수면 확산"), Fixture.Flood->GetRelativeScale3D().X > FloodBefore.GetScale3D().X);
	TestFalse(TEXT("활성 중 잘못된 재시작 거절"), Fixture.Visual->StartFlood(1.f, -1.f, Fixture.Flood, nullptr, nullptr));
	TestTrue(TEXT("거절된 재시작은 원래 변환 복원"), Fixture.Flood->GetRelativeTransform().Equals(FloodBefore));
	TestTrue(TEXT("선택 조명만 있어도 재생"), Fixture.Visual->StartFlood(1.f, 2.f, nullptr, nullptr, Fixture.Glow));
	Fixture.Visual->StopVisuals();

	Fixture.Owner->DispatchBeginPlay();
	TestTrue(TEXT("종료 검사 BeginPlay"), Fixture.Visual->HasBegunPlay());
	TestTrue(TEXT("종료 전 범람 시작"), Fixture.Visual->StartFlood(1.f, 2.f, Fixture.Flood, Fixture.Suction, Fixture.Glow));
	Fixture.Visual->StartSuction();
	Fixture.Visual->TickComponent(0.2f, LEVELTICK_All, nullptr);
	Fixture.Visual->EndPlay(EEndPlayReason::RemovedFromWorld);
	TestTrue(TEXT("EndPlay 수면 복원"), Fixture.Flood->GetRelativeTransform().Equals(FloodBefore));
	TestEqual(TEXT("EndPlay 조명 FColor 복원"), Fixture.Glow->LightColor, FColor(33, 77, 149, 255));
	TestEqual(TEXT("EndPlay 조명 강도 복원"), Fixture.Glow->Intensity, 123.f);
	TestFalse(TEXT("EndPlay Tick 꺼짐"), Fixture.Visual->IsComponentTickEnabled());
	TestFalse(TEXT("EndPlay 이후 흡입 시작 거절"), Fixture.Visual->StartSuction());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherWellVisualLatePublicTimeTest,
	"DIGETHER.Game.Hub.WellVisual.LateStartUsesPublicServerTime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherWellVisualLatePublicTimeTest::RunTest(const FString& Parameters)
{
	FDigetherWellVisualFixture Fixture;
	if (!TestNotNull(TEXT("연출 검사 컴포넌트"), Fixture.Visual))
	{
		return false;
	}
	const FTransform FloodBefore = Fixture.Flood->GetRelativeTransform();
	ADigetherGameplayGameState* State = Fixture.World->SpawnActor<ADigetherGameplayGameState>();
	if (!TestNotNull(TEXT("공개 시계 GameState"), State))
	{
		return false;
	}
	Fixture.World->SetGameState(State);
	UDigetherGameOverPublicStateComponent* Public = State->GetGameOverPublicStateComponent();
	FDigetherGameOverPublicState Snapshot;
	Snapshot.Phase = EDigetherGameOverPhase::FailureEvent;
	Snapshot.Sequence = 1;
	Snapshot.Tuning.FailMotionSeconds = 10.f;
	Snapshot.Tuning.FloodRatio = 0.5f;
	Snapshot.PhaseStartServerSeconds = State->GetServerWorldTimeSeconds() - 3.0;
	TestTrue(TEXT("이미 3초 지난 실패 상태"), Public->SetPublicState(Snapshot));
	TestTrue(TEXT("부모가 남은 범람 시간으로 호출"), Fixture.Visual->StartFlood(Public->GetSecondsUntilSuction(),
		Snapshot.Tuning.FailMotionSeconds, Fixture.Flood, Fixture.Suction, Fixture.Glow));
	const FTransform LateStart = Fixture.Flood->GetRelativeTransform();
	TestTrue(TEXT("늦은 시작은 작은 수면부터 다시 재생하지 않음"), LateStart.GetScale3D().X > FloodBefore.GetScale3D().X
		&& LateStart.GetLocation().Z > FloodBefore.GetLocation().Z);
	// 서버 시각이 그대로면 로컬 Delta가 커도 진행하지 않음
	Fixture.Visual->TickComponent(2.f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("서버 시계가 로컬 Delta보다 우선"), Fixture.Flood->GetRelativeTransform().Equals(LateStart));
	Snapshot.PhaseStartServerSeconds -= 1.0;
	TestTrue(TEXT("공개 기준으로 1초 더 지난 상태"), Public->SetPublicState(Snapshot));
	Fixture.Visual->TickComponent(0.01f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("공개 서버 시계 진행 반영"), Fixture.Flood->GetRelativeScale3D().X > LateStart.GetScale3D().X);
	Snapshot.Tuning.FloodRatio = 0.f;
	TestTrue(TEXT("0초 범람 공개 튜닝"), Public->SetPublicState(Snapshot));
	TestTrue(TEXT("0초 범람 재시작"), Fixture.Visual->StartFlood(0.f, 10.f, Fixture.Flood, Fixture.Suction, Fixture.Glow));
	TestFalse(TEXT("0초 분모에서도 유한 변환"), Fixture.Flood->GetRelativeTransform().ContainsNaN());
	Snapshot.Phase = EDigetherGameOverPhase::Credits;
	TestTrue(TEXT("실패 사건 종료 공개"), Public->SetPublicState(Snapshot));
	Fixture.Visual->TickComponent(0.01f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("실제 사건 종료 뒤 Tick 해제"), Fixture.Visual->IsComponentTickEnabled());
	TestTrue(TEXT("실제 사건 종료 뒤 원래 수면 복원"), Fixture.Flood->GetRelativeTransform().Equals(FloodBefore));
	return true;
}

#endif
