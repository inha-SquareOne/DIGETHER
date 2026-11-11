// 기존 개발 명령 이전: 이름·동작 유지(-DigetherAutoCmds·다인 검증 스크립트 호환)
// 모두 실행한 프로세스의 로컬 컨트롤러가 서버 RPC를 보내는 방식, 허용 여부는 서버가 판정(IsDevServerRpcAllowed 등)
#include "Commands/DigetherDevCommands.h"

#include "Game/Framework/DigetherGameplayPlayerController.h"

namespace DigetherDev
{
	namespace
	{
		using FControllerAction = TFunction<FString(ADigetherGameplayPlayerController& Controller, const TArray<FString>& Args)>;

		void RegisterLocal(const TCHAR* Name, const TCHAR* Category, const TCHAR* RequestIds, const TCHAR* Summary,
			TArray<FParam> Params, FControllerAction Action, const bool bDestructive = false, const bool bAllowDuringGameOver = false,
			const bool bChangesState = true)
		{
			FCommandDesc Desc;
			Desc.Name = Name;
			Desc.Category = Category;
			Desc.RequestIds = RequestIds;
			Desc.Summary = Summary;
			Desc.Params = MoveTemp(Params);
			Desc.Scope = EScope::Local;
			Desc.bDestructive = bDestructive;
			Desc.bAllowDuringGameOver = bAllowDuringGameOver;
			Desc.bChangesState = bChangesState;
			Register(MoveTemp(Desc), [Action = MoveTemp(Action)](const FContext& Context, const TArray<FString>& Args)
			{
				return FResult::Ok(Action(*Context.GameController, Args));
			});
		}

		const TCHAR* Sent = TEXT("서버에 요청을 보냈습니다. 결과는 로그(LogDigetherFramework)와 화면에서 확인하세요.");
	}

	void RegisterLegacyCommands()
	{
		const TCHAR* Legacy = TEXT("기존 시험");

		RegisterLocal(TEXT("DigetherSave"), TEXT("진행"), TEXT("A-05"), TEXT("현재 진행을 지금 저장한다(호스트만 반영)"), {},
			[](ADigetherGameplayPlayerController& Controller, const TArray<FString>&) { Controller.RequestManualSave(); return FString(Sent); });
		RegisterLocal(TEXT("DigetherLoad"), TEXT("진행"), TEXT(""), TEXT("선택한 슬롯의 저장한 진행을 불러온다(호스트만 반영)"), {},
			[](ADigetherGameplayPlayerController& Controller, const TArray<FString>&) { Controller.RequestLoadGame(); return FString(Sent); },
			/*bDestructive*/ true);
		RegisterLocal(TEXT("DigetherNewGame"), TEXT("진행"), TEXT(""), TEXT("현재 슬롯을 새 게임으로 시작한다(호스트만 반영)"), {},
			[](ADigetherGameplayPlayerController& Controller, const TArray<FString>&) { Controller.RequestNewGame(); return FString(Sent); },
			/*bDestructive*/ true);

		RegisterLocal(TEXT("DigetherLever"), TEXT("엘리베이터"), TEXT(""), TEXT("출발·상승 레버를 당기거나 놓는다(실제 레버 요청과 같은 경로)"),
			{ MakeEnumParam(TEXT("레버"), { TEXT("Departure"), TEXT("Ascent") }),
			  MakeEnumParam(TEXT("동작"), { TEXT("Hold"), TEXT("Release") }),
			  MakeEnumParam(TEXT("위치"), { TEXT("Inside"), TEXT("Outside") }, true) },
			[](ADigetherGameplayPlayerController& Controller, const TArray<FString>& Args) { Controller.DigetherLever(JoinFrom(Args, 0)); return FString(Sent); });
		RegisterLocal(TEXT("DigetherRunSmoke"), Legacy, TEXT(""), TEXT("출발→상승→귀환을 자동으로 운전한다(호스트)"), {},
			[](ADigetherGameplayPlayerController& Controller, const TArray<FString>&) { Controller.DigetherRunSmoke(); return FString(Sent); });
		RegisterLocal(TEXT("DigetherGrantCredits"), TEXT("돈·아이템"), TEXT("D-01"), TEXT("내 잔고에 돈을 더한다"),
			{ MakeRangeParam(TEXT("금액"), EParamType::Int, 1, MAX_int32) },
			[](ADigetherGameplayPlayerController& Controller, const TArray<FString>& Args) { Controller.DigetherGrantCredits(Args[0]); return FString(Sent); });
		RegisterLocal(TEXT("DigetherPour"), Legacy, TEXT(""), TEXT("가장 가까운(또는 지정한) 정산기에 몇 초 동안 붓는다"),
			{ MakeRangeParam(TEXT("초"), EParamType::Float, 0.1, 600), MakeParam(TEXT("정산기 ID"), EParamType::Text, true) },
			[](ADigetherGameplayPlayerController& Controller, const TArray<FString>& Args) { Controller.DigetherPour(JoinFrom(Args, 0)); return FString(Sent); });
		RegisterLocal(TEXT("DigetherPourStop"), Legacy, TEXT(""), TEXT("붓기를 멈춘다"), {},
			[](ADigetherGameplayPlayerController& Controller, const TArray<FString>&) { Controller.DigetherPourStop(); return FString(TEXT("붓기를 멈췄습니다.")); });
		RegisterLocal(TEXT("DigetherSetQuotaProgress"), TEXT("진행"), TEXT("A-02"), TEXT("현재 목표의 납부 인정액을 정한다(목표 미만)"),
			{ MakeRangeParam(TEXT("금액"), EParamType::Int, 0, MAX_int32) },
			[](ADigetherGameplayPlayerController& Controller, const TArray<FString>& Args) { Controller.DigetherSetQuotaProgress(Args[0]); return FString(Sent); });
		RegisterLocal(TEXT("DigetherCreditFastForward"), TEXT("진행"), TEXT("A-04"), TEXT("결과 크레딧 빨리감기(호스트만 반영)"),
			{ MakeParam(TEXT("켜기"), EParamType::Bool) },
			[](ADigetherGameplayPlayerController& Controller, const TArray<FString>& Args)
			{
				Controller.DigetherCreditFastForward(Args[0] == TEXT("1") || Args[0].Equals(TEXT("true"), ESearchCase::IgnoreCase) ? TEXT("1") : TEXT("0"));
				return FString(Sent);
			},
			false, /*bAllowDuringGameOver*/ true);
		RegisterLocal(TEXT("DigetherDumpState"), Legacy, TEXT(""), TEXT("이 프로세스가 받은 복제 상태를 로그로 남긴다"), {},
			[](ADigetherGameplayPlayerController& Controller, const TArray<FString>&) { Controller.DigetherDumpState(); return FString(TEXT("로그(LogDigetherFramework)에 출력했습니다.")); },
			false, true, /*bChangesState*/ false);
		RegisterLocal(TEXT("DigetherTeleportOut"), Legacy, TEXT(""), TEXT("첫 PlayerStart로 이동한다(엘리베이터 밖에 남기기)"), {},
			[](ADigetherGameplayPlayerController& Controller, const TArray<FString>&) { Controller.DigetherTeleportOut(); return FString(Sent); });
		RegisterLocal(TEXT("DigetherTeleportToDeposit"), Legacy, TEXT(""), TEXT("정산기 앞으로 이동한다"),
			{ MakeParam(TEXT("정산기 ID"), EParamType::Text, true, TEXT("GameTestDeposit")) },
			[](ADigetherGameplayPlayerController& Controller, const TArray<FString>& Args) { Controller.DigetherTeleportToDeposit(JoinFrom(Args, 0)); return FString(Sent); });
		RegisterLocal(TEXT("DigetherHubTeleport"), Legacy, TEXT(""), TEXT("거점 시설 앞으로 이동한다"),
			{ MakeParam(TEXT("시설 ID"), EParamType::Text) },
			[](ADigetherGameplayPlayerController& Controller, const TArray<FString>& Args) { Controller.DigetherHubTeleport(JoinFrom(Args, 0)); return FString(Sent); });
		RegisterLocal(TEXT("DigetherHubAction"), Legacy, TEXT(""), TEXT("거점 시설 거래를 요청한다(실제 시설 요청과 같은 경로)"),
			{ MakeParam(TEXT("시설 ID"), EParamType::Text), MakeParam(TEXT("동작"), EParamType::Text, false, TEXT("EDigetherHubAction 이름")),
			  MakeParam(TEXT("선택지 ID"), EParamType::Text, true) },
			[](ADigetherGameplayPlayerController& Controller, const TArray<FString>& Args) { Controller.DigetherHubAction(JoinFrom(Args, 0)); return FString(Sent); });
		RegisterLocal(TEXT("DigetherShowRunResult"), Legacy, TEXT(""), TEXT("마지막 탐사 결과 화면을 다시 연다"), {},
			[](ADigetherGameplayPlayerController& Controller, const TArray<FString>&)
			{
				return Controller.ShowLastRunResult() ? FString(TEXT("탐사 결과 화면을 열었습니다.")) : FString(TEXT("보여 줄 탐사 결과가 없습니다."));
			},
			false, true, /*bChangesState*/ false);
	}

	void RegisterReservedCommands()
	{
		// 선행 시스템이 없어 아직 실행 거절, 웹 화면에 회색으로 표시
		struct FReserved
		{
			const TCHAR* Name;
			const TCHAR* Category;
			const TCHAR* RequestIds;
			const TCHAR* Summary;
			const TCHAR* Reason;
		};
		const FReserved Reserved[] = {
			{ TEXT("DigetherSpawnItem"), TEXT("돈·아이템"), TEXT("D-02"), TEXT("아이템을 ID·수량·가치로 소환한다"), TEXT("인벤토리·픽업 시스템 구현 뒤 연결") },
			{ TEXT("DigetherClearInventory"), TEXT("돈·아이템"), TEXT("D-03"), TEXT("인벤토리를 비운다"), TEXT("인벤토리 구현 뒤 연결") },
			{ TEXT("DigetherGiveWood"), TEXT("돈·아이템"), TEXT("D-03"), TEXT("목재를 지급한다(무료·구매 구분)"), TEXT("인벤토리·목재 구현 뒤 연결") },
			{ TEXT("DigetherResetEmergencyShovel"), TEXT("돈·아이템"), TEXT("D-04"), TEXT("긴급 삽 지원 사용 여부를 초기화한다"), TEXT("긴급 삽 지원 구현 뒤 연결") },
			{ TEXT("DigetherAftershock"), TEXT("탐사"), TEXT("B-04"), TEXT("여진을 즉시 일으킨다"), TEXT("여진의 월드 효과(붕괴·보강) 구현 뒤 연결, 여진 단계는 남은 시간 설정으로 확인") },
			{ TEXT("DigetherSetBurial"), TEXT("탐사"), TEXT("B-05"), TEXT("매몰 단계를 강제한다"), TEXT("매몰 시스템 구현 뒤 연결") },
			{ TEXT("DigetherAdvanceTime"), TEXT("탐사"), TEXT("B-01"), TEXT("남은 탐사 시간을 앞으로 진행한다"), TEXT("탐사 시간 명령 연결 전") },
			{ TEXT("DigetherRerunSettlement"), TEXT("진행"), TEXT(""), TEXT("직전 정산을 같은 스냅샷으로 다시 실행한다"), TEXT("정산 재실행 경로 미구현(요청서 밖 기존 예약)") },
		};
		for (const FReserved& Item : Reserved)
		{
			FCommandDesc Desc;
			Desc.Name = Item.Name;
			Desc.Category = Item.Category;
			Desc.RequestIds = Item.RequestIds;
			Desc.Summary = Item.Summary;
			Desc.UnavailableReason = Item.Reason;
			Desc.Params = { MakeParam(TEXT("인자"), EParamType::Text, true) };
			Register(MoveTemp(Desc), [](const FContext&, const TArray<FString>&) { return FResult::Fail(TEXT("미연결")); });
		}
	}
}
