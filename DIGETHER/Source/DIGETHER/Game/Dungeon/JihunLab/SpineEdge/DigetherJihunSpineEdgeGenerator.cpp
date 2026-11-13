#include "DigetherJihunSpineEdgeGenerator.h"

#include "DigetherJihunSpineEdgeOccupancy.h"
#include "Math/RandomStream.h"
#include "Templates/TypeHash.h"

// 유니티 빌드에서 다른 파일과 이름이 겹치지 않게 전용 네임스페이스 사용
namespace DigetherJihunSpineEdgeGen
{
	using namespace DigetherJihunSpineEdge;
	using FSpec = FDigetherJihunSpineEdgeModuleSpec;
	using FRules = FDigetherJihunSpineEdgeRules;
	using EDir = EDigetherJihunSpineEdgeDir;
	using EEdgeType = EDigetherJihunSpineEdgeType;
	using ERole = EDigetherJihunSpineEdgeRole;

	// 복도 북쪽(교실 쪽) 면 모양
	enum class EFace : uint8
	{
		DoorLow,
		DoorHigh,
		Window,
		Wall,
		Count
	};

	// 복도 칸 진행 방향 모양. Closed*는 막힌 쪽이 로컬 W/E
	enum class EShape : uint8
	{
		Straight,
		ClosedWest,
		ClosedEast,
		Count
	};

	enum class EKind : uint8
	{
		Lobby,
		Straight,
		Corner,
		Stairs,
		Classroom,
		Interior
	};

	// 교실 앞 복도 3칸 중 위치. Low/High는 교실 로컬 X 기준
	enum class ESlot : uint8
	{
		None,
		Low,
		Mid,
		High
	};

	// 계단 문 비트: 아래층 W·E, 위층 W·E
	constexpr int32 StairDoorW0 = 1 << 0;
	constexpr int32 StairDoorE0 = 1 << 1;
	constexpr int32 StairDoorW1 = 1 << 2;
	constexpr int32 StairDoorE1 = 1 << 3;
	constexpr int32 StairMaskCount = 16;

	// 교실 하나가 덮는 복도 칸 수, GleyBox 실측(교실 1800 = 복도 600 × 3)
	constexpr int32 BlocksPerClassroom = 3;

	// 단계별 난수 분리용 값
	constexpr uint32 ShapeSalt = 1;
	constexpr uint32 RoomSalt = 2;
	constexpr uint32 VariantSalt = 3;

	struct FCornerEntry
	{
		int32 Index = INDEX_NONE;
		EDir OpenA = EDir::West;
		EDir OpenB = EDir::North;
	};

	struct FCatalogIndex
	{
		int32 Lobby = INDEX_NONE;
		int32 LobbyRow = 0;
		bool bLobbyWest = false;
		bool bLobbyEast = false;
		int32 BlockUnits = 0;
		TArray<int32> Corridors[static_cast<int32>(EShape::Count)][static_cast<int32>(EFace::Count)];
		TArray<FCornerEntry> Corners;
		TArray<int32> Classrooms;
		TArray<int32> Interiors;
		TArray<int32> StairsByMask[StairMaskCount];
		FIntPoint StairsSize = FIntPoint::ZeroValue;
		bool bHasStairs = false;

		const TArray<int32>& GetCorridors(const EShape Shape, const EFace Face) const
		{
			return Corridors[static_cast<int32>(Shape)][static_cast<int32>(Face)];
		}
	};

	struct FElem
	{
		EKind Kind = EKind::Straight;
		int32 Floor = 0;
		FIntPoint RectMin = FIntPoint::ZeroValue;
		FIntPoint RectSize = FIntPoint::ZeroValue;
		int32 FloorSpan = 1;
		EDir H = EDir::East;
		EDir C = EDir::North;
		// 복도 칸의 갈래 기준점(뒤쪽·바깥쪽 모서리), 교실 사각형 계산용
		FIntPoint Origin = FIntPoint::ZeroValue;
		ESlot Slot = ESlot::None;
		bool bClosedAhead = false;
		bool bLowerAhead = false;
		bool bUpperBehind = false;
		bool bUpperAhead = false;
		int32 ModuleIndex = INDEX_NONE;
		int32 QuarterTurns = 0;
	};

	// 복도 한 갈래. Origin = 다음 요소의 뒤쪽·바깥쪽 모서리 칸 좌표
	struct FArm
	{
		int32 Floor = 0;
		EDir H = EDir::East;
		EDir C = EDir::North;
		FIntPoint Origin = FIntPoint::ZeroValue;
		int32 TargetLength = 1;
		int32 StairsFrom = 0;
		int32 Corners = 0;
		bool bFromLobby = false;
		TArray<int32> Elems;
	};

	bool IsUsable(const FSpec& Spec)
	{
		return Spec.bEnabled && Spec.Weight > 0.0f && Spec.SizeUnits.X > 0 && Spec.SizeUnits.Y > 0;
	}

	bool SideHas(const FSpec& Spec, const EDir Side, const EEdgeType Type, const int32 FloorOffset = 0)
	{
		for (const FDigetherJihunSpineEdgeEdge& Edge : Spec.Edges)
		{
			if (Edge.Side == Side && Edge.Type == Type && Edge.FloorOffset == FloorOffset && Edge.EndCm > Edge.StartCm)
			{
				return true;
			}
		}
		return false;
	}

	bool SideHasPassage(const FSpec& Spec, const EDir Side)
	{
		return SideHas(Spec, Side, EEdgeType::Open) || SideHas(Spec, Side, EEdgeType::Door) || SideHas(Spec, Side, EEdgeType::Entrance);
	}

	bool SideFullyOpen(const FSpec& Spec, const EDir Side, const int32 LengthCm)
	{
		TArray<FIntPoint> Spans;
		for (const FDigetherJihunSpineEdgeEdge& Edge : Spec.Edges)
		{
			if (Edge.Side == Side && Edge.Type == EEdgeType::Open && Edge.FloorOffset == 0)
			{
				Spans.Add(FIntPoint(Edge.StartCm, Edge.EndCm));
			}
		}
		Spans.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X < B.X; });
		int32 Covered = 0;
		for (const FIntPoint& Span : Spans)
		{
			if (Span.X > Covered)
			{
				return false;
			}
			Covered = FMath::Max(Covered, Span.Y);
		}
		return Covered >= LengthCm;
	}

	EFace ClassifyNorthFace(const FSpec& Spec, const int32 UnitSizeCm)
	{
		if (SideHas(Spec, EDir::North, EEdgeType::Open) || SideHas(Spec, EDir::North, EEdgeType::Entrance))
		{
			return EFace::Count;
		}
		for (const FDigetherJihunSpineEdgeEdge& Edge : Spec.Edges)
		{
			if (Edge.Side == EDir::North && Edge.Type == EEdgeType::Door && Edge.FloorOffset == 0)
			{
				// 문 중심이 모듈 가운데보다 로컬 W쪽이면 Low
				const int32 CenterTwice = Edge.StartCm + Edge.EndCm;
				return CenterTwice < Spec.SizeUnits.X * UnitSizeCm ? EFace::DoorLow : EFace::DoorHigh;
			}
		}
		return SideHas(Spec, EDir::North, EEdgeType::Window) ? EFace::Window : EFace::Wall;
	}

	void IndexCorridor(const TArray<FSpec>& Catalog, const int32 Index, const int32 UnitSizeCm, FCatalogIndex& Out)
	{
		const FSpec& Spec = Catalog[Index];
		if (Spec.FloorSpan != 1 || Spec.SizeUnits.X != Spec.SizeUnits.Y)
		{
			return;
		}
		if (Out.BlockUnits != 0 && Spec.SizeUnits.X != Out.BlockUnits)
		{
			return;
		}

		const bool bOpenE = SideHas(Spec, EDir::East, EEdgeType::Open);
		const bool bOpenN = SideHas(Spec, EDir::North, EEdgeType::Open);
		const bool bOpenW = SideHas(Spec, EDir::West, EEdgeType::Open);
		const bool bOpenS = SideHas(Spec, EDir::South, EEdgeType::Open);
		const int32 OpenCount = int32(bOpenE) + int32(bOpenN) + int32(bOpenW) + int32(bOpenS);

		if (OpenCount == 2 && bOpenE != bOpenW)
		{
			// 인접한 두 면만 열린 ㄱ자. 다른 면에 문이 있으면 지원하지 않음
			bool bAnyDoor = false;
			for (const FDigetherJihunSpineEdgeEdge& Edge : Spec.Edges)
			{
				bAnyDoor |= Edge.Type == EEdgeType::Door || Edge.Type == EEdgeType::Entrance;
			}
			if (!bAnyDoor)
			{
				FCornerEntry Entry;
				Entry.Index = Index;
				Entry.OpenA = bOpenE ? EDir::East : EDir::West;
				Entry.OpenB = bOpenN ? EDir::North : EDir::South;
				Out.BlockUnits = Spec.SizeUnits.X;
				Out.Corners.Add(Entry);
			}
			return;
		}

		// 남쪽은 외부 면이어야 함(문·열림이 있는 모듈은 1차 제외)
		if (bOpenN || bOpenS || SideHas(Spec, EDir::South, EEdgeType::Door) || SideHas(Spec, EDir::South, EEdgeType::Entrance))
		{
			return;
		}

		EShape Shape = EShape::Count;
		if (bOpenW && bOpenE)
		{
			Shape = EShape::Straight;
		}
		else if (!bOpenW && bOpenE)
		{
			Shape = EShape::ClosedWest;
		}
		else if (bOpenW && !bOpenE)
		{
			Shape = EShape::ClosedEast;
		}
		const EFace Face = ClassifyNorthFace(Spec, UnitSizeCm);
		if (Shape == EShape::Count || Face == EFace::Count)
		{
			return;
		}
		Out.BlockUnits = Spec.SizeUnits.X;
		Out.Corridors[static_cast<int32>(Shape)][static_cast<int32>(Face)].Add(Index);
	}

	void IndexStairs(const FSpec& Spec, const int32 Index, FCatalogIndex& Out)
	{
		if (Spec.FloorSpan != 2)
		{
			return;
		}
		if (Out.bHasStairs && Spec.SizeUnits != Out.StairsSize)
		{
			return;
		}
		int32 Mask = 0;
		Mask |= SideHas(Spec, EDir::West, EEdgeType::Door, 0) ? StairDoorW0 : 0;
		Mask |= SideHas(Spec, EDir::East, EEdgeType::Door, 0) ? StairDoorE0 : 0;
		Mask |= SideHas(Spec, EDir::West, EEdgeType::Door, 1) ? StairDoorW1 : 0;
		Mask |= SideHas(Spec, EDir::East, EEdgeType::Door, 1) ? StairDoorE1 : 0;
		const bool bLowerDoor = (Mask & (StairDoorW0 | StairDoorE0)) != 0;
		const bool bUpperDoor = (Mask & (StairDoorW1 | StairDoorE1)) != 0;
		if (!bLowerDoor || !bUpperDoor)
		{
			return;
		}
		Out.bHasStairs = true;
		Out.StairsSize = Spec.SizeUnits;
		Out.StairsByMask[Mask].Add(Index);
	}

	bool BuildIndex(const TArray<FSpec>& Catalog, const FRules& Rules, FCatalogIndex& Out, FString& OutReason)
	{
		const int32 Unit = Rules.UnitSizeCm;
		for (int32 Index = 0; Index < Catalog.Num(); ++Index)
		{
			const FSpec& Spec = Catalog[Index];
			if (!IsUsable(Spec))
			{
				continue;
			}
			switch (Spec.Role)
			{
			case ERole::Corridor:
				IndexCorridor(Catalog, Index, Unit, Out);
				break;
			case ERole::Stairs:
				IndexStairs(Spec, Index, Out);
				break;
			case ERole::ClassroomInterior:
				Out.Interiors.Add(Index);
				break;
			case ERole::Classroom:
			{
				// 복도 쪽(S) 전체가 열리고 나머지 면에 통로가 없는 교실만 사용
				const bool bOpenFront = SideFullyOpen(Spec, EDir::South, Spec.SizeUnits.X * Unit);
				const bool bClosedElse = !SideHasPassage(Spec, EDir::North) && !SideHasPassage(Spec, EDir::East) && !SideHasPassage(Spec, EDir::West);
				if (Spec.FloorSpan == 1 && bOpenFront && bClosedElse)
				{
					Out.Classrooms.Add(Index);
				}
				break;
			}
			case ERole::Lobby:
				if (Out.Lobby == INDEX_NONE && Spec.FloorSpan == 1)
				{
					Out.Lobby = Index;
				}
				break;
			default:
				break;
			}
		}

		if (Out.Lobby == INDEX_NONE)
		{
			OutReason = TEXT("catalog has no lobby");
			return false;
		}
		const FSpec& Lobby = Catalog[Out.Lobby];
		for (const FDigetherJihunSpineEdgeEdge& Edge : Lobby.Edges)
		{
			if (Edge.Type == EEdgeType::Open && Edge.FloorOffset == 0 && (Edge.Side == EDir::West || Edge.Side == EDir::East))
			{
				Out.LobbyRow = Edge.StartCm / Unit;
				Out.bLobbyWest |= Edge.Side == EDir::West;
				Out.bLobbyEast |= Edge.Side == EDir::East;
			}
		}
		if (!Out.bLobbyWest && !Out.bLobbyEast)
		{
			OutReason = TEXT("lobby has no open west/east side");
			return false;
		}
		if (Out.BlockUnits <= 0 || Out.GetCorridors(EShape::Straight, EFace::Wall).IsEmpty()
			|| Out.GetCorridors(EShape::ClosedWest, EFace::Wall).IsEmpty() || Out.GetCorridors(EShape::ClosedEast, EFace::Wall).IsEmpty())
		{
			OutReason = TEXT("catalog lacks straight or dead-end corridors with a wall face");
			return false;
		}
		Out.Corners.RemoveAll([&Catalog, &Out](const FCornerEntry& Entry) { return Catalog[Entry.Index].SizeUnits.X != Out.BlockUnits; });
		Out.Classrooms.RemoveAll([&Catalog, &Out](const int32 Index) { return Catalog[Index].SizeUnits.X != Out.BlockUnits * BlocksPerClassroom; });
		Out.Interiors.RemoveAll([&Catalog, &Out](const int32 Index) { return Catalog[Index].SizeUnits.X != Out.BlockUnits * BlocksPerClassroom; });
		if (Out.bHasStairs && Out.StairsSize.Y <= Out.BlockUnits)
		{
			OutReason = TEXT("stairs must be deeper than one corridor block");
			return false;
		}
		return true;
	}

	int32 PickWeighted(const TArray<int32>& Candidates, const TArray<FSpec>& Catalog, FRandomStream& Rng)
	{
		check(!Candidates.IsEmpty());
		float Total = 0.0f;
		for (const int32 Index : Candidates)
		{
			Total += Catalog[Index].Weight;
		}
		float Roll = Rng.FRand() * Total;
		for (const int32 Index : Candidates)
		{
			Roll -= Catalog[Index].Weight;
			if (Roll < 0.0f)
			{
				return Index;
			}
		}
		return Candidates.Last();
	}

	// 로컬 N을 교실 쪽 C에 맞추는 회전
	int32 TurnsForClassroomSide(const EDir C)
	{
		return (static_cast<int32>(C) + 3) % 4;
	}

	class FAttempt
	{
	public:
		FAttempt(const TArray<FSpec>& InCatalog, const FRules& InRules, const FCatalogIndex& InIndex, const int32 AttemptSeed)
			: Catalog(InCatalog)
			, Rules(InRules)
			, Index(InIndex)
			, ShapeRng(static_cast<int32>(HashCombineFast(GetTypeHash(AttemptSeed), ShapeSalt)))
			, RoomRng(static_cast<int32>(HashCombineFast(GetTypeHash(AttemptSeed), RoomSalt)))
			, VariantRng(static_cast<int32>(HashCombineFast(GetTypeHash(AttemptSeed), VariantSalt)))
		{
		}

		bool Run(FDigetherJihunSpineEdgeLayout& OutLayout)
		{
			const int32 MinFloors = FMath::Max(1, Rules.MinFloors);
			const int32 MaxFloors = FMath::Max(MinFloors, Rules.MaxFloors);
			TargetFloors = ShapeRng.RandRange(MinFloors, MaxFloors);
			if (TargetFloors > 1 && !Index.bHasStairs)
			{
				Reason = TEXT("multiple floors requested but catalog has no stairs");
				return false;
			}

			if (!PlaceLobby())
			{
				return false;
			}
			for (int32 Floor = 0; Floor < TargetFloors; ++Floor)
			{
				if (!GenerateFloor(Floor))
				{
					return false;
				}
			}
			PlaceClassrooms();
			if (ClassroomCount < Rules.MinClassrooms)
			{
				Reason = FString::Printf(TEXT("only %d classrooms fit (min %d)"), ClassroomCount, Rules.MinClassrooms);
				return false;
			}
			if (!ResolveModules())
			{
				return false;
			}

			OutLayout.FloorCount = TargetFloors;
			OutLayout.ClassroomCount = ClassroomCount;
			OutLayout.Placements.Reset(Elems.Num());
			for (const FElem& Elem : Elems)
			{
				const FSpec& Spec = Catalog[Elem.ModuleIndex];
				FDigetherJihunSpineEdgePlacement& Placement = OutLayout.Placements.AddDefaulted_GetRef();
				Placement.ModuleIndex = Elem.ModuleIndex;
				Placement.Floor = Elem.Floor;
				Placement.QuarterTurns = Elem.QuarterTurns;
				Placement.PivotUnits = Elem.RectMin - RotatedMinOffset(Spec.SizeUnits, Elem.QuarterTurns);
			}
			return true;
		}

		FString Reason;

	private:
		const TArray<FSpec>& Catalog;
		const FRules& Rules;
		const FCatalogIndex& Index;
		FRandomStream ShapeRng;
		FRandomStream RoomRng;
		FRandomStream VariantRng;
		FDigetherJihunSpineEdgeOccupancy Occupancy;
		TArray<FElem> Elems;
		TArray<FArm> Arms;
		int32 TargetFloors = 1;
		int32 ClassroomCount = 0;

		static void RunRect(const FIntPoint& Origin, const EDir H, const EDir C,
			const int32 Along0, const int32 Along1, const int32 Cross0, const int32 Cross1,
			FIntPoint& OutMin, FIntPoint& OutSize)
		{
			const FIntPoint F = DirStep(H);
			const FIntPoint S = DirStep(C);
			const FIntPoint P0 = Origin + F * Along0 + S * Cross0;
			const FIntPoint P1 = Origin + F * Along1 + S * Cross1;
			OutMin = FIntPoint(FMath::Min(P0.X, P1.X), FMath::Min(P0.Y, P1.Y));
			OutSize = FIntPoint(FMath::Abs(P1.X - P0.X), FMath::Abs(P1.Y - P0.Y));
		}

		int32 TryAdd(const FElem& Elem, const bool bOccupies = true)
		{
			if (bOccupies && !Occupancy.IsRectFree(Elem.RectMin, Elem.RectSize, Elem.Floor, Elem.FloorSpan))
			{
				return INDEX_NONE;
			}
			const int32 NewIndex = Elems.Add(Elem);
			if (bOccupies)
			{
				Occupancy.MarkRect(Elem.RectMin, Elem.RectSize, Elem.Floor, Elem.FloorSpan, NewIndex);
			}
			return NewIndex;
		}

		FElem MakeBlock(const EKind Kind, const int32 Floor, const FIntPoint& Origin, const EDir H, const EDir C) const
		{
			FElem Elem;
			Elem.Kind = Kind;
			Elem.Floor = Floor;
			Elem.H = H;
			Elem.C = C;
			Elem.Origin = Origin;
			RunRect(Origin, H, C, 0, Index.BlockUnits, 0, Index.BlockUnits, Elem.RectMin, Elem.RectSize);
			return Elem;
		}

		int32 RandomArmLength()
		{
			const int32 MinLength = FMath::Max(1, Rules.MinArmElements);
			return ShapeRng.RandRange(MinLength, FMath::Max(MinLength, Rules.MaxArmElements));
		}

		bool PlaceLobby()
		{
			const FSpec& Lobby = Catalog[Index.Lobby];
			FElem Elem;
			Elem.Kind = EKind::Lobby;
			Elem.RectSize = Lobby.SizeUnits;
			Elem.ModuleIndex = Index.Lobby;
			TryAdd(Elem);

			auto AddLobbyArm = [this](const FIntPoint& Origin, const EDir H)
			{
				FArm Arm;
				Arm.Floor = 0;
				Arm.H = H;
				Arm.C = EDir::North;
				Arm.Origin = Origin;
				Arm.TargetLength = RandomArmLength();
				Arm.StairsFrom = ShapeRng.RandRange(0, Arm.TargetLength - 1);
				Arm.bFromLobby = true;
				Arms.Add(Arm);
			};
			if (Index.bLobbyWest)
			{
				AddLobbyArm(FIntPoint(0, Index.LobbyRow), EDir::West);
			}
			if (Index.bLobbyEast)
			{
				AddLobbyArm(FIntPoint(Lobby.SizeUnits.X, Index.LobbyRow), EDir::East);
			}
			return true;
		}

		bool GenerateFloor(const int32 Floor)
		{
			bool bWantStairs = Floor + 1 < TargetFloors;
			bool bAnyArm = false;
			// 계단이 위층 갈래를 Arms 끝에 추가하므로 현재 개수까지만 처리
			const int32 ArmCount = Arms.Num();
			for (int32 ArmIndex = 0; ArmIndex < ArmCount; ++ArmIndex)
			{
				if (Arms[ArmIndex].Floor != Floor)
				{
					continue;
				}
				bAnyArm = true;
				FArm Arm = Arms[ArmIndex];
				GenerateArm(Arm, bWantStairs);
				if (Arm.bFromLobby && Arm.Elems.IsEmpty())
				{
					Reason = TEXT("lobby opening has no corridor");
					return false;
				}
				Arms[ArmIndex] = MoveTemp(Arm);
			}
			if (!bAnyArm)
			{
				Reason = FString::Printf(TEXT("floor %d has no corridor"), Floor);
				return false;
			}
			if (bWantStairs)
			{
				Reason = FString::Printf(TEXT("no stairs fit on floor %d"), Floor);
				return false;
			}
			return true;
		}

		void GenerateArm(FArm& Arm, bool& bWantStairs)
		{
			while (Arm.Elems.Num() < Arm.TargetLength)
			{
				const int32 Step = Arm.Elems.Num();
				if (bWantStairs && Step >= Arm.StairsFrom && TryStairs(Arm))
				{
					bWantStairs = false;
					continue;
				}
				const bool bCornerRoll = Arm.Corners < Rules.MaxCornersPerArm && Step >= 1 && Step < Arm.TargetLength - 1
					&& !Index.Corners.IsEmpty() && ShapeRng.FRand() < Rules.CornerChance;
				if (bCornerRoll && TryCorner(Arm))
				{
					continue;
				}
				if (!TryStraight(Arm))
				{
					break;
				}
			}
			if (!Arm.Elems.IsEmpty())
			{
				FElem& Last = Elems[Arm.Elems.Last()];
				if (Last.Kind == EKind::Straight)
				{
					Last.bClosedAhead = true;
				}
			}
			// 계단 앞쪽 문은 뒤에 이어진 요소가 있을 때만
			for (int32 Pos = 0; Pos + 1 < Arm.Elems.Num(); ++Pos)
			{
				FElem& Elem = Elems[Arm.Elems[Pos]];
				if (Elem.Kind == EKind::Stairs)
				{
					Elem.bLowerAhead = true;
				}
			}
		}

		bool TryStraight(FArm& Arm)
		{
			const int32 NewIndex = TryAdd(MakeBlock(EKind::Straight, Arm.Floor, Arm.Origin, Arm.H, Arm.C));
			if (NewIndex == INDEX_NONE)
			{
				return false;
			}
			Arm.Elems.Add(NewIndex);
			Arm.Origin += DirStep(Arm.H) * Index.BlockUnits;
			return true;
		}

		// 교실 쪽으로 꺾음. 꺾은 뒤 첫 칸이 비어 있을 때만 놓아 열린 면이 허공을 보지 않게 함
		bool TryCorner(FArm& Arm)
		{
			const int32 B = Index.BlockUnits;
			const FElem Corner = MakeBlock(EKind::Corner, Arm.Floor, Arm.Origin, Arm.H, Arm.C);
			const FIntPoint NextOrigin = Arm.Origin + DirStep(Arm.H) * B + DirStep(Arm.C) * B;
			const EDir NextH = Arm.C;
			const EDir NextC = OppositeDir(Arm.H);
			const FElem Next = MakeBlock(EKind::Straight, Arm.Floor, NextOrigin, NextH, NextC);
			if (!Occupancy.IsRectFree(Corner.RectMin, Corner.RectSize, Arm.Floor)
				|| !Occupancy.IsRectFree(Next.RectMin, Next.RectSize, Arm.Floor))
			{
				return false;
			}
			Arm.Elems.Add(TryAdd(Corner));
			Arm.H = NextH;
			Arm.C = NextC;
			Arm.Origin = NextOrigin;
			++Arm.Corners;
			return true;
		}

		// 계단 = 복도에 끼우는 층계참 + 교실 쪽으로 튀어나온 계단실, 2개 층 점유
		bool TryStairs(FArm& Arm)
		{
			const FIntPoint Size = Index.StairsSize;
			FElem Stairs;
			Stairs.Kind = EKind::Stairs;
			Stairs.Floor = Arm.Floor;
			Stairs.FloorSpan = 2;
			Stairs.H = Arm.H;
			Stairs.C = Arm.C;
			RunRect(Arm.Origin, Arm.H, Arm.C, 0, Size.X, 0, Size.Y, Stairs.RectMin, Stairs.RectSize);
			if (!Occupancy.IsRectFree(Stairs.RectMin, Stairs.RectSize, Arm.Floor, 2))
			{
				return false;
			}

			// 위층 갈래 첫 칸을 계단과 함께 확보해야 위층 문이 허공을 보지 않음
			const int32 UpperFloor = Arm.Floor + 1;
			const FIntPoint AheadOrigin = Arm.Origin + DirStep(Arm.H) * Size.X;
			const FElem Ahead = MakeBlock(EKind::Straight, UpperFloor, AheadOrigin, Arm.H, Arm.C);
			const FElem Behind = MakeBlock(EKind::Straight, UpperFloor, Arm.Origin, OppositeDir(Arm.H), Arm.C);
			bool bAheadFree = Occupancy.IsRectFree(Ahead.RectMin, Ahead.RectSize, UpperFloor);
			bool bBehindFree = Occupancy.IsRectFree(Behind.RectMin, Behind.RectSize, UpperFloor);
			if (!bAheadFree && !bBehindFree)
			{
				return false;
			}
			// 양쪽 다 가능해도 일부는 한쪽만 열어 계단 변형과 위층 모양을 다양하게
			if (bAheadFree && bBehindFree && ShapeRng.FRand() >= Rules.UpperBothSidesChance)
			{
				const bool bKeepAhead = ShapeRng.FRand() < 0.5f;
				bAheadFree = bKeepAhead;
				bBehindFree = !bKeepAhead;
			}

			Stairs.bUpperAhead = bAheadFree;
			Stairs.bUpperBehind = bBehindFree;
			Arm.Elems.Add(TryAdd(Stairs));
			if (bAheadFree)
			{
				AddUpperArm(Ahead, AheadOrigin, Arm.H, Arm.C);
			}
			if (bBehindFree)
			{
				AddUpperArm(Behind, Arm.Origin, OppositeDir(Arm.H), Arm.C);
			}
			Arm.Origin = AheadOrigin;
			return true;
		}

		void AddUpperArm(const FElem& FirstBlock, const FIntPoint& FirstOrigin, const EDir H, const EDir C)
		{
			FArm Upper;
			Upper.Floor = FirstBlock.Floor;
			Upper.H = H;
			Upper.C = C;
			Upper.Elems.Add(TryAdd(FirstBlock));
			Upper.Origin = FirstOrigin + DirStep(H) * Index.BlockUnits;
			Upper.TargetLength = RandomArmLength();
			Upper.StairsFrom = ShapeRng.RandRange(1, FMath::Max(1, Upper.TargetLength - 1));
			Arms.Add(Upper);
		}

		// 같은 갈래에서 연속한 직선 칸 3개마다 교실 쪽이 비면 교실 배치
		void PlaceClassrooms()
		{
			if (Index.Classrooms.IsEmpty())
			{
				return;
			}
			const int32 B = Index.BlockUnits;
			for (const FArm& Arm : Arms)
			{
				int32 Pos = 0;
				while (Pos + BlocksPerClassroom <= Arm.Elems.Num())
				{
					bool bRun = true;
					for (int32 Offset = 0; Offset < BlocksPerClassroom; ++Offset)
					{
						bRun &= Elems[Arm.Elems[Pos + Offset]].Kind == EKind::Straight;
					}
					if (!bRun || RoomRng.FRand() >= Rules.ClassroomChance)
					{
						++Pos;
						continue;
					}
					const int32 First = Arm.Elems[Pos];
					const EDir H = Elems[First].H;
					const EDir C = Elems[First].C;
					const int32 ClassroomIndex = PickWeighted(Index.Classrooms, Catalog, RoomRng);
					const int32 Depth = Catalog[ClassroomIndex].SizeUnits.Y;
					const FIntPoint Origin = Elems[First].Origin;

					FElem Room;
					Room.Kind = EKind::Classroom;
					Room.Floor = Arm.Floor;
					Room.H = H;
					Room.C = C;
					Room.ModuleIndex = ClassroomIndex;
					RunRect(Origin, H, C, 0, B * BlocksPerClassroom, B, B + Depth, Room.RectMin, Room.RectSize);
					if (TryAdd(Room) == INDEX_NONE)
					{
						++Pos;
						continue;
					}
					++ClassroomCount;

					const int32 Turns = TurnsForClassroomSide(C);
					const bool bLocalXAlongRun = RotateDir(EDir::East, Turns) == H;
					Elems[Arm.Elems[Pos]].Slot = bLocalXAlongRun ? ESlot::Low : ESlot::High;
					Elems[Arm.Elems[Pos + 1]].Slot = ESlot::Mid;
					Elems[Arm.Elems[Pos + 2]].Slot = bLocalXAlongRun ? ESlot::High : ESlot::Low;

					if (!Index.Interiors.IsEmpty() && RoomRng.FRand() < Rules.InteriorChance)
					{
						FElem Interior = Room;
						Interior.Kind = EKind::Interior;
						Interior.ModuleIndex = PickWeighted(Index.Interiors, Catalog, RoomRng);
						TryAdd(Interior, false);
					}
					Pos += BlocksPerClassroom;
				}
			}
		}

		bool ResolveModules()
		{
			for (FElem& Elem : Elems)
			{
				switch (Elem.Kind)
				{
				case EKind::Lobby:
					Elem.QuarterTurns = 0;
					break;
				case EKind::Straight:
					if (!ResolveCorridor(Elem))
					{
						return false;
					}
					break;
				case EKind::Corner:
					if (!ResolveCorner(Elem))
					{
						return false;
					}
					break;
				case EKind::Stairs:
					if (!ResolveStairs(Elem))
					{
						return false;
					}
					break;
				case EKind::Classroom:
				case EKind::Interior:
					Elem.QuarterTurns = TurnsForClassroomSide(Elem.C);
					break;
				default:
					break;
				}
				const FSpec& Spec = Catalog[Elem.ModuleIndex];
				if (!ensure(RotatedSize(Spec.SizeUnits, Elem.QuarterTurns) == Elem.RectSize))
				{
					Reason = FString::Printf(TEXT("module %s size does not match its slot"), *Spec.Id.ToString());
					return false;
				}
			}
			return true;
		}

		bool ResolveCorridor(FElem& Elem)
		{
			Elem.QuarterTurns = TurnsForClassroomSide(Elem.C);
			EShape Shape = EShape::Straight;
			if (Elem.bClosedAhead)
			{
				Shape = RotateDir(EDir::West, Elem.QuarterTurns) == Elem.H ? EShape::ClosedWest : EShape::ClosedEast;
			}
			TArray<int32> Candidates;
			switch (Elem.Slot)
			{
			case ESlot::Low:
				Candidates = Index.GetCorridors(Shape, EFace::DoorLow);
				break;
			case ESlot::High:
				Candidates = Index.GetCorridors(Shape, EFace::DoorHigh);
				break;
			case ESlot::Mid:
				Candidates = Index.GetCorridors(Shape, EFace::Window);
				Candidates.Append(Index.GetCorridors(Shape, EFace::Wall));
				break;
			default:
				Candidates = Index.GetCorridors(Shape, EFace::Wall);
				break;
			}
			if (Candidates.IsEmpty())
			{
				Reason = FString::Printf(TEXT("no corridor for shape %d slot %d"), static_cast<int32>(Shape), static_cast<int32>(Elem.Slot));
				return false;
			}
			Elem.ModuleIndex = PickWeighted(Candidates, Catalog, VariantRng);
			return true;
		}

		bool ResolveCorner(FElem& Elem)
		{
			const EDir WantA = OppositeDir(Elem.H);
			const EDir WantB = Elem.C;
			TArray<int32> Candidates;
			TMap<int32, int32> TurnsByModule;
			for (const FCornerEntry& Entry : Index.Corners)
			{
				for (int32 Turns = 0; Turns < 4; ++Turns)
				{
					const EDir A = RotateDir(Entry.OpenA, Turns);
					const EDir B = RotateDir(Entry.OpenB, Turns);
					if ((A == WantA && B == WantB) || (A == WantB && B == WantA))
					{
						Candidates.Add(Entry.Index);
						TurnsByModule.Add(Entry.Index, Turns);
						break;
					}
				}
			}
			if (Candidates.IsEmpty())
			{
				Reason = TEXT("no corner corridor fits");
				return false;
			}
			Elem.ModuleIndex = PickWeighted(Candidates, Catalog, VariantRng);
			Elem.QuarterTurns = TurnsByModule.FindChecked(Elem.ModuleIndex);
			return true;
		}

		bool ResolveStairs(FElem& Elem)
		{
			Elem.QuarterTurns = TurnsForClassroomSide(Elem.C);
			const EDir LocalW = RotateDir(EDir::West, Elem.QuarterTurns);
			const EDir LocalE = RotateDir(EDir::East, Elem.QuarterTurns);
			const EDir Behind = OppositeDir(Elem.H);
			const EDir Ahead = Elem.H;
			auto HasDoor = [&](const EDir Side, const bool bBehind, const bool bAhead)
			{
				return (bBehind && Side == Behind) || (bAhead && Side == Ahead);
			};
			int32 Mask = 0;
			Mask |= HasDoor(LocalW, true, Elem.bLowerAhead) ? StairDoorW0 : 0;
			Mask |= HasDoor(LocalE, true, Elem.bLowerAhead) ? StairDoorE0 : 0;
			Mask |= HasDoor(LocalW, Elem.bUpperBehind, Elem.bUpperAhead) ? StairDoorW1 : 0;
			Mask |= HasDoor(LocalE, Elem.bUpperBehind, Elem.bUpperAhead) ? StairDoorE1 : 0;
			if (Index.StairsByMask[Mask].IsEmpty())
			{
				Reason = FString::Printf(TEXT("no stairs with door mask %d"), Mask);
				return false;
			}
			Elem.ModuleIndex = PickWeighted(Index.StairsByMask[Mask], Catalog, VariantRng);
			return true;
		}
	};
}

FDigetherJihunSpineEdgeLayout FDigetherJihunSpineEdgeGenerator::Generate(
	const TArray<FDigetherJihunSpineEdgeModuleSpec>& Catalog,
	const FDigetherJihunSpineEdgeRules& Rules,
	const int32 Seed)
{
	using namespace DigetherJihunSpineEdgeGen;

	FDigetherJihunSpineEdgeLayout Layout;
	Layout.SeedUsed = Seed;

	FCatalogIndex Index;
	if (Rules.UnitSizeCm <= 0 || !BuildIndex(Catalog, Rules, Index, Layout.FailureReason))
	{
		if (Layout.FailureReason.IsEmpty())
		{
			Layout.FailureReason = TEXT("invalid unit size");
		}
		return Layout;
	}

	const int32 MaxAttempts = FMath::Max(1, Rules.MaxAttempts);
	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		const int32 AttemptSeed = Seed + Attempt;
		FAttempt Run(Catalog, Rules, Index, AttemptSeed);
		FDigetherJihunSpineEdgeLayout Candidate;
		Layout.Attempts = Attempt + 1;
		Layout.SeedUsed = AttemptSeed;
		if (Run.Run(Candidate))
		{
			Candidate.bSuccess = true;
			Candidate.SeedUsed = AttemptSeed;
			Candidate.Attempts = Attempt + 1;
			return Candidate;
		}
		Layout.FailureReason = Run.Reason;
	}
	return Layout;
}

namespace DigetherJihunSpineEdge
{
	void GetPlacementRect(
		const FDigetherJihunSpineEdgeModuleSpec& Spec,
		const FDigetherJihunSpineEdgePlacement& Placement,
		FIntPoint& OutMin,
		FIntPoint& OutSize)
	{
		OutMin = Placement.PivotUnits + RotatedMinOffset(Spec.SizeUnits, Placement.QuarterTurns);
		OutSize = RotatedSize(Spec.SizeUnits, Placement.QuarterTurns);
	}

	void GetWorldEdges(
		const FDigetherJihunSpineEdgeModuleSpec& Spec,
		const FDigetherJihunSpineEdgePlacement& Placement,
		const int32 UnitSizeCm,
		TArray<FDigetherJihunSpineEdgeWorldEdge>& OutEdges)
	{
		const FIntPoint SizeCm = Spec.SizeUnits * UnitSizeCm;
		const FIntPoint PivotCm = Placement.PivotUnits * UnitSizeCm;
		for (const FDigetherJihunSpineEdgeEdge& Edge : Spec.Edges)
		{
			FIntPoint A;
			FIntPoint B;
			switch (Edge.Side)
			{
			case EDigetherJihunSpineEdgeDir::North:
				A = FIntPoint(Edge.StartCm, SizeCm.Y);
				B = FIntPoint(Edge.EndCm, SizeCm.Y);
				break;
			case EDigetherJihunSpineEdgeDir::South:
				A = FIntPoint(Edge.StartCm, 0);
				B = FIntPoint(Edge.EndCm, 0);
				break;
			case EDigetherJihunSpineEdgeDir::West:
				A = FIntPoint(0, Edge.StartCm);
				B = FIntPoint(0, Edge.EndCm);
				break;
			default:
				A = FIntPoint(SizeCm.X, Edge.StartCm);
				B = FIntPoint(SizeCm.X, Edge.EndCm);
				break;
			}
			A = PivotCm + RotatePointCm(A, Placement.QuarterTurns);
			B = PivotCm + RotatePointCm(B, Placement.QuarterTurns);

			FDigetherJihunSpineEdgeWorldEdge& World = OutEdges.AddDefaulted_GetRef();
			World.Floor = Placement.Floor + Edge.FloorOffset;
			World.Dir = RotateDir(Edge.Side, Placement.QuarterTurns);
			World.Type = Edge.Type;
			const bool bAlongX = World.Dir == EDigetherJihunSpineEdgeDir::North || World.Dir == EDigetherJihunSpineEdgeDir::South;
			World.LineCm = bAlongX ? A.Y : A.X;
			World.StartCm = bAlongX ? FMath::Min(A.X, B.X) : FMath::Min(A.Y, B.Y);
			World.EndCm = bAlongX ? FMath::Max(A.X, B.X) : FMath::Max(A.Y, B.Y);
		}
	}
}
