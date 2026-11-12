#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/Character.h"
#include "DigetherPlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class USkeletalMeshComponent;
class USkeletalMesh;
class ADigetherWeaponBase;
class UAnimMontage;
class UDigetherPlayerInputData;
class UInputMappingContext;
struct FInputActionValue;

// 정식 1인칭 플레이어 캐릭터(S03), 별도 GameMode의 기본 Pawn으로 지정 예정
// 이동·체형 수치는 DT_PlayerStats 표(FDigetherPlayerStatRow)에서 읽고 코드 기본값은 없다
// 서버와 클라이언트가 같은 표를 읽어 이동 예측 값이 일치해야 한다
// 달리기와 스태미나는 아직 없다(전용 이동 컴포넌트를 제거했으므로 스태미나 작업에서 방식을 정해 다시 만든다)
UCLASS()
class DIGETHER_API ADigetherPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	explicit ADigetherPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void NotifyControllerChanged() override;

	// 달리기 최대 속도(표 값), 스태미나 구현에서 사용
	float GetSprintSpeedCmPerSec() const { return SprintSpeedCmPerSec; }

	// 표 값을 적용했는지 여부, false면 이동 수치가 엔진 기본값 그대로다
	bool HasAppliedPlayerStats() const { return bPlayerStatsApplied; }

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 들고 있는 장비, 없으면 nullptr
	ADigetherWeaponBase* GetEquippedWeapon() const { return EquippedWeapon; }

	// 서버 전용: 장비를 바꾼다(기존 장비는 제거). 빈 클래스면 빈손이 된다
	void EquipWeapon(TSubclassOf<ADigetherWeaponBase> WeaponClass);

protected:
	// 입력 액션 모음 에셋(DA_PlayerInput), 키 배치는 이 에셋의 액션으로 코드가 만든다
	UPROPERTY(EditDefaultsOnly, Category="Digether|Input")
	TSoftObjectPtr<UDigetherPlayerInputData> InputData;

	// 수치 표 에셋, 에디터에서 CSV(Content/Data/Player/DT_PlayerStats.csv)를 FDigetherPlayerStatRow로 임포트해 만든다
	UPROPERTY(EditDefaultsOnly, Category="Digether|PlayerStat")
	TSoftObjectPtr<UDataTable> PlayerStatTable;

	// 표에서 읽을 행 이름(프로필 이름)
	UPROPERTY(EditDefaultsOnly, Category="Digether|PlayerStat")
	FName PlayerStatProfile = TEXT("Default");

	// 시작할 때 들 장비 클래스(BP_Weapon_*), 서버가 생성해 복제하고 비우면 빈손이다
	// 장비 교체는 EquipWeapon으로 하며 캐릭터는 장비 종류를 알지 못한다
	UPROPERTY(EditDefaultsOnly, Category="Digether|Weapon")
	TSubclassOf<ADigetherWeaponBase> DefaultWeaponClass;

	// 1인칭 팔에 쓸 메시, 비우면 3인칭 몸과 같은 메시를 쓴다(머리는 숨긴다)
	// 전용 팔 모델이 오면 여기에 지정한다. 3인칭 몸과 같은 스켈레톤이어야 자세를 그대로 따라간다
	UPROPERTY(EditDefaultsOnly, Category="Digether|Visual")
	TSoftObjectPtr<USkeletalMesh> FirstPersonArmsMesh;

	// 1인칭 팔에서 숨길 본(이 본과 아래 전부), 3인칭 몸 전체를 팔 대신 쓰는 동안 머리가 시야를 가리지 않게 한다
	UPROPERTY(EditDefaultsOnly, Category="Digether|Visual")
	TArray<FName> FirstPersonHiddenBones = { TEXT("neck_01") };

	// 1인칭 팔 메시의 몸 메시 기준 위치 보정(cm), 어깨가 카메라를 가리면 뒤로 민다. 눈으로 보며 맞추는 값
	UPROPERTY(EditAnywhere, Category="Digether|Visual")
	FTransform FirstPersonArmsOffset;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	// 표를 읽어 이동·점프·중력·스케일·캡슐에 적용, 실패하면 이유를 로그로 남기고 적용하지 않는다
	void ApplyPlayerStats();

	// 입력 컨텍스트를 로컬 플레이어에 등록·해제
	void SetMappingContext(AController* InController, bool bAdd) const;

	// 카메라 스프링암, 캡슐에 붙고 컨트롤러 회전을 따른다. 팔 길이·오프셋은 BP에서 조정한다
	UPROPERTY(VisibleAnywhere, Category="Digether|Components")
	TObjectPtr<USpringArmComponent> CameraSpringArm;

	UPROPERTY(VisibleAnywhere, Category="Digether|Components")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	// 1인칭 팔, 직접 조종하는 플레이어 화면에만 보인다(OnlyOwnerSee)
	// 별도 애니메이션 없이 3인칭 몸(GetMesh())의 자세를 그대로 따라 한다(리더 포즈)
	UPROPERTY(VisibleAnywhere, Category="Digether|Components")
	TObjectPtr<USkeletalMeshComponent> FirstPersonArms;

	// 들고 있는 장비, 서버가 정하고 복제한다. 받은 머신은 OnRep에서 몸·1인칭 팔에 붙인다
	UPROPERTY(ReplicatedUsing=OnRep_EquippedWeapon)
	TObjectPtr<ADigetherWeaponBase> EquippedWeapon;

	UFUNCTION()
	void OnRep_EquippedWeapon();

	// 장비를 몸·1인칭 팔에 붙인다(화면이 없는 전용 서버는 붙이지 않는다)
	void AttachEquippedWeapon();

	// 입력 데이터로 코드가 만든 매핑 컨텍스트, 로컬 플레이어에 등록·해제한다
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	// 입력 처리, 이동·시점은 소유 클라이언트에서만 호출된다
	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleJumpPressed();
	void HandleCrouchStarted();
	void HandleCrouchCompleted();
	void HandleAttackPressed();

	// 공격 요청: 서버가 허용하면 모든 머신(서버 포함)이 몽타주를 재생한다. 피해 판정은 이 클래스가 하지 않는다
	UFUNCTION(Server, Reliable)
	void Server_Attack();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayAttack(ADigetherWeaponBase* Weapon);

	// 서버 전용, 이 시각 이전의 공격 요청은 거절한다(몽타주 길이로 정함)
	double NextAttackTimeSeconds = 0.0;

	// 1인칭 팔 위치와 리더 포즈를 현재 설정 값으로 다시 적용, 화면용이라 서버 판정과 무관
	void RefreshVisuals();

	// 표에서 읽은 값, 적용 전에는 0
	float SprintSpeedCmPerSec = 0.f;
	bool bPlayerStatsApplied = false;
};
