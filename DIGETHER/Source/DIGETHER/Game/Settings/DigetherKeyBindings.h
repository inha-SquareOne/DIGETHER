#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DigetherKeyBindings.generated.h"

class UEnhancedInputUserSettings;
class ULocalPlayer;

// 키 재설정 화면 한 줄, 키보드·마우스 첫 번째 칸만 다룸
USTRUCT(BlueprintType)
struct FDigetherKeyBindingRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Digether|Input")
	FName MappingName;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Input")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Input")
	FText Category;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Input")
	FKey CurrentKey;

	UPROPERTY(BlueprintReadOnly, Category="Digether|Input")
	FKey DefaultKey;
};

namespace DigetherKeyBindings
{
	// 키 지정, 같은 키를 쓰던 다른 행은 바꾸기 전 키로 교환해 빈 행을 만들지 않음
	DIGETHER_API void AssignWithSwap(TMap<FName, FKey>& Keys, FName MappingName, const FKey& NewKey);

	// 키 재설정 입력으로 받을 수 있는 키, 게임패드·터치·축·Esc 제외
	DIGETHER_API bool IsBindableKey(const FKey& Key);
}

// 엔진 Enhanced Input 사용자 설정(키 변경 저장·적용) 래퍼
UCLASS()
class DIGETHER_API UDigetherKeyBindingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static UEnhancedInputUserSettings* GetUserSettings(const ULocalPlayer* LocalPlayer);

	// DIGETHER Options의 컨텍스트를 사용자 설정에 등록, 메인 메뉴처럼 컨텍스트를 아직 안 붙인 화면에서도 목록 확보
	static void RegisterRebindableContexts(const ULocalPlayer* LocalPlayer);

	UFUNCTION(BlueprintCallable, Category="Digether|Input")
	static TArray<FDigetherKeyBindingRow> GetKeyBindingRows(const ULocalPlayer* LocalPlayer);

	// 바뀐 키만 반영 후 적용·비동기 저장, 실패한 행이 있으면 false
	static bool ApplyKeyBindings(const ULocalPlayer* LocalPlayer, const TMap<FName, FKey>& Keys);
};
