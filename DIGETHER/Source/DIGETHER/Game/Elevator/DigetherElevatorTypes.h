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
	Finished,
	// 기획 A3: 게이지 완료 뒤 문이 닫히는 대기 단계다. 기존 값의 숫자를 바꾸지 않도록 끝에 둔다.
	DoorClosing
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
	AdvanceTime,
	BeginDoorClose,
	ReopenDoor
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
	NotInitialized,
	// 문 닫힘 시간이 이번 호출에서 끝났다. 탑승 목록 고정·운행 시작은 조율자가 이어서 한다.
	DoorClosed
};

UENUM(BlueprintType)
enum class EDigetherElevatorLeverKind : uint8
{
	Ascent,
	Departure
};

// 기획 S15·T93: 갱도 상승 레버는 엘리베이터 안과 밖에 하나씩 둔다. 두 레버는 같은 규칙으로 동작하고,
// 밖의 레버를 당긴 사람은 타지 않으므로 화물만 올려 보낼 수 있다. 출발 레버는 이 값을 쓰지 않는다.
UENUM(BlueprintType)
enum class EDigetherElevatorLeverPlacement : uint8
{
	Inside,
	Outside
};

// 거점 출발 레버를 당기는 동안의 경고 연출 종류다. 연출(조명·소리·문구 대상)은 BP가 정한다.
UENUM(BlueprintType)
enum class EDigetherDepartureWarningKind : uint8
{
	None,
	// 1~3일차: 주황색 형광등·비프음, 팀원에게 '승강기가 곧 출발합니다'.
	Standard,
	// 할당량 미달 4일차(마감일): 빨간 경고등·사이렌, 레버 조작자에게 '현재 할당량 마감일입니다!'.
	Deadline
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
	// 사용 안 함(2026-11-05): 크레딧 중 출발 차단은 게임 오버 단계가 연결층에서 막는다. 값 순서 유지를 위해 남긴다.
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

	// DoorClosing이 시작된 대기 단계(HubWaiting·BottomWaiting)다. 재개방·출발 방향을 정한다.
	// DoorClosing이 아니면 기본값 HubWaiting으로 둔다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	EDigetherElevatorPhase DoorOriginPhase = EDigetherElevatorPhase::HubWaiting;
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

	// 문 연출용 닫힘 비율이다. 열림은 0, 완전히 닫힘은 1이며 DoorClosing이 아니면 0이다.
	// TravelProgress와 같이 서버가 진행할 때마다 값으로 복제한다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	float DoorCloseProgress = 0.f;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherElevatorLeverPublicState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	float Progress = 0.f;

	// 4일차 마감 경고(WarningKind == Deadline)일 때만 참이다. 기존 BP 호환을 위해 남긴다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	bool bWarningActive = false;

	// 출발 레버를 당기는 동안의 경고 종류다. 게이지 완료·해제·상승 레버는 None이다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	EDigetherDepartureWarningKind WarningKind = EDigetherDepartureWarningKind::None;
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

	// 할당량 기간 세대 번호다. 출발 레버 조작 중 목표 달성으로 기간이 바뀌었는지 판정한다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	int32 PeriodGeneration = 0;

	// 같은 엘리베이터의 같은 종류 다른 레버가 진행 중이거나 확정된 상태다(S15 안팎 레버 동시 입력은 한 건만 받음).
	// 먼저 시작한 레버만 진행하고, 이 레버의 시작 요청은 Busy로 거절한다.
	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	bool bOtherLeverOccupied = false;

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

	// 출발 레버를 잡기 시작한 할당량 기간 세대 번호다. 출발 게이지가 아니면 0이다.
	UPROPERTY()
	int32 DeparturePeriodGeneration = 0;

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
