#include "Game/Dev/DigetherCheatManager.h"

#include "Game/Dev/DigetherDevCommands.h"
#include "Game/Dev/DigetherDevLog.h"

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
