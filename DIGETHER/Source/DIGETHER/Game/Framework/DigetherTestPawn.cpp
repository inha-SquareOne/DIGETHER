#include "Game/Framework/DigetherTestPawn.h"

#include "Components/SphereComponent.h"

ADigetherTestPawn::ADigetherTestPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 탑승 트리거의 겹침 목록에 엔진 기본 시험 Pawn도 참여하게 한다.
	GetCollisionComponent()->SetGenerateOverlapEvents(true);
}
