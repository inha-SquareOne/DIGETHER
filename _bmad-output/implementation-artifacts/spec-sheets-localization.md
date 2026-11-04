---
title: Google Sheets 현지화 연동
type: feature
created: 2026-11-04
status: in-progress
context: [docs/project/PROJECT_BOUNDARIES.md, docs/ai/EXECUTION_POLICY.md]
---

<frozen-after-approval reason="사용자가 Google Sheets 원본 및 String Table·번역 연동을 선택함">

## Intent

**Problem:** 문구 원본과 번역을 협업할 스프레드시트 및 Unreal 반영 절차가 없다.

**Approach:** 기존 String Table을 기준으로 시트 원문을 CSV로 준비하고, Unreal이 내보낸 PO의 식별자와 원문을 대조해 번역을 반영한다.

## Boundaries & Constraints

**Always:** 기존 에셋 경로와 키를 보존하고, 중복·누락·서식 인자 오류는 반영 전에 거절한다. 시트와 대상 문화는 사용자 선택에 따른다.

**Ask First:** 새 시트가 들어갈 계정·팀 드라이브와 실제 지원 언어가 미정이다. 공유 바이너리 에셋의 동시 편집·미저장 상태가 불명확하면 저장하지 않는다.

**Never:** PO 식별자를 추측하지 않고, 시트의 번역 열이 String Table CSV만으로 현지화된다고 가정하지 않는다. 다른 작업의 변경을 덮지 않는다.

## I/O & Edge-Case Matrix

| 상황 | 입력 | 기대 결과 | 실패 처리 |
|---|---|---|---|
| 정상 | 시트 행과 기존 키, 일치하는 PO | String Table CSV 및 번역된 PO 준비 | 새 출력만 생성 |
| 누락 또는 중복 | 기존 키 누락·시트 중복 | 기존 에셋 보호 | 출력 전 오류 |
| 불일치 | PO 식별자·원문 또는 서식 인자 불일치 | 오번역 차단 | 오류와 원인 표시 |
| 미준비 | 시트·문화·실제 PO 부재 | 독립 도구·절차 준비 | 실제 반영 미완료로 보고 |

</frozen-after-approval>

## Code Map

- `DIGETHER/Source/DIGETHER/Game/Online/DigetherSessionText.cpp` — 기존 String Table 참조.
- `DIGETHER/Source/DIGETHER/Game/Tests/DigetherSessionTextTest.cpp` — 기존 키·서식 검사.
- `scripts/localization/` — 변환과 Unreal 에디터 내보내기 도구 및 테스트.

## Tasks & Acceptance

**Execution:**
- [x] `scripts/localization/` — 시트 CSV 검증, Unreal CSV·PO 변환 및 안전한 기존 테이블 내보내기.
- [x] `DIGETHER/Config/DefaultEditor.ini`, `DefaultGame.ini` — 사용자 선택 문화와 대상·패키징 설정. 동일 설정의 격리 로드 검증.
- [x] `docs/` — 사용법과 실제 적용·검증 한계 설명.
- [x] `.claude/docs/` — 결과와 미검증, 다음 작업 기록.
- [ ] 실제 프로젝트의 시트 번역 Import·Compile 및 게임 표시·패키징 검증.

**Acceptance Criteria:**
- 기존 CSV와 Unreal PO의 식별자·원문이 일치할 때 시트 번역을 명시된 문화의 PO에 준비한다.
- 미완성·오류 입력은 기존 파일이나 에셋을 변경하지 않는다.
- 실제 에디터 적용과 실행을 하지 않은 경우 그 한계를 보고한다.

## Design Notes

Unreal CSV의 추가 언어 열은 메타데이터이며 번역 데이터가 아니다. PO는 Unreal이 생성한 것을 입력으로 사용한다. 기존 `LOCTEXT`는 현지화 수집 대상이지만 String Table 대상 행과 구별해야 한다.

## Verification

- 합성 입력으로 성공·중복·누락·원문 변경·서식 인자·출력 보호를 검사한다.
- Unreal 5.8.4의 실제 에셋·PO 왕복과 패키지 언어 표시를 별도로 확인한다.

합성 Python 테스트 22개 통과. UE 5.8.4 격리 에셋 사본으로 53개 문구 Export·Gather·PO Import·Compile을 확인했다. 가져오기 전 기준 CSV와 별도 최신 export를 비교한다. 실제 최신 추출·미저장 상태와 잠금은 운영자가 확인한다. 전체 프로젝트 사본 Gather는 메모리 부족 위험으로 중단했다. 실제 번역·프로젝트 적용과 패키지 표시는 미검증이다. Claude Opus 5.5 xhigh 명세 교차 검증은 10분 넘게 결과가 없어 중단했으며 미완료다.
