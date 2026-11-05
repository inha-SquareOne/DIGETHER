#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DigetherPrototypeTypes.generated.h"

class APlayerController;

// 시험용 상태. 게임 디자인의 확정 수치나 서비스 정책이 아니다.
UENUM(BlueprintType)
enum class EPrototypePhase : uint8
{
	Base,
	Expedition,
	GameOver,
	Returning
};

UENUM(BlueprintType)
enum class EPrototypeLife : uint8
{
	Active,
	Buried,
	Dead,
	Escaped,
	InTransit
};

UENUM(BlueprintType)
enum class EPrototypeElevatorState : uint8
{
	Ground,
	Ascending,
	Top,
	Descending,
	FinalAscending
};

USTRUCT(BlueprintType)
struct FPrototypeItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	FGuid Id;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	int32 Value = 0;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	float WeightKg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	bool bHidden = false;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	bool bTwoHanded = false;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	bool bEquipment = false;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	bool bEmergencyDevice = false;
};

// PlayerState의 소유자 전용 복제 데이터. 다른 플레이어에게 이 구조체를 보내지 않는다.
USTRUCT(BlueprintType)
struct FPrototypePrivateState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	int32 Money = 0;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	int32 WoodCount = 2;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	float Health = 100.f;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	EPrototypeLife Life = EPrototypeLife::Active;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	int32 BurialLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	TArray<FPrototypeItem> Items;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	int32 LastPayout = 0;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	float HoldProgress = 0.f;
};

// GameState의 전체 참가자에게 공개되는 시험용 상태.
USTRUCT(BlueprintType)
struct FPrototypePublicState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	EPrototypePhase Phase = EPrototypePhase::Base;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	int32 Day = 1;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	int32 QuotaTarget = 100;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	int32 QuotaProgress = 0;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	int32 RunNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	FGuid RunId;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	int32 ElevatorTrips = 0;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	int32 MaxElevatorTrips = 2;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	EPrototypeElevatorState ElevatorState = EPrototypeElevatorState::Ground;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	float ElevatorProgress = 0.f;

	// 정확한 킬로그램이 아니라 공개용 시험 단계만 표시한다.
	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	int32 ElevatorWeightStage = 0;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	float RemainingSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Prototype")
	FString Message;
};

// 서버에서만 호출. 구현체는 서버 자신의 설정과 GameMode 검증을 이용한다.
UINTERFACE(MinimalAPI)
class UDigetherPrototypeInteractable : public UInterface
{
	GENERATED_BODY()
};

class DIGETHER_API IDigetherPrototypeInteractable
{
	GENERATED_BODY()

public:
	virtual bool InteractPrototype(APlayerController* Caller, bool bAlternate, FString& OutMessage) = 0;
};
