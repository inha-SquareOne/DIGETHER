#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Tests/DigetherVoiceEnvironmentTestTypes.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Game/Framework/DigetherGameplayGameState.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Run/DigetherGameOverPublicStateComponent.h"
#include "Game/Voice/DigetherVoiceComponent.h"
#include "Game/Voice/DigetherVoiceTalker.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "OnlineSubsystemTypes.h"
#include "Sound/SoundAttenuation.h"
#include "TimerManager.h"

namespace
{
	// 실제 VoiceComponent 타이머가 선택한 프로필을 관찰, 음성 패킷/마이크 없이 소비 경로만 검사
	class FDigetherVoiceConsumerCommand : public IAutomationLatentCommand
	{
	public:
		explicit FDigetherVoiceConsumerCommand(FAutomationTestBase& InTest) : Test(InTest)
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			State = World->SpawnActor<ADigetherGameplayGameState>();
			World->SetGameState(State);
			Controller = World->SpawnActor<APlayerController>();
			Controller->SetAsLocalPlayerController();
			Remote = World->SpawnActor<ADigetherGameplayPlayerState>();
			Remote->SetUniqueId(FUniqueNetIdRepl(FUniqueNetIdRef(FUniqueNetIdString::Create(
				FGuid::NewGuid().ToString(), FName(TEXT("DIGETHER_TEST"))))));
			Pawn = World->SpawnActor<ADigetherVoiceEnvironmentTestPawn>();
			Pawn->SetPlayerState(Remote);
			Remote->RetainPawn(Pawn);
			State->PlayerArray.AddUnique(Remote);
			Voice = NewObject<UDigetherVoiceComponent>(Controller);
			Voice->RegisterComponent();
			UActorComponent* Component = Voice;
			Component->InitializeComponent();
			Component->BeginPlay();
			Talker = Remote->FindComponentByClass<UDigetherVoiceTalker>();
			if (Talker != nullptr)
			{
				Audio = NewObject<UAudioComponent>(Controller);
				Audio->RegisterComponent();
				Talker->OnTalkingBegin(Audio);
			}
		}

		~FDigetherVoiceConsumerCommand()
		{
			if (Talker != nullptr)
			{
				Talker->OnTalkingEnd();
			}
			if (Voice != nullptr)
			{
				UActorComponent* Component = Voice;
				Component->EndPlay(EEndPlayReason::Destroyed);
			}
			if (World != nullptr)
			{
				World->DestroyWorld(false);
			}
		}

		virtual bool Update() override
		{
			if (!Test.TestNotNull(TEXT("실제 VoiceComponent가 remote PlayerState에 만든 Talker"), Talker)
				|| !Test.TestNotNull(TEXT("프로필 관찰용 오디오"), Audio))
			{
				return true;
			}
			// TimerManager는 프레임당 1회만 갱신, 엔진 전역 시각/프레임을 시험이 덮지 않음
			if (LastFrame == GFrameCounter)
			{
				return false;
			}
			LastFrame = GFrameCounter;
			World->GetTimerManager().Tick(0.3f);
			switch (Step++)
			{
			case 0:
				Test.TestTrue(TEXT("기본 소비자는 원격 Pawn에 공간 음성 부착"), Talker->Settings.ComponentToAttachTo == Pawn->GetRootComponent());
				Test.TestEqual(TEXT("기본 소비자의 최대거리25m"), Audio->AttenuationOverrides.FalloffDistance, 2200.f);
				Pawn->SetTestBuried(true);
				Remote->RefreshVoiceEnvironment();
				break;
			case 1:
				Test.TestTrue(TEXT("실제 소비자가 서버 매몰 투영을 LPF로 연결"), Audio->bEnableLowPassFilter);
				Test.TestEqual(TEXT("실제 소비자가 최대거리12.5m 프로필 선택"), Audio->AttenuationOverrides.FalloffDistance, 950.f);
				Remote->GetLifeComponent()->ApplyDeath(EDigetherDeathCause::Killed);
				Remote->RefreshVoiceEnvironment();
				break;
			case 2:
				Test.TestFalse(TEXT("실제 소비자가 관전자 음성을 비공간으로 전환"), Audio->AttenuationOverrides.bAttenuate);
				Test.TestFalse(TEXT("실제 소비자가 관전자 매몰 필터 해제"), Audio->bEnableLowPassFilter);
				Remote->GetLifeComponent()->Revive();
				Remote->RefreshVoiceEnvironment();
				{
					FDigetherGameOverPublicState Credits;
					Credits.Phase = EDigetherGameOverPhase::Credits;
					State->GetGameOverPublicStateComponent()->SetPublicState(Credits);
				}
				break;
			case 3:
				Test.TestFalse(TEXT("Credits 공개상태가 살아 있는 매몰 발화자도 비공간으로 전환"), Audio->AttenuationOverrides.bAttenuate);
				Test.TestFalse(TEXT("Credits에서는 매몰 원본이 참이어도 LPF 해제"), Audio->bEnableLowPassFilter);
				State->GetGameOverPublicStateComponent()->SetPublicState(FDigetherGameOverPublicState());
				Pawn->SetTestBuried(false);
				Remote->RefreshVoiceEnvironment();
				break;
			default:
				Test.TestTrue(TEXT("Credits 종료 후 실제 소비자 정상 공간 음성 복귀"), Audio->AttenuationOverrides.bSpatialize);
				Test.TestEqual(TEXT("정상 거리 복구"), Audio->AttenuationOverrides.FalloffDistance, 2200.f);
				Test.TestFalse(TEXT("정상 복귀 뒤 필터 잔류 없음"), Audio->bEnableLowPassFilter);
				return true;
			}
			return false;
		}

	private:
		FAutomationTestBase& Test;
		UWorld* World = nullptr;
		ADigetherGameplayGameState* State = nullptr;
		APlayerController* Controller = nullptr;
		ADigetherGameplayPlayerState* Remote = nullptr;
		ADigetherVoiceEnvironmentTestPawn* Pawn = nullptr;
		UDigetherVoiceComponent* Voice = nullptr;
		UDigetherVoiceTalker* Talker = nullptr;
		UAudioComponent* Audio = nullptr;
		uint64 LastFrame = MAX_uint64;
		int32 Step = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherVoiceConsumerTest,
	"DIGETHER.Game.Voice.ConsumerProfiles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherVoiceConsumerTest::RunTest(const FString& Parameters)
{
	AddCommand(new FDigetherVoiceConsumerCommand(*this));
	return true;
}

#endif
