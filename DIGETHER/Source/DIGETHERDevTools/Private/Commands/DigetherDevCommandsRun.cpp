// 진행·탐사·돈·엘리베이터 개발 명령(호스트), 판정 변경은 DIGETHER 모듈의 개발 진입점만 호출
#include "Commands/DigetherDevCommands.h"

#include "Engine/World.h"
#include "Game/Framework/DigetherGameplayGameMode.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "Game/Hub/DigetherHubCatalog.h"
#include "Game/Hub/DigetherHubStateActor.h"

namespace DigetherDev
{
	namespace
	{
		using FRunModeAction = TFunction<bool(ADigetherGameplayGameMode& Mode, const FContext& Context,
			const TArray<FString>& Args, FString& OutMessage)>;

		// 호스트 판정 명령 공통, 등록부가 호스트·게임 맵·게임 오버를 먼저 검사
		void RegisterRunHost(const TCHAR* Name, const TCHAR* Category, const TCHAR* RequestIds, const TCHAR* Summary,
			TArray<FParam> Params, FRunModeAction Action)
		{
			FCommandDesc Desc;
			Desc.Name = Name;
			Desc.Category = Category;
			Desc.RequestIds = RequestIds;
			Desc.Summary = Summary;
			Desc.Params = MoveTemp(Params);
			Desc.Scope = EScope::Host;
			Register(MoveTemp(Desc), [Action = MoveTemp(Action)](const FContext& Context, const TArray<FString>& Args)
			{
				ADigetherGameplayGameMode* Mode = Context.GetGameMode();
				if (Mode == nullptr)
				{
					return FResult::Fail(TEXT("게임 맵(거점·갱도)에서만 쓸 수 있습니다."));
				}
				FString Message;
				return Action(*Mode, Context, Args, Message) ? FResult::Ok(Message) : FResult::Fail(Message);
			});
		}

		// 대상 플레이어별 실행, 한 명이라도 성공하면 성공·전원 결과를 한 줄로
		bool RunForEachTarget(const FContext& Context, const FString& Token, FString& OutMessage,
			const TFunctionRef<bool(ADigetherGameplayPlayerState* Player, FString& OutLine)> Action)
		{
			TArray<ADigetherGameplayPlayerState*> Players;
			if (!ResolvePlayers(Context, Token, Players, OutMessage))
			{
				return false;
			}
			TArray<FString> Lines;
			int32 Succeeded = 0;
			for (ADigetherGameplayPlayerState* Player : Players)
			{
				FString Line;
				Succeeded += Action(Player, Line) ? 1 : 0;
				Lines.Add(MoveTemp(Line));
			}
			OutMessage = FString::Join(Lines, TEXT(" / "));
			return Succeeded > 0;
		}

		FString DescribeHubResult(const EDigetherHubResult Result)
		{
			switch (Result)
			{
			case EDigetherHubResult::Success: return TEXT("처리했습니다.");
			case EDigetherHubResult::CapacityExceeded: return TEXT("해금 목적지 상한을 넘습니다.");
			case EDigetherHubResult::DestinationLocked: return TEXT("출발 준비 중(목적지 고정)에는 바꿀 수 없습니다.");
			case EDigetherHubResult::StageLocked: return TEXT("해금되지 않은 목적지입니다.");
			default: return TEXT("거점 대기(출발 전) 상태와 목적지 정차 위치(Stage Anchor)를 확인하세요.");
			}
		}

		FString ListStages(const ADigetherHubStateActor& Hub)
		{
			TArray<FString> Names;
			if (IsValid(Hub.Catalog))
			{
				for (const FDigetherHubStage& Stage : Hub.Catalog->Stages)
				{
					Names.Add(FString::Printf(TEXT("%s%s%s"), *Stage.StageId.ToString(),
						Hub.UnlockedStageIds.Contains(Stage.StageId) ? TEXT("(해금)") : TEXT(""),
						Stage.StageId == Hub.SelectedStageId ? TEXT("(선택)") : TEXT("")));
				}
			}
			return Names.IsEmpty() ? FString(TEXT("없음")) : FString::Join(Names, TEXT(", "));
		}

		const TCHAR* NoHub = TEXT("거점 상태(ADigetherHubStateActor)가 맵에 하나 있어야 합니다.");
	}

	void RegisterRunCommands()
	{
		const TCHAR* Progress = TEXT("진행");
		const TCHAR* Explore = TEXT("탐사");

		// --- A-01·A-02 진행 ---
		RegisterRunHost(TEXT("DigetherSetDay"), Progress, TEXT("A-01"),
			TEXT("전체 일차와 기간 일차(1~4)를 정한다. 거점 대기에서만, 목표·납부액 유지. 5일차 실패는 4일차로 두고 미달 출발→귀환으로 재현"),
			{ MakeRangeParam(TEXT("전체 일차"), EParamType::Int, 1, 9999), MakeRangeParam(TEXT("기간 일차"), EParamType::Int, 1, 4) },
			[](ADigetherGameplayGameMode& Mode, const FContext&, const TArray<FString>& Args, FString& OutMessage)
			{
				return Mode.DevSetDay(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]), OutMessage);
			});
		RegisterRunHost(TEXT("DigetherCompleteQuota"), Progress, TEXT("A-02"),
			TEXT("현재 할당량을 즉시 달성한다(붓기 달성과 같은 처리: 다음 목표·기간 1일차·달성 연출·붓기 종료·자동 저장). 납부 접수 중에만"),
			{},
			[](ADigetherGameplayGameMode& Mode, const FContext&, const TArray<FString>&, FString& OutMessage)
			{
				return Mode.DevCompleteQuota(OutMessage);
			});
		RegisterRunHost(TEXT("DigetherSetQuotaTarget"), Progress, TEXT("A-02"),
			TEXT("현재 목표 금액을 바꾼다(현재 납부 인정액보다 커야 함)"),
			{ MakeRangeParam(TEXT("금액"), EParamType::Int, 1, MAX_int32) },
			[](ADigetherGameplayGameMode& Mode, const FContext&, const TArray<FString>& Args, FString& OutMessage)
			{
				return Mode.DevSetQuotaTarget(FCString::Atoi(*Args[0]), OutMessage);
			});

		// --- A-03·B-01 탐사 시간 ---
		RegisterRunHost(TEXT("DigetherEndRun"), Explore, TEXT("A-03"),
			TEXT("탐사를 끝낸다. Timeout: 남은 시간 0으로 실제 시간 종료 경로. Survive: 갱도 바닥 대기에서만, 생존자를 엘리베이터에 태우고 정상 상승을 자동 운전(운행 시간 그대로)"),
			{ MakeEnumParam(TEXT("방식"), { TEXT("Timeout"), TEXT("Survive") }) },
			[](ADigetherGameplayGameMode& Mode, const FContext& Context, const TArray<FString>& Args, FString& OutMessage)
			{
				return Args[0].Equals(TEXT("Survive"), ESearchCase::IgnoreCase)
					? Mode.DevEndRunSurvive(Context.GameController, OutMessage)
					: Mode.DevEndRunTimeout(OutMessage);
			});
		RegisterRunHost(TEXT("DigetherSetRunTime"), Explore, TEXT("B-01"),
			TEXT("남은 탐사 시간을 초 단위로 정한다(갱도 도착 뒤, 0~탐사 시간-상승 페널티). 0이면 시간 종료, 여진 단계도 함께 바뀜"),
			{ MakeRangeParam(TEXT("남은 초"), EParamType::Float, 0, 36000) },
			[](ADigetherGameplayGameMode& Mode, const FContext&, const TArray<FString>& Args, FString& OutMessage)
			{
				return Mode.DevSetRunRemainingSeconds(FCString::Atod(*Args[0]), OutMessage);
			});
		RegisterRunHost(TEXT("DigetherPauseRunTime"), Explore, TEXT("B-01"),
			TEXT("탐사 시간을 멈추거나(1) 다시 흐르게(0) 한다. 이번 탐사에만 적용, 정지 중에도 상승 페널티는 반영"),
			{ MakeParam(TEXT("정지"), EParamType::Bool) },
			[](ADigetherGameplayGameMode& Mode, const FContext&, const TArray<FString>& Args, FString& OutMessage)
			{
				const bool bPause = Args[0] == TEXT("1") || Args[0].Equals(TEXT("true"), ESearchCase::IgnoreCase)
					|| Args[0].Equals(TEXT("on"), ESearchCase::IgnoreCase);
				return Mode.DevSetRunTimePaused(bPause, OutMessage);
			});
		// 예약 명령(사용 불가) 교체
		RegisterRunHost(TEXT("DigetherAdvanceTime"), Explore, TEXT("B-01"),
			TEXT("탐사 시간을 초만큼 앞으로 보낸다(남은 시간에서 뺌, 0 아래면 시간 종료)"),
			{ MakeRangeParam(TEXT("초"), EParamType::Float, 0.1, 36000) },
			[](ADigetherGameplayGameMode& Mode, const FContext&, const TArray<FString>& Args, FString& OutMessage)
			{
				return Mode.DevAdvanceRunTime(FCString::Atod(*Args[0]), OutMessage);
			});

		// --- B-02·B-03 목적지 ---
		RegisterRunHost(TEXT("DigetherSelectStage"), Explore, TEXT("B-02"),
			TEXT("해금·비용을 무시하고 목적지를 고른다(미해금이면 비용 없이 해금). 비우면 목록. 맵 번호 로드는 갱도 맵 선택 로드 구현 뒤"),
			{ MakeParam(TEXT("StageId"), EParamType::Text, true, TEXT("거점 목적지 ID, 비우면 목록")) },
			[](ADigetherGameplayGameMode&, const FContext& Context, const TArray<FString>& Args, FString& OutMessage)
			{
				ADigetherHubStateActor* Hub = ADigetherHubStateActor::Find(Context.World);
				if (!IsValid(Hub))
				{
					OutMessage = NoHub;
					return false;
				}
				const FString StageText = JoinFrom(Args, 0).TrimStartAndEnd();
				if (StageText.IsEmpty())
				{
					OutMessage = FString::Printf(TEXT("목적지: %s"), *ListStages(*Hub));
					return true;
				}
				const FName StageId(*StageText);
				if (!IsValid(Hub->Catalog) || Hub->Catalog->FindStage(StageId) == nullptr)
				{
					OutMessage = FString::Printf(TEXT("목적지 %s가 카탈로그에 없습니다. 목적지: %s"), *StageText, *ListStages(*Hub));
					return false;
				}
				const EDigetherHubResult Result = Hub->DevSelectStage(StageId);
				OutMessage = Result == EDigetherHubResult::Success
					? FString::Printf(TEXT("목적지 %s 선택(비용 없음). 다음 자동 저장에 반영됩니다."), *StageText)
					: DescribeHubResult(Result);
				return Result == EDigetherHubResult::Success;
			});
		RegisterRunHost(TEXT("DigetherUnlockAllStages"), Explore, TEXT("B-03"),
			TEXT("카탈로그의 모든 목적지를 비용 없이 해금한다(거점 대기에서만, 저장에 반영)"),
			{},
			[](ADigetherGameplayGameMode&, const FContext& Context, const TArray<FString>&, FString& OutMessage)
			{
				ADigetherHubStateActor* Hub = ADigetherHubStateActor::Find(Context.World);
				if (!IsValid(Hub))
				{
					OutMessage = NoHub;
					return false;
				}
				int32 Newly = 0;
				const EDigetherHubResult Result = Hub->DevUnlockAllStages(Newly);
				OutMessage = Result == EDigetherHubResult::Success
					? FString::Printf(TEXT("새로 해금 %d개. 목적지: %s"), Newly, *ListStages(*Hub))
					: DescribeHubResult(Result);
				return Result == EDigetherHubResult::Success;
			});

		// --- D-01 돈 ---
		RegisterRunHost(TEXT("DigetherSetBalance"), TEXT("돈·아이템"), TEXT("D-01"),
			TEXT("잔고를 금액으로 정한다(더하기가 아닌 절대값). 잔고는 본인에게만 복제되므로 호스트는 상태 패널로 확인"),
			{ MakeRangeParam(TEXT("금액"), EParamType::Int, 0, MAX_int32),
			  MakeParam(TEXT("대상"), EParamType::Player, true, TEXT("self·all·플레이어 번호")) },
			[](ADigetherGameplayGameMode& Mode, const FContext& Context, const TArray<FString>& Args, FString& OutMessage)
			{
				const int32 Credits = FCString::Atoi(*Args[0]);
				return RunForEachTarget(Context, ArgOr(Args, 1, TEXT("self")), OutMessage,
					[&Mode, Credits](ADigetherGameplayPlayerState* Player, FString& OutLine)
					{
						return Mode.DevSetBalance(Player, Credits, OutLine);
					});
			});
	}
}
