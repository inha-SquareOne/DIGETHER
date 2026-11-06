#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DigetherFloorCoinActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

// 목표 초과로 정산기 앞 바닥에 떨어진 금화 더미(기획 S24), 누구나 E로 주워 자기 잔고에 추가
// 위치·존재만 복제, 금액·식별자·소비 표시는 서버 전용(클라이언트는 금액 모름)
// 생성·합치기·줍기는 GameMode가 서버에서만 처리, 정산기당 더미 하나로 합산
UCLASS()
class DIGETHER_API ADigetherFloorCoinActor : public AActor
{
	GENERATED_BODY()

public:
	ADigetherFloorCoinActor();

	// 서버에서 스폰 직후 1회 호출, 금액 1 미만·무효 ID·정산기 ID 없음이면 false
	bool InitializeServerState(const FGuid& InCoinId, FName InStationId, int32 InAmountCredits);
	// 소비됐거나 int32 초과 시 false(포화로 돈 소실 방지)
	bool AddAmount(int32 Credits);
	// 합치기 되돌리기 전용, 남은 금액이 1 미만이 되면 false
	bool RemoveAmount(int32 Credits);

	// 줍기 판정 시작 전에 표시, 같은 처리 시점의 두 번째 줍기 차단
	void MarkConsumed() { bConsumed = true; }
	void ClearConsumed() { bConsumed = false; }

	FGuid GetCoinId() const { return CoinId; }
	FName GetStationId() const { return StationId; }
	int32 GetAmountCredits() const { return AmountCredits; }
	bool IsConsumed() const { return bConsumed; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Deposit", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Deposit", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> CoinMesh;

	// 서버 전용 값, UPROPERTY·복제 미노출
	FGuid CoinId;
	FName StationId;
	int32 AmountCredits = 0;
	bool bConsumed = false;
};
