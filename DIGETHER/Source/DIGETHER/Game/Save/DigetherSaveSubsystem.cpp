#include "Game/Save/DigetherSaveSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Game/Save/DigetherSaveLog.h"
#include "Game/Save/DigetherSaveRules.h"
#include "Game/Save/DigetherSaveStorage.h"
#include "Game/Save/DigetherSaveTuning.h"
#include "GameFramework/OnlineReplStructs.h"

void UDigetherSaveSubsystem::Deinitialize()
{
	HostAuthenticatedPlayerKey.Reset();
	SlotSelection = FDigetherSaveSlotSelection();
	Super::Deinitialize();
}

bool UDigetherSaveSubsystem::HasHostAuthority() const
{
	const UGameInstance* Instance = GetGameInstance();
	const UWorld* World = IsValid(Instance) ? Instance->GetWorld() : nullptr;
	return IsInGameThread() && IsValid(World)
		&& (World->GetNetMode() == NM_Standalone || World->GetNetMode() == NM_ListenServer);
}

EDigetherSaveResult UDigetherSaveSubsystem::BindHostIdentity(const FUniqueNetIdRepl& HostUniqueId)
{
	if (!HasHostAuthority())
	{
		return EDigetherSaveResult::NotAuthority;
	}
	FString Candidate;
	const EDigetherSaveResult Result = DigetherSaveRules::MakeAuthenticatedPlayerKey(HostUniqueId, Candidate);
	if (Result == EDigetherSaveResult::Success && Candidate != HostAuthenticatedPlayerKey)
	{
		HostAuthenticatedPlayerKey = MoveTemp(Candidate);
	}
	return Result;
}

EDigetherSaveResult UDigetherSaveSubsystem::ReadSlot(FDigetherSaveSlotData& OutSlot) const
{
	if (HostAuthenticatedPlayerKey.IsEmpty())
	{
		return EDigetherSaveResult::HostIdentityMissing;
	}
	bool bRecovered = false;
	return DigetherSaveStorage::Read(DigetherSaveStorage::MakeSlotNames(HostAuthenticatedPlayerKey,
		SlotSelection.SlotIndex), HostAuthenticatedPlayerKey, OutSlot, bRecovered);
}

EDigetherSaveResult UDigetherSaveSubsystem::Write(const FDigetherSaveSnapshot& Snapshot, const bool bExplicitNewGame)
{
	if (!HasHostAuthority())
	{
		return EDigetherSaveResult::NotAuthority;
	}
	if (HostAuthenticatedPlayerKey.IsEmpty())
	{
		return EDigetherSaveResult::HostIdentityMissing;
	}
	if (Snapshot.HostAuthenticatedPlayerKey != HostAuthenticatedPlayerKey)
	{
		return EDigetherSaveResult::HostIdentityMismatch;
	}
	// 현재 선택 슬롯 하나만 읽고 쓰기, 다른 진행 슬롯은 새 게임·게임 오버 초기화에서도 건드리지 않음
	const FDigetherSaveSlotNames Names = DigetherSaveStorage::MakeSlotNames(HostAuthenticatedPlayerKey,
		SlotSelection.SlotIndex);
	// 두 슬롯은 한 번만 읽음, 같은 게임 스레드 호출 안에서 기존 슬롯 선택과 쓰기 허용·순서 판정에 공용
	const FDigetherSaveSlotReads Reads = DigetherSaveStorage::ReadSlots(Names, HostAuthenticatedPlayerKey);
	FDigetherSaveSlotData Existing;
	bool bRecovered = false;
	const EDigetherSaveResult ExistingResult = DigetherSaveStorage::Read(Reads, Existing, bRecovered);
	if (ExistingResult == EDigetherSaveResult::UnsupportedVersion
		|| (!bExplicitNewGame && ExistingResult != EDigetherSaveResult::Success
			&& ExistingResult != EDigetherSaveResult::NotFound))
	{
		return ExistingResult;
	}
	FDigetherSaveSlotData Candidate;
	const EDigetherSaveResult BuildResult = DigetherSaveRules::TryBuildSlot(Snapshot,
		ExistingResult == EDigetherSaveResult::Success ? &Existing : nullptr, bExplicitNewGame, Candidate);
	if (BuildResult != EDigetherSaveResult::Success)
	{
		return BuildResult;
	}
	// 저장 규칙은 전역 시각 비의존, 목록 표시용 저장 시각은 디스크 쓰기 직전 이 계층에서 채움
	Candidate.SavedAtUtcTicks = FDateTime::UtcNow().GetTicks();
	return DigetherSaveStorage::Write(Names, HostAuthenticatedPlayerKey, Candidate, bExplicitNewGame, Reads);
}

EDigetherSaveResult UDigetherSaveSubsystem::Read(FDigetherSaveSnapshot& OutSnapshot)
{
	if (!HasHostAuthority())
	{
		return EDigetherSaveResult::NotAuthority;
	}
	FDigetherSaveSlotData Candidate;
	const EDigetherSaveResult Result = ReadSlot(Candidate);
	if (Result != EDigetherSaveResult::Success)
	{
		return Result;
	}
	// 이전 파일의 출발 기준점은 읽기만 함, 진행은 마지막 정상 저장(Progress)에서 시작
	OutSnapshot = MoveTemp(Candidate.Progress);
	return EDigetherSaveResult::Success;
}

bool UDigetherSaveSubsystem::SetSlotSelection(const FDigetherSaveSlotSelection& Selection)
{
	const UGameInstance* Instance = GetGameInstance();
	const UWorld* World = IsValid(Instance) ? Instance->GetWorld() : nullptr;
	if (!IsInGameThread() || !IsValid(World) || World->GetNetMode() != NM_Standalone)
	{
		UE_LOG(LogDigetherSave, Warning, TEXT("진행 슬롯 선택 거절: 방 밖(독립 실행)에서만 바꿀 수 있습니다."));
		return false;
	}
	if (!DigetherSaveRules::IsValidSlotIndex(Selection.SlotIndex))
	{
		UE_LOG(LogDigetherSave, Warning, TEXT("진행 슬롯 선택 거절: 슬롯 번호 %d는 0..%d 밖입니다."),
			Selection.SlotIndex, DigetherSaveTuning::MaxProgressSlots - 1);
		return false;
	}
	SlotSelection = Selection;
	UE_LOG(LogDigetherSave, Log, TEXT("진행 슬롯 선택: 슬롯=%d 새 게임=%d"), SlotSelection.SlotIndex,
		SlotSelection.StartMode == EDigetherSaveStartMode::NewGame);
	return true;
}

int32 UDigetherSaveSubsystem::GetMaxSlotCount()
{
	return DigetherSaveTuning::MaxProgressSlots;
}

FString UDigetherSaveSubsystem::ResolveListingHostKey() const
{
	// 게임 맵에서 서버 연결층이 바인딩한 호스트 키 우선(쓰기와 같은 키)
	if (!HostAuthenticatedPlayerKey.IsEmpty())
	{
		return HostAuthenticatedPlayerKey;
	}
	// 실행 직후 메뉴에는 바인딩이 없음, 같은 계정이면 게임 맵 PlayerState ID와 같은 키
	const UGameInstance* Instance = GetGameInstance();
	const ULocalPlayer* LocalPlayer = IsValid(Instance) ? Instance->GetFirstGamePlayer() : nullptr;
	FString LocalKey;
	if (IsValid(LocalPlayer)
		&& DigetherSaveRules::MakeAuthenticatedPlayerKey(LocalPlayer->GetPreferredUniqueNetId(), LocalKey)
			== EDigetherSaveResult::Success)
	{
		return LocalKey;
	}
	return FString();
}

TArray<FDigetherSaveSlotSummary> UDigetherSaveSubsystem::ListSlots() const
{
	TArray<FDigetherSaveSlotSummary> Summaries;
	if (!HasHostAuthority())
	{
		return Summaries;
	}
	const FString HostKey = ResolveListingHostKey();
	Summaries.Reserve(DigetherSaveTuning::MaxProgressSlots);
	for (int32 SlotIndex = 0; SlotIndex < DigetherSaveTuning::MaxProgressSlots; ++SlotIndex)
	{
		FDigetherSaveSlotData Slot;
		EDigetherSaveResult Result = EDigetherSaveResult::HostIdentityMissing;
		if (!HostKey.IsEmpty())
		{
			bool bRecovered = false;
			Result = DigetherSaveStorage::Read(DigetherSaveStorage::MakeSlotNames(HostKey, SlotIndex), HostKey,
				Slot, bRecovered);
		}
		Summaries.Add(DigetherSaveRules::MakeSlotSummary(SlotIndex, Result, Slot));
	}
	return Summaries;
}

void UDigetherSaveSubsystem::CompleteNewGameRequest()
{
	SlotSelection.StartMode = EDigetherSaveStartMode::Continue;
}
