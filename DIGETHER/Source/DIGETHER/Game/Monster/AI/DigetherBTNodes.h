#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "DigetherBTNodes.generated.h"

// 몬스터 공용·종별 BT 노드, 실행 상태는 노드 멤버가 아니라 종 컴포넌트·Blackboard에 둠(노드 인스턴스 공유)

// 땅거미 도약: 준비·비행·착지가 끝날 때까지 진행, 중단되면 도약 취소
UCLASS(meta=(DisplayName="Digether Spider Leap"))
class DIGETHER_API UBTTask_DigetherSpiderLeap : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_DigetherSpiderLeap();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual FString GetStaticDescription() const override;
};

// 땅거미 도약 가능 조건(추적 중·도약 거리 안·시야 막힘 없음)
UCLASS(meta=(DisplayName="Digether Spider Can Leap"))
class DIGETHER_API UBTDecorator_DigetherSpiderCanLeap : public UBTDecorator
{
	GENERATED_BODY()

public:
	UBTDecorator_DigetherSpiderCanLeap();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual FString GetStaticDescription() const override;
};

// 기준점(Blackboard 벡터) 주변 도달 가능한 임의 지점을 골라 선택한 키에 기록, 배회용
// 반지름 0이면 종 데이터(땅거미 추적 영역)의 비율 사용
UCLASS(meta=(DisplayName="Digether Pick Wander Location"))
class DIGETHER_API UBTTask_DigetherPickWanderLocation : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_DigetherPickWanderLocation();

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

	// 기준점 키(예: HomeLocation)
	UPROPERTY(EditAnywhere, Category="Digether")
	FBlackboardKeySelector OriginKey;

	// 시험용 덮어쓰기, 0이면 종 데이터 참조(R06)
	UPROPERTY(EditAnywhere, Category="Digether", meta=(ClampMin="0", Units="cm"))
	float RadiusOverride = 0.f;

	// 종 데이터 참조 시 추적 영역 대비 비율
	UPROPERTY(EditAnywhere, Category="Digether", meta=(ClampMin="0.05", ClampMax="1"))
	float AreaFraction = 0.4f;
};
