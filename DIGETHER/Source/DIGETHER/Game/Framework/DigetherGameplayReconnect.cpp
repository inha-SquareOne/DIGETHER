#include "Game/Framework/DigetherGameplayGameMode.h"

#include "Engine/World.h"
#include "Game/Elevator/DigetherElevatorLeverActor.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"

bool ADigetherGameplayGameMode::ReadyToStartMatch_Implementation()
{
	return bRunReady;
}

APlayerController* ADigetherGameplayGameMode::Login(UPlayer* NewPlayer, const ENetRole InRemoteRole,
	const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	// 무효 ID의 로컬 직접 실행도 정원·탐사 잠금을 검사하며 저장·재접속 키로 승격하지 않는다.
	if (UniqueId.IsValid())
	{
		PreLogin(Options, FString(), UniqueId, ErrorMessage);
	}
	else if (!CheckPlayerAdmission(UniqueId, true, ErrorMessage))
	{
		return nullptr;
	}
	if (!ErrorMessage.IsEmpty())
	{
		return nullptr;
	}
	APlayerController* Controller = Super::Login(NewPlayer, InRemoteRole, Portal, Options, UniqueId, ErrorMessage);
	if (IsValid(Controller) && ErrorMessage.IsEmpty())
	{
		// PostLogin 대기 중에도 임시 PS와 보존 PS가 같은 ID의 인원을 중복 예약하지 않게 연결한다.
		FindInactivePlayer(Controller);
	}
	return Controller;
}

void ADigetherGameplayGameMode::NotifyPlayerDisconnecting(ADigetherGameplayPlayerController* Controller)
{
	if (!HasAuthority() || !IsValid(Controller) || Controller->GetWorld() != GetWorld())
	{
		return;
	}
	// Pawn 해제 전에 레버 점유를 풀어 끊긴 입력이 완료로 처리되지 않게 한다.
	for (const TWeakObjectPtr<ADigetherElevatorLeverActor>& Lever : LeverActors)
	{
		ReleaseLeverPull(Controller, Lever.Get());
	}
}

void ADigetherGameplayGameMode::AddInactivePlayer(APlayerState* PlayerState, APlayerController* PC)
{
	ADigetherGameplayPlayerState* Player = Cast<ADigetherGameplayPlayerState>(PlayerState);
	if (!HasAuthority() || !IsValid(Player) || !IsValid(PC) || PC->PlayerState != Player
		|| Player->GetWorld() != GetWorld() || PC->GetWorld() != GetWorld()
		|| !Player->IsGameplayInitialized() || !Player->GetGameplayPlayerId().IsValid()
		|| !Player->GetUniqueId().IsValid() || GetWorld()->bIsTearingDown)
	{
		return;
	}
	// Duplicate하면 RunParticipants와 남은 Pawn이 이전 상태를 가리키므로 원본을 보존한다.
	Player->SetIsInactive(true);
	Player->SetReplicates(false);
	Player->SetLifeSpan(0.f);
	Player->SetOwner(nullptr);
	if (IsValid(GameState))
	{
		GameState->RemovePlayerState(Player);
	}
	InactivePlayerArray.AddUnique(Player);
	// 재빙의 대기는 현재 연결의 작업이다. 다시 끊기면 다음 로그인에서 새로 등록한다.
	ReconnectedPlayers.Remove(Player);
}

bool ADigetherGameplayGameMode::FindInactivePlayer(APlayerController* PC)
{
	ADigetherGameplayPlayerState* Fresh = IsValid(PC) ? PC->GetPlayerState<ADigetherGameplayPlayerState>() : nullptr;
	if (!HasAuthority() || !IsValid(Fresh) || PC->GetWorld() != GetWorld()
		|| Fresh->GetWorld() != GetWorld() || !Fresh->GetUniqueId().IsValid() || GetWorld()->bIsTearingDown)
	{
		return false;
	}
	for (int32 Index = 0; Index < InactivePlayerArray.Num(); ++Index)
	{
		ADigetherGameplayPlayerState* Existing = Cast<ADigetherGameplayPlayerState>(InactivePlayerArray[Index]);
		if (!IsValid(Existing) || Existing->GetWorld() != GetWorld() || !Existing->IsInactive()
			|| !Existing->IsGameplayInitialized() || !Existing->GetGameplayPlayerId().IsValid()
			|| !Existing->GetUniqueId().IsValid()
			|| Existing->GetUniqueId() != Fresh->GetUniqueId())
		{
			continue;
		}
		PC->SetPlayerState(Existing);
		Existing->SetOwner(PC);
		Existing->SetIsInactive(false);
		Existing->SetReplicates(true);
		Existing->SetLifeSpan(0.f);
		if (IsValid(GameState))
		{
			GameState->RemovePlayerState(Fresh);
			GameState->AddPlayerState(Existing);
		}
		InactivePlayerArray.RemoveAt(Index);
		ReconnectedPlayers.AddUnique(Existing);
		// 임시 로그인 PS가 같은 ID의 온라인 등록을 해제하지 않도록 엔진 순서를 따른다.
		Fresh->SetIsInactive(true);
		Fresh->SetUniqueId(FUniqueNetIdRepl());
		Fresh->Destroy();
		Existing->OnReactivated();
		return true;
	}
	return false;
}
