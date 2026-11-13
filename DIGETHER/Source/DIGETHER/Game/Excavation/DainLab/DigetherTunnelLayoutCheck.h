#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DigetherTunnelLayoutCheck.generated.h"

class APlayerStart;
class ATargetPoint;

// 개인 갱도 레벨의 필수 지점만 검사, 충돌·런·네트워크 준비 판정과 분리
UCLASS()
class DIGETHER_API ADigetherTunnelLayoutCheck : public AActor
{
	GENERATED_BODY()

public:
	ADigetherTunnelLayoutCheck();

	// 같은 레벨의 PlayerStart와 입구 표식 개수 검사, 모두 하나일 때만 참조 확정
	UFUNCTION(BlueprintCallable, Category = "Digether|Tunnel")
	bool CheckLayout();

	// 에디터 버튼은 반환값 없는 함수만 지원, 결과는 CheckMessage에 표시
	UFUNCTION(CallInEditor, Category = "Digether|Tunnel")
	void ValidateLayout() { CheckLayout(); }

	// TargetPoint의 Actor Tags에 지정할 개인 시험용 표식, 공용 GameplayTag 아님
	static const FName EntranceTag;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Digether|Tunnel")
	FText CheckMessage;

	// 성공한 마지막 검사 결과, 실패 시 둘 다 해제해 이전 지점 재사용 방지
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Digether|Tunnel")
	TObjectPtr<APlayerStart> StartPoint;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Digether|Tunnel")
	TObjectPtr<ATargetPoint> DungeonEntrance;

protected:
	virtual void BeginPlay() override;
};
