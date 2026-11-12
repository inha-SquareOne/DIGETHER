#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DigetherDainVerification.generated.h"
class ADigetherDainCharacter;
class ADigetherDiggableSoil;

// 개인 맵 자동 검증 전용, 수동 조작과 동일한 이동·굴착 진입점 사용
UCLASS()
class ADigetherDainVerification : public AActor
{
	GENERATED_BODY()
public:
	ADigetherDainVerification();
	virtual void Tick(float DeltaSeconds) override;

private:
	// 성공 여부·이유 입력 → JSON/CSV 기록과 시험 프로세스 종료 요청
	void Finish(bool bPassed, const FString& Reason);
	// 접근→보호→굴착→통과→귀환의 검증 단계, 타임아웃은 초 단위
	int32 Stage = 0;
	float StageSeconds = 0;
	float TotalSeconds = 0;
	// 굴착 전/1회 후 차단 위치(cm), 순간이동 없는 통과 비교용
	float BlockedX = 0;
	float AfterOneHitX = 0;
	// 보호 바닥 요청 거부와 밀도 불변의 관찰 결과
	bool bProtectedRejected = false;
	// 월드 소유 시험 Actor의 약한 참조, Tick에서 유효성 재확인
	UPROPERTY()
	TWeakObjectPtr<ADigetherDainCharacter> Character;
	UPROPERTY()
	TWeakObjectPtr<ADigetherDiggableSoil> Wall;
};
