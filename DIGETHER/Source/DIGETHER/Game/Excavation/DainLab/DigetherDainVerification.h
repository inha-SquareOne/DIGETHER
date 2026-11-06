#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DigetherDainVerification.generated.h"
class ADigetherDainCharacter;
class ADigetherDainSoilWall;

// Test harness only: drives the same character, trace and dig functions as a human.
UCLASS()
class ADigetherDainVerification : public AActor
{
	GENERATED_BODY()
public:
	ADigetherDainVerification();
	virtual void Tick(float DeltaSeconds) override;
private:
	void Finish(bool bPassed, const FString& Reason);
	uint32 GetWallRevision() const;
	uint32 GetWallHash() const;
	int32 Stage = 0;
	float StageSeconds = 0;
	float TotalSeconds = 0;
	float BlockedX = 0;
	float AfterOneHitX = 0;
	bool bProtectedRejected = false;
	UPROPERTY() TObjectPtr<ADigetherDainCharacter> Character;
	UPROPERTY() TObjectPtr<AActor> Wall;
};

