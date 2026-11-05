#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DigetherHubCatalog.generated.h"

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherHubOffer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1"))
	int32 PriceCredits = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1", ClampMax="1000"))
	int32 Quantity = 1;
};

USTRUCT(BlueprintType)
struct DIGETHER_API FDigetherHubStage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName StageId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	// 요청자 잔고에서 차감하는 팀 공용 해금 비용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0"))
	int32 UnlockCostCredits = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bInitiallyUnlocked = false;
};

// 최종 밸런스와 구분한 거점 시험 수치·표시값 정의 에셋
UCLASS(BlueprintType)
class DIGETHER_API UDigetherHubCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	bool IsCatalogValid() const;
	const FDigetherHubOffer* FindOffer(FName ItemId) const;
	const FDigetherHubStage* FindStage(FName StageId) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trial Hub")
	TArray<FDigetherHubOffer> Offers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trial Hub")
	TArray<FDigetherHubStage> Stages;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trial Hub", meta=(ClampMin="1"))
	int32 OfferingCostCredits = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trial Hub", meta=(ClampMin="0.1"))
	float OfferingPulseSeconds = 10.f;

	// 개발 빌드 전용 시험 지원금, Shipping 지급 제외
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trial Hub", meta=(ClampMin="0"))
	int32 TrialStarterCredits = 500;
};
