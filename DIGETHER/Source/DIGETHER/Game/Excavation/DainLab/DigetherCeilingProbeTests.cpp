#if WITH_DEV_AUTOMATION_TESTS
#include "DigetherCeilingProbe.h"
#include "Game/Excavation/Dain/DigetherDiggableSoil.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "HAL/PlatformTime.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherCeilingProbeTest, "DIGETHER.Game.Tunnel.CarvedCeiling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherCeilingProbeTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->AddToRoot();
	Instance->InitializeStandalone();
	UWorld* World = Instance->GetWorld();
	ADigetherDiggableSoil* Soil = World->SpawnActor<ADigetherDiggableSoil>();
	ADigetherCeilingProbe* Probe = World->SpawnActor<ADigetherCeilingProbe>();
	Probe->Soil = Soil;
	Probe->SetActorLocation(FVector(200));
	Probe->Region->SetBoxExtent(FVector(300));
	for (int32 Mode : {0, 8, 16})
	{
		TestTrue(TEXT("Initialize comparison mode"), Soil->SetComparisonChunkCells(Mode));
		TestTrue(TEXT("Capture original closed surface"), Probe->CaptureBaseline());
		TestEqual(TEXT("Original ceiling and other original faces excluded"), Probe->RefreshCandidates(), 0);
		TestTrue(TEXT("Carve interior cavity with existing core"),
			Soil->TryExcavate(FVector(200), 100) == EDigetherSoilEditResult::Applied);
		const uint32 Hash = Soil->GetDensityHash();
		const uint32 Revision = Soil->GetRevision();
		const int32 CeilingCount = Probe->RefreshCandidates();
		TestTrue(TEXT("New downward cavity surface selected"), CeilingCount > 0);
		TestEqual(TEXT("Refresh does not change density"), Soil->GetDensityHash(), Hash);
		TestEqual(TEXT("Refresh does not change revision"), Soil->GetRevision(), Revision);
		Probe->SetActorLocation(FVector(200, 200, 130));
		Probe->Region->SetBoxExtent(FVector(150, 150, 45));
		TestEqual(TEXT("Carved floor excluded"), Probe->RefreshCandidates(), 0);
		Probe->SetActorLocation(FVector(290, 200, 200));
		Probe->Region->SetBoxExtent(FVector(60, 150, 20));
		TestEqual(TEXT("Carved side wall excluded"), Probe->RefreshCandidates(), 0);
		Probe->SetActorLocation(FVector(200, 200, 290));
		Probe->Region->SetBoxExtent(FVector(150, 150, 50));
		TestTrue(TEXT("Carved roof inside designated region"), Probe->RefreshCandidates() > 0);
		Probe->SetActorLocation(FVector(1000));
		TestEqual(TEXT("Outside designated region excluded"), Probe->RefreshCandidates(), 0);
		Probe->SetActorLocation(FVector(200));
		Probe->Region->SetBoxExtent(FVector(300));
		TestTrue(TEXT("Rejected dig"), Soil->TryExcavate(FVector(200), -1) == EDigetherSoilEditResult::Rejected);
		TestEqual(TEXT("Rejected dig preserves classification"), Probe->RefreshCandidates(), CeilingCount);
		ADigetherCeilingProbe* LateProbe = World->SpawnActor<ADigetherCeilingProbe>();
		LateProbe->Soil = Soil;
		TestFalse(TEXT("Late capture never treats carved state as initial"), LateProbe->CaptureBaseline());
		LateProbe->Destroy();
		TestTrue(TEXT("Reset terrain"), Soil->ResetSoil());
		TestTrue(TEXT("Recapture reset baseline"), Probe->CaptureBaseline());
		TestEqual(TEXT("Reset removes all candidates"), Probe->RefreshCandidates(), 0);
	}
	Instance->Shutdown();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	Instance->RemoveFromRoot();
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherCeilingPerformanceTest, "DIGETHER.Game.Tunnel.CeilingPerformance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherCeilingPerformanceTest::RunTest(const FString& Parameters)
{
	// 전용 명령행에서 실제 저장 맵을 로드한 뒤 실행, 변경은 저장하지 않음
	UWorld* World = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.World() && Context.World()->WorldType == EWorldType::Editor &&
			Context.World()->GetMapName().EndsWith(TEXT("L_DainTunnelLayout")))
		{
			World = Context.World();
			break;
		}
	}
	if (!World)
	{
		AddError(TEXT("Run with L_DainTunnelLayout loaded; this benchmark is not part of the generic regression filter."));
		return false;
	}
	ADigetherCeilingProbe* Probe = nullptr;
	for (TActorIterator<ADigetherCeilingProbe> It(World); It; ++It)
	{
		if (IsValid(It->Soil) && It->Soil->ActorHasTag(TEXT("TunnelShortcut")))
		{
			Probe = *It;
			break;
		}
	}
	if (!TestNotNull(TEXT("Map shortcut probe"), Probe))
	{
		return false;
	}
	ADigetherDiggableSoil* Soil = Probe->Soil;
	const bool bOriginalDraw = Probe->bDrawCandidates;
	const int32 OriginalMode = Soil->GetComparisonChunkCells();
	uint32 ExpectedHash = 0;
	const auto Report = [this](int32 Mode, const TCHAR* Stage, TArray<double> Values)
	{
		double Sum = 0;
		for (double Value : Values) { Sum += Value; }
		Values.Sort();
		AddInfo(FString::Printf(TEXT("CEILING_BENCH mode=%d stage=%s n=%d mean=%.4f p95=%.4f max=%.4f ms"),
			Mode, Stage, Values.Num(), Sum / Values.Num(), Values[FMath::CeilToInt(Values.Num() * .95) - 1], Values.Last()));
	};
	for (int32 Mode = 0; Mode < 4; ++Mode)
	{
		TestTrue(TEXT("Reset same eight-cell wall"), Soil->SetComparisonChunkCells(8));
		TestTrue(TEXT("Capture same initial geometry"), Probe->CaptureBaseline());
		Probe->bDrawCandidates = (Mode & 2) != 0;
		TArray<double> Dig, Check, Draw, Unchanged, Total;
		int32 Applied = 0;
		for (int32 Index = 0; Index < 24; ++Index)
		{
			const FVector Center = FVector(Soil->GridCells) * Soil->CellSpacing * .5;
			// 격자/스케일 변경 전후에도 기존 4배 흙벽과 같은 월드 타격 위치 사용
			const FVector Local = Center + FVector(-300 + (Index % 6) * 120, ((Index / 6) % 2) * 120 - 60, (Index / 12) * 70 - 35) / Soil->GetActorScale3D();
			const double Start = FPlatformTime::Seconds();
			Applied += Soil->TryExcavate(Soil->GetActorTransform().TransformPosition(Local), 75) == EDigetherSoilEditResult::Applied;
			const double DigMs = (FPlatformTime::Seconds() - Start) * 1000;
			const double CheckStart = FPlatformTime::Seconds();
			Probe->RefreshCandidates();
			const double CheckMs = (FPlatformTime::Seconds() - CheckStart) * 1000;
			// 검사 OFF도 동일 선 개수를 준비하되 그 준비 비용은 비교 합계에서 제외
			const double DrawStart = FPlatformTime::Seconds();
			Probe->DrawCandidates();
			const double DrawMs = (FPlatformTime::Seconds() - DrawStart) * 1000;
			const double RepeatStart = FPlatformTime::Seconds();
			Probe->RefreshCandidates();
			Unchanged.Add((FPlatformTime::Seconds() - RepeatStart) * 1000);
			Dig.Add(DigMs);
			Check.Add((Mode & 1) ? CheckMs : 0);
			Draw.Add(DrawMs);
			Total.Add(DigMs + Check.Last() + DrawMs);
			FlushPersistentDebugLines(World);
		}
		Report(Mode, TEXT("Dig"), Dig);
		Report(Mode, TEXT("Check"), Check);
		Report(Mode, TEXT("DrawSubmit"), Draw);
		Report(Mode, TEXT("UnchangedCheck"), Unchanged);
		Report(Mode, TEXT("CPUStageSum"), Total);
		AddInfo(FString::Printf(TEXT("CEILING_BENCH mode=%d applied=%d triangles=%d hash=%u"),
			Mode, Applied, Probe->CandidateTriangles, Soil->GetDensityHash()));
		if (Mode == 0) { ExpectedHash = Soil->GetDensityHash(); }
		TestEqual(TEXT("All toggles retain same final excavation"), Soil->GetDensityHash(), ExpectedHash);
	}
	Soil->SetComparisonChunkCells(OriginalMode);
	Probe->CaptureBaseline();
	Probe->bDrawCandidates = bOriginalDraw;
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherCeilingAccelerationTest, "DIGETHER.Game.Tunnel.CeilingAcceleration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDigetherCeilingAccelerationTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->AddToRoot();
	Instance->InitializeStandalone();
	UWorld* World = Instance->GetWorld();
	ADigetherDiggableSoil* Soil = World->SpawnActor<ADigetherDiggableSoil>();
	Soil->GridCells = FIntVector(9, 33, 19);
	Soil->CellSpacing = 25;
	TestTrue(TEXT("Initialize actual map grid"), Soil->SetComparisonChunkCells(8));
	ADigetherCeilingProbe* Probe = World->SpawnActor<ADigetherCeilingProbe>();
	Probe->Soil = Soil;
	TestTrue(TEXT("Baseline blocks prepared"), Probe->CaptureBaseline());
	for (const FIntVector Cells : {FIntVector(9,33,19), FIntVector(33,33,19)})
	{
		Soil->GridCells = Cells;
		TestTrue(TEXT("Reset baseline grid"), Soil->ResetSoil());
		TestTrue(TEXT("Rebuild spatial hierarchy"), Probe->CaptureBaseline());
	// 최적화 이전의 전체 순회와 안/밖·원래 표면 근처의 판정이 같은지 비교
	for (int32 Index = 0; Index < 400; ++Index)
	{
		const FVector Point((Index * 37) % 250 - 10, (Index * 101) % 850 - 10, (Index * 53) % 500 - 10);
		double Best = TNumericLimits<double>::Max();
		double Signed = 0;
		for (int32 Vertex = 0; Vertex < Probe->InitialTriangles.Num(); Vertex += 3)
		{
			const FVector A = Probe->InitialTriangles[Vertex], B = Probe->InitialTriangles[Vertex + 1], C = Probe->InitialTriangles[Vertex + 2];
			const FVector Closest = FMath::ClosestPointOnTriangleToPoint(Point, A, B, C);
			const double Distance = FVector::DistSquared(Point, Closest);
			if (Distance < Best)
			{
				Best = Distance;
				Signed = FVector::DotProduct(Point - Closest, FVector::CrossProduct(C - A, B - A).GetSafeNormal());
			}
		}
		TestEqual(TEXT("Spatial hierarchy equals original full scan"), Probe->WasInsideInitialSoil(Point), Best > .25 && Signed < -.5);
	}
	}
	Soil->GridCells = FIntVector(9,33,19);
	Soil->ResetSoil();
	Probe->CaptureBaseline();
	Probe->SetActorLocation(FVector(112.5, 412.5, 237.5));
	Probe->Region->SetBoxExtent(FVector(500));
	Soil->TryExcavate(FVector(112.5, 412.5, 237.5), 75);
	const int32 Count = Probe->RefreshCandidates();
	TestTrue(TEXT("Initial result"), Count > 0);
	const TArray<FVector> Vertices = Probe->CandidateVertices;
	TestEqual(TEXT("Unchanged state reuses result"), Probe->RefreshCandidates(), Count);
	TestTrue(TEXT("Cached vertices preserved"), Vertices == Probe->CandidateVertices);
	Probe->SetActorLocation(FVector(5000));
	TestEqual(TEXT("Region movement invalidates cache"), Probe->RefreshCandidates(), 0);
	Probe->SetActorLocation(FVector(112.5, 412.5, 237.5));
	TestEqual(TEXT("Region restored"), Probe->RefreshCandidates(), Count);
	Probe->CeilingAngleDegrees = -1;
	TestEqual(TEXT("Invalid angle clears result"), Probe->RefreshCandidates(), 0);
	Probe->CeilingAngleDegrees = 45;
	TestEqual(TEXT("Valid angle rebuilds result"), Probe->RefreshCandidates(), Count);
	Soil->ResetSoil();
	TestEqual(TEXT("Reset invalidates carved cache"), Probe->RefreshCandidates(), 0);
	Soil->GridCells = FIntVector(8);
	Soil->ResetSoil();
	TestEqual(TEXT("Changed initial shape never becomes carved ceiling"), Probe->RefreshCandidates(), 0);
	Instance->Shutdown();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	Instance->RemoveFromRoot();
	return true;
}
#endif
