#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidgetPool.h"
#include "CommonActivatableWidget.h"
#include "CommonInputSettings.h"
#include "ICommonInputModule.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/Online/DigetherSessionSubsystem.h"
#include "Misc/AutomationTest.h"

// CommonUI 스택은 닫힌 패널 UObject를 풀에 남겨 재사용(Slate만 해제)
// 같은 인스턴스 재오픈 시 세션 알림 수신 확인
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherFrontendWidgetReuseTest,
	"DIGETHER.Game.UI.WidgetReuse", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherFrontendWidgetReuseTest::RunTest(const FString& Parameters)
{
	static const TCHAR* WidgetPaths[] = {
		TEXT("/Game/Frontend/UI/MainMenu/WBP_MainMenuScreen.WBP_MainMenuScreen_C"),
		TEXT("/Game/Frontend/UI/MainMenu/WBP_CreateRoomPanel.WBP_CreateRoomPanel_C"),
		TEXT("/Game/Frontend/UI/MainMenu/WBP_JoinRoomPanel.WBP_JoinRoomPanel_C"),
		TEXT("/Game/Frontend/UI/Lobby/WBP_LobbyScreen.WBP_LobbyScreen_C"),
	};

	// 게임 실행 땐 CommonUI가 입력 데이터 선로드, 명령줄 시험은 여기서 로드
	ICommonInputModule::GetSettings().LoadData();

	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->AddToRoot();
	GameInstance->InitializeStandalone();
	UWorld* World = GameInstance->GetWorld();
	UDigetherSessionSubsystem* Session = GameInstance->GetSubsystem<UDigetherSessionSubsystem>();

	if (TestNotNull(TEXT("시험용 세션 서브시스템"), Session))
	{
		for (const TCHAR* Path : WidgetPaths)
		{
			const TSubclassOf<UCommonActivatableWidget> WidgetClass = LoadClass<UCommonActivatableWidget>(nullptr, Path);
			if (!TestNotNull(FString::Printf(TEXT("위젯 클래스 %s"), Path), WidgetClass.Get()))
			{
				continue;
			}

			FUserWidgetPool Pool;
			Pool.SetWorld(World);
			UCommonActivatableWidget* First = Pool.GetOrCreateInstance(WidgetClass);
			if (!TestNotNull(FString::Printf(TEXT("%s 생성"), Path), First))
			{
				continue;
			}
			TestTrue(FString::Printf(TEXT("%s 처음 열면 구독"), Path), Session->OnStatusChanged.IsBoundToObject(First));

			Pool.Release(First, true);
			TestFalse(FString::Printf(TEXT("%s 닫으면 해제"), Path), Session->OnStatusChanged.IsBoundToObject(First));

			UCommonActivatableWidget* Reopened = Pool.GetOrCreateInstance(WidgetClass);
			TestEqual(FString::Printf(TEXT("%s 같은 인스턴스 재사용"), Path), Reopened, First);
			TestTrue(FString::Printf(TEXT("%s 다시 열면 구독"), Path), Session->OnStatusChanged.IsBoundToObject(Reopened));

			Pool.ResetPool();
		}
	}

	GameInstance->Shutdown();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	GameInstance->RemoveFromRoot();
	return true;
}

#endif
