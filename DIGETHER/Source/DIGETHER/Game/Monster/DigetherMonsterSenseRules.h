#pragma once

#include "CoreMinimal.h"
#include "Game/Monster/DigetherMonsterTypes.h"

// 감각 판정 순수 기하 규칙, 가림(트레이스)은 호출자 책임
namespace DigetherMonsterSenseRules
{
	// 몬스터 시야 원뿔, HalfAngle은 전체 시야각의 절반(도)
	DIGETHER_API bool IsInViewCone(const FVector& Origin, const FVector& Forward, const FVector& Target,
		float HalfAngleDegrees, float Range);

	// 카메라 표본 기준 화면 안 여부(원근 투영), 카메라 뒤·거리 밖은 거짓
	DIGETHER_API bool IsPointOnScreen(const FDigetherCameraSample& Sample, const FVector& Point, float MaxDistance);

	// 화면 중심 방향과의 각도(도), 카메라 위치와 같으면 180
	DIGETHER_API float AngleFromViewCenter(const FDigetherCameraSample& Sample, const FVector& Point);

	// 클라이언트 실제 투영의 유효 수평·수직 시야각, bMaintainXFov는 엔진 축 유지 설정이 X일 때
	DIGETHER_API void ComputeEffectiveFov(float FovDegrees, float AspectRatio, bool bMaintainXFov,
		float& OutHorizontal, float& OutVertical);

	// 서버 수신 검증, 비유한 값·범위 밖 시야각은 기본값으로 대체, 대체했으면 거짓
	DIGETHER_API bool SanitizeSample(FDigetherCameraSample& Sample, float MinFov, float MaxFov, float MaxAspectRatio,
		float DefaultFov, float DefaultAspectRatio);

	// '보는 중' 판정 유예, 순간 끊김이 Grace 안이면 계속 보는 중
	struct FGazeDwell
	{
		// 유예 포함 연속으로 보는 시간
		float DwellSeconds = 0.f;
		// 마지막으로 실제로 본 뒤 지난 시간
		float SinceRawLookSeconds = TNumericLimits<float>::Max();
	};

	// 반환: 유예 적용 후 보는 중인지, 연속 시간은 State.DwellSeconds
	DIGETHER_API bool UpdateGazeDwell(FGazeDwell& State, float DeltaSeconds, bool bRawLooking, float GraceSeconds);
}
