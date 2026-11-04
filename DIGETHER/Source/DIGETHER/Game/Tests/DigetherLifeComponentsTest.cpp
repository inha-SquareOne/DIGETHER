#if WITH_DEV_AUTOMATION_TESTS

#include "Game/Life/DigetherLifeStateComponent.h"
#include "Game/Life/DigetherLifeRules.h"
#include "Misc/AutomationTest.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

namespace
{
	bool AreLifeComponentSnapshotsEqual(const FDigetherLifeComponentState& Left, const FDigetherLifeComponentState& Right)
	{
		return FDigetherLifeComponentState::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
	}

	bool HasLifeReplicationCondition(const UDigetherLifeStateComponent* Component, const FName PropertyName,
		const ELifetimeCondition Condition)
	{
		UClass* ComponentClass = Component->GetClass();
		ComponentClass->SetUpRuntimeReplicationData();
		const FProperty* Property = FindFProperty<FProperty>(ComponentClass, PropertyName);
		if (Property == nullptr || !Property->HasAllPropertyFlags(CPF_Net | CPF_RepNotify))
		{
			return false;
		}

		TArray<FLifetimeProperty> Properties;
		Component->GetLifetimeReplicatedProps(Properties);
		for (const FLifetimeProperty& ReplicatedProperty : Properties)
		{
			if (ReplicatedProperty.RepIndex == Property->RepIndex)
			{
				return ReplicatedProperty.Condition == Condition;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherLifeComponentsTest,
	"DIGETHER.Game.Life.Components", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherLifeComponentsTest::RunTest(const FString& Parameters)
{
	// 테스트 픽스처이며 최종 기획 값이 아니다.
	// 월드 없는 검사 범위다. 권위 owner의 실제 복원 왕복·지연 통지는 후속 월드 검사로 남긴다.
	UDigetherLifeStateComponent* Component = NewObject<UDigetherLifeStateComponent>();
	if (!TestNotNull(TEXT("월드 없는 생존 컴포넌트"), Component))
	{
		return false;
	}
	const FDigetherLifeComponentState Initial = Component->CaptureState();
	TestTrue(TEXT("생존 컴포넌트 기본 복제 활성"), Component->GetIsReplicated());
	TestTrue(TEXT("기본 CaptureState는 활동 불변식 충족"), FDigetherLifeRules::IsValidState(Initial));
	TestEqual(TEXT("기본 생사 활동"), Initial.PublicState.Life, EDigetherLifeState::Active);
	TestFalse(TEXT("기본 관전 꺼짐"), Initial.PublicState.bSpectating);
	TestEqual(TEXT("기본 사인 없음"), Initial.PrivateState.DeathCause, EDigetherDeathCause::None);
	TestEqual(TEXT("기본 탈출 종류 없음"), Initial.PrivateState.EscapeKind, EDigetherEscapeKind::None);
	TestTrue(TEXT("기본 관전 대상 없음"), Initial.PrivateState.SpectateTargetPlayerKey.IsEmpty());
	TestTrue(TEXT("생사·관전 여부는 공개 복제"), HasLifeReplicationCondition(Component, TEXT("PublicState"), COND_None));
	TestTrue(TEXT("사인·탈출 종류·관전 대상은 owner 전용 복제"), HasLifeReplicationCondition(Component, TEXT("PrivateState"), COND_OwnerOnly));

	FDigetherLifeComponentState Death = FDigetherLifeRules::ApplyDeath(Initial, EDigetherDeathCause::CollapseTimeExpired).State;
	Death.PrivateState.SpectateTargetPlayerKey = TEXT("fixture-target");
	TestTrue(TEXT("보관할 사망·관전 값의 불변식"), FDigetherLifeRules::IsValidState(Death));
	const FDigetherLifeComponentState Retained = Death;
	const FDigetherLifeUpdateResult Revived = FDigetherLifeRules::Revive(Retained);
	TestEqual(TEXT("보관 값에도 같은 부활 함수 적용"), Revived.Result, EDigetherLifeResult::Success);
	TestTrue(TEXT("보관 값 부활 결과는 초기값과 일치"), AreLifeComponentSnapshotsEqual(Initial, Revived.State));
	TestTrue(TEXT("보관 원본은 불변"), AreLifeComponentSnapshotsEqual(Death, Retained));

	int32 PublicNotificationCount = 0;
	int32 PrivateNotificationCount = 0;
	FDigetherLifePublicState NotifiedPublic;
	FDigetherLifePrivateState NotifiedPrivate;
	const FDelegateHandle PublicHandle = Component->OnPublicStateChanged.AddLambda(
		[&PublicNotificationCount, &NotifiedPublic](const FDigetherLifePublicState& State)
		{
			++PublicNotificationCount;
			NotifiedPublic = State;
		});
	const FDelegateHandle PrivateHandle = Component->OnPrivateStateChanged.AddLambda(
		[&PrivateNotificationCount, &NotifiedPrivate](const FDigetherLifePrivateState& State)
		{
			++PrivateNotificationCount;
			NotifiedPrivate = State;
		});

	TestEqual(TEXT("owner 없는 복원 거절"), Component->RestoreState(Death), EDigetherLifeResult::NotAuthority);
	TestEqual(TEXT("owner 없는 무통지 복원도 거절"), Component->RestoreState(Death, false), EDigetherLifeResult::NotAuthority);
	FDigetherLifeComponentState Invalid = Death;
	Invalid.PrivateState.DeathCause = EDigetherDeathCause::None;
	TestFalse(TEXT("복원 사전 검사에서 사망 원인 누락 거절"), FDigetherLifeRules::IsValidState(Invalid));
	TestEqual(TEXT("owner 없는 잘못된 복원도 권한 먼저 거절"), Component->RestoreState(Invalid), EDigetherLifeResult::NotAuthority);
	TestEqual(TEXT("owner 없는 확정 결과 적용 거절"), Component->ApplyUpdate(FDigetherLifeRules::ApplyDeath(Initial, EDigetherDeathCause::Killed)), EDigetherLifeResult::NotAuthority);
	TestEqual(TEXT("owner 없는 사망 명령 거절"), Component->ApplyDeath(EDigetherDeathCause::Killed), EDigetherLifeResult::NotAuthority);
	TestEqual(TEXT("owner 없는 탈출 명령 거절"), Component->ApplyEscape(EDigetherEscapeKind::Elevator), EDigetherLifeResult::NotAuthority);
	TestEqual(TEXT("owner 없는 부활 명령 거절"), Component->Revive(), EDigetherLifeResult::NotAuthority);
	const TArray<FDigetherSpectateCandidate> Candidates;
	TestEqual(TEXT("owner 없는 관전 대상 변경 거절"), Component->SelectSpectateTarget(Candidates, TEXT("fixture-self"), EDigetherSpectateDirection::Next), EDigetherLifeResult::NotAuthority);
	TestEqual(TEXT("owner 없는 확정 통지 거절"), Component->NotifyStateChanged(), EDigetherLifeResult::NotAuthority);
	TestTrue(TEXT("모든 거절 후 CaptureState 불변"), AreLifeComponentSnapshotsEqual(Initial, Component->CaptureState()));
	TestEqual(TEXT("거절된 요청의 공개 통지 없음"), PublicNotificationCount, 0);
	TestEqual(TEXT("거절된 요청의 개인 통지 없음"), PrivateNotificationCount, 0);

	// OnRep는 현재 스냅샷의 표현만 통지한다. 실제 네트워크 전송 검증은 별도 M2 범위다.
	UFunction* PublicOnRep = Component->FindFunction(TEXT("OnRep_PublicState"));
	UFunction* PrivateOnRep = Component->FindFunction(TEXT("OnRep_PrivateState"));
	if (TestNotNull(TEXT("공개 복제 알림 등록"), PublicOnRep))
	{
		Component->ProcessEvent(PublicOnRep, nullptr);
	}
	TestEqual(TEXT("공개 OnRep는 공개 통지 한 번"), PublicNotificationCount, 1);
	TestEqual(TEXT("공개 OnRep에서 개인 통지 없음"), PrivateNotificationCount, 0);
	TestTrue(TEXT("공개 OnRep의 값 일치"), FDigetherLifePublicState::StaticStruct()->CompareScriptStruct(&Initial.PublicState, &NotifiedPublic, 0));
	if (TestNotNull(TEXT("개인 복제 알림 등록"), PrivateOnRep))
	{
		Component->ProcessEvent(PrivateOnRep, nullptr);
	}
	TestEqual(TEXT("개인 OnRep에서 공개 재통지 없음"), PublicNotificationCount, 1);
	TestEqual(TEXT("개인 OnRep는 개인 통지 한 번"), PrivateNotificationCount, 1);
	TestTrue(TEXT("개인 OnRep의 값 일치"), FDigetherLifePrivateState::StaticStruct()->CompareScriptStruct(&Initial.PrivateState, &NotifiedPrivate, 0));
	TestTrue(TEXT("두 OnRep는 게임 상태를 바꾸지 않음"), AreLifeComponentSnapshotsEqual(Initial, Component->CaptureState()));

	Component->OnPublicStateChanged.Remove(PublicHandle);
	Component->OnPrivateStateChanged.Remove(PrivateHandle);
	return true;
}

#endif
