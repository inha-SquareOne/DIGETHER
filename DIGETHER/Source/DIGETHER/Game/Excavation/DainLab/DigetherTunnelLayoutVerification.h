#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DigetherTunnelLayoutVerification.generated.h"

class ADigetherExcavationLabCharacter;
class ADigetherDiggableSoil;

// 신규 개인 레벨 전용 자동 검증, -TunnelVerify에서만 이동·굴착·종료 수행
UCLASS()
class ADigetherTunnelLayoutVerification : public AActor
{
	GENERATED_BODY()
public:
	ADigetherTunnelLayoutVerification();
	virtual void Tick(float DeltaSeconds) override;
protected:
	virtual void BeginPlay() override;
private:
	bool MoveTo(FVector Target, bool bDig = false);
	void Advance();
	void Finish(bool bPassed, const FString& Reason);
	TWeakObjectPtr<ADigetherExcavationLabCharacter> Character;
	TWeakObjectPtr<ADigetherDiggableSoil> Shortcut;
	TWeakObjectPtr<ADigetherDiggableSoil> Pit;
	int32 Stage = 0;
	int32 Waypoint = 0;
	float StageSeconds = 0;
	float TotalSeconds = 0;
	float NextProbeSeconds = 0;
};
