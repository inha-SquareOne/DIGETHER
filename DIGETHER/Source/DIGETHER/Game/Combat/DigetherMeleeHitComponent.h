#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Combat/DigetherMeleeRules.h"
#include "DigetherMeleeHitComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDigetherMeleeHit, AActor*, HitActor, float, Damage, FVector, ImpactPoint);

// 근접 공격 히트박스 판정(서버 전용), 소유 액터의 몽타주에 놓인 판정 구간 노티파이
// (UDigetherAnimNotifyState_MeleeHitWindow)가 Begin/Tick/End를 호출하고, 서버에서만 상자 겹침 검사와 피해 적용을 함
// - 판정 값(FDigetherMeleeHitParams)과 피해 주체는 소유자가 SetHitSource로 넣음(플레이어는 장비 교체 때)
// - 한 판정 구간 안에서 같은 액터는 한 번만 맞음
// - 피해는 엔진 ApplyDamage로 보내므로 받는 쪽은 OnTakeAnyDamage로 처리함(몬스터 체력 컴포넌트 등)
// - 클라이언트는 아무것도 하지 않음, 맞은 연출이 필요하면 받는 쪽의 복제 상태나 OnMeleeHit에서 별도로 알림
// 플레이어 외 액터(몬스터 등)도 이 컴포넌트와 노티파이를 그대로 붙여 쓸 수 있음
UCLASS(ClassGroup=(Digether), meta=(BlueprintSpawnableComponent))
class DIGETHER_API UDigetherMeleeHitComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigetherMeleeHitComponent();

	// 서버 전용: 이후 판정 구간에 쓸 값과 피해 주체(장비 액터 등, 비우면 소유 액터), 진행 중인 구간에도 즉시 반영됨
	void SetHitSource(const FDigetherMeleeHitParams& InParams, AActor* InDamageCauser);

	// 서버 전용: 판정 값을 지움(빈손), 이후 구간은 아무것도 맞히지 않음
	void ClearHitSource();

	// 판정 구간 노티파이가 호출함, 서버가 아니면 무시함
	void BeginHitWindow(float DamageScale);
	void TickHitWindow();
	void EndHitWindow();

	bool IsHitWindowActive() const { return ActiveWindowCount > 0; }

	// 서버에서 대상에게 피해를 적용한 직후 호출됨(효과음·채굴 연결 등 확장 지점)
	UPROPERTY(BlueprintAssignable, Category="Digether|Melee")
	FOnDigetherMeleeHit OnMeleeHit;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool CanRunOnThisMachine() const;

	// 눈 위치·조준 방향, Pawn은 서버가 가진 조준 회전(GetBaseAimRotation)을 씀
	void GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	// 현재 상자에 겹친 대상을 찾아 아직 맞지 않은 대상에게 피해를 줌
	void PerformHitCheck();

	bool HasLineOfSight(const FVector& From, const AActor* Target, const FVector& TargetPoint) const;

	FDigetherMeleeHitParams Params;
	bool bHasUsableParams = false;
	TWeakObjectPtr<AActor> DamageCauser;

	// 겹친 구간 수(몽타주 블렌드 중 앞뒤 구간이 겹칠 수 있음), 0이 되면 구간 종료
	int32 ActiveWindowCount = 0;
	float ActiveDamageScale = 1.f;

	// 현재 구간에서 이미 맞은 액터
	TSet<TWeakObjectPtr<AActor>> HitActorsThisWindow;
};
