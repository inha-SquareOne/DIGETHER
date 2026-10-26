#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Tools/WorkbenchToolsAlgo.h"

class SEditableTextBox;

// T06: 에디터 PIE와 분리된 별도 프로세스로 N명 멀티플레이 실행을 띄우고, 이 패널이 만든 핸들만 추적·종료한다.
// 준비 완료 신호를 받지 않으므로 상태는 "수동 관찰 필요"이며 접속 성공·M2 통과로 취급하지 않는다.
// 별도로 요청 설정을 복제한 격리 PIE(설정 CDO/저장값 미변경)를 시작·중지할 수 있다.
class SWorkbenchT06MultiRunPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorkbenchT06MultiRunPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SWorkbenchT06MultiRunPanel() override;

private:
	struct FRunningRole
	{
		WorkbenchToolsAlgo::FLaunchRoleSpec Spec;
		FProcHandle Handle;
		WorkbenchToolsAlgo::ERoleRunState State = WorkbenchToolsAlgo::ERoleRunState::NotLaunched;
		uint32 ProcessId = 0;
		FString StartedAtUtc;
		int32 ExitCode = 0;
		bool bExitCodeKnown = false;
	};

	enum class ERunState : uint8
	{
		Idle,
		Running,
		Finished,
		TimedOut,
		Stopped,
		LaunchFailed,
		Aborted, // 일부 역할의 조기 종료·실패, PIE 진입, 엔진 종료로 전체 회수
	};

	FReply OnUseCurrentMapClicked();
	FReply OnStartClicked();
	FReply OnStopClicked();
	FReply OnOpenLogFolderClicked();
	FReply OnStartIsolatedPieClicked();
	FReply OnStopIsolatedPieClicked();

	EActiveTimerReturnType PollProcesses(double InCurrentTime, float InDeltaTime);
	void OnPreBeginPie(bool bIsSimulating);
	void OnEndPie(bool bIsSimulating);
	void FinishRun(ERunState FinalState, const FString& Reason);
	void TerminateOwnedProcesses();
	void ReleaseHandles();
	bool LaunchNextRole();
	void WriteRunResult(const FString& FinalStatus, const FString& Reason);
	TArray<WorkbenchToolsAlgo::FRoleRunRecord> SnapshotRecords() const;
	bool IsRunActive() const;
	bool IsStartEnabled() const;
	bool IsPlayInEditorActive() const;
	FText GetStatusText() const;
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FRunningRole> Role, const TSharedRef<STableViewBase>& OwnerTable);

	WorkbenchToolsAlgo::FMultiRunRequest Request;
	float TimeoutMinutes = 30.f;

	ERunState State = ERunState::Idle;
	FGuid RunId;
	FString LogDir;
	FString RunStartedAtUtc;
	double StartTimeSeconds = 0.0;
	double LastLaunchTimeSeconds = 0.0;
	int32 NextLaunchIndex = 0;
	FString ExecutablePath;
	FString ProjectFilePath;
	TArray<FString> LastErrors;
	FString FailureReason;

	// ActivePanelRuns 증가를 이 패널이 한 번만 되돌리도록 하는 소유 표시
	bool bCountedActive = false;
	// 격리 PIE를 이 패널이 시작했는지(다른 PIE는 건드리지 않는다)
	bool bOwnsIsolatedPie = false;

	TArray<TSharedPtr<FRunningRole>> Roles;
	TSharedPtr<SListView<TSharedPtr<FRunningRole>>> RolesView;
	TSharedPtr<SEditableTextBox> MapBox;
	FDelegateHandle PreExitHandle;
	FDelegateHandle PreBeginPieHandle;
	FDelegateHandle EndPieHandle;

	static int32 ActivePanelRuns;
};
