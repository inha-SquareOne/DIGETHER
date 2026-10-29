#include "DigetherPrototypeMineScene.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	enum class EMineTint : uint8
	{
		Wood,
		Rock,
		Metal,
		Rail,
		School,
		SchoolGreen,
		Tile,
		Glass,
		Lamp,
		Fluor,
		Num
	};

	enum class EMineLight : uint8
	{
		Warm,
		School
	};

	constexpr float CubeSize = 100.f;
}

ADigetherPrototypeMineScene::ADigetherPrototypeMineScene()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetCanBeDamaged(false);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* const CubeMesh = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
	UStaticMesh* const CylinderMesh = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;

	// 기본 도형은 100cm 단위 크기이므로 Size(cm)/100을 스케일로 쓴다. 충돌은 bBlocks인 부품만 켠다.
	auto AddPart = [&](UStaticMesh* Mesh, EMineTint Tint, const FVector& Location, const FVector& Size, const FRotator& Rotation, bool bBlocks)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Part_%03d"), Parts.Num()));
		Part->SetupAttachment(Root);
		Part->SetMobility(EComponentMobility::Static);
		if (Mesh)
		{
			Part->SetStaticMesh(Mesh);
		}
		Part->SetRelativeLocationAndRotation(Location, Rotation);
		Part->SetRelativeScale3D(Size / CubeSize);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
		if (bBlocks)
		{
			Part->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		}
		else
		{
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		Parts.Add(Part);
		PartTints.Add(static_cast<uint8>(Tint));
	};
	auto Box = [&](EMineTint Tint, const FVector& Location, const FVector& Size, const FRotator& Rotation = FRotator::ZeroRotator, bool bBlocks = false)
	{
		const bool bOuterShell = Tint == EMineTint::Rock &&
			((Size.Z >= 300.f && (FMath::Abs(Location.Y) > 900.f || Location.X < -500.f || Location.X > 6000.f)) ||
			 (Size.Z <= 50.f && Size.X > 2000.f && Location.Z > 400.f));
		AddPart(CubeMesh, Tint, Location, Size, Rotation, bBlocks || bOuterShell || (Tint == EMineTint::School && Size.Z >= 300.f));
	};
	auto Cylinder = [&](EMineTint Tint, const FVector& Location, float Diameter, float Height, const FRotator& Rotation = FRotator::ZeroRotator)
	{
		AddPart(CylinderMesh, Tint, Location, FVector(Diameter, Diameter, Height), Rotation, false);
	};
	auto AddLight = [&](EMineLight Kind, const FVector& Location)
	{
		UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("Light_%02d"), Lights.Num()));
		Light->SetupAttachment(Root);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetRelativeLocation(Location);
		Light->SetCastShadows(false);
		Lights.Add(Light);
		LightKinds.Add(static_cast<uint8>(Kind));
	};

	FRandomStream Random(0x5EED);

	// ---- 기지 외피: 천장 z450, 뒷벽 x-500, 측벽 y±1000. 바닥은 WorldBuilder 담당. ----
	Box(EMineTint::Rock, FVector(750.f, 0.f, 460.f), FVector(2530.f, 2060.f, 20.f));
	Box(EMineTint::Rock, FVector(4050.f, 0.f, 775.f), FVector(4100.f, 2300.f, 30.f));
	Box(EMineTint::Rock, FVector(-515.f, 0.f, 225.f), FVector(30.f, 2060.f, 450.f));
	Box(EMineTint::Rock, FVector(750.f, 1015.f, 225.f), FVector(2530.f, 30.f, 450.f));
	// 이전 시험 복도 출입구는 닫고 폐교는 포털로 연결한다.
	Box(EMineTint::Rock, FVector(92.5f, -1015.f, 225.f), FVector(1215.f, 30.f, 450.f));
	Box(EMineTint::Rock, FVector(1557.5f, -1015.f, 225.f), FVector(915.f, 30.f, 450.f));
	Box(EMineTint::Rock, FVector(900.f, -1015.f, 225.f), FVector(400.f, 30.f, 450.f), FRotator::ZeroRotator, true);
	// 동쪽 벽은 갱도 입구(y±500)를 열어 둔다. 입구 위는 갱도 천장(z760)까지 막는다.
	Box(EMineTint::Rock, FVector(2000.f, 757.5f, 380.f), FVector(30.f, 515.f, 760.f));
	Box(EMineTint::Rock, FVector(2000.f, -757.5f, 380.f), FVector(30.f, 515.f, 760.f));
	Box(EMineTint::Rock, FVector(2000.f, 0.f, 605.f), FVector(30.f, 1000.f, 310.f));

	// ---- 기지 목재 지지대 ----
	for (const float X : {-400.f, 0.f, 400.f, 800.f, 1200.f, 1600.f})
	{
		Box(EMineTint::Wood, FVector(X, 975.f, 225.f), FVector(35.f, 35.f, 450.f));
	}
	for (const float X : {-400.f, 0.f, 400.f, 1200.f, 1600.f})
	{
		Box(EMineTint::Wood, FVector(X, -975.f, 225.f), FVector(35.f, 35.f, 450.f));
	}
	for (const float X : {-300.f, 300.f, 900.f, 1500.f})
	{
		Box(EMineTint::Wood, FVector(X, 0.f, 432.f), FVector(35.f, 2000.f, 30.f));
	}

	// ---- 광산기지 작업대(뒷벽 중앙) ----
	Box(EMineTint::Wood, FVector(-420.f, 0.f, 90.f), FVector(80.f, 320.f, 10.f));
	for (const float X : {-450.f, -390.f})
	{
		for (const float Y : {-150.f, 150.f})
		{
			Box(EMineTint::Wood, FVector(X, Y, 42.f), FVector(8.f, 8.f, 84.f));
		}
	}
	Box(EMineTint::Wood, FVector(-420.f, 0.f, 35.f), FVector(70.f, 300.f, 5.f));
	Box(EMineTint::Metal, FVector(-497.f, 0.f, 170.f), FVector(6.f, 300.f, 120.f));
	Box(EMineTint::Rail, FVector(-415.f, -90.f, 106.f), FVector(30.f, 50.f, 24.f));
	Cylinder(EMineTint::Metal, FVector(-420.f, 90.f, 105.f), 22.f, 25.f);

	// ---- 낮은 상자·통(모서리). 게임 장치를 가리지 않도록 높이 60 이하 ----
	for (const float Y : {-880.f, 880.f})
	{
		Box(EMineTint::Wood, FVector(-420.f, Y, 30.f), FVector(90.f, 90.f, 60.f), FRotator(0.f, 12.f, 0.f));
		Box(EMineTint::Wood, FVector(1900.f, Y, 30.f), FVector(90.f, 90.f, 60.f), FRotator(0.f, -8.f, 0.f));
		Cylinder(EMineTint::Metal, FVector(-300.f, Y, 30.f), 55.f, 60.f);
	}

	// ---- 기지 조명 ----
	for (const FVector2D& Spot : {FVector2D(0.f, 0.f), FVector2D(900.f, 550.f), FVector2D(900.f, -550.f), FVector2D(1700.f, 0.f), FVector2D(-350.f, 0.f)})
	{
		Box(EMineTint::Lamp, FVector(Spot.X, Spot.Y, 440.f), FVector(30.f, 30.f, 20.f));
		AddLight(EMineLight::Warm, FVector(Spot.X, Spot.Y, 390.f));
	}

	// ---- 갱도 암벽 면(회전 큐브). y±1100 안쪽으로 최대 약 70cm만 돌출한다. ----
	for (int32 Index = 0; Index < 10; ++Index)
	{
		const float X = 2200.f + 400.f * Index;
		for (const float Side : {-1.f, 1.f})
		{
			// 북쪽 미로 입구 (x 약 2800~3200)를 남긴다.
			if (Index == 2 && Side > 0.f) continue;
			Box(EMineTint::Rock,
				FVector(X + Random.FRandRange(-20.f, 20.f), Side * (1170.f + Random.FRandRange(0.f, 40.f)), 380.f),
				FVector(440.f, 150.f, 780.f), FRotator(0.f, Random.FRandRange(-7.f, 7.f), 0.f));
		}
	}
	// 끝벽 전체를 닫아 굴착 패치 뒤로 돌아가도 맵 밖으로 나갈 수 없게 한다.
	Box(EMineTint::Rock, FVector(6100.f, 775.f, 380.f), FVector(150.f, 650.f, 780.f));
	Box(EMineTint::Rock, FVector(6100.f, -775.f, 380.f), FVector(150.f, 650.f, 780.f));
	Box(EMineTint::Rock, FVector(6000.f, 0.f, 380.f), FVector(150.f, 900.f, 780.f), FRotator::ZeroRotator, true);

	// 굴착 볼륨의 노출된 측면·뒷면은 파괴 불가 암벽으로 닫는다.
	Box(EMineTint::Rock, FVector(5680.f, 0.f, 200.f), FVector(260.f, 830.f, 830.f), FRotator::ZeroRotator, true);
	Box(EMineTint::Rock, FVector(5425.f, 415.f, 200.f), FVector(250.f, 30.f, 830.f), FRotator::ZeroRotator, true);
	Box(EMineTint::Rock, FVector(5425.f, -415.f, 200.f), FVector(250.f, 30.f, 830.f), FRotator::ZeroRotator, true);
	Box(EMineTint::Rock, FVector(5425.f, 0.f, 615.f), FVector(250.f, 800.f, 30.f), FRotator::ZeroRotator, true);
	Box(EMineTint::Rock, FVector(5425.f, 0.f, -215.f), FVector(250.f, 800.f, 30.f), FRotator::ZeroRotator, true);

	// 서쪽 가장자리 고정 작업 계단. 흙을 걷어내면 30cm 단차로 지표까지 돌아온다.
	for (int32 Step = 0; Step < 8; ++Step)
	{
		const float Top = Step == 0 ? -2.f : -30.f * Step;
		Box(EMineTint::Metal, FVector(4010.f + 45.f * Step, 300.f, (Top - 250.f) * .5f),
			FVector(45.f, 180.f, Top + 250.f), FRotator::ZeroRotator, true);
	}

	// 천장에 매달린 암석 덩어리(z630 이상)와 벽 밑 바위(낮음).
	const float LumpX[] = {2500.f, 3300.f, 3900.f, 4600.f, 5200.f, 5800.f};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(LumpX); ++Index)
	{
		Box(EMineTint::Rock, FVector(LumpX[Index], (Index % 2 == 0) ? 700.f : -700.f, 700.f),
			FVector(180.f, 200.f, 140.f), FRotator(Random.FRandRange(-12.f, 12.f), Random.FRandRange(-40.f, 40.f), Random.FRandRange(-12.f, 12.f)));
	}
	const float RockX[] = {2500.f, 3100.f, 3800.f, 4300.f, 5000.f, 5400.f, 5900.f};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(RockX); ++Index)
	{
		Box(EMineTint::Rock, FVector(RockX[Index], (Index % 2 == 0) ? 1000.f : -1000.f, 30.f),
			FVector(130.f, 100.f, 70.f), FRotator(Random.FRandRange(-6.f, 6.f), Random.FRandRange(-45.f, 45.f), Random.FRandRange(-6.f, 6.f)));
	}

	// ---- 갱도 목재 프레임. 이동 중앙과 굴착 구멍 위는 z690 이상 보로만 지난다. ----
	for (const float X : {2400.f, 3600.f, 4400.f, 5000.f})
	{
		Box(EMineTint::Wood, FVector(X, 1040.f, 345.f), FVector(35.f, 35.f, 690.f));
		Box(EMineTint::Wood, FVector(X, -1040.f, 345.f), FVector(35.f, 35.f, 690.f));
		Box(EMineTint::Wood, FVector(X, 0.f, 690.f), FVector(35.f, 2120.f, 35.f));
	}
	// 굴착벽 패치(x5300) 옆·위 프레임. 패치 몸체(y±400, z-200~600)에는 들어가지 않는다.
	Box(EMineTint::Wood, FVector(5500.f, 480.f, 320.f), FVector(40.f, 40.f, 640.f));
	Box(EMineTint::Wood, FVector(5500.f, -480.f, 320.f), FVector(40.f, 40.f, 640.f));
	Box(EMineTint::Wood, FVector(5500.f, 0.f, 660.f), FVector(40.f, 1040.f, 40.f));

	// ---- 철제 승강기 상부 구조(cage는 별도 액터). 승강기 운행 범위 z0~500 위에만 둔다. ----
	Box(EMineTint::Metal, FVector(3000.f, 0.f, 730.f), FVector(30.f, 2100.f, 30.f));
	Cylinder(EMineTint::Metal, FVector(3000.f, 220.f, 690.f), 110.f, 40.f, FRotator(0.f, 0.f, 90.f));

	// ---- 굴착 구멍(x4000~4800,y±400) 가장자리 노란 난간. 구멍 위는 비운다. ----
	for (const float Y : {-440.f, 440.f})
	{
		for (const float X : {4000.f, 4400.f, 4800.f})
		{
			Box(EMineTint::Rail, FVector(X, Y, 50.f), FVector(8.f, 8.f, 100.f));
		}
		Box(EMineTint::Rail, FVector(4400.f, Y, 95.f), FVector(800.f, 5.f, 5.f));
		Box(EMineTint::Rail, FVector(4400.f, Y, 50.f), FVector(800.f, 5.f, 5.f));
	}

	// ---- 광차 레일: y-700/-800, 구멍 바깥. 높이 15 이하라 이동에 영향이 없다. ----
	Box(EMineTint::Metal, FVector(3800.f, -700.f, 10.f), FVector(3400.f, 6.f, 10.f));
	Box(EMineTint::Metal, FVector(3800.f, -800.f, 10.f), FVector(3400.f, 6.f, 10.f));
	for (float X = 2200.f; X <= 5400.f; X += 200.f)
	{
		Box(EMineTint::Wood, FVector(X, -750.f, 3.f), FVector(24.f, 150.f, 6.f));
	}

	// 갱도 벽가 낮은 상자.
	Box(EMineTint::Wood, FVector(2500.f, 950.f, 30.f), FVector(80.f, 80.f, 60.f), FRotator(0.f, 20.f, 0.f));
	Box(EMineTint::Wood, FVector(3800.f, -950.f, 30.f), FVector(80.f, 80.f, 60.f), FRotator(0.f, -15.f, 0.f));
	Box(EMineTint::Wood, FVector(5100.f, 950.f, 30.f), FVector(80.f, 80.f, 60.f), FRotator(0.f, 8.f, 0.f));
	Box(EMineTint::Wood, FVector(5600.f, -950.f, 30.f), FVector(80.f, 80.f, 60.f), FRotator(0.f, 30.f, 0.f));

	// ---- 갱도 조명(천장 케이블 + 랜턴) ----
	for (const float X : {2500.f, 3300.f, 4050.f, 4750.f, 5300.f, 5800.f})
	{
		Cylinder(EMineTint::Metal, FVector(X, 0.f, 700.f), 4.f, 120.f);
		Box(EMineTint::Lamp, FVector(X, 0.f, 640.f), FVector(30.f, 30.f, 20.f));
		AddLight(EMineLight::Warm, FVector(X, 0.f, 610.f));
	}

	// ---- 북쪽 시험 미로. 폭 500cm 통로와 세 교차 벽의 엇갈린 문을 둔다. ----
	Box(EMineTint::Rock, FVector(4200.f, 2350.f, -10.f), FVector(3200.f, 2500.f, 20.f), FRotator::ZeroRotator, true);
	Box(EMineTint::Rock, FVector(4200.f, 2350.f, 460.f), FVector(3200.f, 2500.f, 20.f), FRotator::ZeroRotator, true);
	Box(EMineTint::Rock, FVector(2600.f, 2350.f, 225.f), FVector(30.f, 2500.f, 450.f), FRotator::ZeroRotator, true);
	Box(EMineTint::Rock, FVector(5800.f, 2350.f, 225.f), FVector(30.f, 2500.f, 450.f), FRotator::ZeroRotator, true);
	Box(EMineTint::Rock, FVector(4200.f, 3600.f, 225.f), FVector(3200.f, 30.f, 450.f), FRotator::ZeroRotator, true);
	// 기존 벽과 연결되는 남쪽 경계, 입구 x2800~3200은 개방.
	Box(EMineTint::Rock, FVector(2700.f, 1110.f, 225.f), FVector(200.f, 30.f, 450.f), FRotator::ZeroRotator, true);
	Box(EMineTint::Rock, FVector(4500.f, 1110.f, 225.f), FVector(2600.f, 30.f, 450.f), FRotator::ZeroRotator, true);
	auto MazeWall = [&](float X, float GapLow, float GapHigh)
	{
		Box(EMineTint::Rock, FVector(X, (1110.f + GapLow) * .5f, 225.f), FVector(35.f, GapLow - 1110.f, 450.f), FRotator::ZeroRotator, true);
		Box(EMineTint::Rock, FVector(X, (GapHigh + 3600.f) * .5f, 225.f), FVector(35.f, 3600.f - GapHigh, 450.f), FRotator::ZeroRotator, true);
	};
	MazeWall(3600.f, 2750.f, 3250.f);
	MazeWall(4400.f, 1500.f, 2000.f);
	MazeWall(5200.f, 2750.f, 3250.f);
	// 막다른 가지는 주 경로를 가로막지 않는다.
	Box(EMineTint::Rock, FVector(4250.f, 2450.f, 225.f), FVector(300.f, 35.f, 450.f), FRotator::ZeroRotator, true);
	Box(EMineTint::Rock, FVector(5650.f, 2150.f, 225.f), FVector(300.f, 35.f, 450.f), FRotator::ZeroRotator, true);
	for (const FVector2D& Spot : {FVector2D(3000.f, 1600.f), FVector2D(3000.f, 3000.f), FVector2D(4000.f, 3000.f), FVector2D(4000.f, 1750.f), FVector2D(4800.f, 1750.f), FVector2D(4800.f, 3000.f), FVector2D(5600.f, 3000.f)})
	{
		Box(EMineTint::Wood, FVector(Spot.X, Spot.Y, 425.f), FVector(35.f, 650.f, 35.f));
		Box(EMineTint::Lamp, FVector(Spot.X, Spot.Y, 405.f), FVector(25.f, 25.f, 20.f));
		AddLight(EMineLight::Warm, FVector(Spot.X, Spot.Y, 370.f));
	}

	// ---- 별도 폐교 시험 공간: 중앙 복도와 북쪽 교실 두 개. ----
	Box(EMineTint::Tile, FVector(21000.f, 0.f, -10.f), FVector(3000.f, 2400.f, 20.f), FRotator::ZeroRotator, true);
	Box(EMineTint::School, FVector(21000.f, 0.f, 410.f), FVector(3000.f, 2400.f, 20.f), FRotator::ZeroRotator, true);
	for (const float X : {19500.f, 22500.f})
		Box(EMineTint::School, FVector(X, 0.f, 200.f), FVector(30.f, 2400.f, 400.f), FRotator::ZeroRotator, true);
	for (const float Y : {-1200.f, 1200.f})
		Box(EMineTint::School, FVector(21000.f, Y, 200.f), FVector(3000.f, 30.f, 400.f), FRotator::ZeroRotator, true);
	Box(EMineTint::School, FVector(21000.f, -350.f, 200.f), FVector(3000.f, 30.f, 400.f), FRotator::ZeroRotator, true);
	Box(EMineTint::School, FVector(21100.f, 775.f, 200.f), FVector(30.f, 850.f, 400.f), FRotator::ZeroRotator, true);
	// 복도 북벽은 두 교실 문만 비운다.
	for (const FVector2D& Segment : {FVector2D(19500.f, 20350.f), FVector2D(20650.f, 21450.f), FVector2D(21750.f, 22500.f)})
		Box(EMineTint::School, FVector((Segment.X + Segment.Y) * .5f, 350.f, 200.f), FVector(Segment.Y - Segment.X, 30.f, 400.f), FRotator::ZeroRotator, true);
	for (const float X : {20500.f, 21600.f})
		Box(EMineTint::School, FVector(X, 350.f, 350.f), FVector(300.f, 30.f, 100.f), FRotator::ZeroRotator, true);
	// 녹색 하부 벽은 장식판이며 물리 경계는 그 뒤의 벽이 맡는다.
	for (const float Y : {-1180.f, 1180.f})
		Box(EMineTint::SchoolGreen, FVector(21000.f, Y, 70.f), FVector(2950.f, 5.f, 140.f));
	Box(EMineTint::SchoolGreen, FVector(21000.f, -330.f, 70.f), FVector(2950.f, 5.f, 140.f));
	for (const FVector2D& Segment : {FVector2D(19500.f, 20350.f), FVector2D(20650.f, 21450.f), FVector2D(21750.f, 22500.f)})
		Box(EMineTint::SchoolGreen, FVector((Segment.X + Segment.Y) * .5f, 330.f, 70.f), FVector(Segment.Y - Segment.X, 5.f, 140.f));
	for (const float X : {20100.f, 20200.f, 20800.f, 20900.f, 21200.f, 21300.f, 22000.f, 22100.f})
	{
		Box(EMineTint::Metal, FVector(X, -275.f, 100.f), FVector(65.f, 110.f, 200.f), FRotator::ZeroRotator, true);
		// 복도 쪽 문 윤곽과 손잡이. 상호작용·이동 충돌은 몸체만 담당한다.
		Box(EMineTint::SchoolGreen, FVector(X, -218.f, 100.f), FVector(57.f, 3.f, 185.f));
		Box(EMineTint::Rail, FVector(X + 20.f, -215.f, 105.f), FVector(5.f, 5.f, 18.f));
	}
	// 중앙 교실 문과 회수품 경로(x20500 / x21600, y350~700)는 비운다.
	for (const float X : {20100.f, 20300.f, 20700.f, 20900.f, 21200.f, 21400.f, 21800.f, 22000.f})
	{
		for (const float Y : {550.f, 950.f})
		{
			Box(EMineTint::Wood, FVector(X, Y, 70.f), FVector(120.f, 90.f, 12.f), FRotator::ZeroRotator, true);
			for (const float DX : {-45.f, 45.f}) for (const float DY : {-30.f, 30.f})
				Box(EMineTint::Metal, FVector(X + DX, Y + DY, 33.f), FVector(8.f, 8.f, 66.f));
			Box(EMineTint::Wood, FVector(X, Y + 100.f, 42.f), FVector(60.f, 55.f, 8.f));
			Box(EMineTint::Wood, FVector(X, Y + 125.f, 75.f), FVector(60.f, 8.f, 70.f));
		}
	}
	// 교실 칠판, 트레이, 어두운 창과 창틀은 폐교의 용도를 식별시키는 비충돌 장식이다.
	for (const float X : {19520.f, 21120.f})
	{
		Box(EMineTint::SchoolGreen, FVector(X, 760.f, 235.f), FVector(8.f, 480.f, 150.f));
		Box(EMineTint::Wood, FVector(X + 8.f, 760.f, 155.f), FVector(20.f, 500.f, 12.f));
		Box(EMineTint::Fluor, FVector(X + 16.f, 720.f, 165.f), FVector(6.f, 40.f, 5.f));
	}
	for (const float X : {20100.f, 20800.f, 21300.f, 22000.f})
	{
		Box(EMineTint::Glass, FVector(X, 1180.f, 250.f), FVector(210.f, 5.f, 170.f));
		for (const float DX : {-110.f, 0.f, 110.f})
			Box(EMineTint::Wood, FVector(X + DX, 1174.f, 250.f), FVector(10.f, 12.f, 190.f));
		for (const float Z : {155.f, 250.f, 345.f})
			Box(EMineTint::Wood, FVector(X, 1174.f, Z), FVector(230.f, 12.f, 10.f));
	}
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const float X = Index == 0 ? 20500.f : 21600.f;
		UTextRenderComponent* RoomNumber = CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("SchoolRoomNumber%d"), Index));
		RoomNumber->SetupAttachment(Root);
		RoomNumber->SetRelativeLocationAndRotation(FVector(X, 326.f, 280.f), FRotator(0.f, -90.f, 0.f));
		RoomNumber->SetHorizontalAlignment(EHTA_Center);
		RoomNumber->SetWorldSize(24.f);
		RoomNumber->SetText(FText::AsNumber(Index + 1));
	}
	for (const FVector& Debris : {FVector(19900.f, 1120.f, 8.f), FVector(20880.f, 1080.f, 8.f), FVector(21170.f, 1080.f, 8.f), FVector(22300.f, 950.f, 8.f)})
		Box(EMineTint::Wood, Debris, FVector(100.f, 30.f, 12.f), FRotator(0.f, 25.f, 0.f));
	Box(EMineTint::School, FVector(21850.f, 1170.f, 170.f), FVector(180.f, 8.f, 150.f), FRotator(0.f, 0.f, 8.f));
	for (const FVector2D& Spot : {FVector2D(19800.f, 0.f), FVector2D(20500.f, 0.f), FVector2D(21600.f, 0.f), FVector2D(20500.f, 800.f), FVector2D(21600.f, 800.f)})
	{
		Box(EMineTint::Fluor, FVector(Spot.X, Spot.Y, 395.f), FVector(120.f, 25.f, 6.f));
		AddLight(EMineLight::School, FVector(Spot.X, Spot.Y, 355.f));
	}
}

void ADigetherPrototypeMineScene::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMaterials();
	ApplyLighting();
}

void ADigetherPrototypeMineScene::BeginPlay()
{
	Super::BeginPlay();
	if (TintMaterials.Num() == 0)
	{
		ApplyMaterials();
	}
}

void ADigetherPrototypeMineScene::ApplyMaterials()
{
	TintMaterials.Reset();

	UMaterialInterface* Parent = SurfaceMaterial;
	if (!Parent)
	{
		// 엔진 기본 도형 재질에 "Color" 파라미터가 있는지는 실행 확인이 필요하다.
		Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"), nullptr, LOAD_NoWarn);
	}
	if (!Parent)
	{
		for (UStaticMeshComponent* Part : Parts)
		{
			if (Part)
			{
				Part->SetMaterial(0, nullptr);
			}
		}
		return;
	}

	const FLinearColor Tints[] = {
		WoodColor,
		RockColor,
		MetalColor,
		RailColor,
		FLinearColor(0.64f, 0.65f, 0.57f),
		FLinearColor(0.20f, 0.34f, 0.22f),
		FLinearColor(0.45f, 0.40f, 0.30f),
		FLinearColor(0.03f, 0.06f, 0.09f),
		FLinearColor(1.f, 0.45f, 0.08f),
		FLinearColor(0.80f, 0.90f, 0.80f)};
	static_assert(UE_ARRAY_COUNT(Tints) == static_cast<int32>(EMineTint::Num), "Tint 배열과 EMineTint가 일치해야 한다.");

	for (const FLinearColor& Tint : Tints)
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Parent, this);
		Material->SetVectorParameterValue(TintParameterName, Tint);
		Material->SetVectorParameterValue(TEXT("Color"), Tint);
		TintMaterials.Add(Material);
	}

	for (int32 Index = 0; Index < Parts.Num(); ++Index)
	{
		if (Parts[Index] && TintMaterials.IsValidIndex(PartTints[Index]))
		{
			Parts[Index]->SetMaterial(0, TintMaterials[PartTints[Index]]);
		}
	}
}

void ADigetherPrototypeMineScene::ApplyLighting()
{
	for (int32 Index = 0; Index < Lights.Num(); ++Index)
	{
		UPointLightComponent* Light = Lights[Index];
		if (!Light)
		{
			continue;
		}
		const bool bSchool = LightKinds[Index] == static_cast<uint8>(EMineLight::School);
		Light->SetIntensityUnits(ELightUnits::Lumens);
		Light->SetIntensity(LightIntensityLumens * (bSchool ? 0.7f : 1.f));
		Light->SetLightColor(bSchool ? FLinearColor(0.85f, 1.f, 0.9f) : LightColor);
		Light->SetAttenuationRadius(LightAttenuationRadius);
		Light->SetCastShadows(bLightsCastShadows);
	}
}
