#include "Game/Online/DigetherSessionSubsystem.h"

#include "Game/Frontend/DigetherFrontendSettings.h"
#include "Game/Online/DigetherJoinRules.h"
#include "Game/Online/DigetherRoomProtocol.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystem.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogDigetherSession, Log, All);

namespace
{
	const TCHAR* SteamSocketsDriverPath = TEXT("/Script/SteamSockets.SteamSocketsNetDriver");
	// 코드 참가 대기 중 방을 다시 찾는 간격.
	constexpr float RetryIntervalSeconds = 3.f;
	// 한 번 검색에서 받을 최대 세션 수. 480 시험 App ID는 다른 게임의 로비도 섞여 오므로 넉넉히 받는다.
	constexpr int32 MaxSearchResults = 100;
	// 온라인 ID가 없을 때 요청에 쓰는 로컬 플레이어 번호.
	constexpr int32 LocalPlayerNum = 0;

	// 온라인 ID가 있으면 그것으로, 없으면 로컬 플레이어 번호로 세션 요청을 보낸다.
	template <typename RequestFunc>
	bool RequestAsLocalUser(const FUniqueNetIdPtr& UserId, RequestFunc&& Request)
	{
		return UserId.IsValid() ? Request(*UserId) : Request(LocalPlayerNum);
	}

	FText FormatWithRoomCode(EDigetherSessionText Key, const FString& RoomCode)
	{
		return FText::FormatNamed(DigetherSessionText::Get(Key), TEXT("RoomCode"), FText::FromString(RoomCode));
	}
}

UDigetherSessionSubsystem* UDigetherSessionSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? UGameInstance::GetSubsystem<UDigetherSessionSubsystem>(World->GetGameInstance()) : nullptr;
}

void UDigetherSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SetStatus(MakeIdleStatus());

	if (const IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
	{
		Sessions = Subsystem->GetSessionInterface();
		if (Sessions.IsValid())
		{
			InviteHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(
				FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &ThisClass::HandleInviteAccepted));
		}
	}
	if (GEngine)
	{
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
	}
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
}

void UDigetherSessionSubsystem::Deinitialize()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().ClearTimer(RetryTimer);
		GameInstance->GetTimerManager().ClearTimer(DeadlineTimer);
	}
	if (Sessions.IsValid())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
		Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteHandle);
	}
	Sessions.Reset();
	Search.Reset();
	if (GEngine)
	{
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
	}
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	OnStatusChanged.Clear();
	OnRoomsUpdated.Clear();
	Super::Deinitialize();
}

bool UDigetherSessionSubsystem::IsSteamMode() const
{
	const IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	return Subsystem && Subsystem->GetSubsystemName() == STEAM_SUBSYSTEM;
}

bool UDigetherSessionSubsystem::IsOnlineAvailable() const
{
	const IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	return Subsystem && Subsystem->GetSessionInterface().IsValid()
		&& (Subsystem->GetSubsystemName() == STEAM_SUBSYSTEM || Subsystem->GetSubsystemName() == NULL_SUBSYSTEM);
}

bool UDigetherSessionSubsystem::IsInSession() const
{
	return Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr;
}

FString UDigetherSessionSubsystem::GetLocalPlayerName() const
{
	const IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	const IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	FString Name = Identity.IsValid() ? Identity->GetPlayerNickname(LocalPlayerNum) : FString();
	if (Name.IsEmpty())
	{
		Name = FPlatformProcess::UserName();
	}
	return Name.IsEmpty() ? DigetherSessionText::Get(EDigetherSessionText::DefaultPlayerName).ToString() : Name;
}

FString UDigetherSessionSubsystem::MakeDefaultRoomName() const
{
	// 이름이 길어도 "{PlayerName}" 밖의 글자(접미사 등)는 남기고 플레이어 이름 쪽을 줄인다.
	const FText Pattern = DigetherSessionText::Get(EDigetherSessionText::DefaultRoomName);
	const int32 FixedLength = FText::FormatNamed(Pattern, TEXT("PlayerName"), FText::GetEmpty()).ToString().Len();
	const FString PlayerName = GetLocalPlayerName().Left(FMath::Max(0, DigetherRoomProtocol::MaxRoomNameLength - FixedLength));
	return DigetherRoomProtocol::CleanRoomName(FText::FormatNamed(Pattern, TEXT("PlayerName"), FText::FromString(PlayerName)).ToString());
}

FText UDigetherSessionSubsystem::ConsumeNotice()
{
	FText Result = Notice;
	Notice = FText::GetEmpty();
	return Result;
}

void UDigetherSessionSubsystem::SetStatus(const FText& Message)
{
	Status = Message;
	UE_LOG(LogDigetherSession, Log, TEXT("%s"), *Message.ToString());
	OnStatusChanged.Broadcast();
}

FText UDigetherSessionSubsystem::MakeIdleStatus() const
{
	if (IsSteamMode())
	{
		return FText::FormatNamed(DigetherSessionText::Get(EDigetherSessionText::SteamConnected), TEXT("PlayerName"), FText::FromString(GetLocalPlayerName()));
	}
	const IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	return DigetherSessionText::Get(Subsystem && Subsystem->GetSubsystemName() == NULL_SUBSYSTEM
		? EDigetherSessionText::LanMode
		: EDigetherSessionText::OnlineUnavailable);
}

void UDigetherSessionSubsystem::ReportFailure(const FText& Message)
{
	Notice = Message;
	DestroyAfterFailure();
	SetStatus(Message);
}

FTimerManager& UDigetherSessionSubsystem::GetTimerManager() const
{
	return GetGameInstance()->GetTimerManager();
}

IOnlineSessionPtr UDigetherSessionSubsystem::AcquireSessions(bool bRequireLogin)
{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (!IsOnlineAvailable())
	{
		SetStatus(EDigetherSessionText::OnlineUnavailable);
		return nullptr;
	}
	if (bRequireLogin && IsSteamMode())
	{
		const IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
		if (!Identity.IsValid() || Identity->GetLoginStatus(LocalPlayerNum) != ELoginStatus::LoggedIn || !Identity->GetUniquePlayerId(LocalPlayerNum).IsValid())
		{
			SetStatus(EDigetherSessionText::SteamLoginUnavailable);
			return nullptr;
		}
	}
	Sessions = Subsystem->GetSessionInterface();
	return Sessions;
}

FUniqueNetIdPtr UDigetherSessionSubsystem::GetLocalUserId() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const ULocalPlayer* LocalPlayer = GameInstance ? GameInstance->GetFirstGamePlayer() : nullptr;
	if (LocalPlayer)
	{
		const FUniqueNetIdRepl Preferred = LocalPlayer->GetPreferredUniqueNetId();
		if (Preferred.IsValid())
		{
			return Preferred.GetUniqueNetId();
		}
	}
	const IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	const IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	return Identity.IsValid() ? Identity->GetUniquePlayerId(LocalPlayerNum) : nullptr;
}

bool UDigetherSessionSubsystem::CheckCanRequest(const FString& RoomCode, bool bRequireCode)
{
	if (IsBusy())
	{
		SetStatus(EDigetherSessionText::Busy);
		return false;
	}
	if ((bRequireCode || !RoomCode.IsEmpty()) && !DigetherRoomProtocol::IsValidRoomCode(RoomCode))
	{
		SetStatus(FText::FormatNamed(DigetherSessionText::Get(EDigetherSessionText::InvalidRoomCode), TEXT("MaxLength"), DigetherRoomProtocol::MaxRoomCodeLength));
		return false;
	}
	if (!AcquireSessions(true).IsValid())
	{
		return false;
	}
	if (IsInSession())
	{
		SetStatus(EDigetherSessionText::AlreadyInRoom);
		return false;
	}
	// 메뉴로 돌아온 이유는 사용자가 다음 요청을 보낼 때까지만 보여 준다.
	Notice = FText::GetEmpty();
	return true;
}

bool UDigetherSessionSubsystem::RevertRejectedRequest(bool bRequested, EDigetherSessionOperation RequestOperation)
{
	if (bRequested || Operation != RequestOperation)
	{
		return false;
	}
	Operation = EDigetherSessionOperation::Idle;
	return true;
}

bool UDigetherSessionSubsystem::CheckSteamSocketsAvailable()
{
	if (!IsSteamMode())
	{
		return true;
	}
	const UClass* DriverClass = LoadClass<UNetDriver>(nullptr, SteamSocketsDriverPath);
	const UNetDriver* Driver = DriverClass ? DriverClass->GetDefaultObject<UNetDriver>() : nullptr;
	if (!Driver || !Driver->IsAvailable())
	{
		SetStatus(EDigetherSessionText::SteamSocketsUnavailable);
		return false;
	}
	return true;
}

void UDigetherSessionSubsystem::OpenMap(const FString& Map, const TCHAR* Options)
{
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*Map), true, Options ? FString(Options) : FString());
}

void UDigetherSessionSubsystem::HostRoom(const FString& RoomName, bool bPublic, const FString& RoomCode)
{
	const FString Code = RoomCode.IsEmpty() ? DigetherRoomProtocol::MakeRoomCode() : RoomCode;
	if (!CheckCanRequest(Code, true) || !CheckSteamSocketsAvailable())
	{
		return;
	}

	ActiveRoomCode = Code;
	ActiveRoomName = DigetherRoomProtocol::CleanRoomName(RoomName);
	if (ActiveRoomName.IsEmpty())
	{
		ActiveRoomName = MakeDefaultRoomName();
	}
	Results.Reset();

	const bool bSteam = IsSteamMode();
	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = DigetherRoomProtocol::MaxPlayers;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowInvites = true;
	Settings.bAllowJoinViaPresence = true;
	// Steam 친구 전용 로비는 공개 목록·코드 검색에 나오지 않고 초대·친구 참가로만 들어온다.
	Settings.bAllowJoinViaPresenceFriendsOnly = !bPublic;
	Settings.bIsLANMatch = !bSteam;
	Settings.bUsesPresence = bSteam;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bUseLobbiesVoiceChatIfAvailable = false;
	DigetherRoomProtocol::WriteRoom(Settings, ActiveRoomCode, ActiveRoomName);

	Operation = EDigetherSessionOperation::Creating;
	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleCreateComplete));
	SetStatus(EDigetherSessionText::Creating);
	const bool bRequested = RequestAsLocalUser(GetLocalUserId(), [&](const auto& User)
	{
		return Sessions->CreateSession(User, NAME_GameSession, Settings);
	});
	if (RevertRejectedRequest(bRequested, EDigetherSessionOperation::Creating))
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		SetStatus(EDigetherSessionText::CreateRequestFailed);
	}
}

void UDigetherSessionSubsystem::HandleCreateComplete(FName SessionName, bool bSuccess)
{
	Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	if (Operation != EDigetherSessionOperation::Creating || SessionName != NAME_GameSession)
	{
		return;
	}
	if (!bSuccess)
	{
		Operation = EDigetherSessionOperation::Idle;
		SetStatus(EDigetherSessionText::CreateFailed);
		return;
	}
	Operation = EDigetherSessionOperation::Travelling;
	TravelIntent = ETravelIntent::Host;
	SetStatus(FormatWithRoomCode(EDigetherSessionText::Created, ActiveRoomCode));
	OpenMap(UDigetherFrontendSettings::Get()->GetLobbyMapName(), TEXT("listen"));
}

void UDigetherSessionSubsystem::EndCodeJoin()
{
	CodeJoin = ECodeJoinState::None;
	GetTimerManager().ClearTimer(RetryTimer);
	GetTimerManager().ClearTimer(DeadlineTimer);
}

void UDigetherSessionSubsystem::FindRooms(const FString& RoomCode)
{
	if (!CheckCanRequest(RoomCode, false))
	{
		return;
	}
	EndCodeJoin();
	JoinDeadline = 0.0;
	SearchRoomCode = RoomCode;
	StartSearch();
}

void UDigetherSessionSubsystem::JoinByCode(const FString& RoomCode, float TimeoutSeconds)
{
	if (!CheckCanRequest(RoomCode, true))
	{
		return;
	}
	EndCodeJoin();
	CodeJoin = ECodeJoinState::Pending;
	SearchRoomCode = RoomCode;
	JoinDeadline = TimeoutSeconds > 0.f ? FPlatformTime::Seconds() + TimeoutSeconds : 0.0;
	StartSearch();
	if (CodeJoin == ECodeJoinState::Pending && TimeoutSeconds > 0.f)
	{
		GetTimerManager().SetTimer(DeadlineTimer, this, &ThisClass::HandleJoinDeadline, TimeoutSeconds, false);
	}
}

void UDigetherSessionSubsystem::StartSearch()
{
	Results.Reset();
	Search = MakeShared<FOnlineSessionSearch>();
	Search->bIsLanQuery = !IsSteamMode();
	Search->MaxSearchResults = MaxSearchResults;
	DigetherRoomProtocol::ApplySearchFilter(*Search, SearchRoomCode);
	Operation = EDigetherSessionOperation::Finding;
	FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::HandleFindComplete));
	SetStatus(SearchRoomCode.IsEmpty()
		? DigetherSessionText::Get(EDigetherSessionText::Searching)
		: FormatWithRoomCode(EDigetherSessionText::SearchingCode, SearchRoomCode));
	const TSharedRef<FOnlineSessionSearch> SearchRef = Search.ToSharedRef();
	const bool bRequested = RequestAsLocalUser(GetLocalUserId(), [&](const auto& User)
	{
		return Sessions->FindSessions(User, SearchRef);
	});
	if (RevertRejectedRequest(bRequested, EDigetherSessionOperation::Finding))
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		EndCodeJoin();
		SetStatus(EDigetherSessionText::SearchRequestFailed);
	}
}

void UDigetherSessionSubsystem::HandleFindComplete(bool bSuccess)
{
	Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	if (Operation != EDigetherSessionOperation::Finding)
	{
		return;
	}
	Operation = EDigetherSessionOperation::Idle;
	if (CodeJoin == ECodeJoinState::ExpiredDuringSearch)
	{
		EndCodeJoin();
		SetStatus(EDigetherSessionText::JoinTimeout);
		return;
	}

	Results.Reset();
	RoomInfos.Reset();
	if (bSuccess && Search.IsValid())
	{
		for (const FOnlineSessionSearchResult& Result : Search->SearchResults)
		{
			if (DigetherRoomProtocol::Matches(Result, SearchRoomCode))
			{
				RoomInfos.Add(DigetherRoomProtocol::ReadRoomInfo(Result, Results.Num()));
				Results.Add(Result);
			}
		}
	}
	OnRoomsUpdated.Broadcast();

	if (CodeJoin != ECodeJoinState::Pending)
	{
		SetStatus(bSuccess
			? FText::FormatNamed(DigetherSessionText::Get(EDigetherSessionText::RoomsFound), TEXT("Count"), Results.Num())
			: DigetherSessionText::Get(EDigetherSessionText::SearchFailed));
		return;
	}

	const double Now = FPlatformTime::Seconds();
	const bool bHasDeadline = JoinDeadline > 0.0;
	if (Results.Num() > 0 && (!bHasDeadline || Now <= JoinDeadline))
	{
		EndCodeJoin();
		JoinResult(Results[0]);
		return;
	}
	if (bHasDeadline && Now < JoinDeadline)
	{
		Operation = EDigetherSessionOperation::Waiting;
		SetStatus(FormatWithRoomCode(EDigetherSessionText::WaitingForRoom, SearchRoomCode));
		GetTimerManager().SetTimer(RetryTimer, this, &ThisClass::RetrySearch, RetryIntervalSeconds, false);
		return;
	}
	EndCodeJoin();
	SetStatus(FormatWithRoomCode(EDigetherSessionText::CodeNotFound, SearchRoomCode));
}

void UDigetherSessionSubsystem::RetrySearch()
{
	if (Operation != EDigetherSessionOperation::Waiting)
	{
		return;
	}
	if (FPlatformTime::Seconds() >= JoinDeadline)
	{
		Operation = EDigetherSessionOperation::Idle;
		EndCodeJoin();
		SetStatus(EDigetherSessionText::JoinTimeout);
		return;
	}
	StartSearch();
}

void UDigetherSessionSubsystem::HandleJoinDeadline()
{
	if (CodeJoin != ECodeJoinState::Pending)
	{
		return;
	}
	GetTimerManager().ClearTimer(RetryTimer);
	Results.Reset();
	RoomInfos.Reset();
	OnRoomsUpdated.Broadcast();
	if (Operation == EDigetherSessionOperation::Finding)
	{
		CodeJoin = ECodeJoinState::ExpiredDuringSearch;
		SetStatus(EDigetherSessionText::JoinTimeoutPending);
		return;
	}
	EndCodeJoin();
	Operation = EDigetherSessionOperation::Idle;
	SetStatus(EDigetherSessionText::JoinTimeout);
}

void UDigetherSessionSubsystem::JoinRoom(int32 Index)
{
	if (!CheckCanRequest(FString(), false))
	{
		return;
	}
	if (!Results.IsValidIndex(Index) || !DigetherRoomProtocol::Matches(Results[Index], FString()))
	{
		SetStatus(EDigetherSessionText::StaleRoom);
		return;
	}
	JoinResult(Results[Index]);
}

void UDigetherSessionSubsystem::JoinResult(const FOnlineSessionSearchResult& Result)
{
	DigetherRoomProtocol::ReadRoomIdentity(Result, ActiveRoomCode, ActiveRoomName);
	Operation = EDigetherSessionOperation::Joining;
	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleJoinComplete));
	SetStatus(EDigetherSessionText::Joining);
	const bool bRequested = RequestAsLocalUser(GetLocalUserId(), [&](const auto& User)
	{
		return Sessions->JoinSession(User, NAME_GameSession, Result);
	});
	if (RevertRejectedRequest(bRequested, EDigetherSessionOperation::Joining))
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		SetStatus(EDigetherSessionText::JoinRequestFailed);
	}
}

void UDigetherSessionSubsystem::HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	if (Operation != EDigetherSessionOperation::Joining || SessionName != NAME_GameSession)
	{
		return;
	}
	FString Address;
	APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
	if (Result != EOnJoinSessionCompleteResult::Success || !Sessions->GetResolvedConnectString(NAME_GameSession, Address) || Address.IsEmpty() || !PlayerController)
	{
		Operation = EDigetherSessionOperation::Idle;
		const EDigetherSessionText Reason = Result == EOnJoinSessionCompleteResult::SessionIsFull ? EDigetherSessionText::RoomFull
			: Result == EOnJoinSessionCompleteResult::SessionDoesNotExist ? EDigetherSessionText::RoomClosed
			: EDigetherSessionText::JoinFailed;
		ReportFailure(DigetherSessionText::Get(Reason));
		return;
	}
	Operation = EDigetherSessionOperation::Travelling;
	TravelIntent = ETravelIntent::Join;
	SetStatus(EDigetherSessionText::Connecting);
	PlayerController->ClientTravel(Address, TRAVEL_Absolute);
}

void UDigetherSessionSubsystem::LeaveRoom()
{
	if (Operation == EDigetherSessionOperation::Waiting)
	{
		EndCodeJoin();
		Operation = EDigetherSessionOperation::Idle;
		SetStatus(EDigetherSessionText::WaitCancelled);
		return;
	}
	if (IsBusy())
	{
		SetStatus(EDigetherSessionText::Busy);
		return;
	}
	if (!IsInSession())
	{
		ActiveRoomCode.Reset();
		ActiveRoomName.Reset();
		Operation = EDigetherSessionOperation::Travelling;
		TravelIntent = ETravelIntent::Leave;
		OpenMap(UDigetherFrontendSettings::Get()->GetMainMenuMapName());
		return;
	}
	Operation = EDigetherSessionOperation::Destroying;
	bTravelAfterDestroy = true;
	DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleDestroyComplete));
	SetStatus(EDigetherSessionText::Leaving);
	if (RevertRejectedRequest(Sessions->DestroySession(NAME_GameSession), EDigetherSessionOperation::Destroying))
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
		SetStatus(EDigetherSessionText::LeaveRequestFailed);
	}
}

void UDigetherSessionSubsystem::DestroyAfterFailure()
{
	if (!IsInSession() || Operation == EDigetherSessionOperation::Destroying)
	{
		return;
	}
	Operation = EDigetherSessionOperation::Destroying;
	bTravelAfterDestroy = false;
	DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleDestroyComplete));
	if (RevertRejectedRequest(Sessions->DestroySession(NAME_GameSession), EDigetherSessionOperation::Destroying))
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}
}

void UDigetherSessionSubsystem::HandleDestroyComplete(FName SessionName, bool bSuccess)
{
	Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	if (Operation != EDigetherSessionOperation::Destroying || SessionName != NAME_GameSession)
	{
		return;
	}
	if (!bSuccess)
	{
		Operation = EDigetherSessionOperation::Idle;
		SetStatus(EDigetherSessionText::LeaveFailed);
		return;
	}
	Results.Reset();
	RoomInfos.Reset();
	ActiveRoomCode.Reset();
	ActiveRoomName.Reset();
	if (!bTravelAfterDestroy)
	{
		// 연결 실패 정리는 엔진이 기본 맵으로 돌려보내므로 여기서 이동하지 않는다.
		// 실패 문구는 그대로 두고, 작업이 끝났음을 화면에 알려 버튼을 다시 켠다.
		Operation = EDigetherSessionOperation::Idle;
		OnStatusChanged.Broadcast();
		return;
	}
	Operation = EDigetherSessionOperation::Travelling;
	TravelIntent = ETravelIntent::Leave;
	SetStatus(EDigetherSessionText::Left);
	OpenMap(UDigetherFrontendSettings::Get()->GetMainMenuMapName());
}

bool UDigetherSessionSubsystem::StartGame()
{
	UWorld* World = GetGameInstance()->GetWorld();
	if (!World || World->GetNetMode() != NM_ListenServer)
	{
		SetStatus(EDigetherSessionText::HostOnlyStart);
		return false;
	}
	if (IsBusy())
	{
		SetStatus(EDigetherSessionText::Busy);
		return false;
	}
	const FString GameMap = UDigetherFrontendSettings::Get()->GetGameMapName();
	if (GameMap.IsEmpty())
	{
		SetStatus(EDigetherSessionText::GameMapMissing);
		return false;
	}
	if (Sessions.IsValid())
	{
		if (FOnlineSessionSettings* Settings = Sessions->GetSessionSettings(NAME_GameSession))
		{
			DigetherRoomProtocol::WriteInGame(*Settings, true);
			Sessions->UpdateSession(NAME_GameSession, *Settings, true);
		}
	}
	Operation = EDigetherSessionOperation::Travelling;
	TravelIntent = ETravelIntent::Start;
	SetStatus(EDigetherSessionText::Starting);
	// 대기실과 게임의 PlayerController·PlayerState 클래스가 달라 비심리스 이동으로 다시 접속시킨다.
	if (!World->ServerTravel(GameMap + TEXT("?listen"), true))
	{
		Operation = EDigetherSessionOperation::Idle;
		TravelIntent = ETravelIntent::None;
		SetStatus(EDigetherSessionText::StartTravelFailed);
		return false;
	}
	return true;
}

bool UDigetherSessionSubsystem::ShowInviteUI()
{
	const IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	const IOnlineExternalUIPtr ExternalUI = Subsystem && IsSteamMode() ? Subsystem->GetExternalUIInterface() : nullptr;
	if (!ExternalUI.IsValid() || !IsInSession())
	{
		SetStatus(EDigetherSessionText::InviteSteamOnly);
		return false;
	}
	if (!ExternalUI->ShowInviteUI(LocalPlayerNum, NAME_GameSession))
	{
		SetStatus(EDigetherSessionText::InviteOverlayFailed);
		return false;
	}
	return true;
}

void UDigetherSessionSubsystem::HandleInviteAccepted(bool bSuccess, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& Result)
{
	// 초대 수락은 일반 요청과 안내 문구가 달라 사전 조건을 따로 검사한다.
	DigetherJoinRules::FDigetherInviteCheck Check;
	Check.bBusy = IsBusy();
	if (!Check.bBusy)
	{
		Check.bOnlineAvailable = AcquireSessions(true).IsValid();
	}
	if (!Check.bBusy && Check.bOnlineAvailable)
	{
		Check.bLocalController = ControllerId == LocalPlayerNum;
		Check.bSuccess = bSuccess;
		Check.bUserIdValid = UserId.IsValid();
		Check.bProtocolMatches = DigetherRoomProtocol::Matches(Result, FString());
		Check.bAlreadyInSession = IsInSession();
	}
	switch (DigetherJoinRules::EvaluateInvite(Check))
	{
	case DigetherJoinRules::EDigetherInviteDecision::RejectBusy:
		SetStatus(EDigetherSessionText::InviteBusy);
		return;
	case DigetherJoinRules::EDigetherInviteDecision::RejectOnlineUnavailable:
		// 로그인·온라인 실패의 구체 문구는 AcquireSessions가 설정했다.
		return;
	case DigetherJoinRules::EDigetherInviteDecision::RejectInvalidInvite:
		SetStatus(EDigetherSessionText::InviteVersionMismatch);
		return;
	case DigetherJoinRules::EDigetherInviteDecision::RejectAlreadyInSession:
		SetStatus(EDigetherSessionText::InviteLeaveFirst);
		return;
	case DigetherJoinRules::EDigetherInviteDecision::Accept:
		// 일반 요청과 같이 지난 연결 실패 공지를 다음 참가에 남기지 않는다.
		Notice = FText::GetEmpty();
		JoinResult(Result);
		return;
	}
}

void UDigetherSessionSubsystem::HandlePostLoadMap(UWorld* World)
{
	if (!World || World->GetGameInstance() != GetGameInstance() || Operation != EDigetherSessionOperation::Travelling)
	{
		return;
	}
	Operation = EDigetherSessionOperation::Idle;
	const ETravelIntent Completed = TravelIntent;
	TravelIntent = ETravelIntent::None;
	if (Completed == ETravelIntent::Leave)
	{
		SetStatus(MakeIdleStatus());
		return;
	}
	if (!IsInSession())
	{
		SetStatus(EDigetherSessionText::NoSession);
		return;
	}
	if (IsSteamMode())
	{
		const UNetDriver* Driver = World->GetNetDriver();
		if (!Driver || Driver->GetClass()->GetPathName() != SteamSocketsDriverPath)
		{
			SetStatus(EDigetherSessionText::SteamSocketsDriverMissing);
			return;
		}
	}
	const ENetMode NetMode = World->GetNetMode();
	if ((Completed == ETravelIntent::Host || Completed == ETravelIntent::Start) && NetMode == NM_ListenServer)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("RoomCode"), FText::FromString(ActiveRoomCode));
		Args.Add(TEXT("MaxPlayers"), DigetherRoomProtocol::MaxPlayers);
		SetStatus(FText::Format(DigetherSessionText::Get(EDigetherSessionText::RoomOpen), Args));
	}
	else if (Completed == ETravelIntent::Join && NetMode == NM_Client)
	{
		SetStatus(EDigetherSessionText::HostConnected);
	}
	else
	{
		SetStatus(EDigetherSessionText::NetModeMismatch);
	}
}

void UDigetherSessionSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type Failure, const FString& Error)
{
	if (!World || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	if (Operation == EDigetherSessionOperation::Travelling)
	{
		Operation = EDigetherSessionOperation::Idle;
		TravelIntent = ETravelIntent::None;
	}
	ReportFailure(FText::FormatNamed(DigetherSessionText::Get(EDigetherSessionText::TravelFailed), TEXT("Error"), FText::FromString(Error)));
}

void UDigetherSessionSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Failure, const FString& Error)
{
	if (!World || World->GetGameInstance() != GetGameInstance() || World->GetNetMode() != NM_Client)
	{
		return;
	}
	if (Operation == EDigetherSessionOperation::Travelling)
	{
		Operation = EDigetherSessionOperation::Idle;
		TravelIntent = ETravelIntent::None;
	}
	const bool bLost = Failure == ENetworkFailure::ConnectionLost || Failure == ENetworkFailure::ConnectionTimeout;
	ReportFailure(bLost
		? DigetherSessionText::Get(EDigetherSessionText::ConnectionLost)
		: FText::FormatNamed(DigetherSessionText::Get(EDigetherSessionText::ConnectionFailed), TEXT("Error"), FText::FromString(Error)));
}
