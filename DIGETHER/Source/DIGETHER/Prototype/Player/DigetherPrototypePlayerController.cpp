#include "DigetherPrototypePlayerController.h"
#include "DigetherPrototypeCharacter.h"
#include "DigetherDepositWidget.h"

#include "Prototype/World/DigetherPrototypeWorld.h"
#include "Blueprint/UserWidget.h"
#include "Prototype/DigetherPrototypeGameMode.h"
#include "Prototype/DigetherPrototypePlayerState.h"
#include "Prototype/DigetherPrototypeGameState.h"
#include "Prototype/DigetherPrototypeCorpse.h"
#include "Prototype/World/DigetherExcavationTerrain.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"

ADigetherPrototypePlayerController::ADigetherPrototypePlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	DepositWidgetClass = UDigetherDepositWidget::StaticClass();
}

void ADigetherPrototypePlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickServerDeposit();
	TickLocalDeposit();
	if (!IsLocalController() || !GetWorld() || !GetPawn()) { return; }
	const ADigetherPrototypePlayerState* LocalState = GetPlayerState<ADigetherPrototypePlayerState>();
	if (!LocalState) { return; }
	if (LocalState->PrivateState.Life == EPrototypeLife::Active || LocalState->PrivateState.Life == EPrototypeLife::Buried || LocalState->PrivateState.Life == EPrototypeLife::InTransit)
	{
		if (GetViewTarget() != GetPawn()) { SetViewTarget(GetPawn()); }
		return;
	}
	APawn* Current = Cast<APawn>(GetViewTarget());
	const ADigetherPrototypePlayerState* CurrentState = Current ? Current->GetPlayerState<ADigetherPrototypePlayerState>() : nullptr;
	if (Current && Current != GetPawn() && CurrentState && CurrentState->PublicLife == EPrototypeLife::Active) { return; }
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Candidate = *It;
		const ADigetherPrototypePlayerState* CandidateState = Candidate ? Candidate->GetPlayerState<ADigetherPrototypePlayerState>() : nullptr;
		if (Candidate != GetPawn() && CandidateState && CandidateState->PublicLife == EPrototypeLife::Active)
		{
			SetViewTarget(Candidate);
			return;
		}
	}
}

void ADigetherPrototypePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearServerDepositSession();
	CloseLocalDeposit();
	Super::EndPlay(EndPlayReason);
}

void ADigetherPrototypePlayerController::PawnLeavingGame()
{
	ClearServerDepositSession();
	CloseLocalDeposit();
	if (HasAuthority())
	{
		if (ADigetherPrototypeGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ADigetherPrototypeGameMode>() : nullptr)
		{
			if (Mode->HandleControllerDisconnect(this))
			{
				UnPossess();
				return;
			}
		}
	}
	Super::PawnLeavingGame();
}

void ADigetherPrototypePlayerController::FlushPressedKeys()
{
	Super::FlushPressedKeys();
	if (IsLocalController())
	{
		if (ADigetherPrototypeCharacter* PrototypeCharacter = Cast<ADigetherPrototypeCharacter>(GetPawn())) { PrototypeCharacter->ResetLocalInput(); }
		ServerCancelInteraction();
	}
}

bool ADigetherPrototypePlayerController::ValidateInteractRequest(AActor* Target, FString& OutMessage) const
{
	if (!IsValid(Target))
	{
		OutMessage = TEXT("대상을 찾을 수 없습니다.");
		return false;
	}
	UWorld* World = GetWorld();
	if (!World || Target->GetWorld() != World)
	{
		OutMessage = TEXT("동일한 월드의 대상이 아닙니다.");
		return false;
	}
	APawn* MyPawn = GetPawn();
	if (!IsValid(MyPawn))
	{
		OutMessage = TEXT("캐릭터를 찾을 수 없습니다.");
		return false;
	}
	FVector Eye;
	FRotator EyeRotation;
	MyPawn->GetActorEyesViewPoint(Eye, EyeRotation);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PrototypeControllerInteract), false, MyPawn);
	if (World->LineTraceSingleByChannel(Hit, Eye, Target->GetActorLocation(), ECC_Visibility, Params) && Hit.GetActor() != Target)
	{
		OutMessage = TEXT("대상이 시야에서 가려져 있습니다.");
		return false;
	}
	return true;
}

void ADigetherPrototypePlayerController::ServerInteract_Implementation(AActor* Target, bool bAlternate)
{
	if (!HasAuthority())
	{
		return;
	}
	FString OutMessage;
	UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (Now - LastInteractTime < static_cast<double>(MinInteractIntervalSeconds))
	{
		ClientPrototypeMessage(TEXT("상호작용이 너무 빠릅니다. 잠시 후 다시 시도하세요."));
		return;
	}

	ADigetherPrototypeGameMode* Mode = World ? World->GetAuthGameMode<ADigetherPrototypeGameMode>() : nullptr;
	if (!Mode || !Mode->CanInteract(this, Target, OutMessage))
	{
		ClientPrototypeMessage(OutMessage.IsEmpty() ? TEXT("상호작용할 수 없습니다.") : OutMessage);
		return;
	}
	if (!ValidateInteractRequest(Target, OutMessage))
	{
		ClientPrototypeMessage(OutMessage);
		return;
	}
	IDigetherPrototypeInteractable* Interactable = Cast<IDigetherPrototypeInteractable>(Target);
	if (!Interactable)
	{
		ClientPrototypeMessage(TEXT("상호작용할 수 없는 대상입니다."));
		return;
	}

	if (ADigetherPrototypeStation* Station = Cast<ADigetherPrototypeStation>(Target))
	{
		if (Station->Kind == EPrototypeStationKind::Deposit)
		{
			LastInteractTime = Now;
			OpenServerDepositSession(Station);
			return;
		}
	}

	// Starting another action closes the amount-entry session on authority too.
	if (ServerDepositSessionId != 0) { EndServerDepositSession(FString()); }
	LastInteractTime = Now;
	Interactable->InteractPrototype(this, bAlternate, OutMessage);
	if (!OutMessage.IsEmpty())
	{
		ClientPrototypeMessage(OutMessage);
	}
}

bool ADigetherPrototypePlayerController::TraceTerrainRequest(bool bRequireTool, FHitResult& OutHit, FString& OutMessage)
{
	UWorld* World = GetWorld();
	ADigetherPrototypeCharacter* PrototypePawn = Cast<ADigetherPrototypeCharacter>(GetPawn());
	const ADigetherPrototypePlayerState* PrototypePlayer = GetPlayerState<ADigetherPrototypePlayerState>();
	const ADigetherPrototypeGameState* State = World ? World->GetGameState<ADigetherPrototypeGameState>() : nullptr;
	if (!HasAuthority() || !World || !IsValid(PrototypePawn) || !PrototypePlayer || !State ||
		State->PublicState.Phase != EPrototypePhase::Expedition || PrototypePlayer->PrivateState.Life != EPrototypeLife::Active) { return false; }

	const double Now = World->GetTimeSeconds();
	if (Now - LastInteractTime < FMath::Max(0.01f, MinInteractIntervalSeconds)) { return false; }
	// Rejected requests consume the interval to bound server work.
	LastInteractTime = Now;
	bool bHasTool = false;
	for (const FPrototypeItem& Item : PrototypePlayer->PrivateState.Items)
	{
		bHasTool |= Item.bEquipment;
		if (Item.bTwoHanded) { OutMessage = TEXT("양손 화물을 내려놓아야 작업할 수 있습니다."); return false; }
	}
	if (bRequireTool && !bHasTool) { OutMessage = TEXT("굴착하려면 시험 장비가 필요합니다."); return false; }
	for (TActorIterator<ADigetherPrototypeCorpse> It(World); It; ++It)
	{
		if (It->Carrier == PrototypePawn) { OutMessage = TEXT("시체를 내려놓아야 작업할 수 있습니다."); return false; }
	}

	FVector Origin;
	const FRotator Aim = GetControlRotation();
	// Use the server's component transform, matching the local reticle when crouched.
	if (!PrototypePawn->GetPrototypeAimOrigin(Origin) || Aim.ContainsNaN() || !FMath::IsFinite(PrototypePawn->InteractTraceDistance)) { return false; }
	const float Reach = FMath::Clamp(PrototypePawn->InteractTraceDistance, 1.f, 1000.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PrototypeServerTerrain), false, PrototypePawn);
	if (!World->LineTraceSingleByChannel(OutHit, Origin, Origin + Aim.Vector() * Reach, ECC_Visibility, Params)) { return false; }
	ADigetherExcavationTerrain* Terrain = Cast<ADigetherExcavationTerrain>(OutHit.GetActor());
	return IsValid(Terrain) && Terrain->GetWorld() == World && !OutHit.ImpactPoint.ContainsNaN();
}

void ADigetherPrototypePlayerController::ServerDig_Implementation()
{
	FHitResult Hit;
	FString Reason;
	if (!TraceTerrainRequest(true, Hit, Reason))
	{
		if (!Reason.IsEmpty()) { ClientPrototypeMessage(Reason); }
		return;
	}
	if (ADigetherExcavationTerrain* Terrain = Cast<ADigetherExcavationTerrain>(Hit.GetActor()))
	{
		if (Terrain->ServerExcavateAt(Hit.ImpactPoint, Reason)) { ClientDigConfirmed(); }
		else if (!Reason.IsEmpty()) { ClientPrototypeMessage(Reason); }
	}
}

void ADigetherPrototypePlayerController::ServerSupportTerrain_Implementation()
{
	FHitResult Hit;
	FString Reason;
	if (!TraceTerrainRequest(false, Hit, Reason))
	{
		if (!Reason.IsEmpty()) { ClientPrototypeMessage(Reason); }
		return;
	}
	if (ADigetherExcavationTerrain* Terrain = Cast<ADigetherExcavationTerrain>(Hit.GetActor()))
	{
		Terrain->ServerSupportAt(Hit.ImpactPoint, this, Reason);
		if (!Reason.IsEmpty()) { ClientPrototypeMessage(Reason); }
	}
}

void ADigetherPrototypePlayerController::ClientDigConfirmed_Implementation()
{
	if (ADigetherPrototypeCharacter* PrototypePawn = Cast<ADigetherPrototypeCharacter>(GetPawn()))
	{
		PrototypePawn->PlayConfirmedDigSwing();
	}
}

void ADigetherPrototypePlayerController::ServerCancelInteraction_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}
	if (ADigetherPrototypeGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ADigetherPrototypeGameMode>() : nullptr)
	{
		Mode->CancelInteraction(this);
	}
}

void ADigetherPrototypePlayerController::ServerPawnAction_Implementation(APawn* Target, bool bAttack)
{
	ADigetherPrototypeGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ADigetherPrototypeGameMode>() : nullptr;
	FString Message;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (!HasAuthority() || !Mode || !Target || Now - LastInteractTime < MinInteractIntervalSeconds ||
		!Mode->CanInteract(this, Target, Message) || !ValidateInteractRequest(Target, Message))
	{
		ClientPrototypeMessage(Message.IsEmpty() ? TEXT("대상에게 접근할 수 없습니다.") : Message);
		return;
	}
	APlayerController* Other = Cast<APlayerController>(Target->GetController());
	if (!Other || Other == this) { ClientPrototypeMessage(TEXT("연결된 다른 플레이어가 필요합니다.")); return; }
	LastInteractTime = Now;
	if (bAttack)
	{
		const ADigetherPrototypePlayerState* PrototypePlayerState = GetPlayerState<ADigetherPrototypePlayerState>();
		bool bEquipment = false;
		bool bTwoHanded = false;
		if (PrototypePlayerState) for (const FPrototypeItem& Item : PrototypePlayerState->PrivateState.Items) { bEquipment |= Item.bEquipment; bTwoHanded |= Item.bTwoHanded; }
		if (!bEquipment || bTwoHanded) { ClientPrototypeMessage(TEXT("공격할 시험 장비가 필요하며 양손 화물 운반 중에는 사용할 수 없습니다.")); return; }
		Mode->RequestDamage(this, Other, 25.f, Message);
	}
	else { Mode->RequestRescue(this, Other, Message); }
	ClientPrototypeMessage(Message.IsEmpty() ? TEXT("해당 행동을 수행할 수 없습니다.") : Message);
}

void ADigetherPrototypePlayerController::ServerDrop_Implementation(int32 Slot)
{
	ADigetherPrototypeGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ADigetherPrototypeGameMode>() : nullptr;
	ADigetherPrototypePlayerState* PrototypePlayerState = GetPlayerState<ADigetherPrototypePlayerState>();
	FString Message;
	int32 NormalSlot = 0;
	int32 ItemIndex = INDEX_NONE;
	if (PrototypePlayerState && Slot >= 1 && Slot <= 5)
	{
		for (int32 Index = 0; Index < PrototypePlayerState->PrivateState.Items.Num(); ++Index)
		{
			const bool bHiddenSlot = PrototypePlayerState->PrivateState.Items[Index].bHidden;
			if ((Slot == 5 && bHiddenSlot) || (!bHiddenSlot && ++NormalSlot == Slot)) { ItemIndex = Index; break; }
		}
	}
	if (Mode) { Mode->RequestDrop(this, ItemIndex, Message); }
	ClientPrototypeMessage(Message.IsEmpty() ? TEXT("선택한 슬롯이 비어 있습니다.") : Message);
}

void ADigetherPrototypePlayerController::ServerSelfEscape_Implementation()
{
	FString Message;
	if (ADigetherPrototypeGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ADigetherPrototypeGameMode>() : nullptr) { Mode->RequestSelfEscape(this, Message); }
	ClientPrototypeMessage(Message.IsEmpty() ? TEXT("스스로 탈출할 수 없습니다.") : Message);
}

void ADigetherPrototypePlayerController::ServerSaveOrLoad_Implementation(bool bLoad)
{
	FString Message;
	if (ADigetherPrototypeGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ADigetherPrototypeGameMode>() : nullptr)
	{
		if (bLoad) { Mode->RequestLoad(this, Message); } else { Mode->RequestSave(this, Message); }
	}
	ClientPrototypeMessage(Message.IsEmpty() ? TEXT("시험 저장 작업이 실패했습니다.") : Message);
}

void ADigetherPrototypePlayerController::ClientPrototypeMessage_Implementation(const FString& Message)
{
	LastServerMessage = Message;
	LastMessageUntil = GetWorld() ? GetWorld()->GetTimeSeconds() + 4.f : 0.f;
}

FString ADigetherPrototypePlayerController::GetLastServerMessage() const
{
	return GetWorld() && GetWorld()->GetTimeSeconds() < LastMessageUntil ? LastServerMessage : FString();
}

void ADigetherPrototypePlayerController::SetAimTarget(AActor* NewTarget, const FString& Hint)
{
	AimTarget = NewTarget;
	AimHint = Hint;
}

bool ADigetherPrototypePlayerController::TryParseDepositAmount(const FString& Input, int32& OutAmount, FString& OutError)
{
	OutAmount = 0;
	const FString Trimmed = Input.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		OutError = TEXT("금액을 입력하세요.");
		return false;
	}
	int64 Value = 0;
	for (int32 Index = 0; Index < Trimmed.Len(); ++Index)
	{
		const TCHAR Ch = Trimmed[Index];
		if (Ch < TEXT('0') || Ch > TEXT('9'))
		{
			OutError = TEXT("숫자만 입력할 수 있습니다.");
			return false;
		}
		Value = Value * 10 + static_cast<int64>(Ch - TEXT('0'));
		if (Value > static_cast<int64>(MAX_int32))
		{
			OutError = TEXT("입력할 수 있는 범위를 넘었습니다.");
			return false;
		}
	}
	if (Value <= 0)
	{
		OutError = TEXT("1 이상의 금액을 입력하세요.");
		return false;
	}
	OutAmount = static_cast<int32>(Value);
	OutError.Reset();
	return true;
}

void ADigetherPrototypePlayerController::ClearServerDepositSession()
{
	ServerDepositSessionId = 0;
	ServerDepositStation.Reset();
	ServerDepositDay = 0;
	ServerDepositQuotaTarget = 0;
	ServerDepositPeriodGeneration = 0;
	ServerDepositExpiresAt = 0.0;
}

void ADigetherPrototypePlayerController::EndServerDepositSession(const FString& Message)
{
	const int32 EndedId = ServerDepositSessionId;
	ClearServerDepositSession();
	if (EndedId != 0) { ClientCloseDeposit(EndedId, Message); }
}

bool ADigetherPrototypePlayerController::ValidateServerDepositSession(FString& OutMessage)
{
	UWorld* World = GetWorld();
	ADigetherPrototypeStation* Station = ServerDepositStation.Get();
	const ADigetherPrototypeGameState* State = World ? World->GetGameState<ADigetherPrototypeGameState>() : nullptr;
	const ADigetherPrototypePlayerState* PlayerStateRef = GetPlayerState<ADigetherPrototypePlayerState>();
	ADigetherPrototypeGameMode* Mode = World ? World->GetAuthGameMode<ADigetherPrototypeGameMode>() : nullptr;
	if (!World || !State || !PlayerStateRef || !Mode)
	{
		OutMessage = TEXT("납부 상태를 확인할 수 없습니다.");
		return false;
	}
	if (!IsValid(Station) || Station->Kind != EPrototypeStationKind::Deposit)
	{
		OutMessage = TEXT("납부대를 찾을 수 없습니다.");
		return false;
	}
	if (State->PublicState.Phase != EPrototypePhase::Base)
	{
		OutMessage = TEXT("거점에서만 납부할 수 있습니다.");
		return false;
	}
	if (Mode->GetPeriodGeneration() != ServerDepositPeriodGeneration ||
		State->PublicState.Day != ServerDepositDay || State->PublicState.QuotaTarget != ServerDepositQuotaTarget)
	{
		OutMessage = TEXT("납부 목표 기간이 바뀌었습니다. 납부대에서 다시 시도하세요.");
		return false;
	}
	if (PlayerStateRef->PrivateState.Life != EPrototypeLife::Active)
	{
		OutMessage = TEXT("행동할 수 없는 상태에서는 납부할 수 없습니다.");
		return false;
	}
	if (World->GetTimeSeconds() > ServerDepositExpiresAt)
	{
		OutMessage = TEXT("납부 입력 시간이 지났습니다. 납부대에서 다시 시도하세요.");
		return false;
	}
	OutMessage.Reset();
	if (!Mode->CanInteract(this, Station, OutMessage))
	{
		if (OutMessage.IsEmpty()) { OutMessage = TEXT("납부대에서 멀어져 입력을 닫았습니다."); }
		return false;
	}
	if (!ValidateInteractRequest(Station, OutMessage))
	{
		return false;
	}
	return true;
}

void ADigetherPrototypePlayerController::OpenServerDepositSession(ADigetherPrototypeStation* Station)
{
	UWorld* World = GetWorld();
	const ADigetherPrototypeGameState* State = World ? World->GetGameState<ADigetherPrototypeGameState>() : nullptr;
	const ADigetherPrototypePlayerState* PlayerStateRef = GetPlayerState<ADigetherPrototypePlayerState>();
	if (!HasAuthority() || !World || !State || !PlayerStateRef || !IsValid(Station)) { return; }
	if (State->PublicState.Phase != EPrototypePhase::Base)
	{
		ClientPrototypeMessage(TEXT("거점에서만 납부할 수 있습니다."));
		return;
	}
	if (PlayerStateRef->PrivateState.Money <= 0)
	{
		ClientPrototypeMessage(TEXT("납부할 보유금이 없습니다."));
		return;
	}
	// A client can bypass local input suppression: never retain a lever hold
	// while opening a payment session.
	World->GetAuthGameMode<ADigetherPrototypeGameMode>()->CancelInteraction(this);
	for (TActorIterator<ADigetherPrototypePlayerController> It(World); It; ++It)
	{
		const ADigetherPrototypePlayerController* Other = *It;
		if (Other && Other != this && Other->ServerDepositSessionId != 0 && Other->ServerDepositStation.Get() == Station)
		{
			ClientPrototypeMessage(TEXT("다른 작업자가 납부대를 사용 중입니다."));
			return;
		}
	}
	const double Now = World->GetTimeSeconds();
	if (ServerDepositSessionId != 0 && ServerDepositStation.Get() == Station)
	{
		ServerDepositExpiresAt = Now + FMath::Max(5.f, DepositSessionTimeoutSeconds);
		ClientOpenDeposit(Station, ServerDepositSessionId);
		return;
	}
	if (ServerDepositSessionId != 0) { EndServerDepositSession(FString()); }
	ServerDepositSessionId = NextDepositSessionId;
	NextDepositSessionId = NextDepositSessionId == MAX_int32 ? 1 : NextDepositSessionId + 1;
	ServerDepositStation = Station;
	ServerDepositDay = State->PublicState.Day;
	ServerDepositQuotaTarget = State->PublicState.QuotaTarget;
	ServerDepositPeriodGeneration = World->GetAuthGameMode<ADigetherPrototypeGameMode>()->GetPeriodGeneration();
	ServerDepositExpiresAt = Now + FMath::Max(5.f, DepositSessionTimeoutSeconds);
	NextServerDepositCheck = Now + 0.1;
	ClientOpenDeposit(Station, ServerDepositSessionId);
}

void ADigetherPrototypePlayerController::TickServerDeposit()
{
	UWorld* World = GetWorld();
	if (ServerDepositSessionId == 0 || !HasAuthority() || !World) { return; }
	const double Now = World->GetTimeSeconds();
	if (Now < NextServerDepositCheck) { return; }
	NextServerDepositCheck = Now + 0.1;
	FString Message;
	if (!ValidateServerDepositSession(Message)) { EndServerDepositSession(Message); }
}

void ADigetherPrototypePlayerController::ServerDepositAmount_Implementation(ADigetherPrototypeStation* Station, int32 Amount, int32 ExpectedCycle)
{
	if (!HasAuthority()) { return; }
	UWorld* World = GetWorld();
	if (!World) { return; }
	const double Now = World->GetTimeSeconds();
	if (Now - LastDepositSubmitTime < static_cast<double>(FMath::Max(0.01f, MinInteractIntervalSeconds)))
	{
		// Do not amplify repeated invalid-session requests into reliable replies.
		if (ExpectedCycle == ServerDepositSessionId && ServerDepositSessionId != 0)
			ClientDepositRejected(ExpectedCycle, TEXT("요청이 너무 빠릅니다. 잠시 후 다시 시도하세요."));
		return;
	}
	LastDepositSubmitTime = Now;
	if (ServerDepositSessionId == 0 || ExpectedCycle != ServerDepositSessionId)
	{
		ClientCloseDeposit(ExpectedCycle, TEXT("납부 입력이 만료되었습니다. 납부대에서 다시 시도하세요."));
		return;
	}
	ADigetherPrototypeGameMode* Mode = World ? World->GetAuthGameMode<ADigetherPrototypeGameMode>() : nullptr;
	const ADigetherPrototypePlayerState* PlayerStateRef = GetPlayerState<ADigetherPrototypePlayerState>();
	if (!World || !Mode || !PlayerStateRef)
	{
		EndServerDepositSession(TEXT("납부 상태를 확인할 수 없습니다."));
		return;
	}
	if (!IsValid(Station) || Station != ServerDepositStation.Get() || Station->Kind != EPrototypeStationKind::Deposit)
	{
		EndServerDepositSession(TEXT("납부대가 일치하지 않아 입력을 닫았습니다."));
		return;
	}
	FString Message;
	if (!ValidateServerDepositSession(Message))
	{
		EndServerDepositSession(Message);
		return;
	}
	if (Amount <= 0)
	{
		ClientDepositRejected(ExpectedCycle, TEXT("1 이상의 금액을 입력하세요."));
		return;
	}
	if (Amount > PlayerStateRef->PrivateState.Money)
	{
		ClientDepositRejected(ExpectedCycle, TEXT("보유금보다 큰 금액은 납부할 수 없습니다."));
		return;
	}
	ServerDepositExpiresAt = Now + FMath::Max(5.f, DepositSessionTimeoutSeconds);
	if (Mode->RequestDeposit(this, Amount, Message))
	{
		const int32 EndedId = ServerDepositSessionId;
		ClearServerDepositSession();
		ClientCloseDeposit(EndedId, Message.IsEmpty() ? FString(TEXT("납부했습니다.")) : Message);
		return;
	}
	ClientDepositRejected(ExpectedCycle, Message.IsEmpty() ? FString(TEXT("납부할 수 없습니다.")) : Message);
}

void ADigetherPrototypePlayerController::ServerCancelDeposit_Implementation(int32 SessionId)
{
	if (!HasAuthority() || SessionId == 0 || SessionId != ServerDepositSessionId) { return; }
	ClearServerDepositSession();
}

void ADigetherPrototypePlayerController::ClientOpenDeposit_Implementation(ADigetherPrototypeStation* Station, int32 SessionId)
{
	if (!IsLocalController() || !IsValid(Station) || SessionId <= 0) { return; }
	if (LocalDepositSessionId == SessionId && LocalDepositStation.Get() == Station)
	{
		ApplyLocalDepositUI();
		return;
	}
	if (LocalDepositSessionId != 0) { CloseLocalDeposit(); }
	LocalDepositSessionId = SessionId;
	LocalDepositStation = Station;
	DepositInputText.Reset();
	DepositInputError.Reset();
	bDepositSubmitting = false;
	ApplyLocalDepositUI();
}

void ADigetherPrototypePlayerController::ClientCloseDeposit_Implementation(int32 SessionId, const FString& Message)
{
	if (!Message.IsEmpty()) { ClientPrototypeMessage_Implementation(Message); }
	if (SessionId != 0 && SessionId == LocalDepositSessionId) { CloseLocalDeposit(); }
}

void ADigetherPrototypePlayerController::ClientDepositRejected_Implementation(int32 SessionId, const FString& Message)
{
	if (SessionId == 0 || SessionId != LocalDepositSessionId) { return; }
	DepositInputError = Message;
	bDepositSubmitting = false;
	RefreshDepositWidget();
}

void ADigetherPrototypePlayerController::SetDepositInputText(const FString& Text)
{
	if (!IsDepositInputOpen() || DepositInputText == Text) { return; }
	DepositInputText = Text;
	DepositInputError.Reset();
	if (DepositWidget)
	{
		DepositWidget->SetInputText(Text);
		DepositWidget->SetErrorMessage(DepositInputError);
	}
}

void ADigetherPrototypePlayerController::SubmitDepositInput()
{
	if (!IsDepositInputOpen() || bDepositSubmitting) { return; }
	ADigetherPrototypeStation* Station = LocalDepositStation.Get();
	if (!IsValid(Station))
	{
		CloseLocalDeposit();
		return;
	}
	int32 Amount = 0;
	FString ParseError;
	if (!TryParseDepositAmount(DepositInputText, Amount, ParseError))
	{
		DepositInputError = ParseError;
		RefreshDepositWidget();
		return;
	}
	DepositInputError.Reset();
	bDepositSubmitting = true;
	RefreshDepositWidget();
	ServerDepositAmount(Station, Amount, LocalDepositSessionId);
}

void ADigetherPrototypePlayerController::CloseDepositInput()
{
	if (!IsDepositInputOpen()) { return; }
	const int32 ClosingId = LocalDepositSessionId;
	CloseLocalDeposit();
	ServerCancelDeposit(ClosingId);
}

void ADigetherPrototypePlayerController::TickLocalDeposit()
{
	if (!IsLocalController() || LocalDepositSessionId == 0) { return; }
	if (!LocalDepositStation.IsValid())
	{
		CloseLocalDeposit();
		return;
	}
	RefreshDepositWidget();
}

void ADigetherPrototypePlayerController::RefreshDepositWidget()
{
	if (!DepositWidget) { return; }
	const UWorld* World = GetWorld();
	const ADigetherPrototypeGameState* State = World ? World->GetGameState<ADigetherPrototypeGameState>() : nullptr;
	const ADigetherPrototypePlayerState* PlayerStateRef = GetPlayerState<ADigetherPrototypePlayerState>();
	const int32 Balance = PlayerStateRef ? PlayerStateRef->PrivateState.Money : 0;
	const int32 Remaining = State ? FMath::Max(0, State->PublicState.QuotaTarget - State->PublicState.QuotaProgress) : 0;
	DepositWidget->SetDepositInfo(Balance, Remaining);
	DepositWidget->SetErrorMessage(DepositInputError);
	DepositWidget->SetSubmitting(bDepositSubmitting);
}

void ADigetherPrototypePlayerController::ApplyLocalDepositUI()
{
	if (bDepositInputApplied || !IsLocalController() || IsRunningDedicatedServer()) { return; }
	if (!DepositWidget && DepositWidgetClass)
	{
		DepositWidget = CreateWidget<UDigetherDepositWidget>(this, DepositWidgetClass);
		if (DepositWidget) { DepositWidget->AddToViewport(50); }
	}
	bSavedShowMouseCursor = bShowMouseCursor;
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	if (DepositWidget)
	{
		TSharedPtr<SWidget> Focus = DepositWidget->GetInputSlateWidget();
		if (!Focus.IsValid()) { Focus = DepositWidget->TakeWidget(); }
		InputMode.SetWidgetToFocus(Focus);
	}
	SetInputMode(InputMode);
	if (DepositWidget) { DepositWidget->FocusInput(); }
	if (ADigetherPrototypeCharacter* PrototypeCharacter = Cast<ADigetherPrototypeCharacter>(GetPawn())) { PrototypeCharacter->ResetLocalInput(); }
	bDepositInputApplied = true;
	RefreshDepositWidget();
}

void ADigetherPrototypePlayerController::CloseLocalDeposit()
{
	if (DepositWidget)
	{
		DepositWidget->RemoveFromParent();
		DepositWidget = nullptr;
	}
	LocalDepositSessionId = 0;
	LocalDepositStation.Reset();
	DepositInputText.Reset();
	DepositInputError.Reset();
	bDepositSubmitting = false;
	if (!bDepositInputApplied) { return; }
	bDepositInputApplied = false;
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
	bShowMouseCursor = bSavedShowMouseCursor;
	SetInputMode(FInputModeGameOnly());
	if (ADigetherPrototypeCharacter* PrototypeCharacter = Cast<ADigetherPrototypeCharacter>(GetPawn())) { PrototypeCharacter->ResetLocalInput(); }
}
