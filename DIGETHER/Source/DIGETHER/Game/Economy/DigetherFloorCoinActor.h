#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DigetherFloorCoinActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

// 목표를 넘겨 정산기 앞 바닥에 떨어진 금화 더미다(기획 S24). 누구나 E로 주워 자기 잔고에 넣는다.
// 위치·존재만 복제한다. 금액·식별자·소비 표시는 서버 전용이라 클라이언트는 금액을 알 수 없다.
// 생성·합치기·줍기는 GameMode가 서버에서만 한다. 정산기당 더미 하나에 합산한다.
UCLASS()
class DIGETHER_API ADigetherFloorCoinActor : public AActor
{
	GENERATED_BODY()

public:
	ADigetherFloorCoinActor();

	// 서버에서 스폰 직후 한 번만 부른다. 금액 1 이상·유효 ID·정산기 ID가 아니면 false다.
	bool InitializeServerState(const FGuid& InCoinId, FName InStationId, int32 InAmountCredits);
	// 소비됐거나 int32를 넘으면 false다(포화로 돈을 버리지 않는다).
	bool AddAmount(int32 Credits);
	// 합치기 되돌리기에만 쓴다. 남은 금액이 1 미만이 되면 false다.
	bool RemoveAmount(int32 Credits);

	// 줍기 판정을 시작하기 전에 표시해 같은 처리 시점의 두 번째 줍기를 막는다.
	void MarkConsumed() { bConsumed = true; }
	void ClearConsumed() { bConsumed = false; }

	FGuid GetCoinId() const { return CoinId; }
	FName GetStationId() const { return StationId; }
	int32 GetAmountCredits() const { return AmountCredits; }
	bool IsConsumed() const { return bConsumed; }

	UStaticMeshComponent* GetCoinMesh() const { return CoinMesh; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Deposit", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Digether|Deposit", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> CoinMesh;

	// 서버 전용 값이다. UPROPERTY·복제로 노출하지 않는다.
	FGuid CoinId;
	FName StationId;
	int32 AmountCredits = 0;
	bool bConsumed = false;
};
