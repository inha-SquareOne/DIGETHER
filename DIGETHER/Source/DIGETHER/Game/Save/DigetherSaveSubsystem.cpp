#include "Game/Save/DigetherSaveSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/Save/DigetherSaveStorage.h"
#include "Game/Save/DigetherSaveRules.h"

void UDigetherSaveSubsystem::Deinitialize()
{
	HostAuthenticatedPlayerKey.Reset();
	LastWrittenSlot.Reset();
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
		LastWrittenSlot.Reset();
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
	return DigetherSaveStorage::Read(DigetherSaveStorage::MakeSlotNames(HostAuthenticatedPlayerKey),
		HostAuthenticatedPlayerKey, OutSlot, bRecovered);
}

EDigetherSaveResult UDigetherSaveSubsystem::Write(const FDigetherSaveSnapshot& Snapshot,
	const bool bExplicitNewGame, const bool bCheckpoint)
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
	LastWrittenSlot.Reset();
	FDigetherSaveSlotData Existing;
	const EDigetherSaveResult ExistingResult = ReadSlot(Existing);
	if (ExistingResult == EDigetherSaveResult::UnsupportedVersion
		|| (!bExplicitNewGame && ExistingResult != EDigetherSaveResult::Success
			&& ExistingResult != EDigetherSaveResult::NotFound))
	{
		return ExistingResult;
	}
	FDigetherSaveSlotData Candidate;
	const EDigetherSaveResult BuildResult = DigetherSaveRules::TryBuildSlot(Snapshot,
		ExistingResult == EDigetherSaveResult::Success ? &Existing : nullptr, bExplicitNewGame, bCheckpoint, Candidate);
	if (BuildResult != EDigetherSaveResult::Success)
	{
		return BuildResult;
	}
	const EDigetherSaveResult WriteResult = DigetherSaveStorage::Write(
		DigetherSaveStorage::MakeSlotNames(HostAuthenticatedPlayerKey), HostAuthenticatedPlayerKey,
		Candidate, bExplicitNewGame);
	if (WriteResult == EDigetherSaveResult::Success)
	{
		LastWrittenSlot = MoveTemp(Candidate);
	}
	return WriteResult;
}

EDigetherSaveResult UDigetherSaveSubsystem::Read(FDigetherSaveSnapshot& OutSnapshot, const bool bCheckpoint)
{
	if (!HasHostAuthority())
	{
		return EDigetherSaveResult::NotAuthority;
	}
	LastWrittenSlot.Reset();
	FDigetherSaveSlotData Candidate;
	const EDigetherSaveResult Result = ReadSlot(Candidate);
	if (Result != EDigetherSaveResult::Success)
	{
		return Result;
	}
	if (bCheckpoint && !Candidate.bHasDepartureCheckpoint)
	{
		return EDigetherSaveResult::CheckpointMissing;
	}
	OutSnapshot = bCheckpoint ? Candidate.DepartureCheckpoint : Candidate.Progress;
	LastWrittenSlot = MoveTemp(Candidate);
	return EDigetherSaveResult::Success;
}

bool UDigetherSaveSubsystem::IsCheckpointCurrent(const FDigetherSaveSnapshot& Current) const
{
	return HasHostAuthority() && LastWrittenSlot.IsSet()
		&& Current.HostAuthenticatedPlayerKey == HostAuthenticatedPlayerKey
		&& DigetherSaveRules::CanDepartWithCheckpoint(Current, LastWrittenSlot.GetValue()) == EDigetherSaveResult::Success;
}
