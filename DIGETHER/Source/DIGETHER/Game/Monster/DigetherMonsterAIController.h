#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "DigetherMonsterAIController.generated.h"

// 서버에만 존재, 빙의한 몬스터 정의의 BT 실행(BT 에셋은 기획자 편집, R05)
UCLASS()
class DIGETHER_API ADigetherMonsterAIController : public AAIController
{
	GENERATED_BODY()

public:
	ADigetherMonsterAIController();

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
};
