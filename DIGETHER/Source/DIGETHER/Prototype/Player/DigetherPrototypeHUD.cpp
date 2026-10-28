#include "DigetherPrototypeHUD.h"

#include "DigetherPrototypeCharacter.h"
#include "DigetherPrototypePlayerController.h"
#include "Prototype/DigetherPrototypeGameState.h"
#include "Prototype/DigetherPrototypePlayerState.h"
#include "Prototype/Online/DigetherPrototypeGameInstance.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"

namespace
{
	struct FHudLine
	{
		FString Text;
		FColor Color = FColor::White;
	};

	// 이진 탐색으로 폭에 맞는 글자 수를 찾아 줄바꿈한다. 가능하면 공백에서 끊는다.
	void WrapText(UCanvas* Canvas, const UFont* Font, const FString& Text, float MaxWidth, float Scale, TArray<FString>& OutLines)
	{
		const int32 Len = Text.Len();
		int32 Start = 0;
		while (Start < Len)
		{
			int32 Lo = 1;
			int32 Hi = Len - Start;
			int32 Fit = 1;
			while (Lo <= Hi)
			{
				const int32 Mid = (Lo + Hi) / 2;
				float W = 0.f;
				float H = 0.f;
				Canvas->TextSize(Font, Text.Mid(Start, Mid), W, H, Scale, Scale);
				if (W <= MaxWidth)
				{
					Fit = Mid;
					Lo = Mid + 1;
				}
				else
				{
					Hi = Mid - 1;
				}
			}
			int32 Cut = Start + Fit;
			if (Cut < Len)
			{
				for (int32 i = Cut; i > Start; --i)
				{
					if (Text[i] == TEXT(' '))
					{
						Cut = i;
						break;
					}
				}
			}
			OutLines.Add(Text.Mid(Start, Cut - Start).TrimEnd());
			Start = Cut;
			while (Start < Len && Text[Start] == TEXT(' '))
			{
				++Start;
			}
		}
	}

	// 반투명 패널에 줄바꿈된 텍스트를 그리고 하단 Y를 돌려준다. MaxBottom을 넘는 줄은 말줄임 처리한다.
	// bAnchorBottom이면 MaxBottom에 하단을 맞추고 실제 상단을 OutTop에 쓴다.
	float DrawPanel(UCanvas* Canvas, const UFont* Font, float Scale, float X, float Y, float W, float MaxBottom,
		bool bAnchorBottom, const TArray<FHudLine>& Lines, float* OutTop = nullptr)
	{
		const float Pad = 6.f * Scale;
		float Unused = 0.f;
		float LineH = 0.f;
		Canvas->TextSize(Font, FString(TEXT("가Ag")), Unused, LineH, Scale, Scale);
		LineH += 2.f * Scale;

		TArray<FHudLine> Wrapped;
		for (const FHudLine& Line : Lines)
		{
			TArray<FString> Parts;
			WrapText(Canvas, Font, Line.Text, W - 2.f * Pad, Scale, Parts);
			for (const FString& Part : Parts)
			{
				Wrapped.Add({Part, Line.Color});
			}
		}
		if (Wrapped.Num() == 0)
		{
			if (OutTop) { *OutTop = bAnchorBottom ? MaxBottom : Y; }
			return bAnchorBottom ? MaxBottom : Y;
		}

		if (bAnchorBottom)
		{
			Y = MaxBottom - (2.f * Pad + LineH * Wrapped.Num());
		}
		if (OutTop) { *OutTop = Y; }

		const int32 MaxLines = FMath::FloorToInt((MaxBottom - Y - 2.f * Pad) / LineH);
		if (MaxLines <= 0)
		{
			return Y;
		}
		if (Wrapped.Num() > MaxLines)
		{
			Wrapped.SetNum(MaxLines);
			Wrapped.Last().Text += TEXT(" …");
		}

		const float H = 2.f * Pad + LineH * Wrapped.Num();
		FCanvasTileItem Back(FVector2D(X, Y), FVector2D(W, H), FLinearColor(0.f, 0.f, 0.f, 0.58f));
		Back.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Back);

		FFontRenderInfo RenderInfo;
		RenderInfo.bEnableShadow = true;
		float TextY = Y + Pad;
		for (const FHudLine& Line : Wrapped)
		{
			Canvas->SetDrawColor(Line.Color);
			Canvas->DrawText(Font, Line.Text, X + Pad, TextY, Scale, Scale, RenderInfo);
			TextY += LineH;
		}
		Canvas->SetDrawColor(FColor::White);
		return Y + H;
	}

	const TCHAR* ElevatorStateText(EPrototypeElevatorState State)
	{
		switch (State)
		{
		case EPrototypeElevatorState::Ground: return TEXT("지상 대기");
		case EPrototypeElevatorState::Ascending: return TEXT("상승 중");
		case EPrototypeElevatorState::Top: return TEXT("상단 도착");
		case EPrototypeElevatorState::Descending: return TEXT("하강 중");
		case EPrototypeElevatorState::FinalAscending: return TEXT("최종 상승 중");
		default: return TEXT("알 수 없음");
		}
	}
}

void ADigetherPrototypeHUD::RefreshDisplayText()
{
	NoticeLine = TEXT("시험 빌드 · 수치는 시험값(정식 확정 아님)");
	ControlsLine = TEXT("조작 · 이동 WASD · 시야 마우스 · 점프 Space · 달리기 Shift · 웅크리기 Ctrl · 랜턴 L");
	ElevatorLine.Empty();
	PublicMessageLine.Empty();
	HoldFraction = 0.f;
	ChecklistLines.Reset();
	ObjectiveLine.Empty();
	bHasHealth = false;
	bHasStamina = false;
	bHasSlotData = false;
	bHiddenSlotFilled = false;
	SlotTexts.Init(FString(), 4);

	ADigetherPrototypePlayerController* PC = Cast<ADigetherPrototypePlayerController>(GetOwningPlayerController());
	const ADigetherPrototypeGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ADigetherPrototypeGameState>() : nullptr;

	EPrototypePhase Phase = EPrototypePhase::Base;
	bool bHasPhase = false;
	if (GameState)
	{
		const FPrototypePublicState& Pub = GameState->PublicState;
		Phase = Pub.Phase;
		bHasPhase = true;
		const TCHAR* PhaseText = Pub.Phase == EPrototypePhase::Base ? TEXT("거점")
			: Pub.Phase == EPrototypePhase::Expedition ? TEXT("탐사")
			: Pub.Phase == EPrototypePhase::Returning ? TEXT("거점 귀환 대기")
			: TEXT("게임 오버");
		StatusLine = FString::Printf(TEXT("구역: %s · %d일차 · 목표 %d/%d · 상승 %d/%d회"),
			PhaseText, Pub.Day, Pub.QuotaProgress, Pub.QuotaTarget, Pub.ElevatorTrips, Pub.MaxElevatorTrips);
		PublicMessageLine = Pub.Message;
		CompactStatusLine = FString::Printf(TEXT("%d일차 · 목표 %d/%d"), Pub.Day, Pub.QuotaProgress, Pub.QuotaTarget);

		if (Pub.Phase == EPrototypePhase::Expedition &&
			(Pub.ElevatorState != EPrototypeElevatorState::Ground || Pub.ElevatorProgress > 0.f))
		{
			ElevatorLine = FString::Printf(TEXT("승강기: %s · 진행 %d%%"),
				ElevatorStateText(Pub.ElevatorState), FMath::RoundToInt(FMath::Clamp(Pub.ElevatorProgress, 0.f, 1.f) * 100.f));
		}
	}
	else
	{
		StatusLine = TEXT("구역 정보를 불러오는 중입니다.");
		CompactStatusLine = TEXT("구역 정보 대기 중");
	}
	if (const UDigetherPrototypeGameInstance* Session = Cast<UDigetherPrototypeGameInstance>(GetGameInstance()))
	{
		CompactStatusLine += FString::Printf(TEXT(" · %s"), *Session->GetSteamStatus());
	}

	EPrototypeLife Life = EPrototypeLife::Active;
	const ADigetherPrototypePlayerState* PS = PC ? PC->GetPlayerState<ADigetherPrototypePlayerState>() : nullptr;
	if (PS)
	{
		const FPrototypePrivateState& Priv = PS->PrivateState;
		Life = Priv.Life;
		int32 NormalCount = 0;
		bool bHasHidden = false;
		for (const FPrototypeItem& Item : Priv.Items)
		{
			if (Item.bHidden) bHasHidden = true;
			else ++NormalCount;
		}
		if (PC->IsLocalController())
		{
			// 슬롯 내용은 로컬 소유자 화면에만 표시한다. 히든은 보유 여부만 표시한다.
			bHasSlotData = true;
			bHiddenSlotFilled = bHasHidden;
			bHasHealth = true;
			HealthValue = Priv.Health;
			HealthFraction = FMath::Clamp(Priv.Health / FMath::Max(1.f, DisplayMaxHealth), 0.f, 1.f);
			int32 NormalIndex = 0;
			for (const FPrototypeItem& Item : Priv.Items)
			{
				if (Item.bHidden || NormalIndex >= SlotTexts.Num()) continue;
				SlotTexts[NormalIndex++] = Item.bEmergencyDevice ? FString(TEXT("긴급"))
					: Item.bTwoHanded ? FString(TEXT("양손"))
					: Item.bEquipment ? FString(TEXT("장비"))
					: FString::FromInt(Item.Value);
			}
		}
		const TCHAR* LifeText = Priv.Life == EPrototypeLife::Active ? TEXT("활동 중")
			: Priv.Life == EPrototypeLife::Buried ? TEXT("매몰 — 구조 대기")
			: Priv.Life == EPrototypeLife::Dead ? TEXT("사망 — 관전")
			: Priv.Life == EPrototypeLife::InTransit ? TEXT("승강 이동 중 — 이동 잠김")
			: TEXT("탈출 완료 — 관전");
		PlayerLine = FString::Printf(TEXT("내 상태 · 잔고 %d원 · HP %.0f · 목재 %d · 슬롯 %d/4 · 히든 %s · %s"),
			Priv.Money, Priv.Health, Priv.WoodCount, NormalCount, bHasHidden ? TEXT("보유") : TEXT("없음"), LifeText);
		if (Priv.LastPayout > 0)
		{
			PlayerLine += FString::Printf(TEXT(" · 최근 정산 +%d원"), Priv.LastPayout);
		}
		if (PC->IsLocalController())
		{
			HoldFraction = FMath::Clamp(Priv.HoldProgress, 0.f, 1.f);
		}
	}
	else
	{
		// 늦은 PlayerState 생성(접속 초기) 대비.
		PlayerLine = TEXT("플레이어 정보를 불러오는 중입니다.");
	}

	const ADigetherPrototypeCharacter* Character = PC ? Cast<ADigetherPrototypeCharacter>(PC->GetPawn()) : nullptr;
	if (Character)
	{
		bHasStamina = true;
		StaminaFraction = Character->GetStaminaFraction();
		SelectedSlotIndex = FMath::Clamp(Character->GetSelectedSlot(), 1, 5);
		PlayerLine +=FString::Printf(TEXT(" · 선택 슬롯 %d · 스태미나 %.0f%% · 랜턴 %s"),
			Character->GetSelectedSlot(), Character->GetStaminaFraction() * 100.f,
			Character->IsHeadlampOn() ? TEXT("켜짐") : TEXT("꺼짐"));
	}

	if (PC)
	{
		const AActor* AimTarget = PC->GetAimTarget();
		TargetLine = AimTarget ? FString::Printf(TEXT("조준: %s"), *PC->GetAimHint()) : FString();
		MessageLine = PC->GetLastServerMessage();
		if (MessageLine.IsEmpty() && PS) { MessageLine = PS->PrivateState.Message; }
		if (PC->IsDepositInputOpen()) { TargetLine.Empty(); MessageLine.Empty(); }
	}
	else
	{
		TargetLine.Empty();
		MessageLine.Empty();
	}

	// 단계별 시험 목표 체크리스트. 키 목록은 하단 조작 안내에 따로 두고 여기서는 목표 중심으로 쓴다.
	if (!bHasPhase)
	{
		ChecklistTitle = TEXT("시험 목표");
		ChecklistLines.Add(TEXT("구역 정보를 받으면 단계별 안내가 표시됩니다."));
	}
	else if (Phase == EPrototypePhase::Returning)
	{
		ChecklistTitle = TEXT("거점 귀환 대기 (게임 오버 아님)");
		ChecklistLines.Add(TEXT("- 호스트가 모두를 거점으로 되돌리는 중입니다. 잠시 기다리세요."));
		ChecklistLines.Add(TEXT("- 귀환이 끝나면 거점에서 정산 결과(잔고·알림)를 확인합니다."));
		ChecklistLines.Add(TEXT("- 사망·탈출한 동료도 귀환 후 다시 참여합니다."));
	}
	else if (Phase == EPrototypePhase::GameOver)
	{
		ChecklistTitle = TEXT("시험 목표 · 게임 오버");
		ChecklistLines.Add(TEXT("- 이번 시험이 종료되었습니다."));
		ChecklistLines.Add(TEXT("- 출발 장치를 조준하고 F를 누르면 재시작을 요청합니다(호스트 판정)."));
	}
	else if (Phase == EPrototypePhase::Base)
	{
		ChecklistTitle = TEXT("시험 목표 · 거점");
		ChecklistLines.Add(TEXT("- 출발: 출발 장치 앞에서 E를 유지하면 게이지가 찹니다. 놓거나 우클릭하면 취소됩니다."));
		ChecklistLines.Add(TEXT("- 정산·납부: 탐사에서 번 몫은 정산 뒤 잔고와 알림에서 확인하고, 납부대에서 E를 눌러 입력창을 열어 금액을 숫자로 입력합니다. Enter로 제출하고 ESC로 취소합니다."));
		ChecklistLines.Add(TEXT("- 목표 달성: 잔고 이내 금액을 입력하면 목표 잔여분까지만 납부되며, 목표를 채우면 새 목표와 1일차로 전환됩니다. 시험 상점은 E로 장비, F로 긴급 탈출 장치를 삽니다."));
	}
	else if (Life == EPrototypeLife::Dead || Life == EPrototypeLife::Escaped)
	{
		ChecklistTitle = Life == EPrototypeLife::Dead ? TEXT("사망 — 관전 중") : TEXT("탈출 완료 — 관전 중");
		ChecklistLines.Add(TEXT("- 아직 탐사 중인 생존자의 진행을 관전합니다."));
		ChecklistLines.Add(TEXT("- 모두 귀환하면 거점에서 정산·납부 후 다시 참여합니다."));
	}
	else if (Life == EPrototypeLife::Buried)
	{
		ChecklistTitle = TEXT("매몰됨");
		ChecklistLines.Add(TEXT("- 동료가 E를 유지해 구조해 주길 기다리세요."));
		ChecklistLines.Add(TEXT("- 스스로 B를 눌러 매몰 탈출을 시도할 수도 있습니다."));
	}
	else if (Life == EPrototypeLife::InTransit)
	{
		ChecklistTitle = TEXT("승강 이동 중");
		ChecklistLines.Add(TEXT("- 이동과 상호작용이 잠겨 있습니다. 도착할 때까지 기다리세요."));
		ChecklistLines.Add(TEXT("- 도착 결과는 알림에서 확인합니다."));
	}
	else
	{
		ChecklistTitle = TEXT("시험 목표 · 탐사");
		ChecklistLines.Add(TEXT("- 진행 흐름: 갱도 미로를 걸어 포털 발견 → E로 포털을 써 폐교 탐사 → 회수품 획득 → 포털로 갱도 복귀 → 승강기로 귀환 → 거점 납부대에서 금액 입력."));
		ChecklistLines.Add(TEXT("- 굴착·보강: 노란 난간 안 갈색 흙·막장 벽을 좌클릭 유지, F는 보강. 여진 신호가 오면 서둘러 보강하고, 매몰된 동료는 E 유지로 구조합니다."));
		ChecklistLines.Add(TEXT("- 구덩이 복귀: 입구 쪽 금속 계단 주변 흙을 걷어낸 뒤 계단을 걸어 올라오세요."));
		ChecklistLines.Add(TEXT("- 회수품: E는 일반 슬롯(1~4), F는 히든 슬롯(5). 양손 화물은 굴착·보강·히든에 쓸 수 없으니 Q로 내려놓으세요."));
		ChecklistLines.Add(TEXT("- 탈출: 엘리베이터 탑승 원 안에서 E 유지. 승강 중에는 이동이 잠기고, 긴급 탈출은 상점 F로 산 시험 장치를 들고, 회수품·시체 없이 장치 앞에서 E를 유지합니다."));
	}

	if (!bHasPhase)
	{
		ObjectiveLine = TEXT("구역 정보 대기 중");
	}
	else if (Phase == EPrototypePhase::Returning)
	{
		ObjectiveLine = TEXT("거점 귀환 대기");
	}
	else if (Phase == EPrototypePhase::GameOver)
	{
		ObjectiveLine = TEXT("시험 종료 · 출발 장치 F로 재시작");
	}
	else if (Phase == EPrototypePhase::Base)
	{
		ObjectiveLine = TEXT("목표: 출발 장치 E 유지로 탐사 출발 · 납부는 납부대 E로 금액 입력");
	}
	else if (Life == EPrototypeLife::Dead || Life == EPrototypeLife::Escaped)
	{
		ObjectiveLine = TEXT("관전 중 · 귀환 후 다시 참여");
	}
	else if (Life == EPrototypeLife::Buried)
	{
		ObjectiveLine = TEXT("매몰됨 · 구조 대기 또는 B로 탈출");
	}
	else if (Life == EPrototypeLife::InTransit)
	{
		ObjectiveLine = TEXT("승강 이동 중");
	}
	else
	{
		ObjectiveLine = TEXT("갱도 미로 포털 → 폐교 탐사 → 승강기 귀환 → 납부대 입력 · 굴착은 노란 난간 안 흙을 좌클릭 유지");
	}

	if (PC && PC->IsDepositInputOpen())
	{
		ObjectiveLine = TEXT("납부 입력 중 — 금액 숫자 입력 후 Enter 제출 / ESC 취소");
	}
}

namespace
{
	void FillRect(UCanvas* Canvas, float X, float Y, float W, float H, const FLinearColor& Color)
	{
		FCanvasTileItem Tile(FVector2D(X, Y), FVector2D(W, H), Color);
		Tile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Tile);
	}

	void StrokeRect(UCanvas* Canvas, float X, float Y, float W, float H, float T, const FLinearColor& Color)
	{
		FillRect(Canvas, X, Y, W, T, Color);
		FillRect(Canvas, X, Y + H - T, W, T, Color);
		FillRect(Canvas, X, Y + T, T, H - 2.f * T, Color);
		FillRect(Canvas, X + W - T, Y + T, T, H - 2.f * T, Color);
	}

	void DrawLabel(UCanvas* Canvas, const UFont* Font, float Scale, const FString& Text, float X, float Y, const FColor& Color)
	{
		FFontRenderInfo RenderInfo;
		RenderInfo.bEnableShadow = true;
		Canvas->SetDrawColor(Color);
		Canvas->DrawText(Font, Text, X, Y, Scale, Scale, RenderInfo);
		Canvas->SetDrawColor(FColor::White);
	}

	void DrawCenteredLabel(UCanvas* Canvas, const UFont* Font, float Scale, const FString& Text, float CenterX, float CenterY, const FColor& Color)
	{
		float W = 0.f;
		float H = 0.f;
		Canvas->TextSize(Font, Text, W, H, Scale, Scale);
		DrawLabel(Canvas, Font, Scale, Text, CenterX - W * 0.5f, CenterY - H * 0.5f, Color);
	}

	void DrawBar(UCanvas* Canvas, const UFont* Font, float Scale, float X, float Y, float W, float H,
		float Fraction, const FLinearColor& Fill, const FString& Label)
	{
		FillRect(Canvas, X, Y, W, H, FLinearColor(0.f, 0.f, 0.f, 0.6f));
		FillRect(Canvas, X, Y, W * FMath::Clamp(Fraction, 0.f, 1.f), H, Fill);
		StrokeRect(Canvas, X, Y, W, H, FMath::Max(1.f, Scale), FLinearColor(0.f, 0.f, 0.f, 0.9f));
		DrawCenteredLabel(Canvas, Font, Scale * 0.9f, Label, X + W * 0.5f, Y + H * 0.5f, FColor::White);
	}

	void DrawSlot(UCanvas* Canvas, const UFont* Font, float Scale, float X, float Y, float Size,
		int32 Number, const FString& Content, bool bSelected, const FLinearColor& Border)
	{
		FillRect(Canvas, X, Y, Size, Size, FLinearColor(0.f, 0.f, 0.f, Content.IsEmpty() ? 0.5f : 0.72f));
		StrokeRect(Canvas, X, Y, Size, Size, bSelected ? 3.f * Scale : FMath::Max(1.f, 1.5f * Scale),
			bSelected ? FLinearColor(1.f, 0.85f, 0.25f, 1.f) : Border);
		DrawLabel(Canvas, Font, Scale * 0.85f, FString::FromInt(Number), X + 4.f * Scale, Y + 2.f * Scale, FColor(200, 200, 200));
		if (!Content.IsEmpty())
		{
			DrawCenteredLabel(Canvas, Font, Scale * 0.9f, Content, X + Size * 0.5f, Y + Size * 0.62f, FColor::White);
		}
	}

	void DrawCrosshair(UCanvas* Canvas, float Scale, float CenterX, float CenterY)
	{
		const float Thick = FMath::Max(2.f, 2.f * Scale);
		const float Arm = 8.f * Scale;
		const float Gap = 5.f * Scale;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			const float Pad = Pass == 0 ? 1.f : 0.f;
			const FLinearColor Color = Pass == 0 ? FLinearColor(0.f, 0.f, 0.f, 0.6f) : FLinearColor(1.f, 1.f, 1.f, 0.9f);
			FillRect(Canvas, CenterX - Thick * 0.5f - Pad, CenterY - Thick * 0.5f - Pad, Thick + 2.f * Pad, Thick + 2.f * Pad, Color);
			FillRect(Canvas, CenterX - Gap - Arm - Pad, CenterY - Thick * 0.5f - Pad, Arm + 2.f * Pad, Thick + 2.f * Pad, Color);
			FillRect(Canvas, CenterX + Gap - Pad, CenterY - Thick * 0.5f - Pad, Arm + 2.f * Pad, Thick + 2.f * Pad, Color);
			FillRect(Canvas, CenterX - Thick * 0.5f - Pad, CenterY - Gap - Arm - Pad, Thick + 2.f * Pad, Arm + 2.f * Pad, Color);
			FillRect(Canvas, CenterX - Thick * 0.5f - Pad, CenterY + Gap - Pad, Thick + 2.f * Pad, Arm + 2.f * Pad, Color);
		}
	}
}

void ADigetherPrototypeHUD::DrawHUD()
{
	Super::DrawHUD();
	if (const APlayerController* OwnerController = GetOwningPlayerController())
	{
		if (OwnerController->WasInputKeyJustPressed(EKeys::F1))
		{
			bShowHelpPanel = !bShowHelpPanel;
		}
	}
	RefreshDisplayText();
	DrawPrototypeHUD();
}

void ADigetherPrototypeHUD::DrawPrototypeHUD_Implementation()
{
	if (!Canvas)
	{
		return;
	}
	// 한국어 표시: 엔진 기본 폰트(GetSmallFont)의 내장 fallback을 사용한다.
	// 실제 글리프 렌더링은 에디터/빌드 실행 확인이 필요하며 이번 작업에서는 실행하지 않았다.
	const UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!Font)
	{
		return;
	}

	const float SizeX = static_cast<float>(Canvas->SizeX);
	const float SizeY = static_cast<float>(Canvas->SizeY);
	const float Scale = FMath::Clamp(SizeY / 720.f, 0.75f, 1.8f);
	const float Margin = 10.f * Scale;
	const float Gap = 6.f * Scale;
	const float ColumnW = FMath::Min((SizeX - 3.f * Margin) * 0.5f, 560.f * Scale);
	const float CenterX = SizeX * 0.5f;
	const float CenterY = SizeY * 0.5f;
	const FColor Amber(255, 215, 120);
	const FColor Dim(200, 200, 200);

	// 하단 중앙 슬롯 배치: 일반 4칸 + 간격을 둔 별도 히든 1칸.
	const float SlotSize = 54.f * Scale;
	const float SlotGap = 6.f * Scale;
	const float HiddenGap = 16.f * Scale;
	const float SlotsW = 5.f * SlotSize + 3.f * SlotGap + HiddenGap;
	const float SlotsX = (SizeX - SlotsW) * 0.5f;
	const float SlotsY = SizeY - Margin - SlotSize;
	const float LimitBottom = SlotsY - Gap;

	// 좌상단: 일차 · 목표 · Steam 상태 한 줄과 짧은 시험값 경고.
	TArray<FHudLine> Status;
	Status.Add({CompactStatusLine, FColor::White});
	Status.Add({NoticeLine, Amber});
	const float LeftBottom = DrawPanel(Canvas, Font, Scale, Margin, Margin, ColumnW, LimitBottom, false, Status);

	// 우상단: 현재 목표 한 줄(승강 상태가 있으면 이어 붙임)과 공용 알림.
	TArray<FHudLine> Objective;
	FString ObjectiveText = ObjectiveLine;
	if (!ElevatorLine.IsEmpty())
	{
		ObjectiveText += FString::Printf(TEXT(" · %s"), *ElevatorLine);
	}
	Objective.Add({ObjectiveText, Amber});
	if (!PublicMessageLine.IsEmpty())
	{
		Objective.Add({FString::Printf(TEXT("공용: %s"), *PublicMessageLine), FColor(140, 255, 170)});
	}
	const float RightBottom = DrawPanel(Canvas, Font, Scale, SizeX - Margin - ColumnW, Margin, ColumnW, LimitBottom, false, Objective);

	// 좌하단: 체력·스태미나 막대. 수분 등 정해지지 않은 값은 그리지 않는다.
	const float BarH = 18.f * Scale;
	const float BarW = FMath::Min(220.f * Scale, FMath::Max(SlotsX - 2.f * Margin, 80.f * Scale));
	float BarY = SizeY - Margin - BarH;
	if (bHasStamina)
	{
		DrawBar(Canvas, Font, Scale, Margin, BarY, BarW, BarH, StaminaFraction, FLinearColor(0.9f, 0.8f, 0.25f, 0.95f),
			FString::Printf(TEXT("스태미나 %d%%"), FMath::RoundToInt(StaminaFraction * 100.f)));
		BarY -= BarH + Gap;
	}
	if (bHasHealth)
	{
		DrawBar(Canvas, Font, Scale, Margin, BarY, BarW, BarH, HealthFraction, FLinearColor(0.85f, 0.2f, 0.2f, 0.95f),
			FString::Printf(TEXT("체력 %.0f"), HealthValue));
	}

	// 하단 중앙: 일반 슬롯 4칸과 별도 히든 슬롯. 로컬 소유자 데이터만 쓰며 히든은 보유 여부만 표시한다.
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FString Content = (bHasSlotData && SlotTexts.IsValidIndex(Index)) ? SlotTexts[Index] : FString();
		DrawSlot(Canvas, Font, Scale, SlotsX + Index * (SlotSize + SlotGap), SlotsY, SlotSize, Index + 1, Content,
			SelectedSlotIndex == Index + 1, FLinearColor(0.6f, 0.6f, 0.6f, 0.85f));
	}
	DrawSlot(Canvas, Font, Scale, SlotsX + 4.f * SlotSize + 3.f * SlotGap + HiddenGap, SlotsY, SlotSize, 5,
		(bHasSlotData && bHiddenSlotFilled) ? FString(TEXT("히든")) : FString(), SelectedSlotIndex == 5,
		FLinearColor(0.65f, 0.5f, 0.9f, 0.9f));

	// 우하단: F1 도움말 안내. 슬롯과 겹치면 생략한다.
	const float HintW = 200.f * Scale;
	if (SizeX - Margin - HintW > SlotsX + SlotsW + Gap)
	{
		TArray<FHudLine> Hint;
		Hint.Add({bShowHelpPanel ? FString(TEXT("F1 도움말 닫기")) : FString(TEXT("F1 도움말·체크리스트")), Dim});
		DrawPanel(Canvas, Font, Scale, SizeX - Margin - HintW, 0.f, HintW, SizeY - Margin, true, Hint);
	}

	// F1 상세 패널: 체크리스트, 상세 상태, 조작 안내. 중앙 조준점 위쪽 영역으로 제한한다.
	if (bShowHelpPanel)
	{
		TArray<FHudLine> Help;
		Help.Add({ChecklistTitle, Amber});
		for (const FString& Line : ChecklistLines)
		{
			Help.Add({Line, FColor::White});
		}
		Help.Add({NoticeLine, Amber});
		Help.Add({StatusLine, FColor::White});
		if (!ElevatorLine.IsEmpty())
		{
			Help.Add({ElevatorLine, FColor(140, 220, 255)});
		}
		Help.Add({PlayerLine, FColor::White});
		Help.Add({ControlsLine, Dim});
		Help.Add({TEXT("E 유지 상호작용·구조 · 좌클릭 굴착/공격 · F 보강·히든 · 놓거나 우클릭 취소"), Dim});
		Help.Add({TEXT("1~4 일반 슬롯 · 5 히든 · Q 놓기 · B 매몰 탈출 · F5 저장 · F9 불러오기(호스트)"), Dim});
		const float HelpW = FMath::Min(SizeX - 2.f * Margin, 760.f * Scale);
		DrawPanel(Canvas, Font, Scale, (SizeX - HelpW) * 0.5f, FMath::Max(LeftBottom, RightBottom) + Gap, HelpW,
			CenterY - 20.f * Scale, false, Help);
	}

	// 중앙: 조준점, 조준 설명, 개인 행동·실패 메시지, 로컬 유지 게이지.
	DrawCrosshair(Canvas, Scale, CenterX, CenterY);

	const bool bHold = HoldFraction > 0.f;
	if (!TargetLine.IsEmpty() || !MessageLine.IsEmpty() || bHold)
	{
		TArray<FHudLine> Aim;
		if (!TargetLine.IsEmpty())
		{
			Aim.Add({TargetLine, FColor::White});
		}
		if (!MessageLine.IsEmpty())
		{
			Aim.Add({FString::Printf(TEXT("알림: %s"), *MessageLine), FColor::Yellow});
		}
		if (bHold)
		{
			Aim.Add({FString::Printf(TEXT("유지 중 %d%% — 놓으면 취소"), FMath::RoundToInt(HoldFraction * 100.f)), FColor(255, 170, 60)});
		}
		const float AimW = FMath::Min(SizeX * 0.5f, 520.f * Scale);
		const float AimX = (SizeX - AimW) * 0.5f;
		const float AimY = FMath::Max(CenterY + 24.f * Scale,FMath::Max(LeftBottom, RightBottom) + Gap);
		const float HoldBarH = bHold ? 10.f * Scale : 0.f;
		const float AimBottom = DrawPanel(Canvas, Font, Scale, AimX, AimY, AimW, LimitBottom - HoldBarH - Gap, false, Aim);
		if (bHold && AimBottom + Gap + HoldBarH <= LimitBottom)
		{
			const float HoldBarY = AimBottom + Gap;
			FCanvasTileItem BarBack(FVector2D(AimX, HoldBarY), FVector2D(AimW, HoldBarH), FLinearColor(0.f, 0.f, 0.f, 0.6f));
			BarBack.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(BarBack);
			FCanvasTileItem BarFill(FVector2D(AimX, HoldBarY), FVector2D(AimW * FMath::Clamp(HoldFraction, 0.f, 1.f), HoldBarH),FLinearColor(1.f, 0.65f, 0.2f, 0.95f));
			BarFill.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(BarFill);
		}
	}
}
