#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "DigetherPrototypeGameInstance.generated.h"

UCLASS()
class DIGETHER_API UDigetherPrototypeGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void OnStart() override;
	virtual void Shutdown() override;

	UFUNCTION(Exec, BlueprintCallable, Category="Prototype|Steam")
	void SteamHost(const FString& Room);
	UFUNCTION(Exec, BlueprintCallable, Category="Prototype|Steam")
	void SteamFind(const FString& Room);
	UFUNCTION(Exec, BlueprintCallable, Category="Prototype|Steam")
	void SteamJoin(int32 Index);
	UFUNCTION(Exec, BlueprintCallable, Category="Prototype|Steam")
	void SteamLeave();
	UFUNCTION(BlueprintPure, Category="Prototype|Steam")
	FString GetSteamStatus() const { return Status; }

private:
	enum class EOperation : uint8 { Idle, Creating, Finding, Waiting, Joining, Destroying, Travelling };
	enum class ETravelIntent : uint8 { None, Host, Join, Leave };
	IOnlineSessionPtr GetSteamSessions();
	bool ValidateRoom(const FString& Room) const;
	bool Matches(const FOnlineSessionSearchResult& Result, const FString& Room) const;
	void SetStatus(const FString& Message);
	void StartFind();
	void RetryFind();
	void OnJoinDeadline();
	void JoinResult(const FOnlineSessionSearchResult& Result);
	void OnCreate(FName SessionName, bool bSuccess);
	void OnFind(bool bSuccess);
	void OnJoin(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroy(FName SessionName, bool bSuccess);
	void OnInvite(bool bSuccess, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& Result);
	void OnTravelFailure(UWorld* World, ETravelFailure::Type Failure, const FString& Error);
	void OnNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Failure, const FString& Error);
	void OnPostLoadMap(UWorld* World);
	void ClearDelegates();

	EOperation Operation = EOperation::Idle;
	ETravelIntent TravelIntent = ETravelIntent::None;
	FString Status = TEXT("Steam 대기 중");
	FString ActiveRoom;
	bool bAutoJoin = false;
	bool bDeadlineExpired = false;
	double JoinDeadline = 0.0;
	TSharedPtr<FOnlineSessionSearch> Search;
	TArray<FOnlineSessionSearchResult> Results;
	IOnlineSessionPtr Sessions;
	FTimerHandle RetryTimer, DeadlineTimer;
	FDelegateHandle CreateHandle, FindHandle, JoinHandle, DestroyHandle, InviteHandle;
	FDelegateHandle TravelFailureHandle, NetworkFailureHandle, PostLoadMapHandle;
};
