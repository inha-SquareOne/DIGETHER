#pragma once

#include "CoreMinimal.h"

class UWorld;

// 웹 상태 패널·화면 상태 표시용 요약, 값은 판정 코드의 컴포넌트에서 그대로 읽음
namespace DigetherDev
{
	struct FStateSummary
	{
		bool bHasWorld = false;
		bool bHost = false;
		bool bGameplay = false;
		FString MapName;
		FString NetMode;
		int32 TotalDay = 0;
		int32 PeriodDay = 0;
		int32 TargetCredits = 0;
		// 납부 누적은 서버 값, 참가자는 -1
		int32 DepositedCredits = -1;
		// 남은 탐사 시간은 서버만 알고 복제하지 않음, 참가자·탐사 전은 -1
		double RemainingSeconds = -1.0;
		FString RunPhase;
		int32 AftershockStage = 0;
		bool bPaymentDeadline = false;
		bool bGameOver = false;
		FString ElevatorPhase;
		int32 CompletedAscents = 0;
		FString WeightStage;

		struct FPlayer
		{
			int32 PlayerId = 0;
			FString Name;
			bool bSelf = false;
			// 잔고는 본인에게만 복제, 호스트가 아닌 다른 사람은 -1
			int32 Balance = -1;
			FString Life;
		};
		TArray<FPlayer> Players;

		bool bMonsterDirector = false;
		bool bMonsterRun = false;
		bool bSpawnTimerRunning = false;
		float SpawnTimerSeconds = 0.f;
		float SpawnChance = 0.f;
		int32 LiveMonsters = 0;
	};

	FStateSummary CaptureState(UWorld* World);
	FString StateToJson(const FStateSummary& State, bool bCheatUsed, const FString& WebUrl, const TArray<TPair<FString, bool>>& Panels);
	void StateToLines(const FStateSummary& State, TArray<FString>& OutLines);
}
