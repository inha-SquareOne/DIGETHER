# 구현 현황

기준일: 2026-11-10 (커밋 ee12223, `feature/minsung`). 도메인별 **코드 유무·자동 테스트 등록 수·실행으로 확인한 범위·미검증**을 한 곳에 모은다. 설계 문서는 현황을 본문에 다시 적지 않고 이 문서를 링크한다. 기능 상태가 바뀐 작업 안에서 해당 행을 갱신한다.

읽는 법:

- "코드 있음"은 소스가 저장소에 있다는 뜻이다. 동작 확인은 '실행으로 확인한 범위' 칸만 근거로 삼는다.
- 테스트 수는 2026-11-10에 소스의 등록 이름을 센 값이다. 실행 결과가 아니다.
- 실행 확인은 확인한 날짜의 사실이다. 그 뒤 코드가 바뀐 도메인은 다시 확인해야 한다. 상세 증거는 작업자 로컬의 날짜별 작업 기록에 있으며 Git에는 없다.
- 다인 검증 등급 M0~M3는 [멀티플레이 테스트](../ai/MULTIPLAYER_TEST.md), 검사 항목은 [검증 계획](../verification/VERIFICATION_PLAN.md)을 따른다.

## 자동 테스트 실행

- 마지막 전체 실행 기록: 2026-11-07, `DIGETHER.Game` 184개 실패 0(Editor 빌드 뒤). 2026-11-10 소스의 등록 수도 `DIGETHER.Game.*` 184개다. 그 밖에 `DIGETHER.Dain.*` 6개, `DIGETHER.Prototype.*` 3개가 있다.
- 표준 검사 실행기 `scripts/Invoke-UnrealChecks.ps1`의 `Automation`은 `Workbench.` 테스트만 고른다. 게임 테스트는 [테스트 안내](../unreal/TESTING.md)의 명령 형식에 `DIGETHER.` 접두사를 넣어 직접 실행한다. 실행기에 게임 테스트를 넣는 일은 남아 있다.

## 도메인별 현황

| 도메인 | 코드 위치(`Source/DIGETHER/`) | 테스트 등록 | 실행으로 확인한 범위 | 미검증·남은 것 |
|---|---|---|---|---|
| 세션·메인 메뉴·대기실 | `Game/Online`, `Game/Frontend`, `Game/UI/MainMenu`·`Lobby` | Online 16 | LAN 2창 방 생성·참가(10-31), 대기실→게임 Seamless 이동 LAN 확인(11-06) | Steam 두 PC 재시험, 실패 경로 문구, 방 목록 상태 갱신 |
| 게임 진입·런 흐름 | `Game/Framework`, `Game/Run` | Framework 55, Run 13 | 1인 출발→2회 상승→종료→귀환 스모크, 리슨+클라이언트 3의 레버 요청·시간 만료 실종·귀환 부활(11-04), 4인 귀환·비공개 값(11-05), 갱도 하위 레벨을 넣은 패키지에서 출발→갱도→귀환(11-07) | 관전 순환, 사출, 시체 회수, 로딩 실패 복귀(호출 경로 없음) |
| 경제·납부 | `Game/Economy`, `Game/UI/Deposit` | Economy 18 | 4인 납부·재접속(11-05), 원격 붓기 4인 10회·지연 100ms/손실 5%(11-06) | 물리 긴 누름 입력, M3 |
| 엘리베이터·레버 | `Game/Elevator` | Elevator 11 | 런 흐름 스모크와 4인 검증에 포함 | 정식 갱도에서의 탑승 판정 |
| 생존·관전 | `Game/Life` | Life 5 | 4인 사망 손실 비공개·귀환 부활(11-04) | 관전 대상 전환의 다인 확인, 지연 조건의 입력 잠금 |
| 저장·재접속 | `Game/Save`, `Game/Framework`의 저장·재접속 파일 | Save 30(+Framework 통합) | 재시작 불러오기, 손상→백업 복구, 2프로세스 탐사 중 재접속(11-04) | 3~4인 재접속, 이어하기 방에 저장 명단 밖 참가자, 귀환 뒤 미접속자 자리 반환, 거점 바닥 물품 저장·복구 |
| 거점 시설 | `Game/Hub`, `Game/UI/Hub` | Hub 17 | 기본 Blueprint 연결과 PIE에서 시설 외형 교체·복원(11-05) | 실제 UI 조작으로 하는 거래 전 과정, 다인(M2), cook, 최종 아트 |
| 게임 오버·크레딧 | `Game/Framework`, `Game/Run`, `Game/UI/Results` | Framework·Run에 포함 | 1인 스모크 완주(11-05), 0원 이탈 크레딧 수정 뒤 자동 테스트(11-06) | 다인(M2), 크레딧 중 이탈·재참가 |
| 음성 | `Game/Voice`, `Game/UI/Voice` | Voice 4 | LAN 2프로세스 발화자 등록(11-06), 설정창 마이크 막대·미리 듣기 PIE(11-07) | 실제 음성 청취, Steam 두 PC(M3), 갱도↔던전 구역·매몰 처리(미구현) |
| 설정·키 재설정 | `Game/Settings`, `Game/Input`, `Game/UI/Settings`·`Pause` | Settings 3, UI 4, Input 1 | 설정 4탭·키 교환·저장 복원(새 프로세스, 11-06) | 일시정지 메뉴의 실제 키·버튼 조작, 재설정 뒤 안내 문구 화면, 게임패드(설계 초안만) |
| 몬스터 | `Game/Monster` | Monster 2 | 정의 자산 검사·체력 규칙 자동 테스트 | AI 행동·스폰·감지 미구현, 전투 실행 미검증 |
| 굴착(프로토타입 계열) | `Prototype/World` | Prototype 1 | UE 5.8.2 시점 패키지에서 1인 렌더·로컬 4인 경합 10회·재접속(10-28) | 5.8.4 재확인, 청크 확장 |
| 굴착(다인 계열) | `Game/Excavation/Dain`·`DainLab` | Dain 6 | 담당 문서의 1인 확인([통합 설계](DAIN_EXCAVATION_INTEGRATION.md)) | 복제·다인 미구현, 기준 계열·지형 위상·동기화 방식 결정 대기 |
| 인벤토리·장비 | 없음 | 0 | — | 미구현. 거점 소지품은 `Game/Hub`의 이름+수량 목록으로만 있다 |
| 던전 진입·미로·폐교 | `Prototype/World`(시험용 포탈) | — | 프로토타입 패키지 1인·로컬 4인(10-29) | 정식 구현 없음 |
| 정의 데이터·개발 지원 | `Game/Data`, `Game/Core`, `Game/Dev` | Data 3, Dev 2 | DataValidation(정의 자산), 로그 분류·치트 틀 자동 테스트 | 튜닝 요청 R01~R14 충족 여부는 [튜닝 환경 현황](TUNING_ENVIRONMENT_STATUS.md) |

## 실행 맵

`DIGETHER/Config`: 시작 맵 `L_MainMenu`, 게임 맵 `L_Hub`, cook 대상 `L_GameTest`·`L_Hub`. 메인 메뉴 → 대기실(`L_Lobby`) → Seamless 이동 → 거점(`L_Hub`) 순서이며 갱도는 거점 맵의 하위 레벨로 붙는다. 맵 내부 구성은 SVN 관리 에셋이라 이 문서에서 확인하지 않았다.

## 전 도메인 공통 미검증

- 서로 다른 PC의 Steam 세션·음성(M3)
- cook·Shipping 패키지에서의 전체 흐름
- 성능 예산과 측정
