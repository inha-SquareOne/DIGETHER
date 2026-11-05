#pragma once

#include "CoreMinimal.h"

class AActor;
class ADigetherElevatorLeverActor;
class APlayerController;

// GameMode 분할 구현 파일 공용 헬퍼, 정의는 DigetherGameplayRunFlow.cpp
// 익명 네임스페이스 대신 이름 있는 네임스페이스: 유니티·비유니티 빌드 모두 단일 정의
namespace DigetherGameplayRunFlowInternal
{
	void SetSpectateViewTarget(APlayerController* Controller, AActor* Target);

	const ADigetherElevatorLeverActor* FindOccupiedSiblingLever(
		const TArray<TWeakObjectPtr<ADigetherElevatorLeverActor>>& Levers, const ADigetherElevatorLeverActor* Lever);
}
