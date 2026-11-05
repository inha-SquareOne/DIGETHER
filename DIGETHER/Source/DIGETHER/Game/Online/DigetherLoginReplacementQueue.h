#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Templates/Function.h"

class APlayerController;

// 기획 C2 같은 계정 교체에서 이전 연결 정리를 기다리는 PreLoginAsync 승인 대기열이다.
// 엔진 계약상 PreLoginAsync의 완료 콜백은 한 번 반드시 불러야 한다(안 부르면 접속이 멈추거나 시간 초과).
// 이 대기열은 콜백을 잃지 않도록 밀려난 대기·준비된 대기·시간 초과 대기·종료 시 남은 대기를 모두 꺼내 돌려주며,
// 콜백 호출과 재판정은 호출자(GameMode)가 한다. 대기열 안에서 콜백을 부르지 않아 순회 중 재진입이 없다.
class DIGETHER_API FDigetherLoginReplacementQueue
{
public:
	struct FEntry
	{
		FUniqueNetIdRepl UniqueId;
		// 끊으라고 요청한 이전 연결의 컨트롤러. 정리되면 무효가 된다.
		TWeakObjectPtr<APlayerController> Previous;
		AGameModeBase::FOnPreLoginCompleteDelegate OnComplete;
		double StartSeconds = 0.0;
	};

	// 같은 ID의 기존 대기는 새 요청에 밀려 OutSuperseded로 나온다. 같은 계정은 가장 늦게 들어온 접속을 받는다(C2).
	void Add(FEntry&& Entry, TArray<FEntry>& OutSuperseded);
	// 이전 연결이 정리된 대기는 OutReady로, 제한 시간이 지난 대기는 OutTimedOut으로 꺼낸다. 나머지는 남긴다.
	void Collect(double NowSeconds, double TimeoutSeconds, TFunctionRef<bool(const FEntry&)> IsPreviousReleased,
		TArray<FEntry>& OutReady, TArray<FEntry>& OutTimedOut);
	// 종료 정리용. 남은 대기를 모두 꺼낸다.
	TArray<FEntry> TakeAll();
	bool IsEmpty() const { return Entries.IsEmpty(); }
	int32 Num() const { return Entries.Num(); }

private:
	TArray<FEntry> Entries;
};
