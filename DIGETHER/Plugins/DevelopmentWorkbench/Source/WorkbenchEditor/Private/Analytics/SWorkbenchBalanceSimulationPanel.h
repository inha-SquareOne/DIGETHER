// T03 합성 잠재가치 분석 패널. 실게임 provider와 연결되지 않은 합성 시뮬레이션이며 실게임 수익이 아니다.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Analytics/WorkbenchBalanceSimulation.h"

class STextBlock;
class SMultiLineEditableTextBox;
class FActiveTimerHandle;

class SWorkbenchBalanceSimulationPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorkbenchBalanceSimulationPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedPtr<SMultiLineEditableTextBox> PoolCsvA;
	TSharedPtr<SMultiLineEditableTextBox> PoolCsvB;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<STextBlock> ResultText;

	int32 MasterSeed = 1000;
	int32 NumEpisodes = 200;
	int32 DrawsPerEpisode = 10;
	bool bCompareB = false;

	enum class EPhase : uint8 { Idle, SimA, SimB, BootA, BootB, BootDiff };

	// 실행 상태. 실행·취소마다 Generation을 올려 이전 타이머 틱이 새 상태를 건드리지 못하게 한다.
	uint32 Generation = 0;
	bool bRunning = false;
	EPhase Phase = EPhase::Idle;
	WorkbenchAnalytics::FBalanceSimRunState RunState;
	WorkbenchAnalytics::FBalanceSimBootstrapState BootState;
	WorkbenchAnalytics::FBalanceSimScenarioConfig PendingConfigB;
	bool bHasPendingB = false;
	TOptional<WorkbenchAnalytics::FBalanceSimScenarioResult> ResultA;
	TOptional<WorkbenchAnalytics::FBalanceSimScenarioResult> ResultB;
	WorkbenchAnalytics::FBalanceSimBootstrapResult DiffCi;
	FString LastExportNote;

	// 실행 시점 입력 스냅샷. 현재 입력 서명과 다르면 결과가 오래된 것으로 표시한다.
	FString RunSignature;
	FString RunSnapshotNote;

	FReply OnLoadSampleClicked();
	FReply OnRunClicked();
	FReply OnCancelClicked();
	FReply OnExportClicked(bool bJson);

	EActiveTimerReturnType TickRun(double InCurrentTime, float InDeltaTime, uint32 InGeneration);
	void FinishPhase();
	void BeginBootPhase(EPhase Next);
	void FinishBootPhase();
	void MarkBootstrapSkipped(EPhase From, const FString& Reason);
	FString BuildInputSignature() const;
	void ShowReport();
	void SetOutput(const FString& Status, const FString& Detail);

	static bool BuildConfig(const FString& CsvText, const FString& Label, int32 Seed, int32 Draws, int32 Episodes,
		WorkbenchAnalytics::FBalanceSimScenarioConfig& OutConfig, FString& OutError);
	static FString BuildScenarioReport(const FString& Title, const WorkbenchAnalytics::FBalanceSimScenarioResult& Result);
};
