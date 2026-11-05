#pragma once

#include "CoreMinimal.h"
#include "Game/Life/DigetherLifeTypes.h"
#include "DigetherRunTypes.generated.h"

UENUM(BlueprintType)
enum class EDigetherRunResult : uint8
{
	Success,
	InvalidInput,
	NotAuthority,
	NotInProgress,
	AlreadyInProgress,
	AlreadyEnded
};

UENUM(BlueprintType)
enum class EDigetherRunPhase : uint8
{
	NotStarted,
	InProgress,
	Ended
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherRunPublicState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run")
	EDigetherRunPhase Phase = EDigetherRunPhase::NotStarted;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run")
	int32 AftershockStage = 0;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run")
	bool bGameOver = false;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run")
	bool bPaymentDeadline = false;
};

UENUM(BlueprintType)
enum class EDigetherRunTerminationTrigger : uint8
{
	TimeExpired,
	FinalAscentCompleted
};

UENUM(BlueprintType)
enum class EDigetherRunElevatorState : uint8
{
	StoppedInMine,
	InTransit,
	AtBase
};

UENUM(BlueprintType)
enum class EDigetherRunPlayerLocation : uint8
{
	Elevator,
	MineOrDungeon,
	Base
};

UENUM(BlueprintType)
enum class EDigetherRunPlayerOutcome : uint8
{
	Returned,
	SurvivedInTransit,
	MissingDeath,
	Unchanged
};

UENUM(BlueprintType)
enum class EDigetherRunElevatorAction : uint8
{
	NormalReturn,
	EmptyAscentAfterEjection,
	Unchanged
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherRunPlayerInput
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run")
	FString PlayerKey;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run")
	EDigetherLifeState Life = EDigetherLifeState::Active;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run")
	EDigetherRunPlayerLocation Location = EDigetherRunPlayerLocation::MineOrDungeon;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherRunTerminationInput
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run")
	EDigetherRunTerminationTrigger Trigger = EDigetherRunTerminationTrigger::TimeExpired;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run")
	EDigetherRunElevatorState ElevatorState = EDigetherRunElevatorState::StoppedInMine;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run")
	bool bElevatorOverweight = false;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run")
	TArray<FDigetherRunPlayerInput> Players;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherRunPlayerOutcome
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run")
	FString PlayerKey;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run")
	EDigetherRunPlayerOutcome Outcome = EDigetherRunPlayerOutcome::Unchanged;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run")
	EDigetherDeathCause DeathCause = EDigetherDeathCause::None;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherRunTerminationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run")
	EDigetherRunResult Result = EDigetherRunResult::InvalidInput;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run")
	EDigetherRunElevatorAction ElevatorAction = EDigetherRunElevatorAction::Unchanged;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Run")
	bool bEjectCargo = false;

	// 입력과 같은 순서, 식별자 검증 실패 시 일부만 처리하지 않고 배열 전체를 비움
	UPROPERTY(BlueprintReadOnly, Category="Digether|Run")
	TArray<FDigetherRunPlayerOutcome> Players;
};

struct DIGETHER_API FDigetherRunTimeResult
{
	EDigetherRunResult Result = EDigetherRunResult::InvalidInput;
	double RemainingSeconds = 0.0;
	bool bExpired = false;
};

struct DIGETHER_API FDigetherRunAftershockResult
{
	EDigetherRunResult Result = EDigetherRunResult::InvalidInput;
	int32 Stage = 0;
	double IntervalSeconds = 0.0;
};

struct DIGETHER_API FDigetherRunQuotaResult
{
	EDigetherRunResult Result = EDigetherRunResult::InvalidInput;
	bool bPaymentDeadline = false;
	bool bGameOver = false;
};
