#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DigetherJihunGridGraphTypes.h"
#include "DigetherJihunGridGraphBuilder.generated.h"

class ULevelStreamingDynamic;
class UDigetherJihunGridGraphRuleSet;

// 서버가 정한 생성 요청. Seed와 Revision을 한 구조체로 복제해 두 값이 따로 도착하는 경우를 막음
USTRUCT(BlueprintType)
struct FDigetherJihunGridGraphBuildRequest
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Digether|GridGraph")
	int32 Seed = 0;

	// 0 = 아직 요청 없음. 서버가 다시 생성할 때마다 1씩 증가
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Digether|GridGraph")
	int32 Revision = 0;
};

// 생성 결과를 기획 모듈 umap의 Level Instance로 불러오는 시험용 액터. 액터 위치·yaw가 던전 원점
// 서버가 Seed를 정해 복제하고, 서버·클라이언트가 각자 같은 결과를 로컬로 조립(생성기 결정성 전제)
UCLASS()
class DIGETHER_API ADigetherJihunGridGraphBuilder : public AActor
{
	GENERATED_BODY()

public:
	ADigetherJihunGridGraphBuilder();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버 전용. 새 Seed로 다시 생성하고 클라이언트에 복제. bRandomSeed면 InSeed 무시
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Digether|GridGraph")
	void RequestBuild(int32 InSeed);

	UFUNCTION(BlueprintCallable, Category = "Digether|GridGraph")
	void ClearLayout();

	const FDigetherJihunGridGraphLayout& GetLayout() const { return Layout; }
	const FDigetherJihunGridGraphBuildRequest& GetBuildRequest() const { return BuildRequest; }

#if WITH_EDITOR
	// 에디터 뷰포트에 칸 상자만 그림(모듈은 불러오지 않음)
	UFUNCTION(CallInEditor, Category = "Digether|GridGraph")
	void PreviewInEditor();

	UFUNCTION(CallInEditor, Category = "Digether|GridGraph")
	void NextSeedPreview();

	UFUNCTION(CallInEditor, Category = "Digether|GridGraph")
	void ClearPreview();
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 비우면 GleyBox 실측 카탈로그와 기본 규칙 사용. 서버·클라이언트가 같은 에셋을 가져야 같은 결과
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	TObjectPtr<UDigetherJihunGridGraphRuleSet> RuleSet;

	// bRandomSeed가 꺼져 있을 때 서버가 쓰는 Seed
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	int32 Seed = 0;

	// 서버가 시작할 때마다 무작위 Seed 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	bool bRandomSeed = false;

	// 서버 BeginPlay에서 바로 생성 요청
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph")
	bool bBuildOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Debug")
	bool bDrawLayoutOnBuild = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Debug", meta = (ClampMin = "0.0"))
	float DrawLayoutSeconds = 30.0f;

	// 미리보기·디버그 표시에서 이 층(0부터)만 그림. -1이면 전체
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Digether|GridGraph|Debug", meta = (ClampMin = "-1"))
	int32 PreviewFloor = -1;

private:
	void GatherInputs(TArray<FDigetherJihunGridGraphModuleSpec>& OutCatalog, FDigetherJihunGridGraphRules& OutRules) const;

	// BuildRequest대로 로컬 조립. 서버·클라이언트 공용
	bool BuildFromRequest();

	UFUNCTION()
	void OnRep_BuildRequest();

	UFUNCTION()
	void HandleModuleShown();

	UPROPERTY(ReplicatedUsing = OnRep_BuildRequest, VisibleInstanceOnly, Category = "Digether|GridGraph")
	FDigetherJihunGridGraphBuildRequest BuildRequest;

	// 이 머신에서 마지막으로 조립한 Revision. 같은 요청을 두 번 조립하지 않게 함
	int32 BuiltRevision = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ULevelStreamingDynamic>> LoadedModules;

	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Digether|GridGraph")
	FDigetherJihunGridGraphLayout Layout;

	// 불러오기 시간 측정용(3단계 확인 항목)
	double BuildStartSeconds = 0.0;
	int32 ShownModules = 0;
};
