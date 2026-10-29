#include "DigetherPrototypeGameInstance.h"

#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "OnlineSubsystem.h"
#include "Online/OnlineSessionNames.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogDigetherPrototypeSteam, Log, All);

namespace
{
	const FName PrototypeKey(TEXT("DIGETHER_PROTO"));
	const FName RoomKey(TEXT("DIGETHER_ROOM"));
	const FString PrototypeVersion(TEXT("maze-school-v1"));
	const TCHAR* PrototypeMap = TEXT("/Game/Prototype/Maps/L_Prototype");
}

void UDigetherPrototypeGameInstance::Init()
{
	Super::Init();
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (FParse::Param(FCommandLine::Get(), TEXT("nosteam")))
	{
		SetStatus(TEXT("로컬 IP 시험 모드 (-nosteam)"));
	}
	else if (Subsystem && Subsystem->GetSubsystemName() == NULL_SUBSYSTEM)
	{
		SetStatus(TEXT("로컬 IP 시험 모드 (Null)"));
	}
	else if (!Subsystem || Subsystem->GetSubsystemName() != STEAM_SUBSYSTEM)
	{
		SetStatus(TEXT("Steam 온라인 서비스를 사용할 수 없습니다."));
	}
	if (Subsystem && Subsystem->GetSubsystemName() == STEAM_SUBSYSTEM)
	{
		Sessions = Subsystem->GetSessionInterface();
		if (Sessions.IsValid()) InviteHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(
			FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &ThisClass::OnInvite));
	}
	if (GEngine)
	{
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::OnTravelFailure);
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::OnNetworkFailure);
	}
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::OnPostLoadMap);
}

void UDigetherPrototypeGameInstance::OnStart()
{
	Super::OnStart();
	const bool bHost = FParse::Param(FCommandLine::Get(), TEXT("PrototypeSteamHost"));
	const bool bJoin = FParse::Param(FCommandLine::Get(), TEXT("PrototypeSteamJoin"));
	FString Room;
	FParse::Value(FCommandLine::Get(), TEXT("PrototypeSteamRoom="), Room);
	if (bHost && bJoin) { SetStatus(TEXT("Steam 호스트와 참가 옵션을 동시에 사용할 수 없습니다.")); return; }
	if (bHost) SteamHost(Room);
	else if (bJoin)
	{
		bAutoJoin = true;
		JoinDeadline = FPlatformTime::Seconds() + 60.0;
		SteamFind(Room);
		if (bAutoJoin) GetTimerManager().SetTimer(DeadlineTimer, this, &ThisClass::OnJoinDeadline, 60.f, false);
	}
	else if (!Room.IsEmpty() && ValidateRoom(Room)) ActiveRoom = Room;
}

void UDigetherPrototypeGameInstance::Shutdown()
{
	GetTimerManager().ClearTimer(RetryTimer);
	GetTimerManager().ClearTimer(DeadlineTimer);
	ClearDelegates();
	Sessions.Reset();
	Search.Reset();
	if (GEngine)
	{
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
	}
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	Super::Shutdown();
}

void UDigetherPrototypeGameInstance::ClearDelegates()
{
	if (!Sessions.IsValid()) return;
	Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteHandle);
}

void UDigetherPrototypeGameInstance::SetStatus(const FString& Message)
{
	Status = Message;
	UE_LOG(LogDigetherPrototypeSteam, Log, TEXT("%s"), *Message);
}

IOnlineSessionPtr UDigetherPrototypeGameInstance::GetSteamSessions()
{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (!Subsystem || Subsystem->GetSubsystemName() != STEAM_SUBSYSTEM)
	{
		SetStatus(TEXT("Steam 온라인 서비스를 사용할 수 없습니다."));
		return nullptr;
	}
	const IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
	if (!Identity.IsValid() || Identity->GetLoginStatus(0) != ELoginStatus::LoggedIn || !Identity->GetUniquePlayerId(0).IsValid())
	{
		SetStatus(TEXT("Steam 로그인 상태를 확인할 수 없습니다."));
		return nullptr;
	}
	Sessions = Subsystem->GetSessionInterface();
	if (!Sessions.IsValid()) SetStatus(TEXT("Steam 세션을 사용할 수 없습니다."));
	return Sessions;
}

bool UDigetherPrototypeGameInstance::ValidateRoom(const FString& Room) const
{
	if (Room.IsEmpty() || Room.Len() > 32) return false;
	for (const TCHAR C : Room)
		if (!((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9') || C == '-' || C == '_')) return false;
	return true;
}

bool UDigetherPrototypeGameInstance::Matches(const FOnlineSessionSearchResult& Result, const FString& Room) const
{
	FString Version, FoundRoom;
	return Result.IsValid() && Result.Session.SessionSettings.BuildUniqueId == GetBuildUniqueId()
		&& Result.Session.SessionSettings.Get(PrototypeKey, Version) && Version.Equals(PrototypeVersion, ESearchCase::CaseSensitive)
		&& Result.Session.SessionSettings.Get(RoomKey, FoundRoom) && ValidateRoom(FoundRoom)
		&& (Room.IsEmpty() || FoundRoom.Equals(Room, ESearchCase::CaseSensitive));
}

void UDigetherPrototypeGameInstance::SteamHost(const FString& Room)
{
	if (Operation != EOperation::Idle) { SetStatus(TEXT("다른 Steam 작업이 진행 중입니다.")); return; }
	if (!ValidateRoom(Room)) { SetStatus(TEXT("방 코드는 영문·숫자·하이픈·밑줄 1~32자여야 합니다.")); return; }
	if (!GetSteamSessions().IsValid()) return;
	if (Sessions->GetNamedSession(NAME_GameSession)) { SetStatus(TEXT("기존 Steam 세션에서 먼저 나가세요.")); return; }
	const UClass* SteamDriverClass = LoadClass<UNetDriver>(nullptr, TEXT("/Script/SteamSockets.SteamSocketsNetDriver"));
	const UNetDriver* SteamDriver = SteamDriverClass ? SteamDriverClass->GetDefaultObject<UNetDriver>() : nullptr;
	if (!SteamDriver || !SteamDriver->IsAvailable())
	{
		SetStatus(TEXT("SteamSockets를 사용할 수 없어 방을 열지 않았습니다."));
		return;
	}
	ActiveRoom = Room;
	Results.Reset();
	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = 4;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowInvites = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bIsLANMatch = false;
	Settings.bUsesPresence = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bUseLobbiesVoiceChatIfAvailable = false;
	Settings.Set(PrototypeKey, PrototypeVersion, EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(RoomKey, Room, EOnlineDataAdvertisementType::ViaOnlineService);
	Operation = EOperation::Creating;
	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreate));
	SetStatus(TEXT("Steam 방을 만들고 있습니다."));
	if (!Sessions->CreateSession(0, NAME_GameSession, Settings) && Operation == EOperation::Creating)
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		Operation = EOperation::Idle;
		SetStatus(TEXT("Steam 방 생성 요청에 실패했습니다."));
	}
}

void UDigetherPrototypeGameInstance::OnCreate(FName SessionName, bool bSuccess)
{
	Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	if (Operation != EOperation::Creating || SessionName != NAME_GameSession) return;
	if (!bSuccess) { Operation = EOperation::Idle; SetStatus(TEXT("Steam 방 생성에 실패했습니다.")); return; }
	Operation = EOperation::Travelling;
	TravelIntent = ETravelIntent::Host;
	SetStatus(TEXT("Steam 방을 만들었습니다. 호스트 맵으로 이동합니다."));
	UGameplayStatics::OpenLevel(this, FName(PrototypeMap), true, TEXT("listen"));
}

void UDigetherPrototypeGameInstance::SteamFind(const FString& Room)
{
	if (Operation != EOperation::Idle) { bAutoJoin = false; SetStatus(TEXT("다른 Steam 작업이 진행 중입니다.")); return; }
	if (!ValidateRoom(Room)) { bAutoJoin = false; SetStatus(TEXT("방 코드는 영문·숫자·하이픈·밑줄 1~32자여야 합니다.")); return; }
	if (!GetSteamSessions().IsValid()) { bAutoJoin = false; return; }
	if (Sessions->GetNamedSession(NAME_GameSession)) { bAutoJoin = false; SetStatus(TEXT("기존 Steam 세션에서 먼저 나가세요.")); return; }
	ActiveRoom = Room;
	StartFind();
}

void UDigetherPrototypeGameInstance::StartFind()
{
	Results.Reset();
	Search = MakeShared<FOnlineSessionSearch>();
	Search->bIsLanQuery = false;
	Search->MaxSearchResults = 100;
	Search->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	Search->QuerySettings.Set(PrototypeKey, PrototypeVersion, EOnlineComparisonOp::Equals);
	Search->QuerySettings.Set(RoomKey, ActiveRoom, EOnlineComparisonOp::Equals);
	Operation = EOperation::Finding;
	FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFind));
	SetStatus(TEXT("Steam 방을 찾고 있습니다."));
	if (!Sessions->FindSessions(0, Search.ToSharedRef()) && Operation == EOperation::Finding)
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		Operation = EOperation::Idle;
		bAutoJoin = false;
		SetStatus(TEXT("Steam 방 검색 요청에 실패했습니다."));
	}
}

void UDigetherPrototypeGameInstance::OnFind(bool bSuccess)
{
	Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	if (Operation != EOperation::Finding) return;
	Operation = EOperation::Idle;
	if (bDeadlineExpired)
	{
		bDeadlineExpired = false;
		SetStatus(TEXT("60초 안에 Steam 방을 찾지 못했습니다."));
		return;
	}
	if (bSuccess && Search.IsValid())
		for (const FOnlineSessionSearchResult& Result : Search->SearchResults)
			if (Matches(Result, ActiveRoom)) Results.Add(Result);
	if (bAutoJoin && Results.Num() > 0 && FPlatformTime::Seconds() <= JoinDeadline)
	{
		bAutoJoin = false;
		GetTimerManager().ClearTimer(DeadlineTimer);
		JoinResult(Results[0]);
		return;
	}
	if (bAutoJoin && FPlatformTime::Seconds() < JoinDeadline)
	{
		Operation = EOperation::Waiting;
		SetStatus(TEXT("Steam 방을 기다리고 있습니다."));
		GetTimerManager().SetTimer(RetryTimer, this, &ThisClass::RetryFind, 3.f, false);
		return;
	}
	if (bAutoJoin) { bAutoJoin = false; SetStatus(TEXT("60초 안에 Steam 방을 찾지 못했습니다.")); return; }
	SetStatus(bSuccess ? FString::Printf(TEXT("일치하는 Steam 방 %d개를 찾았습니다. SteamJoin 인덱스로 참가하세요."), Results.Num()) : TEXT("Steam 방 검색에 실패했습니다."));
}

void UDigetherPrototypeGameInstance::RetryFind()
{
	if (Operation != EOperation::Waiting) return;
	if (FPlatformTime::Seconds() >= JoinDeadline)
	{
		Operation = EOperation::Idle;
		bAutoJoin = false;
		SetStatus(TEXT("60초 안에 Steam 방을 찾지 못했습니다."));
		return;
	}
	StartFind();
}

void UDigetherPrototypeGameInstance::OnJoinDeadline()
{
	if (!bAutoJoin) return;
	bAutoJoin = false;
	GetTimerManager().ClearTimer(RetryTimer);
	Results.Reset();
	if (Operation == EOperation::Finding)
	{
		// Steam의 로비 검색은 실제 취소되지 않으므로 늦은 완료가 새 검색에 섞이지 않게 기다린다.
		bDeadlineExpired = true;
		SetStatus(TEXT("60초 안에 Steam 방을 찾지 못했습니다. 진행 중인 검색 종료를 기다립니다."));
		return;
	}
	Operation = EOperation::Idle;
	SetStatus(TEXT("60초 안에 Steam 방을 찾지 못했습니다."));
}

void UDigetherPrototypeGameInstance::SteamJoin(int32 Index)
{
	if (Operation != EOperation::Idle) { SetStatus(TEXT("다른 Steam 작업이 진행 중입니다.")); return; }
	if (!GetSteamSessions().IsValid()) return;
	if (Sessions->GetNamedSession(NAME_GameSession)) { SetStatus(TEXT("기존 Steam 세션에서 먼저 나가세요.")); return; }
	if (!Results.IsValidIndex(Index) || !Matches(Results[Index], ActiveRoom)) { SetStatus(TEXT("유효한 Steam 방 인덱스가 아닙니다. 다시 검색하세요.")); return; }
	JoinResult(Results[Index]);
}

void UDigetherPrototypeGameInstance::JoinResult(const FOnlineSessionSearchResult& Result)
{
	Operation = EOperation::Joining;
	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoin));
	SetStatus(TEXT("Steam 방에 참가하고 있습니다."));
	if (!Sessions->JoinSession(0, NAME_GameSession, Result) && Operation == EOperation::Joining)
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		Operation = EOperation::Idle;
		SetStatus(TEXT("Steam 방 참가 요청에 실패했습니다."));
	}
}

void UDigetherPrototypeGameInstance::OnJoin(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	if (Operation != EOperation::Joining || SessionName != NAME_GameSession) return;
	FString Address;
	APlayerController* PC = GetFirstLocalPlayerController();
	if (Result != EOnJoinSessionCompleteResult::Success || !Sessions->GetResolvedConnectString(NAME_GameSession, Address) || Address.IsEmpty() || !PC)
	{
		Operation = EOperation::Idle;
		SetStatus(TEXT("Steam 방 참가 또는 접속 주소 확인에 실패했습니다. 세션에서 나간 후 다시 시도하세요."));
		return;
	}
	Operation = EOperation::Travelling;
	TravelIntent = ETravelIntent::Join;
	SetStatus(TEXT("Steam 호스트에 연결하고 있습니다."));
	PC->ClientTravel(Address, TRAVEL_Absolute);
}

void UDigetherPrototypeGameInstance::SteamLeave()
{
	if (Operation == EOperation::Waiting)
	{
		GetTimerManager().ClearTimer(RetryTimer);
		GetTimerManager().ClearTimer(DeadlineTimer);
		bAutoJoin = false;
		Operation = EOperation::Idle;
		SetStatus(TEXT("Steam 방 대기를 취소했습니다."));
		return;
	}
	if (Operation != EOperation::Idle) { SetStatus(TEXT("다른 Steam 작업이 진행 중입니다.")); return; }
	// 로그인 연결이 끊겼더라도 남은 세션 정리는 요청할 수 있어야 한다.
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	Sessions = Subsystem && Subsystem->GetSubsystemName() == STEAM_SUBSYSTEM ? Subsystem->GetSessionInterface() : nullptr;
	if (!Sessions.IsValid()) { SetStatus(TEXT("Steam 세션 서비스를 사용할 수 없어 종료할 수 없습니다.")); return; }
	if (!Sessions->GetNamedSession(NAME_GameSession)) { SetStatus(TEXT("나갈 Steam 세션이 없습니다.")); return; }
	Operation = EOperation::Destroying;
	DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroy));
	SetStatus(TEXT("Steam 세션에서 나가고 있습니다."));
	if (!Sessions->DestroySession(NAME_GameSession) && Operation == EOperation::Destroying)
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
		Operation = EOperation::Idle;
		SetStatus(TEXT("Steam 세션 종료 요청에 실패했습니다."));
	}
}

void UDigetherPrototypeGameInstance::OnDestroy(FName SessionName, bool bSuccess)
{
	Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	if (Operation != EOperation::Destroying || SessionName != NAME_GameSession) return;
	if (!bSuccess) { Operation = EOperation::Idle; SetStatus(TEXT("Steam 세션 종료에 실패했습니다.")); return; }
	Results.Reset();
	ActiveRoom.Reset();
	Operation = EOperation::Travelling;
	TravelIntent = ETravelIntent::Leave;
	SetStatus(TEXT("Steam 세션을 종료했습니다. 시험 맵으로 돌아갑니다."));
	UGameplayStatics::OpenLevel(this, FName(PrototypeMap), true);
}

void UDigetherPrototypeGameInstance::OnInvite(bool bSuccess, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& Result)
{
	if (Operation != EOperation::Idle) { SetStatus(TEXT("다른 Steam 작업 중에는 초대에 참가할 수 없습니다.")); return; }
	if (!GetSteamSessions().IsValid()) return;
	if (ControllerId != 0 || !bSuccess || !UserId.IsValid() || !Matches(Result, ActiveRoom)) { SetStatus(TEXT("이 프로젝트·버전·방 코드와 일치하지 않는 Steam 초대입니다.")); return; }
	if (Sessions->GetNamedSession(NAME_GameSession)) { SetStatus(TEXT("초대를 수락하려면 기존 Steam 세션에서 먼저 나가세요.")); return; }
	Result.Session.SessionSettings.Get(RoomKey, ActiveRoom);
	JoinResult(Result);
}

void UDigetherPrototypeGameInstance::OnPostLoadMap(UWorld* World)
{
	if (World && World->GetGameInstance() == this && Operation == EOperation::Travelling)
	{
		Operation = EOperation::Idle;
		const ETravelIntent CompletedTravel = TravelIntent;
		TravelIntent = ETravelIntent::None;
		if (CompletedTravel == ETravelIntent::Leave)
		{
			SetStatus(TEXT("Steam 세션을 종료하고 시험 맵으로 돌아왔습니다."));
			return;
		}
		if (!Sessions.IsValid() || !Sessions->GetNamedSession(NAME_GameSession))
		{
			SetStatus(TEXT("Steam 연결 실패: 세션이 없습니다. 다시 시도하세요."));
			return;
		}
		const UNetDriver* Driver = World->GetNetDriver();
		const bool bSteamDriver = Driver && Driver->GetClass()->GetPathName() == TEXT("/Script/SteamSockets.SteamSocketsNetDriver");
		if (!bSteamDriver)
		{
			SetStatus(TEXT("Steam 연결 실패: SteamSockets 전송 드라이버를 확인할 수 없습니다. 세션에서 나간 후 다시 시도하세요."));
			return;
		}
		if (CompletedTravel == ETravelIntent::Host && World->GetNetMode() == NM_ListenServer)
		{
			SetStatus(FString::Printf(TEXT("Steam 호스트 열림: 방 코드 %s, 최대 4명"), *ActiveRoom));
		}
		else if (CompletedTravel == ETravelIntent::Join && World->GetNetMode() == NM_Client)
		{
			SetStatus(TEXT("Steam 호스트 연결 완료"));
		}
		else
		{
			SetStatus(TEXT("Steam 연결 실패: 호스트 또는 클라이언트 네트워크 상태가 아닙니다. 세션에서 나간 후 다시 시도하세요."));
		}
	}
}

void UDigetherPrototypeGameInstance::OnTravelFailure(UWorld* World, ETravelFailure::Type Failure, const FString& Error)
{
	if (World && World->GetGameInstance() == this && Operation == EOperation::Travelling)
	{
		Operation = EOperation::Idle;
		TravelIntent = ETravelIntent::None;
		SetStatus(FString::Printf(TEXT("Steam 맵 이동에 실패했습니다: %s"), *Error));
	}
}

void UDigetherPrototypeGameInstance::OnNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Failure, const FString& Error)
{
	if (World && World->GetGameInstance() == this && Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession))
	{
		if (Operation == EOperation::Travelling)
		{
			Operation = EOperation::Idle;
			TravelIntent = ETravelIntent::None;
		}
		SetStatus(FString::Printf(TEXT("Steam 연결에 실패했습니다: %s. 세션에서 나간 후 다시 시도하세요."), *Error));
	}
}
