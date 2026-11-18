#include "DigetherJihunGridGraphSkeleton.h"

#include "Math/RandomStream.h"

namespace DigetherJihunGridGraphSkel
{
	using namespace DigetherJihunGridGraph;
	using EDir = EDigetherJihunGridGraphDir;
	using FRules = FDigetherJihunGridGraphRules;
	using FDims = FDigetherJihunGridGraphSkeletonDims;
	using FSkeleton = FDigetherJihunGridGraphSkeleton;
	using FFloor = FDigetherJihunGridGraphSkeletonFloor;
	using FEdge = FDigetherJihunGridGraphSkeletonEdge;
	using FStair = FDigetherJihunGridGraphSkeletonStair;
	using FStub = FDigetherJihunGridGraphSkeletonStub;

	constexpr EDir AllDirs[4] = { EDir::East, EDir::North, EDir::West, EDir::South };

	// 계단 후보를 로비 거리로 묶는 폭(모듈 수). 같은 묶음 안에서는 무작위
	constexpr int32 StairDistanceBucket = 6;

	// 다음 층 계단을 고르기 전에 먼저 키울 교차점 수
	constexpr int32 StairPhaseNodes = 3;

	// 짝 층이 필요 없는 간선
	constexpr int32 NoPairFloor = INDEX_NONE;
	// 중간 계단 간격인데 짝 층을 쓸 수 없는 간선
	constexpr int32 InvalidPairFloor = -2;

	uint64 EdgeKey(const int32 Floor, const FIntPoint& Node, const EDir Dir)
	{
		return (static_cast<uint64>(static_cast<uint16>(Floor)) << 40)
			| (static_cast<uint64>(static_cast<uint16>(Node.X)) << 24)
			| (static_cast<uint64>(static_cast<uint16>(Node.Y)) << 8)
			| static_cast<uint64>(Dir);
	}

	bool RectsOverlap(const FIntPoint& MinA, const FIntPoint& SizeA, const FIntPoint& MinB, const FIntPoint& SizeB)
	{
		return MinA.X < MinB.X + SizeB.X && MinB.X < MinA.X + SizeA.X
			&& MinA.Y < MinB.Y + SizeB.Y && MinB.Y < MinA.Y + SizeA.Y;
	}

	template <typename T>
	void Shuffle(TArray<T>& Items, FRandomStream& Rng)
	{
		for (int32 Index = Items.Num() - 1; Index > 0; --Index)
		{
			Items.Swap(Index, Rng.RandRange(0, Index));
		}
	}

	EDir RandomPerpendicular(const EDir Dir, FRandomStream& Rng)
	{
		return RotateDir(Dir, Rng.RandBool() ? 1 : 3);
	}

	bool IsAlongX(const EDir Dir)
	{
		return Dir == EDir::East || Dir == EDir::West;
	}

	class FBuilder
	{
	public:
		FBuilder(const FRules& InRules, const FDims& InDims, const int32 Seed, FSkeleton& InOut)
			: Rules(InRules)
			, Dims(InDims)
			, Rng(Seed)
			, Out(InOut)
		{
		}

		bool Run(FString& OutReason)
		{
			if (!CheckRules(OutReason))
			{
				return false;
			}
			BuildLattice();
			SetUpFloors();

			if (!PlaceLobby(OutReason))
			{
				return false;
			}
			// 로비 층에서 위·아래로 한 층씩. 다음 층 시작점이 정해진 뒤 그 층을 처리
			if (!ProcessFloor(Out.LobbyFloor, OutReason))
			{
				return false;
			}
			for (int32 Floor = Out.LobbyFloor + 1; Floor <= HighMain; ++Floor)
			{
				if (!ProcessFloor(Floor, OutReason))
				{
					return false;
				}
			}
			for (int32 Floor = Out.LobbyFloor - 1; Floor >= LowMain; --Floor)
			{
				if (!ProcessFloor(Floor, OutReason))
				{
					return false;
				}
			}
			if (HighMain + 1 < Out.Floors.Num())
			{
				AddStub(HighMain + 1);
			}
			if (LowMain > 0)
			{
				AddStub(LowMain - 1);
			}
			// 막다른 끝 계단 칸이 막다른 끝으로 남도록 고리는 계단을 고른 뒤 추가
			for (int32 Floor = LowMain; Floor <= HighMain; ++Floor)
			{
				AddLoops(Floor);
			}
			AdjustFacingRoomSides();
			AdjustTeeRoomSides();
			return true;
		}

	private:
		const FRules& Rules;
		const FDims& Dims;
		FRandomStream Rng;
		FSkeleton& Out;
		int32 LowMain = 0;
		int32 HighMain = 0;
		int32 LobbyColumn = 0;
		FIntPoint LobbyA = FIntPoint::ZeroValue;
		FIntPoint LobbyB = FIntPoint::ZeroValue;

		int32 B() const
		{
			return Dims.BlockUnits;
		}

		bool CheckRules(FString& OutReason) const
		{
			if (Rules.FloorCount < 1 || Rules.LobbyFloor < 0 || Rules.LobbyFloor >= Rules.FloorCount)
			{
				OutReason = TEXT("lobby floor outside floor count");
				return false;
			}
			if (Rules.LatticeColumns < 2 || Rules.LatticeRows < 2)
			{
				OutReason = TEXT("lattice needs at least 2x2 nodes");
				return false;
			}
			if (Rules.MinRunModules < 1 || Rules.MinRunModules > Rules.MaxRunModules)
			{
				OutReason = TEXT("bad run length range");
				return false;
			}
			if (Rules.MinNodesPerFloor > Rules.MaxNodesPerFloor || Rules.MinStairsBetweenFloors > Rules.MaxStairsBetweenFloors
				|| Rules.DeadEndMinModules > Rules.DeadEndMaxModules)
			{
				OutReason = TEXT("min greater than max in shape rules");
				return false;
			}
			return true;
		}

		void BuildLattice()
		{
			Out.BlockUnits = B();
			Out.StairsAlongUnits = Dims.StairsUnits.X;
			LobbyColumn = Rules.LatticeColumns >= 4 ? Rng.RandRange(1, Rules.LatticeColumns - 3) : 0;
			auto MakeLines = [this](const int32 Count, const int32 ForcedGap, TArray<int32>& Lines, TArray<bool>& StairGaps, TArray<int32>& Parity)
			{
				Lines.SetNum(Count);
				StairGaps.Init(false, Count - 1);
				Parity.Init(0, Count - 1);
				Lines[0] = 0;
				for (int32 Index = 0; Index + 1 < Count; ++Index)
				{
					int32 Run = Rng.RandRange(Rules.MinRunModules, Rules.MaxRunModules);
					bool bStair = Run >= 2 && Rng.FRand() < Rules.InlineStairGapChance;
					if (!bStair && Rng.FRand() < Rules.ClassroomDepthGapChance)
					{
						Run = FMath::Max(1, Dims.ClassroomDepthUnits / B());
					}
					if (Index == ForcedGap)
					{
						// 로비 간격: 로비가 들어가고 중간 계단 없음
						Run = FMath::Max(Run, Dims.LobbyBlocks);
						bStair = false;
					}
					StairGaps[Index] = bStair;
					Parity[Index] = Rng.RandRange(0, 1);
					Lines[Index + 1] = Lines[Index] + B() + B() * Run + (bStair ? Dims.StairsUnits.X : 0);
				}
			};
			MakeLines(Rules.LatticeColumns, LobbyColumn, Out.ColumnUnits, Out.ColumnStairGap, Out.ColumnStairParity);
			MakeLines(Rules.LatticeRows, INDEX_NONE, Out.RowUnits, Out.RowStairGap, Out.RowStairParity);
		}

		void SetUpFloors()
		{
			Out.Floors.SetNum(Rules.FloorCount);
			Out.LobbyFloor = Rules.LobbyFloor;
			LowMain = 0;
			HighMain = Rules.FloorCount - 1;
			if (Rules.bDeadEndOuterFloors && Rules.FloorCount >= 3)
			{
				LowMain = Out.LobbyFloor > 0 ? 1 : 0;
				HighMain = Out.LobbyFloor < Rules.FloorCount - 1 ? Rules.FloorCount - 2 : Rules.FloorCount - 1;
			}
			for (int32 Floor = 0; Floor < Rules.FloorCount; ++Floor)
			{
				Out.Floors[Floor].bDeadEnd = Floor < LowMain || Floor > HighMain;
			}
		}

		int32 RandomStairCount()
		{
			return Rng.RandRange(Rules.MinStairsBetweenFloors, FMath::Max(Rules.MinStairsBetweenFloors, Rules.MaxStairsBetweenFloors));
		}

		int32 Degree(const int32 Floor, const FIntPoint& Node) const
		{
			const FFloor& Data = Out.Floors[Floor];
			return CountDirs(Data.Masks.FindRef(Node)) + (Data.StairDirs.Contains(Node) ? 1 : 0);
		}

		// 간선을 지날 때 늘어나는 모듈 수(도착 교차점 칸 포함). 로비·다리·계단은 한 모듈로 셈
		int32 EdgeCost(const FEdge& Edge) const
		{
			int32 Cost = Out.RunBlocks(Edge.From, Edge.Dir) + 1;
			Cost += Edge.StairOffset != INDEX_NONE ? 1 : 0;
			Cost -= Edge.bLobby ? Dims.LobbyBlocks - 1 : 0;
			Cost -= Edge.BridgeOffset != INDEX_NONE ? Dims.BridgeBlocks - 1 : 0;
			return Cost;
		}

		// 교실 한 줄 깊이만큼만 떨어진 격자 간격(평행 복도 사이에 교실이 끼는 자리)
		bool IsDepthGap(const FIntPoint& From, const EDir Dir) const
		{
			return !Out.IsStairGap(From, Dir) && Out.RunBlocks(From, Dir) * B() == Dims.ClassroomDepthUnits;
		}

		int32 EstimateCost(const FIntPoint& From, const EDir Dir) const
		{
			return Out.RunBlocks(From, Dir) + 1 + (Out.IsStairGap(From, Dir) ? 1 : 0);
		}

		// 중간 계단 간격의 간선은 위·아래 두 층이 같은 계단을 지나므로 짝 층이 필요
		int32 PairFloor(const int32 Floor, const FIntPoint& From, const EDir Dir) const
		{
			if (!Out.IsStairGap(From, Dir))
			{
				return NoPairFloor;
			}
			const int32 Parity = Out.StairParity(From, Dir);
			const bool bLower = ((Floor - Parity) % 2 + 2) % 2 == 0;
			const int32 Pair = bLower ? Floor + 1 : Floor - 1;
			if (Pair < 0 || Pair >= Out.Floors.Num() || Out.Floors[Pair].bDeadEnd || Out.Floors[Floor].bDeadEnd)
			{
				return InvalidPairFloor;
			}
			return Pair;
		}

		// 끝점 하나에 간선을 더해도 되는지(T자 수는 따로 셈)
		bool EndpointOk(const int32 Floor, const FIntPoint& Node, const EDir Dir) const
		{
			const FFloor& Data = Out.Floors[Floor];
			if (!Data.Masks.Contains(Node))
			{
				return true;
			}
			if ((Data.Masks.FindRef(Node) & DirBit(Dir)) != 0)
			{
				return false;
			}
			if (const EDir* StairDir = Data.StairDirs.Find(Node))
			{
				// 막다른 끝 계단 칸은 격자 간선 1개만(계단 앞에 T자가 오지 않게)
				return *StairDir != Dir && CountDirs(Data.Masks.FindRef(Node)) < 1;
			}
			return Degree(Floor, Node) < 3;
		}

		int32 NewTees(const int32 Floor, const FIntPoint& From, const FIntPoint& To) const
		{
			const FFloor& Data = Out.Floors[Floor];
			const int32 A = Data.Masks.Contains(From) && Degree(Floor, From) == 2 ? 1 : 0;
			const int32 C = Data.Masks.Contains(To) && Degree(Floor, To) == 2 ? 1 : 0;
			return A + C;
		}

		bool FloorAllows(const int32 Floor, const FIntPoint& From, const EDir Dir) const
		{
			const FIntPoint To = From + DirStep(Dir);
			return EndpointOk(Floor, From, Dir) && EndpointOk(Floor, To, OppositeDir(Dir))
				&& Out.Floors[Floor].TeeCount + NewTees(Floor, From, To) <= Rules.MaxTeesPerFloor;
		}

		// 계단 모양이 맞는지 빼고 간선 추가 조건 확인
		bool EdgeAllowed(const int32 Floor, const FIntPoint& From, const EDir Dir) const
		{
			const FIntPoint To = From + DirStep(Dir);
			if (!Out.IsInLattice(From) || !Out.IsInLattice(To) || !FloorAllows(Floor, From, Dir))
			{
				return false;
			}
			const int32 Pair = PairFloor(Floor, From, Dir);
			if (Pair == InvalidPairFloor)
			{
				return false;
			}
			return Pair == NoPairFloor || FloorAllows(Pair, From, Dir);
		}

		void AddNode(const int32 Floor, const FIntPoint& Node, const int32 Distance)
		{
			FFloor& Data = Out.Floors[Floor];
			if (!Data.Masks.Contains(Node))
			{
				Data.Nodes.Add(Node);
				Data.Masks.Add(Node, 0);
			}
			int32& Stored = Data.Distance.FindOrAdd(Node, Distance);
			Stored = FMath::Min(Stored, Distance);
		}

		int32 LinkEdge(const FEdge& Edge)
		{
			const FIntPoint To = Edge.To();
			FFloor& Data = Out.Floors[Edge.Floor];
			// 새 교차점도 Nodes 순회에 들어가게 등록. 거리는 호출한 쪽이 AddNode로 갱신
			AddNode(Edge.Floor, Edge.From, MAX_int32);
			AddNode(Edge.Floor, To, MAX_int32);
			const int32 FromDegree = Degree(Edge.Floor, Edge.From);
			const int32 ToDegree = Degree(Edge.Floor, To);
			const int32 Index = Out.Edges.Add(Edge);
			Out.EdgeLookup.Add(EdgeKey(Edge.Floor, Edge.From, Edge.Dir), Index);
			Out.EdgeLookup.Add(EdgeKey(Edge.Floor, To, OppositeDir(Edge.Dir)), Index);
			Data.Masks.FindOrAdd(Edge.From) |= DirBit(Edge.Dir);
			Data.Masks.FindOrAdd(To) |= DirBit(OppositeDir(Edge.Dir));
			Data.TeeCount += (FromDegree == 2 ? 1 : 0) + (ToDegree == 2 ? 1 : 0);
			return Index;
		}

		bool StairFits(const int32 LowerFloor, const FIntPoint& Min, const FIntPoint& Size) const
		{
			for (const FStair& Other : Out.Stairs)
			{
				if (FMath::Abs(Other.LowerFloor - LowerFloor) <= 1 && RectsOverlap(Min, Size, Other.RectMin, Other.RectSize))
				{
					return false;
				}
			}
			if (Out.LobbyEdge != INDEX_NONE && (Out.LobbyFloor == LowerFloor || Out.LobbyFloor == LowerFloor + 1))
			{
				FIntPoint LobbyMin;
				FIntPoint LobbySize;
				LobbyRect(Out, Dims, LobbyMin, LobbySize);
				if (RectsOverlap(Min, Size, LobbyMin, LobbySize))
				{
					return false;
				}
			}
			return true;
		}

		// 간선(짝 층 포함)을 추가. 중간 계단 자리를 못 찾으면 INDEX_NONE
		int32 AddEdge(const int32 Floor, const FIntPoint& From, const EDir Dir, const bool bLobby)
		{
			if (!EdgeAllowed(Floor, From, Dir))
			{
				return INDEX_NONE;
			}
			const int32 Run = Out.RunBlocks(From, Dir);
			FEdge Edge;
			Edge.Floor = Floor;
			Edge.From = From;
			Edge.Dir = Dir;
			Edge.bLobby = bLobby;
			Edge.RoomSide = bLobby ? EDir::North : RandomPerpendicular(Dir, Rng);

			const int32 Pair = PairFloor(Floor, From, Dir);
			FStair Stair;
			if (Pair != NoPairFloor)
			{
				// 계단 앞뒤에 복도가 한 칸 이상(기획: T자 바로 뒤 계단 피함)
				TArray<int32> Offsets;
				for (int32 Offset = 1; Offset < Run; ++Offset)
				{
					Offsets.Add(Offset);
				}
				Shuffle(Offsets, Rng);
				const EDir FirstBack = RandomPerpendicular(Dir, Rng);
				bool bFound = false;
				for (int32 OffsetIndex = 0; OffsetIndex < Offsets.Num() && !bFound; ++OffsetIndex)
				{
					for (const EDir Back : { FirstBack, OppositeDir(FirstBack) })
					{
						Edge.StairOffset = Offsets[OffsetIndex];
						FIntPoint Min;
						FIntPoint Size;
						EdgeSpanRect(Out, Edge, Back, B() * Edge.StairOffset, Dims.StairsUnits.X, 0, Dims.StairsUnits.Y, Min, Size);
						if (StairFits(FMath::Min(Floor, Pair), Min, Size))
						{
							Stair.LowerFloor = FMath::Min(Floor, Pair);
							Stair.bInline = true;
							Stair.Node = From;
							Stair.Dir = Dir;
							Stair.BackSide = Back;
							Stair.RectMin = Min;
							Stair.RectSize = Size;
							bFound = true;
							break;
						}
					}
				}
				if (!bFound)
				{
					return INDEX_NONE;
				}
			}
			else if (!bLobby && Run >= Dims.BridgeBlocks + 2 && Rng.FRand() < Rules.BridgeChance)
			{
				Edge.BridgeOffset = Rng.RandRange(1, Run - Dims.BridgeBlocks - 1);
			}

			Edge.bStairLower = Pair != NoPairFloor && Floor < Pair;
			const int32 Index = LinkEdge(Edge);
			if (Pair != NoPairFloor)
			{
				Out.Stairs.Add(Stair);
				FEdge Twin = Edge;
				Twin.Floor = Pair;
				Twin.bStairLower = Pair < Floor;
				Twin.RoomSide = RandomPerpendicular(Dir, Rng);
				// 짝 층 거리: 계단을 거쳐 같은 줄로 돌아오는 길이로 어림
				const int32 Base = Out.Floors[Floor].Distance.FindRef(From);
				AddNode(Pair, From, Base + 2 * Edge.StairOffset + 3);
				AddNode(Pair, Edge.To(), Base + Run + 3);
				LinkEdge(Twin);
			}
			return Index;
		}

		bool PlaceLobby(FString& OutReason)
		{
			const int32 Top = Rules.LatticeRows - 1;
			LobbyA = FIntPoint(LobbyColumn, Top);
			LobbyB = FIntPoint(LobbyColumn + 1, Top);
			const int32 Run = Out.RunBlocks(LobbyA, EDir::East);
			if (Run < Dims.LobbyBlocks)
			{
				OutReason = TEXT("lobby does not fit between lattice nodes");
				return false;
			}
			// 맨 위 행 북쪽은 비어 있어 로비 출입구(북)가 건물 밖을 봄
			Out.LobbyOffset = Rng.RandRange(0, Run - Dims.LobbyBlocks);
			AddNode(Out.LobbyFloor, LobbyA, Out.LobbyOffset + 1);
			AddNode(Out.LobbyFloor, LobbyB, Run - Out.LobbyOffset - Dims.LobbyBlocks + 1);
			Out.LobbyEdge = AddEdge(Out.LobbyFloor, LobbyA, EDir::East, true);
			if (Out.LobbyEdge == INDEX_NONE)
			{
				OutReason = TEXT("lobby edge rejected");
				return false;
			}
			return true;
		}

		// 시작점 근처만 조금 키워 다음 층 계단을 먼저 정하고, 남은 거리 한도 안에서 나머지를 키움
		// 끝까지 키운 뒤 계단을 고르면 위층으로 갈수록 거리 한도가 남지 않음
		bool ProcessFloor(const int32 Floor, FString& OutReason)
		{
			const int32 Target = Rng.RandRange(Rules.MinNodesPerFloor, FMath::Max(Rules.MinNodesPerFloor, Rules.MaxNodesPerFloor));
			Grow(Floor, FMath::Min(Target, Out.Floors[Floor].Nodes.Num() + StairPhaseNodes));

			const int32 MainBudget = Rules.MinRunModules + 2;
			const int32 DeadEndBudget = Rules.DeadEndMinModules + 1;
			if (Floor >= Out.LobbyFloor && Floor + 1 < Out.Floors.Num())
			{
				const bool bMain = Floor + 1 <= HighMain;
				if (!ChooseStairs(Floor, Floor + 1, bMain ? RandomStairCount() : 1, bMain ? MainBudget : DeadEndBudget, OutReason))
				{
					return false;
				}
			}
			if (Floor <= Out.LobbyFloor && Floor > 0)
			{
				const bool bMain = Floor - 1 >= LowMain;
				if (!ChooseStairs(Floor, Floor - 1, bMain ? RandomStairCount() : 1, bMain ? MainBudget : DeadEndBudget, OutReason))
				{
					return false;
				}
			}
			Grow(Floor, Target);
			return true;
		}

		void Grow(const int32 Floor, const int32 Target)
		{
			FFloor& Data = Out.Floors[Floor];
			struct FCandidate
			{
				FIntPoint Node;
				EDir Dir;
			};
			TSet<uint64> Blocked;
			TArray<FCandidate> Candidates;
			while (Data.Nodes.Num() < Target)
			{
				Candidates.Reset();
				for (const FIntPoint& Node : Data.Nodes)
				{
					for (const EDir Dir : AllDirs)
					{
						const FIntPoint Next = Node + DirStep(Dir);
						if (!Out.IsInLattice(Next) || Data.Masks.Contains(Next) || Blocked.Contains(EdgeKey(Floor, Node, Dir)))
						{
							continue;
						}
						// 교실 하나만큼 여유를 두고 가장 먼 거리 상한 안에서만 확장
						if (Data.Distance.FindRef(Node) + EstimateCost(Node, Dir) + 1 > Rules.MaxFarthestModules)
						{
							continue;
						}
						if (EdgeAllowed(Floor, Node, Dir) && (!IsDepthGap(Node, Dir) || Rng.FRand() < Rules.DepthGapEdgeChance))
						{
							Candidates.Add({ Node, Dir });
						}
					}
				}
				if (Candidates.IsEmpty())
				{
					break;
				}
				const FCandidate Pick = Candidates[Rng.RandRange(0, Candidates.Num() - 1)];
				const FIntPoint Next = Pick.Node + DirStep(Pick.Dir);
				const int32 FromDistance = Data.Distance.FindRef(Pick.Node);
				const int32 EdgeIndex = AddEdge(Floor, Pick.Node, Pick.Dir, false);
				if (EdgeIndex == INDEX_NONE)
				{
					Blocked.Add(EdgeKey(Floor, Pick.Node, Pick.Dir));
					continue;
				}
				AddNode(Floor, Next, FromDistance + EdgeCost(Out.Edges[EdgeIndex]));
			}
		}

		// 계단 붙일 막다른 끝이 없으면 로비에 가까운 교차점에서 간선 하나를 더 내어 만듦
		void AddStairSpur(const int32 Source, const int32 Target, const int32 TargetBudget, TArray<FIntPoint>& OutCandidates)
		{
			FFloor& Data = Out.Floors[Source];
			TArray<FIntPoint> Nodes = Data.Nodes;
			Nodes.StableSort([&Data](const FIntPoint& A, const FIntPoint& C) { return Data.Distance.FindRef(A) < Data.Distance.FindRef(C); });
			for (const FIntPoint& Node : Nodes)
			{
				TArray<EDir> Dirs(AllDirs, UE_ARRAY_COUNT(AllDirs));
				Shuffle(Dirs, Rng);
				for (const EDir Dir : Dirs)
				{
					const FIntPoint Next = Node + DirStep(Dir);
					const int32 Reach = Data.Distance.FindRef(Node) + EstimateCost(Node, Dir);
					// 중간 계단 간격은 짝 층 교차점이 생겨 막다른 끝이 되지 않을 수 있어 제외
					if (!Out.IsInLattice(Next) || Data.Masks.Contains(Next) || Out.Floors[Target].Masks.Contains(Next)
						|| Out.IsStairGap(Node, Dir) || Reach + 2 + TargetBudget > Rules.MaxFarthestModules)
					{
						continue;
					}
					const int32 EdgeIndex = AddEdge(Source, Node, Dir, false);
					if (EdgeIndex == INDEX_NONE)
					{
						continue;
					}
					AddNode(Source, Next, Data.Distance.FindRef(Node) + EdgeCost(Out.Edges[EdgeIndex]));
					OutCandidates.Add(Next);
					return;
				}
			}
		}

		// Source 층의 막다른 교차점에 계단을 붙이고 Target 층 같은 교차점을 시작점으로 추가
		bool ChooseStairs(const int32 Source, const int32 Target, const int32 Count, const int32 TargetBudget, FString& OutReason)
		{
			FFloor& From = Out.Floors[Source];
			TArray<FIntPoint> Candidates;
			for (const FIntPoint& Node : From.Nodes)
			{
				const bool bLobbyNode = Source == Out.LobbyFloor && (Node == LobbyA || Node == LobbyB);
				const bool bLeaf = CountDirs(From.Masks.FindRef(Node)) == 1 && !From.StairDirs.Contains(Node);
				const bool bBudget = From.Distance.FindRef(Node) + 2 + TargetBudget <= Rules.MaxFarthestModules;
				if (bLeaf && !bLobbyNode && bBudget && !Out.Floors[Target].Masks.Contains(Node))
				{
					Candidates.Add(Node);
				}
			}
			if (Candidates.IsEmpty())
			{
				AddStairSpur(Source, Target, TargetBudget, Candidates);
			}
			// 위층에 거리 여유를 남기도록 로비에서 가까운 막다른 끝부터(같은 거리대 안에서는 무작위)
			Shuffle(Candidates, Rng);
			Candidates.StableSort([&From](const FIntPoint& A, const FIntPoint& C)
			{
				return From.Distance.FindRef(A) / StairDistanceBucket < From.Distance.FindRef(C) / StairDistanceBucket;
			});

			const int32 LowerFloor = FMath::Min(Source, Target);
			int32 Placed = 0;
			for (const FIntPoint& Node : Candidates)
			{
				if (Placed >= Count)
				{
					break;
				}
				TArray<EDir> Dirs;
				for (const EDir Dir : AllDirs)
				{
					if ((From.Masks.FindRef(Node) & DirBit(Dir)) == 0)
					{
						Dirs.Add(Dir);
					}
				}
				Shuffle(Dirs, Rng);
				bool bDone = false;
				for (const EDir Dir : Dirs)
				{
					const EDir FirstBack = RandomPerpendicular(Dir, Rng);
					for (const EDir Back : { FirstBack, OppositeDir(FirstBack) })
					{
						FIntPoint Min;
						FIntPoint Size;
						StairRect(Out.NodeUnits(Node), Dir, Back, Dims, Min, Size);
						if (!StairFits(LowerFloor, Min, Size))
						{
							continue;
						}
						FStair Stair;
						Stair.LowerFloor = LowerFloor;
						Stair.Node = Node;
						Stair.Dir = Dir;
						Stair.BackSide = Back;
						Stair.RectMin = Min;
						Stair.RectSize = Size;
						Out.Stairs.Add(Stair);
						From.StairDirs.Add(Node, Dir);
						AddNode(Target, Node, From.Distance.FindRef(Node) + 2);
						Out.Floors[Target].StairDirs.Add(Node, Dir);
						bDone = true;
						break;
					}
					if (bDone)
					{
						break;
					}
				}
				Placed += bDone ? 1 : 0;
			}
			if (Placed == 0)
			{
				int32 MinDistance = MAX_int32;
				for (const FIntPoint& Node : From.Nodes)
				{
					MinDistance = FMath::Min(MinDistance, From.Distance.FindRef(Node));
				}
				OutReason = FString::Printf(TEXT("no dead-end node for stairs %d->%d (nodes %d, candidates %d, min distance %d, tees %d)"),
					Source, Target, From.Nodes.Num(), Candidates.Num(), MinDistance, From.TeeCount);
				return false;
			}
			return true;
		}

		void AddStub(const int32 Floor)
		{
			FFloor& Data = Out.Floors[Floor];
			for (const FIntPoint& Node : Data.Nodes)
			{
				const EDir StairDir = Data.StairDirs.FindChecked(Node);
				TArray<EDir> Dirs;
				for (const EDir Dir : AllDirs)
				{
					if (Dir != StairDir)
					{
						Dirs.Add(Dir);
					}
				}
				const EDir Dir = Dirs[Rng.RandRange(0, Dirs.Num() - 1)];
				const int32 Budget = Rules.MaxFarthestModules - Data.Distance.FindRef(Node) - 1;
				const int32 Modules = FMath::Clamp(Rng.RandRange(Rules.DeadEndMinModules, Rules.DeadEndMaxModules), 0, FMath::Max(0, Budget));

				FStub Stub;
				Stub.Floor = Floor;
				Stub.Node = Node;
				Stub.Dir = Dir;
				Stub.RoomSide = RandomPerpendicular(Dir, Rng);
				Stub.Modules = Modules;
				Out.Stubs.Add(Stub);
				if (Modules > 0)
				{
					Data.Masks.FindOrAdd(Node) |= DirBit(Dir);
				}
			}
		}

		void AddLoops(const int32 Floor)
		{
			const TArray<FIntPoint> Nodes = Out.Floors[Floor].Nodes;
			for (const FIntPoint& Node : Nodes)
			{
				for (const EDir Dir : { EDir::East, EDir::North })
				{
					const FIntPoint Next = Node + DirStep(Dir);
					const FFloor& Data = Out.Floors[Floor];
					if (!Data.Masks.Contains(Next) || Data.StairDirs.Contains(Node) || Data.StairDirs.Contains(Next))
					{
						continue;
					}
					const float Chance = Rules.LoopChance * (IsDepthGap(Node, Dir) ? Rules.DepthGapEdgeChance : 1.0f);
					if (EdgeAllowed(Floor, Node, Dir) && Rng.FRand() < Chance)
					{
						AddEdge(Floor, Node, Dir, false);
					}
				}
			}
		}

		// 교실 깊이만큼 떨어진 평행 간선 쌍은 교실 쪽을 서로 마주 보게 함
		// 한쪽 복도가 교실 앞, 다른 쪽이 교실 뒤 열린 곳과 이어짐(시안의 복도-교실-복도)
		void AdjustFacingRoomSides()
		{
			for (int32 Index = 0; Index < Out.Edges.Num(); ++Index)
			{
				FEdge& Edge = Out.Edges[Index];
				if (Edge.bLobby)
				{
					continue;
				}
				for (const EDir Side : { RotateDir(Edge.Dir, 1), RotateDir(Edge.Dir, 3) })
				{
					const FIntPoint Neighbor = Edge.From + DirStep(Side);
					if (!Out.IsInLattice(Neighbor) || Out.IsStairGap(Edge.From, Side)
						|| Out.RunBlocks(Edge.From, Side) * B() != Dims.ClassroomDepthUnits)
					{
						continue;
					}
					const int32 Other = Out.FindEdge(Edge.Floor, Neighbor, Edge.Dir);
					if (Other != INDEX_NONE && !Out.Edges[Other].bLobby)
					{
						Edge.RoomSide = Side;
						Out.Edges[Other].RoomSide = OppositeDir(Side);
						break;
					}
				}
			}
		}

		// T자(102)는 갈래 반대쪽이 벽이므로, 곧게 지나는 두 간선의 교실 쪽을 그 벽 쪽으로 맞춤
		void AdjustTeeRoomSides()
		{
			for (int32 Floor = 0; Floor < Out.Floors.Num(); ++Floor)
			{
				const FFloor& Data = Out.Floors[Floor];
				for (const FIntPoint& Node : Data.Nodes)
				{
					const uint8 Mask = Data.Masks.FindRef(Node);
					if (CountDirs(Mask) != 3 || Data.StairDirs.Contains(Node))
					{
						continue;
					}
					EDir Missing = EDir::East;
					for (const EDir Dir : AllDirs)
					{
						Missing = (Mask & DirBit(Dir)) == 0 ? Dir : Missing;
					}
					for (const EDir Dir : { RotateDir(Missing, 1), RotateDir(Missing, 3) })
					{
						const int32 EdgeIndex = Out.FindEdge(Floor, Node, Dir);
						if (EdgeIndex != INDEX_NONE && !Out.Edges[EdgeIndex].bLobby)
						{
							Out.Edges[EdgeIndex].RoomSide = Missing;
						}
					}
				}
			}
		}
	};
}

FIntPoint FDigetherJihunGridGraphSkeletonEdge::To() const
{
	return From + DigetherJihunGridGraph::DirStep(Dir);
}

bool FDigetherJihunGridGraphSkeleton::IsInLattice(const FIntPoint& Node) const
{
	return Node.X >= 0 && Node.Y >= 0 && Node.X < ColumnUnits.Num() && Node.Y < RowUnits.Num();
}

FIntPoint FDigetherJihunGridGraphSkeleton::NodeUnits(const FIntPoint& Node) const
{
	return FIntPoint(ColumnUnits[Node.X], RowUnits[Node.Y]);
}

bool FDigetherJihunGridGraphSkeleton::IsStairGap(const FIntPoint& From, const EDigetherJihunGridGraphDir Dir) const
{
	const FIntPoint To = From + DigetherJihunGridGraph::DirStep(Dir);
	if (!IsInLattice(From) || !IsInLattice(To))
	{
		return false;
	}
	return DigetherJihunGridGraphSkel::IsAlongX(Dir) ? ColumnStairGap[FMath::Min(From.X, To.X)] : RowStairGap[FMath::Min(From.Y, To.Y)];
}

int32 FDigetherJihunGridGraphSkeleton::StairParity(const FIntPoint& From, const EDigetherJihunGridGraphDir Dir) const
{
	const FIntPoint To = From + DigetherJihunGridGraph::DirStep(Dir);
	return DigetherJihunGridGraphSkel::IsAlongX(Dir) ? ColumnStairParity[FMath::Min(From.X, To.X)] : RowStairParity[FMath::Min(From.Y, To.Y)];
}

int32 FDigetherJihunGridGraphSkeleton::RunBlocks(const FIntPoint& From, const EDigetherJihunGridGraphDir Dir) const
{
	const FIntPoint To = From + DigetherJihunGridGraph::DirStep(Dir);
	if (!IsInLattice(From) || !IsInLattice(To))
	{
		return 0;
	}
	const FIntPoint Delta = NodeUnits(To) - NodeUnits(From);
	const int32 Gap = FMath::Abs(Delta.X) + FMath::Abs(Delta.Y) - BlockUnits - (IsStairGap(From, Dir) ? StairsAlongUnits : 0);
	return Gap / BlockUnits;
}

FIntPoint FDigetherJihunGridGraphSkeleton::RunBlockMin(const FDigetherJihunGridGraphSkeletonEdge& Edge, const int32 Index) const
{
	int32 Offset = BlockUnits * Index;
	if (Edge.StairOffset != INDEX_NONE && Index >= Edge.StairOffset)
	{
		Offset += StairsAlongUnits;
	}
	FIntPoint Min;
	FIntPoint Size;
	DigetherJihunGridGraph::EdgeSpanRect(*this, Edge, DigetherJihunGridGraph::RotateDir(Edge.Dir, 1), Offset, BlockUnits, 0, BlockUnits, Min, Size);
	return Min;
}

int32 FDigetherJihunGridGraphSkeleton::FindEdge(const int32 Floor, const FIntPoint& Node, const EDigetherJihunGridGraphDir Dir) const
{
	const int32* Found = EdgeLookup.Find(DigetherJihunGridGraphSkel::EdgeKey(Floor, Node, Dir));
	return Found ? *Found : INDEX_NONE;
}

namespace DigetherJihunGridGraph
{
	uint8 DirBit(const EDigetherJihunGridGraphDir Dir)
	{
		return static_cast<uint8>(1u << static_cast<uint32>(Dir));
	}

	int32 CountDirs(const uint8 Mask)
	{
		return FMath::CountBits(Mask & 0xF);
	}

	FIntPoint BlockBackOuterCorner(const FIntPoint& BlockMin, const EDigetherJihunGridGraphDir H, const EDigetherJihunGridGraphDir C, const int32 BlockUnits)
	{
		const FIntPoint F = DirStep(H);
		const FIntPoint S = DirStep(C);
		return FIntPoint(
			BlockMin.X + ((F.X < 0 || S.X < 0) ? BlockUnits : 0),
			BlockMin.Y + ((F.Y < 0 || S.Y < 0) ? BlockUnits : 0));
	}

	void RunRect(const FIntPoint& Origin, const EDigetherJihunGridGraphDir H, const EDigetherJihunGridGraphDir C,
		const int32 Along0, const int32 Along1, const int32 Cross0, const int32 Cross1, FIntPoint& OutMin, FIntPoint& OutSize)
	{
		const FIntPoint F = DirStep(H);
		const FIntPoint S = DirStep(C);
		const FIntPoint P0 = Origin + F * Along0 + S * Cross0;
		const FIntPoint P1 = Origin + F * Along1 + S * Cross1;
		OutMin = FIntPoint(FMath::Min(P0.X, P1.X), FMath::Min(P0.Y, P1.Y));
		OutSize = FIntPoint(FMath::Abs(P1.X - P0.X), FMath::Abs(P1.Y - P0.Y));
	}

	void StairRect(const FIntPoint& NodeMin, const EDigetherJihunGridGraphDir H, const EDigetherJihunGridGraphDir BackSide,
		const FDigetherJihunGridGraphSkeletonDims& Dims, FIntPoint& OutMin, FIntPoint& OutSize)
	{
		const FIntPoint Front = BlockBackOuterCorner(NodeMin, H, BackSide, Dims.BlockUnits) + DirStep(H) * Dims.BlockUnits;
		RunRect(Front, H, BackSide, 0, Dims.StairsUnits.X, 0, Dims.StairsUnits.Y, OutMin, OutSize);
	}

	void EdgeSpanRect(const FDigetherJihunGridGraphSkeleton& Skeleton, const FDigetherJihunGridGraphSkeletonEdge& Edge,
		const EDigetherJihunGridGraphDir Side, const int32 OffsetUnits, const int32 LengthUnits, const int32 Cross0, const int32 Cross1,
		FIntPoint& OutMin, FIntPoint& OutSize)
	{
		const int32 B = Skeleton.BlockUnits;
		const FIntPoint Origin = BlockBackOuterCorner(Skeleton.NodeUnits(Edge.From), Edge.Dir, Side, B);
		RunRect(Origin, Edge.Dir, Side, B + OffsetUnits, B + OffsetUnits + LengthUnits, Cross0, Cross1, OutMin, OutSize);
	}

	void LobbyRect(const FDigetherJihunGridGraphSkeleton& Skeleton, const FDigetherJihunGridGraphSkeletonDims& Dims,
		FIntPoint& OutMin, FIntPoint& OutSize)
	{
		const FDigetherJihunGridGraphSkeletonEdge& Edge = Skeleton.Edges[Skeleton.LobbyEdge];
		EdgeSpanRect(Skeleton, Edge, Edge.RoomSide, Dims.BlockUnits * Skeleton.LobbyOffset, Dims.LobbyBlocks * Dims.BlockUnits,
			0, Dims.LobbyDepthUnits, OutMin, OutSize);
	}

	bool BuildSkeleton(const FDigetherJihunGridGraphRules& Rules, const FDigetherJihunGridGraphSkeletonDims& Dims,
		const int32 Seed, FDigetherJihunGridGraphSkeleton& Out, FString& OutReason)
	{
		Out = FDigetherJihunGridGraphSkeleton();
		DigetherJihunGridGraphSkel::FBuilder Builder(Rules, Dims, Seed, Out);
		return Builder.Run(OutReason);
	}
}
