// DigetherSaveRules의 주·백업 슬롯 선택·쓰기 순서 정책과 진행 슬롯(번호·목록 요약·시작 선택) 규칙
#include "Game/Save/DigetherSaveRules.h"

#include "Game/Save/DigetherSaveTuning.h"

namespace
{
	bool IsRecoverableReadFailure(const EDigetherSaveResult Result)
	{
		return Result == EDigetherSaveResult::NotFound || Result == EDigetherSaveResult::CorruptData
			|| Result == EDigetherSaveResult::InvalidState || Result == EDigetherSaveResult::UnsafePhase;
	}
}

EDigetherSaveResult DigetherSaveRules::SelectReadableSlot(const EDigetherSaveResult PrimaryResult,
	const EDigetherSaveResult BackupResult, bool& bOutUseBackup)
{
	if (PrimaryResult == EDigetherSaveResult::UnsupportedVersion
		|| BackupResult == EDigetherSaveResult::UnsupportedVersion)
	{
		return EDigetherSaveResult::UnsupportedVersion;
	}
	if (PrimaryResult == EDigetherSaveResult::Success)
	{
		bOutUseBackup = false;
		return EDigetherSaveResult::Success;
	}
	if (!IsRecoverableReadFailure(PrimaryResult))
	{
		return PrimaryResult;
	}
	if (BackupResult == EDigetherSaveResult::Success)
	{
		bOutUseBackup = true;
		return EDigetherSaveResult::Success;
	}
	if (!IsRecoverableReadFailure(BackupResult) || PrimaryResult == EDigetherSaveResult::NotFound)
	{
		return BackupResult;
	}
	return PrimaryResult;
}

EDigetherSaveResult DigetherSaveRules::CanWriteSlots(const EDigetherSaveResult PrimaryResult,
	const EDigetherSaveResult BackupResult, const bool bExplicitNewGame)
{
	if (PrimaryResult == EDigetherSaveResult::UnsupportedVersion
		|| BackupResult == EDigetherSaveResult::UnsupportedVersion)
	{
		return EDigetherSaveResult::UnsupportedVersion;
	}
	if (bExplicitNewGame)
	{
		return EDigetherSaveResult::Success;
	}
	// 읽을 수 있는 primary가 있어도 검사하지 못한 backup을 일반 저장으로 덮지 않음
	if (PrimaryResult != EDigetherSaveResult::Success && !IsRecoverableReadFailure(PrimaryResult))
	{
		return PrimaryResult;
	}
	if (BackupResult != EDigetherSaveResult::Success && !IsRecoverableReadFailure(BackupResult))
	{
		return BackupResult;
	}
	bool bUseBackup = false;
	const EDigetherSaveResult ReadResult = SelectReadableSlot(PrimaryResult, BackupResult, bUseBackup);
	return PrimaryResult == EDigetherSaveResult::NotFound && BackupResult == EDigetherSaveResult::NotFound
		? EDigetherSaveResult::Success : ReadResult;
}

EDigetherSaveWriteStep DigetherSaveRules::AdvanceWriteStep(const EDigetherSaveWriteStep Current,
	const bool bSucceeded)
{
	if (Current == EDigetherSaveWriteStep::Complete || Current == EDigetherSaveWriteStep::Failed)
	{
		return Current;
	}
	if (!bSucceeded)
	{
		return EDigetherSaveWriteStep::Failed;
	}
	switch (Current)
	{
	case EDigetherSaveWriteStep::WriteBackup:
		return EDigetherSaveWriteStep::VerifyBackup;
	case EDigetherSaveWriteStep::VerifyBackup:
		return EDigetherSaveWriteStep::WritePrimary;
	case EDigetherSaveWriteStep::WritePrimary:
		return EDigetherSaveWriteStep::VerifyPrimary;
	case EDigetherSaveWriteStep::VerifyPrimary:
		return EDigetherSaveWriteStep::Complete;
	default:
		return EDigetherSaveWriteStep::Failed;
	}
}

bool DigetherSaveRules::ShouldWritePrimaryFirst(const EDigetherSaveResult PrimaryResult,
	const EDigetherSaveResult BackupResult)
{
	return PrimaryResult != EDigetherSaveResult::Success && BackupResult == EDigetherSaveResult::Success;
}

bool DigetherSaveRules::IsValidSlotIndex(const int32 SlotIndex)
{
	return SlotIndex >= 0 && SlotIndex < DigetherSaveTuning::MaxProgressSlots;
}

FDigetherSaveSlotSummary DigetherSaveRules::MakeSlotSummary(const int32 SlotIndex, const EDigetherSaveResult ReadResult,
	const FDigetherSaveSlotData& Slot)
{
	FDigetherSaveSlotSummary Summary;
	Summary.SlotIndex = SlotIndex;
	switch (ReadResult)
	{
	case EDigetherSaveResult::Success:
		Summary.Status = EDigetherSaveSlotStatus::Ready;
		break;
	case EDigetherSaveResult::NotFound:
		Summary.Status = EDigetherSaveSlotStatus::Empty;
		return Summary;
	case EDigetherSaveResult::UnsupportedVersion:
		Summary.Status = EDigetherSaveSlotStatus::FutureVersion;
		return Summary;
	case EDigetherSaveResult::HostIdentityMissing:
	case EDigetherSaveResult::UnsupportedIdentity:
		Summary.Status = EDigetherSaveSlotStatus::IdentityUnavailable;
		return Summary;
	default:
		Summary.Status = EDigetherSaveSlotStatus::Unreadable;
		return Summary;
	}
	// 정확한 공동 인정액·개인 잔고·인증 ID 제외, 게임 안에서 모두에게 보이는 값만
	const FDigetherSaveSnapshot& Progress = Slot.Progress;
	Summary.TotalDay = Progress.Quota.Quota.TotalDay;
	Summary.PeriodDay = Progress.Quota.Quota.Day;
	Summary.TargetCredits = Progress.Quota.Quota.TargetCredits;
	Summary.CompletedPeriods = Progress.Quota.Quota.PeriodGeneration;
	Summary.bGameOver = Progress.RunPublicState.bGameOver;
	Summary.PlayerRecordCount = Progress.Players.Num();
	Summary.bHasSavedAt = Slot.SavedAtUtcTicks > 0 && Slot.SavedAtUtcTicks <= FDateTime::MaxValue().GetTicks();
	Summary.SavedAtUtc = Summary.bHasSavedAt ? FDateTime(Slot.SavedAtUtcTicks) : FDateTime();
	return Summary;
}

bool DigetherSaveRules::ShouldStartNewGame(const FDigetherSaveSlotSelection& Selection, const bool bNewGameUrlOption)
{
	return bNewGameUrlOption || Selection.StartMode == EDigetherSaveStartMode::NewGame;
}

bool DigetherSaveRules::TryParseSlotOption(const FString& Option, FDigetherSaveSlotSelection& OutSelection)
{
	const FString Trimmed = Option.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		OutSelection = FDigetherSaveSlotSelection();
		return true;
	}
	struct FPrefix
	{
		const TCHAR* Text;
		EDigetherSaveStartMode Mode;
	};
	const FPrefix Prefixes[] = { { TEXT("new"), EDigetherSaveStartMode::NewGame },
		{ TEXT("load"), EDigetherSaveStartMode::Continue } };
	for (const FPrefix& Prefix : Prefixes)
	{
		if (!Trimmed.StartsWith(Prefix.Text, ESearchCase::IgnoreCase))
		{
			continue;
		}
		const FString Digits = Trimmed.RightChop(FCString::Strlen(Prefix.Text));
		int32 SlotIndex = 0;
		if (!Digits.IsEmpty())
		{
			// 숫자만 허용, "+1"·"1a" 같은 입력 거절
			for (const TCHAR Character : Digits)
			{
				if (!FChar::IsDigit(Character))
				{
					return false;
				}
			}
			if (Digits.Len() > 3)
			{
				return false;
			}
			SlotIndex = FCString::Atoi(*Digits);
		}
		if (!IsValidSlotIndex(SlotIndex))
		{
			return false;
		}
		FDigetherSaveSlotSelection Selection;
		Selection.SlotIndex = SlotIndex;
		Selection.StartMode = Prefix.Mode;
		OutSelection = Selection;
		return true;
	}
	return false;
}
