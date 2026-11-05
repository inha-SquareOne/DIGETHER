#include "Game/Dev/DigetherCheatManager.h"

#include "Game/Dev/DigetherDevCommands.h"
#include "Game/Dev/DigetherDevLog.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "UObject/UObjectGlobals.h"

#if !UE_BUILD_SHIPPING
namespace
{
	using FSaveRequest = void (ADigetherGameplayPlayerController::*)();

	void RegisterSaveCommand(const FName Name, const FSaveRequest Request)
	{
		// 경로는 명령을 실행한 로컬 Controller 조회에만 사용한다. 인증·재접속 ID와 무관하다.
		// 처리기는 UObject를 캡처하지 않아 월드 종료 후에도 이전 객체를 붙잡지 않는다.
		DigetherDev::Register(Name, [Request](const TArray<FString>& Args)
		{
			ADigetherGameplayPlayerController* Controller = Args.Num() == 1
				? FindObject<ADigetherGameplayPlayerController>(nullptr, *Args[0]) : nullptr;
			if (!IsValid(Controller) || !Controller->IsLocalController())
			{
				return FString(TEXT("저장 명령의 로컬 Controller가 없습니다."));
			}
			(Controller->*Request)();
			return FString(TEXT("서버에 진행 명령을 요청했습니다."));
		});
	}
}
#endif

void UDigetherCheatManager::InitCheatManager()
{
	Super::InitCheatManager();
#if !UE_BUILD_SHIPPING
	// 클라이언트에는 GameMode가 없으므로 로컬 CheatManager가 자기 프로세스의 처리기를 등록한다.
	RegisterSaveCommand(TEXT("DigetherSave"), &ADigetherGameplayPlayerController::RequestManualSave);
	RegisterSaveCommand(TEXT("DigetherLoad"), &ADigetherGameplayPlayerController::RequestLoadGame);
	RegisterSaveCommand(TEXT("DigetherNewGame"), &ADigetherGameplayPlayerController::RequestNewGame);
#endif
}

void UDigetherCheatManager::DigetherCheatList()
{
	for (const DigetherDev::FCommandInfo& Info : DigetherDev::GetPlannedCommands())
	{
		UE_LOG(LogDigetherDev, Display, TEXT("%s [%s] %s - %s"), *Info.Name.ToString(), Info.OwnerPackage, Info.Summary,
			DigetherDev::IsConnected(Info.Name) ? TEXT("연결됨") : TEXT("아직 연결 안 됨"));
	}
}

void UDigetherCheatManager::DigetherAdvanceTime(const FString& Seconds)
{
	DigetherDev::Execute(TEXT("DigetherAdvanceTime"), { Seconds });
}

void UDigetherCheatManager::DigetherSpawnItem(const FString& ItemId, const FString& Count)
{
	DigetherDev::Execute(TEXT("DigetherSpawnItem"), { ItemId, Count });
}

void UDigetherCheatManager::DigetherAftershock()
{
	DigetherDev::Execute(TEXT("DigetherAftershock"), {});
}

void UDigetherCheatManager::DigetherRerunSettlement()
{
	DigetherDev::Execute(TEXT("DigetherRerunSettlement"), {});
}

void UDigetherCheatManager::DigetherSave()
{
	const APlayerController* Controller = GetOuterAPlayerController();
	DigetherDev::Execute(TEXT("DigetherSave"), IsValid(Controller)
		? TArray<FString>{ Controller->GetPathName() } : TArray<FString>{});
}

void UDigetherCheatManager::DigetherLoad()
{
	const APlayerController* Controller = GetOuterAPlayerController();
	DigetherDev::Execute(TEXT("DigetherLoad"), IsValid(Controller)
		? TArray<FString>{ Controller->GetPathName() } : TArray<FString>{});
}

void UDigetherCheatManager::DigetherNewGame()
{
	const APlayerController* Controller = GetOuterAPlayerController();
	DigetherDev::Execute(TEXT("DigetherNewGame"), IsValid(Controller)
		? TArray<FString>{ Controller->GetPathName() } : TArray<FString>{});
}
