#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DigetherSpiderWebActor.generated.h"

class ADigetherSpiderNestActor;
class UBoxComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EDigetherSpiderWebRemoval : uint8
{
	// 공격으로 제거, 제거 지점을 땅거미가 조사
	Attacked,
	// 붙은 지형이 굴착·붕괴로 사라짐, 감지하지 않음
	Terrain,
	RunEnded
};

// 거미줄: 평면 판(메시는 BP) + 판 주변 보이지 않는 상자로 감속·밟음·제거 판정
// 둥지가 수명 소유(땅거미가 죽어도 남음), 판정은 서버
UCLASS()
class DIGETHER_API ADigetherSpiderWebActor : public AActor
{
	GENERATED_BODY()

public:
	ADigetherSpiderWebActor();

	// 서버, 둥지가 생성 직후 호출
	void InitializeWeb(ADigetherSpiderNestActor* InNest, float InSlowMultiplier);

	// 서버, 지형이 사라졌을 때 굴착·붕괴 쪽이 호출(감지 없음)
	UFUNCTION(BlueprintCallable, Category="Digether|Spider")
	void RemoveByTerrain();

	void RemoveByRunEnd();

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 제거 연출(모든 클라이언트), BP 구현
	UFUNCTION(BlueprintImplementableEvent, Category="Digether|Spider")
	void OnWebRemoved(EDigetherSpiderWebRemoval Reason);

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastWebRemoved(EDigetherSpiderWebRemoval Reason);

	void HandlePawnEntered(APawn* Pawn);
	void Remove(EDigetherSpiderWebRemoval Reason, AController* ByController);
	void ReleaseAllSlowed();

	UPROPERTY(VisibleAnywhere, Category="Digether|Spider")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, Category="Digether|Spider")
	TObjectPtr<UStaticMeshComponent> Mesh;

	TWeakObjectPtr<ADigetherSpiderNestActor> Nest;
	float SlowMultiplier = 0.5f;
	TArray<TWeakObjectPtr<APawn>> SlowedPawns;
	bool bRemoved = false;
};
