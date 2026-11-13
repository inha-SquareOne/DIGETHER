#include "Game/Voice/DigetherVoiceComponent.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Run/DigetherGameOverPublicStateComponent.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Settings/DigetherGameUserSettings.h"
#include "Game/Settings/DigetherOptionsSettings.h"
#include "Game/Voice/DigetherVoiceLog.h"
#include "Game/Voice/DigetherVoiceTalker.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Game/Voice/DigetherVoiceTypes.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Net/VoiceConfig.h"
#include "Interfaces/VoiceInterface.h"
#include "OnlineSubsystemUtils.h"
#include "Sound/SoundAttenuation.h"
#include "TimerManager.h"

namespace
{
#if !UE_BUILD_SHIPPING
	TAutoConsoleVariable<bool> CVarVoiceProbe(TEXT("Digether.VoiceProbe"), false,
		TEXT("호스트에서 다른 참가자 발화 감지(IsRemotePlayerTalking·진폭·음소거)를 로그로 기록(PQ-13)"));

	// 키 입력 없이 누르고 말하기·토글 시험: Digether.VoicePushToTalk 1|0, 토글 모드는 1 → 0 한 번에 전환
	FAutoConsoleCommandWithWorldAndArgs VoicePushToTalkCommand(TEXT("Digether.VoicePushToTalk"),
		TEXT("로컬 플레이어의 누르고 말하기 입력을 누름(1)·뗌(0)으로 지정"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (World == nullptr || Args.Num() != 1)
			{
				return;
			}
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				APlayerController* Controller = It->Get();
				UDigetherVoiceComponent* Voice = IsValid(Controller) && Controller->IsLocalController()
					? Controller->FindComponentByClass<UDigetherVoiceComponent>() : nullptr;
				if (Voice != nullptr)
				{
					Voice->SetPushToTalkHeld(Args[0] == TEXT("1"));
				}
			}
		}));
#endif

	bool IsSpectatorTalker(const APlayerState* State)
	{
		const ADigetherGameplayPlayerState* Gameplay = Cast<ADigetherGameplayPlayerState>(State);
		const UDigetherLifeStateComponent* Life = IsValid(Gameplay) ? Gameplay->GetLifeComponent() : nullptr;
		return Life != nullptr && Life->CaptureState().PublicState.bSpectating;
	}
}

UDigetherVoiceComponent::UDigetherVoiceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

APlayerController* UDigetherVoiceComponent::GetOwningController() const
{
	return Cast<APlayerController>(GetOwner());
}

void UDigetherVoiceComponent::BeginPlay()
{
	Super::BeginPlay();
	APlayerController* Controller = GetOwningController();
	if (!IsValid(Controller) || !Controller->IsLocalController() || GetWorld() == nullptr)
	{
		return;
	}
	SurvivorAttenuation = NewObject<USoundAttenuation>(this, TEXT("SurvivorVoiceAttenuation"));
	FSoundAttenuationSettings& Settings = SurvivorAttenuation->Attenuation;
	Settings.bAttenuate = true;
	// 방향감 켬(기획 G)
	Settings.bSpatialize = true;
	// 가까울 때 빠르게, 멀수록 천천히 작아지는 자연 감쇠
	Settings.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	Settings.AttenuationShape = EAttenuationShape::Sphere;
	Settings.AttenuationShapeExtents = FVector(DigetherVoiceDefaults::RangeFullCm, 0.f, 0.f);
	Settings.FalloffDistance = DigetherVoiceDefaults::RangeMaxCm - DigetherVoiceDefaults::RangeFullCm;

	BuriedAttenuation = NewObject<USoundAttenuation>(this, TEXT("BuriedVoiceAttenuation"));
	BuriedAttenuation->Attenuation = Settings;
	BuriedAttenuation->Attenuation.FalloffDistance = DigetherVoiceDefaults::RangeMaxCm
		* DigetherVoiceDefaults::BuriedRangeRatio - DigetherVoiceDefaults::RangeFullCm;
	UnspatializedAttenuation = NewObject<USoundAttenuation>(this, TEXT("UnspatializedVoiceAttenuation"));
	UnspatializedAttenuation->Attenuation.bAttenuate = false;
	UnspatializedAttenuation->Attenuation.bSpatialize = false;

	GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this, &ThisClass::Refresh,
		DigetherVoiceDefaults::RefreshSeconds, true);
	Refresh();
}

void UDigetherVoiceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
	}
	if (APlayerController* Controller = GetOwningController())
	{
		if (Controller->IsLocalController())
		{
			Controller->ClearAudioListenerAttenuationOverride();
			if (AppliedMappingContext != nullptr)
			{
				if (const ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer())
				{
					if (UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
					{
						Input->RemoveMappingContext(AppliedMappingContext);
					}
				}
			}
		}
	}
	Talkers.Reset();
	Super::EndPlay(EndPlayReason);
}

void UDigetherVoiceComponent::HandleServerVoiceEnabled(const bool bEnable)
{
	bServerVoiceEnabled = bEnable;
	// 엔진이 접속·맵 이동 때 송신 상태를 바꿨을 수 있어 다시 적용
	bHasAppliedSpeaking = false;
	ApplyMicrophoneMode();
}

void UDigetherVoiceComponent::SetPushToTalkHeld(const bool bHeld)
{
	const UDigetherGameUserSettings* Settings = UDigetherGameUserSettings::Get();
	// 토글 모드는 누를 때마다 켜짐/꺼짐 전환, 떼는 입력은 무시
	if (bHeld && !bPushToTalkHeld && Settings != nullptr && Settings->GetMicrophoneMode() == EDigetherMicrophoneMode::Toggle)
	{
		bToggleOpen = !bToggleOpen;
	}
	bPushToTalkHeld = bHeld;
	ApplyMicrophoneMode();
}

void UDigetherVoiceComponent::ApplyMicrophoneMode()
{
	APlayerController* Controller = GetOwningController();
	if (!IsValid(Controller) || !Controller->IsLocalController())
	{
		return;
	}
	const UDigetherGameUserSettings* Settings = UDigetherGameUserSettings::Get();
	const EDigetherMicrophoneMode Mode = Settings != nullptr ? Settings->GetMicrophoneMode() : EDigetherMicrophoneMode::AlwaysOn;
	const bool bSpeaking = DigetherMicrophone::ShouldTransmit(bServerVoiceEnabled, Mode, bPushToTalkHeld, bToggleOpen);
	if (bHasAppliedSpeaking && bAppliedSpeaking == bSpeaking)
	{
		return;
	}
	Controller->ToggleSpeaking(bSpeaking);
	bHasAppliedSpeaking = true;
	bAppliedSpeaking = bSpeaking;
	UE_LOG(LogDigetherVoice, Log, TEXT("마이크 송신 %s (모드 %s)"), bSpeaking ? TEXT("켬") : TEXT("끔"),
		Mode == EDigetherMicrophoneMode::PushToTalk ? TEXT("누르고 말하기") : Mode == EDigetherMicrophoneMode::Toggle ? TEXT("토글") : TEXT("항상 켜짐"));
}

void UDigetherVoiceComponent::Refresh()
{
	TryBindInput();
	// 설정 화면 변경을 주기적으로 반영
	ApplyMicrophoneMode();
	UpdateTalkers();
	UpdateListener();
#if !UE_BUILD_SHIPPING
	UpdateProbe();
#endif
}

void UDigetherVoiceComponent::TryBindInput()
{
	APlayerController* Controller = GetOwningController();
	if (bInputBound || !IsValid(Controller))
	{
		return;
	}
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(Controller->InputComponent);
	if (Input == nullptr)
	{
		return;
	}
	const UInputAction* Action = PushToTalkAction;
	UInputMappingContext* Context = VoiceMappingContext;
	if (Action == nullptr)
	{
		const FKey FallbackKey = UDigetherOptionsSettings::Get()->FallbackPushToTalkKey;
		if (!FallbackKey.IsValid())
		{
			return;
		}
		// 키 재설정 저장은 에셋 경로 기준이라 실행 중 생성 액션은 목록에 안 올림
		FallbackPushToTalkAction = NewObject<UInputAction>(this, TEXT("FallbackPushToTalk"), RF_Transient);
		FallbackPushToTalkAction->ValueType = EInputActionValueType::Boolean;
		FallbackMappingContext = NewObject<UInputMappingContext>(this, TEXT("FallbackVoiceContext"), RF_Transient);
		FallbackMappingContext->MapKey(FallbackPushToTalkAction, FallbackKey);
		Action = FallbackPushToTalkAction;
		Context = FallbackMappingContext;
	}
	Input->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::HandlePushToTalkStarted);
	Input->BindAction(Action, ETriggerEvent::Completed, this, &ThisClass::HandlePushToTalkReleased);
	Input->BindAction(Action, ETriggerEvent::Canceled, this, &ThisClass::HandlePushToTalkReleased);
	if (Context != nullptr)
	{
		if (const ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Subsystem->AddMappingContext(Context, 1);
				AppliedMappingContext = Context;
			}
		}
	}
	bInputBound = true;
}

void UDigetherVoiceComponent::HandlePushToTalkStarted(const FInputActionValue& Value)
{
	SetPushToTalkHeld(true);
}

void UDigetherVoiceComponent::HandlePushToTalkReleased(const FInputActionValue& Value)
{
	SetPushToTalkHeld(false);
}

void UDigetherVoiceComponent::UpdateTalkers()
{
	const APlayerController* Controller = GetOwningController();
	const AGameStateBase* GameState = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	if (!IsValid(Controller) || !IsValid(GameState))
	{
		return;
	}
	const ADigetherGameplayGameState* GameplayState = Cast<ADigetherGameplayGameState>(GameState);
	const UDigetherGameOverPublicStateComponent* GameOver = GameplayState != nullptr
		? GameplayState->GetGameOverPublicStateComponent() : nullptr;
	const bool bAllOpen = GameOver != nullptr && GameOver->GetPhase() == EDigetherGameOverPhase::Credits;
	for (auto It = Talkers.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !It.Value().IsValid())
		{
			It.RemoveCurrent();
		}
	}
	for (APlayerState* State : GameState->PlayerArray)
	{
		if (!IsValid(State) || State == Controller->PlayerState || !State->GetUniqueId().IsValid())
		{
			continue;
		}
		TWeakObjectPtr<UDigetherVoiceTalker>& TalkerRef = Talkers.FindOrAdd(State);
		UDigetherVoiceTalker* Talker = TalkerRef.Get();
		if (Talker == nullptr)
		{
			Talker = NewObject<UDigetherVoiceTalker>(State, NAME_None, RF_Transient);
			Talker->RegisterComponent();
			Talker->RegisterWithPlayerState(State);
			TalkerRef = Talker;
		}
		// 관전자·크레딧은 비공간 음성, 생존자는 서버 매몰 투영값만 사용
		const APawn* Pawn = State->GetPawn();
		USceneComponent* Source = !bAllOpen && !IsSpectatorTalker(State) && IsValid(Pawn) ? Pawn->GetRootComponent() : nullptr;
		const ADigetherGameplayPlayerState* GameplayPlayer = Cast<ADigetherGameplayPlayerState>(State);
		const bool bBuried = Source != nullptr && GameplayPlayer != nullptr && GameplayPlayer->IsBuriedForVoice();
		USoundAttenuation* Attenuation = Source == nullptr ? UnspatializedAttenuation.Get()
			: bBuried ? BuriedAttenuation.Get() : SurvivorAttenuation.Get();
		Talker->SetPlaybackEnvironment(Source, Attenuation, bBuried, UDigetherOptionsSettings::Get()->BuriedVoiceLowPassHz);
		// 설정 화면 목소리 볼륨 변경도 이 주기에 반영
		ApplyPlaybackVolume(State);
	}
}

FString UDigetherVoiceComponent::PlayerKey(const APlayerState* Player)
{
	return IsValid(Player) && Player->GetUniqueId().IsValid() ? Player->GetUniqueId().ToString() : FString();
}

TArray<APlayerState*> UDigetherVoiceComponent::GetRemoteVoicePlayers() const
{
	TArray<APlayerState*> Players;
	const APlayerController* Controller = GetOwningController();
	const AGameStateBase* GameState = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	if (!IsValid(Controller) || !IsValid(GameState))
	{
		return Players;
	}
	for (APlayerState* State : GameState->PlayerArray)
	{
		if (IsValid(State) && State != Controller->PlayerState && !PlayerKey(State).IsEmpty())
		{
			Players.Add(State);
		}
	}
	return Players;
}

void UDigetherVoiceComponent::SetPlayerVoiceVolume(APlayerState* Player, const float Volume)
{
	const FString Key = PlayerKey(Player);
	if (Key.IsEmpty())
	{
		return;
	}
	PlayerVolumes.Add(Key, FMath::Clamp(Volume, 0.f, DigetherVoiceDefaults::MaxPlayerVolume));
	ApplyPlaybackVolume(Player);
}

float UDigetherVoiceComponent::GetPlayerVoiceVolume(const APlayerState* Player) const
{
	const float* Volume = PlayerVolumes.Find(PlayerKey(Player));
	return Volume != nullptr ? *Volume : 1.f;
}

void UDigetherVoiceComponent::SetPlayerVoiceMuted(APlayerState* Player, const bool bMuted)
{
	const FString Key = PlayerKey(Player);
	if (Key.IsEmpty())
	{
		return;
	}
	if (bMuted)
	{
		LocallyMutedPlayers.Add(Key);
	}
	else
	{
		LocallyMutedPlayers.Remove(Key);
	}
	ApplyPlaybackVolume(Player);
}

bool UDigetherVoiceComponent::IsPlayerVoiceMuted(const APlayerState* Player) const
{
	return LocallyMutedPlayers.Contains(PlayerKey(Player));
}

void UDigetherVoiceComponent::ApplyPlaybackVolume(const APlayerState* Player) const
{
	const TWeakObjectPtr<UDigetherVoiceTalker>* TalkerRef = Talkers.Find(const_cast<APlayerState*>(Player));
	UDigetherVoiceTalker* Talker = TalkerRef != nullptr ? TalkerRef->Get() : nullptr;
	if (Talker == nullptr)
	{
		return;
	}
	const UDigetherGameUserSettings* Settings = UDigetherGameUserSettings::Get();
	Talker->SetPlaybackVolume(DigetherVoiceDefaults::PlaybackVolume(Settings != nullptr ? Settings->GetVoiceVolume() : 1.f,
		GetPlayerVoiceVolume(Player), IsPlayerVoiceMuted(Player)));
}

void UDigetherVoiceComponent::UpdateListener()
{
	APlayerController* Controller = GetOwningController();
	if (!IsValid(Controller))
	{
		return;
	}
	// 관전 중이면 거리는 관전 대상 위치 기준, 방향은 카메라 기준
	const APawn* ViewPawn = Cast<APawn>(Controller->GetViewTarget());
	if (IsValid(ViewPawn) && ViewPawn != Controller->GetPawn() && ViewPawn->GetRootComponent() != nullptr)
	{
		Controller->SetAudioListenerAttenuationOverride(ViewPawn->GetRootComponent(), FVector::ZeroVector);
	}
	else
	{
		Controller->ClearAudioListenerAttenuationOverride();
	}
}

#if !UE_BUILD_SHIPPING
void UDigetherVoiceComponent::UpdateProbe()
{
	const APlayerController* Controller = GetOwningController();
	if (!CVarVoiceProbe.GetValueOnGameThread() || !IsValid(Controller) || !Controller->HasAuthority())
	{
		ProbeTalking.Reset();
		bProbeStarted = false;
		return;
	}
	const IOnlineVoicePtr Voice = Online::GetVoiceInterface(GetWorld());
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!Voice.IsValid() || !IsValid(GameState))
	{
		if (!bProbeStarted)
		{
			UE_LOG(LogDigetherVoice, Warning, TEXT("[VoiceProbe] 음성 인터페이스 없음(bHasVoiceEnabled·[Voice] bEnabled 확인)"));
			bProbeStarted = true;
		}
		return;
	}
	if (!bProbeStarted)
	{
		UE_LOG(LogDigetherVoice, Display, TEXT("[VoiceProbe] 시작\n%s"), *Voice->GetVoiceDebugState());
	}
	const int32 LocalUserNum = Controller->GetLocalPlayer() != nullptr ? Controller->GetLocalPlayer()->GetControllerId() : 0;
	for (const APlayerState* State : GameState->PlayerArray)
	{
		if (!IsValid(State) || State == Controller->PlayerState || !State->GetUniqueId().IsValid())
		{
			continue;
		}
		const FUniqueNetId& Id = *State->GetUniqueId();
		const bool bTalking = Voice->IsRemotePlayerTalking(Id);
		const float Amplitude = Voice->GetAmplitudeOfRemoteTalker(Id);
		const bool bMuted = Voice->IsMuted(LocalUserNum, Id);
		bool& bWasTalking = ProbeTalking.FindOrAdd(Id.ToString(), false);
		if (bTalking || bWasTalking != bTalking)
		{
			UE_LOG(LogDigetherVoice, Display, TEXT("[VoiceProbe] %s talking=%d amplitude=%.3f hostMuted=%d"),
				*State->GetPlayerName(), bTalking ? 1 : 0, Amplitude, bMuted ? 1 : 0);
		}
		bWasTalking = bTalking;
	}
	bProbeStarted = true;
}
#endif
