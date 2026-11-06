#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DigetherDainCharacter.generated.h"
class UCameraComponent;

// Disposable single-player input adapter. Not the DIGETHER common Character.
UCLASS()
class ADigetherDainCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	ADigetherDainCharacter();
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
	bool DigOnce();
	bool TraceTool(FHitResult& OutHit) const;
	UPROPERTY(EditAnywhere, Category="Lab|Tool", meta=(ClampMin="50", ClampMax="125")) float BrushRadius = 125.f;
	UPROPERTY(EditAnywhere, Category="Lab|Tool", meta=(ClampMin="25", ClampMax="125")) float SoilBrushRadius = 75.f;
	UPROPERTY(EditAnywhere, Category="Lab|Tool", meta=(ClampMin="0.15")) float HitInterval = .35f;
private:
	void StartDig();
	void StopDig();
	void LookYaw(float Value);
	void LookPitch(float Value);
	UPROPERTY() TObjectPtr<UCameraComponent> Camera;
	bool bDigHeld = false;
	double LastHitTime = -100.0;
};

