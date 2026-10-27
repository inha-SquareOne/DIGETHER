// 합성 시험 fixture. 게임 아이템 스키마를 확정하지 않는다. DefinitionId/Price/Weight/RarityId는 도구 파일럿을
// 검증하기 위한 예시 필드이며 실제 승인 스키마가 생기면 프로젝트 어댑터가 자신의 DataAsset/DataTable 타입을 매핑한다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UObject/Object.h"
#include "Core/WorkbenchScalarValue.h"
#include "WorkbenchItemFixture.generated.h"

/** DataTable 행 1개 = 합성 아이템 1개. 실제 판매·경제 데이터를 이 구조로 운영하지 않는다. */
USTRUCT(BlueprintType)
struct WORKBENCHEDITOR_API FWorkbenchSyntheticItemRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 안정적인 정의 ID. RowName과 별개이며 같은 테이블 안 중복을 검사한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workbench|Item")
	FName DefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workbench|Item")
	FString DisplayName;

	/** 자유 입력 종류 태그. 허용 값 집합은 프로젝트 정책 미결이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workbench|Item")
	FName Category;

	/** 판매 값. 0이 오류인지는 판매 정책 미결이라 검증기가 결정 대기로 표시한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workbench|Item")
	int32 Price = 0;

	/** 중량 값. 단위는 정책 미결이라 가정하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workbench|Item")
	float Weight = 0.f;

	/** 희귀도 식별자. 허용 집합은 정책 미결이라 자유 FName이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workbench|Item")
	FName RarityId;
};

/**
 * 초안 세션에서 Details 패널에 바인딩하는 transient 프록시. 원본 DataTable 행 메모리에 바인딩하지 않으며
 * 명시 적용에서만 원본에 반영한다. 원본 PostEditChange는 실행하지 않는 순수 값 보관 객체다.
 */
UCLASS(Transient)
class WORKBENCHEDITOR_API UWorkbenchItemDraftObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Workbench|Draft")
	FName DefinitionId;

	UPROPERTY(EditAnywhere, Category = "Workbench|Draft")
	FString DisplayName;

	UPROPERTY(EditAnywhere, Category = "Workbench|Draft")
	FName Category;

	UPROPERTY(EditAnywhere, Category = "Workbench|Draft")
	int32 Price = 0;

	UPROPERTY(EditAnywhere, Category = "Workbench|Draft")
	float Weight = 0.f;

	UPROPERTY(EditAnywhere, Category = "Workbench|Draft")
	FName RarityId;

	/** 이 초안이 가리키는 원본 행 이름. 편집 대상이 아니라 추적용이다. */
	UPROPERTY(VisibleAnywhere, Category = "Workbench|Draft|Source")
	FName SourceRowName;

	void CopyFromRow(FName InRowName, const FWorkbenchSyntheticItemRow& InRow);
	FWorkbenchSyntheticItemRow ToRow() const;
};

namespace WorkbenchItemFixture
{
	/** 허용 열 목록(이름·타입). T08 CSV와 편집 allowlist가 이 목록만 쓴다. */
	WORKBENCHEDITOR_API const TArray<FWorkbenchColumnSpec>& GetColumns();

	WORKBENCHEDITOR_API FWorkbenchRowValues RowToValues(const FWorkbenchSyntheticItemRow& Row);

	/** 허용 열 값만 행에 반영한다. 허용 목록에 없거나 타입이 다른 값이 있으면 false. */
	WORKBENCHEDITOR_API bool ApplyValuesToRow(FWorkbenchSyntheticItemRow& Row, const FWorkbenchRowValues& Values, FText& OutError);

	/** UTF-8 바이트 FNV-1a 32비트. 실행·프로세스와 무관하게 같은 문자열이면 같은 값이다. */
	WORKBENCHEDITOR_API uint32 Fnv1a32(const FString& Text);

	/** 해시 입력이 되는 정규 직렬화(FName은 문자열, 각 필드는 길이 접두). 고정 fixture 시험에서 그대로 비교한다. */
	WORKBENCHEDITOR_API FString SerializeRowCanonical(const FWorkbenchSyntheticItemRow& Row);
	WORKBENCHEDITOR_API FString SerializeTableCanonical(const TMap<FName, FWorkbenchSyntheticItemRow>& Rows);

	/** 행 값의 결정적 해시. FName 인덱스가 아닌 문자열 기반이라 재시작 뒤에도 같다. */
	WORKBENCHEDITOR_API uint32 ComputeRowHash(const FWorkbenchSyntheticItemRow& Row);

	/** 행 이름 순서와 무관한 테이블 전체 해시. */
	WORKBENCHEDITOR_API uint32 ComputeTableHash(const TMap<FName, FWorkbenchSyntheticItemRow>& Rows);

	/** 검증 규칙을 모두 통과해야 하는 정상 합성 행. */
	WORKBENCHEDITOR_API TArray<TPair<FName, FWorkbenchSyntheticItemRow>> MakeCleanRows();

	/** 규칙 시연용 의도적 문제 행(중복 ID, 가격 0, 음수 중량·빈 이름). */
	WORKBENCHEDITOR_API TArray<TPair<FName, FWorkbenchSyntheticItemRow>> MakeInvalidRows();

	/** 정상 + 문제 행. 게임 데이터가 아니다. */
	WORKBENCHEDITOR_API TArray<TPair<FName, FWorkbenchSyntheticItemRow>> MakeSyntheticRows();

	/** 주어진 행으로 DataTable을 만든다. Outer가 null이면 transient 패키지. */
	WORKBENCHEDITOR_API UDataTable* CreateTableFromRows(UObject* Outer, FName Name, const TArray<TPair<FName, FWorkbenchSyntheticItemRow>>& Rows, EObjectFlags Flags = RF_Transactional);

	/** 메모리 전용 합성 테이블을 만든다(저장 불가 패키지). */
	WORKBENCHEDITOR_API UDataTable* CreateSyntheticTable(UObject* Outer = nullptr);

	/** 디스크 저장 시험용 합성 DataTable 에셋 패키지 경로. */
	WORKBENCHEDITOR_API FString GetSyntheticAssetPackagePath();

	/** 합성 에셋을 새로 만들거나(이미 있으면 로드) 반환한다. 만든 직후는 저장 전(dirty) 상태다. */
	WORKBENCHEDITOR_API UDataTable* FindOrCreateSyntheticAsset(FText& OutError);
}
