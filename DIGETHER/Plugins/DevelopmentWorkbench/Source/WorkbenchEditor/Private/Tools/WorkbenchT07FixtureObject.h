#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WorkbenchT07FixtureObject.generated.h"

// T07 예시 전용 합성 대상. 게임 규칙·스키마가 아니며, 프로필 동작을 눈으로 확인하는 데만 쓴다.
// 자동으로 적용되지 않고, 패널의 '예시 대상 연결'을 누를 때만 임시 인스턴스로 만들어진다.
UCLASS(Transient)
class UWorkbenchT07FixtureObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "WorkbenchFixture")
	bool bShowAdvanced = false;

	UPROPERTY(EditAnywhere, Category = "WorkbenchFixture")
	int32 FixtureValue = 0;

	UPROPERTY(EditAnywhere, Category = "WorkbenchFixture")
	FString FixtureName;
};
