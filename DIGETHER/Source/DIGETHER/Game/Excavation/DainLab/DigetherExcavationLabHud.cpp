#include "DigetherExcavationLabHud.h"
#include "Engine/Canvas.h"
#include "DigetherExcavationLabCharacter.h"
#include "GameFramework/PlayerController.h"
// Canvas가 준비된 프레임에만 개인 시험 안내 출력
void ADigetherExcavationLabHud::DrawHUD()
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
					   "EXCAVATION LAB | WASD: move | Hold LMB: dig brown soil | Bedrock: protected | Pickaxe: break rubble")
				 .ToString(),
			 FLinearColor::White, 20, 20);
	DrawText(NSLOCTEXT("DigetherExcavationLab", "Instructions",
					   "Dig repeatedly and aim around the rim (including feet) to widen the opening. Reach EXIT.")
				 .ToString(),
			 FLinearColor::Yellow, 20, 45);
	const ADigetherExcavationLabCharacter* Character = PlayerOwner ? Cast<ADigetherExcavationLabCharacter>(PlayerOwner->GetPawn()) : nullptr;
	if (IsValid(Character))
	{
		const FText ToolName = StaticEnum<EDigetherLabTool>()->GetDisplayNameTextByValue(static_cast<int64>(Character->GetSelectedTool()));
		DrawText(FText::Format(NSLOCTEXT("DigetherExcavationLab", "ToolSettings",
			"[1] Hands  [2] Shovel  [3] Pickaxe  [4] Steel  [5] Upper  [6] Drill  |  {0}: {1} cm / {2} s (test)"),
			ToolName, FText::AsNumber(Character->GetSelectedRadiusCm()),
			FText::AsNumber(Character->GetSelectedIntervalSeconds())).ToString(), FLinearColor(0.f, 1.f, 1.f), 20, 70);
	}
}
