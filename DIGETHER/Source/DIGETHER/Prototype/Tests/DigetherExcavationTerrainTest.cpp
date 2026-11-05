#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Prototype/World/DigetherExcavationField.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherExcavationFieldTest,
	"DIGETHER.Prototype.Excavation.Field", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherExcavationFieldTest::RunTest(const FString& Parameters)
{
	using namespace DigetherExcavation;
	const TArray<uint8> Initial = InitialDensity();
	TestEqual(TEXT("bounded snapshot count"), Initial.Num(), SampleCount);
	TestTrue(TEXT("snapshot fits the default replication array limit"), SampleCount <= 2048);
	TestTrue(TEXT("initial underground is solid"), IsSolid(Initial, FVector(0, 0, -100)));
	TArray<uint8> Dug = Initial;
	TestTrue(TEXT("surface cut changes density"), Carve(Dug, FVector::ZeroVector));
	TestFalse(TEXT("sphere center is air"), IsSolid(Dug, FVector(0, 0, -50)));
	TestTrue(TEXT("away from cut stays solid"), IsSolid(Dug, FVector(300, 0, -100)));
	TestNotEqual(TEXT("cut changes hash"), Hash(Dug), Hash(Initial));
	for (int32 Z = 0; Z < SamplesZ; ++Z)
		for (int32 Y = 0; Y < SamplesXY; ++Y)
			for (int32 X = 0; X < SamplesXY; ++X)
				if (X == 0 || X == CellsXY || Y == 0 || Y == CellsXY || Z == 0)
					TestEqual(TEXT("protected boundary"), Dug[Index(X, Y, Z)], Initial[Index(X, Y, Z)]);
	TArray<uint8> Repeat = Initial;
	TestTrue(TEXT("same cut on another peer"), Carve(Repeat, FVector::ZeroVector));
	TestEqual(TEXT("deterministic field"), Hash(Dug), Hash(Repeat));
	TestFalse(TEXT("repeating same cut is idempotent"), Carve(Repeat, FVector::ZeroVector));
	TestTrue(TEXT("cut at pit floor deepens"), Carve(Repeat, FVector(0, 0, -100)));
	TestFalse(TEXT("deeper sample removed"), IsSolid(Repeat, FVector(0, 0, -150)));
	TestEqual(TEXT("reset restores original hash"), Hash(InitialDensity()), Hash(Initial));

	const FTransform Identity = FTransform::Identity;
	TArray<uint8> Untouched = Initial;
	const TArray<FBox> NoBoxes;
	TestFalse(TEXT("restore cannot add soil to untouched terrain"), Restore(Untouched, Identity, FVector::ZeroVector, 150.f, NoBoxes));
	TestEqual(TEXT("untouched restore preserves original"), Hash(Untouched), Hash(Initial));
	TArray<uint8> Restored = Dug;
	const TArray<FBox> Boxes = { FBox(FVector(-5, -5, -55), FVector(5, 5, -45)) };
	TestTrue(TEXT("restore away from protected box changes density"), Restore(Restored, Identity, FVector::ZeroVector, 150.f, Boxes));
	const float Margin = Spacing * FMath::Sqrt(3.f);
	const FBox Expanded = Boxes[0].ExpandBy(Margin);
	for (int32 Z = 0; Z < SamplesZ; ++Z)
		for (int32 Y = 0; Y < SamplesXY; ++Y)
			for (int32 X = 0; X < SamplesXY; ++X)
			{
				const int32 I = Index(X, Y, Z);
				if (Expanded.IsInsideOrOn(Position(X, Y, Z))) { TestEqual(TEXT("protected cell and margin unchanged"), Restored[I], Dug[I]); }
				if (Initial[I] <= 128) { TestTrue(TEXT("initial air never becomes soil"), Restored[I] <= 128); }
			}
	TArray<uint8> FullyRestored = Dug;
	TestTrue(TEXT("restore dug volume"), Restore(FullyRestored, Identity, FVector::ZeroVector, 150.f, NoBoxes));
	TestTrue(TEXT("restored soil can be dug again"), Carve(FullyRestored, FVector::ZeroVector));
	const FTransform Wall(FRotator(90.f, 0.f, 0.f), FVector(3400.f, 100.f, 25.f));
	const FVector WallHit = Wall.TransformPosition(FVector::ZeroVector);
	TArray<uint8> RotatedDug = Initial;
	TestTrue(TEXT("wall transform round trip"), Wall.InverseTransformPosition(WallHit).Equals(FVector::ZeroVector, .01f));
	TestTrue(TEXT("rotated wall can be dug at world hit"), Carve(RotatedDug, Wall.InverseTransformPosition(WallHit)));
	TestEqual(TEXT("rotated wall has same local density"), Hash(RotatedDug), Hash(Dug));
	TestTrue(TEXT("rotated wall restores in world space"), Restore(RotatedDug, Wall, WallHit, 150.f, NoBoxes));
	TArray<uint8> IdentityRestored = Dug;
	TestTrue(TEXT("identity restore reference"), Restore(IdentityRestored, Identity, FVector::ZeroVector, 150.f, NoBoxes));
	TestEqual(TEXT("rotation preserves restored density"), Hash(RotatedDug), Hash(IdentityRestored));
	TestFalse(TEXT("invalid radius rejected"), Restore(RotatedDug, Wall, WallHit, 301.f, NoBoxes));
	FSurface Plane, Bowl, Again;
	TestTrue(TEXT("initial surface builds"), BuildSurface(Initial, Plane));
	TestTrue(TEXT("curved surface builds"), BuildSurface(Dug, Bowl));
	TestTrue(TEXT("same surface builds again"), BuildSurface(Dug, Again));
	TestEqual(TEXT("deterministic vertex count"), Bowl.Vertices.Num(), Again.Vertices.Num());
	bool bHasSlopedNormal = false, bHasDeepVertex = false;
	for (int32 I = 0; I + 2 < Bowl.Indices.Num(); I += 3)
	{
		const FVector A = Bowl.Vertices[Bowl.Indices[I]], B = Bowl.Vertices[Bowl.Indices[I + 1]], C = Bowl.Vertices[Bowl.Indices[I + 2]];
		const FVector Normal = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
		TestTrue(TEXT("nondegenerate triangle uses UE winding opposite to air normal"), !Normal.IsNearlyZero() && FVector::DotProduct(Normal, Bowl.Normals[Bowl.Indices[I]]) < -.999);
		bHasSlopedNormal |= FMath::Abs(Normal.X) > .1 || FMath::Abs(Normal.Y) > .1;
		bHasDeepVertex |= A.Z < -10 || B.Z < -10 || C.Z < -10;
	}
	TestTrue(TEXT("sphere cut has sloped sides"), bHasSlopedNormal);
	TestTrue(TEXT("sphere cut has depth"), bHasDeepVertex);
	// Every non-boundary edge must be shared by two triangles, including cell seams.
	TMap<FString, int32> EdgeCounts;
	TMap<FString, bool> OuterEdges;
	const auto VertexKey = [](const FVector& V) { return FString::Printf(TEXT("%.3f,%.3f,%.3f"), V.X, V.Y, V.Z); };
	for (int32 I = 0; I + 2 < Bowl.Indices.Num(); I += 3)
	{
		for (int32 E = 0; E < 3; ++E)
		{
			const FVector A = Bowl.Vertices[Bowl.Indices[I + E]], B = Bowl.Vertices[Bowl.Indices[I + (E + 1) % 3]];
			FString KA = VertexKey(A), KB = VertexKey(B);
			if (KB < KA) { Swap(KA, KB); }
			const FString Key = KA + TEXT("|") + KB;
			++EdgeCounts.FindOrAdd(Key);
			const float Limit = CellsXY * Spacing / 2.f;
			OuterEdges.FindOrAdd(Key) = (FMath::IsNearlyEqual(FMath::Abs(A.X), Limit) && FMath::IsNearlyEqual(A.X, B.X)) ||
				(FMath::IsNearlyEqual(FMath::Abs(A.Y), Limit) && FMath::IsNearlyEqual(A.Y, B.Y));
		}
	}
	for (const auto& Edge : EdgeCounts)
	{
		TestTrue(TEXT("cell seam has no open or non-manifold edge"), Edge.Value == 2 || (Edge.Value == 1 && OuterEdges.FindChecked(Edge.Key)));
	}
	for (const FVector& Normal : Plane.Normals) { TestTrue(TEXT("ground faces upwards"), Normal.Z > .99); }
	// ProceduralMesh sets bFlipNormals: Chaos swaps the first two indices when cooking.
	// Test the resulting face normal for both flat ground and the rotated wall fixture.
	for (int32 I = 0; I + 2 < Plane.Indices.Num(); I += 3)
	{
		const FVector A = Plane.Vertices[Plane.Indices[I]];
		const FVector B = Plane.Vertices[Plane.Indices[I + 1]];
		const FVector C = Plane.Vertices[Plane.Indices[I + 2]];
		const FVector CookedNormal = FVector::CrossProduct(A - B, C - B).GetSafeNormal();
		TestTrue(TEXT("flat floor cooked face points up"), CookedNormal.Z > .99);
		const FVector WallNormal = Wall.TransformVectorNoScale(CookedNormal).GetSafeNormal();
		TestTrue(TEXT("rotated wall cooked face points toward entrance"), WallNormal.X < -.99);
		TestTrue(TEXT("cooked face matches air-facing shading normal"), FVector::DotProduct(CookedNormal, Plane.Normals[Plane.Indices[I]]) > .999);
	}
	TArray<uint8> Invalid;
	TestFalse(TEXT("invalid field rejected"), BuildSurface(Invalid, Again));
	return true;
}
#endif
