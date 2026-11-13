#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Game/Player/DigetherPlayerCharacter.h"
#include "Game/Player/DigetherPlayerHealthComponent.h"
#include "Misc/AutomationTest.h"

namespace
{
	// 디스크 맵·기존 에디터 월드 미사용, 조기 반환에도 월드 정리
	class FDigetherPlayerHealthTestWorld
	{
	public:
		FDigetherPlayerHealthTestWorld()
		{
			UWorld::InitializationValues Values;
			Values.InitializeScenes(false).AllowAudioPlayback(false).CreatePhysicsScene(false)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false)
				.CreateFXSystem(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &Values);
		}

		~FDigetherPlayerHealthTestWorld()
		{
			if (World != nullptr)
			{
				World->DestroyWorld(false);
			}
		}

		FDigetherPlayerHealthTestWorld(const FDigetherPlayerHealthTestWorld&) = delete;
		FDigetherPlayerHealthTestWorld& operator=(const FDigetherPlayerHealthTestWorld&) = delete;

		UWorld* World = nullptr;
	};
}

// 실제 캐릭터를 띄워 표(MaxHealth) → 체력 컴포넌트 → 엔진 피해 방송(OnTakeAnyDamage) → 사망 시 이동 정지까지 연결을 확인한다
// 네트워크 복제와 PlayerState 생존 상태 통지는 이 테스트 범위 밖(PIE·M2에서 확인)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherPlayerHealthComponentTest,
	"DIGETHER.Game.Player.Health.CharacterDamage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherPlayerHealthComponentTest::RunTest(const FString& Parameters)
{
	FDigetherPlayerHealthTestWorld Fixture;
	if (!TestNotNull(TEXT("임시 게임 월드"), Fixture.World))
	{
		return false;
	}
	// 액터 초기화(PostInitializeComponents → 표 적용)는 월드가 액터 초기화를 마쳐야 실행된다
	Fixture.World->InitializeActorsForPlay(FURL());
	ADigetherPlayerCharacter* Character = Fixture.World->SpawnActor<ADigetherPlayerCharacter>();
	if (!TestNotNull(TEXT("플레이어 캐릭터 생성"), Character))
	{
		return false;
	}
	UDigetherPlayerHealthComponent* Health = Character->GetHealthComponent();
	if (!TestNotNull(TEXT("체력 컴포넌트"), Health))
	{
		return false;
	}
	TestTrue(TEXT("표 적용됨"), Character->HasAppliedPlayerStats());
	TestTrue(TEXT("체력 준비됨(표 MaxHealth)"), Health->IsReady());
	TestEqual(TEXT("최대 체력은 표 값 100"), Health->GetMaxHealth(), 100.f);
	TestEqual(TEXT("시작 체력 가득"), Health->GetCurrentHealth(), 100.f);

	// 월드 BeginPlay 없이 이 액터만 시작해 피해 구독·사망 처리 구독을 연결한다
	Character->DispatchBeginPlay();
	TestTrue(TEXT("캐릭터 시작됨"), Character->HasActorBegunPlay());

	// 엔진 ApplyDamage는 APawn::ShouldTakeDamage에서 권위 GameMode를 요구한다(실제 게임에는 항상 있음)
	// 이 임시 월드에는 GameMode가 없으므로, 그 다음 단계인 OnTakeAnyDamage 방송을 직접 보내 연결만 확인한다
	const auto SendDamage = [](ADigetherPlayerCharacter* Target, float Amount)
	{
		Target->OnTakeAnyDamage.Broadcast(Target, Amount, GetDefault<UDamageType>(), nullptr, nullptr);
	};

	int32 DiedCount = 0;
	Health->OnDied.AddLambda([&DiedCount]() { ++DiedCount; });

	SendDamage(Character, 12.5f);
	TestEqual(TEXT("12.5 피해 후 87.5"), Health->GetCurrentHealth(), 87.5f);
	TestFalse(TEXT("아직 생존"), Character->IsDead());

	SendDamage(Character, 1000.f);
	TestEqual(TEXT("초과 피해 후 0"), Health->GetCurrentHealth(), 0.f);
	TestTrue(TEXT("사망"), Character->IsDead());
	TestEqual(TEXT("사망 통지 1회"), DiedCount, 1);
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	TestTrue(TEXT("사망 시 이동 정지(MOVE_None)"), Movement != nullptr && Movement->MovementMode == MOVE_None);

	SendDamage(Character, 10.f);
	TestEqual(TEXT("사망 후 추가 피해에도 사망 통지 1회"), DiedCount, 1);

	// 개발 명령·부활 경로: 가득 채우고 이동 복구
	Health->ResetToFull();
	TestFalse(TEXT("초기화 후 생존"), Character->IsDead());
	TestEqual(TEXT("초기화 후 100"), Health->GetCurrentHealth(), 100.f);
	TestTrue(TEXT("초기화 후 걷기 복구"), Movement != nullptr && Movement->MovementMode == MOVE_Walking);

	return true;
}

#endif
