#include "Game/Dev/DigetherCheatManager.h"

#include "Game/Dev/DigetherDevCommands.h"
#include "Game/Dev/DigetherDevLog.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Monster/DigetherMonsterDevCommands.h"
#include "UObject/UObjectGlobals.h"

#if !UE_BUILD_SHIPPING
namespace
{
	using FSaveRequest = void (ADigetherGameplayPlayerController::*)();

	void RegisterSaveCommand(const FName Name, const FSaveRequest Request)
	{
		// 경로는 명령 실행 로컬 Controller 조회 전용, 인증·재접속 ID와 무관
		// UObject 미캡처, 월드 종료 후 이전 객체 참조 방지
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

namespace
{
	// 저장 명령 처리기가 받는 인자, Controller 경로 하나 또는 빈 배열
	TArray<FString> MakeControllerArgs(const APlayerController* Controller)
	{
		return IsValid(Controller) ? TArray<FString>{ Controller->GetPathName() } : TArray<FString>{};
	}
}

void UDigetherCheatManager::InitCheatManager()
{
	Super::InitCheatManager();
#if !UE_BUILD_SHIPPING
	// 클라이언트에는 GameMode가 없어 로컬 CheatManager가 자기 프로세스 처리기 등록
	RegisterSaveCommand(TEXT("DigetherSave"), &ADigetherGameplayPlayerController::RequestManualSave);
	RegisterSaveCommand(TEXT("DigetherLoad"), &ADigetherGameplayPlayerController::RequestLoadGame);
	RegisterSaveCommand(TEXT("DigetherNewGame"), &ADigetherGameplayPlayerController::RequestNewGame);
	DigetherMonsterDev::RegisterCommands();
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
	DigetherDev::Execute(TEXT("DigetherSave"), MakeControllerArgs(GetOuterAPlayerController()));
}

void UDigetherCheatManager::DigetherLoad()
{
	DigetherDev::Execute(TEXT("DigetherLoad"), MakeControllerArgs(GetOuterAPlayerController()));
}

void UDigetherCheatManager::DigetherNewGame()
{
	DigetherDev::Execute(TEXT("DigetherNewGame"), MakeControllerArgs(GetOuterAPlayerController()));
}

void UDigetherCheatManager::DigetherMonsterSpawn(const FString& DefinitionName)
{
	TArray<FString> Args = MakeControllerArgs(GetOuterAPlayerController());
	Args.Add(DefinitionName);
	DigetherDev::Execute(TEXT("DigetherMonsterSpawn"), Args);
}

void UDigetherCheatManager::DigetherMonsterForceHabitat()
{
	DigetherDev::Execute(TEXT("DigetherMonsterForceHabitat"), MakeControllerArgs(GetOuterAPlayerController()));
}

void UDigetherCheatManager::DigetherMonsterAdvanceSpawnTimer(const FString& Seconds)
{
	TArray<FString> Args = MakeControllerArgs(GetOuterAPlayerController());
	Args.Add(Seconds);
	DigetherDev::Execute(TEXT("DigetherMonsterAdvanceSpawnTimer"), Args);
}

void UDigetherCheatManager::DigetherMonsterEvaluate()
{
	DigetherDev::Execute(TEXT("DigetherMonsterEvaluate"), MakeControllerArgs(GetOuterAPlayerController()));
}
