#include "DigetherCeilingProbe.h"
#include "Game/Excavation/Dain/DigetherDiggableSoil.h"
#include "Components/BoxComponent.h"
#include "ProceduralMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Algo/Sort.h"

namespace
{
	TArray<FVector> ReadActiveTriangles(const ADigetherDiggableSoil& Soil)
	{
		TArray<FVector> Result;
		TInlineComponentArray<UProceduralMeshComponent*> Meshes;
		Soil.GetComponents(Meshes);
		for (UProceduralMeshComponent* Mesh : Meshes)
		{
			// 이중 버퍼의 준비 중 후보는 표시·충돌이 모두 비활성
			if (!Mesh->IsVisible() || Mesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
			{
				continue;
			}
			const FProcMeshSection* Section = Mesh->GetProcMeshSection(0);
			if (!Section)
			{
				continue;
			}
			for (uint32 Index : Section->ProcIndexBuffer)
			{
				const FVector World = Mesh->GetComponentTransform().TransformPosition(Section->ProcVertexBuffer[Index].Position);
				Result.Add(Soil.GetActorTransform().InverseTransformPosition(World));
			}
		}
		return Result;
	}
}

ADigetherCeilingProbe::ADigetherCeilingProbe()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = .2f;
	Region = CreateDefaultSubobject<UBoxComponent>(TEXT("CollapseCandidateRegion"));
	SetRootComponent(Region);
	Region->SetBoxExtent(FVector(300));
	Region->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Region->SetGenerateOverlapEvents(false);
}

bool ADigetherCeilingProbe::CaptureBaseline()
{
	bBaselineReady = false;
	CandidateTriangles = 0;
	CandidateVertices.Reset();
	InitialTriangles.Reset();
	InitialNodes.Reset();
	InitialTriangleOrder.Reset();
	if (!IsValid(Soil) || Soil->GetRevision() != 0)
	{
		return false;
	}
	InitialTriangles = ReadActiveTriangles(*Soil);
	for (int32 Index = 0; Index < InitialTriangles.Num() / 3; ++Index) InitialTriangleOrder.Add(Index);
	if (!InitialTriangleOrder.IsEmpty()) BuildInitialNode(0, InitialTriangleOrder.Num());
	bBaselineReady = !InitialTriangles.IsEmpty();
	BaselineDensityHash = Soil->GetDensityHash();
	BaselineSoil = Soil;
	LastRevision = 0;
	LastAngle = -1.f;
	return bBaselineReady;
}

int32 ADigetherCeilingProbe::BuildInitialNode(int32 First, int32 Count)
{
	FInitialSurfaceNode Node;
	Node.First = First;
	Node.Count = Count;
	for (int32 Index = First; Index < First + Count; ++Index)
		for (int32 Corner = 0; Corner < 3; ++Corner)
			Node.Bounds += InitialTriangles[InitialTriangleOrder[Index] * 3 + Corner];
	const int32 NodeIndex = InitialNodes.Add(Node);
	if (Count > 8)
	{
		const FVector Size = Node.Bounds.GetSize();
		const int32 Axis = Size.X >= Size.Y && Size.X >= Size.Z ? 0 : (Size.Y >= Size.Z ? 1 : 2);
		Algo::Sort(MakeArrayView(InitialTriangleOrder.GetData() + First, Count), [this, Axis](int32 A, int32 B)
		{
			const double CA = (InitialTriangles[A*3] + InitialTriangles[A*3+1] + InitialTriangles[A*3+2])[Axis];
			const double CB = (InitialTriangles[B*3] + InitialTriangles[B*3+1] + InitialTriangles[B*3+2])[Axis];
			return CA == CB ? A < B : CA < CB;
		});
		const int32 Left = BuildInitialNode(First, Count / 2);
		const int32 Right = BuildInitialNode(First + Count / 2, Count - Count / 2);
		// 재귀 Add가 배열을 재할당할 수 있으므로 노드 참조를 잡아두지 않음
		InitialNodes[NodeIndex].Left = Left;
		InitialNodes[NodeIndex].Right = Right;
	}
	return NodeIndex;
}

bool ADigetherCeilingProbe::WasInsideInitialSoil(const FVector& Point) const
{
	// 기존 최근접 면·0.5cm 제외 규칙 유지. 가까운 경계부터 방문해 먼 하위 노드 제거
	double BestDistance = TNumericLimits<double>::Max();
	double SignedDistance = 0;
	int32 BestTriangle = MAX_int32;
	TArray<int32, TInlineAllocator<64>> Pending;
	if (!InitialNodes.IsEmpty()) Pending.Add(0);
	while (!Pending.IsEmpty())
	{
		const FInitialSurfaceNode& Node = InitialNodes[Pending.Pop(EAllowShrinking::No)];
		if (Node.Bounds.ComputeSquaredDistanceToPoint(Point) > BestDistance) continue;
		if (Node.Left != INDEX_NONE)
		{
			const double LeftDistance = InitialNodes[Node.Left].Bounds.ComputeSquaredDistanceToPoint(Point);
			const double RightDistance = InitialNodes[Node.Right].Bounds.ComputeSquaredDistanceToPoint(Point);
			Pending.Add(LeftDistance <= RightDistance ? Node.Right : Node.Left);
			Pending.Add(LeftDistance <= RightDistance ? Node.Left : Node.Right);
			continue;
		}
		for (int32 Index = Node.First; Index < Node.First + Node.Count; ++Index)
		{
			const int32 Triangle = InitialTriangleOrder[Index];
			const FVector A = InitialTriangles[Triangle*3], B = InitialTriangles[Triangle*3+1], C = InitialTriangles[Triangle*3+2];
			const FVector Closest = FMath::ClosestPointOnTriangleToPoint(Point, A, B, C);
			const double Distance = FVector::DistSquared(Point, Closest);
			if (Distance < BestDistance || (Distance == BestDistance && Triangle < BestTriangle))
			{
				BestDistance = Distance;
				BestTriangle = Triangle;
				SignedDistance = FVector::DotProduct(Point - Closest, FVector::CrossProduct(C - A, B - A).GetSafeNormal());
			}
		}
	}
	return BestDistance > .25 && SignedDistance < -.5;
}

int32 ADigetherCeilingProbe::RefreshCandidates()
{
	if (!IsValid(Soil) || BaselineSoil.Get() != Soil || !bBaselineReady ||
		!FMath::IsFinite(CeilingAngleDegrees) || CeilingAngleDegrees < 0 || CeilingAngleDegrees >= 90)
	{
		CandidateVertices.Reset();
		CandidateTriangles = 0;
		LastAngle = -1.f;
		return 0;
	}
	const uint32 DensityHash = Soil->GetDensityHash();
	// Revision 0의 재초기화도 초기 형상이 바뀌면 기준 표면을 새로 캡처
	if (Soil->GetRevision() == 0 && DensityHash != BaselineDensityHash)
	{
		CaptureBaseline();
	}
	if (LastAngle == CeilingAngleDegrees && LastRevision == Soil->GetRevision() && LastDensityHash == DensityHash &&
		LastSoilTransform.Equals(Soil->GetActorTransform()) && LastRegionTransform.Equals(Region->GetComponentTransform()) &&
		LastExtent == Region->GetUnscaledBoxExtent())
	{
		return CandidateTriangles;
	}
	CandidateVertices.Reset();
	CandidateTriangles = 0;
	LastRevision = Soil->GetRevision();
	LastDensityHash = DensityHash;
	LastSoilTransform = Soil->GetActorTransform();
	LastRegionTransform = Region->GetComponentTransform();
	LastExtent = Region->GetUnscaledBoxExtent();
	LastAngle = CeilingAngleDegrees;
	const FBox Box(-LastExtent, LastExtent);
	const double MinDownDot = FMath::Cos(FMath::DegreesToRadians(CeilingAngleDegrees));
	const TArray<FVector> Current = ReadActiveTriangles(*Soil);
	for (int32 Index = 0; Index < Current.Num(); Index += 3)
	{
		const FVector A = LastSoilTransform.TransformPosition(Current[Index]);
		const FVector B = LastSoilTransform.TransformPosition(Current[Index + 1]);
		const FVector C = LastSoilTransform.TransformPosition(Current[Index + 2]);
		// 코어는 UE 순서 A,C,B로 저장하므로 역외적으로 공기 방향 복원
		// 조명용 보간 노멀 대신 월드 삼각형 방향 사용, 회전·비균일 스케일 반영
		const FVector Normal = FVector::CrossProduct(C - A, B - A).GetSafeNormal();
		if (FVector::DotProduct(Normal, -FVector::UpVector) < MinDownDot ||
			!Box.IsInsideOrOn(LastRegionTransform.InverseTransformPosition(A)) ||
			!Box.IsInsideOrOn(LastRegionTransform.InverseTransformPosition(B)) ||
			!Box.IsInsideOrOn(LastRegionTransform.InverseTransformPosition(C)))
		{
			continue;
		}
		if (WasInsideInitialSoil((Current[Index] + Current[Index + 1] + Current[Index + 2]) / 3.0))
		{
			CandidateVertices.Append({A, B, C});
		}
	}
	CandidateTriangles = CandidateVertices.Num() / 3;
	return CandidateTriangles;
}

void ADigetherCeilingProbe::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsValid(Soil))
	{
		CandidateVertices.Reset();
		CandidateTriangles = 0;
		bBaselineReady = false;
		return;
	}
	if (!Soil->HasActorBegunPlay())
	{
		return;
	}
	if (BaselineSoil.Get() != Soil || !bBaselineReady || (LastRevision > 0 && Soil->GetRevision() == 0))
	{
		CaptureBaseline();
	}
	RefreshCandidates();
	DrawCandidates();
}

void ADigetherCeilingProbe::DrawCandidates() const
{
	if (!bDrawCandidates)
	{
		return;
	}
	DrawDebugBox(GetWorld(), Region->GetComponentLocation(), Region->GetScaledBoxExtent(),
		Region->GetComponentQuat(), FColor::Cyan, false, .25f);
	for (int32 Index = 0; Index < CandidateVertices.Num(); Index += 3)
	{
		for (int32 Edge = 0; Edge < 3; ++Edge)
		{
			DrawDebugLine(GetWorld(), CandidateVertices[Index + Edge], CandidateVertices[Index + (Edge + 1) % 3],
				FColor::Green, false, .25f, 0, 1.5f);
		}
	}
}
