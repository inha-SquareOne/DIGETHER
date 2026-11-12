#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DigetherPlayerInputData.generated.h"

class UInputAction;
class UInputMappingContext;

// 플레이어 입력 액션(IA) 모음, 에셋 이름 DA_PlayerInput
// 액션 자체(IA_*)는 에디터 에셋이고, 이 데이터 에셋이 캐릭터에 한 번에 넘기는 묶음 역할을 한다
// 키 배치(IMC)는 BuildMappingContext가 코드로 만든다. 키 재설정(S31)은 엔진 Enhanced Input 사용자 설정이 이어받는다
UCLASS(BlueprintType, Const)
class DIGETHER_API UDigetherPlayerInputData : public UDataAsset
{
	GENERATED_BODY()

public:
	// 마우스 이동, 2D 축(X 요, Y 피치)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Input")
	TObjectPtr<UInputAction> LookAction;

	// WASD, 2D 축(X 좌우, Y 앞뒤)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Input")
	TObjectPtr<UInputAction> MoveAction;

	// Space
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Input")
	TObjectPtr<UInputAction> JumpAction;

	// Shift
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Input")
	TObjectPtr<UInputAction> SprintAction;

	// Ctrl
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Input")
	TObjectPtr<UInputAction> CrouchAction;

	// 좌클릭
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Digether|Input")
	TObjectPtr<UInputAction> AttackAction;

	// 액션이 모두 지정됐는지 확인하고 빠진 이름을 모아 반환한다
	bool CollectMissingActions(TArray<FName>& OutMissing) const;

	// 위 액션에 키를 배치한 매핑 컨텍스트를 만든다, 호출한 쪽이 Outer가 되어 수명을 관리한다
	UInputMappingContext* BuildMappingContext(UObject* Outer) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
