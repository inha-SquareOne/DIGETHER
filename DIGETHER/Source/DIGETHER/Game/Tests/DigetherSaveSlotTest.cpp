#if WITH_DEV_AUTOMATION_TESTS

// 아래 숫자와 ID는 테스트 픽스처, 기획 값 아님.

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/Save/DigetherSaveRules.h"
#include "Game/Save/DigetherSaveStorage.h"
#include "Game/Save/DigetherSaveSubsystem.h"
#include "Game/Save/DigetherSaveTuning.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/SecureHash.h"
#include "OnlineSubsystemTypes.h"

namespace
{
	constexpr int32 SlotTestUserIndex = 0;
	const FName SlotTestProvider(TEXT("SAVE_SLOT_TEST"));

	FDigetherSaveSnapshot MakeSlotTestSnapshot(const FString& HostKey, const FGuid& GameId, const int32 PeriodDay,
		const int32 TotalDay)
	{
		// 모든 수치·ID는 저장 계약 검사용 픽스처이며 최종 기획 값이 아니다.
		FDigetherSaveSnapshot Snapshot;
		Snapshot.HostAuthenticatedPlayerKey = HostKey;
		Snapshot.GameId = GameId;
		Snapshot.RunId = FGuid::NewGuid();
		Snapshot.Quota.Quota.Day = PeriodDay;
		Snapshot.Quota.Quota.TotalDay = TotalDay;
		Snapshot.Quota.Quota.TargetCredits = 300;
		Snapshot.Quota.Quota.DepositedCredits = 120;
		Snapshot.Quota.Quota.PeriodGeneration = 2;
		Snapshot.RunTuning.SourceProfile = FPrimaryAssetId(TEXT("DigetherTuningProfile:DA_SaveSlotFixture"));
		Snapshot.RunTuning.ProfileVersion = 1;
		Snapshot.RunTuning.InitialQuotaCredits = 100;
		Snapshot.RunTuning.RunSeconds = 120.f;
		Snapshot.RunTuning.InteractionDistanceCm = 200.f;
		Snapshot.RunTuning.ElevatorCapacityKg = 500.f;
		Snapshot.RunTuning.ElevatorTripLimit = 2;
		Snapshot.RunTuning.AscentPenaltySeconds = 10.f;
		Snapshot.RunTuning.ElevatorTravelSeconds = 2.f;
		Snapshot.RunTuning.DepositIntervalSeconds = 0.5f;
		Snapshot.RunTuning.EmergencyWeightLimitKg = 3.f;
		Snapshot.RunTuning.DeathLossPercent = 100;
		Snapshot.RunTuning.CorpseRefundPercent = 50;
		FDigetherSavePlayerRecord Host;
		Host.AuthenticatedPlayerKey = HostKey;
		Host.GameplayPlayerId = FGuid::NewGuid();
		Host.Wallet.BalanceCredits = 70;
		Snapshot.Players.Add(Host);
		return Snapshot;
	}

	// 실제 GameInstance·독립 실행 월드에서 저장 서브시스템을 만든다. 디스크 슬롯은 고유 계정 이름으로 격리한다.
	class FDigetherSaveSlotTestInstance
	{
	public:
		explicit FDigetherSaveSlotTestInstance(FAutomationTestBase& InTest) : Test(InTest)
		{
			if (GEngine == nullptr)
			{
				Test.AddError(TEXT("저장 슬롯 서브시스템 시험에 GEngine이 필요합니다."));
				return;
			}
			Instance = NewObject<UGameInstance>(GEngine);
			Instance->AddToRoot();
			Instance->InitializeStandalone();
			Save = Instance->GetSubsystem<UDigetherSaveSubsystem>();
			HostId = FUniqueNetIdRepl(FUniqueNetIdRef(FUniqueNetIdString::Create(FGuid::NewGuid().ToString(), SlotTestProvider)));
			DigetherSaveRules::MakeAuthenticatedPlayerKey(HostId, HostKey);
		}

		~FDigetherSaveSlotTestInstance()
		{
			if (!HostKey.IsEmpty())
			{
				for (int32 SlotIndex = 0; SlotIndex < DigetherSaveTuning::MaxProgressSlots; ++SlotIndex)
				{
					const FDigetherSaveSlotNames Names = DigetherSaveStorage::MakeSlotNames(HostKey, SlotIndex);
					for (const FString& SlotName : { Names.Primary, Names.Backup })
					{
						if (UGameplayStatics::DoesSaveGameExist(SlotName, SlotTestUserIndex))
						{
							Test.TestTrue(TEXT("슬롯 시험 임시 파일 삭제"),
								UGameplayStatics::DeleteGameInSlot(SlotName, SlotTestUserIndex));
						}
					}
				}
			}
			if (Instance != nullptr)
			{
				Instance->Shutdown();
				Instance->RemoveFromRoot();
			}
		}

		FDigetherSaveSlotTestInstance(const FDigetherSaveSlotTestInstance&) = delete;
		FDigetherSaveSlotTestInstance& operator=(const FDigetherSaveSlotTestInstance&) = delete;

		bool IsReady() const
		{
			const UWorld* World = Instance != nullptr ? Instance->GetWorld() : nullptr;
			return IsValid(Save) && World != nullptr && World->GetNetMode() == NM_Standalone && !HostKey.IsEmpty();
		}

		bool AreSlotFilesAbsent(const int32 SlotIndex) const
		{
			const FDigetherSaveSlotNames Names = DigetherSaveStorage::MakeSlotNames(HostKey, SlotIndex);
			return !UGameplayStatics::DoesSaveGameExist(Names.Primary, SlotTestUserIndex)
				&& !UGameplayStatics::DoesSaveGameExist(Names.Backup, SlotTestUserIndex);
		}

		UDigetherSaveSubsystem* Save = nullptr;
		FUniqueNetIdRepl HostId;
		FString HostKey;

	private:
		FAutomationTestBase& Test;
		UGameInstance* Instance = nullptr;
	};

	const FDigetherSaveSlotSummary* FindSummary(const TArray<FDigetherSaveSlotSummary>& Summaries, const int32 SlotIndex)
	{
		return Summaries.FindByPredicate([SlotIndex](const FDigetherSaveSlotSummary& Entry)
		{
			return Entry.SlotIndex == SlotIndex;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSaveSlotNamesTest,
	"DIGETHER.Game.Save.Slots.NamesKeepLegacySlotZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSaveSlotNamesTest::RunTest(const FString& Parameters)
{
	const FString HostKey = TEXT("SAVE_SLOT_TEST:host");
	// 슬롯 도입 전 이름 규칙을 이 테스트 안에서 독립으로 다시 만든다(저장 코드의 상수를 쓰지 않는다).
	const FTCHARToUTF8 HostBytes(*HostKey);
	const FString LegacyPrimary = TEXT("Digether_HostProgress_v3_")
		+ FSHA1::HashBuffer(HostBytes.Get(), HostBytes.Length()).ToString();
	const FDigetherSaveSlotNames Slot0 = DigetherSaveStorage::MakeSlotNames(HostKey, 0);
	TestEqual(TEXT("슬롯 0 주 이름은 슬롯 도입 전 이름과 같음"), Slot0.Primary, LegacyPrimary);
	TestEqual(TEXT("슬롯 0 백업 이름은 슬롯 도입 전 이름과 같음"), Slot0.Backup, LegacyPrimary + TEXT("_Backup"));
	const FDigetherSaveSlotNames Default = DigetherSaveStorage::MakeSlotNames(HostKey);
	TestEqual(TEXT("번호를 생략하면 슬롯 0"), Default.Primary, Slot0.Primary);

	TSet<FString> AllNames;
	for (int32 SlotIndex = 0; SlotIndex < DigetherSaveTuning::MaxProgressSlots; ++SlotIndex)
	{
		const FDigetherSaveSlotNames Names = DigetherSaveStorage::MakeSlotNames(HostKey, SlotIndex);
		TestFalse(FString::Printf(TEXT("슬롯 %d 이름 생성"), SlotIndex), Names.Primary.IsEmpty() || Names.Backup.IsEmpty());
		TestTrue(FString::Printf(TEXT("슬롯 %d 이름은 같은 호스트 접두사"), SlotIndex), Names.Primary.StartsWith(LegacyPrimary));
		TestFalse(FString::Printf(TEXT("슬롯 %d 이름은 인증 ID 원문을 노출하지 않음"), SlotIndex), Names.Primary.Contains(HostKey, ESearchCase::CaseSensitive));
		AllNames.Add(Names.Primary);
		AllNames.Add(Names.Backup);
	}
	TestEqual(TEXT("모든 슬롯의 주·백업 이름이 서로 다름"), AllNames.Num(), DigetherSaveTuning::MaxProgressSlots * 2);
	TestEqual(TEXT("슬롯 1 이름"), DigetherSaveStorage::MakeSlotNames(HostKey, 1).Primary, LegacyPrimary + TEXT("_S1"));

	for (const int32 Invalid : { -1, DigetherSaveTuning::MaxProgressSlots })
	{
		const FDigetherSaveSlotNames Names = DigetherSaveStorage::MakeSlotNames(HostKey, Invalid);
		TestTrue(FString::Printf(TEXT("범위 밖 슬롯 %d는 빈 이름"), Invalid), Names.Primary.IsEmpty() && Names.Backup.IsEmpty());
		FDigetherSaveSlotData Output;
		TestTrue(TEXT("빈 이름 읽기는 InvalidState"), DigetherSaveStorage::ReadSlot(Names.Primary, HostKey, Output)
			== EDigetherSaveResult::InvalidState);
	}
	// 이름 버전은 슬롯 도입 전 파일을 계속 찾도록 3으로 고정한다(스키마·wire 버전과 별개).
	TestEqual(TEXT("이름 버전은 진행 스키마 버전과 별개 상수"), DigetherSaveTuning::SlotNameVersion, 3);
	TestTrue(TEXT("슬롯 1개 이상"), DigetherSaveTuning::MaxProgressSlots >= 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSaveSlotRulesTest,
	"DIGETHER.Game.Save.Slots.SummaryParseAndStartupChoice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSaveSlotRulesTest::RunTest(const FString& Parameters)
{
	FDigetherSaveSlotData Slot;
	Slot.Progress = MakeSlotTestSnapshot(TEXT("SAVE_SLOT_TEST:host"), FGuid(1, 2, 3, 4), 3, 10);
	Slot.SavedAtUtcTicks = FDateTime(2026, 11, 5, 9, 0, 0).GetTicks();
	const FDigetherSaveSlotSummary Ready = DigetherSaveRules::MakeSlotSummary(1, EDigetherSaveResult::Success, Slot);
	TestEqual(TEXT("요약 슬롯 번호"), Ready.SlotIndex, 1);
	TestEqual(TEXT("읽을 수 있는 슬롯"), Ready.Status, EDigetherSaveSlotStatus::Ready);
	TestEqual(TEXT("요약 전체 일차"), Ready.TotalDay, 10);
	TestEqual(TEXT("요약 기간 일차"), Ready.PeriodDay, 3);
	TestEqual(TEXT("요약 목표"), Ready.TargetCredits, 300);
	TestEqual(TEXT("요약 달성 기간 수"), Ready.CompletedPeriods, 2);
	TestEqual(TEXT("요약 참가 기록 수"), Ready.PlayerRecordCount, 1);
	TestTrue(TEXT("요약 저장 시각 있음"), Ready.bHasSavedAt && Ready.SavedAtUtc == FDateTime(2026, 11, 5, 9, 0, 0));
	TestFalse(TEXT("요약 게임 오버 아님"), Ready.bGameOver);

	FDigetherSaveSlotData Legacy = Slot;
	Legacy.SavedAtUtcTicks = 0;
	TestFalse(TEXT("v5 이전 파일은 저장 시각 모름"),
		DigetherSaveRules::MakeSlotSummary(0, EDigetherSaveResult::Success, Legacy).bHasSavedAt);

	const struct
	{
		EDigetherSaveResult Read;
		EDigetherSaveSlotStatus Expected;
	} StatusCases[] = {
		{ EDigetherSaveResult::NotFound, EDigetherSaveSlotStatus::Empty },
		{ EDigetherSaveResult::UnsupportedVersion, EDigetherSaveSlotStatus::FutureVersion },
		{ EDigetherSaveResult::CorruptData, EDigetherSaveSlotStatus::Unreadable },
		{ EDigetherSaveResult::HostIdentityMismatch, EDigetherSaveSlotStatus::Unreadable },
		{ EDigetherSaveResult::IoFailure, EDigetherSaveSlotStatus::Unreadable },
		{ EDigetherSaveResult::HostIdentityMissing, EDigetherSaveSlotStatus::IdentityUnavailable }
	};
	for (const auto& Case : StatusCases)
	{
		const FDigetherSaveSlotSummary Summary = DigetherSaveRules::MakeSlotSummary(2, Case.Read, Slot);
		TestEqual(FString::Printf(TEXT("읽기 결과 %s의 상태"), LexToString(Case.Read)), Summary.Status, Case.Expected);
		// 읽지 못한 슬롯에서는 다른 슬롯 값이 새지 않게 요약 값을 비운다.
		TestTrue(FString::Printf(TEXT("읽기 결과 %s는 요약 값 없음"), LexToString(Case.Read)),
			Summary.TotalDay == 0 && Summary.PeriodDay == 0 && Summary.TargetCredits == 0 && !Summary.bHasSavedAt);
	}

	FDigetherSaveSlotSelection Continue;
	FDigetherSaveSlotSelection NewGame;
	NewGame.StartMode = EDigetherSaveStartMode::NewGame;
	TestFalse(TEXT("기본 선택(이어하기)은 불러오기"), DigetherSaveRules::ShouldStartNewGame(Continue, false));
	TestTrue(TEXT("새 게임 선택은 새 게임"), DigetherSaveRules::ShouldStartNewGame(NewGame, false));
	TestTrue(TEXT("개발 URL 옵션은 선택과 무관하게 새 게임"), DigetherSaveRules::ShouldStartNewGame(Continue, true));
	TestTrue(TEXT("둘 다면 새 게임"), DigetherSaveRules::ShouldStartNewGame(NewGame, true));

	const struct
	{
		const TCHAR* Option;
		bool bAccepted;
		int32 SlotIndex;
		EDigetherSaveStartMode Mode;
	} ParseCases[] = {
		{ TEXT(""), true, 0, EDigetherSaveStartMode::Continue },
		{ TEXT("  "), true, 0, EDigetherSaveStartMode::Continue },
		{ TEXT("new"), true, 0, EDigetherSaveStartMode::NewGame },
		{ TEXT("NEW2"), true, 2, EDigetherSaveStartMode::NewGame },
		{ TEXT("load"), true, 0, EDigetherSaveStartMode::Continue },
		{ TEXT("load1"), true, 1, EDigetherSaveStartMode::Continue },
		{ TEXT("new3"), false, 0, EDigetherSaveStartMode::Continue },
		{ TEXT("new-1"), false, 0, EDigetherSaveStartMode::Continue },
		{ TEXT("new1a"), false, 0, EDigetherSaveStartMode::Continue },
		{ TEXT("save1"), false, 0, EDigetherSaveStartMode::Continue }
	};
	for (const auto& Case : ParseCases)
	{
		FDigetherSaveSlotSelection Output;
		Output.SlotIndex = 77;
		const bool bAccepted = DigetherSaveRules::TryParseSlotOption(Case.Option, Output);
		TestEqual(FString::Printf(TEXT("옵션 '%s' 수락 여부"), Case.Option), bAccepted, Case.bAccepted);
		if (Case.bAccepted)
		{
			TestEqual(FString::Printf(TEXT("옵션 '%s' 슬롯"), Case.Option), Output.SlotIndex, Case.SlotIndex);
			TestEqual(FString::Printf(TEXT("옵션 '%s' 시작 방식"), Case.Option), Output.StartMode, Case.Mode);
		}
		else
		{
			TestEqual(FString::Printf(TEXT("옵션 '%s' 거절 시 출력 유지"), Case.Option), Output.SlotIndex, 77);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigetherSaveSlotSubsystemTest,
	"DIGETHER.Game.Save.Slots.SubsystemWritesOnlySelectedSlotAndLists",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDigetherSaveSlotSubsystemTest::RunTest(const FString& Parameters)
{
	FDigetherSaveSlotTestInstance Fixture(*this);
	if (!TestTrue(TEXT("독립 실행 GameInstance와 저장 서브시스템"), Fixture.IsReady()))
	{
		return false;
	}
	UDigetherSaveSubsystem* Save = Fixture.Save;
	for (int32 SlotIndex = 0; SlotIndex < DigetherSaveTuning::MaxProgressSlots; ++SlotIndex)
	{
		if (!TestTrue(TEXT("고유 계정의 슬롯은 시험 전 없음"), Fixture.AreSlotFilesAbsent(SlotIndex)))
		{
			return false;
		}
	}
	if (!TestEqual(TEXT("호스트 계정 바인딩"), Save->BindHostIdentity(Fixture.HostId), EDigetherSaveResult::Success))
	{
		return false;
	}
	TArray<FDigetherSaveSlotSummary> Summaries = Save->ListSlots();
	if (!TestEqual(TEXT("슬롯 수만큼 요약"), Summaries.Num(), DigetherSaveTuning::MaxProgressSlots))
	{
		return false;
	}
	for (const FDigetherSaveSlotSummary& Summary : Summaries)
	{
		TestEqual(TEXT("새 계정의 모든 슬롯은 비어 있음"), Summary.Status, EDigetherSaveSlotStatus::Empty);
	}

	// 슬롯 1에 진행을 쓴다. 다른 슬롯은 파일이 생기지 않는다.
	const FGuid GameA = FGuid::NewGuid();
	FDigetherSaveSlotSelection SlotOne;
	SlotOne.SlotIndex = 1;
	if (!TestTrue(TEXT("슬롯 1 선택"), Save->SetSlotSelection(SlotOne))
		|| !TestEqual(TEXT("슬롯 1 진행 저장"), Save->Write(MakeSlotTestSnapshot(Fixture.HostKey, GameA, 2, 5), false),
			EDigetherSaveResult::Success))
	{
		return false;
	}
	TestTrue(TEXT("슬롯 0 파일 없음"), Fixture.AreSlotFilesAbsent(0));
	TestTrue(TEXT("슬롯 2 파일 없음"), Fixture.AreSlotFilesAbsent(2));
	Summaries = Save->ListSlots();
	const FDigetherSaveSlotSummary* One = FindSummary(Summaries, 1);
	if (TestNotNull(TEXT("슬롯 1 요약"), One))
	{
		TestEqual(TEXT("슬롯 1 읽을 수 있음"), One->Status, EDigetherSaveSlotStatus::Ready);
		TestEqual(TEXT("슬롯 1 전체 일차"), One->TotalDay, 5);
		TestEqual(TEXT("슬롯 1 기간 일차"), One->PeriodDay, 2);
		TestTrue(TEXT("슬롯 1 저장 시각 기록"), One->bHasSavedAt);
	}
	TestEqual(TEXT("슬롯 0은 계속 비어 있음"), FindSummary(Summaries, 0)->Status, EDigetherSaveSlotStatus::Empty);

	FDigetherSaveSnapshot Loaded;
	TestTrue(TEXT("선택 슬롯 1에서 읽기"), Save->Read(Loaded) == EDigetherSaveResult::Success
		&& Loaded.GameId == GameA && Loaded.Quota.Quota.TotalDay == 5);
	FDigetherSaveSlotSelection SlotZero;
	TestTrue(TEXT("슬롯 0 선택"), Save->SetSlotSelection(SlotZero));
	TestEqual(TEXT("슬롯 0 읽기는 없음"), Save->Read(Loaded), EDigetherSaveResult::NotFound);

	// 슬롯 2 새 게임은 슬롯 1을 지우거나 덮지 않는다.
	FDigetherSaveSlotSelection SlotTwoNew;
	SlotTwoNew.SlotIndex = 2;
	SlotTwoNew.StartMode = EDigetherSaveStartMode::NewGame;
	const FGuid GameB = FGuid::NewGuid();
	TestTrue(TEXT("슬롯 2 새 게임 선택"), Save->SetSlotSelection(SlotTwoNew));
	TestEqual(TEXT("슬롯 2 새 게임 저장"), Save->Write(MakeSlotTestSnapshot(Fixture.HostKey, GameB, 1, 1), true),
		EDigetherSaveResult::Success);
	Save->CompleteNewGameRequest();
	TestEqual(TEXT("새 게임 완료 뒤 같은 슬롯"), Save->GetSlotSelection().SlotIndex, 2);
	TestEqual(TEXT("새 게임 완료 뒤 이어하기"), Save->GetSlotSelection().StartMode, EDigetherSaveStartMode::Continue);

	// 크레딧 뒤 새 게임(새 게임 ID, 명시 새 게임 쓰기)을 슬롯 1에 쓰면 슬롯 1만 바뀐다(S30 208행·S33 240행·T38).
	TestTrue(TEXT("슬롯 1 다시 선택"), Save->SetSlotSelection(SlotOne));
	const FGuid GameAfterCredits = FGuid::NewGuid();
	TestEqual(TEXT("같은 게임 ID가 아닌 새 게임은 일반 쓰기로 덮지 않음"),
		Save->Write(MakeSlotTestSnapshot(Fixture.HostKey, GameAfterCredits, 1, 1), false), EDigetherSaveResult::InvalidState);
	TestEqual(TEXT("슬롯 1 크레딧 뒤 새 게임 저장"),
		Save->Write(MakeSlotTestSnapshot(Fixture.HostKey, GameAfterCredits, 1, 1), true), EDigetherSaveResult::Success);
	Summaries = Save->ListSlots();
	TestEqual(TEXT("슬롯 1은 새 게임 전체 1일차"), FindSummary(Summaries, 1)->TotalDay, 1);
	FDigetherSaveSnapshot AfterCredits;
	TestTrue(TEXT("슬롯 1은 새 게임 ID 하나"), Save->Read(AfterCredits) == EDigetherSaveResult::Success
		&& AfterCredits.GameId == GameAfterCredits && AfterCredits.GameId != GameA);
	TestEqual(TEXT("슬롯 2 새 게임은 보존"), FindSummary(Summaries, 2)->Status, EDigetherSaveSlotStatus::Ready);
	TestEqual(TEXT("슬롯 0은 끝까지 비어 있음"), FindSummary(Summaries, 0)->Status, EDigetherSaveSlotStatus::Empty);

	// 슬롯 1의 다른 게임 ID 일반 저장은 기존 진행을 덮지 않는다(새 게임 선택 없이 다른 진행을 쓰지 않음).
	TestEqual(TEXT("다른 게임의 일반 저장 거절"),
		Save->Write(MakeSlotTestSnapshot(Fixture.HostKey, GameB, 2, 2), false), EDigetherSaveResult::InvalidState);
	return true;
}

#endif
