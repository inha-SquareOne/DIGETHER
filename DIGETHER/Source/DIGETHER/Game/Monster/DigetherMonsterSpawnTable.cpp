#include "Game/Monster/DigetherMonsterSpawnTable.h"

#include "Game/Monster/DigetherMonsterDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "DigetherMonsterSpawnTable"

const FPrimaryAssetType UDigetherMonsterSpawnTable::PrimaryAssetType(TEXT("DigetherMonsterSpawnTable"));

FPrimaryAssetId UDigetherMonsterSpawnTable::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, GetFName());
}

namespace
{
	void CollectEntryProblems(const TArray<FDigetherMonsterPeriodicEntry>& Entries, const EDigetherMonsterSpawnKind ExpectedKind,
		const FText& ListName, TArray<FText>& OutProblems)
	{
		for (int32 Index = 0; Index < Entries.Num(); ++Index)
		{
			const FDigetherMonsterPeriodicEntry& Entry = Entries[Index];
			if (!IsValid(Entry.Definition))
			{
				OutProblems.Add(FText::Format(LOCTEXT("EntryMissingDefinition", "{0} {1}번 항목에 정의가 없습니다."), ListName, Index));
				continue;
			}
			if (Entry.Definition->SpawnKind != ExpectedKind)
			{
				OutProblems.Add(FText::Format(LOCTEXT("EntryWrongKind", "{0} {1}번 항목 {2}의 스폰 방식이 목록과 맞지 않습니다."),
					ListName, Index, FText::FromName(Entry.Definition->GetFName())));
			}
			if (!FMath::IsFinite(Entry.Weight) || Entry.Weight < 0.f || Entry.MaxAlive < 0)
			{
				OutProblems.Add(FText::Format(LOCTEXT("EntryInvalidValue", "{0} {1}번 항목의 가중치·최대 수가 잘못됐습니다."), ListName, Index));
			}
		}
	}
}

bool UDigetherMonsterSpawnTable::CollectProblems(TArray<FText>& OutProblems) const
{
	const int32 Before = OutProblems.Num();
	if (StageId.IsNone())
	{
		OutProblems.Add(LOCTEXT("StageIdRequired", "StageId(목적지 ID)를 지정해야 합니다."));
	}
	const FDigetherMonsterSpawnCurve& C = Curve;
	const bool bFinite = FMath::IsFinite(C.GraceSeconds) && FMath::IsFinite(C.IntervalSeconds) && FMath::IsFinite(C.StartChance)
		&& FMath::IsFinite(C.MaxChance) && FMath::IsFinite(C.MaxChanceAtSeconds);
	if (!bFinite || C.GraceSeconds < 0.f || C.IntervalSeconds < 0.1f)
	{
		OutProblems.Add(LOCTEXT("InvalidCurveTiming", "유예는 0 이상, 판정 간격은 0.1초 이상이어야 합니다."));
	}
	else if (C.StartChance < 0.f || C.StartChance > 1.f || C.MaxChance < 0.f || C.MaxChance > 1.f)
	{
		OutProblems.Add(LOCTEXT("InvalidCurveChance", "확률은 0~1이어야 합니다."));
	}
	else if (!C.OverrideCurve && C.MaxChance > 0.f && C.MaxChanceAtSeconds <= C.GraceSeconds)
	{
		OutProblems.Add(LOCTEXT("InvalidCurveReach", "최종 확률 도달 시각은 유예보다 커야 합니다."));
	}
	else if (!C.OverrideCurve && C.StartChance > C.MaxChance)
	{
		OutProblems.Add(LOCTEXT("InvalidCurveOrder", "시작 확률은 최종 확률보다 클 수 없습니다."));
	}
	const FDigetherDungeonPowerLimit& P = DungeonPowerLimit;
	if (P.Base < 0 || P.PerGoalAchieved < 0 || P.Max < P.Base)
	{
		OutProblems.Add(LOCTEXT("InvalidPowerLimit", "파워 한도는 0 이상이고 최대는 기본값 이상이어야 합니다."));
	}
	CollectEntryProblems(DungeonEntries, EDigetherMonsterSpawnKind::PeriodicPower, LOCTEXT("DungeonList", "던전 테이블"), OutProblems);
	CollectEntryProblems(MinePeriodicEntries, EDigetherMonsterSpawnKind::Periodic, LOCTEXT("MineList", "갱도 주기 테이블"), OutProblems);
	for (int32 Index = 0; Index < MineHabitatEntries.Num(); ++Index)
	{
		const FDigetherMonsterHabitatEntry& Entry = MineHabitatEntries[Index];
		if (!IsValid(Entry.Definition) || Entry.Definition->SpawnKind != EDigetherMonsterSpawnKind::Habitat)
		{
			OutProblems.Add(FText::Format(LOCTEXT("HabitatInvalid", "서식지 목록 {0}번 항목의 정의가 없거나 서식지형이 아닙니다."), Index));
		}
		if (!FMath::IsFinite(Entry.Chance) || Entry.Chance < 0.f || Entry.Chance > 1.f)
		{
			OutProblems.Add(FText::Format(LOCTEXT("HabitatChance", "서식지 목록 {0}번 항목의 확률은 0~1이어야 합니다."), Index));
		}
	}
	return OutProblems.Num() == Before;
}

#if WITH_EDITOR
EDataValidationResult UDigetherMonsterSpawnTable::IsDataValid(FDataValidationContext& Context) const
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
