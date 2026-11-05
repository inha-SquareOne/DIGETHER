#pragma once

#include "Game/Elevator/DigetherElevatorTypes.h"
#include "Game/Elevator/DigetherElevatorTuning.h"

namespace DigetherElevatorRules
{
	// ELV_ASCENT_LIMIT: 일반 상승 2회 확정(최초 하강 제외), 2만 허용
	// 변경 시 FDigetherRunTuning::ElevatorTripLimit의 ClampMin·ClampMax 메타(리터럴)도 함께 변경
	inline constexpr int32 MinTripLimit = 2;
	inline constexpr int32 MaxTripLimit = 2;

	DIGETHER_API bool IsValidTuning(const FDigetherElevatorTuning& Tuning);
	DIGETHER_API bool IsValidTripLimit(int32 TripLimit);
	DIGETHER_API bool IsValidRunParameters(float CapacityKg, int32 TripLimit, float TravelSeconds);
	DIGETHER_API bool HasOnlyValidLoadKinds(const TArray<FDigetherElevatorLoadEntry>& Entries);
	DIGETHER_API bool IsValidLeverKind(EDigetherElevatorLeverKind Kind);
	// 기획 S15: 엘리베이터 하나에 출발 레버 1개, 갱도 상승 레버 안 1개·밖 1개만 허용
	DIGETHER_API bool IsValidLeverLayout(int32 DepartureLevers, int32 AscentInsideLevers, int32 AscentOutsideLevers);
	DIGETHER_API float SanitizeWeight(const FDigetherElevatorLoadEntry& Entry);
	DIGETHER_API double SumWeight(const TArray<FDigetherElevatorLoadEntry>& Entries);
	DIGETHER_API bool IsOverCapacity(double TotalWeightKg, float CapacityKg);
	DIGETHER_API EDigetherElevatorWeightStage CalculateWeightStage(double TotalWeightKg, float CapacityKg,
		const FDigetherElevatorTuning& Tuning);
	DIGETHER_API FDigetherElevatorWeightSummary EvaluateLoad(const TArray<FDigetherElevatorLoadEntry>& Entries,
		float CapacityKg, const FDigetherElevatorTuning& Tuning);
	DIGETHER_API bool IsMoving(EDigetherElevatorPhase Phase);
	DIGETHER_API float CalculateTravelProgress(const FDigetherElevatorState& State, float TravelSeconds);
	// 문 닫힘을 시작한 대기 단계의 닫힘 시간, 거점은 HubDoorCloseSeconds, 갱도는 MineDoorCloseSeconds
	// 그 밖의 단계는 0
	DIGETHER_API float GetDoorCloseSeconds(EDigetherElevatorPhase DoorOriginPhase, const FDigetherElevatorTuning& Tuning);
	// 문 닫힘 중 0~1, 다른 단계·잘못된 입력은 0
	DIGETHER_API float CalculateDoorCloseProgress(const FDigetherElevatorState& State, const FDigetherElevatorTuning& Tuning);
	// 문 닫힘 시간이 다 지나 탑승 목록을 고정할 순간인지 판정
	DIGETHER_API bool IsDoorFullyClosed(const FDigetherElevatorState& State, const FDigetherElevatorTuning& Tuning);
	// 거점 게이지 완료 후에는 출발 취소 불가, 문 닫힘 중 무게 초과 재개방은 갱도 상승 문에만 적용
	DIGETHER_API bool ShouldReopenDoorForOverweight(const FDigetherElevatorState& State, double TotalWeightKg, float CapacityKg);
	// 출발 레버 조작 중 경고 종류, 4일차(마감일)는 Deadline, 그 밖의 일차는 Standard, 출발 조작이 아니면 None
	DIGETHER_API EDigetherDepartureWarningKind CalculateDepartureWarning(const FDigetherElevatorGaugeState& State);
	// 중간 상태 관측 보장을 위해 호출당 시간 전이 1회만 허용, 남은 시간 폐기
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
