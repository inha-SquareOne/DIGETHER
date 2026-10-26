#include "SWorkbenchRoomConnectivityPanel.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

using namespace WorkbenchAnalytics;

TSharedRef<SWidget> SWorkbenchRoomConnectivityPanel::MakeToleranceEntry(const FString& Label, double* Value, double MinValue)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(2).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(FText::FromString(Label))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)
		[
			SNew(SNumericEntryBox<double>)
			.MinValue(MinValue)
			.Value_Lambda([Value]() { return TOptional<double>(*Value); })
			.OnValueCommitted_Lambda([Value, MinValue](double NewValue, ETextCommit::Type)
			{
				*Value = FMath::IsFinite(NewValue) ? FMath::Max(NewValue, MinValue) : MinValue;
			})
		];
}

TSharedRef<SWidget> SWorkbenchRoomConnectivityPanel::MakeSampleButton(const FString& Label, ERoomManifestSample Sample)
{
	return SNew(SButton)
		.Text(FText::FromString(Label))
		.OnClicked(this, &SWorkbenchRoomConnectivityPanel::OnLoadSampleClicked, Sample);
}

void SWorkbenchRoomConnectivityPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(FText::FromString(TEXT(
					"T04 룸 연결성 검사 — 입력한 매니페스트의 스키마·tolerance·연결 그래프만 검사합니다.\n"
					"미연결: 실제 월드 충돌, NavMesh, PCG. 이 검사를 통과해도 플레이어가 실제로 이동 가능하다는 뜻이 아닙니다.\n"
					"예제는 합성 데이터이며 실제 게임 룸 규격이 아닙니다.")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(FText::FromString(TEXT(
					"형식(한 줄에 하나, '#'은 주석, 단위 cm):\n"
					"ROOM,RoomId,SizeX,SizeY,SizeZ,Scale,OriginX,OriginY,YawDegrees\n"
					"DOOR,DoorId,RoomId,Dir(+X|-X|+Y|-Y),Offset,Width,Height[,ConnectsTo]\n"
					"ENTRY,RoomId / EXIT,RoomId")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SAssignNew(ManifestBox, SMultiLineEditableTextBox)
				.AllowMultiLine(true)
				.Text(FText::FromString(GetRoomManifestSampleText(ERoomManifestSample::Normal)))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(SWrapBox).UseAllottedSize(true)
				+ SWrapBox::Slot()[MakeToleranceEntry(TEXT("위치 허용(cm):"), &Spec.PositionToleranceCm, 0.0)]
				+ SWrapBox::Slot()[MakeToleranceEntry(TEXT("크기 허용(cm):"), &Spec.SizeToleranceCm, 0.0)]
				+ SWrapBox::Slot()[MakeToleranceEntry(TEXT("각도 허용(도):"), &Spec.AngleToleranceDegrees, 0.0)]
				+ SWrapBox::Slot()[MakeToleranceEntry(TEXT("스케일 허용:"), &Spec.ScaleTolerance, 0.0)]
				+ SWrapBox::Slot()[MakeToleranceEntry(TEXT("기대 스케일(0=검사 안 함):"), &Spec.ExpectedScale, 0.0)]
				+ SWrapBox::Slot()[MakeToleranceEntry(TEXT("기대 문 너비(0=검사 안 함):"), &Spec.ExpectedDoorWidth, 0.0)]
				+ SWrapBox::Slot()[MakeToleranceEntry(TEXT("기대 문 높이(0=검사 안 함):"), &Spec.ExpectedDoorHeight, 0.0)]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("합성 예제 불러오기:")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(SWrapBox).UseAllottedSize(true)
				+ SWrapBox::Slot().Padding(2)[MakeSampleButton(TEXT("정상"), ERoomManifestSample::Normal)]
				+ SWrapBox::Slot().Padding(2)[MakeSampleButton(TEXT("실패: 한쪽 연결"), ERoomManifestSample::OneSidedLink)]
				+ SWrapBox::Slot().Padding(2)[MakeSampleButton(TEXT("실패: 방향 뒤집힘"), ERoomManifestSample::FlippedDirection)]
				+ SWrapBox::Slot().Padding(2)[MakeSampleButton(TEXT("실패: 스케일 불일치"), ERoomManifestSample::ScaleMismatch)]
				+ SWrapBox::Slot().Padding(2)[MakeSampleButton(TEXT("실패: 문 크기 불일치"), ERoomManifestSample::SizeMismatch)]
				+ SWrapBox::Slot().Padding(2)[MakeSampleButton(TEXT("실패: 룸 겹침"), ERoomManifestSample::Overlap)]
				+ SWrapBox::Slot().Padding(2)[MakeSampleButton(TEXT("실패: 고립 룸"), ERoomManifestSample::IsolatedRoom)]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("검사 실행")))
				.OnClicked(this, &SWorkbenchRoomConnectivityPanel::OnAnalyzeClicked)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SAssignNew(StatusText, STextBlock)
				.AutoWrapText(true)
				.Text(FText::FromString(TEXT("상태: 아직 실행하지 않음.")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(SSeparator)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SAssignNew(ResultText, STextBlock).AutoWrapText(true)
			]
		]
	];
}

void SWorkbenchRoomConnectivityPanel::SetOutput(const FString& Status, const FString& Detail)
{
	StatusText->SetText(FText::FromString(Status));
	ResultText->SetText(FText::FromString(Detail));
}

FReply SWorkbenchRoomConnectivityPanel::OnLoadSampleClicked(ERoomManifestSample Sample)
{
	ManifestBox->SetText(FText::FromString(GetRoomManifestSampleText(Sample)));
	SetOutput(TEXT("상태: 합성 예제를 불러왔습니다. 아직 검사하지 않았습니다."), FString());
	return FReply::Handled();
}

FString SWorkbenchRoomConnectivityPanel::BuildReport(const FRoomManifestReport& Report)
{
	FString Text = FString::Printf(TEXT("룸 %d개, 문 %d개, 양방향 연결 %d쌍 / 오류 %d건, 경고 %d건\n\n"),
		Report.RoomCount, Report.DoorCount, Report.MutualLinkCount, Report.CountErrors(), Report.CountWarnings());

	for (const FRoomManifestIssue& Issue : Report.Issues)
	{
		Text += FString::Printf(TEXT("[%s] %s (%s): %s\n"),
			Issue.Severity == ERoomIssueSeverity::Error ? TEXT("오류") : TEXT("경고"),
			*GetRoomIssueTypeLabel(Issue.Type), *Issue.ObjectId, *Issue.Message);
	}
	if (Report.Issues.Num() == 0)
	{
		Text += TEXT("발견된 문제 없음.\n");
	}

	Text += FString::Printf(TEXT("\n입구에서 선언된 연결로 도달하는 룸(%d): %s\n"), Report.ReachableRooms.Num(), *FString::Join(Report.ReachableRooms, TEXT(", ")));
	if (Report.RoomsWithoutReturn.Num() > 0)
	{
		Text += FString::Printf(TEXT("입구로 되돌아올 수 없는 룸(%d): %s\n"), Report.RoomsWithoutReturn.Num(), *FString::Join(Report.RoomsWithoutReturn, TEXT(", ")));
	}

	Text += Report.PassesGraphChecks()
		? TEXT("\n결과: 스키마·그래프 검사 통과(오류 0건). 월드 충돌·NavMesh·PCG는 검사하지 않았으므로 실제 이동 가능 여부는 확인되지 않았습니다.")
		: TEXT("\n결과: 스키마·그래프 검사 실패. 위 오류를 해결하세요. (월드 충돌·NavMesh·PCG 미연결)");
	return Text;
}

FReply SWorkbenchRoomConnectivityPanel::OnAnalyzeClicked()
{
	FRoomManifest Manifest;
	TArray<FString> ParseErrors;
	if (!ParseRoomManifestText(ManifestBox->GetText().ToString(), Manifest, ParseErrors))
	{
		FString Text = TEXT("매니페스트 파싱 실패:\n");
		for (const FString& Error : ParseErrors)
		{
			Text += TEXT("- ") + Error + TEXT("\n");
		}
		SetOutput(TEXT("상태: 파싱 오류. 검사하지 않았습니다."), Text);
		return FReply::Handled();
	}

	const FRoomManifestReport Report = AnalyzeRoomManifest(Manifest, Spec);
	SetOutput(Report.PassesGraphChecks() ? TEXT("상태: 검사 완료 — 스키마·그래프 검사 통과 (이동 가능 여부는 미검증).") : TEXT("상태: 검사 완료 — 오류 발견."),
		BuildReport(Report));
	return FReply::Handled();
}
