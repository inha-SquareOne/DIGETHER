#include "Game/Economy/DigetherDepositSessionRegistry.h"

#include "Game/Economy/DigetherEconomyRules.h"

namespace
{
	bool IsValidServerTime(const double NowServerSeconds)
	{
		return FMath::IsFinite(NowServerSeconds) && NowServerSeconds >= 0.0;
	}

	bool IsValidCloseReason(const EDigetherDepositSessionCloseReason Reason)
	{
		switch (Reason)
		{
		case EDigetherDepositSessionCloseReason::ExplicitCancel:
		case EDigetherDepositSessionCloseReason::ScreenClosed:
		case EDigetherDepositSessionCloseReason::InteractionLost:
		case EDigetherDepositSessionCloseReason::Death:
		case EDigetherDepositSessionCloseReason::Departure:
		case EDigetherDepositSessionCloseReason::Disconnected:
		case EDigetherDepositSessionCloseReason::Loaded:
		case EDigetherDepositSessionCloseReason::Expired:
		case EDigetherDepositSessionCloseReason::PeriodChanged:
			return true;
		default:
			return false;
		}
	}
}

FDigetherDepositSessionOpenResult FDigetherDepositSessionRegistry::OpenSession(const FString& PlayerKey,
	const FName StationId, const int32 PeriodGeneration, const double NowServerSeconds, const double LifetimeSeconds)
{
	FDigetherDepositSessionOpenResult Result;
	const double ExpiresAtServerSeconds = NowServerSeconds + LifetimeSeconds;
	if (PlayerKey.IsEmpty() || StationId.IsNone() || PeriodGeneration < 0 || !IsValidServerTime(NowServerSeconds)
		|| !FMath::IsFinite(LifetimeSeconds) || LifetimeSeconds <= 0.0
		|| !FMath::IsFinite(ExpiresAtServerSeconds) || ExpiresAtServerSeconds <= NowServerSeconds)
	{
		return Result;
	}

	const int32 StationIndex = Sessions.IndexOfByPredicate([StationId](const FDigetherDepositSession& Session)
	{
		return Session.StationId == StationId;
	});
	// 새 납부대 점유 검사부터 끝내야 이동 실패가 원래 입력 세션을 없애지 않는다.
	if (StationIndex != INDEX_NONE && Sessions[StationIndex].PlayerKey != PlayerKey)
	{
		Result.Result = EDigetherEconomyResult::StationOccupied;
		return Result;
	}

	const int32 PlayerIndex = Sessions.IndexOfByPredicate([&PlayerKey](const FDigetherDepositSession& Session)
	{
		return Session.PlayerKey == PlayerKey;
	});
	if (PlayerIndex != INDEX_NONE && Sessions[PlayerIndex].StationId == StationId)
	{
		// 재열기는 프로토타입처럼 만료만 갱신한다. 기간 일치는 제출 시 다시 검사한다.
		// 만료된 ID 제거와 기간 변경 전체 종료는 호출자가 Expire/CloseAll로 확정한다.
		Sessions[PlayerIndex].ExpiresAtServerSeconds = ExpiresAtServerSeconds;
		Result.Session = Sessions[PlayerIndex];
	}
	else
	{
		FDigetherDepositSession Session;
		Session.PlayerKey = PlayerKey;
		Session.SessionId = FGuid::NewGuid();
		Session.StationId = StationId;
		Session.PeriodGeneration = PeriodGeneration;
		Session.ExpiresAtServerSeconds = ExpiresAtServerSeconds;
		if (PlayerIndex != INDEX_NONE)
		{
			Sessions[PlayerIndex] = Session;
		}
		else
		{
			Sessions.Add(Session);
		}
		Result.Session = Session;
	}
	Result.Result = EDigetherEconomyResult::Success;
	return Result;
}

FDigetherDepositResult FDigetherDepositSessionRegistry::SubmitDeposit(const FDigetherDepositSubmission& Submission,
	FDigetherWalletState& WalletState, FDigetherQuotaState& QuotaState, const FDigetherEconomyTuning& Tuning)
{
	FDigetherDepositResult Result;
	Result.WalletState = WalletState;
	Result.QuotaState = QuotaState;
	if (!IsValidServerTime(Submission.NowServerSeconds))
	{
		return Result;
	}

	const int32 SessionIndex = Sessions.IndexOfByPredicate([&Submission](const FDigetherDepositSession& Session)
	{
		return Session.SessionId == Submission.SessionId;
	});
	if (SessionIndex == INDEX_NONE)
	{
		Result.Result = EDigetherEconomyResult::SessionNotFound;
		return Result;
	}
	const FDigetherDepositSession& Session = Sessions[SessionIndex];
	if (Session.PlayerKey != Submission.PlayerKey)
	{
		Result.Result = EDigetherEconomyResult::SessionOwnerMismatch;
		return Result;
	}
	if (Session.StationId != Submission.StationId)
	{
		Result.Result = EDigetherEconomyResult::StationMismatch;
		return Result;
	}
	if (Session.PeriodGeneration != Submission.PeriodGeneration
		|| Session.PeriodGeneration != QuotaState.PeriodGeneration)
	{
		Result.Result = EDigetherEconomyResult::PeriodMismatch;
		return Result;
	}
	if (Submission.NowServerSeconds >= Session.ExpiresAtServerSeconds)
	{
		Result.Result = EDigetherEconomyResult::SessionExpired;
		return Result;
	}
	Result = FDigetherEconomyRules::CalculateDeposit(WalletState, QuotaState, Submission.AmountCredits,
		Tuning, Submission.bDepositClosed);
	if (Result.Result != EDigetherEconomyResult::Success)
	{
		return Result;
	}

	WalletState = Result.WalletState;
	QuotaState = Result.QuotaState;
	Sessions.RemoveAt(SessionIndex);
	return Result;
}

FDigetherDepositSessionCloseResult FDigetherDepositSessionRegistry::CloseSession(const FString& PlayerKey,
	const FGuid& SessionId, const EDigetherDepositSessionCloseReason Reason)
{
	FDigetherDepositSessionCloseResult Result;
	Result.Reason = Reason;
	if (!IsValidCloseReason(Reason))
	{
		return Result;
	}
	const int32 SessionIndex = Sessions.IndexOfByPredicate([&SessionId](const FDigetherDepositSession& Session)
	{
		return Session.SessionId == SessionId;
	});
	if (SessionIndex == INDEX_NONE)
	{
		Result.Result = EDigetherEconomyResult::SessionNotFound;
		return Result;
	}
	if (Sessions[SessionIndex].PlayerKey != PlayerKey)
	{
		Result.Result = EDigetherEconomyResult::SessionOwnerMismatch;
		return Result;
	}
	Result.ClosedSessions.Add(Sessions[SessionIndex]);
	Sessions.RemoveAt(SessionIndex);
	Result.Result = EDigetherEconomyResult::Success;
	return Result;
}

FDigetherDepositSessionCloseResult FDigetherDepositSessionRegistry::ClosePlayerSession(const FString& PlayerKey,
	const EDigetherDepositSessionCloseReason Reason)
{
	const FDigetherDepositSession* Session = Sessions.FindByPredicate([&PlayerKey](const FDigetherDepositSession& Entry)
	{
		return Entry.PlayerKey == PlayerKey;
	});
	if (Session != nullptr)
	{
		// CloseSession에서 배열을 바꾸기 전에 ID 복사본을 확보한다.
		const FGuid SessionId = Session->SessionId;
		return CloseSession(PlayerKey, SessionId, Reason);
	}
	FDigetherDepositSessionCloseResult Result;
	Result.Reason = Reason;
	Result.Result = IsValidCloseReason(Reason)
		? EDigetherEconomyResult::SessionNotFound : EDigetherEconomyResult::InvalidInput;
	return Result;
}

FDigetherDepositSessionCloseResult FDigetherDepositSessionRegistry::CloseAllSessions(
	const EDigetherDepositSessionCloseReason Reason)
{
	FDigetherDepositSessionCloseResult Result;
	Result.Reason = Reason;
	if (!IsValidCloseReason(Reason))
	{
		return Result;
	}
	Result.ClosedSessions = MoveTemp(Sessions);
	Sessions.Reset();
	Result.Result = EDigetherEconomyResult::Success;
	return Result;
}

FDigetherDepositSessionCloseResult FDigetherDepositSessionRegistry::ExpireSessions(const double NowServerSeconds)
{
	FDigetherDepositSessionCloseResult Result;
	Result.Reason = EDigetherDepositSessionCloseReason::Expired;
	if (!IsValidServerTime(NowServerSeconds))
	{
		return Result;
	}
	for (int32 Index = Sessions.Num() - 1; Index >= 0; --Index)
	{
		if (NowServerSeconds >= Sessions[Index].ExpiresAtServerSeconds)
		{
			Result.ClosedSessions.Add(Sessions[Index]);
			Sessions.RemoveAt(Index);
		}
	}
	Result.Result = EDigetherEconomyResult::Success;
	return Result;
}

TArray<FDigetherDepositSession> FDigetherDepositSessionRegistry::CaptureSessions() const
{
	return Sessions;
}
