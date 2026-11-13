#include "Game/Combat/DigetherMeleeRules.h"

FDigetherMeleeHitParams::FDigetherMeleeHitParams()
{
	TargetObjectTypes = { ECC_Pawn, ECC_PhysicsBody };
}

bool DigetherMeleeRules::IsUsable(const FDigetherMeleeHitParams& Params, TArray<FString>* OutProblems)
{
	bool bUsable = true;
	const auto RequirePositive = [&bUsable, OutProblems](const TCHAR* Name, float Value)
	{
		if (!(Value > 0.f) || !FMath::IsFinite(Value))
		{
			bUsable = false;
			if (OutProblems != nullptr)
			{
				OutProblems->Add(FString::Printf(TEXT("%s는 0보다 커야 한다(현재 %g)"), Name, Value));
			}
		}
	};
	RequirePositive(TEXT("Damage"), Params.Damage);
	RequirePositive(TEXT("RangeCm"), Params.RangeCm);
	RequirePositive(TEXT("HalfWidthCm"), Params.HalfWidthCm);
	RequirePositive(TEXT("HalfHeightCm"), Params.HalfHeightCm);
	if (Params.ForwardOffsetCm < 0.f || !FMath::IsFinite(Params.ForwardOffsetCm))
	{
		bUsable = false;
		if (OutProblems != nullptr)
		{
			OutProblems->Add(FString::Printf(TEXT("ForwardOffsetCm는 0 이상이어야 한다(현재 %g)"), Params.ForwardOffsetCm));
		}
	}
	if (Params.TargetObjectTypes.IsEmpty())
	{
		bUsable = false;
		if (OutProblems != nullptr)
		{
			OutProblems->Add(TEXT("TargetObjectTypes가 비어 있다"));
		}
	}
	return bUsable;
}

FDigetherMeleeHitBox DigetherMeleeRules::ComputeHitBox(const FVector& ViewLocation, const FRotator& ViewRotation, const FDigetherMeleeHitParams& Params)
{
	FDigetherMeleeHitBox Box;
	Box.Rotation = ViewRotation.Quaternion();
	const float HalfRange = Params.RangeCm * 0.5f;
	Box.Center = ViewLocation + Box.Rotation.GetForwardVector() * (Params.ForwardOffsetCm + HalfRange);
	Box.HalfExtent = FVector(HalfRange, Params.HalfWidthCm, Params.HalfHeightCm);
	return Box;
}

float DigetherMeleeRules::ComputeDamage(const FDigetherMeleeHitParams& Params, float DamageScale)
{
	if (!(DamageScale > 0.f) || !FMath::IsFinite(DamageScale))
	{
		return 0.f;
	}
	return FMath::Max(0.f, Params.Damage * DamageScale);
}
