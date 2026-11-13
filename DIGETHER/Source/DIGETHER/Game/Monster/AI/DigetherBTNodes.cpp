#include "Game/Monster/AI/DigetherBTNodes.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "Game/Monster/DigetherMonsterLog.h"
#include "Game/Monster/Spider/DigetherGroundSpiderComponent.h"
#include "NavigationSystem.h"

namespace
{
	UDigetherGroundSpiderComponent* FindSpider(const UBehaviorTreeComponent& OwnerComp)
	{
		const AAIController* AI = OwnerComp.GetAIOwner();
		const APawn* Pawn = AI ? AI->GetPawn() : nullptr;
		return Pawn ? Pawn->FindComponentByClass<UDigetherGroundSpiderComponent>() : nullptr;
	}
}

UBTTask_DigetherSpiderLeap::UBTTask_DigetherSpiderLeap()
{
	NodeName = TEXT("Digether Spider Leap");
	INIT_TASK_NODE_NOTIFY_FLAGS();
}

EBTNodeResult::Type UBTTask_DigetherSpiderLeap::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UDigetherGroundSpiderComponent* Spider = FindSpider(OwnerComp);
	return Spider && Spider->StartLeap() ? EBTNodeResult::InProgress : EBTNodeResult::Failed;
}

void UBTTask_DigetherSpiderLeap::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	const UDigetherGroundSpiderComponent* Spider = FindSpider(OwnerComp);
	if (!Spider || !Spider->IsLeaping())
	{
		FinishLatentTask(OwnerComp, Spider ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
	}
}

EBTNodeResult::Type UBTTask_DigetherSpiderLeap::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (UDigetherGroundSpiderComponent* Spider = FindSpider(OwnerComp))
	{
		Spider->CancelLeap();
	}
	return EBTNodeResult::Aborted;
}

FString UBTTask_DigetherSpiderLeap::GetStaticDescription() const
{
	return TEXT("땅거미 도약(준비→비행→착지), 수치는 종 데이터");
}

UBTDecorator_DigetherSpiderCanLeap::UBTDecorator_DigetherSpiderCanLeap()
{
	NodeName = TEXT("Digether Spider Can Leap");
	INIT_DECORATOR_NODE_NOTIFY_FLAGS();
	FlowAbortMode = EBTFlowAbortMode::LowerPriority;
	bAllowAbortNone = false;
	bAllowAbortLowerPri = true;
	// 도약 시작으로 추적 조건이 꺼져도 진행 중인 도약은 취소하지 않음.
	bAllowAbortChildNodes = false;
}

void UBTDecorator_DigetherSpiderCanLeap::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	ConditionalFlowAbort(OwnerComp, EBTDecoratorAbortRequest::ConditionResultChanged);
}

bool UBTDecorator_DigetherSpiderCanLeap::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	const UDigetherGroundSpiderComponent* Spider = FindSpider(OwnerComp);
	return Spider && Spider->CanLeapNow();
}

FString UBTDecorator_DigetherSpiderCanLeap::GetStaticDescription() const
{
	return TEXT("추적 중, 도약 거리 안, 시야 막힘 없음, 조건 관찰로 낮은 우선순위 행동 선점");
}

UBTTask_DigetherPickWanderLocation::UBTTask_DigetherPickWanderLocation()
{
	NodeName = TEXT("Digether Pick Wander Location");
	BlackboardKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_DigetherPickWanderLocation, BlackboardKey));
	OriginKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_DigetherPickWanderLocation, OriginKey));
}

void UBTTask_DigetherPickWanderLocation::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);
	if (const UBlackboardData* BlackboardAsset = GetBlackboardAsset())
	{
		OriginKey.ResolveSelectedKey(*BlackboardAsset);
	}
}

EBTNodeResult::Type UBTTask_DigetherPickWanderLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	const AAIController* AI = OwnerComp.GetAIOwner();
	const APawn* Pawn = AI ? AI->GetPawn() : nullptr;
	if (!Blackboard || !Pawn)
	{
		return EBTNodeResult::Failed;
	}
	FVector Origin = Pawn->GetActorLocation();
	if (OriginKey.SelectedKeyType == UBlackboardKeyType_Vector::StaticClass())
	{
		Origin = Blackboard->GetValueAsVector(OriginKey.SelectedKeyName);
	}
	float Radius = RadiusOverride;
	if (Radius <= 0.f)
	{
		const UDigetherGroundSpiderComponent* Spider = Pawn->FindComponentByClass<UDigetherGroundSpiderComponent>();
		if (!Spider)
		{
			// 조용히 기본값을 쓰지 않음(R06): 종 데이터가 없으면 노드 실패
			UE_LOG(LogDigetherMonster, Warning, TEXT("%s: 배회 반지름 종 데이터 없음, RadiusOverride를 지정하세요."), *GetNameSafe(Pawn));
			return EBTNodeResult::Failed;
		}
		Radius = Spider->GetTrackRadius() * AreaFraction;
	}
	const UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());
	FNavLocation Result;
	if (!Nav || !Nav->GetRandomReachablePointInRadius(Origin, Radius, Result))
	{
		return EBTNodeResult::Failed;
	}
	Blackboard->SetValueAsVector(BlackboardKey.SelectedKeyName, Result.Location);
	return EBTNodeResult::Succeeded;
}

FString UBTTask_DigetherPickWanderLocation::GetStaticDescription() const
{
	return RadiusOverride > 0.f
		? FString::Printf(TEXT("%s 주변 %.0fcm [덮어씀] → %s"), *OriginKey.SelectedKeyName.ToString(), RadiusOverride, *BlackboardKey.SelectedKeyName.ToString())
		: FString::Printf(TEXT("%s 주변 추적 영역×%.2f(종 데이터) → %s"), *OriginKey.SelectedKeyName.ToString(), AreaFraction, *BlackboardKey.SelectedKeyName.ToString());
}
