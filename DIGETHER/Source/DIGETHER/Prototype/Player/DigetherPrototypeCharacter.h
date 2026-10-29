#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputCoreTypes.h"
#include "Prototype/DigetherPrototypeTypes.h"
#include "DigetherPrototypeCharacter.generated.h"

class UCameraComponent;
class USpotLightComponent;
class USceneComponent;
class UStaticMeshComponent;
class UInputComponent;
class ADigetherPrototypePlayerController;

// 그레이박스 시험용 플레이어 캐릭터. 1인칭 카메라 + 원격 관전용 primitive 메시만 제공하며
// 아트·애니메이션은 본 계약 밖이다. 이동 복제는 CharacterMovementComponent 기본 동작을 재사용한다.
UCLASS()
class DIGETHER_API ADigetherPrototypeCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADigetherPrototypeCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void ResetLocalInput();
	void PlayConfirmedDigSwing();
	bool GetPrototypeAimOrigin(FVector& OutOrigin) const;

	UFUNCTION(BlueprintPure, Category = "Prototype")
	int32 GetSelectedSlot() const { return SelectedSlot; }

	UFUNCTION(BlueprintPure, Category = "Prototype")
	bool IsSprinting() const { return bIsSprinting; }

	UFUNCTION(BlueprintPure, Category = "Prototype")
	float GetStamina() const { return Stamina; }

	UFUNCTION(BlueprintPure, Category = "Prototype")
	float GetStaminaFraction() const { return FMath::Clamp(Stamina / FMath::Max(1.f, MaxStamina), 0.f, 1.f); }

	UFUNCTION(BlueprintPure, Category = "Prototype")
	bool IsHeadlampOn() const { return bHeadlampOn; }

	// 아래 값은 모두 시험 픽스처이며 최종 기획 수치가 아니다.
	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Fixture")
	float WalkSpeed = 350.f;

	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Fixture")
	float SprintSpeed = 600.f;

	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Fixture")
	float MaxStamina = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Fixture")
	float StaminaDrainPerSecond = 20.f;

	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Fixture")
	float StaminaRegenPerSecond = 12.f;

	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Fixture")
	float InteractTraceDistance = 350.f;

private:
	UPROPERTY(VisibleAnywhere, Category = "Prototype|Components")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, Category = "Prototype|Components")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Prototype|Components")
	TObjectPtr<USpotLightComponent> Headlamp;

	UPROPERTY(VisibleAnywhere, Category = "Prototype|Components")
	TObjectPtr<USceneComponent> ToolPivot;

	UPROPERTY(VisibleAnywhere, Category = "Prototype|Components")
	TObjectPtr<UStaticMeshComponent> ToolHandle;

	UPROPERTY(VisibleAnywhere, Category = "Prototype|Components")
	TObjectPtr<UStaticMeshComponent> ToolBlade;

	void MoveForward(float Value);
	void MoveRight(float Value);
	void MoveKeyPressed(FKey Key);
	void MoveKeyReleased(FKey Key);
	void LookYaw(float Value);
	void LookPitch(float Value);
	void StartSprint();
	void StopSprint();
	void StartCrouchPrototype();
	void StopCrouchPrototype();
	void JumpPrototype();
	void Interact();
	void AttackOrDig();
	void InteractAlternate();
	void CancelInteract();
	void ReleaseInteract();
	void ReleaseAttack();
	void ReleaseAlternate();
	void ReleaseInteractionKey(const FKey& Key);
	void SelectSlot1();
	void SelectSlot2();
	void SelectSlot3();
	void SelectSlot4();
	void SelectSlot5();
	void ToggleHeadlampInput();
	void DropSelected();
	void SelfEscape();
	void SaveFixture();
	void LoadFixture();

	UFUNCTION(Server, Reliable)
	void ServerSetSprinting(bool bSprint);

	UFUNCTION(Server, Reliable)
	void ServerSetHeadlamp(bool bOn);

	UFUNCTION()
	void OnRep_Headlamp();

	void ApplyHeadlampVisual(bool bOn) const;
	void UpdateAimTrace();
	bool IsLifeRestricted() const;
	float MovementSpeed(bool bSprint) const;
	ADigetherPrototypePlayerController* GetPrototypeController() const;
	static FString DescribeAimTarget(const AActor* Target);

	int32 SelectedSlot = 1;
	UPROPERTY(Replicated)
	float Stamina = 100.f;
	bool bIsSprinting = false;
	bool bWantsSprint = false;
	FKey ActiveInteractionKey;
	TSet<FKey> HeldMoveKeys;
	bool bHoldingDig = false;
	double NextDigInputTime = 0.0;
	float ToolSwingRemaining = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_Headlamp)
	bool bHeadlampOn = false;
};
