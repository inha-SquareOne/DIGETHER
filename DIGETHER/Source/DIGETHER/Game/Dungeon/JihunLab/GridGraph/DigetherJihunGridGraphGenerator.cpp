#include "DigetherJihunGridGraphGenerator.h"

#include "DigetherJihunGridGraphOccupancy.h"
#include "DigetherJihunGridGraphSkeleton.h"
#include "Math/RandomStream.h"
#include "Templates/TypeHash.h"

// 유니티 빌드에서 다른 파일과 이름이 겹치지 않게 전용 네임스페이스 사용
namespace DigetherJihunGridGraphGen
{
	using namespace DigetherJihunGridGraph;
	using FSpec = FDigetherJihunGridGraphModuleSpec;
	using FRules = FDigetherJihunGridGraphRules;
	using FDims = FDigetherJihunGridGraphSkeletonDims;
	using FSkeleton = FDigetherJihunGridGraphSkeleton;
	using EDir = EDigetherJihunGridGraphDir;
	using EEdgeType = EDigetherJihunGridGraphEdgeType;
	using ERole = EDigetherJihunGridGraphRole;

	constexpr EDir AllDirs[4] = { EDir::East, EDir::North, EDir::West, EDir::South };

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

	// 교실 배치가 복도 칸에 요구하는 교실 쪽 면
	enum class EFaceNeed : uint8
	{
		Wall,
		DoorLow,
		DoorHigh,
		AnyDoor,
		// 3칸 세트 가운데(002 복도창 또는 005 벽). 002는 여기서만 나옴
		WindowOrWall
	};

	// 교실 복도 쪽 열림 모양. Full = 3칸 세트, Low/HighOnly = 복도 한 칸 문
	enum class EFront : uint8
	{
		Full,
		LowOnly,
		HighOnly
	};

	enum class EKind : uint8
	{
		Lobby,
		Corridor,
		Junction,
		Bridge,
		Stairs,
		Classroom,
		Interior
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
	constexpr uint32 SkeletonSalt = 1;
	constexpr uint32 RoomSalt = 2;
	constexpr uint32 VariantSalt = 3;

	struct FJunctionEntry
	{
		int32 Index = INDEX_NONE;
		uint8 LocalMask = 0;
	};

	// 교실 뒤쪽(복도 반대편) 열림. 열린 끝은 뒤편 복도 칸의 문과 이어짐(기획 답변 1-1)
	enum class EBack : uint8
	{
		None,
		LowEnd,
		HighEnd
	};

	struct FClassroomEntry
	{
		int32 Index = INDEX_NONE;
		EFront Front = EFront::Full;
		EBack Back = EBack::None;
	};

	struct FCatalogIndex
	{
		int32 Lobby = INDEX_NONE;
		FDims Dims;
		TArray<int32> Corridors[static_cast<int32>(EShape::Count)][static_cast<int32>(EFace::Count)];
		TArray<FJunctionEntry> Junctions;
		TArray<int32> Bridges;
		TArray<FClassroomEntry> Classrooms;
		TArray<int32> Interiors;
		TArray<int32> StairsByMask[StairMaskCount];
		bool bHasStairs = false;
		bool bHasTee = false;

		const TArray<int32>& GetCorridors(const EShape Shape, const EFace Face) const
		{
			return Corridors[static_cast<int32>(Shape)][static_cast<int32>(Face)];
		}
	};

	struct FElem
	{
		EKind Kind = EKind::Corridor;
		int32 Floor = 0;
		// 복도 칸 최소 좌표(칸 단위)
		FIntPoint CellMin = FIntPoint::ZeroValue;
		FIntPoint RectMin = FIntPoint::ZeroValue;
		FIntPoint RectSize = FIntPoint::ZeroValue;
		int32 FloorSpan = 1;
		// 복도·다리·로비의 진행 축 방향
		EDir Along = EDir::East;
		// 교실 쪽(복도 로컬 N), 계단은 꺾임 계단 쪽
		EDir RoomSide = EDir::North;
		// 막다른 끝 복도의 막힌 쪽. Along과 같은 축
		bool bEnd = false;
		EDir ClosedSide = EDir::East;
		uint8 OpenMask = 0;
		EFaceNeed Need = EFaceNeed::Wall;
		EDir StairDir = EDir::East;
		// 복도 중간 계단(위·아래층 양쪽 문)
		bool bInlineStair = false;
		int32 ModuleIndex = INDEX_NONE;
		int32 QuarterTurns = 0;
	};

	// 한 간선(또는 짧은 복도) 안에서 끊김 없이 이어진 복도 칸. 교실 배치 단위
	struct FSegment
	{
		TArray<int32> Elems;
		EDir Along = EDir::East;
		EDir RoomSide = EDir::North;
	};

	bool IsUsable(const FSpec& Spec)
	{
		return Spec.bEnabled && Spec.Weight > 0.0f && Spec.SizeUnits.X > 0 && Spec.SizeUnits.Y > 0;
	}

	bool SideHas(const FSpec& Spec, const EDir Side, const EEdgeType Type, const int32 FloorOffset = 0)
	{
		for (const FDigetherJihunGridGraphEdge& Edge : Spec.Edges)
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

	bool AnyDoorOrEntrance(const FSpec& Spec)
	{
		for (const FDigetherJihunGridGraphEdge& Edge : Spec.Edges)
		{
			if (Edge.Type == EEdgeType::Door || Edge.Type == EEdgeType::Entrance)
			{
				return true;
			}
		}
		return false;
	}

	uint8 OpenMask(const FSpec& Spec)
	{
		uint8 Mask = 0;
		for (const EDir Dir : AllDirs)
		{
			Mask |= SideHas(Spec, Dir, EEdgeType::Open) ? DirBit(Dir) : 0;
		}
		return Mask;
	}

	uint8 RotateMask(const uint8 Mask, const int32 QuarterTurns)
	{
		uint8 Out = 0;
		for (const EDir Dir : AllDirs)
		{
			Out |= (Mask & DirBit(Dir)) != 0 ? DirBit(RotateDir(Dir, QuarterTurns)) : 0;
		}
		return Out;
	}

	// 한 면의 열림 구간을 합쳐 [시작, 끝] 목록으로
	TArray<FIntPoint> OpenSpans(const FSpec& Spec, const EDir Side)
	{
		TArray<FIntPoint> Spans;
		for (const FDigetherJihunGridGraphEdge& Edge : Spec.Edges)
		{
			if (Edge.Side == Side && Edge.Type == EEdgeType::Open && Edge.FloorOffset == 0 && Edge.EndCm > Edge.StartCm)
			{
				Spans.Add(FIntPoint(Edge.StartCm, Edge.EndCm));
			}
		}
		Spans.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X < B.X; });
		TArray<FIntPoint> Merged;
		for (const FIntPoint& Span : Spans)
		{
			if (!Merged.IsEmpty() && Span.X <= Merged.Last().Y)
			{
				Merged.Last().Y = FMath::Max(Merged.Last().Y, Span.Y);
			}
			else
			{
				Merged.Add(Span);
			}
		}
		return Merged;
	}

	EFace ClassifyNorthFace(const FSpec& Spec, const int32 UnitSizeCm)
	{
		if (SideHas(Spec, EDir::North, EEdgeType::Open) || SideHas(Spec, EDir::North, EEdgeType::Entrance))
		{
			return EFace::Count;
		}
		for (const FDigetherJihunGridGraphEdge& Edge : Spec.Edges)
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
		if (Spec.FloorSpan != 1 || Spec.SizeUnits.X != Spec.SizeUnits.Y || Spec.SizeUnits.X != Out.Dims.BlockUnits)
		{
			return;
		}
		const uint8 Mask = OpenMask(Spec);
		const uint8 Straight = DirBit(EDir::West) | DirBit(EDir::East);
		if (CountDirs(Mask) >= 2 && Mask != Straight)
		{
			// ㄱ자·T자. 문이 달린 갈림 복도는 지원하지 않음
			if (!AnyDoorOrEntrance(Spec))
			{
				Out.Junctions.Add({ Index, Mask });
				Out.bHasTee |= CountDirs(Mask) == 3;
			}
			return;
		}
		// 남쪽은 외부 면이어야 함. 103(남문)은 2단계 교실 연결 대상이라 제외
		if ((Mask & (DirBit(EDir::North) | DirBit(EDir::South))) != 0
			|| SideHas(Spec, EDir::South, EEdgeType::Door) || SideHas(Spec, EDir::South, EEdgeType::Entrance))
		{
			return;
		}
		const bool bOpenW = (Mask & DirBit(EDir::West)) != 0;
		const bool bOpenE = (Mask & DirBit(EDir::East)) != 0;
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
		if (Shape != EShape::Count && Face != EFace::Count)
		{
			Out.Corridors[static_cast<int32>(Shape)][static_cast<int32>(Face)].Add(Index);
		}
	}

	void IndexStairs(const FSpec& Spec, const int32 Index, FCatalogIndex& Out)
	{
		if (Spec.FloorSpan != 2 || (Out.bHasStairs && Spec.SizeUnits != Out.Dims.StairsUnits))
		{
			return;
		}
		int32 Mask = 0;
		Mask |= SideHas(Spec, EDir::West, EEdgeType::Door, 0) ? StairDoorW0 : 0;
		Mask |= SideHas(Spec, EDir::East, EEdgeType::Door, 0) ? StairDoorE0 : 0;
		Mask |= SideHas(Spec, EDir::West, EEdgeType::Door, 1) ? StairDoorW1 : 0;
		Mask |= SideHas(Spec, EDir::East, EEdgeType::Door, 1) ? StairDoorE1 : 0;
		Out.bHasStairs = true;
		Out.Dims.StairsUnits = Spec.SizeUnits;
		Out.StairsByMask[Mask].Add(Index);
	}

	// 복도 쪽(S) 열림 모양과 뒤쪽(N) 한쪽 끝 열림을 분류. 옆면 통로·뒤쪽 전체 열림(002, 임시 가정으로 제외)은 쓰지 않음
	void IndexClassroom(const FSpec& Spec, const int32 Index, const int32 UnitSizeCm, FCatalogIndex& Out)
	{
		const int32 BlockCm = Out.Dims.BlockUnits * UnitSizeCm;
		if (Spec.FloorSpan != 1 || Spec.SizeUnits.X != Out.Dims.BlockUnits * BlocksPerClassroom)
		{
			return;
		}
		if (SideHasPassage(Spec, EDir::East) || SideHasPassage(Spec, EDir::West)
			|| SideHas(Spec, EDir::North, EEdgeType::Door) || SideHas(Spec, EDir::North, EEdgeType::Entrance))
		{
			return;
		}
		const TArray<FIntPoint> Spans = OpenSpans(Spec, EDir::South);
		if (Spans.Num() != 1)
		{
			return;
		}
		FClassroomEntry Entry;
		Entry.Index = Index;
		const FIntPoint Span = Spans[0];
		if (Span.X <= 0 && Span.Y >= BlockCm * BlocksPerClassroom)
		{
			Entry.Front = EFront::Full;
		}
		else if (Span.Y <= BlockCm)
		{
			Entry.Front = EFront::LowOnly;
		}
		else if (Span.X >= BlockCm * (BlocksPerClassroom - 1))
		{
			Entry.Front = EFront::HighOnly;
		}
		else
		{
			return;
		}
		const TArray<FIntPoint> BackSpans = OpenSpans(Spec, EDir::North);
		if (BackSpans.Num() > 1)
		{
			return;
		}
		if (BackSpans.Num() == 1)
		{
			if (BackSpans[0].Y <= BlockCm)
			{
				Entry.Back = EBack::LowEnd;
			}
			else if (BackSpans[0].X >= BlockCm * (BlocksPerClassroom - 1))
			{
				Entry.Back = EBack::HighEnd;
			}
			else
			{
				return;
			}
		}
		Out.Classrooms.Add(Entry);
		Out.Dims.ClassroomDepthUnits = Spec.SizeUnits.Y;
	}

	bool BuildIndex(const TArray<FSpec>& Catalog, const FRules& Rules, FCatalogIndex& Out, FString& OutReason)
	{
		const int32 Unit = Rules.UnitSizeCm;
		// 복도 모듈 크기를 기준 칸으로 삼음(직선 복도 중 첫 번째)
		for (const FSpec& Spec : Catalog)
		{
			if (IsUsable(Spec) && Spec.Role == ERole::Corridor && Spec.SizeUnits.X == Spec.SizeUnits.Y
				&& OpenMask(Spec) == (DirBit(EDir::West) | DirBit(EDir::East)))
			{
				Out.Dims.BlockUnits = Spec.SizeUnits.X;
				break;
			}
		}
		const int32 B = Out.Dims.BlockUnits;

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
			case ERole::Classroom:
				IndexClassroom(Spec, Index, Unit, Out);
				break;
			case ERole::ClassroomInterior:
				if (Spec.SizeUnits.X == B * BlocksPerClassroom)
				{
					Out.Interiors.Add(Index);
				}
				break;
			case ERole::Bridge:
				if (Spec.FloorSpan == 1 && Spec.SizeUnits.Y == B && Spec.SizeUnits.X % B == 0
					&& OpenMask(Spec) == (DirBit(EDir::West) | DirBit(EDir::East))
					&& (Out.Bridges.IsEmpty() || Spec.SizeUnits.X / B == Out.Dims.BridgeBlocks))
				{
					Out.Dims.BridgeBlocks = Spec.SizeUnits.X / B;
					Out.Bridges.Add(Index);
				}
				break;
			case ERole::Lobby:
				// 앞 띠가 W·E로 열리고 폭이 복도 칸의 배수인 로비
				if (Out.Lobby == INDEX_NONE && Spec.FloorSpan == 1 && Spec.SizeUnits.X % B == 0
					&& SideHas(Spec, EDir::West, EEdgeType::Open) && SideHas(Spec, EDir::East, EEdgeType::Open))
				{
					Out.Lobby = Index;
					Out.Dims.LobbyBlocks = Spec.SizeUnits.X / B;
					Out.Dims.LobbyDepthUnits = Spec.SizeUnits.Y;
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
		if (Out.GetCorridors(EShape::Straight, EFace::Wall).IsEmpty()
			|| Out.GetCorridors(EShape::ClosedWest, EFace::Wall).IsEmpty() || Out.GetCorridors(EShape::ClosedEast, EFace::Wall).IsEmpty())
		{
			OutReason = TEXT("catalog lacks straight or dead-end corridors with a wall face");
			return false;
		}
		bool bHasCorner = false;
		for (const FJunctionEntry& Entry : Out.Junctions)
		{
			bHasCorner |= CountDirs(Entry.LocalMask) == 2;
		}
		if (!bHasCorner)
		{
			OutReason = TEXT("catalog has no corner corridor");
			return false;
		}
		if (Rules.FloorCount > 1 && (Out.StairsByMask[StairDoorW0 | StairDoorW1].IsEmpty() || Out.StairsByMask[StairDoorE0 | StairDoorE1].IsEmpty()))
		{
			OutReason = TEXT("multiple floors need stairs with doors on one side of both floors");
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

	// 로컬 N을 교실 쪽에 맞추는 회전
	int32 TurnsForRoomSide(const EDir RoomSide)
	{
		return (static_cast<int32>(RoomSide) + 3) % 4;
	}

	TArray<EFace> FacesFor(const EFaceNeed Need)
	{
		switch (Need)
		{
		case EFaceNeed::DoorLow:
			return { EFace::DoorLow };
		case EFaceNeed::DoorHigh:
			return { EFace::DoorHigh };
		case EFaceNeed::AnyDoor:
			return { EFace::DoorLow, EFace::DoorHigh };
		case EFaceNeed::WindowOrWall:
			return { EFace::Window, EFace::Wall };
		default:
			return { EFace::Wall };
		}
	}

	// 교실 로컬 X 기준 Low·Mid·High 칸에 필요한 복도 면
	void NeedsForFront(const EFront Front, EFaceNeed OutNeeds[BlocksPerClassroom])
	{
		switch (Front)
		{
		case EFront::Full:
			OutNeeds[0] = EFaceNeed::DoorLow;
			OutNeeds[1] = EFaceNeed::WindowOrWall;
			OutNeeds[2] = EFaceNeed::DoorHigh;
			break;
		case EFront::LowOnly:
			OutNeeds[0] = EFaceNeed::AnyDoor;
			OutNeeds[1] = EFaceNeed::Wall;
			OutNeeds[2] = EFaceNeed::Wall;
			break;
		default:
			OutNeeds[0] = EFaceNeed::Wall;
			OutNeeds[1] = EFaceNeed::Wall;
			OutNeeds[2] = EFaceNeed::AnyDoor;
			break;
		}
	}

	struct FPlacedEdge
	{
		int32 Placement = INDEX_NONE;
		FDigetherJihunGridGraphWorldEdge Edge;
	};

	bool IsPassage(const EEdgeType Type)
	{
		return Type == EEdgeType::Open || Type == EEdgeType::Door;
	}

	uint64 LineKey(const FDigetherJihunGridGraphWorldEdge& Edge)
	{
		const bool bAlongX = Edge.Dir == EDir::North || Edge.Dir == EDir::South;
		return (static_cast<uint64>(static_cast<uint16>(Edge.Floor)) << 40)
			| (static_cast<uint64>(bAlongX ? 1 : 0) << 32)
			| static_cast<uint64>(static_cast<uint32>(Edge.LineCm));
	}

	class FAttempt
	{
	public:
		FAttempt(const TArray<FSpec>& InCatalog, const FRules& InRules, const FCatalogIndex& InIndex, const int32 AttemptSeed)
			: Catalog(InCatalog)
			, Rules(InRules)
			, Index(InIndex)
			, SkeletonSeed(static_cast<int32>(HashCombineFast(GetTypeHash(AttemptSeed), SkeletonSalt)))
			, RoomRng(static_cast<int32>(HashCombineFast(GetTypeHash(AttemptSeed), RoomSalt)))
			, VariantRng(static_cast<int32>(HashCombineFast(GetTypeHash(AttemptSeed), VariantSalt)))
		{
		}

		bool Run(FDigetherJihunGridGraphLayout& OutLayout)
		{
			if (!BuildSkeleton(Rules, Index.Dims, SkeletonSeed, Skeleton, Reason))
			{
				return false;
			}
			if (!PlaceCorridors() || !PlaceStairs())
			{
				return false;
			}
			PlaceClassrooms();
			if (!ResolveModules())
			{
				return false;
			}

			OutLayout.FloorCount = Skeleton.Floors.Num();
			OutLayout.ClassroomCount = ClassroomCount;
			OutLayout.ThroughClassroomCount = ThroughClassroomCount;
			for (const FDigetherJihunGridGraphSkeletonStair& Stair : Skeleton.Stairs)
			{
				OutLayout.InlineStairCount += Stair.bInline ? 1 : 0;
			}
			OutLayout.Placements.Reset(Elems.Num());
			for (const FElem& Elem : Elems)
			{
				const FSpec& Spec = Catalog[Elem.ModuleIndex];
				FDigetherJihunGridGraphPlacement& Placement = OutLayout.Placements.AddDefaulted_GetRef();
				Placement.ModuleIndex = Elem.ModuleIndex;
				Placement.Floor = Elem.Floor;
				Placement.QuarterTurns = Elem.QuarterTurns;
				Placement.PivotUnits = Elem.RectMin - RotatedMinOffset(Spec.SizeUnits, Elem.QuarterTurns);
			}
			return Validate(OutLayout);
		}

		FString Reason;

	private:
		const TArray<FSpec>& Catalog;
		const FRules& Rules;
		const FCatalogIndex& Index;
		int32 SkeletonSeed = 0;
		FRandomStream RoomRng;
		FRandomStream VariantRng;
		FSkeleton Skeleton;
		FDigetherJihunGridGraphOccupancy Occupancy;
		TArray<FElem> Elems;
		TArray<FSegment> Segments;
		// (칸 X, 칸 Y, 층) → 복도 칸 요소. 교실 뒤편 복도 찾기용
		TMap<FIntVector, int32> CorridorAt;
		int32 LobbyElem = INDEX_NONE;
		int32 ClassroomCount = 0;
		int32 ThroughClassroomCount = 0;

		int32 B() const
		{
			return Index.Dims.BlockUnits;
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

		FElem MakeBlockElem(const EKind Kind, const int32 Floor, const FIntPoint& CellMin) const
		{
			FElem Elem;
			Elem.Kind = Kind;
			Elem.Floor = Floor;
			Elem.CellMin = CellMin;
			Elem.RectMin = CellMin;
			Elem.RectSize = FIntPoint(B(), B());
			return Elem;
		}

		// 교차점 칸의 교실 쪽: 이어진 간선 → 짧은 복도 → 계단 뒤쪽 순으로 따름
		EDir NodeRoomSide(const int32 Floor, const FIntPoint& Node, const uint8 Mask) const
		{
			for (const EDir Dir : AllDirs)
			{
				if ((Mask & DirBit(Dir)) == 0)
				{
					continue;
				}
				const int32 EdgeIndex = Skeleton.FindEdge(Floor, Node, Dir);
				if (EdgeIndex != INDEX_NONE)
				{
					return Skeleton.Edges[EdgeIndex].RoomSide;
				}
				for (const FDigetherJihunGridGraphSkeletonStub& Stub : Skeleton.Stubs)
				{
					if (Stub.Floor == Floor && Stub.Node == Node && Stub.Dir == Dir)
					{
						return Stub.RoomSide;
					}
				}
			}
			for (const FDigetherJihunGridGraphSkeletonStair& Stair : Skeleton.Stairs)
			{
				if (Stair.Node == Node && (Stair.LowerFloor == Floor || Stair.LowerFloor + 1 == Floor))
				{
					return Stair.BackSide;
				}
			}
			return RotateDir(AllDirs[FMath::CountTrailingZeros(static_cast<uint32>(Mask))], 1);
		}

		bool Add(const FElem& Elem, int32* OutIndex = nullptr)
		{
			const int32 NewIndex = TryAdd(Elem);
			if (NewIndex == INDEX_NONE)
			{
				Reason = FString::Printf(TEXT("overlap at (%d,%d) floor %d kind %d"), Elem.RectMin.X, Elem.RectMin.Y, Elem.Floor, static_cast<int32>(Elem.Kind));
				return false;
			}
			if (Elem.Kind == EKind::Corridor)
			{
				CorridorAt.Add(FIntVector(Elem.CellMin.X, Elem.CellMin.Y, Elem.Floor), NewIndex);
			}
			if (OutIndex)
			{
				*OutIndex = NewIndex;
			}
			return true;
		}

		bool PlaceNodes()
		{
			for (int32 Floor = 0; Floor < Skeleton.Floors.Num(); ++Floor)
			{
				const FDigetherJihunGridGraphSkeletonFloor& Data = Skeleton.Floors[Floor];
				for (const FIntPoint& Node : Data.Nodes)
				{
					uint8 Mask = Data.Masks.FindRef(Node);
					if (const EDir* StairDir = Data.StairDirs.Find(Node))
					{
						Mask |= DirBit(*StairDir);
					}
					const int32 Count = CountDirs(Mask);
					const uint8 AxisX = DirBit(EDir::East) | DirBit(EDir::West);
					const uint8 AxisY = DirBit(EDir::North) | DirBit(EDir::South);
					FElem Elem = MakeBlockElem(EKind::Corridor, Floor, Skeleton.NodeUnits(Node));
					if (Count == 0 || Count > 3)
					{
						Reason = FString::Printf(TEXT("node (%d,%d) floor %d has %d openings"), Node.X, Node.Y, Floor, Count);
						return false;
					}
					if (Count == 1 || Mask == AxisX || Mask == AxisY)
					{
						const EDir Open = AllDirs[FMath::CountTrailingZeros(static_cast<uint32>(Mask))];
						Elem.Along = Open;
						Elem.RoomSide = NodeRoomSide(Floor, Node, Mask);
						Elem.bEnd = Count == 1;
						Elem.ClosedSide = OppositeDir(Open);
					}
					else
					{
						Elem.Kind = EKind::Junction;
						Elem.OpenMask = Mask;
					}
					if (!Add(Elem))
					{
						return false;
					}
				}
			}
			return true;
		}

		bool PlaceEdges()
		{
			const FDims& Dims = Index.Dims;
			for (int32 EdgeIndex = 0; EdgeIndex < Skeleton.Edges.Num(); ++EdgeIndex)
			{
				const FDigetherJihunGridGraphSkeletonEdge& Edge = Skeleton.Edges[EdgeIndex];
				const int32 Run = Skeleton.RunBlocks(Edge.From, Edge.Dir);
				FSegment Segment;
				Segment.Along = Edge.Dir;
				Segment.RoomSide = Edge.RoomSide;
				auto CloseSegment = [this, &Segment]()
				{
					if (!Segment.Elems.IsEmpty())
					{
						Segments.Add(Segment);
						Segment.Elems.Reset();
					}
				};

				for (int32 Offset = 0; Offset < Run; ++Offset)
				{
					const FIntPoint Block = Skeleton.RunBlockMin(Edge, Offset);
					if (Offset == Edge.StairOffset)
					{
						// 중간 계단 앞뒤 복도는 이어지지 않으므로 교실 배치 단위를 끊음
						CloseSegment();
					}
					const bool bLobby = Edge.bLobby && Offset >= Skeleton.LobbyOffset && Offset < Skeleton.LobbyOffset + Dims.LobbyBlocks;
					const bool bBridge = Edge.BridgeOffset != INDEX_NONE && Offset >= Edge.BridgeOffset && Offset < Edge.BridgeOffset + Dims.BridgeBlocks;
					if (bLobby || bBridge)
					{
						CloseSegment();
						const bool bFirst = bLobby ? Offset == Skeleton.LobbyOffset : Offset == Edge.BridgeOffset;
						if (!bFirst)
						{
							continue;
						}
						FElem Elem = MakeBlockElem(bLobby ? EKind::Lobby : EKind::Bridge, Edge.Floor, Block);
						Elem.Along = Edge.Dir;
						Elem.RoomSide = Edge.RoomSide;
						if (bLobby)
						{
							LobbyRect(Skeleton, Dims, Elem.RectMin, Elem.RectSize);
						}
						else
						{
							EdgeSpanRect(Skeleton, Edge, Edge.RoomSide, B() * Offset, Dims.BridgeBlocks * B(), 0, B(), Elem.RectMin, Elem.RectSize);
						}
						if (!Add(Elem, bLobby ? &LobbyElem : nullptr))
						{
							return false;
						}
						continue;
					}
					FElem Elem = MakeBlockElem(EKind::Corridor, Edge.Floor, Block);
					Elem.Along = Edge.Dir;
					Elem.RoomSide = Edge.RoomSide;
					int32 NewIndex = INDEX_NONE;
					if (!Add(Elem, &NewIndex))
					{
						return false;
					}
					Segment.Elems.Add(NewIndex);
				}
				CloseSegment();
			}
			return true;
		}

		bool PlaceStubs()
		{
			for (const FDigetherJihunGridGraphSkeletonStub& Stub : Skeleton.Stubs)
			{
				FSegment Segment;
				Segment.Along = Stub.Dir;
				Segment.RoomSide = Stub.RoomSide;
				const FIntPoint Base = Skeleton.NodeUnits(Stub.Node);
				for (int32 Offset = 1; Offset <= Stub.Modules; ++Offset)
				{
					FElem Elem = MakeBlockElem(EKind::Corridor, Stub.Floor, Base + DirStep(Stub.Dir) * (B() * Offset));
					Elem.Along = Stub.Dir;
					Elem.RoomSide = Stub.RoomSide;
					Elem.bEnd = Offset == Stub.Modules;
					Elem.ClosedSide = Stub.Dir;
					int32 NewIndex = INDEX_NONE;
					if (!Add(Elem, &NewIndex))
					{
						return false;
					}
					Segment.Elems.Add(NewIndex);
				}
				if (!Segment.Elems.IsEmpty())
				{
					Segments.Add(Segment);
				}
			}
			return true;
		}

		bool PlaceCorridors()
		{
			return PlaceNodes() && PlaceEdges() && PlaceStubs();
		}

		bool PlaceStairs()
		{
			for (const FDigetherJihunGridGraphSkeletonStair& Stair : Skeleton.Stairs)
			{
				FElem Elem;
				Elem.Kind = EKind::Stairs;
				Elem.Floor = Stair.LowerFloor;
				Elem.FloorSpan = 2;
				Elem.CellMin = Stair.RectMin;
				Elem.StairDir = Stair.Dir;
				Elem.RoomSide = Stair.BackSide;
				Elem.bInlineStair = Stair.bInline;
				Elem.RectMin = Stair.RectMin;
				Elem.RectSize = Stair.RectSize;
				if (!Add(Elem))
				{
					return false;
				}
			}
			return true;
		}

		EShape ShapeOf(const FElem& Elem) const
		{
			if (!Elem.bEnd)
			{
				return EShape::Straight;
			}
			return RotateDir(EDir::West, TurnsForRoomSide(Elem.RoomSide)) == Elem.ClosedSide ? EShape::ClosedWest : EShape::ClosedEast;
		}

		bool HasCorridorFor(const EShape Shape, const EFaceNeed Need) const
		{
			for (const EFace Face : FacesFor(Need))
			{
				if (!Index.GetCorridors(Shape, Face).IsEmpty())
				{
					return true;
				}
			}
			return false;
		}

		// 같은 축으로 붙어 있고 교실 쪽이 같은 복도 칸을 한 줄로 묶음. 교차점의 직선 칸을 지나 간선 여러 개에 걸침
		// 중간 계단·로비·다리·꺾임에서 끊김. Along은 항상 +X 또는 +Y
		void BuildLineSegments()
		{
			Segments.Reset();
			TSet<int32> Visited;
			auto Matches = [this](const int32 ElemIndex, const bool bAxisX, const EDir RoomSide)
			{
				const FElem& Elem = Elems[ElemIndex];
				const bool bElemX = Elem.Along == EDir::East || Elem.Along == EDir::West;
				return Elem.Kind == EKind::Corridor && bElemX == bAxisX && Elem.RoomSide == RoomSide;
			};
			for (int32 Start = 0; Start < Elems.Num(); ++Start)
			{
				if (Elems[Start].Kind != EKind::Corridor || Visited.Contains(Start))
				{
					continue;
				}
				const FElem& Seed = Elems[Start];
				const bool bAxisX = Seed.Along == EDir::East || Seed.Along == EDir::West;
				const EDir Forward = bAxisX ? EDir::East : EDir::North;
				const FIntPoint Step = DirStep(Forward) * B();
				auto Neighbor = [this, &Matches, bAxisX, &Seed](const FIntPoint& Cell) -> int32
				{
					const int32* Found = CorridorAt.Find(FIntVector(Cell.X, Cell.Y, Seed.Floor));
					return Found && Matches(*Found, bAxisX, Seed.RoomSide) ? *Found : INDEX_NONE;
				};
				FIntPoint Cell = Seed.CellMin;
				while (Neighbor(Cell - Step) != INDEX_NONE && !Visited.Contains(Neighbor(Cell - Step)))
				{
					Cell -= Step;
				}
				FSegment Segment;
				Segment.Along = Forward;
				Segment.RoomSide = Seed.RoomSide;
				for (int32 Current = Neighbor(Cell); Current != INDEX_NONE && !Visited.Contains(Current); Current = Neighbor(Cell))
				{
					Visited.Add(Current);
					Segment.Elems.Add(Current);
					Cell += Step;
				}
				if (!Segment.Elems.IsEmpty())
				{
					Segments.Add(Segment);
				}
			}
		}

		// 이어진 복도 3칸마다 교실 쪽이 비면 교실 배치
		void PlaceClassrooms()
		{
			if (Index.Classrooms.IsEmpty())
			{
				return;
			}
			BuildLineSegments();
			for (const FSegment& Segment : Segments)
			{
				int32 Pos = 0;
				while (Pos + BlocksPerClassroom <= Segment.Elems.Num())
				{
					if (RoomRng.FRand() >= Rules.ClassroomChance || !TryClassroomAt(Segment, Pos))
					{
						++Pos;
						continue;
					}
					Pos += BlocksPerClassroom;
				}
			}
		}

		// 교실 뒤쪽 열린 끝 뒤에 붙을 복도 칸: 교실 쪽이 이 교실을 보고, 아직 문이 정해지지 않은 칸
		int32 FindBackCorridor(const FSegment& Segment, const int32 FrontElem, const int32 Depth) const
		{
			const FElem& Front = Elems[FrontElem];
			const FIntPoint BehindMin = Front.CellMin + DirStep(Segment.RoomSide) * (B() + Depth);
			const int32* Found = CorridorAt.Find(FIntVector(BehindMin.X, BehindMin.Y, Front.Floor));
			if (!Found)
			{
				return INDEX_NONE;
			}
			const FElem& Behind = Elems[*Found];
			const bool bFacing = Behind.RoomSide == OppositeDir(Segment.RoomSide);
			if (!bFacing || Behind.Need != EFaceNeed::Wall || !HasCorridorFor(ShapeOf(Behind), EFaceNeed::AnyDoor))
			{
				return INDEX_NONE;
			}
			return *Found;
		}

		bool TryClassroomAt(const FSegment& Segment, const int32 Pos)
		{
			const int32 Turns = TurnsForRoomSide(Segment.RoomSide);
			const bool bLocalXAlongRun = RotateDir(EDir::East, Turns) == Segment.Along;
			// 교실 로컬 Low·High 끝에 해당하는 복도 칸
			const int32 LowElem = Segment.Elems[Pos + (bLocalXAlongRun ? 0 : BlocksPerClassroom - 1)];
			const int32 HighElem = Segment.Elems[Pos + (bLocalXAlongRun ? BlocksPerClassroom - 1 : 0)];
			const int32 Depth = Index.Dims.ClassroomDepthUnits;
			const int32 BackLow = FindBackCorridor(Segment, LowElem, Depth);
			const int32 BackHigh = FindBackCorridor(Segment, HighElem, Depth);

			TArray<int32> Fitting;
			TMap<int32, FClassroomEntry> EntryByModule;
			for (const FClassroomEntry& Entry : Index.Classrooms)
			{
				EFaceNeed Needs[BlocksPerClassroom];
				NeedsForFront(Entry.Front, Needs);
				bool bFits = Catalog[Entry.Index].SizeUnits.Y == Depth;
				for (int32 Offset = 0; Offset < BlocksPerClassroom && bFits; ++Offset)
				{
					const int32 Local = bLocalXAlongRun ? Offset : BlocksPerClassroom - 1 - Offset;
					bFits &= HasCorridorFor(ShapeOf(Elems[Segment.Elems[Pos + Offset]]), Needs[Local]);
				}
				bFits &= Entry.Back == EBack::None || (Entry.Back == EBack::LowEnd ? BackLow : BackHigh) != INDEX_NONE;
				if (bFits)
				{
					Fitting.Add(Entry.Index);
					EntryByModule.Add(Entry.Index, Entry);
				}
			}
			if (Fitting.IsEmpty())
			{
				return false;
			}
			const int32 ModuleIndex = PickWeighted(Fitting, Catalog, RoomRng);
			const FClassroomEntry Picked = EntryByModule.FindChecked(ModuleIndex);
			const FElem& First = Elems[Segment.Elems[Pos]];

			FElem Room;
			Room.Kind = EKind::Classroom;
			Room.Floor = First.Floor;
			Room.CellMin = First.CellMin;
			Room.Along = Segment.Along;
			Room.RoomSide = Segment.RoomSide;
			Room.ModuleIndex = ModuleIndex;
			RunRect(BlockBackOuterCorner(First.CellMin, Segment.Along, Segment.RoomSide, B()), Segment.Along, Segment.RoomSide,
				0, B() * BlocksPerClassroom, B(), B() + Depth, Room.RectMin, Room.RectSize);
			if (TryAdd(Room) == INDEX_NONE)
			{
				return false;
			}
			++ClassroomCount;

			EFaceNeed Needs[BlocksPerClassroom];
			NeedsForFront(Picked.Front, Needs);
			for (int32 Offset = 0; Offset < BlocksPerClassroom; ++Offset)
			{
				const int32 Local = bLocalXAlongRun ? Offset : BlocksPerClassroom - 1 - Offset;
				Elems[Segment.Elems[Pos + Offset]].Need = Needs[Local];
			}
			if (Picked.Back != EBack::None)
			{
				// 뒤편 복도 칸 문으로 교실을 통로로 만듦(시안의 복도-교실-복도)
				Elems[Picked.Back == EBack::LowEnd ? BackLow : BackHigh].Need = EFaceNeed::AnyDoor;
				++ThroughClassroomCount;
			}

			if (!Index.Interiors.IsEmpty() && RoomRng.FRand() < Rules.InteriorChance)
			{
				FElem Interior = Room;
				Interior.Kind = EKind::Interior;
				Interior.ModuleIndex = PickWeighted(Index.Interiors, Catalog, RoomRng);
				TryAdd(Interior, false);
			}
			return true;
		}

		bool ResolveModules()
		{
			for (FElem& Elem : Elems)
			{
				bool bResolved = true;
				switch (Elem.Kind)
				{
				case EKind::Lobby:
					Elem.ModuleIndex = Index.Lobby;
					Elem.QuarterTurns = TurnsForRoomSide(Elem.RoomSide);
					break;
				case EKind::Corridor:
					bResolved = ResolveCorridor(Elem);
					break;
				case EKind::Junction:
					bResolved = ResolveJunction(Elem);
					break;
				case EKind::Bridge:
					bResolved = ResolveBridge(Elem);
					break;
				case EKind::Stairs:
					bResolved = ResolveStairs(Elem);
					break;
				case EKind::Classroom:
				case EKind::Interior:
					Elem.QuarterTurns = TurnsForRoomSide(Elem.RoomSide);
					break;
				default:
					break;
				}
				if (!bResolved)
				{
					return false;
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
			Elem.QuarterTurns = TurnsForRoomSide(Elem.RoomSide);
			const EShape Shape = ShapeOf(Elem);
			TArray<int32> Candidates;
			for (const EFace Face : FacesFor(Elem.Need))
			{
				Candidates.Append(Index.GetCorridors(Shape, Face));
			}
			if (Candidates.IsEmpty())
			{
				Reason = FString::Printf(TEXT("no corridor for shape %d need %d"), static_cast<int32>(Shape), static_cast<int32>(Elem.Need));
				return false;
			}
			Elem.ModuleIndex = PickWeighted(Candidates, Catalog, VariantRng);
			return true;
		}

		bool ResolveJunction(FElem& Elem)
		{
			TArray<int32> Candidates;
			TMap<int32, int32> TurnsByModule;
			for (const FJunctionEntry& Entry : Index.Junctions)
			{
				for (int32 Turns = 0; Turns < 4; ++Turns)
				{
					if (RotateMask(Entry.LocalMask, Turns) == Elem.OpenMask)
					{
						Candidates.Add(Entry.Index);
						TurnsByModule.Add(Entry.Index, Turns);
						break;
					}
				}
			}
			if (Candidates.IsEmpty())
			{
				Reason = FString::Printf(TEXT("no junction corridor for mask %d"), Elem.OpenMask);
				return false;
			}
			Elem.ModuleIndex = PickWeighted(Candidates, Catalog, VariantRng);
			Elem.QuarterTurns = TurnsByModule.FindChecked(Elem.ModuleIndex);
			return true;
		}

		bool ResolveBridge(FElem& Elem)
		{
			if (Index.Bridges.IsEmpty())
			{
				Reason = TEXT("bridge requested but catalog has none");
				return false;
			}
			Elem.ModuleIndex = PickWeighted(Index.Bridges, Catalog, VariantRng);
			// 다리는 양 끝이 같아 축만 맞으면 됨
			const bool bAlongX = Elem.Along == EDir::East || Elem.Along == EDir::West;
			Elem.QuarterTurns = (bAlongX ? 0 : 1) + (VariantRng.RandBool() ? 2 : 0);
			return true;
		}

		bool ResolveStairs(FElem& Elem)
		{
			Elem.QuarterTurns = TurnsForRoomSide(Elem.RoomSide);
			const EDir LocalW = RotateDir(EDir::West, Elem.QuarterTurns);
			// 막다른 끝 계단은 교차점 칸을 보는 면에만, 복도 중간 계단은 위·아래층 양쪽에 문
			int32 Mask = LocalW == OppositeDir(Elem.StairDir) ? (StairDoorW0 | StairDoorW1) : (StairDoorE0 | StairDoorE1);
			if (Elem.bInlineStair)
			{
				Mask = StairDoorW0 | StairDoorE0 | StairDoorW1 | StairDoorE1;
			}
			if (Index.StairsByMask[Mask].IsEmpty())
			{
				Reason = FString::Printf(TEXT("no stairs with door mask %d"), Mask);
				return false;
			}
			Elem.ModuleIndex = PickWeighted(Index.StairsByMask[Mask], Catalog, VariantRng);
			return true;
		}

		// 통로 면 짝·로비 도달·거리·모듈 수를 배치 결과만으로 다시 확인
		bool Validate(FDigetherJihunGridGraphLayout& Layout)
		{
			const int32 Count = Layout.Placements.Num();
			TArray<FPlacedEdge> Edges;
			TMap<uint64, TArray<int32>> ByLine;
			for (int32 Placement = 0; Placement < Count; ++Placement)
			{
				TArray<FDigetherJihunGridGraphWorldEdge> World;
				GetWorldEdges(Catalog[Layout.Placements[Placement].ModuleIndex], Layout.Placements[Placement], Rules.UnitSizeCm, World);
				for (const FDigetherJihunGridGraphWorldEdge& Edge : World)
				{
					if (IsPassage(Edge.Type))
					{
						ByLine.FindOrAdd(LineKey(Edge)).Add(Edges.Add({ Placement, Edge }));
					}
				}
			}

			TArray<TArray<int32>> Links;
			Links.SetNum(Count);
			for (const FPlacedEdge& Edge : Edges)
			{
				bool bPartner = false;
				for (const int32 OtherIndex : ByLine.FindChecked(LineKey(Edge.Edge)))
				{
					const FPlacedEdge& Other = Edges[OtherIndex];
					if (Other.Placement != Edge.Placement && Other.Edge.Dir == OppositeDir(Edge.Edge.Dir)
						&& FMath::Min(Edge.Edge.EndCm, Other.Edge.EndCm) > FMath::Max(Edge.Edge.StartCm, Other.Edge.StartCm))
					{
						bPartner = true;
						Links[Edge.Placement].AddUnique(Other.Placement);
					}
				}
				if (!bPartner)
				{
					const FDigetherJihunGridGraphPlacement& P = Layout.Placements[Edge.Placement];
					Reason = FString::Printf(TEXT("dangling passage on %s at (%d,%d) floor %d"),
						*Catalog[P.ModuleIndex].Id.ToString(), P.PivotUnits.X, P.PivotUnits.Y, Edge.Edge.Floor);
					return false;
				}
			}

			TArray<int32> Depth;
			Depth.Init(INDEX_NONE, Count);
			Depth[LobbyElem] = 0;
			TArray<int32> Queue = { LobbyElem };
			for (int32 Head = 0; Head < Queue.Num(); ++Head)
			{
				const int32 Current = Queue[Head];
				for (const int32 Next : Links[Current])
				{
					if (Depth[Next] == INDEX_NONE)
					{
						Depth[Next] = Depth[Current] + 1;
						Queue.Add(Next);
					}
				}
			}
			int32 Farthest = 0;
			int32 Total = 0;
			TArray<int32> PerFloor;
			PerFloor.Init(0, Skeleton.Floors.Num());
			for (int32 Placement = 0; Placement < Count; ++Placement)
			{
				if (Catalog[Layout.Placements[Placement].ModuleIndex].Role == ERole::ClassroomInterior)
				{
					continue;
				}
				++Total;
				++PerFloor[Layout.Placements[Placement].Floor];
				if (Depth[Placement] == INDEX_NONE)
				{
					Reason = FString::Printf(TEXT("unreachable %s"), *Catalog[Layout.Placements[Placement].ModuleIndex].Id.ToString());
					return false;
				}
				Farthest = FMath::Max(Farthest, Depth[Placement]);
			}
			Layout.FarthestModules = Farthest;
			Layout.TotalModules = Total;
			for (int32 Floor = 0; Floor < PerFloor.Num(); ++Floor)
			{
				if (!Skeleton.Floors[Floor].bDeadEnd && PerFloor[Floor] < Rules.MinModulesPerMainFloor)
				{
					Reason = FString::Printf(TEXT("main floor %d has only %d modules"), Floor, PerFloor[Floor]);
					return false;
				}
			}
			if (Farthest < Rules.MinFarthestModules || Farthest > Rules.MaxFarthestModules)
			{
				Reason = FString::Printf(TEXT("farthest %d outside %d~%d"), Farthest, Rules.MinFarthestModules, Rules.MaxFarthestModules);
				return false;
			}
			if (Total < Rules.MinTotalModules || Total > Rules.MaxTotalModules)
			{
				Reason = FString::Printf(TEXT("total modules %d outside %d~%d"), Total, Rules.MinTotalModules, Rules.MaxTotalModules);
				return false;
			}
			return true;
		}
	};
}

FDigetherJihunGridGraphLayout FDigetherJihunGridGraphGenerator::Generate(
	const TArray<FDigetherJihunGridGraphModuleSpec>& Catalog,
	const FDigetherJihunGridGraphRules& Rules,
	const int32 Seed)
{
	using namespace DigetherJihunGridGraphGen;

	FDigetherJihunGridGraphLayout Layout;
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

	// 카탈로그에 없는 모듈이 필요한 규칙은 끔
	FRules Effective = Rules;
	Effective.BridgeChance = Index.Bridges.IsEmpty() ? 0.0f : Rules.BridgeChance;
	Effective.MaxTeesPerFloor = Index.bHasTee ? Rules.MaxTeesPerFloor : 0;

	const int32 MaxAttempts = FMath::Max(1, Rules.MaxAttempts);
	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		const int32 AttemptSeed = Seed + Attempt;
		FAttempt Run(Catalog, Effective, Index, AttemptSeed);
		FDigetherJihunGridGraphLayout Candidate;
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
