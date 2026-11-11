#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/Monster/Spider/DigetherGroundSpiderTypes.h"
#include "Game/Monster/Spider/DigetherSpiderWebActor.h"
#include "DigetherSpiderNestActor.generated.h"

class ADigetherMonsterCharacter;

// 땅거미 둥지 영역(오브젝트가 아닌 영역, 기획), 거미줄 배치·보충과 수명 소유
// 서버 전용, 땅거미가 죽어도 남고 탐사 종료 때 디렉터가 정리
UCLASS(NotBlueprintable)
class DIGETHER_API ADigetherSpiderNestActor : public AActor
{
	GENERATED_BODY()

public:
	ADigetherSpiderNestActor();

	void InitializeNest(ADigetherMonsterCharacter* InSpider, const FVector& InCenter, float InRadius, const FDigetherGroundSpiderParams& Params);

	void HandleWebStepped(ADigetherSpiderWebActor* Web, APawn* Pawn);
	void HandleWebRemoved(ADigetherSpiderWebActor* Web, EDigetherSpiderWebRemoval Reason, AController* ByController);

	// 땅거미 사망 뒤 보충 중지(거미줄은 남김)
	void NotifySpiderGone();

	const FVector& GetCenter() const { return Center; }
	float GetRadius() const { return Radius; }
	int32 GetWebCount() const { return Webs.Num(); }

	// 테스트·치트용, 즉시 한 번 배치 시도
	bool TrySpawnWeb();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Refill();
	void ReportToSpider(EDigetherMonsterStimulus Cause, const FVector& Location, const APawn* ByPawn);

	TWeakObjectPtr<ADigetherMonsterCharacter> Spider;
	FVector Center = FVector::ZeroVector;
	float Radius = 0.f;
	TSubclassOf<ADigetherSpiderWebActor> WebClass;
	int32 BaseCount = 0;
	float RefillSeconds = 20.f;
	float SlowMultiplier = 0.5f;
	float SurfaceTraceDistance = 300.f;
	TArray<TWeakObjectPtr<ADigetherSpiderWebActor>> Webs;
	FTimerHandle RefillTimer;
	FRandomStream Random;
};
