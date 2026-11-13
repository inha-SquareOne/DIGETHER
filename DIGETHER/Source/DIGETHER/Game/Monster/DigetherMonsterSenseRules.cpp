#include "Game/Monster/DigetherMonsterSenseRules.h"

namespace DigetherMonsterSenseRules
{
	namespace
	{
		bool IsFiniteVector(const FVector& V)
		{
			return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z);
		}
	}

	bool IsInViewCone(const FVector& Origin, const FVector& Forward, const FVector& Target,
		const float HalfAngleDegrees, const float Range)
	{
		const FVector ToTarget = Target - Origin;
		const float DistanceSquared = ToTarget.SizeSquared();
		if (Range <= 0.f || DistanceSquared > FMath::Square(Range))
		{
			return false;
		}
		if (DistanceSquared <= KINDA_SMALL_NUMBER)
		{
			return true;
		}
		const FVector Facing = Forward.GetSafeNormal();
		if (Facing.IsNearlyZero())
		{
			return false;
		}
		const float CosHalf = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(HalfAngleDegrees, 0.f, 180.f)));
		return FVector::DotProduct(Facing, ToTarget.GetUnsafeNormal()) >= CosHalf;
	}

	bool IsPointOnScreen(const FDigetherCameraSample& Sample, const FVector& Point, const float MaxDistance)
	{
		const FVector ToPoint = Point - FVector(Sample.Location);
		if (MaxDistance <= 0.f || ToPoint.SizeSquared() > FMath::Square(MaxDistance))
		{
			return false;
		}
		const FVector Local = Sample.Rotation.UnrotateVector(ToPoint);
		if (Local.X <= KINDA_SMALL_NUMBER)
		{
			return false;
		}
		const float TanHalfH = FMath::Tan(FMath::DegreesToRadians(Sample.HorizontalFov * 0.5f));
		const float TanHalfV = FMath::Tan(FMath::DegreesToRadians(Sample.VerticalFov * 0.5f));
		return FMath::Abs(Local.Y / Local.X) <= TanHalfH && FMath::Abs(Local.Z / Local.X) <= TanHalfV;
	}

	float AngleFromViewCenter(const FDigetherCameraSample& Sample, const FVector& Point)
	{
		const FVector ToPoint = Point - FVector(Sample.Location);
		if (ToPoint.IsNearlyZero())
		{
			return 180.f;
		}
		const float Dot = FVector::DotProduct(Sample.Rotation.Vector(), ToPoint.GetUnsafeNormal());
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.f, 1.f)));
	}

	void ComputeEffectiveFov(const float FovDegrees, const float AspectRatio, const bool bMaintainXFov,
		float& OutHorizontal, float& OutVertical)
	{
		const float Aspect = FMath::Max(AspectRatio, KINDA_SMALL_NUMBER);
		const float HalfFov = FMath::DegreesToRadians(FMath::Clamp(FovDegrees, 1.f, 170.f) * 0.5f);
		if (bMaintainXFov)
		{
			OutHorizontal = FMath::RadiansToDegrees(HalfFov * 2.f);
			OutVertical = FMath::RadiansToDegrees(2.f * FMath::Atan(FMath::Tan(HalfFov) / Aspect));
		}
		else
		{
			OutVertical = FMath::RadiansToDegrees(HalfFov * 2.f);
			OutHorizontal = FMath::RadiansToDegrees(2.f * FMath::Atan(FMath::Tan(HalfFov) * Aspect));
		}
	}

	bool SanitizeSample(FDigetherCameraSample& Sample, const float MinFov, const float MaxFov, const float MaxAspectRatio,
		const float DefaultFov, const float DefaultAspectRatio)
	{
		bool bClean = true;
		if (!IsFiniteVector(Sample.Location))
		{
			Sample.Location = FVector::ZeroVector;
			bClean = false;
		}
		if (!FMath::IsFinite(Sample.Rotation.Pitch) || !FMath::IsFinite(Sample.Rotation.Yaw) || !FMath::IsFinite(Sample.Rotation.Roll))
		{
			Sample.Rotation = FRotator::ZeroRotator;
			bClean = false;
		}
		// 가장 넓은 허용 투영: 최대 FOV를 수평으로 유지하거나, 최대 화면비에서 수직 유지
		float MaxH = 0.f;
		float MaxV = 0.f;
		ComputeEffectiveFov(MaxFov, FMath::Max(MaxAspectRatio, 1.f), false, MaxH, MaxV);
		MaxH = FMath::Max(MaxH, MaxFov);
		MaxV = FMath::Max(MaxV, MaxFov);
		const bool bFovValid = FMath::IsFinite(Sample.HorizontalFov) && FMath::IsFinite(Sample.VerticalFov)
			&& Sample.HorizontalFov > 0.f && Sample.VerticalFov > 0.f
			&& Sample.HorizontalFov <= MaxH + KINDA_SMALL_NUMBER && Sample.VerticalFov <= MaxV + KINDA_SMALL_NUMBER
			&& FMath::Max(Sample.HorizontalFov, Sample.VerticalFov) >= MinFov * 0.5f;
		if (!bFovValid)
		{
			ComputeEffectiveFov(DefaultFov, DefaultAspectRatio, true, Sample.HorizontalFov, Sample.VerticalFov);
			bClean = false;
		}
		return bClean;
	}

	bool UpdateGazeDwell(FGazeDwell& State, const float DeltaSeconds, const bool bRawLooking, const float GraceSeconds)
	{
		const float Delta = FMath::Max(0.f, DeltaSeconds);
		if (bRawLooking)
		{
			State.SinceRawLookSeconds = 0.f;
			State.DwellSeconds += Delta;
			return true;
		}
		if (State.SinceRawLookSeconds < TNumericLimits<float>::Max())
		{
			State.SinceRawLookSeconds += Delta;
		}
		if (State.SinceRawLookSeconds <= GraceSeconds)
		{
			State.DwellSeconds += Delta;
			return true;
		}
		State.DwellSeconds = 0.f;
		return false;
	}
}
