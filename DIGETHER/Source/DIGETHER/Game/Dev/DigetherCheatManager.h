#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "DigetherCheatManager.generated.h"

// 개발용 치트 명령 모음, 실제 동작은 각 패키지가 DigetherDev::Register로 연결
// 새 Game PlayerController를 만들면 그 CheatClass에 이 클래스 연결(프로토타입 Controller는 수정 안 함)
// Shipping에서는 엔진이 CheatManager를 만들지 않고 DigetherDev도 실행 거절
UCLASS()
class DIGETHER_API UDigetherCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	virtual void InitCheatManager() override;

	UFUNCTION(Exec)
	void DigetherCheatList();

	UFUNCTION(Exec)
	void DigetherAdvanceTime(const FString& Seconds);

	UFUNCTION(Exec)
	void DigetherSpawnItem(const FString& ItemId, const FString& Count);

	UFUNCTION(Exec)
	void DigetherAftershock();

	UFUNCTION(Exec)
	void DigetherRerunSettlement();

	UFUNCTION(Exec)
	void DigetherSave();

	UFUNCTION(Exec)
	void DigetherLoad();

	UFUNCTION(Exec)
	void DigetherNewGame();

	UFUNCTION(Exec)
	void DigetherMonsterSpawn(const FString& DefinitionName);

	UFUNCTION(Exec)
	void DigetherMonsterForceHabitat();

	UFUNCTION(Exec)
	void DigetherMonsterAdvanceSpawnTimer(const FString& Seconds);

	UFUNCTION(Exec)
	void DigetherMonsterEvaluate();
};
