#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Tests/DigetherVoiceEnvironmentTestTypes.h"

#include "Components/AudioComponent.h"
#include "Components/SynthComponent.h"
#include "Engine/World.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Monster/DigetherZoneVolume.h"
#include "Game/Voice/DigetherVoiceTalker.h"
#include "Game/Voice/DigetherVoiceTypes.h"
#include "Misc/AutomationTest.h"
#include "OnlineSubsystemTypes.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundAttenuation.h"
#include "UObject/UnrealType.h"
#include "VoipListenerSynthComponent.h"

namespace
{
	class FDigetherVoiceEnvironmentTestWorld
	{
	public:
		FDigetherVoiceEnvironmentTestWorld()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		}

		~FDigetherVoiceEnvironmentTestWorld()
		{
			if (World != nullptr)
			{
				World->DestroyWorld(false);
			}
		}

		FDigetherVoiceEnvironmentTestWorld(const FDigetherVoiceEnvironmentTestWorld&) = delete;
		FDigetherVoiceEnvironmentTestWorld& operator=(const FDigetherVoiceEnvironmentTestWorld&) = delete;
		UWorld* Get() const { return World; }

	private:
		UWorld* World = nullptr;
	};

	USoundAttenuation* MakeVoiceTestAttenuation(const bool bSpatial, const bool bBuried)
	{
		USoundAttenuation* Attenuation = NewObject<USoundAttenuation>();
		Attenuation->Attenuation.bAttenuate = bSpatial;
		Attenuation->Attenuation.bSpatialize = bSpatial;
		Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		Attenuation->Attenuation.AttenuationShapeExtents = FVector(DigetherVoiceDefaults::RangeFullCm, 0.f, 0.f);
		Attenuation->Attenuation.FalloffDistance = DigetherVoiceDefaults::RangeMaxCm
			* (bBuried ? DigetherVoiceDefaults::BuriedRangeRatio : 1.f) - DigetherVoiceDefaults::RangeFullCm;
		return Attenuation;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherVoiceEnvironmentProjectionTest,
	"DIGETHER.Game.Voice.EnvironmentProjection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherVoiceEnvironmentProjectionTest::RunTest(const FString& Parameters)
{
	FDigetherVoiceEnvironmentTestWorld Fixture;
	UWorld* World = Fixture.Get();
	if (!TestNotNull(TEXT("음성 상태 시험 월드"), World))
	{
		return false;
	}
	ADigetherGameplayPlayerState* State = World->SpawnActor<ADigetherGameplayPlayerState>();
	ADigetherVoiceEnvironmentTestPawn* Pawn = World->SpawnActor<ADigetherVoiceEnvironmentTestPawn>();
	ADigetherGameplayPlayerState* Copied = World->SpawnActor<ADigetherGameplayPlayerState>();
	if (!TestNotNull(TEXT("PlayerState"), State) || !TestNotNull(TEXT("매몰 인터페이스 시험 Pawn"), Pawn)
		|| !TestNotNull(TEXT("월드 인계 대상"), Copied))
	{
		return false;
	}
	State->RetainPawn(Pawn);
	State->RefreshVoiceEnvironment();
	TestFalse(TEXT("기본 비매몰"), State->IsBuriedForVoice());
	Pawn->SetTestBuried(true);
	State->RefreshVoiceEnvironment();
	TestTrue(TEXT("서버는 현재 retained Pawn 원본에서 매몰 산출"), State->IsBuriedForVoice());

	State->SetRole(ROLE_SimulatedProxy);
	Pawn->SetTestBuried(false);
	State->RefreshVoiceEnvironment();
	TestTrue(TEXT("비권위 호출이 서버 복제 투영값을 덮지 않음"), State->IsBuriedForVoice());
	State->SetRole(ROLE_Authority);
	State->RefreshVoiceEnvironment();
	TestFalse(TEXT("권위 복구 후 현재 원본 재산출"), State->IsBuriedForVoice());

	Pawn->SetTestBuried(true);
	State->RefreshVoiceEnvironment();
	State->CopyProperties(Copied);
	TestFalse(TEXT("매몰 음성 투영은 월드 인계로 복사하지 않음"), Copied->IsBuriedForVoice());
	State->GetLifeComponent()->ApplyDeath(EDigetherDeathCause::Killed);
	State->RefreshVoiceEnvironment();
	TestFalse(TEXT("사망으로 매몰 필터 해제"), State->IsBuriedForVoice());
	State->GetLifeComponent()->Revive();
	State->RefreshVoiceEnvironment();
	TestTrue(TEXT("다시 활동하면 현재 원본 재산출"), State->IsBuriedForVoice());
	State->GetLifeComponent()->ApplyEscape(EDigetherEscapeKind::Emergency);
	State->RefreshVoiceEnvironment();
	TestFalse(TEXT("탈출 관전자에 매몰 효과 없음"), State->IsBuriedForVoice());
	State->GetLifeComponent()->Revive();
	State->RefreshVoiceEnvironment();
	State->SetIsInactive(true);
	State->OnDeactivated();
	TestFalse(TEXT("비활성 접속은 매몰 투영 해제"), State->IsBuriedForVoice());
	State->OnReactivated();
	TestTrue(TEXT("재접속 첫 복제 전에 현재 Pawn 매몰 재산출"), State->IsBuriedForVoice());
	State->RetainPawn(nullptr);
	State->RefreshVoiceEnvironment();
	TestFalse(TEXT("Pawn 없음은 매몰 아님"), State->IsBuriedForVoice());
	State->RetainPawn(World->SpawnActor<ADigetherTestPawn>());
	State->RefreshVoiceEnvironment();
	TestFalse(TEXT("매몰 인터페이스 없는 기존 Pawn 호환"), State->IsBuriedForVoice());

	State->GetClass()->SetUpRuntimeReplicationData();
	const FProperty* Property = FindFProperty<FProperty>(State->GetClass(), TEXT("bVoiceBuried"));
	bool bPublicReplication = false;
	TArray<FLifetimeProperty> Replication;
	State->GetLifetimeReplicatedProps(Replication);
	for (const FLifetimeProperty& Entry : Replication)
	{
		bPublicReplication |= Property != nullptr && Entry.RepIndex == Property->RepIndex && Entry.Condition == COND_None;
	}
	TestTrue(TEXT("음성 효과는 참가자가 재생할 공개 투영값"), bPublicReplication);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherVoiceEnvironmentZoneTest,
	"DIGETHER.Game.Voice.EnvironmentZone", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherVoiceEnvironmentZoneTest::RunTest(const FString& Parameters)
{
	FDigetherVoiceEnvironmentTestWorld Fixture;
	UWorld* World = Fixture.Get();
	if (!TestNotNull(TEXT("구역 시험 월드"), World))
	{
		return false;
	}
	const FVector DungeonCenter(10000.f, 0.f, 0.f);
	ADigetherZoneVolume* Zone = DigetherVoiceEnvironmentTest::CreateDungeon(World, DungeonCenter);
	if (!TestNotNull(TEXT("실제 brush collision 구역 볼륨"), Zone))
	{
		return false;
	}
	TestEqual(TEXT("물리 볼륨 내부는 던전"), ADigetherZoneVolume::GetZoneKindAt(World, DungeonCenter), EDigetherMonsterZone::Dungeon);
	TestTrue(TEXT("물리 볼륨 밖은 던전 아님"), ADigetherZoneVolume::GetZoneKindAt(World, FVector::ZeroVector) != EDigetherMonsterZone::Dungeon);
	TestEqual(TEXT("구역 경계 안쪽"), ADigetherZoneVolume::GetZoneKindAt(World, DungeonCenter + FVector(999.f, 0.f, 0.f)), EDigetherMonsterZone::Dungeon);
	TestTrue(TEXT("구역 경계 바깥쪽"), ADigetherZoneVolume::GetZoneKindAt(World, DungeonCenter + FVector(1001.f, 0.f, 0.f)) != EDigetherMonsterZone::Dungeon);
	Zone->Destroy();
	TestTrue(TEXT("볼륨 제거 뒤 오래된 구역 사용 안 함"), ADigetherZoneVolume::GetZoneKindAt(World, DungeonCenter) != EDigetherMonsterZone::Dungeon);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherVoicePlaybackEnvironmentTest,
	"DIGETHER.Game.Voice.PlaybackEnvironment", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherVoicePlaybackEnvironmentTest::RunTest(const FString& Parameters)
{
	// 음성 패킷 없이 엔진이 Talker에 넘기는 Audio/Synth 설정과 재시작 원본을 검사
	USoundAttenuation* Normal = MakeVoiceTestAttenuation(true, false);
	USoundAttenuation* Buried = MakeVoiceTestAttenuation(true, true);
	USoundAttenuation* Unspatialized = MakeVoiceTestAttenuation(false, false);
	UDigetherVoiceTalker* Talker = NewObject<UDigetherVoiceTalker>();
	UDigetherVoiceTalker* OtherTalker = NewObject<UDigetherVoiceTalker>();
	UVoipListenerSynthComponent* Synth = NewObject<UVoipListenerSynthComponent>();
	UAudioComponent* Audio = NewObject<UAudioComponent>(Synth);
	UAudioComponent* OtherAudio = NewObject<UAudioComponent>();
	USceneComponent* Source = NewObject<USceneComponent>();
	Talker->SetPlaybackEnvironment(Source, Normal, false, 1500.f);
	Talker->SetPlaybackVolume(0.4f);
	Talker->OnTalkingBegin(Audio);
	OtherTalker->SetPlaybackEnvironment(Source, Normal, false, 1500.f);
	OtherTalker->OnTalkingBegin(OtherAudio);
	TestFalse(TEXT("기본 발화 LPF 꺼짐"), Audio->bEnableLowPassFilter);
	TestEqual(TEXT("로컬 음량 보존"), Audio->VolumeMultiplier, 0.4f);
	Talker->SetPlaybackEnvironment(Source, Buried, true, 1500.f);
	TestTrue(TEXT("발화 중 매몰 LPF 즉시 켬"), Audio->bEnableLowPassFilter);
	TestEqual(TEXT("먹먹함 주파수 적용"), Audio->LowPassFilterFrequency, 1500.f);
	TestEqual(TEXT("매몰 원음 구간 유지"), Audio->AttenuationOverrides.AttenuationShapeExtents.X, 300.0);
	TestEqual(TEXT("매몰 최대 거리 절반"), Audio->AttenuationOverrides.FalloffDistance, 950.f);
	TestTrue(TEXT("Synth 재시작도 현재 매몰 감쇠 사용"), Synth->bOverrideAttenuation);
	TestEqual(TEXT("Synth 재시작용 감쇠 일치"), Synth->AttenuationOverrides.FalloffDistance, 950.f);
	TestEqual(TEXT("실제 재생 Audio의 부착 위치 갱신"), Audio->GetAttachParent(), Source);
	TestFalse(TEXT("다른 발화자 필터 오염 없음"), OtherAudio->bEnableLowPassFilter);
	TestEqual(TEXT("공용 정상 감쇠 원본 유지"), Normal->Attenuation.FalloffDistance, 2200.f);
	TestEqual(TEXT("다른 발화자 거리는 정상 유지"), OtherAudio->AttenuationOverrides.FalloffDistance, 2200.f);

	Talker->SetPlaybackEnvironment(nullptr, Unspatialized, false, 1500.f);
	TestFalse(TEXT("관전/크레딧 전환으로 필터 해제"), Audio->bEnableLowPassFilter);
	TestFalse(TEXT("관전/크레딧 거리 감쇠 없음"), Audio->AttenuationOverrides.bAttenuate);
	TestFalse(TEXT("관전/크레딧 공간화 없음"), Audio->AttenuationOverrides.bSpatialize);
	TestNull(TEXT("비공간 전환 뒤 실제 재생 Audio 부착 해제"), Audio->GetAttachParent());
	TestNotNull(TEXT("비공간도 다음 발화에 감쇠 객체 유지"), Talker->Settings.AttenuationSettings.Get());
	Talker->SetPlaybackEnvironment(Source, Normal, false, 1500.f);
	TestTrue(TEXT("같은 발화에서 공간 음성 복귀"), Audio->AttenuationOverrides.bSpatialize);
	TestEqual(TEXT("정상 최대 거리 복구"), Audio->AttenuationOverrides.FalloffDistance, 2200.f);

	Talker->SetPlaybackEnvironment(Source, Buried, true, 2500.f);
	Talker->OnTalkingEnd();
	UAudioComponent* NextAudio = NewObject<UAudioComponent>();
	Talker->OnTalkingBegin(NextAudio);
	TestTrue(TEXT("다음 발화에도 현재 매몰 효과"), NextAudio->bEnableLowPassFilter);
	TestEqual(TEXT("다음 발화에도 조정한 주파수"), NextAudio->LowPassFilterFrequency, 2500.f);
	TestEqual(TEXT("다음 발화에도 개인 음량"), NextAudio->VolumeMultiplier, 0.4f);
	Talker->SetPlaybackEnvironment(Source, Normal, false, 1500.f);
	TestFalse(TEXT("매몰 해제 후 다음 오디오에서 LPF 해제"), NextAudio->bEnableLowPassFilter);
	Talker->OnTalkingEnd();
	OtherTalker->OnTalkingEnd();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherVoiceListenHostMuteReasonsTest,
	"DIGETHER.Game.Voice.ListenHostMuteReasons", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherVoiceListenHostMuteReasonsTest::RunTest(const FString& Parameters)
{
	FDigetherVoiceEnvironmentTestWorld Fixture;
	UWorld* World = Fixture.Get();
	if (!TestNotNull(TEXT("음소거 회귀 시험 월드"), World))
	{
		return false;
	}
	ADigetherGameplayPlayerController* Host = World->SpawnActor<ADigetherGameplayPlayerController>();
	if (!TestNotNull(TEXT("실제 게임 PlayerController"), Host))
	{
		return false;
	}
	Host->SetAsLocalPlayerController();
	TestTrue(TEXT("동일 MuteList를 쓰는 로컬 권위 경로"), Host->HasAuthority() && Host->IsLocalController());
	const FUniqueNetIdRepl TalkerId(FUniqueNetIdRef(FUniqueNetIdString::Create(
		TEXT("fixture-voice-mute-reasons"), FName(TEXT("DIGETHER_TEST")))));
	const FUniqueNetIdPtr Id = TalkerId.GetUniqueNetId();
	const auto Reasons = [&]()
	{
		const EVoiceBlockReasons* Value = Host->MuteList.VoicePacketFilterMap.Find(Id.ToSharedRef());
		return Value != nullptr ? *Value : EVoiceBlockReasons::None;
	};

	Host->GameplayMutePlayer(TalkerId);
	TestEqual(TEXT("게임 차단만 요청하면 generic Muted 추가 없음"), Reasons(), EVoiceBlockReasons::Gameplay);
	TestTrue(TEXT("게임 차단은 실제 엔진 mute 판정에 반영"), Host->IsPlayerMuted(*Id));
	Host->GameplayUnmutePlayer(TalkerId);
	TestEqual(TEXT("구역 복귀 후 차단 사유 없음"), Reasons(), EVoiceBlockReasons::None);
	TestFalse(TEXT("구역 복귀 후 리슨 호스트도 해제"), Host->IsPlayerMuted(*Id));

	Host->ServerMutePlayer_Implementation(TalkerId);
	Host->GameplayMutePlayer(TalkerId);
	TestEqual(TEXT("사용자 음소거와 게임 차단 중첩"), Reasons(), EVoiceBlockReasons::Muted | EVoiceBlockReasons::Gameplay);
	Host->GameplayUnmutePlayer(TalkerId);
	TestEqual(TEXT("게임 차단 해제는 사용자 음소거 보존"), Reasons(), EVoiceBlockReasons::Muted);
	TestTrue(TEXT("사용자 음소거 동안 실제 엔진 판정 유지"), Host->IsPlayerMuted(*Id));
	Host->ServerUnmutePlayer_Implementation(TalkerId);
	TestEqual(TEXT("이후 사용자가 해제하면 잔류 사유 없음"), Reasons(), EVoiceBlockReasons::None);
	TestFalse(TEXT("이후 사용자의 음소거 해제 정상"), Host->IsPlayerMuted(*Id));

	Host->ServerBlockPlayer_Implementation(TalkerId);
	Host->GameplayMutePlayer(TalkerId);
	Host->GameplayUnmutePlayer(TalkerId);
	TestEqual(TEXT("게임 차단 해제는 기존 Blocked 보존"), Reasons(), EVoiceBlockReasons::Blocked);
	TestTrue(TEXT("사용자 차단은 실제 mute 판정 유지"), Host->IsPlayerMuted(*Id));
	Host->ServerUnblockPlayer_Implementation(TalkerId);
	TestEqual(TEXT("이후 사용자 차단 해제 정상"), Reasons(), EVoiceBlockReasons::None);
	TestFalse(TEXT("차단 해제 후 실제 mute 판정도 없음"), Host->IsPlayerMuted(*Id));

	Host->SetRole(ROLE_SimulatedProxy);
	Host->ClientMutePlayer_Implementation(TalkerId);
	TestEqual(TEXT("비권위 클라이언트는 엔진의 generic Muted 수신 경로 유지"), Reasons(), EVoiceBlockReasons::Muted);
	Host->ClientUnmutePlayer_Implementation(TalkerId);
	TestEqual(TEXT("비권위 클라이언트의 기존 해제 경로 유지"), Reasons(), EVoiceBlockReasons::None);
	Host->SetRole(ROLE_Authority);
	return true;
}

#endif
