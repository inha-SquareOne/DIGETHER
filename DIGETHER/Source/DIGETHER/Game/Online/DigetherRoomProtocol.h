#pragma once

#include "CoreMinimal.h"
#include "Game/Online/DigetherRoomTypes.h"

class FOnlineSessionSearch;
class FOnlineSessionSearchResult;
class FOnlineSessionSettings;

// 방을 세션 설정에 광고하고 검색 결과에서 다시 읽는 규약. 세션 흐름(생성·참가·이동)과 분리한다.
// 480은 여러 개발자가 공유하는 시험 App ID이므로 게임·프로토콜 키로 다른 게임의 방을 거른다.
// 접속 규약이 바뀌면 DigetherRoomProtocol.cpp의 ProtocolVersion을 올린다.
namespace DigetherRoomProtocol
{
	constexpr int32 MaxPlayers = 4;
	constexpr int32 MaxRoomNameLength = 32;
	constexpr int32 MaxRoomCodeLength = 32;
	// 자동 생성 코드: 접두 + 혼동 문자를 뺀 무작위 글자.
	inline const TCHAR* const GeneratedCodePrefix = TEXT("DG-");
	constexpr int32 GeneratedCodeRandomLength = 6;

	// 영문·숫자·하이픈·밑줄 1~MaxRoomCodeLength자.
	DIGETHER_API bool IsValidRoomCode(const FString& RoomCode);
	// GeneratedCodePrefix + 혼동 문자를 뺀 GeneratedCodeRandomLength자.
	DIGETHER_API FString MakeRoomCode();
	// 앞뒤 공백·줄바꿈을 정리하고 최대 길이로 자른다.
	DIGETHER_API FString CleanRoomName(const FString& Name);

	// 방 코드·이름·대기실 상태와 프로토콜 키를 세션 설정에 쓴다.
	void WriteRoom(FOnlineSessionSettings& Settings, const FString& RoomCode, const FString& RoomName);
	// 대기실/탐사 중 상태를 바꾼다.
	void WriteInGame(FOnlineSessionSettings& Settings, bool bInGame);
	// 이 게임의 로비만 찾도록 검색 조건을 건다. RoomCode가 비면 모든 공개 방.
	void ApplySearchFilter(FOnlineSessionSearch& Search, const FString& RoomCode);
	// 빌드·프로토콜·코드가 맞는 결과인지 확인한다. RoomCode가 비면 코드 비교를 생략한다.
	bool Matches(const FOnlineSessionSearchResult& Result, const FString& RoomCode);
	FDigetherRoomInfo ReadRoomInfo(const FOnlineSessionSearchResult& Result, int32 Index);
	// 참가할 방의 코드와 이름을 꺼낸다.
	void ReadRoomIdentity(const FOnlineSessionSearchResult& Result, FString& OutCode, FString& OutName);
}
