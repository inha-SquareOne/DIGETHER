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
	// 기획 A3: 게이지 완료 뒤 문 닫힘 대기 단계, 기존 값 번호 유지를 위해 끝에 배치
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
	// 문 닫힘 시간이 이번 호출에서 종료, 탑승 목록 고정·운행 시작은 조율자가 이어서 처리
	DoorClosed
};

UENUM(BlueprintType)
enum class EDigetherElevatorLeverKind : uint8
{
	Ascent,
	Departure
};

// 기획 S15·T93: 갱도 상승 레버는 엘리베이터 안·밖에 하나씩, 두 레버 모두 같은 규칙
// 밖 레버를 당긴 사람은 타지 않으므로 화물만 올려 보내기 가능, 출발 레버는 이 값 미사용
UENUM(BlueprintType)
enum class EDigetherElevatorLeverPlacement : uint8
{
	Inside,
	Outside
};

// 거점 출발 레버를 당기는 동안의 경고 연출 종류, 연출(조명·소리·문구 대상)은 BP 결정
UENUM(BlueprintType)
enum class EDigetherDepartureWarningKind : uint8
{
	None,
	// 1~3일차: 주황색 형광등·비프음, 팀원에게 '승강기가 곧 출발합니다'
	Standard,
	// 할당량 미달 4일차(마감일): 빨간 경고등·사이렌, 레버 조작자에게 '현재 할당량 마감일입니다!'
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
	// 사용 안 함(2026-11-05): 크레딧 중 출발 차단은 게임 오버 단계가 연결층에서 처리, 값 순서 유지용
	WatchingResultCredits,
	InvalidInput,
	NotAuthority,
	NotInitialized
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherElevatorLoadEntry
{
	GENERATED_BODY()

	// Player 항목의 EntryId는 탑승자 ID, 탑승 판정·ID 발급은 호출자 책임
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

	// DoorClosing을 시작한 대기 단계(HubWaiting·BottomWaiting), 재개방·출발 방향 결정
	// DoorClosing이 아니면 기본값 HubWaiting
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

	// 시체 포함 전체 목록을 값으로 보존, 플레이어·시체 저장 무게는 0
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

// 매니페스트·개인 ID를 추가하면 공개 복제 경계가 바뀜
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherElevatorPublicState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	EDigetherElevatorPhase Phase = EDigetherElevatorPhase::HubWaiting;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	int32 CompletedAscents = 0;

	// 프로토타입과 같은 위치 비율, 바닥 0, 거점·상부 1
	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	float TravelProgress = 1.f;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	EDigetherElevatorWeightStage WeightStage = EDigetherElevatorWeightStage::Empty;

	// 문 연출용 닫힘 비율, 열림 0·완전히 닫힘 1, DoorClosing이 아니면 0
	// TravelProgress처럼 서버 진행마다 값으로 복제
	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	float DoorCloseProgress = 0.f;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherElevatorLeverPublicState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	float Progress = 0.f;

	// 4일차 마감 경고(WarningKind == Deadline)일 때만 true, 기존 BP 호환용
	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	bool bWarningActive = false;

	// 출발 레버를 당기는 동안의 경고 종류, 게이지 완료·해제·상승 레버는 None
	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	EDigetherDepartureWarningKind WarningKind = EDigetherDepartureWarningKind::None;
};

// 조율자가 같은 판정 시점의 값을 모아 전달, 다른 도메인 객체 비보관
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

	// 할당량 기간 세대 번호, 출발 레버 조작 중 목표 달성으로 기간이 바뀌었는지 판정
	UPROPERTY(BlueprintReadOnly, Category="Digether|Elevator")
	int32 PeriodGeneration = 0;

	// 같은 엘리베이터·같은 종류의 다른 레버가 진행 중이거나 확정된 상태(S15 안팎 레버 동시 입력은 한 건만 수락)
	// 먼저 시작한 레버만 진행, 이 레버의 시작 요청은 Busy로 거절
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

	// 출발 레버를 잡기 시작한 할당량 기간 세대 번호, 출발 게이지가 아니면 0
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
