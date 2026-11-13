#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/Character.h"
#include "Game/Player/DigetherStaminaRules.h"
#include "DigetherPlayerCharacter.generated.h"

class UCanvas;
class APlayerController;
class UCameraComponent;
class USpringArmComponent;
class USkeletalMeshComponent;
class USkeletalMesh;
class ADigetherWeaponBase;
class UAnimMontage;
class UDigetherMeleeHitComponent;
class UDigetherPlayerHealthComponent;
class UDigetherPlayerInputData;
class UInputMappingContext;
struct FInputActionValue;

// 정식 1인칭 플레이어 캐릭터(S03), 별도 GameMode의 기본 Pawn으로 지정 예정
// 이동·체형 수치는 DT_PlayerStats 표(FDigetherPlayerStatRow)에서 읽고 코드 기본값은 없음
// 서버와 클라이언트가 같은 표를 읽어 이동 예측 값이 일치해야 함
// 달리기·스태미나(B안): 서버가 스태미나 원본이고 소유 클라이언트는 같은 규칙(FDigetherStaminaRules)으로 예측함
// 달리기 시작/정지는 Server RPC로 알리고 서버가 조건을 다시 검사함, 이동 속도 변경은 RefreshMaxWalkSpeed 한 곳에서만 함
// 한계: RPC 지연만큼 서버와 속도 전환 시점이 달라 위치 보정이 생길 수 있음(M2에서 측정, 심하면 커스텀 이동 컴포넌트로 전환 검토)
UCLASS()
class DIGETHER_API ADigetherPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	explicit ADigetherPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void NotifyControllerChanged() override;

	// 달리기 최대 속도(표 값), 스태미나 구현에서 사용
	float GetSprintSpeedCmPerSec() const { return SprintSpeedCmPerSec; }

	// 표 값을 적용했는지 여부, false면 이동 수치가 엔진 기본값 그대로
	bool HasAppliedPlayerStats() const { return bPlayerStatsApplied; }

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	// 달리기 중인지(달리기 의도가 켜져 있음), 실제 이동 중인지는 속도로 따로 봄
	bool IsSprinting() const { return bIsSprinting; }

	// 현재 스태미나(소유자·서버만 의미 있음, 다른 클라이언트에서는 갱신되지 않음)
	float GetStamina() const { return StaminaState.Current; }
	float GetStaminaFraction() const { return StaminaParams.MaxStamina > 0.f ? FMath::Clamp(StaminaState.Current / StaminaParams.MaxStamina, 0.f, 1.f) : 0.f; }

	// 스태미나를 가득 채우고 달리기를 멈춤, 사망·부활·귀환 처리가 호출함(서버에서 호출하면 소유자에게 복제됨)
	void ResetStamina();

	virtual bool CanJumpInternal_Implementation() const override;
	virtual void OnJumped_Implementation() override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	// 들고 있는 장비, 없으면 nullptr
	ADigetherWeaponBase* GetEquippedWeapon() const { return EquippedWeapon; }

	// 체력(서버 원본, 수치는 소유자에게만 복제), 사망 여부는 모든 머신에서 맞음
	UDigetherPlayerHealthComponent* GetHealthComponent() const { return Health; }
	bool IsDead() const;

	// 개발용(Shipping 제외): 콘솔에서 자기 캐릭터에 피해·체력 초기화, 서버에 요청해 실제 피해 경로(ApplyDamage)를 탐
	UFUNCTION(Exec)
	void DigetherDamageSelf(float Amount);

	UFUNCTION(Exec)
	void DigetherResetHealth();

	// 서버 전용: 장비를 바꿈(기존 장비는 제거), 빈 클래스면 빈손이 됨
	void EquipWeapon(TSubclassOf<ADigetherWeaponBase> WeaponClass);

protected:
	// 입력 액션 모음 에셋(DA_PlayerInput), 키 배치는 이 에셋의 액션으로 코드가 만듦
	UPROPERTY(EditDefaultsOnly, Category="Digether|Input")
	TSoftObjectPtr<UDigetherPlayerInputData> InputData;

	// 수치 표 에셋, 에디터에서 CSV(Content/Data/Player/DT_PlayerStats.csv)를 FDigetherPlayerStatRow로 임포트해 만듦
	UPROPERTY(EditDefaultsOnly, Category="Digether|PlayerStat")
	TSoftObjectPtr<UDataTable> PlayerStatTable;

	// 표에서 읽을 행 이름(프로필 이름)
	UPROPERTY(EditDefaultsOnly, Category="Digether|PlayerStat")
	FName PlayerStatProfile = TEXT("Default");

	// 시작할 때 들 장비 클래스(BP_Weapon_*), 서버가 생성해 복제하고 비우면 빈손
	// 장비 교체는 EquipWeapon으로 하며 캐릭터는 장비 종류를 알지 못함
	UPROPERTY(EditDefaultsOnly, Category="Digether|Weapon")
	TSubclassOf<ADigetherWeaponBase> DefaultWeaponClass;

	// 1인칭 팔에 쓸 메시, 비우면 3인칭 몸과 같은 메시를 씀(머리는 숨김)
	// 전용 팔 모델이 오면 여기에 지정함, 3인칭 몸과 같은 스켈레톤이어야 자세를 그대로 따라감
	UPROPERTY(EditDefaultsOnly, Category="Digether|Visual")
	TSoftObjectPtr<USkeletalMesh> FirstPersonArmsMesh;

	// 1인칭 팔에서 숨길 본(이 본과 아래 전부), 3인칭 몸 전체를 팔 대신 쓰는 동안 머리가 시야를 가리지 않게 함
	UPROPERTY(EditDefaultsOnly, Category="Digether|Visual")
	TArray<FName> FirstPersonHiddenBones = { TEXT("neck_01") };

	// 1인칭 팔 메시의 몸 메시 기준 위치 보정(cm), 어깨가 카메라를 가리면 뒤로 밈, 눈으로 보며 맞추는 값
	UPROPERTY(EditAnywhere, Category="Digether|Visual")
	FTransform FirstPersonArmsOffset;

	// 1인칭 눈높이: 캡슐 꼭대기에서 아래로 내린 거리(월드 cm), 서 있을 때·웅크렸을 때 모두 이 거리로 맞춤
	// 드워프 메시(키 165cm로 확대) 기준 헬멧 꼭대기~눈 약 19cm에서 잡은 시험값, 눈으로 보며 맞추는 값
	UPROPERTY(EditDefaultsOnly, Category="Digether|Camera", meta=(ClampMin="0"))
	float EyeDepthFromTopCm = 20.f;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	// 표를 읽어 이동·점프·중력·스케일·캡슐에 적용, 실패하면 이유를 로그로 남기고 적용하지 않음
	void ApplyPlayerStats();

	// 현재 캡슐 높이(서기·웅크리기)에 맞춰 카메라 스프링암 높이와 BaseEyeHeight를 맞춤
	void UpdateEyeHeight();

	// 입력 컨텍스트를 로컬 플레이어에 등록·해제
	void SetMappingContext(AController* InController, bool bAdd) const;

	// 카메라 스프링암, 캡슐에 붙고 컨트롤러 회전을 따름, 팔 길이·오프셋은 BP에서 조정함
	UPROPERTY(VisibleAnywhere, Category="Digether|Components")
	TObjectPtr<USpringArmComponent> CameraSpringArm;

	UPROPERTY(VisibleAnywhere, Category="Digether|Components")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	// 1인칭 팔, 직접 조종하는 플레이어 화면에만 보임(OnlyOwnerSee)
	// 별도 애니메이션 없이 3인칭 몸(GetMesh())의 자세를 그대로 따라 함(리더 포즈)
	UPROPERTY(VisibleAnywhere, Category="Digether|Components")
	TObjectPtr<USkeletalMeshComponent> FirstPersonArms;

	// 근접 히트박스 판정(서버 전용), 공격 몽타주의 MeleeHitWindow 구간에서 장비의 판정 값으로 검사함
	UPROPERTY(VisibleAnywhere, Category="Digether|Components")
	TObjectPtr<UDigetherMeleeHitComponent> MeleeHit;

	// 체력, 최대치는 표(MaxHealth)에서 읽음
	UPROPERTY(VisibleAnywhere, Category="Digether|Components")
	TObjectPtr<UDigetherPlayerHealthComponent> Health;

	// 사망: 달리기 종료, 이동 정지, 이동·점프·웅크리기·공격·달리기 입력 무시(시점 회전은 허용)
	// 서버는 PlayerState의 생존 상태에 사망(Killed)을 알림, 드롭·관전 전환은 런 흐름 담당(아직 연결 안 됨)
	void HandleDied();
	void HandleRevived();

	// 소유 화면의 피해 표시용(서버 값 수신 시각)
	void HandleHealthChanged(float PreviousHealth, float CurrentHealth);
	double LastDamagedRealTimeSeconds = -1.0;

	UFUNCTION(Server, Reliable)
	void Server_DevDamageSelf(float Amount);

	UFUNCTION(Server, Reliable)
	void Server_DevResetHealth();

	// 들고 있는 장비, 서버가 정하고 복제함, 받은 머신은 OnRep에서 몸·1인칭 팔에 붙임
	UPROPERTY(ReplicatedUsing=OnRep_EquippedWeapon)
	TObjectPtr<ADigetherWeaponBase> EquippedWeapon;

	UFUNCTION()
	void OnRep_EquippedWeapon();

	// 장비를 몸·1인칭 팔에 붙임(화면이 없는 전용 서버는 붙이지 않음)
	void AttachEquippedWeapon();

	// 입력 데이터로 코드가 만든 매핑 컨텍스트, 로컬 플레이어에 등록·해제함
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	// 입력 처리, 이동·시점은 소유 클라이언트에서만 호출됨
	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleJumpPressed();
	void HandleCrouchStarted();
	void HandleCrouchCompleted();
	void HandleAttackPressed();
	void HandleSprintStarted();
	void HandleSprintCompleted();

	// 달리기 상태를 바꾸고 이동 속도를 다시 계산함(모든 상태 변경이 이 함수를 거침)
	void SetSprintingState(bool bSprint);

	// 이동 최고 속도 계산, MaxWalkSpeed를 바꾸는 유일한 곳(이후 중량·감속 배율도 여기서 합성함)
	void RefreshMaxWalkSpeed();

	// 지금 달리기를 시작할 수 있는지(표 적용됨, 잠기지 않음, 서 있음, 지면)
	bool CanStartSprint() const;

	// 서버·소유 클라이언트가 매 틱 같은 규칙으로 스태미나를 갱신함
	void TickStamina(float DeltaSeconds);

#if !UE_BUILD_SHIPPING
	// 임시 확인용 체력·스태미나 화면 표시(정식 HUD는 P2-7, 체력 표시 방식은 D2·D3 미정)
	// 콘솔 Digether.Player.HealthDebug 0 / Digether.Player.StaminaDebug 0으로 각각 끔
	// 뷰포트마다 호출되며 자기 컨트롤러의 창에서만 그림(PIE 다인에서 값이 섞이지 않게)
	void DrawStatusDebug(UCanvas* Canvas, APlayerController* ViewportController);
	FDelegateHandle StatusDebugDrawHandle;
#endif

	// 달리기 시작/정지 요청, 서버가 조건을 다시 검사함(거절하면 Client_SprintStopped로 되돌림)
	UFUNCTION(Server, Reliable)
	void Server_SetSprinting(bool bSprint);

	// 서버가 달리기를 거절·종료했음을 소유 클라이언트에 알림(복제 값이 변하지 않아도 되돌리기 위함)
	UFUNCTION(Client, Reliable)
	void Client_SprintStopped();

	UFUNCTION()
	void OnRep_Stamina();

	UFUNCTION()
	void OnRep_IsSprinting();

	// 달리기 의도, 서버와 소유 클라이언트가 각자 정하고 다른 클라이언트에는 서버 값이 복제됨(소유자 제외)
	UPROPERTY(ReplicatedUsing=OnRep_IsSprinting)
	bool bIsSprinting = false;

	// 서버 스태미나 값(소유자 전용 복제), 소유 클라이언트의 예측값이 크게 벗어났을 때만 따라감
	UPROPERTY(ReplicatedUsing=OnRep_Stamina)
	float ReplicatedStamina = 0.f;

	UPROPERTY(ReplicatedUsing=OnRep_Stamina)
	bool bReplicatedSprintLocked = false;

	FDigetherStaminaParams StaminaParams;
	FDigetherStaminaState StaminaState;
	bool bStaminaReady = false;

	// 표에서 읽은 걷기 속도, 적용 전에는 0
	float WalkSpeedCmPerSec = 0.f;

	// 공격 요청: 서버가 허용하면 모든 머신(전용 서버 포함)이 몽타주를 재생함
	// 피해 판정은 몽타주 노티파이 구간에서 MeleeHit 컴포넌트가 서버에서만 함(이 클래스는 판정하지 않음)
	UFUNCTION(Server, Reliable)
	void Server_Attack();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayAttack(ADigetherWeaponBase* Weapon);

	// 서버 전용, 이 시각 이전의 공격 요청은 거절함(몽타주 길이로 정함)
	double NextAttackTimeSeconds = 0.0;

	// 1인칭 팔 위치와 리더 포즈를 현재 설정 값으로 다시 적용, 화면용이라 서버 판정과 무관
	void RefreshVisuals();

	// 표에서 읽은 값, 적용 전에는 0
	float SprintSpeedCmPerSec = 0.f;
	bool bPlayerStatsApplied = false;
};
