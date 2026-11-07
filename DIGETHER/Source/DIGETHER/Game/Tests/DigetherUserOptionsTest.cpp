#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Settings/DigetherGameUserSettings.h"
#include "Game/Settings/DigetherKeyBindings.h"
#include "Game/Settings/DigetherOptionsSettings.h"
#include "Game/Voice/DigetherVoiceTypes.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherKeyBindingSwapTest,
	"DIGETHER.Game.Settings.KeyBindingSwap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherKeyBindingSwapTest::RunTest(const FString& Parameters)
{
	TMap<FName, FKey> Keys = { { TEXT("Jump"), EKeys::SpaceBar }, { TEXT("Interact"), EKeys::E }, { TEXT("Talk"), EKeys::V } };

	DigetherKeyBindings::AssignWithSwap(Keys, TEXT("Interact"), EKeys::F);
	TestEqual(TEXT("빈 키로 바꾸면 그 줄만 변경"), Keys[TEXT("Interact")], EKeys::F);
	TestEqual(TEXT("다른 줄 유지"), Keys[TEXT("Talk")], EKeys::V);

	DigetherKeyBindings::AssignWithSwap(Keys, TEXT("Talk"), EKeys::SpaceBar);
	TestEqual(TEXT("겹친 키는 대상 줄로 이동"), Keys[TEXT("Talk")], EKeys::SpaceBar);
	TestEqual(TEXT("원래 쓰던 줄은 바꾸기 전 키로 교환"), Keys[TEXT("Jump")], EKeys::V);

	const TMap<FName, FKey> Before = Keys;
	DigetherKeyBindings::AssignWithSwap(Keys, TEXT("Missing"), EKeys::Q);
	DigetherKeyBindings::AssignWithSwap(Keys, TEXT("Jump"), EKeys::V);
	TestTrue(TEXT("없는 줄·같은 키는 변화 없음"), Keys.OrderIndependentCompareEqual(Before));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherKeyBindingFilterTest,
	"DIGETHER.Game.Settings.KeyBindingFilter", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherKeyBindingFilterTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("키보드 키"), DigetherKeyBindings::IsBindableKey(EKeys::V));
	TestTrue(TEXT("마우스 버튼"), DigetherKeyBindings::IsBindableKey(EKeys::ThumbMouseButton));
	TestFalse(TEXT("Esc는 취소 전용"), DigetherKeyBindings::IsBindableKey(EKeys::Escape));
	TestFalse(TEXT("게임패드 제외(답변 14: MVP 키보드·마우스)"), DigetherKeyBindings::IsBindableKey(EKeys::Gamepad_FaceButton_Bottom));
	TestFalse(TEXT("마우스 축 제외"), DigetherKeyBindings::IsBindableKey(EKeys::MouseX));
	TestFalse(TEXT("빈 키"), DigetherKeyBindings::IsBindableKey(EKeys::Invalid));
	TestEqual(TEXT("누르고 말하기 임시 키 V"), GetDefault<UDigetherOptionsSettings>()->FallbackPushToTalkKey, EKeys::V);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherMicrophoneModeTest,
	"DIGETHER.Game.Voice.MicrophoneMode", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherMicrophoneModeTest::RunTest(const FString& Parameters)
{
	using DigetherMicrophone::ShouldTransmit;
	TestTrue(TEXT("항상 켜짐"), ShouldTransmit(true, EDigetherMicrophoneMode::AlwaysOn, false, false));
	TestFalse(TEXT("누르고 말하기: 뗌"), ShouldTransmit(true, EDigetherMicrophoneMode::PushToTalk, false, true));
	TestTrue(TEXT("누르고 말하기: 누름"), ShouldTransmit(true, EDigetherMicrophoneMode::PushToTalk, true, false));
	TestTrue(TEXT("토글: 켜짐은 키를 떼도 유지"), ShouldTransmit(true, EDigetherMicrophoneMode::Toggle, false, true));
	TestFalse(TEXT("토글: 꺼짐은 키를 누르고 있어도 끔"), ShouldTransmit(true, EDigetherMicrophoneMode::Toggle, true, false));
	TestFalse(TEXT("서버가 음성을 끄면 모든 모드 끔"), ShouldTransmit(false, EDigetherMicrophoneMode::AlwaysOn, true, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherVoicePlaybackVolumeTest,
	"DIGETHER.Game.Voice.PlaybackVolume", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherVoicePlaybackVolumeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("전체 × 플레이어별"), DigetherVoiceDefaults::PlaybackVolume(0.5f, 2.f, false), 1.f);
	TestEqual(TEXT("나만 음소거는 0"), DigetherVoiceDefaults::PlaybackVolume(1.f, 1.f, true), 0.f);
	TestEqual(TEXT("플레이어별 최대 배율 제한"), DigetherVoiceDefaults::PlaybackVolume(1.f, 5.f, false), DigetherVoiceDefaults::MaxPlayerVolume);
	TestEqual(TEXT("음수 없음"), DigetherVoiceDefaults::PlaybackVolume(-1.f, 1.f, false), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherGameUserSettingsRangeTest,
	"DIGETHER.Game.Settings.UserSettingsRange", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherGameUserSettingsRangeTest::RunTest(const FString& Parameters)
{
	UDigetherGameUserSettings* Settings = NewObject<UDigetherGameUserSettings>();
	Settings->SetFieldOfView(200.f);
	TestEqual(TEXT("시야각 상한"), Settings->GetFieldOfView(), UDigetherGameUserSettings::MaxFieldOfView);
	Settings->SetDisplayGamma(0.f);
	TestEqual(TEXT("밝기 하한"), Settings->GetDisplayGamma(), UDigetherGameUserSettings::MinDisplayGamma);
	Settings->SetVoiceActivationThreshold(1.f);
	TestEqual(TEXT("감지 문턱 상한"), Settings->GetVoiceActivationThreshold(), UDigetherGameUserSettings::MaxVoiceActivationThreshold);
	Settings->SetMicInputGain(0.f);
	TestEqual(TEXT("마이크 증폭 하한"), Settings->GetMicInputGain(), UDigetherGameUserSettings::MinMicInputGain);
	Settings->SetVoiceVolume(2.f);
	Settings->SetMotionBlurEnabled(false);

	Settings->SetToDefaults();
	TestEqual(TEXT("기본 시야각"), Settings->GetFieldOfView(), UDigetherGameUserSettings::DefaultFieldOfView);
	TestEqual(TEXT("기본 밝기"), Settings->GetDisplayGamma(), UDigetherGameUserSettings::DefaultDisplayGamma);
	TestEqual(TEXT("기본 감지 문턱(엔진 기본)"), Settings->GetVoiceActivationThreshold(), UDigetherGameUserSettings::DefaultVoiceActivationThreshold);
	TestEqual(TEXT("기본 목소리 볼륨"), Settings->GetVoiceVolume(), 1.f);
	TestTrue(TEXT("기본 모션 블러 켬"), Settings->IsMotionBlurEnabled());
	return true;
}

#endif
