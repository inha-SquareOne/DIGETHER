#include "DigetherDainHud.h"
#include "Engine/Canvas.h"
#include "DigetherDainCharacter.h"
#include "GameFramework/PlayerController.h"
// Canvas가 준비된 프레임에만 개인 시험 안내 출력
void ADigetherDainHud::DrawHUD()
{
	Super::DrawHUD();
	if (!IsValid(Canvas))
	{
		return;
	}
	const float X = Canvas->SizeX * .5f, Y = Canvas->SizeY * .5f;
	DrawLine(X - 6, Y, X + 6, Y, FLinearColor::White);
	DrawLine(X, Y - 6, X, Y + 6, FLinearColor::White);
	DrawText(NSLOCTEXT("DigetherExcavationLab", "Controls",
					   "EXCAVATION LAB | WASD: move | Hold LMB: dig brown soil | Grey rock: protected")
				 .ToString(),
			 FLinearColor::White, 20, 20);
	DrawText(NSLOCTEXT("DigetherExcavationLab", "Instructions",
					   "Dig repeatedly and aim around the rim (including feet) to widen the opening. Reach EXIT.")
				 .ToString(),
			 FLinearColor::Yellow, 20, 45);
	const ADigetherDainCharacter* Character = PlayerOwner ? Cast<ADigetherDainCharacter>(PlayerOwner->GetPawn()) : nullptr;
	if (IsValid(Character))
	{
		const FText ToolName = Character->GetSelectedTool() == EDigetherLabTool::Pickaxe
			? NSLOCTEXT("DigetherExcavationLab", "Pickaxe", "Pickaxe")
			: NSLOCTEXT("DigetherExcavationLab", "Shovel", "Shovel");
		DrawText(FText::Format(NSLOCTEXT("DigetherExcavationLab", "ToolSettings",
			"[1] Pickaxe / [2] Shovel | Selected: {0} | Radius: {1} cm | Interval: {2} s | TEMP - not approved"),
			ToolName, FText::AsNumber(Character->GetSelectedRadiusCm()),
			FText::AsNumber(Character->GetSelectedIntervalSeconds())).ToString(), FLinearColor(0.f, 1.f, 1.f), 20, 70);
	}
}
