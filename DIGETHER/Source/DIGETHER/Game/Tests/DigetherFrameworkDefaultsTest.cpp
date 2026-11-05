#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SphereComponent.h"
#include "Game/Dev/DigetherCheatManager.h"
#include "Game/Economy/DigetherQuotaStateComponent.h"
#include "Game/Economy/DigetherWalletComponent.h"
#include "Game/Framework/DigetherGameplayGameMode.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Framework/DigetherTestPawn.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrameworkDefaultsTest,
	"DIGETHER.Game.Framework.Defaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrameworkDefaultsTest::RunTest(const FString& Parameters)
{
	const ADigetherGameplayGameMode* GameMode = GetDefault<ADigetherGameplayGameMode>();
	const ADigetherGameplayGameState* GameState = GetDefault<ADigetherGameplayGameState>();
	const ADigetherGameplayPlayerState* PlayerState = GetDefault<ADigetherGameplayPlayerState>();
	const ADigetherGameplayPlayerController* Controller = GetDefault<ADigetherGameplayPlayerController>();
	const ADigetherTestPawn* Pawn = GetDefault<ADigetherTestPawn>();
	if (!TestNotNull(TEXT("GameMode 기본 객체"), GameMode)
		|| !TestNotNull(TEXT("GameState 기본 객체"), GameState)
		|| !TestNotNull(TEXT("PlayerState 기본 객체"), PlayerState)
		|| !TestNotNull(TEXT("PlayerController 기본 객체"), Controller)
		|| !TestNotNull(TEXT("시험 Pawn 기본 객체"), Pawn))
	{
		return false;
	}

	TestTrue(TEXT("새 GameState 클래스 연결"), GameMode->GameStateClass.Get() == ADigetherGameplayGameState::StaticClass());
	TestTrue(TEXT("새 PlayerState 클래스 연결"), GameMode->PlayerStateClass.Get() == ADigetherGameplayPlayerState::StaticClass());
	TestTrue(TEXT("새 PlayerController 클래스 연결"),
		GameMode->PlayerControllerClass.Get() == ADigetherGameplayPlayerController::StaticClass());
	TestTrue(TEXT("임시 시험 Pawn 클래스 연결"), GameMode->DefaultPawnClass.Get() == ADigetherTestPawn::StaticClass());
	TestNull(TEXT("C++에서 튜닝 프로필을 자동 지정하지 않음"), GameMode->TuningProfile.Get());
	TestFalse(TEXT("초기화 전에는 런 진행 거절"), GameMode->IsRunReady());
	TestNull(TEXT("초기화 전에는 런 스냅샷을 제공하지 않음"), GameMode->GetRunTuning());
	TestTrue(TEXT("개발 명령 CheatManager 연결"), Controller->CheatClass.Get() == UDigetherCheatManager::StaticClass());

	const UDigetherQuotaStateComponent* Quota = GameState->GetQuotaComponent();
	const UDigetherWalletComponent* Wallet = PlayerState->GetWalletComponent();
	if (!TestNotNull(TEXT("GameState 기본 공개 상태 컴포넌트"), Quota)
		|| !TestNotNull(TEXT("PlayerState 기본 지갑 컴포넌트"), Wallet))
	{
		return false;
	}

	TestTrue(TEXT("공개 상태는 GameState의 기본 하위객체"),
		Quota->HasAnyFlags(RF_DefaultSubObject) && Quota->GetOuter() == GameState);
	TestTrue(TEXT("지갑은 PlayerState의 기본 하위객체"),
		Wallet->HasAnyFlags(RF_DefaultSubObject) && Wallet->GetOuter() == PlayerState);
	TestTrue(TEXT("GameState Actor 복제 활성"), GameState->GetIsReplicated());
	TestTrue(TEXT("PlayerState Actor 복제 활성"), PlayerState->GetIsReplicated());
	TestTrue(TEXT("공개 상태 컴포넌트 복제 활성"), Quota->GetIsReplicated());
	TestTrue(TEXT("지갑 컴포넌트 복제 활성"), Wallet->GetIsReplicated());

	const USphereComponent* Collision = Pawn->GetCollisionComponent();
	const UPawnMovementComponent* Movement = Pawn->GetMovementComponent();
	if (!TestNotNull(TEXT("시험 Pawn의 기본 구체 충돌"), Collision)
		|| !TestNotNull(TEXT("시험 Pawn의 기본 이동 컴포넌트"), Movement))
	{
		return false;
	}
	TestTrue(TEXT("구체 충돌이 Pawn의 루트"), Pawn->GetRootComponent() == Collision);
	TestTrue(TEXT("시험 Pawn 충돌 활성"), Collision->GetCollisionEnabled() != ECollisionEnabled::NoCollision);
	TestTrue(TEXT("엔진 기본 부유 이동 사용"), Movement->IsA<UFloatingPawnMovement>());
	TestTrue(TEXT("엔진 기본 이동과 시점 입력 사용"), Pawn->bAddDefaultMovementBindings);
	return true;
}

#endif
