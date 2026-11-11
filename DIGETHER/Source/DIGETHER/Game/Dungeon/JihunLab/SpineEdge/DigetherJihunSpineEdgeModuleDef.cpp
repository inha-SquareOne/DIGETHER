#include "DigetherJihunSpineEdgeModuleDef.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "DigetherJihunSpineEdgeModuleDef"

EDataValidationResult UDigetherJihunSpineEdgeModuleDef::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (Spec.Level.IsNull())
	{
		Context.AddError(LOCTEXT("MissingLevel", "모듈 레벨(Level)이 비어 있음"));
		Result = EDataValidationResult::Invalid;
	}
	if (Spec.SizeUnits.X <= 0 || Spec.SizeUnits.Y <= 0)
	{
		Context.AddError(LOCTEXT("BadSize", "SizeUnits는 X, Y 모두 1 이상"));
		Result = EDataValidationResult::Invalid;
	}
	for (const FDigetherJihunSpineEdgeEdge& Edge : Spec.Edges)
	{
		if (Edge.EndCm <= Edge.StartCm || Edge.FloorOffset < 0 || Edge.FloorOffset >= Spec.FloorSpan)
		{
			Context.AddError(FText::Format(
				LOCTEXT("BadEdge", "면 구간 오류: {0}~{1}cm, 층 {2}"),
				Edge.StartCm, Edge.EndCm, Edge.FloorOffset));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}

#undef LOCTEXT_NAMESPACE
#endif
