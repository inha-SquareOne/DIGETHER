#include "Tools/SWorkbenchT06MultiRunPanel.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "IPAddress.h"
#include "Internationalization/Text.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "WorkbenchT06"

using namespace WorkbenchToolsAlgo;

int32 SWorkbenchT06MultiRunPanel::ActivePanelRuns = 0;

namespace
{
	constexpr double LaunchStaggerSeconds = 5.0;

	enum class EPortProbe : uint8 { Free, InUse, Unknown };

	// UDP 바인드 시도로 포트 점유를 확인한다. 최선 노력이며 TCP나 이후 경쟁 상태는 보장하지 않는다.
	EPortProbe ProbeUdpPort(int32 Port)
	{
		ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (!Sockets)
		{
			return EPortProbe::Unknown;
		}
		FSocket* Socket = Sockets->CreateSocket(NAME_DGram, TEXT("WorkbenchT06PortProbe"), FName(NAME_None));
		if (!Socket)
		{
			return EPortProbe::Unknown;
		}
		TSharedRef<FInternetAddr> Addr = Sockets->CreateInternetAddr();
		Addr->SetAnyAddress();
		Addr->SetPort(Port);
		const bool bBound = Socket->Bind(*Addr);
		Socket->Close();
		Sockets->DestroySocket(Socket);
		return bBound ? EPortProbe::Free : EPortProbe::InUse;
	}

	bool IsWorldPackage(const FString& PackagePath)
	{
		if (!FPackageName::IsValidLongPackageName(PackagePath))
		{
			return false;
		}
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPackageName(FName(*PackagePath), Assets);
		const FTopLevelAssetPath WorldClass = UWorld::StaticClass()->GetClassPathName();
		for (const FAssetData& Asset : Assets)
		{
			if (Asset.AssetClassPath == WorldClass)
			{
				return true;
			}
		}
		return false;
	}

	TSharedRef<SWidget> MakeRadio(const FText& Label, TFunction<bool()> IsChecked, TFunction<void()> OnSelect)
	{
		return SNew(SCheckBox)
			.Style(&FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("RadioButton"))
			.IsChecked_Lambda([IsChecked]() { return IsChecked() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([OnSelect](ECheckBoxState State) { if (State == ECheckBoxState::Checked) { OnSelect(); } })
			[
				SNew(STextBlock).Text(Label)
			];
	}

	TSharedRef<SWidget> MakeSpin(const FText& Label, float Value, float Min, float Max, TFunction<void(float)> Setter)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)[SNew(STextBlock).Text(Label)]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(80.f)
				[
					SNew(SSpinBox<float>).MinValue(Min).MaxValue(Max).Value(Value)
					.OnValueChanged_Lambda([Setter](float V) { Setter(V); })
				]
			];
	}
}

void SWorkbenchT06MultiRunPanel::Construct(const FArguments& InArgs)
{
	ExecutablePath = FPlatformProcess::ExecutablePath();
	ProjectFilePath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	PreExitHandle = FCoreDelegates::OnEnginePreExit.AddRaw(this, &SWorkbenchT06MultiRunPanel::TerminateOwnedProcesses);
	PreBeginPieHandle = FEditorDelegates::PreBeginPIE.AddRaw(this, &SWorkbenchT06MultiRunPanel::OnPreBeginPie);
	EndPieHandle = FEditorDelegates::EndPIE.AddRaw(this, &SWorkbenchT06MultiRunPanel::OnEndPie);
	RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateSP(this, &SWorkbenchT06MultiRunPanel::PollProcesses));

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8)
		[
			SNew(STextBlock).AutoWrapText(true)
			.Text(LOCTEXT("Intro", "에디터 PIE와 분리된 별도 프로세스로 N명 실행을 띄운다. 리슨/전용 서버 선택은 시험 실행용 선택지이며 게임의 네트워크 방식은 아직 미확정이다. 저장하지 않은 에디터 변경은 새 프로세스에 반영되지 않는다. 이 도구는 접속 성공이나 준비 완료를 감지하지 않으며 M2 통과를 판정하지 않는다."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SAssignNew(MapBox, SEditableTextBox).HintText(LOCTEXT("MapHint", "/Game/Maps/맵이름 (패키지 경로)"))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0)
			[
				SNew(SButton).Text(LOCTEXT("UseCurrent", "현재 맵 사용")).OnClicked(this, &SWorkbenchT06MultiRunPanel::OnUseCurrentMapClicked)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)[SNew(STextBlock).Text(LOCTEXT("Players", "총 인원(1~4)"))]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(60.f)
					[
						SNew(SSpinBox<int32>).MinValue(1).MaxValue(4).Value(Request.PlayerCount)
						.OnValueChanged_Lambda([this](int32 V) { Request.PlayerCount = V; })
					]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
			[MakeRadio(LOCTEXT("Listen", "리슨(호스트 1 + 클라 N-1)"), [this]() { return !Request.bDedicated; }, [this]() { Request.bDedicated = false; })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
			[MakeRadio(LOCTEXT("Dedicated", "전용(서버 1 + 클라 N)"), [this]() { return Request.bDedicated; }, [this]() { Request.bDedicated = true; })]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)[SNew(STextBlock).Text(LOCTEXT("Port", "포트"))]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(80.f)
					[
						SNew(SSpinBox<int32>).MinValue(1024).MaxValue(65535).Value(Request.Port)
						.OnValueChanged_Lambda([this](int32 V) { Request.Port = V; })
					]
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)[SNew(STextBlock).Text(LOCTEXT("EmuTarget", "네트워크 에뮬레이션 대상"))]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
			[MakeRadio(LOCTEXT("EmuServer", "서버/호스트"), [this]() { return Request.EmulationTarget == ENetEmulationTarget::Server; }, [this]() { Request.EmulationTarget = ENetEmulationTarget::Server; })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
			[MakeRadio(LOCTEXT("EmuClients", "클라이언트"), [this]() { return Request.EmulationTarget == ENetEmulationTarget::Clients; }, [this]() { Request.EmulationTarget = ENetEmulationTarget::Clients; })]
			+ SHorizontalBox::Slot().AutoWidth()
			[MakeRadio(LOCTEXT("EmuAll", "전체"), [this]() { return Request.EmulationTarget == ENetEmulationTarget::Everyone; }, [this]() { Request.EmulationTarget = ENetEmulationTarget::Everyone; })]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)[MakeSpin(LOCTEXT("LatMin", "지연 최소(ms)"), Request.MinLatencyMs, 0.f, 5000.f, [this](float V) { Request.MinLatencyMs = V; })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)[MakeSpin(LOCTEXT("LatMax", "최대(ms)"), Request.MaxLatencyMs, 0.f, 5000.f, [this](float V) { Request.MaxLatencyMs = V; })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)[MakeSpin(LOCTEXT("Loss", "패킷 손실(%)"), Request.PacketLossPercent, 0.f, 100.f, [this](float V) { Request.PacketLossPercent = V; })]
			+ SHorizontalBox::Slot().AutoWidth()[MakeSpin(LOCTEXT("Timeout", "자동 종료(분)"), TimeoutMinutes, 1.f, 240.f, [this](float V) { TimeoutMinutes = V; })]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 2)
		[
			SNew(STextBlock).AutoWrapText(true)
			.Text(LOCTEXT("EmuNote", "지연·손실은 엔진 명령줄(-PktLagMin/-PktLagMax/-PktLoss)로 각 프로세스의 '보내는' 패킷에 적용된다. 받는 방향은 상대 프로세스의 설정이 결정하므로 '전체'가 아니면 비대칭이다. 리슨 호스트의 로컬 플레이어는 네트워크를 거치지 않아 호스트 자신의 체감에는 적용되지 않는다. 에뮬레이션 값은 위 명령줄 적용만 보장하며 실제 체감 지연은 별도로 확인해야 한다."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
			[SNew(SButton).Text(LOCTEXT("Start", "실행")).IsEnabled(this, &SWorkbenchT06MultiRunPanel::IsStartEnabled).OnClicked(this, &SWorkbenchT06MultiRunPanel::OnStartClicked)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
			[SNew(SButton).Text(LOCTEXT("Stop", "내가 띄운 프로세스 종료")).IsEnabled_Lambda([this]() { return IsRunActive(); }).OnClicked(this, &SWorkbenchT06MultiRunPanel::OnStopClicked)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
			[SNew(SButton).Text(LOCTEXT("OpenLogs", "로그 폴더 열기")).IsEnabled_Lambda([this]() { return !LogDir.IsEmpty(); }).OnClicked(this, &SWorkbenchT06MultiRunPanel::OnOpenLogFolderClicked)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
			[SNew(SButton).Text(LOCTEXT("PieStart", "격리 PIE 시작"))
				.ToolTipText(LOCTEXT("PieStartTip", "이 요청의 인원·방식·포트를 복제한 임시 PIE 설정으로 에디터 안에서 시험 실행한다. 저장된 PIE 설정과 CDO는 바꾸지 않는다."))
				.IsEnabled_Lambda([this]() { return !IsRunActive() && !IsPlayInEditorActive(); })
				.OnClicked(this, &SWorkbenchT06MultiRunPanel::OnStartIsolatedPieClicked)]
			+ SHorizontalBox::Slot().AutoWidth()
			[SNew(SButton).Text(LOCTEXT("PieStop", "격리 PIE 중지"))
				.IsEnabled_Lambda([this]() { return bOwnsIsolatedPie && IsPlayInEditorActive(); })
				.OnClicked(this, &SWorkbenchT06MultiRunPanel::OnStopIsolatedPieClicked)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 4)
		[
			SNew(STextBlock).AutoWrapText(true).Text(this, &SWorkbenchT06MultiRunPanel::GetStatusText)
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(8, 4)
		[
			SAssignNew(RolesView, SListView<TSharedPtr<FRunningRole>>)
			.ListItemsSource(&Roles)
			.OnGenerateRow(this, &SWorkbenchT06MultiRunPanel::OnGenerateRow)
		]
	];
}

SWorkbenchT06MultiRunPanel::~SWorkbenchT06MultiRunPanel()
{
	FCoreDelegates::OnEnginePreExit.Remove(PreExitHandle);
	FEditorDelegates::PreBeginPIE.Remove(PreBeginPieHandle);
	FEditorDelegates::EndPIE.Remove(EndPieHandle);
	if (State == ERunState::Running)
	{
		FinishRun(ERunState::Aborted, TEXT("탭이 닫혀 이 패널이 띄운 프로세스를 회수했다."));
	}
	else
	{
		ReleaseHandles();
	}
	if (bOwnsIsolatedPie && GEditor && GEditor->PlayWorld)
	{
		GEditor->RequestEndPlayMap();
	}
}

bool SWorkbenchT06MultiRunPanel::IsPlayInEditorActive() const
{
	return GEditor && GEditor->PlayWorld != nullptr;
}

bool SWorkbenchT06MultiRunPanel::IsRunActive() const
{
	return State == ERunState::Running;
}

bool SWorkbenchT06MultiRunPanel::IsStartEnabled() const
{
	return !IsRunActive() && ActivePanelRuns == 0 && !IsPlayInEditorActive();
}

FReply SWorkbenchT06MultiRunPanel::OnUseCurrentMapClicked()
{
	if (GEditor)
	{
		if (const UWorld* World = GEditor->GetEditorWorldContext().World())
		{
			MapBox->SetText(FText::FromString(World->GetOutermost()->GetName()));
		}
	}
	return FReply::Handled();
}

bool SWorkbenchT06MultiRunPanel::LaunchNextRole()
{
	FRunningRole& Role = *Roles[NextLaunchIndex];
	const FString Args = FString::Printf(TEXT("\"%s\" %s"), *ProjectFilePath, *Role.Spec.CommandLine);
	uint32 Pid = 0;
	Role.Handle = FPlatformProcess::CreateProc(*ExecutablePath, *Args, true, Role.Spec.bHidden, Role.Spec.bHidden, &Pid, 0, nullptr, nullptr);
	LastLaunchTimeSeconds = FPlatformTime::Seconds();
	++NextLaunchIndex;
	if (Role.Handle.IsValid())
	{
		Role.State = ERoleRunState::Running;
		Role.ProcessId = Pid;
		Role.StartedAtUtc = FDateTime::UtcNow().ToIso8601();
		return true;
	}
	Role.State = ERoleRunState::LaunchFailed;
	return false;
}

TArray<FRoleRunRecord> SWorkbenchT06MultiRunPanel::SnapshotRecords() const
{
	TArray<FRoleRunRecord> Records;
	for (const TSharedPtr<FRunningRole>& Role : Roles)
	{
		FRoleRunRecord Record;
		Record.RoleLabel = Role->Spec.RoleLabel;
		Record.State = Role->State;
		Record.ProcessId = Role->ProcessId;
		Record.StartedAtUtc = Role->StartedAtUtc;
		Record.ExitCode = Role->ExitCode;
		Record.bExitCodeKnown = Role->bExitCodeKnown;
		Record.LogPath = Role->Spec.LogPath;
		Records.Add(Record);
	}
	return Records;
}

FReply SWorkbenchT06MultiRunPanel::OnStartClicked()
{
	LastErrors.Reset();
	FailureReason.Reset();
	if (!IsStartEnabled())
	{
		return FReply::Handled();
	}

	Request.MapPackagePath = MapBox->GetText().ToString().TrimStartAndEnd();
	LastErrors = ValidateMultiRunRequest(Request);
	if (LastErrors.Num() == 0 && !IsWorldPackage(Request.MapPackagePath))
	{
		LastErrors.Add(FString::Printf(TEXT("월드(맵) 에셋이 아니거나 찾지 못했다: %s (저장된 맵 패키지 경로만 실행할 수 있다)"), *Request.MapPackagePath));
	}
	if (LastErrors.Num() == 0 && !FPaths::FileExists(ExecutablePath))
	{
		LastErrors.Add(FString::Printf(TEXT("실행 파일을 찾지 못했다: %s"), *ExecutablePath));
	}
	if (LastErrors.Num() == 0 && !FPaths::FileExists(ProjectFilePath))
	{
		LastErrors.Add(FString::Printf(TEXT("프로젝트 파일을 찾지 못했다: %s"), *ProjectFilePath));
	}
	if (LastErrors.Num() == 0 && ProbeUdpPort(Request.Port) == EPortProbe::InUse)
	{
		LastErrors.Add(FString::Printf(TEXT("UDP 포트 %d가 이미 사용 중이다. 다른 포트를 고른다."), Request.Port));
	}
	if (LastErrors.Num() > 0)
	{
		State = ERunState::LaunchFailed;
		return FReply::Handled();
	}

	RunId = FGuid::NewGuid();
	LogDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Workbench/MultiRun") / RunId.ToString(EGuidFormats::Digits));
	IFileManager::Get().MakeDirectory(*LogDir, true);
	if (!IFileManager::Get().DirectoryExists(*LogDir))
	{
		LastErrors.Add(FString::Printf(TEXT("로그 폴더를 만들지 못했다: %s"), *LogDir));
		State = ERunState::LaunchFailed;
		return FReply::Handled();
	}

	Roles.Reset();
	for (const FLaunchRoleSpec& Spec : BuildLaunchPlan(Request, RunId, LogDir))
	{
		TSharedPtr<FRunningRole> Role = MakeShared<FRunningRole>();
		Role->Spec = Spec;
		Roles.Add(Role);
	}

	NextLaunchIndex = 0;
	StartTimeSeconds = FPlatformTime::Seconds();
	RunStartedAtUtc = FDateTime::UtcNow().ToIso8601();
	State = ERunState::Running;
	++ActivePanelRuns;
	bCountedActive = true;

	if (!LaunchNextRole())
	{
		LastErrors.Add(FString::Printf(TEXT("프로세스를 시작하지 못했다: %s"), *ExecutablePath));
		FinishRun(ERunState::LaunchFailed, TEXT("첫 역할 프로세스를 시작하지 못했다."));
	}
	RolesView->RequestListRefresh();
	return FReply::Handled();
}

void SWorkbenchT06MultiRunPanel::TerminateOwnedProcesses()
{
	for (const TSharedPtr<FRunningRole>& Role : Roles)
	{
		if (Role->State == ERoleRunState::Running && Role->Handle.IsValid())
		{
			if (FPlatformProcess::IsProcRunning(Role->Handle))
			{
				FPlatformProcess::TerminateProc(Role->Handle, true);
				Role->State = ERoleRunState::KilledByPanel;
			}
			else
			{
				int32 Code = 0;
				Role->bExitCodeKnown = FPlatformProcess::GetProcReturnCode(Role->Handle, &Code);
				Role->ExitCode = Code;
				Role->State = (Role->bExitCodeKnown && Code == 0) ? ERoleRunState::ExitedOk : ERoleRunState::ExitedFailed;
			}
		}
	}
}

void SWorkbenchT06MultiRunPanel::ReleaseHandles()
{
	if (bCountedActive)
	{
		bCountedActive = false;
		--ActivePanelRuns;
	}
	for (const TSharedPtr<FRunningRole>& Role : Roles)
	{
		if (Role->Handle.IsValid())
		{
			FPlatformProcess::CloseProc(Role->Handle);
		}
	}
}

void SWorkbenchT06MultiRunPanel::WriteRunResult(const FString& FinalStatus, const FString& Reason)
{
	if (LogDir.IsEmpty())
	{
		return;
	}
	FRunSummary Summary;
	Summary.RunId = RunId.ToString(EGuidFormats::Digits);
	Summary.MapPackagePath = Request.MapPackagePath;
	Summary.bDedicated = Request.bDedicated;
	Summary.Port = Request.Port;
	Summary.PlayerCount = Request.PlayerCount;
	Summary.StartedAtUtc = RunStartedAtUtc;
	Summary.EndedAtUtc = FDateTime::UtcNow().ToIso8601();
	Summary.FinalStatus = FinalStatus;
	Summary.FailureReason = Reason;
	Summary.Roles = SnapshotRecords();
	if (!FFileHelper::SaveStringToFile(BuildRunResultJson(Summary), *(LogDir / TEXT("result.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		LastErrors.Add(TEXT("실행 결과 파일(result.json)을 쓰지 못했다."));
	}
}

void SWorkbenchT06MultiRunPanel::FinishRun(ERunState FinalState, const FString& Reason)
{
	TerminateOwnedProcesses();
	ReleaseHandles();
	State = FinalState;
	FailureReason = Reason;
	FString Status;
	switch (FinalState)
	{
	case ERunState::Finished: Status = TEXT("모든 프로세스 종료"); break;
	case ERunState::TimedOut: Status = TEXT("시간 제한으로 회수"); break;
	case ERunState::Stopped: Status = TEXT("사용자가 회수"); break;
	case ERunState::LaunchFailed: Status = TEXT("실행 실패"); break;
	default: Status = TEXT("조기 종료·중단으로 전체 회수"); break;
	}
	WriteRunResult(Status, Reason);
	if (RolesView.IsValid())
	{
		RolesView->RequestListRefresh();
	}
}

void SWorkbenchT06MultiRunPanel::OnPreBeginPie(bool bIsSimulating)
{
	if (State == ERunState::Running)
	{
		FinishRun(ERunState::Aborted, TEXT("에디터 PIE가 시작되어 이 패널이 띄운 프로세스를 회수했다."));
	}
}

void SWorkbenchT06MultiRunPanel::OnEndPie(bool bIsSimulating)
{
	bOwnsIsolatedPie = false;
}

FReply SWorkbenchT06MultiRunPanel::OnStopClicked()
{
	if (IsRunActive())
	{
		FinishRun(ERunState::Stopped, TEXT("사용자가 중지했다."));
	}
	return FReply::Handled();
}

FReply SWorkbenchT06MultiRunPanel::OnOpenLogFolderClicked()
{
	FPlatformProcess::ExploreFolder(*LogDir);
	return FReply::Handled();
}

FReply SWorkbenchT06MultiRunPanel::OnStartIsolatedPieClicked()
{
	LastErrors.Reset();
	if (!GEditor || IsRunActive() || IsPlayInEditorActive())
	{
		LastErrors.Add(TEXT("PIE나 외부 프로세스 실행이 이미 진행 중이라 격리 PIE를 시작하지 않았다."));
		return FReply::Handled();
	}
	if (Request.PlayerCount < 1 || Request.PlayerCount > 4)
	{
		LastErrors.Add(TEXT("인원은 1~4여야 한다."));
		return FReply::Handled();
	}
	if (ProbeUdpPort(Request.Port) == EPortProbe::InUse)
	{
		LastErrors.Add(FString::Printf(TEXT("UDP 포트 %d가 이미 사용 중이다. 다른 포트를 고른다."), Request.Port));
		return FReply::Handled();
	}

	// 저장된 PIE 설정(CDO)은 건드리지 않고 복제본만 요청에 실어 보낸다.
	ULevelEditorPlaySettings* Settings = DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage());
	if (!Settings)
	{
		LastErrors.Add(TEXT("격리용 PIE 설정을 복제하지 못했다."));
		return FReply::Handled();
	}
	Settings->SetRunUnderOneProcess(true);
	Settings->SetPlayNetMode(Request.bDedicated ? EPlayNetMode::PIE_Client : (Request.PlayerCount > 1 ? EPlayNetMode::PIE_ListenServer : EPlayNetMode::PIE_Standalone));
	Settings->SetPlayNumberOfClients(Request.PlayerCount);
	Settings->SetServerPort(static_cast<uint16>(Request.Port));

	FRequestPlaySessionParams Params;
	Params.WorldType = EPlaySessionWorldType::PlayInEditor;
	Params.SessionDestination = EPlaySessionDestinationType::InProcess;
	Params.EditorPlaySettings = Settings;
	bOwnsIsolatedPie = true;
	GEditor->RequestPlaySession(Params);
	return FReply::Handled();
}

FReply SWorkbenchT06MultiRunPanel::OnStopIsolatedPieClicked()
{
	if (bOwnsIsolatedPie && GEditor && GEditor->PlayWorld)
	{
		GEditor->RequestEndPlayMap();
	}
	return FReply::Handled();
}

EActiveTimerReturnType SWorkbenchT06MultiRunPanel::PollProcesses(double InCurrentTime, float InDeltaTime)
{
	if (!IsRunActive())
	{
		return EActiveTimerReturnType::Continue;
	}

	const double Now = FPlatformTime::Seconds();

	for (const TSharedPtr<FRunningRole>& Role : Roles)
	{
		if (Role->State == ERoleRunState::Running && Role->Handle.IsValid() && !FPlatformProcess::IsProcRunning(Role->Handle))
		{
			int32 Code = 0;
			Role->bExitCodeKnown = FPlatformProcess::GetProcReturnCode(Role->Handle, &Code);
			Role->ExitCode = Code;
			Role->State = (Role->bExitCodeKnown && Code == 0) ? ERoleRunState::ExitedOk : ERoleRunState::ExitedFailed;
		}
	}

	const bool bStartupPhase = NextLaunchIndex < Roles.Num();
	if (ShouldAbortRun(SnapshotRecords(), bStartupPhase))
	{
		FinishRun(ERunState::Aborted, bStartupPhase
			? TEXT("시작 단계에서 역할이 조기 종료되어 나머지를 시작하지 않고 전체를 회수했다.")
			: TEXT("실행 중 역할이 비정상 종료되어 전체를 회수했다."));
		return EActiveTimerReturnType::Continue;
	}

	if (bStartupPhase && Now - LastLaunchTimeSeconds >= LaunchStaggerSeconds)
	{
		if (!LaunchNextRole())
		{
			LastErrors.Add(FString::Printf(TEXT("%s 프로세스를 시작하지 못해 나머지를 모두 종료했다."), *Roles[NextLaunchIndex - 1]->Spec.RoleLabel));
			FinishRun(ERunState::LaunchFailed, TEXT("역할 프로세스 시작 실패로 전체를 회수했다."));
			return EActiveTimerReturnType::Continue;
		}
	}

	bool bAllExited = NextLaunchIndex == Roles.Num();
	for (const TSharedPtr<FRunningRole>& Role : Roles)
	{
		if (Role->State == ERoleRunState::Running || Role->State == ERoleRunState::NotLaunched)
		{
			bAllExited = false;
		}
	}

	if (bAllExited)
	{
		FinishRun(ERunState::Finished, FString());
	}
	else if (Now - StartTimeSeconds > TimeoutMinutes * 60.0)
	{
		FinishRun(ERunState::TimedOut, TEXT("자동 종료 시간을 넘겼다."));
	}
	else
	{
		RolesView->RequestListRefresh();
	}
	return EActiveTimerReturnType::Continue;
}

FText SWorkbenchT06MultiRunPanel::GetStatusText() const
{
	FTextBuilder Builder;
	if (IsPlayInEditorActive())
	{
		Builder.AppendLine(bOwnsIsolatedPie
			? LOCTEXT("PieOwn", "이 패널이 시작한 격리 PIE가 실행 중이다. 외부 프로세스 실행은 막혀 있다.")
			: LOCTEXT("PieBlock", "에디터 PIE가 실행 중이라 새 실행을 막는다. PIE를 종료한다."));
	}
	else if (ActivePanelRuns > 0 && !IsRunActive())
	{
		Builder.AppendLine(LOCTEXT("OtherRun", "다른 T06 패널의 실행이 진행 중이라 새 실행을 막는다."));
	}

	switch (State)
	{
	case ERunState::Idle:
		Builder.AppendLine(LOCTEXT("Idle", "대기 중. 맵과 인원을 정하고 실행한다."));
		break;
	case ERunState::Running:
		Builder.AppendLine(FText::Format(LOCTEXT("Running", "실행 중 ({0}초 경과). 프로세스 생존만 확인하며 접속 성공·준비 완료는 감지하지 않는다. 화면과 로그로 직접 확인해야 하고 이 실행 자체는 M2 통과 판정이 아니다. 서버가 뜰 시간을 두려고 역할을 {1}초 간격으로 순서대로 시작한다."),
			FText::AsNumber(FMath::FloorToInt(FPlatformTime::Seconds() - StartTimeSeconds)), FText::AsNumber(FMath::FloorToInt(LaunchStaggerSeconds))));
		break;
	case ERunState::Finished:
		Builder.AppendLine(LOCTEXT("Finished", "모든 프로세스가 종료되었다. 종료 코드가 정상이어도 접속 성공이나 M2 통과를 뜻하지 않는다."));
		break;
	case ERunState::TimedOut:
		Builder.AppendLine(LOCTEXT("TimedOut", "시간 제한을 넘겨 이 패널이 띄운 프로세스를 종료했다."));
		break;
	case ERunState::Stopped:
		Builder.AppendLine(LOCTEXT("Stopped", "사용자가 이 패널이 띄운 프로세스를 종료했다."));
		break;
	case ERunState::LaunchFailed:
		Builder.AppendLine(LOCTEXT("LaunchFailed", "실행하지 못했다."));
		break;
	case ERunState::Aborted:
		Builder.AppendLine(LOCTEXT("Aborted", "중단되어 이 패널이 띄운 프로세스를 모두 회수했다."));
		break;
	}
	if (!FailureReason.IsEmpty())
	{
		Builder.AppendLine(FText::FromString(FailureReason));
	}
	for (const FString& Error : LastErrors)
	{
		Builder.AppendLine(FText::FromString(Error));
	}
	if (!LogDir.IsEmpty())
	{
		Builder.AppendLine(FText::Format(LOCTEXT("LogDir", "실행별 로그 폴더(result.json 포함): {0}"), FText::FromString(LogDir)));
	}
	Builder.AppendLine(LOCTEXT("PieNote", "격리 PIE는 복제한 임시 설정으로 한 프로세스 안에서 실행하는 시험용이며 네트워크 방식은 미확정이다."));
	return Builder.ToText();
}

TSharedRef<ITableRow> SWorkbenchT06MultiRunPanel::OnGenerateRow(TSharedPtr<FRunningRole> Role, const TSharedRef<STableViewBase>& OwnerTable)
{
	const TSharedPtr<FRunningRole> RoleRef = Role;
	auto GetRoleLine = [RoleRef]() -> FText
	{
		FText Status = FText::FromString(RoleRunStateLabel(RoleRef->State));
		if (RoleRef->State == ERoleRunState::ExitedFailed && RoleRef->bExitCodeKnown)
		{
			Status = FText::Format(LOCTEXT("RoleExitCode", "{0} (코드 {1})"), Status, FText::AsNumber(RoleRef->ExitCode));
		}
		const FText PidText = RoleRef->ProcessId != 0
			? FText::Format(LOCTEXT("RolePid", "PID {0}, 시작 {1}"), FText::AsNumber(static_cast<int32>(RoleRef->ProcessId)), FText::FromString(RoleRef->StartedAtUtc))
			: FText::GetEmpty();
		return FText::Format(LOCTEXT("RoleLine", "{0} - {1} {2}"), FText::FromString(RoleRef->Spec.RoleLabel), Status, PidText);
	};

	return SNew(STableRow<TSharedPtr<FRunningRole>>, OwnerTable)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda(GetRoleLine)]
			+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([RoleRef]() { return FText::FromString(RoleRef->Spec.LogPath); })]
		];
}

#undef LOCTEXT_NAMESPACE
