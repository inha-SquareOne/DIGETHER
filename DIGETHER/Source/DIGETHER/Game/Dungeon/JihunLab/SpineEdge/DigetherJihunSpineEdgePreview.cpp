#include "DigetherJihunSpineEdgePreview.h"

#include "DigetherJihunSpineEdgeGenerator.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

namespace DigetherJihunSpineEdgePreviewDetail
{
	// 상자를 모듈 경계보다 조금 안쪽에 그려 이웃 상자 선과 겹치지 않게 함
	constexpr float BoxInsetCm = 20.0f;
	constexpr float EdgeMarkerHeightCm = 250.0f;
	constexpr float LineThickness = 4.0f;

	FColor RoleColor(const EDigetherJihunSpineEdgeRole Role)
	{
		switch (Role)
		{
		case EDigetherJihunSpineEdgeRole::Lobby:
			return FColor::Yellow;
		case EDigetherJihunSpineEdgeRole::Classroom:
			return FColor::Green;
		case EDigetherJihunSpineEdgeRole::Stairs:
			return FColor::Magenta;
		default:
			return FColor::Cyan;
		}
	}
}

FTransform DigetherJihunSpineEdgePreview::GetPlacementTransform(
	const FDigetherJihunSpineEdgePlacement& Placement,
	const FDigetherJihunSpineEdgeRules& Rules,
	const FTransform& Origin)
{
	const FVector LocalLocation(
		Placement.PivotUnits.X * Rules.UnitSizeCm,
		Placement.PivotUnits.Y * Rules.UnitSizeCm,
		Placement.Floor * Rules.FloorHeightCm);
	const FTransform Local(FRotator(0.0, 90.0 * Placement.QuarterTurns, 0.0), LocalLocation);
	return Local * Origin;
}

void DigetherJihunSpineEdgePreview::DrawLayout(
	const UWorld* World,
	const FTransform& Origin,
	const TArray<FDigetherJihunSpineEdgeModuleSpec>& Catalog,
	const FDigetherJihunSpineEdgeRules& Rules,
	const FDigetherJihunSpineEdgeLayout& Layout,
	const bool bPersistent,
	const float LifeSeconds)
{
#if ENABLE_DRAW_DEBUG
	using namespace DigetherJihunSpineEdgePreviewDetail;
	if (!IsValid(World))
	{
		return;
	}
	const float Unit = Rules.UnitSizeCm;
	const float FloorHeight = Rules.FloorHeightCm;
	for (const FDigetherJihunSpineEdgePlacement& Placement : Layout.Placements)
	{
		if (!Catalog.IsValidIndex(Placement.ModuleIndex))
		{
			continue;
		}
		const FDigetherJihunSpineEdgeModuleSpec& Spec = Catalog[Placement.ModuleIndex];
		if (Spec.Role == EDigetherJihunSpineEdgeRole::ClassroomInterior)
		{
			continue;
		}

		FIntPoint Min;
		FIntPoint Size;
		DigetherJihunSpineEdge::GetPlacementRect(Spec, Placement, Min, Size);
		const FVector LocalMin(Min.X * Unit + BoxInsetCm, Min.Y * Unit + BoxInsetCm, Placement.Floor * FloorHeight);
		const FVector LocalMax((Min.X + Size.X) * Unit - BoxInsetCm, (Min.Y + Size.Y) * Unit - BoxInsetCm, (Placement.Floor + Spec.FloorSpan) * FloorHeight - BoxInsetCm);
		const FVector Center = Origin.TransformPosition((LocalMin + LocalMax) * 0.5);
		DrawDebugBox(World, Center, (LocalMax - LocalMin) * 0.5, Origin.GetRotation(), RoleColor(Spec.Role), bPersistent, LifeSeconds, 0, LineThickness);

		// 문은 빨강, 열린 면은 흰색 세로 판으로 표시
		TArray<FDigetherJihunSpineEdgeWorldEdge> Edges;
		DigetherJihunSpineEdge::GetWorldEdges(Spec, Placement, Rules.UnitSizeCm, Edges);
		for (const FDigetherJihunSpineEdgeWorldEdge& Edge : Edges)
		{
			if (Edge.Type != EDigetherJihunSpineEdgeType::Door && Edge.Type != EDigetherJihunSpineEdgeType::Open)
			{
				continue;
			}
			const bool bAlongX = Edge.Dir == EDigetherJihunSpineEdgeDir::North || Edge.Dir == EDigetherJihunSpineEdgeDir::South;
			const FVector A = bAlongX ? FVector(Edge.StartCm, Edge.LineCm, 0.0) : FVector(Edge.LineCm, Edge.StartCm, 0.0);
			const FVector B = bAlongX ? FVector(Edge.EndCm, Edge.LineCm, 0.0) : FVector(Edge.LineCm, Edge.EndCm, 0.0);
			const FVector Floor(0.0, 0.0, Edge.Floor * FloorHeight + BoxInsetCm);
			const FVector Up(0.0, 0.0, EdgeMarkerHeightCm);
			const FColor Color = Edge.Type == EDigetherJihunSpineEdgeType::Door ? FColor::Red : FColor::White;
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
