#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Templates/Function.h"

class APlayerController;

// 기획 C2 같은 계정 교체에서 이전 연결 정리를 기다리는 PreLoginAsync 승인 대기열
// 엔진 계약상 PreLoginAsync 완료 콜백은 반드시 한 번 호출(누락 시 접속 정지 또는 시간 초과)
// 밀려난·준비된·시간 초과·종료 시 남은 대기를 모두 꺼내 돌려줘 콜백 유실 방지
// 콜백 호출·재판정은 호출자(GameMode) 담당, 대기열 안에서 콜백을 부르지 않아 순회 중 재진입 없음
class DIGETHER_API FDigetherLoginReplacementQueue
{
public:
	struct FEntry
	{
		FUniqueNetIdRepl UniqueId;
		// 끊으라고 요청한 이전 연결의 컨트롤러, 정리되면 무효
		TWeakObjectPtr<APlayerController> Previous;
		AGameModeBase::FOnPreLoginCompleteDelegate OnComplete;
		double StartSeconds = 0.0;
	};

	// 같은 ID의 기존 대기는 새 요청에 밀려 OutSuperseded로 나옴, 같은 계정은 가장 늦은 접속 수락(C2)
	void Add(FEntry&& Entry, TArray<FEntry>& OutSuperseded);
	// 이전 연결이 정리된 대기는 OutReady, 제한 시간이 지난 대기는 OutTimedOut으로 꺼냄, 나머지 유지
	void Collect(double NowSeconds, double TimeoutSeconds, TFunctionRef<bool(const FEntry&)> IsPreviousReleased,
		TArray<FEntry>& OutReady, TArray<FEntry>& OutTimedOut);
	// 종료 정리용, 남은 대기 전부 꺼냄
	TArray<FEntry> TakeAll();
	bool IsEmpty() const { return Entries.IsEmpty(); }
	int32 Num() const { return Entries.Num(); }

private:
	TArray<FEntry> Entries;
};
