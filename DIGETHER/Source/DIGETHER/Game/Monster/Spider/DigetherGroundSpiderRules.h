#pragma once

#include "CoreMinimal.h"
#include "Game/Monster/Spider/DigetherGroundSpiderTypes.h"

// 땅거미 판정 순수 규칙, 월드·Actor 비의존(자동 테스트 대상)
namespace DigetherGroundSpiderRules
{
	struct FTargetCandidate
	{
		FString PlayerKey;
		float DistanceSquared = 0.f;
		bool bReachable = false;
	};

	// 도달 가능한 가장 가까운 사람(기획), 없으면 INDEX_NONE
	DIGETHER_API int32 SelectNearestReachable(TConstArrayView<FTargetCandidate> Candidates);

	struct FTrackingInput
	{
		// 살아 있고 비관전인 대상인지
		bool bTargetValid = false;
		bool bReachable = false;
		// 대상과 둥지 중심 거리
		float TargetDistanceFromHome = 0.f;
		float TrackRadius = 0.f;
		// 영역 밖에서 공격받아 영역 밖까지 쫓는 중
		bool bChaseOutsideArea = false;
		float SecondsSinceSeen = 0.f;
		float ReacquireSeconds = 0.f;
	};

	// 시간 제한 없음, 아래 조건에서만 포기(기획)
	DIGETHER_API EDigetherSpiderGiveUpReason EvaluateGiveUp(const FTrackingInput& Input);

	// 도약 목표 지점, 최대 거리로 제한
	DIGETHER_API FVector ComputeLeapDestination(const FVector& Start, const FVector& Target, float MaxDistance);

	// 도약 비행 시간
	DIGETHER_API float ComputeLeapFlightSeconds(const FVector& Start, const FVector& Destination, float Speed);

	struct FContactInput
	{
		double Now = 0.0;
		// 이 대상에게 마지막 접촉 피해 시각, 없으면 음수
		double LastContactDamageTime = -1.0;
		float IntervalSeconds = 1.f;
		// 같은 서버 프레임에 도약 피해를 받았는지
		bool bLeapDamagedThisFrame = false;
		bool bTargetBuried = false;
		bool bSpiderBuried = false;
	};

	DIGETHER_API bool ShouldApplyContactDamage(const FContactInput& Input);
}

namespace DigetherGroundSpiderRules
{
	// 추적 후보 자격: 영역 안이거나, 영역 밖에서 땅거미를 공격한 사람(영역 경계 추적·포기 반복 방지)
	DIGETHER_API bool IsTrackableCandidate(float DistanceFromHome, float TrackRadius, bool bAttackedFromOutside);
}
