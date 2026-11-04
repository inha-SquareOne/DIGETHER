#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "DigetherCheatManager.generated.h"

// 개발용 치트 명령 모음. 실제 동작은 DigetherDev::Register로 각 패키지가 붙인다.
// 새 Game PlayerController가 생기면 그 클래스의 CheatClass에 이 클래스를 연결한다(프로토타입 Controller는 수정하지 않는다).
// Shipping에서는 엔진이 CheatManager를 만들지 않고, 틀도 실행을 거절한다.
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
};
