#pragma once

#include "CoreMinimal.h"
#include "DigetherHubTypes.generated.h"

UENUM(BlueprintType)
enum class EDigetherHubFacilityKind : uint8
{
	Shop,
	Delivery,
	Bed,
	Altar,
	Stage
};

UENUM(BlueprintType)
enum class EDigetherHubAction : uint8
{
	Inspect,
	Purchase,
	ClaimDelivery,
	Rest,
	Stand,
	Offer,
	UnlockStage,
	SelectStage
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherHubItemStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Count = 0;
};

// 인증 키와 구분한 서버 플레이어 ID별 개인 소지품·배송 진행
USTRUCT()
struct DIGETHER_API FDigetherHubPlayerProgress
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid PlayerId;

	UPROPERTY()
	bool bStarterGrantConsidered = false;

	UPROPERTY()
	TArray<FDigetherHubItemStack> Inventory;

	UPROPERTY()
	TArray<FDigetherHubItemStack> PendingDelivery;
};

// 선택적 저장 확장, 기존 빈 저장은 카탈로그 초기값 적용
// 침대 점유·연출·목적지 잠금은 저장 대상 제외
USTRUCT()
struct DIGETHER_API FDigetherHubProgress
{
	GENERATED_BODY()

	UPROPERTY()
	bool bInitialized = false;

	UPROPERTY()
	TArray<FDigetherHubPlayerProgress> Players;

	UPROPERTY()
	TArray<FName> UnlockedStageIds;

	UPROPERTY()
	FName SelectedStageId;

	UPROPERTY()
	int32 TotalOfferingCredits = 0;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherHubOption
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName Id;

	UPROPERTY(BlueprintReadOnly)
	FText Label;

	UPROPERTY(BlueprintReadOnly)
	FText Detail;

	UPROPERTY(BlueprintReadOnly)
	int32 Cost = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly)
	EDigetherHubAction Action = EDigetherHubAction::Inspect;
};

// 요청자 Controller 전용 응답, 개인 소지품 공개 복제 제외
USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherHubView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly)
	FName FacilityId;

	UPROPERTY(BlueprintReadOnly)
	EDigetherHubFacilityKind Kind = EDigetherHubFacilityKind::Shop;

	UPROPERTY(BlueprintReadOnly)
	FText Title;

	UPROPERTY(BlueprintReadOnly)
	FText Message;

	UPROPERTY(BlueprintReadOnly)
	FText Detail;

	UPROPERTY(BlueprintReadOnly)
	int32 Balance = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly)
	bool bInteractionAllowed = false;

	UPROPERTY(BlueprintReadOnly)
	bool bResting = false;

	UPROPERTY(BlueprintReadOnly)
	bool bProgressChanged = false;

	UPROPERTY(BlueprintReadOnly)
	TArray<FDigetherHubOption> Options;
};
