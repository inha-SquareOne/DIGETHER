#pragma once

#include "CoreMinimal.h"
#include "DigetherLifeTypes.generated.h"

UENUM(BlueprintType)
enum class EDigetherLifeState : uint8
{
	Active,
	Dead,
	Escaped
};

UENUM(BlueprintType)
enum class EDigetherDeathCause : uint8
{
	None,
	Killed,
	CollapseTimeExpired,
	CollapseRemainedAfterFinalAscent,
	CollapseEjected
};

UENUM(BlueprintType)
enum class EDigetherEscapeKind : uint8
{
	None,
	Elevator,
	Emergency
};

UENUM(BlueprintType)
enum class EDigetherLifeResult : uint8
{
	Success,
	InvalidInput,
	NotActive,
	AlreadyActive,
	NotSpectating,
	NotAuthority
};

UENUM(BlueprintType)
enum class EDigetherSpectateDirection : uint8
{
	Next,
	Previous
};

UENUM(BlueprintType)
enum class EDigetherEmergencyEscapeResult : uint8
{
	Allowed,
	CarryingRecoverable,
	Overweight,
	NotActive,
	InvalidInput
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherLifePublicState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Life")
	EDigetherLifeState Life = EDigetherLifeState::Active;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Life")
	bool bSpectating = false;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherLifePrivateState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Life")
	EDigetherDeathCause DeathCause = EDigetherDeathCause::None;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Life")
	EDigetherEscapeKind EscapeKind = EDigetherEscapeKind::None;

	// Pawn 참조 대신 키 보관, 연결 해제 중에도 같은 값 규칙 적용
	UPROPERTY(BlueprintReadOnly, Category="Digether|Life")
	FString SpectateTargetPlayerKey;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherLifeComponentState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Life")
	FDigetherLifePublicState PublicState;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Life")
	FDigetherLifePrivateState PrivateState;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherLifeUpdateResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Life")
	EDigetherLifeResult Result = EDigetherLifeResult::InvalidInput;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Life")
	FDigetherLifeComponentState State;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherLifeDropItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Life")
	FGuid InstanceId;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Life")
	bool bHidden = false;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherDeathDropPlan
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Life")
	EDigetherLifeResult Result = EDigetherLifeResult::InvalidInput;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Life")
	TArray<FGuid> InstanceIds;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Life")
	FVector Location = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherSpectateCandidate
{
	GENERATED_BODY()

	// 식별자 발급·현재 세션 목록 최신성은 조율자 책임
	UPROPERTY(BlueprintReadOnly, Category="Digether|Life")
	FString PlayerKey;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Life")
	EDigetherLifeState Life = EDigetherLifeState::Active;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherSpectateSelectionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Life")
	EDigetherLifeResult Result = EDigetherLifeResult::InvalidInput;

	// 성공이어도 관전 가능한 동료가 없으면 빈 값
	UPROPERTY(BlueprintReadOnly, Category="Digether|Life")
	FString TargetPlayerKey;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherEmergencyCarriedItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Life")
	float WeightKg = 0.f;

	// 무게 0이어도 회수품·시체이면 긴급 탈출 차단
	UPROPERTY(BlueprintReadOnly, Category="Digether|Life")
	bool bRecoverable = false;
};
