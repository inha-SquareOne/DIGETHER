#pragma once

#include "CoreMinimal.h"
#include "DigetherElevatorTypes.generated.h"

UENUM(BlueprintType)
enum class EDigetherElevatorPhase : uint8
{
	HubWaiting,
	InitialDescent,
	BottomWaiting,
	Ascending,
	UpperStop,
	ReturnDescent,
	FinalAscent,
	Finished
};

UENUM(BlueprintType)
enum class EDigetherElevatorWeightStage : uint8
{
	Empty,
	Light,
	Medium,
	Heavy,
	Over
};

UENUM(BlueprintType)
enum class EDigetherElevatorLoadKind : uint8
{
	Player,
	Corpse,
	CarriedItem,
	Cargo
};

UENUM(BlueprintType)
enum class EDigetherElevatorCommand : uint8
{
	Reset,
	DepartHub,
	RequestAscent,
	AdvanceTime
};

UENUM(BlueprintType)
enum class EDigetherElevatorResult : uint8
{
	Success,
	NoChange,
	ExplorationFinished,
	InvalidPhase,
	InvalidInput,
	OverCapacity,
	NotAuthority,
	NotInitialized
};

UENUM(BlueprintType)
enum class EDigetherElevatorLeverKind : uint8
{
	Ascent,
	Departure
};

UENUM(BlueprintType)
enum class EDigetherElevatorGaugePhase : uint8
{
	Idle,
	Pulling,
	Confirmed
};

UENUM(BlueprintType)
enum class EDigetherElevatorLeverResult : uint8
{
	Started,
	InProgress,
	Released,
	Confirmed,
	OverweightWarningAndRattle,
	InterruptedOverweight,
	InterruptedConditions,
	Busy,
	NotPuller,
	NotPulling,
	AlreadyConfirmed,
	InvalidPhase,
	WatchingResultCredits,
	InvalidInput,
	NotAuthority,
	NotInitialized
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherElevatorLoadEntry
{
	GENERATED_BODY()

	// Player 항목의 EntryId는 탑승자 ID다. 탑승 판정과 ID의 발급은 호출자 책임이다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	FGuid EntryId;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	EDigetherElevatorLoadKind Kind = EDigetherElevatorLoadKind::Cargo;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	FGuid OwnerId;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	float WeightKg = 0.f;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherElevatorWeightSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	double TotalWeightKg = 0.0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	EDigetherElevatorWeightStage Stage = EDigetherElevatorWeightStage::Empty;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherElevatorState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	EDigetherElevatorPhase Phase = EDigetherElevatorPhase::HubWaiting;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	int32 CompletedAscents = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	float PhaseElapsedSeconds = 0.f;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherElevatorTransition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	FDigetherElevatorState State;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	EDigetherElevatorResult Result = EDigetherElevatorResult::NoChange;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherElevatorAscentConfirmed
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	int32 TripNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	bool bFinalAscent = false;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	TArray<FGuid> PassengerIds;

	// 시체를 포함한 전체 목록을 값으로 보존한다. 플레이어·시체의 저장 무게도 0이다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	TArray<FDigetherElevatorLoadEntry> LoadEntries;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	double TotalWeightKg = 0.0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	EDigetherElevatorWeightStage WeightStage = EDigetherElevatorWeightStage::Empty;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherElevatorDepartureConfirmed
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	FGuid PullerId;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	int32 DayNumber = 0;
};

// 매니페스트와 개인 ID를 이 구조체에 추가하면 공개 복제 경계가 바뀐다.
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherElevatorPublicState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	EDigetherElevatorPhase Phase = EDigetherElevatorPhase::HubWaiting;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	int32 CompletedAscents = 0;

	// 프로토타입과 같은 위치 비율이다. 바닥은 0, 거점·상부는 1이다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	float TravelProgress = 1.f;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	EDigetherElevatorWeightStage WeightStage = EDigetherElevatorWeightStage::Empty;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherElevatorLeverPublicState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	float Progress = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	bool bWarningActive = false;
};

// 조율자가 같은 판정 시점의 값을 모아 전달한다. 다른 도메인 객체를 보관하지 않는다.
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherElevatorPullContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	EDigetherElevatorPhase ElevatorPhase = EDigetherElevatorPhase::HubWaiting;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	int32 CompletedAscents = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	int32 TripLimit = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	float CapacityKg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	int32 DayNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	bool bAnyoneWatchingResultCredits = false;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	TArray<FDigetherElevatorLoadEntry> LoadEntries;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherElevatorGaugeState
{
	GENERATED_BODY()

	UPROPERTY()
	EDigetherElevatorGaugePhase Phase = EDigetherElevatorGaugePhase::Idle;

	UPROPERTY()
	FGuid PullerId;

	UPROPERTY()
	float ElapsedSeconds = 0.f;

	UPROPERTY()
	float DurationSeconds = 0.f;

	UPROPERTY()
	int32 DepartureDayNumber = 0;

	UPROPERTY()
	bool bWarningActive = false;

	UPROPERTY()
	FDigetherElevatorAscentConfirmed Ascent;

	UPROPERTY()
	FDigetherElevatorDepartureConfirmed Departure;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherElevatorLeverFeedback
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	EDigetherElevatorLeverResult Result = EDigetherElevatorLeverResult::NotPulling;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	bool bWarning = false;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	bool bRattle = false;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherElevatorGaugeUpdate
{
	GENERATED_BODY()

	UPROPERTY()
	FDigetherElevatorGaugeState State;

	UPROPERTY()
	FDigetherElevatorLeverFeedback Feedback;
};
