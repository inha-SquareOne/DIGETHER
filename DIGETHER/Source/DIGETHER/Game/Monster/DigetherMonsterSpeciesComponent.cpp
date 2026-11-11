#include "Game/Monster/DigetherMonsterSpeciesComponent.h"

#include "Game/Monster/DigetherMonsterDefinition.h"
#include "Game/Monster/DigetherMonsterLog.h"

UDigetherMonsterSpeciesComponent::UDigetherMonsterSpeciesComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UDigetherMonsterSpeciesComponent::InitializeSpecies(const UDigetherMonsterDefinition& InDefinition)
{
	const UScriptStruct* Required = GetRequiredParamsStruct();
	if (Required)
	{
		const UScriptStruct* Actual = InDefinition.SpeciesParams.GetScriptStruct();
		if (!Actual || !Actual->IsChildOf(Required))
		{
			UE_LOG(LogDigetherMonster, Warning, TEXT("%s: 정의 %s의 종 파라미터(%s)가 %s 요구 타입(%s)과 다릅니다."),
				*GetNameSafe(GetOwner()), *GetNameSafe(&InDefinition), *GetNameSafe(Actual), *GetClass()->GetName(), *Required->GetName());
			return false;
		}
	}
	Definition = &InDefinition;
	return true;
}
