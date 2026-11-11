#pragma once

#include "CoreMinimal.h"

// 스폰 판정 순수 규칙, 월드·Actor·시간 비의존(자동 테스트 대상)
namespace DigetherMonsterSpawnRules
{
	struct FCurveParams
	{
		float GraceSeconds = 0.f;
		float StartChance = 0.f;
		float MaxChance = 0.f;
		float MaxChanceAtSeconds = 0.f;
	};

	// 스폰 타이머 경과 시간의 판정당 확률(0~1)
	// 유예 전 0, 유예~도달 지수형 Start×(Max/Start)^진행률, 도달 후 Max, Start가 0이면 선형
	DIGETHER_API float EvaluateChance(const FCurveParams& Params, float TimerSeconds);

	// 후보 거르기 입력 한 종, 값은 호출자가 장부에서 채움
	struct FCandidate
	{
		int32 SourceIndex = INDEX_NONE;
		float Weight = 0.f;
		// 주기+파워형만 양수
		int32 PowerCost = 0;
		int32 RunSpawned = 0;
		// 0은 제한 없음
		int32 RunSpawnCap = 0;
		int32 Alive = 0;
		// 0은 제한 없음, 갱도 주기형만
		int32 MaxAlive = 0;
	};

	// 파워 부족·탐사당 상한·종별 최대 수·가중치 0을 제외(기획: 적용되는 제한에 따라 제외)
	// RemainingPower가 음수면 파워 제한 미적용(갱도)
	DIGETHER_API TArray<FCandidate> FilterCandidates(TConstArrayView<FCandidate> Candidates, int32 RemainingPower);

	// 가중치 뽑기, 후보가 없거나 가중치 합이 0이면 INDEX_NONE, 반환은 Candidates 안 위치
	DIGETHER_API int32 PickWeighted(TConstArrayView<FCandidate> Candidates, const FRandomStream& Stream);

	// 던전 파워 한도, 기본 + 목표 달성 수 × 증가량, 최대에서 멈춤
	DIGETHER_API int32 EvaluatePowerLimit(int32 Base, int32 PerGoalAchieved, int32 Max, int32 GoalsAchieved);

	// 탐사 한 번의 생성 장부, 던전 파워는 던전 ID별
	struct DIGETHER_API FLedger
	{
		TMap<FName, int32> DungeonPowerUsed;
		TMap<FName, int32> RunSpawnedBySpecies;
		TMap<FName, int32> AliveBySpecies;

		void Reset();
		int32 GetPowerUsed(FName DungeonId) const;
		int32 GetRunSpawned(FName SpeciesId) const;
		int32 GetAlive(FName SpeciesId) const;

		// 생성 확정 때 1회, 파워는 던전 ID가 있을 때만 차감
		void RecordSpawn(FName SpeciesId, FName DungeonId, int32 PowerCost);

		// 처치·소멸 때 1회, 생존 수와 해당 던전 파워 반환(탐사당 생성 수는 유지)
		// 기록이 없는 반환은 거절해 음수 방지
		bool RecordRemoval(FName SpeciesId, FName DungeonId, int32 PowerCost);

		// 예고 중 예약했던 생성이 실패·취소됐을 때, 탐사당 생성 수까지 되돌림
		bool RecordCancelledSpawn(FName SpeciesId, FName DungeonId, int32 PowerCost);
	};
}
