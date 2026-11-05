#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Life/DigetherLifeRules.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#include <limits>

namespace
{
	bool AreLifeStatesEqual(const FDigetherLifeComponentState& Left, const FDigetherLifeComponentState& Right)
	{
		return FDigetherLifeComponentState::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
	}

	FDigetherSpectateCandidate MakeCandidate(const TCHAR* PlayerKey, const EDigetherLifeState Life)
	{
		FDigetherSpectateCandidate Candidate;
		Candidate.PlayerKey = PlayerKey;
		Candidate.Life = Life;
		return Candidate;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherLifeTransitionsTest,
	"DIGETHER.Game.Life.Transitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherLifeTransitionsTest::RunTest(const FString& Parameters)
{
	const FDigetherLifeComponentState Initial;
	TestTrue(TEXT("기본 활동 상태는 유효"), FDigetherLifeRules::IsValidState(Initial));
	TestEqual(TEXT("활동 중 부활은 거절"), FDigetherLifeRules::Revive(Initial).Result, EDigetherLifeResult::AlreadyActive);
	TestTrue(TEXT("활동 중 부활 거절은 불변"), AreLifeStatesEqual(Initial, FDigetherLifeRules::Revive(Initial).State));
	TestFalse(TEXT("사인 없음은 매몰 아님"), FDigetherLifeRules::IsMissingCause(EDigetherDeathCause::None));
	TestFalse(TEXT("처치는 매몰 아님"), FDigetherLifeRules::IsMissingCause(EDigetherDeathCause::Killed));
	TestFalse(TEXT("미정의 사인은 매몰 아님"), FDigetherLifeRules::IsMissingCause(static_cast<EDigetherDeathCause>(MAX_uint8)));
	TestTrue(TEXT("시간 만료는 매몰"), FDigetherLifeRules::IsMissingCause(EDigetherDeathCause::CollapseTimeExpired));
	TestTrue(TEXT("마지막 상승 후 잔류는 매몰"), FDigetherLifeRules::IsMissingCause(EDigetherDeathCause::CollapseRemainedAfterFinalAscent));
	TestTrue(TEXT("방출은 매몰"), FDigetherLifeRules::IsMissingCause(EDigetherDeathCause::CollapseEjected));

	const EDigetherDeathCause Causes[] =
	{
		EDigetherDeathCause::Killed,
		EDigetherDeathCause::CollapseTimeExpired,
		EDigetherDeathCause::CollapseRemainedAfterFinalAscent,
		EDigetherDeathCause::CollapseEjected
	};
	for (const EDigetherDeathCause Cause : Causes)
	{
		const FDigetherLifeUpdateResult Death = FDigetherLifeRules::ApplyDeath(Initial, Cause);
		TestEqual(TEXT("활동에서 사망 확정"), Death.Result, EDigetherLifeResult::Success);
		TestEqual(TEXT("생사는 사망"), Death.State.PublicState.Life, EDigetherLifeState::Dead);
		TestTrue(TEXT("사망 순간 관전 전환"), Death.State.PublicState.bSpectating);
		TestEqual(TEXT("사인 보존"), Death.State.PrivateState.DeathCause, Cause);
		TestEqual(TEXT("사망에는 탈출 종류 없음"), Death.State.PrivateState.EscapeKind, EDigetherEscapeKind::None);
		TestTrue(TEXT("사망 직후 관전 대상 없음"), Death.State.PrivateState.SpectateTargetPlayerKey.IsEmpty());
		TestTrue(TEXT("사망 결과의 불변식"), FDigetherLifeRules::IsValidState(Death.State));
		const FDigetherLifeUpdateResult Repeated = FDigetherLifeRules::ApplyDeath(Death.State, Cause);
		TestEqual(TEXT("중복 사망 거절"), Repeated.Result, EDigetherLifeResult::NotActive);
		TestTrue(TEXT("중복 사망은 상태 불변"), AreLifeStatesEqual(Death.State, Repeated.State));
		const FDigetherLifeUpdateResult Escape = FDigetherLifeRules::ApplyEscape(Death.State, EDigetherEscapeKind::Elevator);
		TestEqual(TEXT("사망 후 탈출 거절"), Escape.Result, EDigetherLifeResult::NotActive);
		TestTrue(TEXT("사망 후 탈출 거절은 불변"), AreLifeStatesEqual(Death.State, Escape.State));
		FDigetherLifeComponentState Spectating = Death.State;
		Spectating.PrivateState.SpectateTargetPlayerKey = TEXT("fixture-friend");
		const FDigetherLifeUpdateResult Revived = FDigetherLifeRules::Revive(Spectating);
		TestEqual(TEXT("사망에서 부활"), Revived.Result, EDigetherLifeResult::Success);
		TestTrue(TEXT("부활은 사인·탈출·관전 전체 초기화"), AreLifeStatesEqual(Initial, Revived.State));
	}

	const EDigetherEscapeKind Kinds[] = { EDigetherEscapeKind::Elevator, EDigetherEscapeKind::Emergency };
	for (const EDigetherEscapeKind Kind : Kinds)
	{
		const FDigetherLifeUpdateResult Escape = FDigetherLifeRules::ApplyEscape(Initial, Kind);
		TestEqual(TEXT("활동에서 탈출 확정"), Escape.Result, EDigetherLifeResult::Success);
		TestEqual(TEXT("생사는 탈출"), Escape.State.PublicState.Life, EDigetherLifeState::Escaped);
		TestTrue(TEXT("탈출 확정 순간 관전 전환"), Escape.State.PublicState.bSpectating);
		TestEqual(TEXT("탈출 종류 보존"), Escape.State.PrivateState.EscapeKind, Kind);
		TestEqual(TEXT("탈출에는 사인 없음"), Escape.State.PrivateState.DeathCause, EDigetherDeathCause::None);
		TestTrue(TEXT("탈출 결과의 불변식"), FDigetherLifeRules::IsValidState(Escape.State));
		TestEqual(TEXT("중복 탈출 거절"), FDigetherLifeRules::ApplyEscape(Escape.State, Kind).Result, EDigetherLifeResult::NotActive);
		TestTrue(TEXT("중복 탈출은 불변"), AreLifeStatesEqual(Escape.State, FDigetherLifeRules::ApplyEscape(Escape.State, Kind).State));
		TestEqual(TEXT("탈출 후 사망 거절"), FDigetherLifeRules::ApplyDeath(Escape.State, EDigetherDeathCause::Killed).Result, EDigetherLifeResult::NotActive);
		TestTrue(TEXT("탈출 후 사망 거절은 불변"), AreLifeStatesEqual(Escape.State, FDigetherLifeRules::ApplyDeath(Escape.State, EDigetherDeathCause::Killed).State));
		FDigetherLifeComponentState Spectating = Escape.State;
		Spectating.PrivateState.SpectateTargetPlayerKey = TEXT("fixture-friend");
		const FDigetherLifeUpdateResult Revived = FDigetherLifeRules::Revive(Spectating);
		TestEqual(TEXT("탈출에서 부활"), Revived.Result, EDigetherLifeResult::Success);
		TestTrue(TEXT("탈출 후 부활도 전체 초기화"), AreLifeStatesEqual(Initial, Revived.State));
	}

	const EDigetherDeathCause InvalidCauses[] = { EDigetherDeathCause::None, static_cast<EDigetherDeathCause>(MAX_uint8) };
	for (const EDigetherDeathCause Cause : InvalidCauses)
	{
		const FDigetherLifeUpdateResult Rejected = FDigetherLifeRules::ApplyDeath(Initial, Cause);
		TestEqual(TEXT("없거나 미정의 사인 거절"), Rejected.Result, EDigetherLifeResult::InvalidInput);
		TestTrue(TEXT("잘못된 사인에서 불변"), AreLifeStatesEqual(Initial, Rejected.State));
	}
	const EDigetherEscapeKind InvalidKinds[] = { EDigetherEscapeKind::None, static_cast<EDigetherEscapeKind>(MAX_uint8) };
	for (const EDigetherEscapeKind Kind : InvalidKinds)
	{
		const FDigetherLifeUpdateResult Rejected = FDigetherLifeRules::ApplyEscape(Initial, Kind);
		TestEqual(TEXT("없거나 미정의 탈출 종류 거절"), Rejected.Result, EDigetherLifeResult::InvalidInput);
		TestTrue(TEXT("잘못된 탈출 종류에서 불변"), AreLifeStatesEqual(Initial, Rejected.State));
	}

	TArray<FDigetherLifeComponentState> InvalidStates;
	FDigetherLifeComponentState Invalid;
	Invalid.PublicState.Life = static_cast<EDigetherLifeState>(MAX_uint8);
	InvalidStates.Add(Invalid);
	Invalid = Initial;
	Invalid.PublicState.bSpectating = true;
	InvalidStates.Add(Invalid);
	Invalid = Initial;
	Invalid.PrivateState.DeathCause = EDigetherDeathCause::Killed;
	InvalidStates.Add(Invalid);
	Invalid = Initial;
	Invalid.PrivateState.EscapeKind = EDigetherEscapeKind::Elevator;
	InvalidStates.Add(Invalid);
	Invalid = Initial;
	Invalid.PrivateState.SpectateTargetPlayerKey = TEXT("fixture-friend");
	InvalidStates.Add(Invalid);
	const FDigetherLifeComponentState Dead = FDigetherLifeRules::ApplyDeath(Initial, EDigetherDeathCause::Killed).State;
	Invalid = Dead;
	Invalid.PrivateState.DeathCause = EDigetherDeathCause::None;
	InvalidStates.Add(Invalid);
	Invalid = Dead;
	Invalid.PublicState.bSpectating = false;
	InvalidStates.Add(Invalid);
	Invalid = Dead;
	Invalid.PrivateState.EscapeKind = EDigetherEscapeKind::Emergency;
	InvalidStates.Add(Invalid);
	Invalid = Dead;
	Invalid.PrivateState.DeathCause = static_cast<EDigetherDeathCause>(MAX_uint8);
	InvalidStates.Add(Invalid);
	Invalid = Dead;
	Invalid.PrivateState.SpectateTargetPlayerKey = TEXT(" \t ");
	InvalidStates.Add(Invalid);
	const FDigetherLifeComponentState Escaped = FDigetherLifeRules::ApplyEscape(Initial, EDigetherEscapeKind::Elevator).State;
	Invalid = Escaped;
	Invalid.PrivateState.EscapeKind = EDigetherEscapeKind::None;
	InvalidStates.Add(Invalid);
	Invalid = Escaped;
	Invalid.PrivateState.DeathCause = EDigetherDeathCause::Killed;
	InvalidStates.Add(Invalid);
	Invalid = Escaped;
	Invalid.PublicState.bSpectating = false;
	InvalidStates.Add(Invalid);
	Invalid = Escaped;
	Invalid.PrivateState.EscapeKind = static_cast<EDigetherEscapeKind>(MAX_uint8);
	InvalidStates.Add(Invalid);
	for (const FDigetherLifeComponentState& State : InvalidStates)
	{
		TestFalse(TEXT("복원에 쓰는 전체 상태 불변식으로 잘못된 조합 거절"), FDigetherLifeRules::IsValidState(State));
		const FDigetherLifeUpdateResult Death = FDigetherLifeRules::ApplyDeath(State, EDigetherDeathCause::Killed);
		const FDigetherLifeUpdateResult Escape = FDigetherLifeRules::ApplyEscape(State, EDigetherEscapeKind::Elevator);
		const FDigetherLifeUpdateResult Revive = FDigetherLifeRules::Revive(State);
		TestEqual(TEXT("잘못된 상태 사망 거절"), Death.Result, EDigetherLifeResult::InvalidInput);
		TestEqual(TEXT("잘못된 상태 탈출 거절"), Escape.Result, EDigetherLifeResult::InvalidInput);
		TestEqual(TEXT("잘못된 상태 부활 거절"), Revive.Result, EDigetherLifeResult::InvalidInput);
		TestTrue(TEXT("잘못된 상태 사망 거절 불변"), AreLifeStatesEqual(State, Death.State));
		TestTrue(TEXT("잘못된 상태 탈출 거절 불변"), AreLifeStatesEqual(State, Escape.State));
		TestTrue(TEXT("잘못된 상태 부활 거절 불변"), AreLifeStatesEqual(State, Revive.State));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherLifeDeathDropTest,
	"DIGETHER.Game.Life.DeathDrop", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherLifeDeathDropTest::RunTest(const FString& Parameters)
{
	// 테스트 픽스처, 기획 값 아님
	const FVector PlayerLocation(12.0, 34.0, 56.0);
	const FVector DropLocation(78.0, 90.0, 12.0);
	FDigetherLifeDropItem Equipment;
	Equipment.InstanceId = FGuid(1, 2, 3, 4);
	FDigetherLifeDropItem Hidden;
	Hidden.InstanceId = FGuid(5, 6, 7, 8);
	Hidden.bHidden = true;
	const TArray<FDigetherLifeDropItem> Items = { Equipment, Hidden };
	const FDigetherDeathDropPlan Fallback = FDigetherLifeRules::PlanDeathDrop(Items, PlayerLocation);
	TestEqual(TEXT("사망 드롭 계획 성공"), Fallback.Result, EDigetherLifeResult::Success);
	TestEqual(TEXT("장비·히든을 모두 드롭"), Fallback.InstanceIds.Num(), Items.Num());
	TestTrue(TEXT("드롭 ID와 입력 순서 보존"), Fallback.InstanceIds == TArray<FGuid>({ Equipment.InstanceId, Hidden.InstanceId }));
	TestEqual(TEXT("위치가 없으면 플레이어 위치"), Fallback.Location, PlayerLocation);
	const FDigetherDeathDropPlan Explicit = FDigetherLifeRules::PlanDeathDrop(Items, PlayerLocation, TOptional<FVector>(DropLocation));
	TestEqual(TEXT("지정 위치 계획 성공"), Explicit.Result, EDigetherLifeResult::Success);
	TestEqual(TEXT("지정 드롭 위치 우선"), Explicit.Location, DropLocation);
	TestEqual(TEXT("빈 소지품도 유효한 계획"), FDigetherLifeRules::PlanDeathDrop({}, PlayerLocation).Result, EDigetherLifeResult::Success);
	TestTrue(TEXT("빈 소지품 계획은 ID 없음"), FDigetherLifeRules::PlanDeathDrop({}, PlayerLocation).InstanceIds.IsEmpty());

	TArray<FDigetherLifeDropItem> InvalidItems = Items;
	InvalidItems.Add(FDigetherLifeDropItem());
	FDigetherDeathDropPlan Invalid = FDigetherLifeRules::PlanDeathDrop(InvalidItems, PlayerLocation);
	TestEqual(TEXT("유효 항목 뒤의 invalid GUID도 전체 거절"), Invalid.Result, EDigetherLifeResult::InvalidInput);
	TestTrue(TEXT("invalid GUID 거절은 부분 ID 없음"), Invalid.InstanceIds.IsEmpty());
	TestEqual(TEXT("invalid GUID 거절은 위치도 미확정"), Invalid.Location, FVector::ZeroVector);
	InvalidItems = Items;
	FDigetherLifeDropItem Duplicate = Equipment;
	Duplicate.bHidden = true;
	InvalidItems.Add(Duplicate);
	Invalid = FDigetherLifeRules::PlanDeathDrop(InvalidItems, PlayerLocation);
	TestEqual(TEXT("히든 여부가 달라도 같은 GUID 중복 거절"), Invalid.Result, EDigetherLifeResult::InvalidInput);
	TestTrue(TEXT("중복 거절은 전체 원자적"), Invalid.InstanceIds.IsEmpty());
	TestEqual(TEXT("거절 후 입력 소지품 수 유지"), InvalidItems.Num(), 3);

	const double NaN = std::numeric_limits<double>::quiet_NaN();
	const double Infinity = std::numeric_limits<double>::infinity();
	const FVector InvalidLocations[] = { FVector(NaN, 0.0, 0.0), FVector(0.0, Infinity, 0.0), FVector(0.0, 0.0, -Infinity) };
	for (const FVector& Location : InvalidLocations)
	{
		const FDigetherDeathDropPlan InvalidFallback = FDigetherLifeRules::PlanDeathDrop(Items, Location);
		TestEqual(TEXT("fallback 위치의 NaN·무한대 거절"), InvalidFallback.Result, EDigetherLifeResult::InvalidInput);
		TestTrue(TEXT("잘못된 fallback 위치는 부분 드롭 없음"), InvalidFallback.InstanceIds.IsEmpty());
		const FDigetherDeathDropPlan InvalidExplicit = FDigetherLifeRules::PlanDeathDrop(Items, PlayerLocation, TOptional<FVector>(Location));
		TestEqual(TEXT("지정 위치의 NaN·무한대 거절"), InvalidExplicit.Result, EDigetherLifeResult::InvalidInput);
		TestTrue(TEXT("잘못된 지정 위치도 부분 드롭 없음"), InvalidExplicit.InstanceIds.IsEmpty());
		TestEqual(TEXT("지정 위치가 있으면 사용하지 않는 fallback은 무관"),
			FDigetherLifeRules::PlanDeathDrop(Items, Location, TOptional<FVector>(DropLocation)).Result, EDigetherLifeResult::Success);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherLifeSpectateTest,
	"DIGETHER.Game.Life.Spectate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherLifeSpectateTest::RunTest(const FString& Parameters)
{
	const FString Self = TEXT("fixture-self");
	const TArray<FDigetherSpectateCandidate> Candidates =
	{
		MakeCandidate(TEXT("fixture-self"), EDigetherLifeState::Active),
		MakeCandidate(TEXT("fixture-a"), EDigetherLifeState::Active),
		MakeCandidate(TEXT("fixture-b"), EDigetherLifeState::Dead),
		MakeCandidate(TEXT("fixture-c"), EDigetherLifeState::Active),
		MakeCandidate(TEXT("fixture-d"), EDigetherLifeState::Escaped)
	};
	TestEqual(TEXT("초기 다음은 첫 활동 동료"), FDigetherLifeRules::SelectSpectateTarget(Candidates, Self, TEXT(""), EDigetherSpectateDirection::Next).TargetPlayerKey, FString(TEXT("fixture-a")));
	TestEqual(TEXT("초기 이전은 마지막 활동 동료"), FDigetherLifeRules::SelectSpectateTarget(Candidates, Self, TEXT(""), EDigetherSpectateDirection::Previous).TargetPlayerKey, FString(TEXT("fixture-c")));
	TestEqual(TEXT("다음은 사망자 건너뛰기"), FDigetherLifeRules::SelectSpectateTarget(Candidates, Self, TEXT("fixture-a"), EDigetherSpectateDirection::Next).TargetPlayerKey, FString(TEXT("fixture-c")));
	TestEqual(TEXT("이전은 사망자 건너뛰기"), FDigetherLifeRules::SelectSpectateTarget(Candidates, Self, TEXT("fixture-c"), EDigetherSpectateDirection::Previous).TargetPlayerKey, FString(TEXT("fixture-a")));
	TestEqual(TEXT("다음 끝에서 자기·탈출 제외하고 순환"), FDigetherLifeRules::SelectSpectateTarget(Candidates, Self, TEXT("fixture-c"), EDigetherSpectateDirection::Next).TargetPlayerKey, FString(TEXT("fixture-a")));
	TestEqual(TEXT("이전 처음에서 순환"), FDigetherLifeRules::SelectSpectateTarget(Candidates, Self, TEXT("fixture-a"), EDigetherSpectateDirection::Previous).TargetPlayerKey, FString(TEXT("fixture-c")));
	TestEqual(TEXT("현 대상 사망 시 원래 위치의 다음"), FDigetherLifeRules::SelectSpectateTarget(Candidates, Self, TEXT("fixture-b"), EDigetherSpectateDirection::Next).TargetPlayerKey, FString(TEXT("fixture-c")));
	TestEqual(TEXT("현 대상 사망 시 원래 위치의 이전"), FDigetherLifeRules::SelectSpectateTarget(Candidates, Self, TEXT("fixture-b"), EDigetherSpectateDirection::Previous).TargetPlayerKey, FString(TEXT("fixture-a")));
	TestEqual(TEXT("현 대상 탈출 시 다음 순환"), FDigetherLifeRules::SelectSpectateTarget(Candidates, Self, TEXT("fixture-d"), EDigetherSpectateDirection::Next).TargetPlayerKey, FString(TEXT("fixture-a")));
	TestEqual(TEXT("현 대상 탈출 시 이전"), FDigetherLifeRules::SelectSpectateTarget(Candidates, Self, TEXT("fixture-d"), EDigetherSpectateDirection::Previous).TargetPlayerKey, FString(TEXT("fixture-c")));
	TestEqual(TEXT("목록에서 사라진 대상의 다음은 처음"), FDigetherLifeRules::SelectSpectateTarget(Candidates, Self, TEXT("fixture-absent"), EDigetherSpectateDirection::Next).TargetPlayerKey, FString(TEXT("fixture-a")));
	TestEqual(TEXT("목록에서 사라진 대상의 이전은 마지막"), FDigetherLifeRules::SelectSpectateTarget(Candidates, Self, TEXT("fixture-absent"), EDigetherSpectateDirection::Previous).TargetPlayerKey, FString(TEXT("fixture-c")));

	const EDigetherSpectateDirection Directions[] = { EDigetherSpectateDirection::Next, EDigetherSpectateDirection::Previous };
	for (const EDigetherSpectateDirection Direction : Directions)
	{
		TestEqual(TEXT("self가 현 대상이어도 자기 제외"), FDigetherLifeRules::SelectSpectateTarget(Candidates, Self, Self, Direction).TargetPlayerKey,
			Direction == EDigetherSpectateDirection::Next ? FString(TEXT("fixture-a")) : FString(TEXT("fixture-c")));
		const TArray<FDigetherSpectateCandidate> Single = { Candidates[1] };
		TestEqual(TEXT("한 명이면 현재 대상 유지"), FDigetherLifeRules::SelectSpectateTarget(Single, Self, TEXT("fixture-a"), Direction).TargetPlayerKey, FString(TEXT("fixture-a")));
		const FDigetherSpectateSelectionResult Empty = FDigetherLifeRules::SelectSpectateTarget({}, Self, TEXT("fixture-a"), Direction);
		TestEqual(TEXT("빈 목록은 정상 결과"), Empty.Result, EDigetherLifeResult::Success);
		TestTrue(TEXT("빈 목록은 관전 대상 없음"), Empty.TargetPlayerKey.IsEmpty());
		const TArray<FDigetherSpectateCandidate> OnlySelf = { Candidates[0] };
		TestTrue(TEXT("자기만 활동 중이면 대상 없음"), FDigetherLifeRules::SelectSpectateTarget(OnlySelf, Self, Self, Direction).TargetPlayerKey.IsEmpty());
		const TArray<FDigetherSpectateCandidate> Inactive = { Candidates[2], Candidates[4] };
		TestTrue(TEXT("사망·탈출만 있으면 대상 없음"), FDigetherLifeRules::SelectSpectateTarget(Inactive, Self, TEXT(""), Direction).TargetPlayerKey.IsEmpty());
	}

	TArray<TArray<FDigetherSpectateCandidate>> InvalidLists;
	TArray<FDigetherSpectateCandidate> Invalid = Candidates;
	Invalid[1].PlayerKey.Reset();
	InvalidLists.Add(Invalid);
	Invalid = Candidates;
	Invalid[1].PlayerKey = TEXT(" \t ");
	InvalidLists.Add(Invalid);
	Invalid = Candidates;
	Invalid[1].Life = static_cast<EDigetherLifeState>(MAX_uint8);
	InvalidLists.Add(Invalid);
	Invalid = Candidates;
	Invalid[2].PlayerKey = Invalid[1].PlayerKey;
	InvalidLists.Add(Invalid);
	for (const TArray<FDigetherSpectateCandidate>& List : InvalidLists)
	{
		const FDigetherSpectateSelectionResult Selection = FDigetherLifeRules::SelectSpectateTarget(List, Self, TEXT(""), EDigetherSpectateDirection::Next);
		TestEqual(TEXT("빈·공백·미정의 상태·중복 후보 거절"), Selection.Result, EDigetherLifeResult::InvalidInput);
		TestTrue(TEXT("후보 형태 오류는 부분 대상 없음"), Selection.TargetPlayerKey.IsEmpty());
	}
	TestEqual(TEXT("빈 자기 식별자 거절"), FDigetherLifeRules::SelectSpectateTarget(Candidates, TEXT(""), TEXT(""), EDigetherSpectateDirection::Next).Result, EDigetherLifeResult::InvalidInput);
	TestEqual(TEXT("공백 자기 식별자 거절"), FDigetherLifeRules::SelectSpectateTarget(Candidates, TEXT(" \t "), TEXT(""), EDigetherSpectateDirection::Next).Result, EDigetherLifeResult::InvalidInput);
	TestEqual(TEXT("공백 현 대상 거절"), FDigetherLifeRules::SelectSpectateTarget(Candidates, Self, TEXT(" \t "), EDigetherSpectateDirection::Next).Result, EDigetherLifeResult::InvalidInput);
	TestEqual(TEXT("미정의 방향 거절"), FDigetherLifeRules::SelectSpectateTarget(Candidates, Self, TEXT(""), static_cast<EDigetherSpectateDirection>(MAX_uint8)).Result, EDigetherLifeResult::InvalidInput);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherLifeEmergencyEscapeTest,
	"DIGETHER.Game.Life.EmergencyEscape", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherLifeEmergencyEscapeTest::RunTest(const FString& Parameters)
{
	// 테스트 픽스처, 기획 값 아님, 실제 한도는 런 튜닝 스냅샷에서 전달
	constexpr float FixtureLimitKg = 10.f;
	FDigetherEmergencyCarriedItem First;
	First.WeightKg = 4.f;
	FDigetherEmergencyCarriedItem Second;
	Second.WeightKg = 6.f;
	TArray<FDigetherEmergencyCarriedItem> Items = { First, Second };
	TestEqual(TEXT("한도와 같은 장비 무게 허용"), FDigetherLifeRules::CheckEmergencyEscape(EDigetherLifeState::Active, Items, FixtureLimitKg), EDigetherEmergencyEscapeResult::Allowed);
	TestEqual(TEXT("빈 소지품은 한도 0에서도 신체 무게 0으로 허용"), FDigetherLifeRules::CheckEmergencyEscape(EDigetherLifeState::Active, {}, 0.f), EDigetherEmergencyEscapeResult::Allowed);
	Items[1].WeightKg = 5.f;
	TestEqual(TEXT("한도 미만 허용"), FDigetherLifeRules::CheckEmergencyEscape(EDigetherLifeState::Active, Items, FixtureLimitKg), EDigetherEmergencyEscapeResult::Allowed);
	Items[1].WeightKg = 7.f;
	TestEqual(TEXT("한도 초과 거절"), FDigetherLifeRules::CheckEmergencyEscape(EDigetherLifeState::Active, Items, FixtureLimitKg), EDigetherEmergencyEscapeResult::Overweight);
	Items[1].bRecoverable = true;
	Items[1].WeightKg = 0.f;
	TestEqual(TEXT("무게 0 회수품도 거절"), FDigetherLifeRules::CheckEmergencyEscape(EDigetherLifeState::Active, Items, FixtureLimitKg), EDigetherEmergencyEscapeResult::CarryingRecoverable);
	Items[1].WeightKg = 7.f;
	TestEqual(TEXT("회수품 거절은 무게 초과보다 우선"), FDigetherLifeRules::CheckEmergencyEscape(EDigetherLifeState::Active, Items, FixtureLimitKg), EDigetherEmergencyEscapeResult::CarryingRecoverable);
	const EDigetherLifeState InactiveStates[] = { EDigetherLifeState::Dead, EDigetherLifeState::Escaped };
	for (const EDigetherLifeState Life : InactiveStates)
	{
		TestEqual(TEXT("사망·탈출은 다시 긴급 탈출 불가"), FDigetherLifeRules::CheckEmergencyEscape(Life, {}, FixtureLimitKg), EDigetherEmergencyEscapeResult::NotActive);
		TestEqual(TEXT("생사 거절은 회수품·무게보다 우선"), FDigetherLifeRules::CheckEmergencyEscape(Life, Items, FixtureLimitKg), EDigetherEmergencyEscapeResult::NotActive);
	}
	TestEqual(TEXT("미정의 생사 거절"), FDigetherLifeRules::CheckEmergencyEscape(static_cast<EDigetherLifeState>(MAX_uint8), {}, FixtureLimitKg), EDigetherEmergencyEscapeResult::InvalidInput);

	const float InvalidWeights[] = { -1.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() };
	for (const float WeightKg : InvalidWeights)
	{
		Items[1].WeightKg = WeightKg;
		TestEqual(TEXT("음수·NaN·무한대 항목 무게 거절"), FDigetherLifeRules::CheckEmergencyEscape(EDigetherLifeState::Active, Items, FixtureLimitKg), EDigetherEmergencyEscapeResult::InvalidInput);
		TestEqual(TEXT("비활동 상태에서도 손상 입력 우선 거절"), FDigetherLifeRules::CheckEmergencyEscape(EDigetherLifeState::Dead, Items, FixtureLimitKg), EDigetherEmergencyEscapeResult::InvalidInput);
		TestEqual(TEXT("음수·NaN·무한대 한도 거절"), FDigetherLifeRules::CheckEmergencyEscape(EDigetherLifeState::Active, {}, WeightKg), EDigetherEmergencyEscapeResult::InvalidInput);
	}
	Items[0].WeightKg = std::numeric_limits<float>::max();
	Items[1].WeightKg = std::numeric_limits<float>::max();
	Items[1].bRecoverable = false;
	TestEqual(TEXT("유한 무게 합은 float 오버플로 없이 한도 초과"), FDigetherLifeRules::CheckEmergencyEscape(EDigetherLifeState::Active, Items, std::numeric_limits<float>::max()), EDigetherEmergencyEscapeResult::Overweight);
	return true;
}

#endif
