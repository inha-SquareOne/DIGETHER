#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Monster/DigetherMonsterDefinition.h"
#include "Game/Monster/Spider/DigetherGroundSpiderComponent.h"
#include "Game/Monster/Spider/DigetherGroundSpiderRules.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherGroundSpiderRulesTest,
	"DIGETHER.Game.Monster.GroundSpiderRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherGroundSpiderRulesTest::RunTest(const FString& Parameters)
{
	using namespace DigetherGroundSpiderRules;

	// 대상 선택: 도달 가능한 가장 가까운 사람
	TArray<FTargetCandidate> Candidates;
	Candidates.Add({TEXT("Near_Unreachable"), 100.f, false});
	Candidates.Add({TEXT("Far"), 900.f, true});
	Candidates.Add({TEXT("Mid"), 400.f, true});
	TestEqual(TEXT("도달 불가 제외 후 가장 가까운 대상"), SelectNearestReachable(Candidates), 2);
	Candidates[2].bReachable = false;
	TestEqual(TEXT("남은 도달 가능 대상"), SelectNearestReachable(Candidates), 1);
	Candidates[1].bReachable = false;
	TestEqual(TEXT("모두 도달 불가"), SelectNearestReachable(Candidates), static_cast<int32>(INDEX_NONE));

	// 포기 조건: 시간 제한 없음
	FTrackingInput Input;
	Input.bTargetValid = true;
	Input.bReachable = true;
	Input.TrackRadius = 2500.f;
	Input.TargetDistanceFromHome = 1000.f;
	Input.ReacquireSeconds = 3.f;
	Input.SecondsSinceSeen = 0.f;
	TestEqual(TEXT("조건 만족 시 계속 추적"), EvaluateGiveUp(Input), EDigetherSpiderGiveUpReason::None);
	FTrackingInput Long = Input;
	Long.SecondsSinceSeen = 2.9f;
	TestEqual(TEXT("오래 쫓아도 시간 제한 없음"), EvaluateGiveUp(Long), EDigetherSpiderGiveUpReason::None);
	FTrackingInput Lost = Input;
	Lost.bTargetValid = false;
	TestEqual(TEXT("대상 사망·이탈"), EvaluateGiveUp(Lost), EDigetherSpiderGiveUpReason::TargetLost);
	FTrackingInput Blocked = Input;
	Blocked.bReachable = false;
	TestEqual(TEXT("도달 불가"), EvaluateGiveUp(Blocked), EDigetherSpiderGiveUpReason::Unreachable);
	FTrackingInput Outside = Input;
	Outside.TargetDistanceFromHome = 2600.f;
	TestEqual(TEXT("추적 영역 밖"), EvaluateGiveUp(Outside), EDigetherSpiderGiveUpReason::OutOfArea);
	Outside.bChaseOutsideArea = true;
	TestEqual(TEXT("영역 밖에서 공격받았으면 계속"), EvaluateGiveUp(Outside), EDigetherSpiderGiveUpReason::None);
	FTrackingInput Unseen = Input;
	Unseen.SecondsSinceSeen = 3.1f;
	TestEqual(TEXT("재감지 실패"), EvaluateGiveUp(Unseen), EDigetherSpiderGiveUpReason::NotReacquired);

	// 추적 후보 자격: 영역 경계 밖 대상은 후보 아님(추적 직후 포기 반복 방지), 영역 밖 공격자는 예외
	TestTrue(TEXT("영역 안 대상"), IsTrackableCandidate(2400.f, 2500.f, false));
	TestFalse(TEXT("영역 밖 대상은 후보 아님"), IsTrackableCandidate(2600.f, 2500.f, false));
	TestTrue(TEXT("영역 밖 공격자는 후보"), IsTrackableCandidate(2600.f, 2500.f, true));
	FTrackingInput Boundary = Input;
	Boundary.TargetDistanceFromHome = 2600.f;
	TestTrue(TEXT("후보에서 뺀 대상은 추적해도 즉시 포기되는 대상과 같음"),
		!IsTrackableCandidate(2600.f, 2500.f, false) && EvaluateGiveUp(Boundary) == EDigetherSpiderGiveUpReason::OutOfArea);

	// 도약: 최대 8m, 1500cm/s
	const FVector Start = FVector::ZeroVector;
	TestTrue(TEXT("가까우면 대상 위치"), ComputeLeapDestination(Start, FVector(500.f, 0.f, 0.f), 800.f).Equals(FVector(500.f, 0.f, 0.f)));
	TestTrue(TEXT("멀면 최대 거리"), ComputeLeapDestination(Start, FVector(2000.f, 0.f, 0.f), 800.f).Equals(FVector(800.f, 0.f, 0.f)));
	TestEqual(TEXT("8m 비행 시간"), ComputeLeapFlightSeconds(Start, FVector(800.f, 0.f, 0.f), 1500.f), 800.f / 1500.f, 1e-4f);
	// 기획 평균 약 415cm/s: 8m / (0.6 + 0.533 + 0.8)
	TestEqual(TEXT("기획 평균 속도 근사"), 800.f / (0.6f + 800.f / 1500.f + 0.8f), 414.f, 2.f);

	// 접촉 피해: 1초마다, 도약 같은 순간·매몰 제외
	FContactInput Contact;
	Contact.Now = 10.0;
	Contact.IntervalSeconds = 1.f;
	TestTrue(TEXT("첫 접촉"), ShouldApplyContactDamage(Contact));
	Contact.LastContactDamageTime = 9.5;
	TestFalse(TEXT("간격 전"), ShouldApplyContactDamage(Contact));
	Contact.LastContactDamageTime = 9.0;
	TestTrue(TEXT("간격 후"), ShouldApplyContactDamage(Contact));
	Contact.bLeapDamagedThisFrame = true;
	TestFalse(TEXT("같은 순간 도약 피해와 중복 없음"), ShouldApplyContactDamage(Contact));
	Contact.bLeapDamagedThisFrame = false;
	Contact.bTargetBuried = true;
	TestFalse(TEXT("매몰자 제외"), ShouldApplyContactDamage(Contact));
	Contact.bTargetBuried = false;
	Contact.bSpiderBuried = true;
	TestFalse(TEXT("땅거미 매몰 중 공격 없음"), ShouldApplyContactDamage(Contact));

	// 종 파라미터 검사
	FDigetherGroundSpiderParams Params;
	Params.WebBaseCount = 0;
	TArray<FText> Problems;
	TestTrue(TEXT("기본 시험값 유효(거미줄 없음)"), Params.CollectProblems(Problems));
	Params.CorpseRadius = 1000.f;
	TestFalse(TEXT("시체 회수 영역은 추적 영역 이상"), Params.CollectProblems(Problems));
	FDigetherGroundSpiderParams NeedsWeb;
	TestFalse(TEXT("거미줄 기준량이 있으면 클래스 필요"), NeedsWeb.CollectProblems(Problems));

	// 종 컴포넌트 파라미터 타입 검사
	UDigetherMonsterDefinition* Wrong = NewObject<UDigetherMonsterDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
	UDigetherGroundSpiderComponent* Component = NewObject<UDigetherGroundSpiderComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	AddExpectedError(TEXT("요구 타입"), EAutomationExpectedErrorFlags::Contains, 0);
	TestFalse(TEXT("종 파라미터 없으면 초기화 실패"), Component->InitializeSpecies(*Wrong));
	UDigetherMonsterDefinition* Right = NewObject<UDigetherMonsterDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
	Right->SpeciesParams.InitializeAs<FDigetherGroundSpiderParams>();
	TestTrue(TEXT("땅거미 파라미터면 초기화 성공"), Component->InitializeSpecies(*Right));
	return true;
}

#endif
