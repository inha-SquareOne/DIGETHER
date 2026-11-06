#include "DigetherDainHud.h"
#include "Engine/Canvas.h"
void ADigetherDainHud::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) { return; }
	const float X = Canvas->SizeX * .5f, Y = Canvas->SizeY * .5f;
	DrawLine(X - 6, Y, X + 6, Y, FLinearColor::White);
	DrawLine(X, Y - 6, X, Y + 6, FLinearColor::White);
	DrawText(TEXT("EXCAVATION LAB | WASD: move | Hold LMB: dig brown soil | Grey rock: protected"), FLinearColor::White, 20, 20);
	DrawText(TEXT("Dig repeatedly and aim around the rim (including feet) to widen the opening. Reach EXIT."), FLinearColor::Yellow, 20, 45);
}
