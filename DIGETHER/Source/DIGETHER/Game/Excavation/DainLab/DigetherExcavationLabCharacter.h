#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Game/Excavation/Dain/DigetherDiggableSoil.h"
#include "DigetherExcavationLabCharacter.generated.h"
class UCameraComponent;

// 개인 시험 선택값, 정식 아이템 식별자와 분리
UENUM()
enum class EDigetherLabTool : uint8
{
	Pickaxe,
	Shovel
};

// 개인 맵 입력 어댑터, 공용 Character·정식 장비 연결과 분리
UCLASS()
class ADigetherExcavationLabCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	ADigetherExcavationLabCharacter();
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
	// 기존 시험 호출 호환, 상세 결과는 TryDigOnce 사용
	bool DigOnce() { return TryDigOnce() == EDigetherSoilEditResult::Applied; }
	// 입력·간격·대상 거절은 Rejected, 유효 흙 타격은 코어 결과 그대로 반환
	EDigetherSoilEditResult TryDigOnce();
	// 시선 앞 ToolReachCm 첫 차단 결과, 무효 사거리·미충돌 시 OutHit 초기화
	bool TraceTool(FHitResult& OutHit) const;
	// 두 도구 공통 시험 사거리(cm), 기존 400cm 유지, 정식 장비 수치 미확정
	UPROPERTY(EditAnywhere, Category = "Digether|Excavation|Lab", meta = (ClampMin = "1", Units = "cm"))
	float ToolReachCm = 400.f;
	// 삽 반경(cm), 기존 직렬화 이름과 75cm 기본값 유지, 기획 미확정
	UPROPERTY(EditAnywhere, Category = "Digether|Excavation|Lab",
			  meta = (ClampMin = "25", ClampMax = "125", Units = "cm"))
	float SoilBrushRadius = 75.f;
	// 삽 사용 간격(초), 기존 직렬화 이름과 기본값 유지, 기획 미확정
	UPROPERTY(EditAnywhere, Category = "Digether|Excavation|Lab", meta = (ClampMin = "0.15", Units = "s"))
	float HitInterval = .35f;

	// 곡괭이 시험 반경(cm)·간격(초), 기획 승인값이 아닌 비교용 임시값
	UPROPERTY(EditAnywhere, Category = "Digether|Excavation|Lab", meta = (ClampMin = "25", ClampMax = "125", Units = "cm"))
	float PickaxeRadiusCm = 50.f;
	UPROPERTY(EditAnywhere, Category = "Digether|Excavation|Lab", meta = (ClampMin = "0.15", Units = "s"))
	float PickaxeIntervalSeconds = .5f;
	// 선택만 변경, 직전 사용의 대기 시간은 유지
	void SelectPickaxe();
	void SelectShovel();
	EDigetherLabTool GetSelectedTool() const { return SelectedTool; }
	float GetSelectedRadiusCm() const;
	float GetSelectedIntervalSeconds() const;

private:
	// 누름/뗌 입력의 반복 요청 상태 전환, 정식 도구 시스템과 분리
	void StartDig();
	void StopDig();
	// 마우스 축 입력, Pitch는 화면 기준에 맞춰 부호 반전
	void LookYaw(float Value);
	void LookPitch(float Value);
	// 타격 Trace 원점·방향을 제공하는 Actor 소유 카메라
	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;
	// 버튼 유지 중 간격 제한을 통과한 요청만 반복
	bool bDigHeld = false;
	// 기존 시험과 같은 삽으로 시작, 장착·복제 상태가 아닌 로컬 시험 선택
	EDigetherLabTool SelectedTool = EDigetherLabTool::Shovel;
	// 직전 도구의 간격으로 정한 다음 허용 월드 시각, 전환으로 연타 제한 우회 금지
	double NextUseSeconds = 0.0;
};
