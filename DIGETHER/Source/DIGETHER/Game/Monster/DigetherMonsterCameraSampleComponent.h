#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Monster/DigetherMonsterTypes.h"
#include "DigetherMonsterCameraSampleComponent.generated.h"

// PlayerController에 부착, 로컬 카메라 위치·방향·유효 시야각을 서버 감각 서브시스템에 전송
// 엔진 ServerUpdateCamera는 Pawn이 움직일 때만 보내 제자리 시선 변경을 놓쳐 쓰지 않음(설계 3.6)
UCLASS(ClassGroup=(Digether), meta=(BlueprintSpawnableComponent))
class DIGETHER_API UDigetherMonsterCameraSampleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherMonsterCameraSampleComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 로컬 카메라로 표본 생성, 카메라가 없으면 거짓
	bool BuildLocalSample(FDigetherCameraSample& OutSample) const;

	// 회전 변화가 이 각도 이상이면 즉시 전송해야 하는지(순수 판정, 테스트용 공개)
	static bool ShouldResend(const FRotator& Previous, const FRotator& Current, float ThresholdDegrees);

private:
	UFUNCTION(Server, Unreliable)
	void ServerSubmitCameraSample(const FDigetherCameraSample& Sample);

	void SendSample();

	float SinceLastSend = 0.f;
	uint32 NextSequence = 1;
	FRotator LastSentRotation = FRotator::ZeroRotator;
	bool bHasSent = false;
};
