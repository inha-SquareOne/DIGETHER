#include "DigetherJihunGridGraphRuleSet.h"

#include "DigetherJihunGridGraphModuleDef.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

TArray<FDigetherJihunGridGraphModuleSpec> UDigetherJihunGridGraphRuleSet::BuildCatalog() const
{
	TArray<FDigetherJihunGridGraphModuleSpec> Catalog;
	Catalog.Reserve(Modules.Num());
	for (const UDigetherJihunGridGraphModuleDef* Module : Modules)
	{
		if (IsValid(Module))
		{
			Catalog.Add(Module->Spec);
		}
	}
	return Catalog;
}

#if WITH_EDITOR
#define LOCTEXT_NAMESPACE "DigetherJihunGridGraphRuleSet"

EDataValidationResult UDigetherJihunGridGraphRuleSet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Modules.IsEmpty())
	{
		// Builder가 실측 카탈로그로 대신 생성함(모듈 Data Asset 작성 전 단계)
		Context.AddWarning(LOCTEXT("NoModules", "모듈 목록(Modules)이 비어 있어 코드의 GleyBox 실측 카탈로그를 사용"));
	}
	if (Rules.MinRunModules > Rules.MaxRunModules || Rules.MinNodesPerFloor > Rules.MaxNodesPerFloor
		|| Rules.MinStairsBetweenFloors > Rules.MaxStairsBetweenFloors || Rules.DeadEndMinModules > Rules.DeadEndMaxModules
		|| Rules.MinFarthestModules > Rules.MaxFarthestModules || Rules.MinTotalModules > Rules.MaxTotalModules)
	{
		Context.AddError(LOCTEXT("BadRange", "최소값이 최대값보다 큰 규칙이 있음"));
		Result = EDataValidationResult::Invalid;
	}
	if (Rules.LobbyFloor >= Rules.FloorCount)
	{
		Context.AddError(LOCTEXT("BadLobbyFloor", "로비 층(LobbyFloor)이 층 수 밖에 있음"));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}

#undef LOCTEXT_NAMESPACE
#endif
