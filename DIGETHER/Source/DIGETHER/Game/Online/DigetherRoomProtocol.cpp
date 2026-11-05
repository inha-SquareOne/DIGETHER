#include "Game/Online/DigetherRoomProtocol.h"

#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"

namespace DigetherRoomProtocol
{
	namespace
	{
		const FName ProtocolKey(TEXT("DIGETHER_GAME"));
		const FName RoomCodeKey(TEXT("DIGETHER_ROOM"));
		const FName RoomNameKey(TEXT("DIGETHER_NAME"));
		const FName RoomStateKey(TEXT("DIGETHER_STATE"));
		const FString ProtocolVersion(TEXT("lobby-v1"));
		const FString StateLobby(TEXT("lobby"));
		const FString StateInGame(TEXT("ingame"));
	}

	FString NormalizeRoomCode(const FString& RoomCode)
	{
		return RoomCode.TrimStartAndEnd().ToUpper();
	}

	bool IsValidRoomCode(const FString& RoomCode)
	{
		if (RoomCode.IsEmpty() || RoomCode.Len() > MaxRoomCodeLength)
		{
			return false;
		}
		for (const TCHAR C : RoomCode)
		{
			const bool bUpperAlnum = (C >= TEXT('A') && C <= TEXT('Z')) || (C >= TEXT('0') && C <= TEXT('9'));
			if (!bUpperAlnum && C != TEXT('-') && C != TEXT('_'))
			{
				return false;
			}
		}
		return true;
	}

	FString MakeRoomCode()
	{
		// 혼동하기 쉬운 0/O, 1/I는 뺀다.
		static const TCHAR Alphabet[] = TEXT("ABCDEFGHJKLMNPQRSTUVWXYZ23456789");
		constexpr int32 AlphabetLength = UE_ARRAY_COUNT(Alphabet) - 1;
		FString Code(GeneratedCodePrefix);
		for (int32 Index = 0; Index < GeneratedCodeRandomLength; ++Index)
		{
			Code.AppendChar(Alphabet[FMath::RandRange(0, AlphabetLength - 1)]);
		}
		return Code;
	}

	FString CleanRoomName(const FString& Name)
	{
		FString Clean = Name.TrimStartAndEnd().Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT(" "));
		return Clean.Left(MaxRoomNameLength);
	}

	void WriteRoom(FOnlineSessionSettings& Settings, const FString& RoomCode, const FString& RoomName)
	{
		Settings.Set(ProtocolKey, ProtocolVersion, EOnlineDataAdvertisementType::ViaOnlineService);
		Settings.Set(RoomCodeKey, RoomCode, EOnlineDataAdvertisementType::ViaOnlineService);
		Settings.Set(RoomNameKey, RoomName, EOnlineDataAdvertisementType::ViaOnlineService);
		WriteInGame(Settings, false);
	}

	void WriteInGame(FOnlineSessionSettings& Settings, bool bInGame)
	{
		Settings.Set(RoomStateKey, bInGame ? StateInGame : StateLobby, EOnlineDataAdvertisementType::ViaOnlineService);
	}

	void ApplySearchFilter(FOnlineSessionSearch& Search, const FString& RoomCode)
	{
		Search.QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
		Search.QuerySettings.Set(ProtocolKey, ProtocolVersion, EOnlineComparisonOp::Equals);
		if (!RoomCode.IsEmpty())
		{
			Search.QuerySettings.Set(RoomCodeKey, RoomCode, EOnlineComparisonOp::Equals);
		}
	}

	bool Matches(const FOnlineSessionSearchResult& Result, const FString& RoomCode)
	{
		FString Version, FoundCode;
		return Result.IsValid()
			&& Result.Session.SessionSettings.BuildUniqueId == GetBuildUniqueId()
			&& Result.Session.SessionSettings.Get(ProtocolKey, Version) && Version.Equals(ProtocolVersion, ESearchCase::CaseSensitive)
			&& Result.Session.SessionSettings.Get(RoomCodeKey, FoundCode) && IsValidRoomCode(FoundCode)
			&& (RoomCode.IsEmpty() || FoundCode.Equals(RoomCode, ESearchCase::CaseSensitive));
	}

	FDigetherRoomInfo ReadRoomInfo(const FOnlineSessionSearchResult& Result, int32 Index)
	{
		FDigetherRoomInfo Info;
		Info.Index = Index;
		const FOnlineSessionSettings& Settings = Result.Session.SessionSettings;
		Settings.Get(RoomCodeKey, Info.RoomCode);
		Settings.Get(RoomNameKey, Info.RoomName);
		FString State;
		Settings.Get(RoomStateKey, State);
		Info.bInGame = State == StateInGame;
		Info.HostName = Result.Session.OwningUserName;
		if (Info.RoomName.IsEmpty())
		{
			Info.RoomName = Info.HostName.IsEmpty() ? Info.RoomCode : Info.HostName;
		}
		Info.MaxPlayers = FMath::Max(1, Settings.NumPublicConnections + Settings.NumPrivateConnections);
		const int32 Open = Result.Session.NumOpenPublicConnections + Result.Session.NumOpenPrivateConnections;
		Info.CurrentPlayers = FMath::Clamp(Info.MaxPlayers - Open, 1, Info.MaxPlayers);
		Info.PingMs = Result.PingInMs;
		return Info;
	}

	void ReadRoomIdentity(const FOnlineSessionSearchResult& Result, FString& OutCode, FString& OutName)
	{
		Result.Session.SessionSettings.Get(RoomCodeKey, OutCode);
		Result.Session.SessionSettings.Get(RoomNameKey, OutName);
	}
}
