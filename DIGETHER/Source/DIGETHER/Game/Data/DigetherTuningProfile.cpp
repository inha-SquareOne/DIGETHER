#include "Game/Data/DigetherTuningProfile.h"

#include "Game/Elevator/DigetherElevatorRules.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "DigetherTuningProfile"

const FPrimaryAssetType UDigetherTuningProfile::PrimaryAssetType(TEXT("DigetherTuningProfile"));

bool FDigetherRunTuning::CollectProblems(TArray<FText>& OutProblems) const
{
	const int32 Before = OutProblems.Num();
	auto Require = [&OutProblems](const bool bCondition, const FText& Problem)
	{
		if (!bCondition)
		{
			OutProblems.Add(Problem);
		}
	};
	// NaN도 걸러지도록 "조건을 만족한다" 형태로 비교한다.
	Require(ProfileVersion >= 1, LOCTEXT("Version", "ProfileVersion은 1 이상이어야 합니다."));
	Require(InitialQuotaCredits >= 1, LOCTEXT("Quota", "InitialQuotaCredits는 1 이상이어야 합니다."));
	Require(RunSeconds >= 1.f, LOCTEXT("RunSeconds", "RunSeconds는 1초 이상이어야 합니다."));
	Require(InteractionDistanceCm >= 1.f, LOCTEXT("Distance", "InteractionDistanceCm는 1 이상이어야 합니다."));
	Require(ElevatorCapacityKg >= 1.f, LOCTEXT("Capacity", "ElevatorCapacityKg는 1 이상이어야 합니다."));
	Require(DigetherElevatorRules::IsValidTripLimit(ElevatorTripLimit), LOCTEXT("Trips", "ElevatorTripLimit은 2여야 합니다(일반 상승 2회 확정)."));
	Require(AscentPenaltySeconds >= 0.f, LOCTEXT("Penalty", "AscentPenaltySeconds는 0 이상이어야 합니다."));
	Require(ElevatorTravelSeconds >= 0.1f, LOCTEXT("Travel", "ElevatorTravelSeconds는 0.1초 이상이어야 합니다."));
	Require(DepositIntervalSeconds >= 0.1f, LOCTEXT("Deposit", "DepositIntervalSeconds는 0.1초 이상이어야 합니다."));
	Require(EmergencyWeightLimitKg >= 0.f, LOCTEXT("Emergency", "EmergencyWeightLimitKg는 0 이상이어야 합니다."));
	Require(DeathLossPercent >= 0 && DeathLossPercent <= 100, LOCTEXT("Loss", "DeathLossPercent는 0~100이어야 합니다."));
	Require(CorpseRefundPercent >= 0 && CorpseRefundPercent <= 100, LOCTEXT("Refund", "CorpseRefundPercent는 0~100이어야 합니다."));
	return OutProblems.Num() == Before;
}

FPrimaryAssetId UDigetherTuningProfile::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, GetFName());
}

FDigetherRunTuning UDigetherTuningProfile::MakeSnapshot() const
{
	FDigetherRunTuning Snapshot = Values;
	Snapshot.SourceProfile = GetPrimaryAssetId();
	return Snapshot;
}

#if WITH_EDITOR
EDataValidationResult UDigetherTuningProfile::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Result = Super::IsDataValid(Context);
	TArray<FText> Problems;
	if (!Values.CollectProblems(Problems))
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
