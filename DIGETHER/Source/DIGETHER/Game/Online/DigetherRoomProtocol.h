#pragma once

#include "CoreMinimal.h"
#include "Game/Online/DigetherRoomTypes.h"

class FOnlineSessionSearch;
class FOnlineSessionSearchResult;
class FOnlineSessionSettings;

// 방을 세션 설정에 광고하고 검색 결과에서 다시 읽는 규약, 세션 흐름(생성·참가·이동)과 분리
// 480은 여러 개발자가 공유하는 시험 App ID라 게임·프로토콜 키로 다른 게임의 방을 거름
// 접속 규약 변경 시 DigetherRoomProtocol.cpp의 ProtocolVersion 증가
namespace DigetherRoomProtocol
{
	constexpr int32 MaxPlayers = 4;
	constexpr int32 MaxRoomNameLength = 32;
	constexpr int32 MaxRoomCodeLength = 32;
	// 자동 생성 코드: 접두 + 혼동 문자를 뺀 무작위 글자
	inline const TCHAR* const GeneratedCodePrefix = TEXT("DG-");
	constexpr int32 GeneratedCodeRandomLength = 6;

	// 사용자·콘솔 입력 코드를 정식 형태(앞뒤 공백 제거 + 대문자)로 변환
	// 세션 요청 입구(HostRoom·JoinByCode·FindRooms)에서 한 번만 적용, 이후 검사·광고·검색은 정식 형태만 다룸
	DIGETHER_API FString NormalizeRoomCode(const FString& RoomCode);
	// 정식 형태(영문 대문자·숫자·하이픈·밑줄 1~MaxRoomCodeLength자)인지 확인
	// 광고·검색 비교가 대소문자 구분이라 정규화 전 소문자 코드는 맞는 방을 못 찾으므로 거절
	DIGETHER_API bool IsValidRoomCode(const FString& RoomCode);
	// GeneratedCodePrefix + 혼동 문자를 뺀 GeneratedCodeRandomLength자
	DIGETHER_API FString MakeRoomCode();
	// 앞뒤 공백 제거, 줄바꿈은 공백으로 바꾸고 최대 길이로 자름
	DIGETHER_API FString CleanRoomName(const FString& Name);

	// 방 코드·이름·대기실 상태와 프로토콜 키를 세션 설정에 기록
	void WriteRoom(FOnlineSessionSettings& Settings, const FString& RoomCode, const FString& RoomName);
	// 대기실/탐사 중 상태 변경
	void WriteInGame(FOnlineSessionSettings& Settings, bool bInGame);
	// 이 게임의 로비만 찾는 검색 조건, RoomCode가 비면 모든 공개 방
	void ApplySearchFilter(FOnlineSessionSearch& Search, const FString& RoomCode);
	// 빌드·프로토콜·코드가 맞는 결과인지, RoomCode가 비면 코드 비교 생략
	// RoomCode와 광고 코드 모두 정식 형태 전제, 대소문자 구분 비교
	bool Matches(const FOnlineSessionSearchResult& Result, const FString& RoomCode);
	FDigetherRoomInfo ReadRoomInfo(const FOnlineSessionSearchResult& Result, int32 Index);
	// 참가할 방의 코드·이름 읽기
	void ReadRoomIdentity(const FOnlineSessionSearchResult& Result, FString& OutCode, FString& OutName);
}
