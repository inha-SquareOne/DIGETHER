#pragma once

#include "CoreMinimal.h"
#include "GameFramework/OnlineReplStructs.h"

class APlayerController;

// 기획 S28 근거리 음성 시험값(2026-11-04 답변 초안 G, 제안), 플레이테스트에서 조정
namespace DigetherVoiceDefaults
{
	// 원래 크기로 들리는 거리(VOICE_RANGE_FULL)
	inline constexpr float RangeFullCm = 300.f;
	// 최대 거리(VOICE_RANGE_MAX), 밖은 무음
	inline constexpr float RangeMaxCm = 2500.f;
	// 매몰 발화자는 최대 거리만 절반, 원음 구간은 유지
	inline constexpr float BuriedRangeRatio = 0.5f;
	// 서버 수신 제한·클라 재생 설정 갱신 간격
	inline constexpr float RefreshSeconds = 0.25f;
	// 플레이어별 음량 최대 배율(작게 들리는 동료 키움), 시험값
	inline constexpr float MaxPlayerVolume = 2.f;

	// 로컬 재생 음량 = 전체 목소리 × 플레이어별, 나만 음소거면 0
	inline float PlaybackVolume(const float OverallVolume, const float PlayerVolume, const bool bMutedLocally)
	{
		return bMutedLocally ? 0.f : FMath::Clamp(OverallVolume, 0.f, 1.f) * FMath::Clamp(PlayerVolume, 0.f, MaxPlayerVolume);
	}
}

enum class EDigetherVoiceRole : uint8
{
	Survivor,
	// 사망·탈출·관전 합류
	Spectator
};

enum class EDigetherVoiceStage : uint8
{
	// 생존자·관전자 구분 적용
	Separated,
	// 결과 크레딧: 전원 음성
	AllOpen
};

struct FDigetherVoiceParticipant
{
	// 공개 PlayerId 문자열, 관전 대상 키와 같은 형식
	FString PlayerKey;
	EDigetherVoiceRole Role = EDigetherVoiceRole::Survivor;
	// 같은 값끼리만 생존자 음성 전달(갱도↔던전 차단)
	int32 ZoneId = 0;
	// 관전자 전용, 비면 관전 대상 없음
	FString SpectateTargetPlayerKey;
};

// 서버 수신 제한 입력 1명분
struct FDigetherVoiceRouteEntry
{
	TWeakObjectPtr<APlayerController> Controller;
	FUniqueNetIdRepl UniqueId;
	FDigetherVoiceParticipant Participant;
};

struct FDigetherVoiceMuteDiff
{
	TArray<FString> ToMute;
	TArray<FString> ToUnmute;
};
