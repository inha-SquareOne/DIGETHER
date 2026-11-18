#include "DigetherCollapseLab.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"

void ADigetherCollapseLab::SpawnFallingClods(const TArray<FVector>& CeilingVertices)
{
	const int32 Count = 72 + 64 * FallingStage;
	Clods.Reset(Count);
	const FVector Extent = Deposit->GetScaledBoxExtent();
	const FQuat Basis = Deposit->GetComponentQuat();
	const FVector Base = Deposit->GetComponentLocation() - Basis.GetAxisZ() * Extent.Z;
	const FVector Lip = CeilingRemoval->GetComponentLocation() - FVector::UpVector * CeilingRemoval->GetScaledBoxExtent().Z;
	const float Height = FallingStage == 1 ? .23f : FallingStage == 2 ? .57f : 1.f;
	FRandomStream Random(HashCombine(GetTypeHash(GetActorLocation()), uint32(CollapseCount + 1)));
	constexpr int32 StreamCount = 3;
	FVector Sources[StreamCount];
	for (int32 Stream = 0; Stream < StreamCount; ++Stream)
	{
		const FVector Aim = Lip + Basis.RotateVector(FVector(Extent.X * (.28 * Stream - .26),
			Extent.Y * Random.FRandRange(-.31f, .31f), 0));
		Sources[Stream] = Aim;
		double Best = MAX_dbl;
		for (int32 T = 0; T + 2 < CeilingVertices.Num(); T += 3)
		{
			const FVector Point = (CeilingVertices[T] + CeilingVertices[T + 1] + CeilingVertices[T + 2]) / 3.;
			const double Dist = FVector::DistSquared2D(Aim, Point);
			if (Dist < Best)
			{
				Best = Dist;
				Sources[Stream] = Point;
			}
		}
	}
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FFallingClod& Clod = Clods.AddDefaulted_GetRef();
		const bool bGrain = Index % 11 != 3;
		const float Spread = bGrain ? 5.f : 15.f;
		Clod.Source = Sources[Index % StreamCount] + FVector(Random.FRandRange(-Spread, Spread),
			Random.FRandRange(-Spread, Spread), -Random.FRandRange(2.f, 7.f));
		FVector Offset = Basis.UnrotateVector(Clod.Source - Base);
		Offset.X += Random.FRandRange(-.24f, .24f) * Extent.X;
		Offset.Y += Random.FRandRange(-.24f, .24f) * Extent.Y;
		Offset.Z = 0.;
		const double Radius = FMath::Sqrt(FMath::Square(Offset.X / Extent.X) + FMath::Square(Offset.Y / Extent.Y));
		if (Radius > .92) Offset *= .92 / Radius;
		const float SizeCm = bGrain ? Random.FRandRange(.7f, 2.4f) : Random.FRandRange(3.f, 5.f + 2.f * FallingStage);
		Clod.Scale = FVector(Random.FRandRange(.8f, 1.5f), Random.FRandRange(.5f, 1.f), Random.FRandRange(.45f, .95f)) * (SizeCm / 35.);
		Clod.Release = Index == 0 ? 0.f : Random.FRandRange(bGrain ? 0.f : .16f, .55f);
		Clod.Duration = Random.FRandRange(.37f, .45f);
		Clod.Rotation = Random.GetUnitVector().ToOrientationQuat();
		Clod.Spin = Random.GetUnitVector() * Random.FRandRange(1.f, 4.2f);
		const float SettledHeight = Height * 1.94f * Extent.Z * FMath::Sqrt(1. - FMath::Square(FMath::Min(.92, Radius)));
		Clod.Landing = Base + Basis.RotateVector(Offset + FVector::UpVector * FMath::Lerp(1.f, SettledHeight, Clod.Release * 1.6f));
		FallingSoil->AddInstance(FTransform(Clod.Rotation, Clod.Source, FVector::ZeroVector), true);
	}
}

void ADigetherCollapseLab::AnimateFallingClods(float Alpha)
{
	for (int32 Index = 0; Index < Clods.Num(); ++Index)
	{
		const FFallingClod& Clod = Clods[Index];
		const float Travel = FMath::Clamp((Alpha - Clod.Release) / Clod.Duration, 0.f, 1.f);
		FVector Position = FMath::Lerp(Clod.Source, Clod.Landing, Travel);
		Position.Z = FMath::Lerp(Clod.Source.Z, Clod.Landing.Z, Travel * Travel);
		const FQuat Rotation = FQuat(Clod.Spin.GetSafeNormal(), Clod.Spin.Size() * Travel) * Clod.Rotation;
		const FVector Scale = Clod.Scale * FMath::Min(1.f, 20.f * Travel);
		FallingSoil->UpdateInstanceTransform(Index, FTransform(Rotation, Position, Scale), true, Index + 1 == Clods.Num(), true);
	}
}
