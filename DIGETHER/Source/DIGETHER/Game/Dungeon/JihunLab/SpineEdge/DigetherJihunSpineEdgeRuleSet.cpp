#include "DigetherJihunSpineEdgeRuleSet.h"

#include "DigetherJihunSpineEdgeModuleDef.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

TArray<FDigetherJihunSpineEdgeModuleSpec> UDigetherJihunSpineEdgeRuleSet::BuildCatalog() const
{
	TArray<FDigetherJihunSpineEdgeModuleSpec> Catalog;
	Catalog.Reserve(Modules.Num());
	for (const UDigetherJihunSpineEdgeModuleDef* Module : Modules)
	{
		if (IsValid(Module))
		{
			Catalog.Add(Module->Spec);
		}
	}
	return Catalog;
}

#if WITH_EDITOR
#define LOCTEXT_NAMESPACE "DigetherJihunSpineEdgeRuleSet"

EDataValidationResult UDigetherJihunSpineEdgeRuleSet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Modules.IsEmpty())
	{
		Context.AddError(LOCTEXT("NoModules", "모듈 목록(Modules)이 비어 있음"));
		Result = EDataValidationResult::Invalid;
	}
	if (Rules.MinFloors > Rules.MaxFloors || Rules.MinArmElements > Rules.MaxArmElements)
	{
		Context.AddError(LOCTEXT("BadRange", "최소값이 최대값보다 큼(층 수 또는 갈래 길이)"));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}

#undef LOCTEXT_NAMESPACE
#endif
