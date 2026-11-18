#include "DigetherJihunGridGraphPreview.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"

namespace DigetherJihunGridGraphPreviewDetail
{
	// 상자를 모듈 경계보다 조금 안쪽에 그려 이웃 상자 선과 겹치지 않게 함
	constexpr float BoxInsetCm = 20.0f;
	constexpr float EdgeMarkerHeightCm = 250.0f;
	constexpr float LineThickness = 4.0f;

	FColor RoleColor(const EDigetherJihunGridGraphRole Role)
	{
		switch (Role)
		{
		case EDigetherJihunGridGraphRole::Lobby:
			return FColor::Yellow;
		case EDigetherJihunGridGraphRole::Classroom:
			return FColor::Green;
		case EDigetherJihunGridGraphRole::Stairs:
			return FColor::Magenta;
		case EDigetherJihunGridGraphRole::Bridge:
			return FColor::Orange;
		default:
			return FColor::Cyan;
		}
	}
}

FTransform DigetherJihunGridGraphPreview::GetPlacementTransform(
	const FDigetherJihunGridGraphPlacement& Placement,
	const FDigetherJihunGridGraphRules& Rules,
	const FTransform& Origin)
{
	const FVector LocalLocation(
		Placement.PivotUnits.X * Rules.UnitSizeCm,
		Placement.PivotUnits.Y * Rules.UnitSizeCm,
		Placement.Floor * Rules.FloorHeightCm);
	const FTransform Local(FRotator(0.0, 90.0 * Placement.QuarterTurns, 0.0), LocalLocation);
	return Local * Origin;
}

void DigetherJihunGridGraphPreview::DrawLayout(
	const UWorld* World,
	const FTransform& Origin,
	const TArray<FDigetherJihunGridGraphModuleSpec>& Catalog,
	const FDigetherJihunGridGraphRules& Rules,
	const FDigetherJihunGridGraphLayout& Layout,
	const int32 OnlyFloor,
	const bool bPersistent,
	const float LifeSeconds)
{
#if ENABLE_DRAW_DEBUG
	using namespace DigetherJihunGridGraphPreviewDetail;
	if (!IsValid(World))
	{
		return;
	}
	const float Unit = Rules.UnitSizeCm;
	const float FloorHeight = Rules.FloorHeightCm;
	for (const FDigetherJihunGridGraphPlacement& Placement : Layout.Placements)
	{
		if (!Catalog.IsValidIndex(Placement.ModuleIndex))
		{
			continue;
		}
		const FDigetherJihunGridGraphModuleSpec& Spec = Catalog[Placement.ModuleIndex];
		const bool bOnFloor = OnlyFloor < 0 || (OnlyFloor >= Placement.Floor && OnlyFloor < Placement.Floor + Spec.FloorSpan);
		if (Spec.Role == EDigetherJihunGridGraphRole::ClassroomInterior || !bOnFloor)
		{
			continue;
		}

		FIntPoint Min;
		FIntPoint Size;
		DigetherJihunGridGraph::GetPlacementRect(Spec, Placement, Min, Size);
		const FVector LocalMin(Min.X * Unit + BoxInsetCm, Min.Y * Unit + BoxInsetCm, Placement.Floor * FloorHeight);
		const FVector LocalMax((Min.X + Size.X) * Unit - BoxInsetCm, (Min.Y + Size.Y) * Unit - BoxInsetCm, (Placement.Floor + Spec.FloorSpan) * FloorHeight - BoxInsetCm);
		const FVector Center = Origin.TransformPosition((LocalMin + LocalMax) * 0.5);
		DrawDebugBox(World, Center, (LocalMax - LocalMin) * 0.5, Origin.GetRotation(), RoleColor(Spec.Role), bPersistent, LifeSeconds, 0, LineThickness);

		// 문은 빨강, 열린 면은 흰색 세로 판으로 표시
		TArray<FDigetherJihunGridGraphWorldEdge> Edges;
		DigetherJihunGridGraph::GetWorldEdges(Spec, Placement, Rules.UnitSizeCm, Edges);
		for (const FDigetherJihunGridGraphWorldEdge& Edge : Edges)
		{
			const bool bPassage = Edge.Type == EDigetherJihunGridGraphEdgeType::Door || Edge.Type == EDigetherJihunGridGraphEdgeType::Open;
			if (!bPassage || (OnlyFloor >= 0 && Edge.Floor != OnlyFloor))
			{
				continue;
			}
			const bool bAlongX = Edge.Dir == EDigetherJihunGridGraphDir::North || Edge.Dir == EDigetherJihunGridGraphDir::South;
			const FVector A = bAlongX ? FVector(Edge.StartCm, Edge.LineCm, 0.0) : FVector(Edge.LineCm, Edge.StartCm, 0.0);
			const FVector B = bAlongX ? FVector(Edge.EndCm, Edge.LineCm, 0.0) : FVector(Edge.LineCm, Edge.EndCm, 0.0);
			const FVector Floor(0.0, 0.0, Edge.Floor * FloorHeight + BoxInsetCm);
			const FVector Up(0.0, 0.0, EdgeMarkerHeightCm);
			const FColor Color = Edge.Type == EDigetherJihunGridGraphEdgeType::Door ? FColor::Red : FColor::White;
			const FVector WA = Origin.TransformPosition(A + Floor);
			const FVector WB = Origin.TransformPosition(B + Floor);
			const FVector WUp = Origin.TransformVector(Up);
			DrawDebugLine(World, WA, WB, Color, bPersistent, LifeSeconds, 0, LineThickness);
			DrawDebugLine(World, WA + WUp, WB + WUp, Color, bPersistent, LifeSeconds, 0, LineThickness);
			DrawDebugLine(World, WA, WA + WUp, Color, bPersistent, LifeSeconds, 0, LineThickness);
			DrawDebugLine(World, WB, WB + WUp, Color, bPersistent, LifeSeconds, 0, LineThickness);
		}
	}
#endif
}
