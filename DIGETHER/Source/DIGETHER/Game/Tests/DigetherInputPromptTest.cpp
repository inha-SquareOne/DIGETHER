#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Input/DigetherInputPrompt.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherInputPromptKeyTest,
	"DIGETHER.Game.Input.PromptKey", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherInputPromptKeyTest::RunTest(const FString& Parameters)
{
	using namespace DigetherInputPrompt;

	const TArray<FKey> Remapped = { EKeys::F, EKeys::E };
	TestEqual(TEXT("매핑 순서의 첫 키(재설정된 키)"), ChooseDisplayKey(Remapped), EKeys::F);

	const TArray<FKey> PadFirst = { EKeys::Gamepad_FaceButton_Bottom, EKeys::RightMouseButton };
	TestEqual(TEXT("G0: 패드 키 건너뛰고 키보드·마우스 키"), ChooseDisplayKey(PadFirst), EKeys::RightMouseButton);

	const TArray<FKey> PadOnly = { EKeys::Gamepad_FaceButton_Bottom, EKeys::Invalid };
	TestEqual(TEXT("표시할 키 없음"), ChooseDisplayKey(PadOnly), EKeys::Invalid);
	TestEqual(TEXT("빈 목록"), ChooseDisplayKey(TArray<FKey>()), EKeys::Invalid);

	TestTrue(TEXT("키 재설정 화면과 같은 짧은 이름"), MakeKeyText(EKeys::E).EqualTo(EKeys::E.GetDisplayName(false)));
	TestFalse(TEXT("빈 키도 빈 문구가 아님"), MakeKeyText(EKeys::Invalid).IsEmpty());
	TestTrue(TEXT("컨트롤러·액션 없으면 미지정 문구"),
		GetActionKeyText(nullptr, nullptr).EqualTo(MakeKeyText(EKeys::Invalid)));
	return true;
}

#endif
