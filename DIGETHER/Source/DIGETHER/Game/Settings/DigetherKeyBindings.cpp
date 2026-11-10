#include "Game/Settings/DigetherKeyBindings.h"

#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "Game/Settings/DigetherOptionsSettings.h"
#include "GameplayTagContainer.h"
#include "InputMappingContext.h"
#include "UserSettings/EnhancedInputUserSettings.h"

namespace
{
	const FPlayerKeyMapping* FindPrimaryMapping(const FKeyMappingRow& Row)
	{
		for (const FPlayerKeyMapping& Mapping : Row.Mappings)
		{
			if (Mapping.GetSlot() == EPlayerMappableKeySlot::First && DigetherKeyBindings::IsBindableKey(Mapping.GetDefaultKey()))
			{
				return &Mapping;
			}
		}
		return nullptr;
	}
}

void DigetherKeyBindings::AssignWithSwap(TMap<FName, FKey>& Keys, const FName MappingName, const FKey& NewKey)
{
	FKey* Target = Keys.Find(MappingName);
	if (Target == nullptr || *Target == NewKey)
	{
		return;
	}
	const FKey OldKey = *Target;
	for (TPair<FName, FKey>& Pair : Keys)
	{
		if (Pair.Key != MappingName && Pair.Value == NewKey)
		{
			Pair.Value = OldKey;
		}
	}
	*Target = NewKey;
}

bool DigetherKeyBindings::IsBindableKey(const FKey& Key)
{
	return Key.IsValid() && Key != EKeys::AnyKey && Key != EKeys::Escape && !Key.IsGamepadKey() && !Key.IsTouch()
		&& !Key.IsAxis1D() && !Key.IsAxis2D() && !Key.IsAxis3D();
}

UEnhancedInputUserSettings* UDigetherKeyBindingLibrary::GetUserSettings(const ULocalPlayer* LocalPlayer)
{
	const UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer != nullptr
		? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer) : nullptr;
	// DefaultInput.ini bEnableUserSettings 꺼져 있으면 null
	return Input != nullptr ? Input->GetUserSettings() : nullptr;
}

void UDigetherKeyBindingLibrary::RegisterRebindableContexts(const ULocalPlayer* LocalPlayer)
{
	UEnhancedInputUserSettings* Settings = GetUserSettings(LocalPlayer);
	if (Settings == nullptr)
	{
		return;
	}
	for (const TSoftObjectPtr<UInputMappingContext>& ContextRef : UDigetherOptionsSettings::Get()->RebindableMappingContexts)
	{
		const UInputMappingContext* Context = ContextRef.LoadSynchronous();
		if (Context != nullptr && !Settings->IsMappingContextRegistered(Context))
		{
			Settings->RegisterInputMappingContext(Context);
		}
	}
}

TArray<FDigetherKeyBindingRow> UDigetherKeyBindingLibrary::GetKeyBindingRows(const ULocalPlayer* LocalPlayer)
{
	TArray<FDigetherKeyBindingRow> Rows;
	RegisterRebindableContexts(LocalPlayer);
	const UEnhancedInputUserSettings* Settings = GetUserSettings(LocalPlayer);
	const UEnhancedPlayerMappableKeyProfile* Profile = Settings != nullptr ? Settings->GetActiveKeyProfile() : nullptr;
	if (Profile == nullptr)
	{
		return Rows;
	}
	for (const TPair<FName, FKeyMappingRow>& Pair : Profile->GetPlayerMappingRows())
	{
		if (const FPlayerKeyMapping* Mapping = FindPrimaryMapping(Pair.Value))
		{
			FDigetherKeyBindingRow& Row = Rows.AddDefaulted_GetRef();
			Row.MappingName = Mapping->GetMappingName();
			Row.DisplayName = Mapping->GetDisplayName().IsEmpty() ? FText::FromName(Row.MappingName) : Mapping->GetDisplayName();
			Row.Category = Mapping->GetDisplayCategory();
			Row.CurrentKey = Mapping->GetCurrentKey();
			Row.DefaultKey = Mapping->GetDefaultKey();
		}
	}
	Rows.Sort([](const FDigetherKeyBindingRow& A, const FDigetherKeyBindingRow& B)
	{
		const int32 Category = A.Category.CompareTo(B.Category);
		return Category != 0 ? Category < 0 : A.DisplayName.CompareTo(B.DisplayName) < 0;
	});
	return Rows;
}

bool UDigetherKeyBindingLibrary::ApplyKeyBindings(const ULocalPlayer* LocalPlayer, const TMap<FName, FKey>& Keys)
{
	UEnhancedInputUserSettings* Settings = GetUserSettings(LocalPlayer);
	const UEnhancedPlayerMappableKeyProfile* Profile = Settings != nullptr ? Settings->GetActiveKeyProfile() : nullptr;
	if (Profile == nullptr)
	{
		return Keys.IsEmpty();
	}
	bool bAllApplied = true;
	bool bChanged = false;
	for (const TPair<FName, FKey>& Pair : Keys)
	{
		const FKeyMappingRow* Row = Profile->FindKeyMappingRow(Pair.Key);
		const FPlayerKeyMapping* Mapping = Row != nullptr ? FindPrimaryMapping(*Row) : nullptr;
		if (Mapping == nullptr || !DigetherKeyBindings::IsBindableKey(Pair.Value))
		{
			bAllApplied = false;
			continue;
		}
		if (Mapping->GetCurrentKey() == Pair.Value)
		{
			continue;
		}
		FMapPlayerKeyArgs Args;
		Args.MappingName = Pair.Key;
		Args.Slot = EPlayerMappableKeySlot::First;
		Args.NewKey = Pair.Value;
		Args.HardwareDeviceId = Mapping->GetHardwareDeviceId().HardwareDeviceIdentifier;
		FGameplayTagContainer FailureReason;
		Settings->MapPlayerKey(Args, FailureReason);
		bAllApplied &= FailureReason.IsEmpty();
		bChanged = true;
	}
	if (bChanged)
	{
		Settings->ApplySettings();
		Settings->AsyncSaveSettings();
	}
	return bAllApplied;
}
