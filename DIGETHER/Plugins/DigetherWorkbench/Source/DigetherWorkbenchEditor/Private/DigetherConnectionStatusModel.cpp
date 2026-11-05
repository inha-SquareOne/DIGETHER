#include "DigetherConnectionStatusModel.h"

#define LOCTEXT_NAMESPACE "DigetherConnectionStatusModel"

TArray<FDigetherConnectionStatusRow> BuildDigetherConnectionStatusRows()
{
	TArray<FDigetherConnectionStatusRow> Rows;

	Rows.Add(FDigetherConnectionStatusRow{
		FName(TEXT("Item")),
		LOCTEXT("DomainItem", "아이템 정의"),
		LOCTEXT("StatusUnconnected_Item", "미연결"),
		LOCTEXT("DetailItem", "실게임 아이템 데이터 provider가 등록되지 않았습니다. 승인된 스키마·가격·중량 단위·희귀도가 확정되지 않았습니다."),
		false
	});

	Rows.Add(FDigetherConnectionStatusRow{
		FName(TEXT("Economy")),
		LOCTEXT("DomainEconomy", "경제 규칙"),
		LOCTEXT("StatusUnconnected_Economy", "미연결"),
		LOCTEXT("DetailEconomy", "가격·회수·정산 계산 provider가 등록되지 않았습니다. 하루 수익 정의와 회수/사망 정책이 결정 대기입니다."),
		false
	});

	Rows.Add(FDigetherConnectionStatusRow{
		FName(TEXT("Spawn")),
		LOCTEXT("DomainSpawn", "스폰 확률"),
		LOCTEXT("StatusUnconnected_Spawn", "미연결"),
		LOCTEXT("DetailSpawn", "스폰 풀·가중치 provider가 등록되지 않았습니다. 선택 규칙·조건·수량 정책이 결정되지 않았습니다."),
		false
	});

	Rows.Add(FDigetherConnectionStatusRow{
		FName(TEXT("Room")),
		LOCTEXT("DomainRoom", "룸/던전 규격"),
		LOCTEXT("StatusUnconnected_Room", "미연결"),
		LOCTEXT("DetailRoom", "룸 표현·문 규격·생성기 provider가 등록되지 않았습니다. 룸 표현과 허용 오차 계약이 결정되지 않았습니다."),
		false
	});

	Rows.Add(FDigetherConnectionStatusRow{
		FName(TEXT("Runtime")),
		LOCTEXT("DomainRuntime", "멀티 런타임 준비 신호"),
		LOCTEXT("StatusUnconnected_Runtime", "미연결"),
		LOCTEXT("DetailRuntime", "게임 Runtime의 RunId/Role/Ready 진단 신호 provider가 등록되지 않았습니다. 리슨/전용 토폴로지와 실행 환경이 결정되지 않았습니다."),
		false
	});

	Rows.Add(FDigetherConnectionStatusRow{
		FName(TEXT("PlanningRevision")),
		LOCTEXT("DomainPlanning", "기획 승인 개정"),
		LOCTEXT("StatusUnregistered_Planning", "미등록"),
		LOCTEXT("DetailPlanning", "Notion 승인 개정 번호가 이 패널에 등록되지 않았습니다. docs/planning의 이관 전 초안만 참조할 수 있습니다."),
		false
	});

	return Rows;
}

#undef LOCTEXT_NAMESPACE
