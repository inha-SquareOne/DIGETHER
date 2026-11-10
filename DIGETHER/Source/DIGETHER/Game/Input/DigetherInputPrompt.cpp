#include "Game/Input/DigetherInputPrompt.h"

#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"

#define LOCTEXT_NAMESPACE "DigetherInputPrompt"

FKey DigetherInputPrompt::ChooseDisplayKey(const TConstArrayView<FKey> Keys)
{
	for (const FKey& Key : Keys)
	{
		if (Key.IsValid() && !Key.IsGamepadKey() && !Key.IsTouch())
		{
			return Key;
		}
	}
	return EKeys::Invalid;
}

FText DigetherInputPrompt::MakeKeyText(const FKey& Key)
{
	return Key.IsValid() ? Key.GetDisplayName(false) : LOCTEXT("KeyUnassigned", "키 미지정");
}

FText DigetherInputPrompt::GetActionKeyText(const APlayerController* Controller, const UInputAction* Action)
{
	const ULocalPlayer* LocalPlayer = IsValid(Controller) ? Controller->GetLocalPlayer() : nullptr;
	const UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer
		? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Input || !Action)
	{
		return MakeKeyText(EKeys::Invalid);
	}
	return MakeKeyText(ChooseDisplayKey(Input->QueryKeysMappedToAction(Action)));
}

#undef LOCTEXT_NAMESPACE
