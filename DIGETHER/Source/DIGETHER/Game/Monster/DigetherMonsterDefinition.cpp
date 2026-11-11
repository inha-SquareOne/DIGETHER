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
	// 불사는 체력 미사용, 유한한 양수 값이 남아 있어도 허용
	// 비유한 값·음수는 처치 가능 여부를 바꿀 때도 위험해 공통 거부
	return OutProblems.Num() == Before;
}

bool UDigetherMonsterDefinition::CollectSpawnProblems(TArray<FText>& OutProblems) const
{
	const int32 Before = OutProblems.Num();
	CollectProblems(OutProblems);
	CollectSpawnOnlyProblems(OutProblems);
	return OutProblems.Num() == Before;
}

void UDigetherMonsterDefinition::CollectSpawnOnlyProblems(TArray<FText>& OutProblems) const
{
	if (Zone == EDigetherMonsterZone::None)
	{
		OutProblems.Add(LOCTEXT("ZoneRequired", "Zone(갱도·던전)을 지정해야 합니다."));
	}
	if (SpawnKind == EDigetherMonsterSpawnKind::PeriodicPower)
	{
		if (Zone != EDigetherMonsterZone::Dungeon)
		{
			OutProblems.Add(LOCTEXT("PowerRequiresDungeon", "주기+파워형은 던전 몬스터만 사용합니다."));
		}
		if (PowerLevel < 1 || PowerLevel > 3)
		{
			OutProblems.Add(LOCTEXT("PowerLevelRange", "주기+파워형의 PowerLevel은 1~3이어야 합니다."));
		}
	}
	else if (Zone == EDigetherMonsterZone::Dungeon)
	{
		OutProblems.Add(LOCTEXT("DungeonRequiresPower", "던전 몬스터는 주기+파워형이어야 합니다."));
	}
	if (!FMath::IsFinite(SpawnTelegraphSeconds) || SpawnTelegraphSeconds < 0.f
		|| !FMath::IsFinite(MinPlayerDistance) || MinPlayerDistance < 0.f)
	{
		OutProblems.Add(LOCTEXT("InvalidSpawnTiming", "예고 시간과 최소 거리는 유한한 0 이상의 값이어야 합니다."));
	}
	if (MonsterClass.IsNull())
	{
		OutProblems.Add(LOCTEXT("MonsterClassRequired", "MonsterClass를 지정해야 합니다."));
	}
	if (BehaviorTree.IsNull())
	{
		OutProblems.Add(LOCTEXT("BehaviorTreeRequired", "BehaviorTree를 지정해야 합니다."));
	}
	if (Movement.CapsuleRadius <= 0.f || Movement.CapsuleHalfHeight < Movement.CapsuleRadius
		|| Movement.MaxStepHeight < 0.f || Movement.WalkSpeed < 0.f)
	{
		OutProblems.Add(LOCTEXT("InvalidMovement", "캡슐 반지름은 양수, 반높이는 반지름 이상, 단차·속도는 0 이상이어야 합니다."));
	}
	// 종 구조체 자체 검사, 종 컴포넌트와의 타입 일치는 캐릭터 초기화 때 확인
	if (const FDigetherMonsterSpeciesParams* Params = SpeciesParams.GetPtr<FDigetherMonsterSpeciesParams>())
	{
		Params->CollectProblems(OutProblems);
	}
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
	// 스폰 데이터는 BP·BT 제작 전 단계에서 비어 있을 수 있어 경고, 디렉터가 생성 시 거절
	TArray<FText> SpawnProblems;
	CollectSpawnOnlyProblems(SpawnProblems);
	for (const FText& Problem : SpawnProblems)
	{
		Context.AddWarning(Problem);
	}
	return Result == EDataValidationResult::Invalid ? Result : EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
