#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "DigetherSoilField.h"
#include <limits>
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/MemoryWriter.h"

using namespace DigetherSoil;
namespace
{
	// 면별 중복 정점을 위치로 묶어 모든 기하 간선의 면 수 2개 확인
	bool IsSurfaceClosed(const FDigetherSoilSurface& Surface)
	{
		// 렌더 정점은 면별 분리, 닫힌 표면 검사는 같은 위치를 묶어 수행
		TMap<FString, int32> Edges;
		for (int32 Index = 0; Index < Surface.Indices.Num(); Index += 3)
		{
			for (int32 EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
			{
				const FVector A = Surface.Vertices[Surface.Indices[Index + EdgeIndex]],
							  B = Surface.Vertices[Surface.Indices[Index + (EdgeIndex + 1) % 3]];
				FString StartKey = FString::Printf(TEXT("%.5f,%.5f,%.5f"), A.X, A.Y, A.Z);
				FString EndKey = FString::Printf(TEXT("%.5f,%.5f,%.5f"), B.X, B.Y, B.Z);
				if (StartKey > EndKey)
				{
					Swap(StartKey, EndKey);
				}
				++Edges.FindOrAdd(StartKey + TEXT("/") + EndKey);
			}
		}
		for (const TPair<FString, int32>& Edge : Edges)
		{
			if (Edge.Value != 2)
			{
				return false;
			}
		}
		return !Edges.IsEmpty();
	}
} // namespace
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSoilTopologyTest, "DIGETHER.Excavation.SoilCore.Topology",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherSoilTopologyTest::RunTest(const FString& Parameters)
{
	FDigetherSoilGrid Grid;
	TArray<uint8> Density;
	TestTrue(TEXT("initialize"), Initialize(Grid, Density));
	FDigetherSoilSurface Surface;
	if (!TestTrue(TEXT("closed volume builds"), BuildSurface(Grid, Density, Surface)))
	{
		return false;
	}
	TestTrue(TEXT("every geometric edge has two faces"), IsSurfaceClosed(Surface));
	FBox Bounds(ForceInit);
	for (const FVector& Position : Surface.Vertices)
	{
		Bounds += Position;
	}
	TestTrue(TEXT("all six faces close inside sample boundary"),
			 Bounds.Min.Equals(FVector(25), 0.01) && Bounds.Max.Equals(FVector(375), 0.01));
	bool bWinding = true;
	for (int32 Index = 0; Index < Surface.Indices.Num(); Index += 3)
	{
		const FVector A = Surface.Vertices[Index], B = Surface.Vertices[Index + 1], C = Surface.Vertices[Index + 2];
		bWinding &= FVector::DotProduct(FVector::CrossProduct(B - A, C - A), Surface.Normals[Index]) < -0.0001;
		bWinding &= FVector::DotProduct((A + B + C) / 3 - FVector(200), Surface.Normals[Index]) > 0;
	}
	TestTrue(TEXT("outward normals and UE reversed winding"), bWinding);
	FBox Changed;
	TestTrue(TEXT("cut side"),
			 Carve(Grid, Density, FVector(25, 200, 200), 100, {}, Changed) == EDigetherSoilCarveResult::Changed);
	TestTrue(TEXT("cut surface builds"), BuildSurface(Grid, Density, Surface));
	TestTrue(TEXT("cut remains watertight"), IsSurfaceClosed(Surface));
	// 분리된 정점도 같은 위치에서는 음영 노멀 일치 필요
	TMap<FString, FVector> SharedNormals;
	bool bSmooth = true;
	for (int32 Index = 0; Index < Surface.Vertices.Num(); ++Index)
	{
		const FVector Position = Surface.Vertices[Index];
		const FString Key = FString::Printf(TEXT("%.5f,%.5f,%.5f"), Position.X, Position.Y, Position.Z);
		if (const FVector* Previous = SharedNormals.Find(Key))
		{
			bSmooth &= Previous->Equals(Surface.Normals[Index], 0.001);
		}
		else
		{
			SharedNormals.Add(Key, Surface.Normals[Index]);
		}
		bSmooth &=
			!Surface.Normals[Index].ContainsNaN() && FMath::IsNearlyEqual(Surface.Normals[Index].Size(), 1.0, 0.001);
	}
	TestTrue(TEXT("continuous unit normals across triangle boundaries"), bSmooth);
	Density.Init(1, Grid.GetSampleCount());
	TestTrue(TEXT("empty is a valid surface"), BuildSurface(Grid, Density, Surface));
	TestEqual(TEXT("empty replaces previous geometry"), Surface.Indices.Num(), 0);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSoilCarveTest, "DIGETHER.Excavation.SoilCore.Carve",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherSoilCarveTest::RunTest(const FString& Parameters)
{
	FDigetherSoilGrid Grid;
	TArray<uint8> Initial;
	Initialize(Grid, Initial);
	TArray<uint8> Density = Initial, Peer = Initial;
	FBox Changed, Other;
	const FVector Center(200);
	TestTrue(TEXT("interior cut changes"),
			 Carve(Grid, Density, Center, 100, {}, Changed) == EDigetherSoilCarveResult::Changed);
	TestTrue(TEXT("center becomes air"), Density[Grid.GetSampleIndex(4, 4, 4)] < 128);
	Carve(Grid, Peer, Center, 100, {}, Other);
	TestTrue(TEXT("same input same density"), Density == Peer);
	TestTrue(TEXT("same brush is idempotent"),
			 Carve(Grid, Density, Center, 100, {}, Other) == EDigetherSoilCarveResult::NoChange);
	TestFalse(TEXT("no change has no bounds"), bool(Other.IsValid));
	bool bBounds = true, bBoundary = true;
	for (int32 Z = 0; Z <= 8; ++Z)
	{
		for (int32 Y = 0; Y <= 8; ++Y)
		{
			for (int32 X = 0; X <= 8; ++X)
			{
				const int32 Index = Grid.GetSampleIndex(X, Y, Z);
				if (Density[Index] != Initial[Index])
				{
					bBounds &= Changed.IsInsideOrOn(Grid.GetSamplePosition(X, Y, Z));
				}
				if (X == 0 || Y == 0 || Z == 0 || X == 8 || Y == 8 || Z == 8)
				{
					bBoundary &= Density[Index] == Initial[Index];
				}
			}
		}
	}
	TestTrue(TEXT("changed bounds enclose changes"), bBounds);
	TestTrue(TEXT("outer air unchanged"), bBoundary);
	Density = Initial;
	const FBox Protected(FVector(175), FVector(225));
	TestTrue(TEXT("partial protected carve"),
			 Carve(Grid, Density, Center, 300, {Protected}, Changed) == EDigetherSoilCarveResult::Changed);
	bool bProtected = true;
	for (int32 Z = 0; Z <= 8; ++Z)
	{
		for (int32 Y = 0; Y <= 8; ++Y)
		{
			for (int32 X = 0; X <= 8; ++X)
			{
				if (Protected.ExpandBy(Grid.SpacingCm).IsInsideOrOn(Grid.GetSamplePosition(X, Y, Z)))
				{
					bProtected &= Density[Grid.GetSampleIndex(X, Y, Z)] == Initial[Grid.GetSampleIndex(X, Y, Z)];
				}
			}
		}
	}
	TestTrue(TEXT("protected samples and one-cell halo unchanged"), bProtected);
	Density = Initial;
	TestTrue(TEXT("all protected means no change"),
			 Carve(Grid, Density, Center, 300, {FBox(FVector::ZeroVector, FVector(400))}, Changed) ==
				 EDigetherSoilCarveResult::NoChange);
	TestTrue(TEXT("full carve"), Carve(Grid, Density, Center, 600, {}, Changed) == EDigetherSoilCarveResult::Changed);
	FDigetherSoilSurface Surface;
	TestTrue(TEXT("full removal succeeds"), BuildSurface(Grid, Density, Surface));
	TestEqual(TEXT("no triangles after removal"), Surface.Indices.Num(), 0);
	for (const FRotator Rotation : {FRotator::ZeroRotator, FRotator(90, 0, 0), FRotator(180, 0, 0)})
	{
		const FTransform Transform(Rotation, FVector(1000, 500, -100));
		FVector Local;
		TestTrue(TEXT("rigid transform accepted"), WorldToLocal(Transform, Transform.TransformPosition(Center), Local));
		Peer = Initial;
		Carve(Grid, Peer, Local, 100, {}, Other);
		TArray<uint8> Reference = Initial;
		Carve(Grid, Reference, Center, 100, {}, Other);
		TestTrue(TEXT("floor wall ceiling same local result"), Peer == Reference);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSoilValidationTest, "DIGETHER.Excavation.SoilCore.Validation",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherSoilValidationTest::RunTest(const FString& Parameters)
{
	FDigetherSoilGrid Grid;
	TArray<uint8> Density;
	Initialize(Grid, Density);
	const TArray<uint8> Before = Density;
	FBox Changed;
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	for (double Radius : {NaN, std::numeric_limits<double>::infinity(), -1.0, 0.0, 1.e20})
	{
		TestTrue(TEXT("invalid radius rejected"),
				 Carve(Grid, Density, FVector(200), Radius, {}, Changed) == EDigetherSoilCarveResult::InvalidInput);
	}
	TestTrue(TEXT("NaN point rejected"),
			 Carve(Grid, Density, FVector(NaN, 0, 0), 100, {}, Changed) == EDigetherSoilCarveResult::InvalidInput);
	TestTrue(TEXT("outside point rejected"),
			 Carve(Grid, Density, FVector(-1, 0, 0), 100, {}, Changed) == EDigetherSoilCarveResult::InvalidInput);
	TestTrue(TEXT("invalid protection rejected"), Carve(Grid, Density, FVector(200), 100, {FBox(ForceInit)}, Changed) ==
													  EDigetherSoilCarveResult::InvalidInput);
	TestTrue(TEXT("rejection preserves data"), Density == Before);
	FVector Local(7);
	TestFalse(TEXT("zero scale rejected"),
			  WorldToLocal(FTransform(FQuat::Identity, FVector::ZeroVector, FVector(0, 1, 1)), FVector(200), Local));
	TestTrue(TEXT("failed conversion preserves output"), Local == FVector(7));
	Density.Pop();
	FDigetherSoilSurface Surface;
	Surface.Vertices.Add(FVector(7));
	TestFalse(TEXT("wrong array length rejected"), BuildSurface(Grid, Density, Surface));
	TestEqual(TEXT("failed build preserves previous surface"), Surface.Vertices.Num(), 1);
	Density = Before;
	Density[0] = 255;
	TestFalse(TEXT("solid exterior rejected"), BuildSurface(Grid, Density, Surface));
	Grid.Cells.X = MAX_int32;
	TestFalse(TEXT("oversized grid rejected before allocation"), Initialize(Grid, Density));
	Grid = FDigetherSoilGrid();
	Grid.SpacingCm = NaN;
	TestFalse(TEXT("invalid spacing rejected"), Initialize(Grid, Density));
	TestTrue(TEXT("invalid carve grid rejected before geometry math"),
			 Carve(Grid, Density, FVector(200), 100, {}, Changed) == EDigetherSoilCarveResult::InvalidInput);
	return true;
}

namespace
{
	// 실제 표면 출력을 셀별로 비교, 성능 계측과 분리
	void CollectSurfaceCells(const FDigetherSoilGrid& Grid, const FDigetherSoilSurface& Surface,
							 TMap<FIntVector, FString>& OutGeometry, TMap<FIntVector, FString>& OutNormals)
	{
		for (int32 Index = 0; Index < Surface.Vertices.Num(); Index += 3)
		{
			const FVector Center =
				(Surface.Vertices[Index] + Surface.Vertices[Index + 1] + Surface.Vertices[Index + 2]) / 3.0 /
				Grid.SpacingCm;
			const FIntVector Cell(FMath::Clamp(FMath::FloorToInt(Center.X), 0, Grid.Cells.X - 1),
								  FMath::Clamp(FMath::FloorToInt(Center.Y), 0, Grid.Cells.Y - 1),
								  FMath::Clamp(FMath::FloorToInt(Center.Z), 0, Grid.Cells.Z - 1));
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const FVector& Position = Surface.Vertices[Index + Corner];
				const FVector& Normal = Surface.Normals[Index + Corner];
				OutGeometry.FindOrAdd(Cell) +=
					FString::Printf(TEXT("%.9f,%.9f,%.9f;"), Position.X, Position.Y, Position.Z);
				OutNormals.FindOrAdd(Cell) += FString::Printf(TEXT("%.9f,%.9f,%.9f;"), Normal.X, Normal.Y, Normal.Z);
			}
		}
	}
} // namespace
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSoilFootprintTest, "DIGETHER.Excavation.SoilCore.Footprint",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherSoilFootprintTest::RunTest(const FString& Parameters)
{
	FDigetherSoilGrid Grid;
	Grid.Cells = FIntVector(9, 35, 19);
	Grid.SpacingCm = 25;
	TArray<uint8> Initial;
	Initialize(Grid, Initial);
	FString Csv = TEXT("scenario,hit,changed_samples,outside_samples,max_distance_cm,max_outside_drop,geometry_cells,"
					   "normal_cells,changed_bounds,geometry_bounds\n");
	FString CellCsv = TEXT("scenario,hit,x,y,z,dependency,geometry,normal\n");
	const FVector Centers[] = {FVector(12.5, 437.5, 237.5), FVector(12.5, 200, 200), FVector(12.5, 37.5, 37.5)};
	for (int32 Scenario = 0; Scenario < 3; ++Scenario)
	{
		TArray<uint8> Density = Initial;
		for (int32 Hit = 0; Hit < 3; ++Hit)
		{
			// 같은 위치 재타격과 깊은 위치 타격을 구분
			const FVector Point = Centers[Scenario] + FVector(Hit == 2 ? 75 : 0, 0, 0);
			const FVector Quantized(FMath::RoundToDouble(Point.X), FMath::RoundToDouble(Point.Y),
									FMath::RoundToDouble(Point.Z));
			TArray<uint8> Before = Density;
			FDigetherSoilSurface OldSurface, NewSurface;
			BuildSurface(Grid, Before, OldSurface);
			FBox ChangedBounds;
			Carve(Grid, Density, Point, 75, {}, ChangedBounds);
			BuildSurface(Grid, Density, NewSurface);
			TMap<FIntVector, FString> OldGeometry, NewGeometry, OldNormals, NewNormals;
			CollectSurfaceCells(Grid, OldSurface, OldGeometry, OldNormals);
			CollectSurfaceCells(Grid, NewSurface, NewGeometry, NewNormals);
			TSet<FIntVector> DependencyCells;
			int32 Changed = 0, Outside = 0, MaxDrop = 0, GeometryCount = 0, NormalCount = 0;
			double MaxDistance = 0;
			FBox ExactBounds(ForceInit), GeometryBounds(ForceInit);
			for (int32 Z = 0; Z <= Grid.Cells.Z; ++Z)
			{
				for (int32 Y = 0; Y <= Grid.Cells.Y; ++Y)
				{
					for (int32 X = 0; X <= Grid.Cells.X; ++X)
					{
						int32 Index = Grid.GetSampleIndex(X, Y, Z);
						if (Before[Index] == Density[Index])
						{
							continue;
						}
						++Changed;
						const FVector Position = Grid.GetSamplePosition(X, Y, Z);
						ExactBounds += Position;
						double Distance = FVector::Distance(Position, Quantized);
						MaxDistance = FMath::Max(MaxDistance, Distance);
						if (Distance > 75)
						{
							++Outside;
							MaxDrop = FMath::Max(MaxDrop, int32(Before[Index]) - Density[Index]);
						}
						// 기울기 이웃과 해당 정점을 사용하는 셀까지 보수적으로 포함
						for (int32 Dz = -2; Dz <= 1; ++Dz)
						{
							for (int32 Dy = -2; Dy <= 1; ++Dy)
							{
								for (int32 Dx = -2; Dx <= 1; ++Dx)
								{
									FIntVector Cell(X + Dx, Y + Dy, Z + Dz);
									if (Cell.X >= 0 && Cell.Y >= 0 && Cell.Z >= 0 && Cell.X < Grid.Cells.X &&
										Cell.Y < Grid.Cells.Y && Cell.Z < Grid.Cells.Z)
									{
										DependencyCells.Add(Cell);
									}
								}
							}
						}
					}
				}
			}
			for (int32 Z = 0; Z < Grid.Cells.Z; ++Z)
			{
				for (int32 Y = 0; Y < Grid.Cells.Y; ++Y)
				{
					for (int32 X = 0; X < Grid.Cells.X; ++X)
					{
						FIntVector Cell(X, Y, Z);
						bool bGeometry = OldGeometry.FindRef(Cell) != NewGeometry.FindRef(Cell);
						bool bNormal = OldNormals.FindRef(Cell) != NewNormals.FindRef(Cell);
						if (bGeometry)
						{
							++GeometryCount;
							GeometryBounds += Grid.GetSamplePosition(X, Y, Z);
							GeometryBounds += Grid.GetSamplePosition(X + 1, Y + 1, Z + 1);
						}
						if (bNormal)
						{
							++NormalCount;
						}
						if (DependencyCells.Contains(Cell) || bGeometry || bNormal)
						{
							CellCsv += FString::Printf(TEXT("%d,%d,%d,%d,%d,%d,%d,%d\n"), Scenario, Hit, X, Y, Z,
													   DependencyCells.Contains(Cell) ? 1 : 0, bGeometry ? 1 : 0,
													   bNormal ? 1 : 0);
						}
					}
				}
			}
			Csv += FString::Printf(TEXT("%d,%d,%d,%d,%.6f,%d,%d,%d,\"%s\",\"%s\"\n"), Scenario, Hit, Changed, Outside,
								   MaxDistance, MaxDrop, GeometryCount, NormalCount, *ExactBounds.ToString(),
								   *GeometryBounds.ToString());
			if (Hit == 1)
			{
				TestEqual(TEXT("same quantized brush is unchanged"), Changed, 0);
			}
		}
	}
	TArray<uint8> Air;
	Air.Init(1, Grid.GetSampleCount());
	FDigetherSoilSurface Empty;
	TestTrue(TEXT("all air accepts empty surface"), BuildSurface(Grid, Air, Empty));
	TestEqual(TEXT("all air triangles"), Empty.Indices.Num(), 0);
	TArray<uint8> Solid;
	Solid.Init(255, Grid.GetSampleCount());
	TestFalse(TEXT("all solid rejected by whole-volume boundary contract"), BuildSurface(Grid, Solid, Empty));
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("ExcavationMeasurements");
	IFileManager::Get().MakeDirectory(*Directory, true);
	TestTrue(TEXT("write footprint evidence"),
			 FFileHelper::SaveStringToFile(Csv, *(Directory / TEXT("footprint.csv"))));
	TestTrue(TEXT("write cell evidence"), FFileHelper::SaveStringToFile(CellCsv, *(Directory / TEXT("cells.csv"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSoilChunkSurfaceTest, "DIGETHER.Excavation.SoilCore.ChunkSurface",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherSoilChunkSurfaceTest::RunTest(const FString& Parameters)
{
	FDigetherSoilGrid Grid;
	Grid.Cells = FIntVector(9, 35, 19);
	Grid.SpacingCm = 25;
	TArray<uint8> Density;
	Initialize(Grid, Density);
	const auto Collect = [](const FDigetherSoilSurface& Surface)
	{
		TMap<FString, int32> Triangles;
		for (int32 Index = 0; Index < Surface.Vertices.Num(); Index += 3)
		{
			FString Key;
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const FVector& Position = Surface.Vertices[Index + Corner];
				const FVector& Normal = Surface.Normals[Index + Corner];
				Key += FString::Printf(TEXT("%.9f/%.9f/%.9f/%.9f/%.9f/%.9f;"), Position.X, Position.Y, Position.Z,
									   Normal.X, Normal.Y, Normal.Z);
			}
			++Triangles.FindOrAdd(Key);
		}
		return Triangles;
	};
	for (int32 Step = 0; Step < 4; ++Step)
	{
		if (Step > 0)
		{
			FBox Changed;
			Carve(Grid, Density, FVector(12.5 + (Step - 1) * 50, 200, 200), 75, {}, Changed);
		}
		FDigetherSoilSurface Whole;
		TestTrue(TEXT("whole surface"), BuildSurface(Grid, Density, Whole));
		const TMap<FString, int32> Expected = Collect(Whole);
		for (int32 Size : {8, 16})
		{
			FDigetherSoilSurface Joined;
			for (int32 Z = 0; Z < Grid.Cells.Z; Z += Size)
			{
				for (int32 Y = 0; Y < Grid.Cells.Y; Y += Size)
				{
					for (int32 X = 0; X < Grid.Cells.X; X += Size)
					{
						FDigetherSoilSurface Part;
						const EDigetherSoilRangeState State = BuildSurfaceRange(
							Grid, Density, FIntVector(X, Y, Z),
							FIntVector(FMath::Min(X + Size, Grid.Cells.X), FMath::Min(Y + Size, Grid.Cells.Y),
									   FMath::Min(Z + Size, Grid.Cells.Z)),
							Part);
						TestTrue(TEXT("range extraction succeeds"), State != EDigetherSoilRangeState::Failed);
						Joined.Vertices.Append(Part.Vertices);
						Joined.Normals.Append(Part.Normals);
					}
				}
			}
			const TMap<FString, int32> Actual = Collect(Joined);
			TestEqual(TEXT("triangle multiplicity matches"), Actual.Num(), Expected.Num());
			bool bEqual = true;
			for (const TPair<FString, int32>& Entry : Expected)
			{
				bEqual &= Actual.FindRef(Entry.Key) == Entry.Value;
			}
			TestTrue(TEXT("positions winding normals match including seams"), bEqual);
		}
	}
	Initialize(Grid, Density);
	FDigetherSoilSurface Empty;
	TestTrue(TEXT("interior solid chunk is valid empty geometry"),
			 BuildSurfaceRange(Grid, Density, FIntVector(2, 2, 2), FIntVector(3, 3, 3), Empty) ==
				 EDigetherSoilRangeState::Solid);
	Density.Init(1, Grid.GetSampleCount());
	TestTrue(TEXT("air chunk distinct from solid"),
			 BuildSurfaceRange(Grid, Density, FIntVector(2, 2, 2), FIntVector(3, 3, 3), Empty) ==
				 EDigetherSoilRangeState::Air);
	TestTrue(TEXT("invalid range rejected"), BuildSurfaceRange(Grid, Density, FIntVector(-1, 0, 0), FIntVector(3, 3, 3),
															   Empty) == EDigetherSoilRangeState::Failed);
	return true;
}

// 전후 실행 결과 비교용 직렬화, 구조체 패딩 대신 실제 배열 값만 기록
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSoilSnapshotTest, "DIGETHER.Excavation.SoilCore.Snapshot",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherSoilSnapshotTest::RunTest(const FString& Parameters)
{
	FDigetherSoilGrid Grid;
	Grid.Cells = FIntVector(9, 35, 19);
	Grid.SpacingCm = 25;
	TArray<uint8> Density, Bytes;
	TestTrue(TEXT("snapshot initialize"), Initialize(Grid, Density));
	FMemoryWriter Writer(Bytes);
	const auto WriteSurface = [&Writer](FDigetherSoilSurface& Surface)
	{
		Writer << Surface.Vertices << Surface.Indices << Surface.Normals << Surface.UVs;
	};
	const FBox ProtectedBox(FVector(100, 300, 100), FVector(125, 350, 150));
	const TArray<FBox> ProtectedBoxes = {ProtectedBox, ProtectedBox, FBox(FVector(25), FVector(50))};
	const FVector Points[] = {FVector(12.5, 400, 200), FVector(12.5, 400, 200), FVector(100, 400, 200),
		FVector(100, 200, 25), FVector(100, 200, 450), FVector(100, 325, 125)};
	for (const FVector& Point : Points)
	{
		FBox Changed;
		uint8 Result = static_cast<uint8>(Carve(Grid, Density, Point, 75, ProtectedBoxes, Changed));
		Writer << Result << Density << Changed;
		FDigetherSoilSurface Surface;
		TestTrue(TEXT("snapshot whole surface"), BuildSurface(Grid, Density, Surface));
		WriteSurface(Surface);
		for (const int32 Size : {8, 16})
		{
			for (int32 Z = 0; Z < Grid.Cells.Z; Z += Size)
			{
				for (int32 Y = 0; Y < Grid.Cells.Y; Y += Size)
				{
					for (int32 X = 0; X < Grid.Cells.X; X += Size)
					{
						const FIntVector Max(FMath::Min(X + Size, Grid.Cells.X),
							FMath::Min(Y + Size, Grid.Cells.Y), FMath::Min(Z + Size, Grid.Cells.Z));
						const EDigetherSoilRangeState State = BuildSurfaceRange(Grid, Density, FIntVector(X, Y, Z), Max, Surface);
						TestTrue(TEXT("snapshot chunk extraction"), State != EDigetherSoilRangeState::Failed);
						uint8 StateValue = static_cast<uint8>(State);
						Writer << StateValue;
						WriteSurface(Surface);
					}
				}
			}
		}
	}
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("ExcavationMeasurements");
	IFileManager::Get().MakeDirectory(*Directory, true);
	TestTrue(TEXT("write exact comparison evidence"),
		FFileHelper::SaveArrayToFile(Bytes, *(Directory / TEXT("surface-snapshot.bin"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSoilScaledBrushTest, "DIGETHER.Excavation.SoilCore.ScaledBrush",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherSoilScaledBrushTest::RunTest(const FString& Parameters)
{
    FDigetherSoilGrid Grid;
    Grid.Cells = FIntVector(16, 16, 16);
    Grid.SpacingCm = 25;
    const FVector Center(200);
    for (const FVector Scale : {FVector(2, 1, 1), FVector(.5, 2, 1), FVector(1, 1, 1)})
    {
        TArray<uint8> Density;
        Initialize(Grid, Density);
        const TArray<uint8> Before = Density;
        const FTransform Transform(FRotator(30, 65, 15), FVector(900, -300, 120), Scale);
        FVector Local;
        TestTrue(TEXT("translated rotated scaled point accepted"), WorldToLocal(Transform, Transform.TransformPosition(Center), Local));
        TestTrue(TEXT("world to local round trip"), Local.Equals(Center, 1.e-6));
        FBox Changed;
        TestTrue(TEXT("scaled brush applied"), Carve(Grid, Density, Local, 75, {}, Changed, Scale) == EDigetherSoilCarveResult::Changed);
        for (int32 Z = 1; Z < Grid.Cells.Z; ++Z)
            for (int32 Y = 1; Y < Grid.Cells.Y; ++Y)
                for (int32 X = 1; X < Grid.Cells.X; ++X)
                {
                    const int32 Index = Grid.GetSampleIndex(X, Y, Z);
                    if (Before[Index] <= 128) continue;
                    const double Distance = FVector::Distance(Transform.TransformPosition(Grid.GetSamplePosition(X,Y,Z)), Transform.TransformPosition(Center));
                    if (Distance < 73) TestTrue(TEXT("inside world radius is air"), Density[Index] <= 128);
                    if (Distance > 77) TestTrue(TEXT("outside world radius remains solid"), Density[Index] > 128);
                }
        Density = Before;
        TestTrue(TEXT("scaled protection unchanged"), Carve(Grid, Density, Center, 75,
            {FBox(FVector::ZeroVector, FVector(400))}, Changed, Scale) == EDigetherSoilCarveResult::NoChange);
        TestTrue(TEXT("protected density preserved"), Density == Before);
    }
    return true;
}

#endif
