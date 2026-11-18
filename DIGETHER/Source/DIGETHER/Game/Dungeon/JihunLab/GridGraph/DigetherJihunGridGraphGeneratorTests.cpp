#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "DigetherJihunGridGraphCatalog.h"
#include "DigetherJihunGridGraphGenerator.h"

// 생성기 내부 검사와 별개로, 결과 배치와 모듈 면 데이터만 보고 다시 판정
namespace DigetherJihunGridGraphTests
{
	using namespace DigetherJihunGridGraph;
	using FSpec = FDigetherJihunGridGraphModuleSpec;
	using FLayout = FDigetherJihunGridGraphLayout;
	using FPlacement = FDigetherJihunGridGraphPlacement;
	using FWorldEdge = FDigetherJihunGridGraphWorldEdge;
	using EType = EDigetherJihunGridGraphEdgeType;
	using ERole = EDigetherJihunGridGraphRole;
	using EDir = EDigetherJihunGridGraphDir;

	constexpr int32 SeedCount = 100;
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	struct FPlacedEdge
	{
		int32 Placement = INDEX_NONE;
		FWorldEdge Edge;
		// 회전 전 모듈 기준 면(복도 N = 교실 쪽, S = 바깥 창)
		EDir LocalSide = EDir::North;
	};

	bool IsPassage(const EType Type)
	{
		return Type == EType::Open || Type == EType::Door;
	}

	bool Faces(const FWorldEdge& A, const FWorldEdge& B)
	{
		return A.Floor == B.Floor && A.Dir == OppositeDir(B.Dir) && A.LineCm == B.LineCm
			&& FMath::Min(A.EndCm, B.EndCm) > FMath::Max(A.StartCm, B.StartCm);
	}

	// 배치 하나의 면·연결·로비 거리
	struct FAnalysis
	{
		TArray<FPlacedEdge> Edges;
		// 같은 층·같은 선 위 면 묶음(마주 보는 면 찾기용)
		TMap<FIntVector, TArray<int32>> ByLine;
		TArray<TArray<int32>> Links;
		TArray<int32> Dangling;
		TArray<int32> Depth;
		int32 Lobby = INDEX_NONE;

		static FIntVector LineKey(const FWorldEdge& Edge)
		{
			const bool bAlongX = Edge.Dir == EDir::North || Edge.Dir == EDir::South;
			return FIntVector(Edge.Floor, bAlongX ? 1 : 0, Edge.LineCm);
		}

		FAnalysis(const FLayout& Layout, const TArray<FSpec>& Catalog, const int32 UnitCm)
		{
			const int32 Count = Layout.Placements.Num();
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const FPlacement& P = Layout.Placements[Index];
				Lobby = Catalog[P.ModuleIndex].Role == ERole::Lobby ? Index : Lobby;
				TArray<FWorldEdge> World;
				const FSpec& Spec = Catalog[P.ModuleIndex];
				GetWorldEdges(Spec, P, UnitCm, World);
				// GetWorldEdges는 Spec.Edges 순서대로 하나씩 만듦
				for (int32 EdgeIndex = 0; EdgeIndex < World.Num(); ++EdgeIndex)
				{
					ByLine.FindOrAdd(LineKey(World[EdgeIndex])).Add(Edges.Add({ Index, World[EdgeIndex], Spec.Edges[EdgeIndex].Side }));
				}
			}
			Links.SetNum(Count);
			for (int32 EdgeIndex = 0; EdgeIndex < Edges.Num(); ++EdgeIndex)
			{
				const FPlacedEdge& Edge = Edges[EdgeIndex];
				if (!IsPassage(Edge.Edge.Type))
				{
					continue;
				}
				bool bPartner = false;
				for (const int32 OtherIndex : ByLine.FindChecked(LineKey(Edge.Edge)))
				{
					const FPlacedEdge& Other = Edges[OtherIndex];
					if (Other.Placement != Edge.Placement && IsPassage(Other.Edge.Type) && Faces(Edge.Edge, Other.Edge))
					{
						bPartner = true;
						Links[Edge.Placement].AddUnique(Other.Placement);
					}
				}
				if (!bPartner)
				{
					Dangling.Add(EdgeIndex);
				}
			}
			Depth.Init(INDEX_NONE, Count);
			if (Lobby != INDEX_NONE)
			{
				Depth[Lobby] = 0;
				TArray<int32> Queue = { Lobby };
				for (int32 Head = 0; Head < Queue.Num(); ++Head)
				{
					for (const int32 Next : Links[Queue[Head]])
					{
						if (Depth[Next] == INDEX_NONE)
						{
							Depth[Next] = Depth[Queue[Head]] + 1;
							Queue.Add(Next);
						}
					}
				}
			}
		}
	};

	FString Describe(const FLayout& Layout, const TArray<FSpec>& Catalog, const int32 Index)
	{
		const FPlacement& P = Layout.Placements[Index];
		return FString::Printf(TEXT("seed %d #%d %s pivot(%d,%d) floor %d turns %d"), Layout.SeedUsed, Index,
			*Catalog[P.ModuleIndex].Id.ToString(), P.PivotUnits.X, P.PivotUnits.Y, P.Floor, P.QuarterTurns);
	}

	FName IdOf(const FLayout& Layout, const TArray<FSpec>& Catalog, const int32 Index)
	{
		return Catalog[Layout.Placements[Index].ModuleIndex].Id;
	}

	struct FFixture
	{
		TArray<FSpec> Catalog = MakeGleyBoxCatalog();
		FDigetherJihunGridGraphRules Rules;

		FLayout Generate(const int32 Seed) const
		{
			return FDigetherJihunGridGraphGenerator::Generate(Catalog, Rules, Seed);
		}
	};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJihunGridGraphTransformTest, "DIGETHER.Game.Dungeon.JihunLab.GridGraph.Transforms", TestFlags)
bool FJihunGridGraphTransformTest::RunTest(const FString& Parameters)
{
	// 회전한 칸 집합이 RotatedMinOffset·RotatedSize 사각형과 정확히 일치해야 함
	const FIntPoint Size(4, 8);
	for (int32 Turns = 0; Turns < 4; ++Turns)
	{
		const FIntPoint Min = RotatedMinOffset(Size, Turns);
		const FIntPoint RectSize = RotatedSize(Size, Turns);
		TSet<FIntPoint> Cells;
		for (int32 X = 0; X < Size.X; ++X)
		{
			for (int32 Y = 0; Y < Size.Y; ++Y)
			{
				const FIntPoint Cell = RotateCell(FIntPoint(X, Y), Turns);
				Cells.Add(Cell);
				TestTrue(FString::Printf(TEXT("turns %d cell inside rect"), Turns),
					Cell.X >= Min.X && Cell.Y >= Min.Y && Cell.X < Min.X + RectSize.X && Cell.Y < Min.Y + RectSize.Y);
			}
		}
		TestEqual(FString::Printf(TEXT("turns %d cell count"), Turns), Cells.Num(), Size.X * Size.Y);
		// yaw 90 = 로컬 +X가 월드 +Y(GleyBox 실측 배치와 같은 규칙)
		TestEqual(TEXT("east rotates by quarter turns"), RotateDir(EDir::East, Turns), static_cast<EDir>(Turns));
	}
	TestEqual(TEXT("point turn 1"), RotatePointCm(FIntPoint(800, 0), 1), FIntPoint(0, 800));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJihunGridGraphDeterminismTest, "DIGETHER.Game.Dungeon.JihunLab.GridGraph.Determinism", TestFlags)
bool FJihunGridGraphDeterminismTest::RunTest(const FString& Parameters)
{
	const FFixture Fixture;
	bool bAnyDifferent = false;
	const FLayout Base = Fixture.Generate(0);
	for (int32 Seed = 0; Seed < 10; ++Seed)
	{
		const FLayout A = Fixture.Generate(Seed);
		const FLayout B = Fixture.Generate(Seed);
		TestEqual(FString::Printf(TEXT("seed %d same seed used"), Seed), A.SeedUsed, B.SeedUsed);
		TestTrue(FString::Printf(TEXT("seed %d same placements"), Seed), A.Placements == B.Placements);
		bAnyDifferent |= !(A.Placements == Base.Placements);
	}
	TestTrue(TEXT("different seeds give different dungeons"), bAnyDifferent);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJihunGridGraphNoOverlapTest, "DIGETHER.Game.Dungeon.JihunLab.GridGraph.NoOverlap", TestFlags)
bool FJihunGridGraphNoOverlapTest::RunTest(const FString& Parameters)
{
	const FFixture Fixture;
	for (int32 Seed = 0; Seed < SeedCount; ++Seed)
	{
		const FLayout Layout = Fixture.Generate(Seed);
		if (!TestTrue(FString::Printf(TEXT("seed %d succeeds: %s"), Seed, *Layout.FailureReason), Layout.bSuccess))
		{
			continue;
		}
		TMap<FIntVector, int32> Owners;
		TSet<int32> UsedFloors;
		for (int32 Index = 0; Index < Layout.Placements.Num(); ++Index)
		{
			const FPlacement& P = Layout.Placements[Index];
			const FSpec& Spec = Fixture.Catalog[P.ModuleIndex];
			for (int32 Z = P.Floor; Z < P.Floor + Spec.FloorSpan; ++Z)
			{
				UsedFloors.Add(Z);
			}
			if (Spec.Role == ERole::ClassroomInterior)
			{
				continue;
			}
			FIntPoint Min;
			FIntPoint Size;
			GetPlacementRect(Spec, P, Min, Size);
			for (int32 Z = P.Floor; Z < P.Floor + Spec.FloorSpan; ++Z)
			{
				for (int32 X = Min.X; X < Min.X + Size.X; ++X)
				{
					for (int32 Y = Min.Y; Y < Min.Y + Size.Y; ++Y)
					{
						if (const int32* Previous = Owners.Find(FIntVector(X, Y, Z)))
						{
							AddError(FString::Printf(TEXT("overlap: %s with %s"), *Describe(Layout, Fixture.Catalog, Index), *Describe(Layout, Fixture.Catalog, *Previous)));
						}
						Owners.Add(FIntVector(X, Y, Z), Index);
					}
				}
			}
		}
		TestEqual(FString::Printf(TEXT("seed %d every floor used"), Seed), UsedFloors.Num(), Fixture.Rules.FloorCount);
		TestEqual(FString::Printf(TEXT("seed %d floor count"), Seed), Layout.FloorCount, Fixture.Rules.FloorCount);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJihunGridGraphConnectivityTest, "DIGETHER.Game.Dungeon.JihunLab.GridGraph.Connectivity", TestFlags)
bool FJihunGridGraphConnectivityTest::RunTest(const FString& Parameters)
{
	const FFixture Fixture;
	for (int32 Seed = 0; Seed < SeedCount; ++Seed)
	{
		const FLayout Layout = Fixture.Generate(Seed);
		if (!Layout.bSuccess)
		{
			continue;
		}
		const FAnalysis Analysis(Layout, Fixture.Catalog, Fixture.Rules.UnitSizeCm);
		if (!TestNotEqual(TEXT("lobby placed"), Analysis.Lobby, static_cast<int32>(INDEX_NONE)))
		{
			continue;
		}
		// 열린 면·문이 허공이나 벽을 보면 안 됨
		for (const int32 EdgeIndex : Analysis.Dangling)
		{
			const FPlacedEdge& Edge = Analysis.Edges[EdgeIndex];
			AddError(FString::Printf(TEXT("dangling %s side %d floor %d: %s"), Edge.Edge.Type == EType::Door ? TEXT("door") : TEXT("opening"),
				static_cast<int32>(Edge.Edge.Dir), Edge.Edge.Floor, *Describe(Layout, Fixture.Catalog, Edge.Placement)));
		}
		for (int32 Index = 0; Index < Layout.Placements.Num(); ++Index)
		{
			const bool bInterior = Fixture.Catalog[Layout.Placements[Index].ModuleIndex].Role == ERole::ClassroomInterior;
			if (!bInterior && Analysis.Depth[Index] == INDEX_NONE)
			{
				AddError(FString::Printf(TEXT("unreachable: %s"), *Describe(Layout, Fixture.Catalog, Index)));
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJihunGridGraphDistanceTest, "DIGETHER.Game.Dungeon.JihunLab.GridGraph.Distance", TestFlags)
bool FJihunGridGraphDistanceTest::RunTest(const FString& Parameters)
{
	// 기획: 가장 빠른 길로 가장 먼 곳까지 약 40모듈, 전체 약 500
	const FFixture Fixture;
	for (int32 Seed = 0; Seed < SeedCount; ++Seed)
	{
		const FLayout Layout = Fixture.Generate(Seed);
		if (!Layout.bSuccess)
		{
			continue;
		}
		const FAnalysis Analysis(Layout, Fixture.Catalog, Fixture.Rules.UnitSizeCm);
		int32 Farthest = 0;
		int32 Total = 0;
		for (int32 Index = 0; Index < Layout.Placements.Num(); ++Index)
		{
			if (Fixture.Catalog[Layout.Placements[Index].ModuleIndex].Role != ERole::ClassroomInterior)
			{
				++Total;
				Farthest = FMath::Max(Farthest, Analysis.Depth[Index]);
			}
		}
		TestEqual(FString::Printf(TEXT("seed %d farthest matches layout"), Seed), Farthest, Layout.FarthestModules);
		TestEqual(FString::Printf(TEXT("seed %d total matches layout"), Seed), Total, Layout.TotalModules);
		TestTrue(FString::Printf(TEXT("seed %d farthest %d in range"), Seed, Farthest),
			Farthest >= Fixture.Rules.MinFarthestModules && Farthest <= Fixture.Rules.MaxFarthestModules);
		TestTrue(FString::Printf(TEXT("seed %d total %d in range"), Seed, Total),
			Total >= Fixture.Rules.MinTotalModules && Total <= Fixture.Rules.MaxTotalModules);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJihunGridGraphFloorShapeTest, "DIGETHER.Game.Dungeon.JihunLab.GridGraph.FloorShape", TestFlags)
bool FJihunGridGraphFloorShapeTest::RunTest(const FString& Parameters)
{
	// 기획: 2층 로비 시작, 1층·5층은 계단 하나 + 짧은 복도(1~3개) 또는 교실 하나
	const FFixture Fixture;
	const int32 Top = Fixture.Rules.FloorCount - 1;
	for (int32 Seed = 0; Seed < SeedCount; ++Seed)
	{
		const FLayout Layout = Fixture.Generate(Seed);
		if (!Layout.bSuccess)
		{
			continue;
		}
		TMap<int32, int32> ModulesOnFloor;
		TMap<int32, int32> StairsTouchingFloor;
		for (int32 Index = 0; Index < Layout.Placements.Num(); ++Index)
		{
			const FPlacement& P = Layout.Placements[Index];
			const FSpec& Spec = Fixture.Catalog[P.ModuleIndex];
			if (Spec.Role == ERole::Lobby)
			{
				TestEqual(FString::Printf(TEXT("seed %d lobby floor"), Seed), P.Floor, Fixture.Rules.LobbyFloor);
			}
			if (Spec.Role == ERole::Stairs)
			{
				++StairsTouchingFloor.FindOrAdd(P.Floor);
				++StairsTouchingFloor.FindOrAdd(P.Floor + 1);
			}
			else if (Spec.Role != ERole::ClassroomInterior)
			{
				++ModulesOnFloor.FindOrAdd(P.Floor);
			}
		}
		// 계단 칸 + 짧은 복도 + 교실 하나
		const int32 DeadEndLimit = 1 + Fixture.Rules.DeadEndMaxModules + 1;
		for (const int32 Floor : { 0, Top })
		{
			TestEqual(FString::Printf(TEXT("seed %d floor %d one stairs"), Seed, Floor), StairsTouchingFloor.FindRef(Floor), 1);
			TestTrue(FString::Printf(TEXT("seed %d floor %d small (%d)"), Seed, Floor, ModulesOnFloor.FindRef(Floor)),
				ModulesOnFloor.FindRef(Floor) >= 1 && ModulesOnFloor.FindRef(Floor) <= DeadEndLimit);
		}
		for (int32 Floor = 1; Floor < Top; ++Floor)
		{
			TestTrue(FString::Printf(TEXT("seed %d main floor %d larger than dead end"), Seed, Floor), ModulesOnFloor.FindRef(Floor) > DeadEndLimit);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJihunGridGraphJunctionRuleTest, "DIGETHER.Game.Dungeon.JihunLab.GridGraph.JunctionRule", TestFlags)
bool FJihunGridGraphJunctionRuleTest::RunTest(const FString& Parameters)
{
	// 기획: T자는 층당 0~3개, 교실과 직접 연결 없음, 계단은 T자 바로 다음에 오지 않음
	const FFixture Fixture;
	const FName Tee(TEXT("Corridor_102"));
	for (int32 Seed = 0; Seed < SeedCount; ++Seed)
	{
		const FLayout Layout = Fixture.Generate(Seed);
		if (!Layout.bSuccess)
		{
			continue;
		}
		const FAnalysis Analysis(Layout, Fixture.Catalog, Fixture.Rules.UnitSizeCm);
		TMap<int32, int32> TeesOnFloor;
		for (int32 Index = 0; Index < Layout.Placements.Num(); ++Index)
		{
			const FSpec& Spec = Fixture.Catalog[Layout.Placements[Index].ModuleIndex];
			if (Spec.Id == Tee)
			{
				++TeesOnFloor.FindOrAdd(Layout.Placements[Index].Floor);
				for (const int32 Next : Analysis.Links[Index])
				{
					const ERole Role = Fixture.Catalog[Layout.Placements[Next].ModuleIndex].Role;
					if (Role == ERole::Classroom || Role == ERole::Stairs)
					{
						AddError(FString::Printf(TEXT("tee linked to role %d: %s"), static_cast<int32>(Role), *Describe(Layout, Fixture.Catalog, Index)));
					}
				}
			}
		}
		for (const TPair<int32, int32>& Pair : TeesOnFloor)
		{
			TestTrue(FString::Printf(TEXT("seed %d floor %d tees %d"), Seed, Pair.Key, Pair.Value), Pair.Value <= Fixture.Rules.MaxTeesPerFloor);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJihunGridGraphClassroomRuleTest, "DIGETHER.Game.Dungeon.JihunLab.GridGraph.ClassroomRule", TestFlags)
bool FJihunGridGraphClassroomRuleTest::RunTest(const FString& Parameters)
{
	// 교실 열린 면은 복도 문과만 마주 봄, Corridor_002(복도창)는 교실 앞 3칸 세트 가운데에만
	const FFixture Fixture;
	const FName WindowCorridor(TEXT("Corridor_002"));
	int32 Checked002 = 0;
	for (int32 Seed = 0; Seed < SeedCount; ++Seed)
	{
		const FLayout Layout = Fixture.Generate(Seed);
		if (!Layout.bSuccess)
		{
			continue;
		}
		const FAnalysis Analysis(Layout, Fixture.Catalog, Fixture.Rules.UnitSizeCm);
		for (int32 EdgeIndex = 0; EdgeIndex < Analysis.Edges.Num(); ++EdgeIndex)
		{
			const FPlacedEdge& Edge = Analysis.Edges[EdgeIndex];
			const FSpec& Spec = Fixture.Catalog[Layout.Placements[Edge.Placement].ModuleIndex];
			const bool bClassroomOpen = Spec.Role == ERole::Classroom && Edge.Edge.Type == EType::Open;
			const bool bWindow002 = Spec.Id == WindowCorridor && Edge.Edge.Type == EType::Window && Edge.LocalSide == EDir::North;
			if (!bClassroomOpen && !bWindow002)
			{
				continue;
			}
			Checked002 += bWindow002 ? 1 : 0;
			bool bDoorFacing = false;
			bool bClassroomFacing = false;
			FString Nearby;
			for (const int32 OtherIndex : Analysis.ByLine.FindChecked(FAnalysis::LineKey(Edge.Edge)))
			{
				const FPlacedEdge& Other = Analysis.Edges[OtherIndex];
				if (Other.Placement != Edge.Placement && Other.Edge.Dir == OppositeDir(Edge.Edge.Dir))
				{
					Nearby += FString::Printf(TEXT(" [%s type %d %d~%d]"), *IdOf(Layout, Fixture.Catalog, Other.Placement).ToString(),
						static_cast<int32>(Other.Edge.Type), Other.Edge.StartCm, Other.Edge.EndCm);
				}
				if (Other.Placement == Edge.Placement || !Faces(Edge.Edge, Other.Edge))
				{
					continue;
				}
				const FSpec& OtherSpec = Fixture.Catalog[Layout.Placements[Other.Placement].ModuleIndex];
				bDoorFacing |= Other.Edge.Type == EType::Door && OtherSpec.Role == ERole::Corridor;
				bClassroomFacing |= OtherSpec.Role == ERole::Classroom && Other.Edge.Type == EType::Open;
			}
			if (bClassroomOpen && !bDoorFacing)
			{
				AddError(FString::Printf(TEXT("classroom opening without corridor door: %s"), *Describe(Layout, Fixture.Catalog, Edge.Placement)));
			}
			if (bWindow002 && !bClassroomFacing)
			{
				AddError(FString::Printf(TEXT("Corridor_002 not in a classroom set: %s window %d~%d facing:%s"), *Describe(Layout, Fixture.Catalog, Edge.Placement),
					Edge.Edge.StartCm, Edge.Edge.EndCm, *Nearby));
			}
		}
	}
	TestTrue(TEXT("Corridor_002 was generated"), Checked002 > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJihunGridGraphExcludedModulesTest, "DIGETHER.Game.Dungeon.JihunLab.GridGraph.ExcludedModules", TestFlags)
bool FJihunGridGraphExcludedModulesTest::RunTest(const FString& Parameters)
{
	const FFixture Fixture;
	// 제외: 교실 사이 연결 복도 103(후속), 뒤쪽 전체가 열린 교실 002(해석 확인 전 임시 가정)
	const TSet<FName> Excluded = { TEXT("Corridor_103"), TEXT("classroom_002") };
	for (int32 Seed = 0; Seed < SeedCount; ++Seed)
	{
		const FLayout Layout = Fixture.Generate(Seed);
		for (int32 Index = 0; Index < Layout.Placements.Num(); ++Index)
		{
			if (Excluded.Contains(IdOf(Layout, Fixture.Catalog, Index)))
			{
				AddError(FString::Printf(TEXT("excluded module used: %s"), *Describe(Layout, Fixture.Catalog, Index)));
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJihunGridGraphCoverageTest, "DIGETHER.Game.Dungeon.JihunLab.GridGraph.Coverage", TestFlags)
bool FJihunGridGraphCoverageTest::RunTest(const FString& Parameters)
{
	// 다른 테스트가 단순한 결과만 보고 통과하지 않도록 고리·T자·다리·교실 변형이 실제로 나오는지 확인
	const FFixture Fixture;
	TMap<FName, int32> Uses;
	int32 Successes = 0;
	int32 TotalAttempts = 0;
	int64 SumFarthest = 0;
	int64 SumTotal = 0;
	int64 SumLoops = 0;
	int32 MinTotal = MAX_int32;
	int32 MaxTotal = 0;
	int32 LayoutsWithLoops = 0;
	int64 SumThrough = 0;
	int64 SumInlineStairs = 0;
	for (int32 Seed = 0; Seed < SeedCount; ++Seed)
	{
		const FLayout Layout = Fixture.Generate(Seed);
		TotalAttempts += Layout.Attempts;
		if (!Layout.bSuccess)
		{
			continue;
		}
		++Successes;
		SumFarthest += Layout.FarthestModules;
		SumTotal += Layout.TotalModules;
		SumThrough += Layout.ThroughClassroomCount;
		SumInlineStairs += Layout.InlineStairCount;
		MinTotal = FMath::Min(MinTotal, Layout.TotalModules);
		MaxTotal = FMath::Max(MaxTotal, Layout.TotalModules);
		for (const FPlacement& P : Layout.Placements)
		{
			++Uses.FindOrAdd(Fixture.Catalog[P.ModuleIndex].Id);
		}
		// 연결 그래프의 독립 고리 수 = 간선 - 정점 + 1(전부 연결된 경우)
		const FAnalysis Analysis(Layout, Fixture.Catalog, Fixture.Rules.UnitSizeCm);
		int32 LinkCount = 0;
		for (const TArray<int32>& Links : Analysis.Links)
		{
			LinkCount += Links.Num();
		}
		const int32 Loops = LinkCount / 2 - Layout.TotalModules + 1;
		SumLoops += Loops;
		LayoutsWithLoops += Loops > 0 ? 1 : 0;
	}
	TestTrue(TEXT("most seeds succeed (95%+)"), Successes * 100 >= SeedCount * 95);
	TestTrue(TEXT("corner used"), Uses.FindRef(TEXT("Corridor_101")) > 0);
	TestTrue(TEXT("tee used"), Uses.FindRef(TEXT("Corridor_102")) > 0);
	TestTrue(TEXT("bridge used"), Uses.FindRef(TEXT("Corridor_201")) + Uses.FindRef(TEXT("Corridor_202")) > 0);
	TestTrue(TEXT("full-front classroom used"), Uses.FindRef(TEXT("classroom_001")) > 0);
	TestTrue(TEXT("single-door classroom used"), Uses.FindRef(TEXT("classroom_009")) + Uses.FindRef(TEXT("classroom_010")) > 0);
	TestTrue(TEXT("interior used"), Uses.FindRef(TEXT("inside_001")) + Uses.FindRef(TEXT("inside_002")) + Uses.FindRef(TEXT("inside_003")) > 0);
	TestTrue(TEXT("loops in most layouts"), LayoutsWithLoops * 100 >= FMath::Max(1, Successes) * 90);
	TestTrue(TEXT("through classrooms used"), SumThrough > 0);
	TestTrue(TEXT("inline stairs used"), SumInlineStairs > 0 && Uses.FindRef(TEXT("Stairs_001")) > 0);

	TArray<FName> Names;
	Uses.GetKeys(Names);
	Names.Sort(FNameLexicalLess());
	FString Summary;
	for (const FName& Name : Names)
	{
		Summary += FString::Printf(TEXT("%s=%d "), *Name.ToString(), Uses[Name]);
	}
	const float Den = static_cast<float>(FMath::Max(1, Successes));
	AddInfo(FString::Printf(TEXT("success=%d/%d avgAttempts=%.2f avgFarthest=%.1f avgTotal=%.1f (min %d max %d) avgLoops=%.1f avgThrough=%.1f avgInlineStairs=%.1f"),
		Successes, SeedCount, TotalAttempts / float(SeedCount), SumFarthest / Den, SumTotal / Den, MinTotal, MaxTotal, SumLoops / Den,
		SumThrough / Den, SumInlineStairs / Den));
	AddInfo(Summary);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJihunGridGraphTuningReportTest, "DIGETHER.Game.Dungeon.JihunLab.GridGraph.TuningReport", TestFlags)
bool FJihunGridGraphTuningReportTest::RunTest(const FString& Parameters)
{
	// 판정 없이 수치만 보고. 기획 목표(가장 먼 곳 약 40, 전체 약 500)와 긴 복도 조건의 균형 조정용
	struct FVariant
	{
		const TCHAR* Name;
		int32 MinRun;
		int32 MaxRun;
		int32 MaxFarthest;
		int32 MinNodes;
		int32 MaxNodes;
	};
	const FVariant Variants[] = {
		{ TEXT("default run2-6 far<=60 nodes16-28"), 2, 6, 60, 16, 28 },
		{ TEXT("run2-6 far<=48 nodes16-28"), 2, 6, 48, 16, 28 },
		{ TEXT("run2-6 far<=80 nodes16-28"), 2, 6, 80, 16, 28 },
		{ TEXT("run2-6 far<=60 nodes30-45"), 2, 6, 60, 30, 45 },
		{ TEXT("run2-6 far<=80 nodes30-45"), 2, 6, 80, 30, 45 },
		{ TEXT("run2-4 far<=48 nodes30-45"), 2, 4, 48, 30, 45 },
		{ TEXT("run2-4 far<=60 nodes30-45"), 2, 4, 60, 30, 45 },
	};
	constexpr int32 Seeds = 50;
	for (const FVariant& Variant : Variants)
	{
		FFixture Fixture;
		Fixture.Rules.MinRunModules = Variant.MinRun;
		Fixture.Rules.MaxRunModules = Variant.MaxRun;
		Fixture.Rules.MaxFarthestModules = Variant.MaxFarthest;
		Fixture.Rules.MinNodesPerFloor = Variant.MinNodes;
		Fixture.Rules.MaxNodesPerFloor = Variant.MaxNodes;
		Fixture.Rules.MinFarthestModules = 1;
		Fixture.Rules.MinTotalModules = 1;
		Fixture.Rules.MaxTotalModules = 100000;
		int32 Successes = 0;
		int64 SumFarthest = 0;
		int64 SumTotal = 0;
		int64 SumClassrooms = 0;
		int32 MinTotal = MAX_int32;
		int32 MaxTotal = 0;
		for (int32 Seed = 0; Seed < Seeds; ++Seed)
		{
			const FLayout Layout = Fixture.Generate(Seed);
			if (!Layout.bSuccess)
			{
				continue;
			}
			++Successes;
			SumFarthest += Layout.FarthestModules;
			SumTotal += Layout.TotalModules;
			SumClassrooms += Layout.ClassroomCount;
			MinTotal = FMath::Min(MinTotal, Layout.TotalModules);
			MaxTotal = FMath::Max(MaxTotal, Layout.TotalModules);
		}
		const float Den = static_cast<float>(FMath::Max(1, Successes));
		AddInfo(FString::Printf(TEXT("[%s] success=%d/%d farthest=%.1f total=%.1f (min %d max %d) classrooms=%.1f"),
			Variant.Name, Successes, Seeds, SumFarthest / Den, SumTotal / Den, MinTotal, MaxTotal, SumClassrooms / Den));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJihunGridGraphPlanDumpTest, "DIGETHER.Game.Dungeon.JihunLab.GridGraph.PlanDump", TestFlags)
bool FJihunGridGraphPlanDumpTest::RunTest(const FString& Parameters)
{
	// 판정 없이 Seed 0~2 층별 평면도를 Saved/GridGraphPlan.txt에 씀(기획 시안 I_School_Map 평면도와 눈으로 비교용)
	// 한 글자 = 200cm 칸. c 복도, L ㄱ자, T T자, R 교실, S 계단, M 로비, B 다리, # 겹침
	const FFixture Fixture;
	FString Text;
	for (int32 Seed = 0; Seed < 3; ++Seed)
	{
		const FLayout Layout = Fixture.Generate(Seed);
		if (!Layout.bSuccess)
		{
			continue;
		}
		FIntPoint Lo(MAX_int32, MAX_int32);
		FIntPoint Hi(MIN_int32, MIN_int32);
		for (const FPlacement& P : Layout.Placements)
		{
			FIntPoint Min;
			FIntPoint Size;
			GetPlacementRect(Fixture.Catalog[P.ModuleIndex], P, Min, Size);
			Lo = FIntPoint(FMath::Min(Lo.X, Min.X), FMath::Min(Lo.Y, Min.Y));
			Hi = FIntPoint(FMath::Max(Hi.X, Min.X + Size.X), FMath::Max(Hi.Y, Min.Y + Size.Y));
		}
		Text += FString::Printf(TEXT("##### seed %d: %d modules, farthest %d, classrooms %d (through %d), inline stairs %d\n"),
			Layout.SeedUsed, Layout.TotalModules, Layout.FarthestModules, Layout.ClassroomCount, Layout.ThroughClassroomCount, Layout.InlineStairCount);
		for (int32 Floor = 0; Floor < Layout.FloorCount; ++Floor)
		{
			const int32 W = Hi.Y - Lo.Y;
			const int32 H = Hi.X - Lo.X;
			TArray<FString> Rows;
			Rows.Init(FString::ChrN(W, TEXT(' ')), H);
			for (const FPlacement& P : Layout.Placements)
			{
				const FSpec& Spec = Fixture.Catalog[P.ModuleIndex];
				if (Spec.Role == ERole::ClassroomInterior || Floor < P.Floor || Floor >= P.Floor + Spec.FloorSpan)
				{
					continue;
				}
				const FString Id = Spec.Id.ToString();
				TCHAR Mark = TEXT('c');
				Mark = Spec.Role == ERole::Classroom ? TEXT('R') : Mark;
				Mark = Spec.Role == ERole::Stairs ? TEXT('S') : Mark;
				Mark = Spec.Role == ERole::Lobby ? TEXT('M') : Mark;
				Mark = Spec.Role == ERole::Bridge ? TEXT('B') : Mark;
				Mark = Id == TEXT("Corridor_101") ? TEXT('L') : Mark;
				Mark = Id == TEXT("Corridor_102") ? TEXT('T') : Mark;
				FIntPoint Min;
				FIntPoint Size;
				GetPlacementRect(Spec, P, Min, Size);
				for (int32 X = Min.X; X < Min.X + Size.X; ++X)
				{
					for (int32 Y = Min.Y; Y < Min.Y + Size.Y; ++Y)
					{
						TCHAR& Cell = Rows[X - Lo.X][Y - Lo.Y];
						Cell = Cell == TEXT(' ') ? Mark : TEXT('#');
					}
				}
			}
			Text += FString::Printf(TEXT("=== floor %d (row 0 = max X, column = Y)\n"), Floor);
			for (int32 Row = H - 1; Row >= 0; --Row)
			{
				Text += Rows[Row].TrimEnd() + TEXT("\n");
			}
		}
	}
	const FString Path = FPaths::ProjectSavedDir() / TEXT("GridGraphPlan.txt");
	TestTrue(TEXT("plan written"), FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	AddInfo(FString::Printf(TEXT("plan: %s"), *FPaths::ConvertRelativePathToFull(Path)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJihunGridGraphRetryLimitTest, "DIGETHER.Game.Dungeon.JihunLab.GridGraph.RetryLimit", TestFlags)
bool FJihunGridGraphRetryLimitTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	// 만족할 수 없는 조건이면 정해진 횟수 뒤 실패 보고
	Fixture.Rules.MinTotalModules = 100000;
	Fixture.Rules.MaxTotalModules = 100000;
	Fixture.Rules.MaxAttempts = 5;
	const FLayout Layout = Fixture.Generate(7);
	TestFalse(TEXT("impossible rules fail"), Layout.bSuccess);
	TestEqual(TEXT("attempt count"), Layout.Attempts, 5);
	TestEqual(TEXT("last seed"), Layout.SeedUsed, 11);
	TestFalse(TEXT("failure reason reported"), Layout.FailureReason.IsEmpty());
	TestTrue(TEXT("no partial placements"), Layout.Placements.IsEmpty());

	// 로비 없는 카탈로그는 시도 없이 실패
	TArray<FSpec> NoLobby = MakeGleyBoxCatalog();
	NoLobby.RemoveAll([](const FSpec& Spec) { return Spec.Role == ERole::Lobby; });
	const FLayout Broken = FDigetherJihunGridGraphGenerator::Generate(NoLobby, FDigetherJihunGridGraphRules(), 0);
	TestFalse(TEXT("catalog without lobby fails"), Broken.bSuccess);
	TestEqual(TEXT("no attempts without lobby"), Broken.Attempts, 0);
	return true;
}
}

#endif
