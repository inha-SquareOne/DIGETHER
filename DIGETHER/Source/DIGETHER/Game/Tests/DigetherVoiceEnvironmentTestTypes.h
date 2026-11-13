#pragma once

#include "CoreMinimal.h"
#include "Game/Core/DigetherMonsterTargetInterface.h"
#include "Game/Framework/DigetherTestPawn.h"
#include "DigetherVoiceEnvironmentTestTypes.generated.h"

class ADigetherZoneVolume;

// 자동 검사·별도 프로세스 음성 시험용, 정식 매몰 게임 상태가 아님
// 실행 콘솔과 생성 경로는 WITH_DEV_AUTOMATION_TESTS에서만 제공
UCLASS(NotBlueprintable, Transient)
class ADigetherVoiceEnvironmentTestPawn : public ADigetherTestPawn, public IDigetherMonsterTargetInterface
{
	GENERATED_BODY()

public:
	virtual void AddMoveSpeedMultiplier(const UObject* Source, float Multiplier) override {}
	virtual void RemoveMoveSpeedMultiplier(const UObject* Source) override {}
	virtual bool IsBuriedForMonster() const override { return bTestBuried; }

	void SetTestBuried(bool bBuried) { bTestBuried = HasAuthority() && bBuried; }
	void KeepOriginalPawn(APawn* Pawn);
	void RestoreOriginalPawn();

private:
	bool bTestBuried = false;
	bool bOriginalHidden = false;
	bool bOriginalCollision = false;

	UPROPERTY(Transient)
	TObjectPtr<APawn> OriginalPawn;
};

#if WITH_DEV_AUTOMATION_TESTS
namespace DigetherVoiceEnvironmentTest
{
	ADigetherZoneVolume* CreateDungeon(UWorld* World, const FVector& Location);
}
#endif
