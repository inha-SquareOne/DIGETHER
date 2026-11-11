#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Monster/DigetherMonsterCameraSampleComponent.h"
#include "Game/Monster/DigetherMonsterSenseRules.h"
#include "Game/Tests/DigetherTestValues.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherMonsterSenseRulesTest,
	"DIGETHER.Game.Monster.SenseRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherMonsterSenseRulesTest::RunTest(const FString& Parameters)
{
	using namespace DigetherMonsterSenseRules;

	// 시야 원뿔: 120°(반각 60), 20m 픽스처
	const FVector Origin = FVector::ZeroVector;
	const FVector Forward = FVector::ForwardVector;
	TestTrue(TEXT("정면 안"), IsInViewCone(Origin, Forward, FVector(1000.f, 0.f, 0.f), 60.f, 2000.f));
	TestTrue(TEXT("반각 경계 안"), IsInViewCone(Origin, Forward, FVector(1000.f, 1000.f * FMath::Tan(FMath::DegreesToRadians(59.f)), 0.f), 60.f, 2000.f));
	TestFalse(TEXT("반각 밖"), IsInViewCone(Origin, Forward, FVector(1000.f, 1000.f * FMath::Tan(FMath::DegreesToRadians(61.f)), 0.f), 60.f, 2000.f));
	TestFalse(TEXT("뒤"), IsInViewCone(Origin, Forward, FVector(-100.f, 0.f, 0.f), 60.f, 2000.f));
	TestFalse(TEXT("거리 밖"), IsInViewCone(Origin, Forward, FVector(2001.f, 0.f, 0.f), 60.f, 2000.f));
	TestFalse(TEXT("시야 0"), IsInViewCone(Origin, Forward, FVector(100.f, 0.f, 0.f), 60.f, 0.f));

	// 카메라 표본: 수평 90°, 수직 60°
	FDigetherCameraSample Sample;
	Sample.Location = FVector::ZeroVector;
	Sample.Rotation = FRotator::ZeroRotator;
	Sample.HorizontalFov = 90.f;
	Sample.VerticalFov = 60.f;
	TestTrue(TEXT("화면 중앙"), IsPointOnScreen(Sample, FVector(1000.f, 0.f, 0.f), 2000.f));
	TestTrue(TEXT("수평 44° 안"), IsPointOnScreen(Sample, FVector(1000.f, 1000.f * FMath::Tan(FMath::DegreesToRadians(44.f)), 0.f), 2000.f));
	TestFalse(TEXT("수평 46° 밖"), IsPointOnScreen(Sample, FVector(1000.f, 1000.f * FMath::Tan(FMath::DegreesToRadians(46.f)), 0.f), 2000.f));
	TestFalse(TEXT("수직 31° 밖"), IsPointOnScreen(Sample, FVector(1000.f, 0.f, 1000.f * FMath::Tan(FMath::DegreesToRadians(31.f))), 2000.f));
	TestFalse(TEXT("뒤"), IsPointOnScreen(Sample, FVector(-1000.f, 0.f, 0.f), 2000.f));
	TestFalse(TEXT("시선 거리 밖(M4: 20m)"), IsPointOnScreen(Sample, FVector(2001.f, 0.f, 0.f), 2000.f));
	FDigetherCameraSample Turned = Sample;
	Turned.Rotation = FRotator(0.f, 90.f, 0.f);
	TestTrue(TEXT("제자리 회전 반영"), IsPointOnScreen(Turned, FVector(0.f, 1000.f, 0.f), 2000.f));
	TestFalse(TEXT("회전 후 이전 정면은 밖"), IsPointOnScreen(Turned, FVector(1000.f, 0.f, 0.f), 2000.f));

	TestEqual(TEXT("중심 각도 0"), AngleFromViewCenter(Sample, FVector(1000.f, 0.f, 0.f)), 0.f, 1e-3f);
	TestEqual(TEXT("중심 각도 15"), AngleFromViewCenter(Sample, FVector(1000.f, 1000.f * FMath::Tan(FMath::DegreesToRadians(15.f)), 0.f)), 15.f, 1e-2f);
	TestEqual(TEXT("같은 위치는 180"), AngleFromViewCenter(Sample, FVector::ZeroVector), 180.f);

	// 유효 시야각: 90° 수평 유지, 16:9 → 수직 약 58.72°
	float H = 0.f;
	float V = 0.f;
	ComputeEffectiveFov(90.f, 16.f / 9.f, true, H, V);
	TestEqual(TEXT("수평 유지"), H, 90.f, 1e-3f);
	TestEqual(TEXT("수직 계산"), V, FMath::RadiansToDegrees(2.f * FMath::Atan(1.f / (16.f / 9.f))), 1e-3f);
	ComputeEffectiveFov(60.f, 2.f, false, H, V);
	TestEqual(TEXT("수직 유지"), V, 60.f, 1e-3f);
	TestTrue(TEXT("수직 유지 시 수평이 더 넓음"), H > 60.f);

	// 표본 검증
	FDigetherCameraSample Bad = Sample;
	Bad.HorizontalFov = DigetherTestValues::NaN();
	TestFalse(TEXT("비유한 시야각 보정"), SanitizeSample(Bad, 60.f, 110.f, 3.6f, 90.f, 16.f / 9.f));
	TestEqual(TEXT("기본 시야각으로 대체"), Bad.HorizontalFov, 90.f, 1e-3f);
	FDigetherCameraSample TooWide = Sample;
	TooWide.HorizontalFov = 179.f;
	TestFalse(TEXT("과대 시야각 보정"), SanitizeSample(TooWide, 60.f, 110.f, 3.6f, 90.f, 16.f / 9.f));
	FDigetherCameraSample Good = Sample;
	TestTrue(TEXT("정상 표본 유지"), SanitizeSample(Good, 60.f, 110.f, 3.6f, 90.f, 16.f / 9.f));
	TestEqual(TEXT("정상 값 불변"), Good.VerticalFov, 60.f);

	// 응시 유예 0.2초, 0.1초 주기
	FGazeDwell Dwell;
	TestTrue(TEXT("보면 보는 중"), UpdateGazeDwell(Dwell, 0.1f, true, 0.2f));
	TestTrue(TEXT("0.1초 끊김은 유예"), UpdateGazeDwell(Dwell, 0.1f, false, 0.2f));
	TestTrue(TEXT("0.2초 끊김까지 유예"), UpdateGazeDwell(Dwell, 0.1f, false, 0.2f));
	TestEqual(TEXT("유예 포함 연속 시간"), Dwell.DwellSeconds, 0.3f, 1e-4f);
	TestFalse(TEXT("0.3초 끊김은 해제"), UpdateGazeDwell(Dwell, 0.1f, false, 0.2f));
	TestEqual(TEXT("해제 시 연속 시간 0"), Dwell.DwellSeconds, 0.f);
	FGazeDwell Fresh;
	TestFalse(TEXT("본 적 없으면 보지 않음"), UpdateGazeDwell(Fresh, 0.1f, false, 0.2f));

	// 카메라 즉시 재전송 판정
	TestTrue(TEXT("5° 이상 회전은 즉시 전송"), UDigetherMonsterCameraSampleComponent::ShouldResend(FRotator::ZeroRotator, FRotator(0.f, 6.f, 0.f), 5.f));
	TestFalse(TEXT("작은 회전은 주기 대기"), UDigetherMonsterCameraSampleComponent::ShouldResend(FRotator::ZeroRotator, FRotator(0.f, 4.f, 0.f), 5.f));
	return true;
}

#endif
