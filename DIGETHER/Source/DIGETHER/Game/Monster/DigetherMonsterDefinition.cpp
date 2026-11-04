#include "Game/Monster/DigetherMonsterDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "DigetherMonsterDefinition"

const FPrimaryAssetType UDigetherMonsterDefinition::PrimaryAssetType(TEXT("DigetherMonster"));

FPrimaryAssetId UDigetherMonsterDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, GetFName());
}

bool UDigetherMonsterDefinition::CollectProblems(TArray<FText>& OutProblems) const
{
	const int32 Before = OutProblems.Num();
	if (!FMath::IsFinite(MaxHealth) || MaxHealth < 0.f)
	{
		OutProblems.Add(LOCTEXT("InvalidMaxHealth", "MaxHealth는 유한한 0 이상의 값이어야 합니다."));
	}
	else if (bKillable && MaxHealth <= 0.f)
	{
		OutProblems.Add(LOCTEXT("KillableRequiresHealth", "처치 가능한 몬스터의 MaxHealth는 0보다 커야 합니다."));
	}
	// 불사는 체력을 사용하지 않으므로 유한한 양수 값이 남아 있어도 오류로 취급하지 않는다.
	// 비유한 값·음수는 처치 가능 여부를 바꿀 때도 안전하게 쓸 수 없으므로 공통으로 거부한다.
	return OutProblems.Num() == Before;
}

#if WITH_EDITOR
EDataValidationResult UDigetherMonsterDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Result = Super::IsDataValid(Context);
	TArray<FText> Problems;
	if (!CollectProblems(Problems))
	{
		for (const FText& Problem : Problems)
		{
			Context.AddError(Problem);
		}
		return EDataValidationResult::Invalid;
	}
	return Result == EDataValidationResult::Invalid ? Result : EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
