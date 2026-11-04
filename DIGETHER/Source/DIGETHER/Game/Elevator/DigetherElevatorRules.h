#pragma once

#include "Game/Elevator/DigetherElevatorTypes.h"
#include "Game/Elevator/DigetherElevatorTuning.h"

namespace DigetherElevatorRules
{
	DIGETHER_API bool IsValidTuning(const FDigetherElevatorTuning& Tuning);
	DIGETHER_API bool IsValidRunParameters(float CapacityKg, int32 TripLimit, float TravelSeconds);
	DIGETHER_API float SanitizeWeight(const FDigetherElevatorLoadEntry& Entry);
	DIGETHER_API double SumWeight(const TArray<FDigetherElevatorLoadEntry>& Entries);
	DIGETHER_API bool IsOverCapacity(double TotalWeightKg, float CapacityKg);
	DIGETHER_API EDigetherElevatorWeightStage CalculateWeightStage(double TotalWeightKg, float CapacityKg,
		const FDigetherElevatorTuning& Tuning);
	DIGETHER_API FDigetherElevatorWeightSummary EvaluateLoad(const TArray<FDigetherElevatorLoadEntry>& Entries,
		float CapacityKg, const FDigetherElevatorTuning& Tuning);
	DIGETHER_API bool IsMoving(EDigetherElevatorPhase Phase);
	DIGETHER_API float CalculateTravelProgress(const FDigetherElevatorState& State, float TravelSeconds);
	// 중간 상태의 관측을 보장하려고 호출당 시간 전이를 한 번만 허용하고 남은 시간은 버린다.
	DIGETHER_API FDigetherElevatorTransition Transition(const FDigetherElevatorState& State,
		EDigetherElevatorCommand Command, float DeltaSeconds, int32 TripLimit, float TravelSeconds,
		const FDigetherElevatorTuning& Tuning);
	DIGETHER_API FDigetherElevatorAscentConfirmed CaptureAscent(int32 CompletedAscents, int32 TripLimit,
		const TArray<FDigetherElevatorLoadEntry>& Entries, float CapacityKg, const FDigetherElevatorTuning& Tuning);
	DIGETHER_API EDigetherElevatorLeverResult CanUseDeparture(const FDigetherElevatorPullContext& Context);
	DIGETHER_API float CalculateDepartureDuration(int32 DayNumber, const FDigetherElevatorTuning& Tuning);
	DIGETHER_API float CalculateGaugeProgress(const FDigetherElevatorGaugeState& State);
	DIGETHER_API FDigetherElevatorGaugeUpdate BeginPull(const FDigetherElevatorGaugeState& State,
		EDigetherElevatorLeverKind Kind, const FGuid& PullerId, const FDigetherElevatorPullContext& Context,
		const FDigetherElevatorTuning& Tuning);
	DIGETHER_API FDigetherElevatorGaugeUpdate ReleasePull(const FDigetherElevatorGaugeState& State, const FGuid& PullerId);
	DIGETHER_API FDigetherElevatorGaugeUpdate AdvancePull(const FDigetherElevatorGaugeState& State,
		EDigetherElevatorLeverKind Kind, float DeltaSeconds, const FDigetherElevatorPullContext& Context,
		const FDigetherElevatorTuning& Tuning);
}
