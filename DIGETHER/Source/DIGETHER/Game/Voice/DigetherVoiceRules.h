#pragma once

#include "CoreMinimal.h"
#include "Game/Voice/DigetherVoiceTypes.h"

// 누가 누구 음성을 받는지 판정(기획 S28, 2026-11-06 사용자 결정), 상태 없음
struct DIGETHER_API FDigetherVoiceRules
{
	// 관전자끼리 전원, 관전자→생존자 차단, 생존자→생존자 같은 구역, 생존자→관전자 관전 대상과 같은 구역
	static bool CanHear(const FDigetherVoiceParticipant& Listener, const FDigetherVoiceParticipant& Talker,
		const TArray<FDigetherVoiceParticipant>& Participants, EDigetherVoiceStage Stage);

	// Listener가 들으면 안 되는 발화자 PlayerKey 목록
	static TSet<FString> CollectBlockedTalkers(const FDigetherVoiceParticipant& Listener,
		const TArray<FDigetherVoiceParticipant>& Participants, EDigetherVoiceStage Stage);

	// 적용된 음소거 → 원하는 음소거 전환에 필요한 변경
	static FDigetherVoiceMuteDiff Diff(const TSet<FString>& Applied, const TSet<FString>& Desired);
};
