#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "DigetherJihunSpineEdgeGenerator.h"
#include "DigetherJihunSpineEdgeSampleCatalog.h"

// 생성기 내부 검사와 별개로, 결과 배치와 모듈 면 데이터만 보고 다시 판정
namespace DigetherJihunSpineEdgeTests
{
	using namespace DigetherJihunSpineEdge;
	using FSpec = FDigetherJihunSpineEdgeModuleSpec;
	using FLayout = FDigetherJihunSpineEdgeLayout;
	using FWorldEdge = FDigetherJihunSpineEdgeWorldEdge;
	using EType = EDigetherJihunSpineEdgeType;
	using ERole = EDigetherJihunSpineEdgeRole;

	constexpr int32 SeedCount = 200;
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	struct FPlacedEdge
	{
		int32 Placement = INDEX_NONE;
		FWorldEdge Edge;
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

	TArray<FPlacedEdge> CollectEdges(const FLayout& Layout, const TArray<FSpec>& Catalog, const int32 UnitCm)
	{
		TArray<FPlacedEdge> Out;
		for (int32 Index = 0; Index < Layout.Placements.Num(); ++Index)
		{
			TArray<FWorldEdge> Edges;
			const FDigetherJihunSpineEdgePlacement& Placement = Layout.Placements[Index];
			GetWorldEdges(Catalog[Placement.ModuleIndex], Placement, UnitCm, Edges);
			for (const FWorldEdge& Edge : Edges)
			{
				Out.Add({ Index, Edge });
			}
		}
		return Out;
	}

	// 통로 면(열림·문)이 다른 모듈의 통로 면과 마주 보는지
	bool HasPartner(const TArray<FPlacedEdge>& All, const FPlacedEdge& Edge)
	{
		for (const FPlacedEdge& Other : All)
		{
			if (Other.Placement != Edge.Placement && IsPassage(Other.Edge.Type) && Faces(Edge.Edge, Other.Edge))
			{
				return true;
			}
		}
		return false;
	}

	FString Describe(const FLayout& Layout, const TArray<FSpec>& Catalog, const int32 Index)
	{
		const FDigetherJihunSpineEdgePlacement& P = Layout.Placements[Index];
		return FString::Printf(TEXT("seed %d #%d %s pivot(%d,%d) floor %d turns %d"), Layout.SeedUsed, Index,
			*Catalog[P.ModuleIndex].Id.ToString(), P.PivotUnits.X, P.PivotUnits.Y, P.Floor, P.QuarterTurns);
	}

	struct FFixture
	{
		TArray<FSpec> Catalog = MakeGleyBoxCatalog();
		FDigetherJihunSpineEdgeRules Rules;
	};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJihunSpineEdgeTransformTest, "DIGETHER.Game.Dungeon.JihunLab.SpineEdge.Transforms", TestFlags)
bool FJihunSpineEdgeTransformTest::RunTest(const FString& Parameters)
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
		TestEqual(TEXT("east rotates by quarter turns"), RotateDir(EDigetherJihunSpineEdgeDir::East, Turns), static_cast<EDigetherJihunSpineEdgeDir>(Turns));
	}
	TestEqual(TEXT("point turn 1"), RotatePointCm(FIntPoint(800, 0), 1), FIntPoint(0, 800));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJihunSpineEdgeDeterminismTest, "DIGETHER.Game.Dungeon.JihunLab.SpineEdge.Determinism", TestFlags)
bool FJihunSpineEdgeDeterminismTest::RunTest(const FString& Parameters)
{
	const FFixture Fixture;
	bool bAnyDifferent = false;
	const FLayout Base = FDigetherJihunSpineEdgeGenerator::Generate(Fixture.Catalog, Fixture.Rules, 0);
	for (int32 Seed = 0; Seed < 20; ++Seed)
	{
		const FLayout A = FDigetherJihunSpineEdgeGenerator::Generate(Fixture.Catalog, Fixture.Rules, Seed);
		const FLayout B = FDigetherJihunSpineEdgeGenerator::Generate(Fixture.Catalog, Fixture.Rules, Seed);
		TestEqual(FString::Printf(TEXT("seed %d same seed used"), Seed), A.SeedUsed, B.SeedUsed);
		TestTrue(FString::Printf(TEXT("seed %d same placements"), Seed), A.Placements == B.Placements);
		bAnyDifferent |= !(A.Placements == Base.Placements);
	}
	TestTrue(TEXT("different seeds give different buildings"), bAnyDifferent);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJihunSpineEdgeNoOverlapTest, "DIGETHER.Game.Dungeon.JihunLab.SpineEdge.NoOverlap", TestFlags)
bool FJihunSpineEdgeNoOverlapTest::RunTest(const FString& Parameters)
{
	const FFixture Fixture;
	for (int32 Seed = 0; Seed < SeedCount; ++Seed)
	{
		const FLayout Layout = FDigetherJihunSpineEdgeGenerator::Generate(Fixture.Catalog, Fixture.Rules, Seed);
		if (!TestTrue(FString::Printf(TEXT("seed %d succeeds: %s"), Seed, *Layout.FailureReason), Layout.bSuccess))
		{
			continue;
		}
		TMap<FIntVector, int32> Owners;
		int32 TopFloor = 0;
		for (int32 Index = 0; Index < Layout.Placements.Num(); ++Index)
		{
			const FDigetherJihunSpineEdgePlacement& P = Layout.Placements[Index];
			const FSpec& Spec = Fixture.Catalog[P.ModuleIndex];
			TopFloor = FMath::Max(TopFloor, P.Floor + Spec.FloorSpan);
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
		TestEqual(FString::Printf(TEXT("seed %d floor count"), Seed), TopFloor, Layout.FloorCount);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJihunSpineEdgeConnectivityTest, "DIGETHER.Game.Dungeon.JihunLab.SpineEdge.Connectivity", TestFlags)
bool FJihunSpineEdgeConnectivityTest::RunTest(const FString& Parameters)
{
	const FFixture Fixture;
	for (int32 Seed = 0; Seed < SeedCount; ++Seed)
	{
		const FLayout Layout = FDigetherJihunSpineEdgeGenerator::Generate(Fixture.Catalog, Fixture.Rules, Seed);
		if (!Layout.bSuccess)
		{
			continue;
		}
		const TArray<FPlacedEdge> Edges = CollectEdges(Layout, Fixture.Catalog, Fixture.Rules.UnitSizeCm);

		// 열린 면·문이 허공이나 벽을 보면 안 됨
		for (const FPlacedEdge& Edge : Edges)
		{
			if (IsPassage(Edge.Edge.Type) && !HasPartner(Edges, Edge))
			{
				AddError(FString::Printf(TEXT("dangling %s side %d floor %d: %s"),
					Edge.Edge.Type == EType::Door ? TEXT("door") : TEXT("opening"),
					static_cast<int32>(Edge.Edge.Dir), Edge.Edge.Floor, *Describe(Layout, Fixture.Catalog, Edge.Placement)));
			}
		}

		// 로비에서 통로 면을 따라 모든 모듈 도달
		TArray<TArray<int32>> Links;
		Links.SetNum(Layout.Placements.Num());
		for (const FPlacedEdge& A : Edges)
		{
			for (const FPlacedEdge& B : Edges)
			{
				if (A.Placement != B.Placement && IsPassage(A.Edge.Type) && IsPassage(B.Edge.Type) && Faces(A.Edge, B.Edge))
				{
					Links[A.Placement].AddUnique(B.Placement);
				}
			}
		}
		int32 Lobby = INDEX_NONE;
		for (int32 Index = 0; Index < Layout.Placements.Num() && Lobby == INDEX_NONE; ++Index)
		{
			Lobby = Fixture.Catalog[Layout.Placements[Index].ModuleIndex].Role == ERole::Lobby ? Index : INDEX_NONE;
		}
		if (!TestNotEqual(TEXT("lobby placed"), Lobby, static_cast<int32>(INDEX_NONE)))
		{
			continue;
		}
		TSet<int32> Seen = { Lobby };
		TArray<int32> Queue = { Lobby };
		while (!Queue.IsEmpty())
		{
			const int32 Current = Queue.Pop(EAllowShrinking::No);
			for (const int32 Next : Links[Current])
			{
				if (!Seen.Contains(Next))
				{
					Seen.Add(Next);
					Queue.Add(Next);
				}
			}
		}
		for (int32 Index = 0; Index < Layout.Placements.Num(); ++Index)
		{
			const bool bInterior = Fixture.Catalog[Layout.Placements[Index].ModuleIndex].Role == ERole::ClassroomInterior;
			if (!bInterior && !Seen.Contains(Index))
			{
				AddError(FString::Printf(TEXT("unreachable: %s"), *Describe(Layout, Fixture.Catalog, Index)));
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJihunSpineEdgeClassroomRuleTest, "DIGETHER.Game.Dungeon.JihunLab.SpineEdge.ClassroomRule", TestFlags)
bool FJihunSpineEdgeClassroomRuleTest::RunTest(const FString& Parameters)
{
	const FFixture Fixture;
	// 교실 앞 문 2개가 교실 양 끝 복도 한 칸(600cm) 안에 하나씩
	const int32 EndZoneCm = Fixture.Rules.UnitSizeCm * 3;
	int32 ClassroomsChecked = 0;
	for (int32 Seed = 0; Seed < SeedCount; ++Seed)
	{
		const FLayout Layout = FDigetherJihunSpineEdgeGenerator::Generate(Fixture.Catalog, Fixture.Rules, Seed);
		if (!Layout.bSuccess)
		{
			continue;
		}
		TestTrue(FString::Printf(TEXT("seed %d min classrooms"), Seed), Layout.ClassroomCount >= Fixture.Rules.MinClassrooms);
		const TArray<FPlacedEdge> Edges = CollectEdges(Layout, Fixture.Catalog, Fixture.Rules.UnitSizeCm);
		for (const FPlacedEdge& Front : Edges)
		{
			const bool bClassroom = Fixture.Catalog[Layout.Placements[Front.Placement].ModuleIndex].Role == ERole::Classroom;
			if (!bClassroom || Front.Edge.Type != EType::Open)
			{
				continue;
			}
			++ClassroomsChecked;
			bool bLowDoor = false;
			bool bHighDoor = false;
			int32 Doors = 0;
			for (const FPlacedEdge& Other : Edges)
			{
				if (Other.Edge.Type == EType::Door && Faces(Front.Edge, Other.Edge))
				{
					++Doors;
					bLowDoor |= Other.Edge.EndCm <= Front.Edge.StartCm + EndZoneCm;
					bHighDoor |= Other.Edge.StartCm >= Front.Edge.EndCm - EndZoneCm;
				}
			}
			if (Doors != 2 || !bLowDoor || !bHighDoor)
			{
				AddError(FString::Printf(TEXT("classroom doors %d (low %d high %d): %s"), Doors, bLowDoor, bHighDoor,
					*Describe(Layout, Fixture.Catalog, Front.Placement)));
			}
		}
	}
	TestTrue(TEXT("classrooms were generated"), ClassroomsChecked > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJihunSpineEdgeStairDoorsTest, "DIGETHER.Game.Dungeon.JihunLab.SpineEdge.StairDoors", TestFlags)
bool FJihunSpineEdgeStairDoorsTest::RunTest(const FString& Parameters)
{
	const FFixture Fixture;
	int32 StairsChecked = 0;
	for (int32 Seed = 0; Seed < SeedCount; ++Seed)
	{
		const FLayout Layout = FDigetherJihunSpineEdgeGenerator::Generate(Fixture.Catalog, Fixture.Rules, Seed);
		if (!Layout.bSuccess)
		{
			continue;
		}
		const TArray<FPlacedEdge> Edges = CollectEdges(Layout, Fixture.Catalog, Fixture.Rules.UnitSizeCm);
		for (int32 Index = 0; Index < Layout.Placements.Num(); ++Index)
		{
			const FDigetherJihunSpineEdgePlacement& P = Layout.Placements[Index];
			if (Fixture.Catalog[P.ModuleIndex].Role != ERole::Stairs)
			{
				continue;
			}
			++StairsChecked;
			// 위·아래층 각각 이어진 문이 하나 이상
			for (int32 Offset = 0; Offset < 2; ++Offset)
			{
				bool bConnected = false;
				for (const FPlacedEdge& Edge : Edges)
				{
					bConnected |= Edge.Placement == Index && Edge.Edge.Type == EType::Door
						&& Edge.Edge.Floor == P.Floor + Offset && HasPartner(Edges, Edge);
				}
				if (!bConnected)
				{
					AddError(FString::Printf(TEXT("stairs floor offset %d has no connected door: %s"), Offset, *Describe(Layout, Fixture.Catalog, Index)));
				}
			}
		}
	}
	TestTrue(TEXT("stairs were generated"), StairsChecked > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJihunSpineEdgeExcludedModulesTest, "DIGETHER.Game.Dungeon.JihunLab.SpineEdge.ExcludedModules", TestFlags)
bool FJihunSpineEdgeExcludedModulesTest::RunTest(const FString& Parameters)
{
	const FFixture Fixture;
	// 1차 제외: T자·남문 복도, 연결 다리, 뒤·옆이 열린 교실 변형
	const TSet<FName> Excluded = {
		TEXT("Corridor_102"), TEXT("Corridor_103"), TEXT("Corridor_201"), TEXT("Corridor_202"),
		TEXT("classroom_002"), TEXT("classroom_003"), TEXT("classroom_004"), TEXT("classroom_005"),
		TEXT("classroom_006"), TEXT("classroom_007"), TEXT("classroom_008"), TEXT("classroom_009"), TEXT("classroom_010") };
	for (int32 Seed = 0; Seed < SeedCount; ++Seed)
	{
		const FLayout Layout = FDigetherJihunSpineEdgeGenerator::Generate(Fixture.Catalog, Fixture.Rules, Seed);
		for (int32 Index = 0; Index < Layout.Placements.Num(); ++Index)
		{
			if (Excluded.Contains(Fixture.Catalog[Layout.Placements[Index].ModuleIndex].Id))
			{
				AddError(FString::Printf(TEXT("excluded module used: %s"), *Describe(Layout, Fixture.Catalog, Index)));
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJihunSpineEdgeCoverageTest, "DIGETHER.Game.Dungeon.JihunLab.SpineEdge.Coverage", TestFlags)
bool FJihunSpineEdgeCoverageTest::RunTest(const FString& Parameters)
{
	// 다른 테스트가 단순한 건물만 보고 통과하지 않도록 꺾임·층 수·막다른 끝이 실제로 나오는지 확인
	const FFixture Fixture;
	TMap<FName, int32> Uses;
	TMap<int32, int32> FloorHistogram;
	int32 TotalModules = 0;
	int32 TotalAttempts = 0;
	for (int32 Seed = 0; Seed < SeedCount; ++Seed)
	{
		const FLayout Layout = FDigetherJihunSpineEdgeGenerator::Generate(Fixture.Catalog, Fixture.Rules, Seed);
		if (!Layout.bSuccess)
		{
			continue;
		}
		++FloorHistogram.FindOrAdd(Layout.FloorCount);
		TotalModules += Layout.Placements.Num();
		TotalAttempts += Layout.Attempts;
		for (const FDigetherJihunSpineEdgePlacement& P : Layout.Placements)
		{
			++Uses.FindOrAdd(Fixture.Catalog[P.ModuleIndex].Id);
		}
	}
	TestTrue(TEXT("corner used"), Uses.FindRef(TEXT("Corridor_101")) > 0);
	TestTrue(TEXT("dead end west-closed used"), Uses.FindRef(TEXT("Corridor_007")) + Uses.FindRef(TEXT("Corridor_008")) > 0);
	TestTrue(TEXT("dead end east-closed used"), Uses.FindRef(TEXT("Corridor_009")) + Uses.FindRef(TEXT("Corridor_010")) > 0);
	TestTrue(TEXT("interior used"), Uses.FindRef(TEXT("inside_001")) + Uses.FindRef(TEXT("inside_002")) + Uses.FindRef(TEXT("inside_003")) > 0);
	TestTrue(TEXT("2-floor buildings"), FloorHistogram.FindRef(2) > 0);
	TestTrue(TEXT("3-floor buildings"), FloorHistogram.FindRef(3) > 0);
	int32 StairVariants = 0;
	for (const TPair<FName, int32>& Use : Uses)
	{
		StairVariants += Use.Key.ToString().StartsWith(TEXT("Stairs_")) ? 1 : 0;
	}
	TestTrue(TEXT("stairs variants spread (4+)"), StairVariants >= 4);

	TArray<FName> Names;
	Uses.GetKeys(Names);
	Names.Sort(FNameLexicalLess());
	FString Summary;
	for (const FName& Name : Names)
	{
		Summary += FString::Printf(TEXT("%s=%d "), *Name.ToString(), Uses[Name]);
	}
	AddInfo(FString::Printf(TEXT("floors2=%d floors3=%d avgModules=%.1f avgAttempts=%.2f"), FloorHistogram.FindRef(2), FloorHistogram.FindRef(3),
		TotalModules / float(SeedCount), TotalAttempts / float(SeedCount)));
	AddInfo(Summary);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJihunSpineEdgeRetryLimitTest, "DIGETHER.Game.Dungeon.JihunLab.SpineEdge.RetryLimit", TestFlags)
bool FJihunSpineEdgeRetryLimitTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	// 만족할 수 없는 조건이면 정해진 횟수 뒤 실패 보고
	Fixture.Rules.MinClassrooms = 1000;
	Fixture.Rules.MaxAttempts = 5;
	const FLayout Layout = FDigetherJihunSpineEdgeGenerator::Generate(Fixture.Catalog, Fixture.Rules, 7);
	TestFalse(TEXT("impossible rules fail"), Layout.bSuccess);
	TestEqual(TEXT("attempt count"), Layout.Attempts, 5);
	TestEqual(TEXT("last seed"), Layout.SeedUsed, 11);
	TestFalse(TEXT("failure reason reported"), Layout.FailureReason.IsEmpty());
	TestTrue(TEXT("no partial placements"), Layout.Placements.IsEmpty());

	// 로비 없는 카탈로그는 시도 없이 실패
	TArray<FDigetherJihunSpineEdgeModuleSpec> NoLobby = MakeGleyBoxCatalog();
	NoLobby.RemoveAll([](const FDigetherJihunSpineEdgeModuleSpec& Spec) { return Spec.Role == ERole::Lobby; });
	const FLayout Broken = FDigetherJihunSpineEdgeGenerator::Generate(NoLobby, FDigetherJihunSpineEdgeRules(), 0);
	TestFalse(TEXT("catalog without lobby fails"), Broken.bSuccess);
	TestEqual(TEXT("no attempts without lobby"), Broken.Attempts, 0);
	return true;
}
}

#endif
