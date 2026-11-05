#include "Game/Framework/DigetherGameplayPlayerState.h"

#include "Engine/World.h"
#include "Game/Economy/DigetherEconomyRules.h"
#include "Game/Economy/DigetherWalletComponent.h"
#include "Game/Framework/DigetherFrameworkLog.h"
#include "Game/Life/DigetherLifeRules.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

ADigetherGameplayPlayerState::ADigetherGameplayPlayerState()
{
	bReplicates = true;
	WalletComponent = CreateDefaultSubobject<UDigetherWalletComponent>(TEXT("WalletComponent"));
	LifeComponent = CreateDefaultSubobject<UDigetherLifeStateComponent>(TEXT("LifeComponent"));
}

void ADigetherGameplayPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADigetherGameplayPlayerState, LastRunResult, COND_OwnerOnly);
}

void ADigetherGameplayPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);
	if (ADigetherGameplayPlayerState* Target = Cast<ADigetherGameplayPlayerState>(PlayerState))
	{
		if (!CopyGameplayStateTo(Target))
		{
			UE_LOG(LogDigetherFramework, Error, TEXT("PlayerState의 지갑·생존 상태 인계를 거절했습니다."));
		}
	}
}

void ADigetherGameplayPlayerState::OverrideWith(APlayerState* PlayerState)
{
	Super::OverrideWith(PlayerState);
	const ADigetherGameplayPlayerState* Source = Cast<ADigetherGameplayPlayerState>(PlayerState);
	// inactive 재활성화 시 엔진은 새 로그인 상태를 전달한다. 기본값으로 최신 장부를 덮지 않는다.
	if (IsValid(Source) && Source->IsGameplayInitialized() && !Source->CopyGameplayStateTo(this))
	{
		UE_LOG(LogDigetherFramework, Error, TEXT("PlayerState의 지갑·생존 상태 복원을 거절했습니다."));
	}
}

bool ADigetherGameplayPlayerState::CopyGameplayStateTo(ADigetherGameplayPlayerState* Target) const
{
	if (!HasAuthority() || !IsValid(Target) || !Target->HasAuthority()
		|| !IsValid(WalletComponent) || !IsValid(LifeComponent)
		|| !IsValid(Target->WalletComponent) || !IsValid(Target->LifeComponent))
	{
		return false;
	}
	const FDigetherWalletState Wallet = WalletComponent->CaptureState();
	const FDigetherLifeComponentState Life = LifeComponent->CaptureState();
	if (!FDigetherEconomyRules::IsValidWalletState(Wallet) || !FDigetherLifeRules::IsValidState(Life))
	{
		return false;
	}
	// 두 setter는 권한과 값만 검사한다. 모두 검사한 뒤 외부 알림 없이 같은 시점의 값을 인계한다.
	if (Target->WalletComponent->RestoreState(Wallet, false) != EDigetherEconomyResult::Success
		|| Target->LifeComponent->RestoreState(Life, false) != EDigetherLifeResult::Success)
	{
		return false;
	}
	Target->GameplayPlayerId = GameplayPlayerId;
	Target->bGameplayInitialized = bGameplayInitialized;
	Target->JoinOrder = JoinOrder;
	Target->LastRunResult = LastRunResult;
	Target->WalletComponent->NotifyStateChanged();
	Target->LifeComponent->NotifyStateChanged();
	Target->OnRep_LastRunResult();
	return true;
}

void ADigetherGameplayPlayerState::OnDeactivated()
{
	const UWorld* World = GetWorld();
	if (HasAuthority() && IsInactive() && World != nullptr && !World->bIsTearingDown)
	{
		// GameMode의 inactive 배열이 세션 수명을 소유한다. 끊긴 동안에도 같은 컴포넌트를 갱신한다.
		return;
	}
	Super::OnDeactivated();
}

void ADigetherGameplayPlayerState::OnReactivated()
{
	Super::OnReactivated();
	if (HasAuthority())
	{
		SetIsInactive(false);
		ForceNetUpdate();
	}
}

UDigetherWalletComponent* ADigetherGameplayPlayerState::GetWalletComponent() const
{
	return WalletComponent.Get();
}

UDigetherLifeStateComponent* ADigetherGameplayPlayerState::GetLifeComponent() const
{
	return LifeComponent.Get();
}

void ADigetherGameplayPlayerState::InitializeGameplayIdentity()
{
	if (HasAuthority() && !GameplayPlayerId.IsValid())
	{
		GameplayPlayerId = FGuid::NewGuid();
	}
}

bool ADigetherGameplayPlayerState::RestoreGameplayIdentity(const FGuid RestoredPlayerId)
{
	if (!HasAuthority() || !RestoredPlayerId.IsValid())
	{
		return false;
	}
	GameplayPlayerId = RestoredPlayerId;
	return true;
}

FGuid ADigetherGameplayPlayerState::GetGameplayPlayerId() const
{
	return GameplayPlayerId;
}

FString ADigetherGameplayPlayerState::GetGameplayPlayerKey() const
{
	return GameplayPlayerId.IsValid() ? GameplayPlayerId.ToString() : FString();
}

void ADigetherGameplayPlayerState::MarkGameplayInitialized()
{
	if (HasAuthority())
	{
		bGameplayInitialized = true;
	}
}

void ADigetherGameplayPlayerState::SetJoinOrder(const int32 InJoinOrder)
{
	if (HasAuthority())
	{
		JoinOrder = FMath::Max(0, InJoinOrder);
	}
}

void ADigetherGameplayPlayerState::RetainPawn(APawn* Pawn)
{
	if (HasAuthority() && (Pawn == nullptr || (IsValid(Pawn) && Pawn->GetWorld() == GetWorld())))
	{
		RetainedPawn = Pawn;
	}
}

APawn* ADigetherGameplayPlayerState::GetRetainedPawn() const
{
	return RetainedPawn.Get();
}

APawn* ADigetherGameplayPlayerState::GetGameplayPawn() const
{
	APawn* Pawn = GetPawn();
	return IsValid(Pawn) ? Pawn : RetainedPawn.Get();
}

FDigetherWalletState ADigetherGameplayPlayerState::CaptureState() const
{
	return WalletComponent->CaptureState();
}

EDigetherEconomyResult ADigetherGameplayPlayerState::RestoreState(const FDigetherWalletState& State, const bool bNotify)
{
	return WalletComponent->RestoreState(State, bNotify);
}

bool ADigetherGameplayPlayerState::PublishRunResult(const FDigetherRunResult& Result)
{
	if (!HasAuthority() || !Result.IsValid() || !IsValid(WalletComponent)
		|| Result.FinalBalanceCredits != WalletComponent->CaptureState().BalanceCredits)
	{
		return false;
	}
	LastRunResult = Result;
	ForceNetUpdate();
	OnRep_LastRunResult();
	return true;
}

void ADigetherGameplayPlayerState::ClearRunResult()
{
	if (HasAuthority() && LastRunResult.RunId.IsValid())
	{
		LastRunResult = FDigetherRunResult();
		ForceNetUpdate();
		OnRep_LastRunResult();
	}
}

void ADigetherGameplayPlayerState::OnRep_LastRunResult()
{
	OnRunResultChanged.Broadcast(LastRunResult);
}
