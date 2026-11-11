#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Game/Monster/DigetherMonsterTypes.h"
#include "DigetherMonsterSenseSubsystem.generated.h"

class ADigetherMonsterCharacter;
class APlayerState;

// 호스트 감각 판정: 0.1초 고정 주기로 몬스터 시야·비시각 자극을 모아 종 컴포넌트에 전달,
// 클라이언트 카메라 표본을 보관해 시선 판정(플레이어 → 몬스터) 질의 제공
// 클라이언트 월드에도 생성되지만 활성화하지 않음
UCLASS()
class DIGETHER_API UDigetherMonsterSenseSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	using FPlayerKeyResolver = TFunction<FString(const APlayerState*)>;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bActive; }

	bool IsActive() const { return bActive; }

	// 테스트·수동 활성용, 클라이언트 넷모드에서는 거절
	bool Activate();

	// 런 도메인과 같은 플레이어 키를 쓰도록 GameMode가 지정, 기본은 고유 ID 문자열
	void SetPlayerKeyResolver(FPlayerKeyResolver Resolver);
	FString ResolvePlayerKey(const APlayerState* PlayerState) const;

	// 몬스터 시야·자극 대상 등록, 디렉터가 생성 때 호출(레벨 직접 배치도 BeginPlay에서 자동 등록)
	void RegisterMonster(ADigetherMonsterCharacter* Monster);
	void UnregisterMonster(ADigetherMonsterCharacter* Monster);

	// 종 컴포넌트가 상태에 따라 시야를 끄고 켬
	void SetSightEnabled(ADigetherMonsterCharacter* Monster, bool bEnabled);

	// 서버, 비시각 자극(거미줄 진동·피격 등), 다음 주기에 전달
	void ReportStimulus(ADigetherMonsterCharacter* Monster, const FDigetherMonsterStimulusEvent& Event);

	// 서버, 클라이언트 카메라 표본 수신(검증·순서 확인), 거절하면 거짓
	bool SubmitCameraSample(APlayerController* Controller, const FDigetherCameraSample& Sample);

	// 유효 기간 안 표본만, 없거나 오래됐으면 거짓(시선 판정은 '보지 않음' 처리)
	bool GetCameraSample(const APlayerController* Controller, FDigetherCameraSample& OutSample) const;

	// 살아 있는(생존·비관전) 플레이어 목록, Pawn 없는 플레이어 제외
	void CollectLivingTargets(TArray<FDigetherMonsterTarget>& OutTargets) const;

	// 가림 판정, 주기당 트레이스 수를 집계
	bool HasLineOfSight(const FVector& From, const FVector& To, TConstArrayView<const AActor*> IgnoredActors) const;

	// 플레이어가 지점을 보는지(화면 안), 표본 없거나 가리면 거짓
	bool IsPointVisibleToPlayer(const APlayerController* Controller, const FVector& Point, float MaxDistance,
		TConstArrayView<const AActor*> IgnoredActors) const;

	// 플레이어 화면 중심과 지점의 각도(도), 표본 없으면 180
	float GetAngleFromPlayerView(const APlayerController* Controller, const FVector& Point) const;

	int32 GetLastStepTraceCount() const { return LastStepTraceCount; }

	// 테스트용, 누적 시간과 무관하게 한 주기 실행
	void StepForTest() { RunStep(); }

private:
	struct FMonsterEntry
	{
		TWeakObjectPtr<ADigetherMonsterCharacter> Monster;
		bool bSightEnabled = true;
		TArray<FDigetherMonsterStimulusEvent> PendingStimuli;
	};

	void RunStep();
	FMonsterEntry* FindEntry(const ADigetherMonsterCharacter* Monster);

	bool bActive = false;
	float Accumulator = 0.f;
	TArray<FMonsterEntry> Monsters;
	TMap<TWeakObjectPtr<const APlayerController>, FDigetherReceivedCameraSample> CameraSamples;
	FPlayerKeyResolver PlayerKeyResolver;
	mutable int32 StepTraceCount = 0;
	int32 LastStepTraceCount = 0;
};
