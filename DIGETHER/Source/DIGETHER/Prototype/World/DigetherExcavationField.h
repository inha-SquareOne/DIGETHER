#pragma once

#include "CoreMinimal.h"

// A bounded prototype volume. Density bytes encode signed distance in 2 cm units,
// with 128 as the zero surface and positive values representing soil.
namespace DigetherExcavation
{
	constexpr int32 CellsXY = 16;
	constexpr int32 CellsZ = 6;
	constexpr int32 SamplesXY = CellsXY + 1;
	constexpr int32 SamplesZ = CellsZ + 1;
	constexpr int32 SampleCount = SamplesXY * SamplesXY * SamplesZ;
	constexpr float Spacing = 50.f;
	constexpr float Radius = 125.f;
	constexpr float Bottom = -250.f;

	struct FSurface
	{
		TArray<FVector> Vertices;
		TArray<int32> Indices;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
	};

	int32 Index(int32 X, int32 Y, int32 Z);
	FVector Position(int32 X, int32 Y, int32 Z);
	TArray<uint8> InitialDensity();
	bool Carve(TArray<uint8>& Density, const FVector& LocalPoint);
	// Returns false for invalid inputs or when no unprotected samples change.
	bool Restore(TArray<uint8>& Density, const FTransform& FieldTransform, const FVector& WorldPoint,
		float WorldRadius, const TArray<FBox>& ProtectedWorldBoxes);
	bool BuildSurface(const TArray<uint8>& Density, FSurface& OutSurface);
	uint32 Hash(const TArray<uint8>& Density);
	bool IsSolid(const TArray<uint8>& Density, const FVector& LocalPoint);
}
