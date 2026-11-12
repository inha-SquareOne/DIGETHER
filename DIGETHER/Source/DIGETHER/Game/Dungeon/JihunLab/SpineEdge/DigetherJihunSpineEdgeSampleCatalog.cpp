#include "DigetherJihunSpineEdgeSampleCatalog.h"

#include "Engine/World.h"

namespace DigetherJihunSpineEdgeSample
{
	using EDir = EDigetherJihunSpineEdgeDir;
	using EType = EDigetherJihunSpineEdgeType;
	using ERole = EDigetherJihunSpineEdgeRole;
	using FSpec = FDigetherJihunSpineEdgeModuleSpec;

	const TCHAR* const ModuleRoot = TEXT("/Game/Level_Map/School_Map_Instance");

	// 실측 치수(cm): 복도 600×600, 교실 1800×1200, 계단 800×1600·2개 층, 로비 2400×1750
	constexpr int32 CorridorCm = 600;
	constexpr int32 ClassroomXCm = 1800;
	constexpr int32 StairsXCm = 800;

	FDigetherJihunSpineEdgeEdge Edge(const EDir Side, const EType Type, const int32 StartCm, const int32 EndCm, const int32 FloorOffset = 0)
	{
		FDigetherJihunSpineEdgeEdge Out;
		Out.Side = Side;
		Out.Type = Type;
		Out.StartCm = StartCm;
		Out.EndCm = EndCm;
		Out.FloorOffset = FloorOffset;
		return Out;
	}

	FSpec Module(const TCHAR* Folder, const TCHAR* Name, const ERole Role, const FIntPoint SizeUnits, const int32 FloorSpan = 1)
	{
		FSpec Spec;
		Spec.Id = FName(Name);
		Spec.Level = TSoftObjectPtr<UWorld>(FSoftObjectPath(FString::Printf(TEXT("%s%s/SchM_%s.SchM_%s"), ModuleRoot, Folder, Name, Name)));
		Spec.Role = Role;
		Spec.SizeUnits = SizeUnits;
		Spec.FloorSpan = FloorSpan;
		return Spec;
	}

	// 복도 북쪽 문 개구부: Doorway 500폭·측면 100 기준
	constexpr int32 DoorLowStart = 100;
	constexpr int32 DoorLowEnd = 400;
	constexpr int32 DoorHighStart = 200;
	constexpr int32 DoorHighEnd = 500;

	enum class ENorth : uint8
	{
		Wall,
		Window,
		DoorLow,
		DoorHigh
	};

	FSpec Corridor(const TCHAR* Name, const bool bOpenWest, const bool bOpenEast, const ENorth North)
	{
		FSpec Spec = Module(TEXT("/Corridor"), Name, ERole::Corridor, FIntPoint(3, 3));
		if (bOpenWest)
		{
			Spec.Edges.Add(Edge(EDir::West, EType::Open, 0, CorridorCm));
		}
		if (bOpenEast)
		{
			Spec.Edges.Add(Edge(EDir::East, EType::Open, 0, CorridorCm));
		}
		Spec.Edges.Add(Edge(EDir::South, EType::Window, 0, CorridorCm));
		switch (North)
		{
		case ENorth::Window:
			Spec.Edges.Add(Edge(EDir::North, EType::Window, 100, 500));
			break;
		case ENorth::DoorLow:
			Spec.Edges.Add(Edge(EDir::North, EType::Door, DoorLowStart, DoorLowEnd));
			break;
		case ENorth::DoorHigh:
			Spec.Edges.Add(Edge(EDir::North, EType::Door, DoorHighStart, DoorHighEnd));
			break;
		default:
			break;
		}
		return Spec;
	}

	FSpec Classroom(const TCHAR* Name)
	{
		return Module(TEXT("/Classroom"), Name, ERole::Classroom, FIntPoint(9, 6));
	}

	// 계단 층계참 문: Doorway 500폭·측면 50 기준
	FSpec Stairs(const TCHAR* Name, const bool bW0, const bool bE0, const bool bW1, const bool bE1)
	{
		FSpec Spec = Module(TEXT("/Stairs"), Name, ERole::Stairs, FIntPoint(4, 8), 2);
		const bool Doors[4] = { bW0, bE0, bW1, bE1 };
		for (int32 Slot = 0; Slot < 4; ++Slot)
		{
			if (Doors[Slot])
			{
				Spec.Edges.Add(Edge(Slot % 2 == 0 ? EDir::West : EDir::East, EType::Door, 100, 500, Slot / 2));
			}
		}
		Spec.Edges.Add(Edge(EDir::South, EType::Window, 0, StairsXCm, 0));
		Spec.Edges.Add(Edge(EDir::South, EType::Window, 0, StairsXCm, 1));
		return Spec;
	}
}

TArray<FDigetherJihunSpineEdgeModuleSpec> DigetherJihunSpineEdge::MakeGleyBoxCatalog()
{
	using namespace DigetherJihunSpineEdgeSample;
	TArray<FSpec> Catalog;

	// 로비: 앞 띠(y 50~550)가 W·E로 이어지고 뒤가 유리 출입구
	FSpec Lobby = Module(TEXT(""), TEXT("MainLobby_001"), ERole::Lobby, FIntPoint(12, 9));
	Lobby.Edges.Add(Edge(EDir::West, EType::Open, 50, 550));
	Lobby.Edges.Add(Edge(EDir::East, EType::Open, 50, 550));
	Lobby.Edges.Add(Edge(EDir::North, EType::Entrance, 100, 2300));
	Catalog.Add(Lobby);

	// 직선·막다른 끝 복도. 남쪽 = 외부 창, 북쪽 = 교실 쪽 면
	Catalog.Add(Corridor(TEXT("Corridor_001"), true, true, ENorth::DoorLow));
	Catalog.Add(Corridor(TEXT("Corridor_002"), true, true, ENorth::Window));
	Catalog.Add(Corridor(TEXT("Corridor_003"), true, true, ENorth::DoorHigh));
	Catalog.Add(Corridor(TEXT("Corridor_004"), true, true, ENorth::DoorLow));
	Catalog.Add(Corridor(TEXT("Corridor_005"), true, true, ENorth::Wall));
	Catalog.Add(Corridor(TEXT("Corridor_006"), true, true, ENorth::DoorHigh));
	Catalog.Add(Corridor(TEXT("Corridor_007"), false, true, ENorth::DoorLow));
	Catalog.Add(Corridor(TEXT("Corridor_008"), false, true, ENorth::Wall));
	Catalog.Add(Corridor(TEXT("Corridor_009"), true, false, ENorth::Wall));
	Catalog.Add(Corridor(TEXT("Corridor_010"), true, false, ENorth::DoorHigh));

	// 101 ㄱ자: W·N 열림, S·E 창
	FSpec Corner = Module(TEXT("/Corridor"), TEXT("Corridor_101"), ERole::Corridor, FIntPoint(3, 3));
	Corner.Edges.Add(Edge(EDir::West, EType::Open, 0, CorridorCm));
	Corner.Edges.Add(Edge(EDir::North, EType::Open, 0, CorridorCm));
	Corner.Edges.Add(Edge(EDir::South, EType::Window, 100, 500));
	Corner.Edges.Add(Edge(EDir::East, EType::Window, 100, 500));
	Catalog.Add(Corner);

	// 102 T자: W만 벽. 1차 생성 미지원
	FSpec Tee = Module(TEXT("/Corridor"), TEXT("Corridor_102"), ERole::Corridor, FIntPoint(3, 3));
	Tee.Edges.Add(Edge(EDir::North, EType::Open, 0, CorridorCm));
	Tee.Edges.Add(Edge(EDir::South, EType::Open, 0, CorridorCm));
	Tee.Edges.Add(Edge(EDir::East, EType::Open, 0, CorridorCm));
	Catalog.Add(Tee);

	// 103: W 막힘, 북문 + 남문(건물 밖). 1차 제외
	FSpec SouthDoor = Corridor(TEXT("Corridor_103"), false, true, ENorth::DoorLow);
	SouthDoor.Edges.RemoveAll([](const FDigetherJihunSpineEdgeEdge& E) { return E.Side == EDir::South; });
	SouthDoor.Edges.Add(Edge(EDir::South, EType::Door, DoorHighStart, DoorHighEnd));
	Catalog.Add(SouthDoor);

	// 201·202 연결 다리 1200×600. 1차 제외
	for (const TCHAR* Name : { TEXT("Corridor_201"), TEXT("Corridor_202") })
	{
		FSpec Bridge = Module(TEXT("/Corridor"), Name, ERole::Bridge, FIntPoint(6, 3));
		Bridge.Edges.Add(Edge(EDir::West, EType::Open, 50, 550));
		Bridge.Edges.Add(Edge(EDir::East, EType::Open, 50, 550));
		Bridge.Edges.Add(Edge(EDir::North, EType::Window, 0, 1200));
		Bridge.Edges.Add(Edge(EDir::South, EType::Window, 0, 1200));
		Catalog.Add(Bridge);
	}

	// 교실: 복도 쪽(S) 열림 여부와 바깥쪽(N) 모양이 변형마다 다름
	FSpec Room001 = Classroom(TEXT("classroom_001"));
	Room001.Edges.Add(Edge(EDir::South, EType::Open, 0, ClassroomXCm));
	Room001.Edges.Add(Edge(EDir::North, EType::Window, 100, 1700));
	Catalog.Add(Room001);

	FSpec Room002 = Classroom(TEXT("classroom_002"));
	Room002.Edges.Add(Edge(EDir::South, EType::Open, 0, ClassroomXCm));
	Room002.Edges.Add(Edge(EDir::North, EType::Open, 0, ClassroomXCm));
	Catalog.Add(Room002);

	FSpec Room003 = Classroom(TEXT("classroom_003"));
	Room003.Edges.Add(Edge(EDir::South, EType::Open, 0, ClassroomXCm));
	Room003.Edges.Add(Edge(EDir::North, EType::Window, 100, 1200));
	Room003.Edges.Add(Edge(EDir::North, EType::Open, 1200, 1800));
	Catalog.Add(Room003);

	FSpec Room004 = Classroom(TEXT("classroom_004"));
	Room004.Edges.Add(Edge(EDir::South, EType::Open, 0, ClassroomXCm));
	Room004.Edges.Add(Edge(EDir::North, EType::Open, 0, 600));
	Room004.Edges.Add(Edge(EDir::North, EType::Window, 600, 1700));
	Catalog.Add(Room004);

	FSpec Room005 = Classroom(TEXT("classroom_005"));
	Room005.Edges.Add(Edge(EDir::South, EType::Open, 0, 600));
	Room005.Edges.Add(Edge(EDir::South, EType::Window, 600, 1700));
	Room005.Edges.Add(Edge(EDir::North, EType::Window, 100, 1200));
	Room005.Edges.Add(Edge(EDir::North, EType::Open, 1200, 1800));
	Catalog.Add(Room005);

	FSpec Room006 = Classroom(TEXT("classroom_006"));
	Room006.Edges.Add(Edge(EDir::South, EType::Window, 100, 1200));
	Room006.Edges.Add(Edge(EDir::South, EType::Open, 1200, 1800));
	Room006.Edges.Add(Edge(EDir::North, EType::Open, 0, 600));
	Room006.Edges.Add(Edge(EDir::North, EType::Window, 600, 1700));
	Catalog.Add(Room006);

	FSpec Room007 = Classroom(TEXT("classroom_007"));
	Room007.Edges.Add(Edge(EDir::South, EType::Open, 0, 600));
	Room007.Edges.Add(Edge(EDir::North, EType::Window, 100, 1200));
	Room007.Edges.Add(Edge(EDir::North, EType::Open, 1200, 1800));
	Catalog.Add(Room007);

	FSpec Room008 = Classroom(TEXT("classroom_008"));
	Room008.Edges.Add(Edge(EDir::South, EType::Open, 1200, 1800));
	Room008.Edges.Add(Edge(EDir::North, EType::Open, 0, 600));
	Room008.Edges.Add(Edge(EDir::North, EType::Window, 600, 1700));
	Catalog.Add(Room008);

	FSpec Room009 = Classroom(TEXT("classroom_009"));
	Room009.Edges.Add(Edge(EDir::South, EType::Open, 0, 600));
	Room009.Edges.Add(Edge(EDir::North, EType::Window, 100, 1700));
	Catalog.Add(Room009);

	FSpec Room010 = Classroom(TEXT("classroom_010"));
	Room010.Edges.Add(Edge(EDir::South, EType::Open, 1200, 1800));
	Room010.Edges.Add(Edge(EDir::North, EType::Window, 100, 1700));
	Catalog.Add(Room010);

	// 교실 내부 가구층: 교실과 같은 피벗에 겹쳐 놓음, 칸 점유 없음
	for (const TCHAR* Name : { TEXT("inside_001"), TEXT("inside_002"), TEXT("inside_003") })
	{
		Catalog.Add(Module(TEXT("/Classroom_inside"), Name, ERole::ClassroomInterior, FIntPoint(9, 6)));
	}

	// 계단 001~009 = 층별 층계참 문 {W, E, WE} 3×3 조합
	Catalog.Add(Stairs(TEXT("Stairs_001"), true, true, true, true));
	Catalog.Add(Stairs(TEXT("Stairs_002"), false, true, true, true));
	Catalog.Add(Stairs(TEXT("Stairs_003"), true, false, true, true));
	Catalog.Add(Stairs(TEXT("Stairs_004"), true, true, false, true));
	Catalog.Add(Stairs(TEXT("Stairs_005"), true, true, true, false));
	Catalog.Add(Stairs(TEXT("Stairs_006"), true, false, true, false));
	Catalog.Add(Stairs(TEXT("Stairs_007"), false, true, false, true));
	Catalog.Add(Stairs(TEXT("Stairs_008"), false, true, true, false));
	Catalog.Add(Stairs(TEXT("Stairs_009"), true, false, false, true));

	return Catalog;
}
