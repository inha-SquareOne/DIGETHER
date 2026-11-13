#include "Game/Monster/DigetherMonsterAIController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BrainComponent.h"
#include "Game/Monster/DigetherMonsterCharacter.h"
#include "Game/Monster/DigetherMonsterDefinition.h"
#include "Game/Monster/DigetherMonsterLog.h"

ADigetherMonsterAIController::ADigetherMonsterAIController()
{
	bWantsPlayerState = false;
}

void ADigetherMonsterAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	const ADigetherMonsterCharacter* Monster = Cast<ADigetherMonsterCharacter>(InPawn);
	const UDigetherMonsterDefinition* Definition = Monster ? Monster->GetDefinition() : nullptr;
	if (!Definition)
	{
		UE_LOG(LogDigetherMonster, Warning, TEXT("%s: 몬스터 정의가 없어 BT를 시작하지 않습니다."), *GetNameSafe(InPawn));
		return;
	}
	// BT는 작은 에셋, 생성 시점 동기 로드(디렉터는 생성 전 정의만 검사)
	UBehaviorTree* Tree = Definition->BehaviorTree.LoadSynchronous();
	if (!Tree)
	{
		UE_LOG(LogDigetherMonster, Warning, TEXT("%s: 정의 %s에 BT가 없어 행동하지 않습니다."), *GetNameSafe(InPawn), *GetNameSafe(Definition));
		return;
	}
	if (!RunBehaviorTree(Tree))
	{
		UE_LOG(LogDigetherMonster, Warning, TEXT("%s: BT %s 시작 실패."), *GetNameSafe(InPawn), *GetNameSafe(Tree));
	}
}

void ADigetherMonsterAIController::OnUnPossess()
{
	if (UBrainComponent* Brain = GetBrainComponent())
	{
		Brain->StopLogic(TEXT("UnPossess"));
	}
	Super::OnUnPossess();
}
