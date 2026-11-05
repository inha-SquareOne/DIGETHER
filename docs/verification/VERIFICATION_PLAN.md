# 검증 계획

기준일: 2026-10-28. Editor 기본/강제 Unity·Game 빌드, 설정 격리 후 최종 Automation 64개 성공(경고 있는 테스트 2개), DataValidation 대상 3개 valid를 확인했다. 허브 표시·상태 조회·새로고침·바로가기 7종의 실제 UI도 확인했다. 후속 실행에서는 전체 Win64 Development 패키지, 맵 4개 기동, 같은 PC의 템플릿 1/2/4인 이동·이탈·새 ID 재입장, Level Snapshots의 별도 프로세스 디스크 복원을 확인했다. 상세 환경·근거·제약은 [개발 도구 안내](../tools/DEVELOPER_TOOLS.md#후속-실기-검증)를 따른다. 아래 DG 게임 시나리오 전체의 통과나 게임 완성으로 판정하지 않는다.

## 적용 수준

| 변경 | 필요한 검증 |
|---|---|
| 문서 | L1: 출처·확정/제안 구분·로컬 링크·규칙 정합성 |
| 코드·에셋 | L1: 해당 빌드, BP 컴파일·로드·참조 |
| 기능·데이터 | L1+L2: 정상·실패 경로 자동 검사 또는 테스트 레벨 |
| 상태 전환·네트워크 | L1~L3와 M2: 4인 별도 프로세스의 통합·경합 |
| 실제 서비스·음성·온라인 품질 | M3: 서로 다른 PC 4인, 실제 계정/서비스 환경 |

공통 [검증 기준](../ai/VERIFICATION.md)과 [멀티플레이 기준](../ai/MULTIPLAYER_TEST.md)을 적용한다. M2는 제공된 호스트형 기획에 따라 호스트+원격 클라이언트 3으로 검사한다. 구체 세션 제공자와 구현 방식은 미정이다. 1인 지원도 별도 검사하며 4인 통과로 대체하지 않는다.

여기서 M0~M3는 **멀티플레이 검증 등급**이다. 로드맵의 M1 프로토타입·M2 알파·M3 베타는 **제작 단계**로, 이름이 같아도 서로 다른 구분이다.

## 로컬 도구 검사

저장소 루트의 [Invoke-UnrealChecks.ps1](../../scripts/Invoke-UnrealChecks.ps1)은 `BuildEditor`·`BuildGame`·`Automation`·`ValidateData` 모드를 제공한다. PowerShell 5.1 이상에서 `-EngineRoot` 또는 `UE_ENGINE_ROOT`로 UE 5.8.4를 지정한다. 실행 예·로그 위치·판정 규칙은 [개발 도구 안내](../tools/DEVELOPER_TOOLS.md#로컬-검사-실행)를 단일 사용법으로 유지한다.

- C++ 테스트 변경 후 BuildEditor를 먼저 실행한다. Automation은 소스의 `Workbench.` 기대 목록과 보고서의 이름·상태·개수를 대조한다. 과거 60개 통과를 현재 기대 개수로 고정하지 않는다.
- ValidateData의 기본 대상은 DataTable이며 전체 프로젝트 검사가 아니다. commandlet 전체 대상 수와 사용자 검증기별 대상 수를 구분한다. 의도적 오류를 포함한 기본 합성 예제는 Workbench 사용자 검증기에서 제외된다.
- 실행별 고유 `DIGETHER/Saved/DeveloperChecks/` 폴더의 `execution.json`·로그·보고서를 보존한다. 종료 코드 0만으로 통과시키지 않는다. `-DryRun`은 실제 검증이 아니다.
- 문서 L1은 `node .\scripts\check-doc-links.mjs`로 로컬 파일·앵커를 확인하고 출처·확정/제안·구현 정합성을 읽는다. 외부 HTTP 페이지의 접근 가능성은 별도다.

2026-10-27 Workbench 60개 자동검사·non-unity 빌드의 [과거 범위](../../DIGETHER/Plugins/DevelopmentWorkbench/README.md#확인한-범위와-한계-2026-10-27)와 2026-10-28 [빌드·Automation·DataValidation·허브 UI 결과](../tools/DEVELOPER_TOOLS.md#확인-결과와-남은-검증)를 구분한다. 사용자 설정 격리 보완 후 기존 실행기 회귀는 PowerShell 5.1·7 각 37개, Node 문서 검사 회귀도 통과했다. DryRun의 폴더·파일 미생성도 실제 확인했다. [후속 실기 검증](../tools/DEVELOPER_TOOLS.md#후속-실기-검증)의 로컬 템플릿 다인은 VT-01의 기술적 일부 관찰이고 새 ID 재입장은 VT-11의 동일 ID 재접속 통과가 아니며 게임 전체 M2 통과도 아니다. Live Coding 재등록·Auto Save 복구·DG 시나리오·다른 PC의 M3는 별도 검증이다.

## 프로젝트 필수 시나리오

| 검사 ID·요구 ID | 대상 | 수용 관찰·예외 | 실행 전 필요한 결정 |
|---|---|---|---|
| VT-01 / DG-001 | 1~4인 접속·이동 | 각 인원에서 진입/종료, 4인 원격 이동 | 세션·입장/이탈 정책 |
| VT-02 / DG-002~004, DG-006~007 | 굴착·보강·붕괴 | 굴착 구간만 붕괴, 보강 효과, 충돌/길/AI 일치 | 지형 방식·범위·여진 간격 |
| VT-03 / DG-005 | 깔림·구출 | 이동 불가·지속 피해·동료 구출, 취소/사망 경합 | 구출 시간·피해 수치 |
| VT-04 / DG-009~010 | 회수품·중량·탈출 | 동시 획득 단일 소유, 적재 경계·초과, 선택 회수, 사망·이탈·미탑승 시 소지/은닉품 전이 | 중량 한도·수치·문턱 범위 |
| VT-05 / DG-011~012 | 개인 정산·상점 | 반복 입력·저장 복원·재접속 뒤에도 정산 1회 반영(현재 상태 검사·런 정산 완료 기록), 구매/납부 경합, 구매 장비의 사망·일차 전환 시 유지/손실이 승인 규칙과 일치 | 값·가격·트랜잭션 범위 |
| VT-06 / DG-013 | 비공개 납부 | 다른 클라이언트가 개인 금액을 받지 않음, 공개 합계·시체 복구액을 통한 개인 금액 역산 검토(감속·장비·무게 단계 추측은 VT-10의 허용 범위) | 공개 정보 범위·호스트 신뢰 |
| VT-07 / DG-014 | 4일 루프 | 달성 후 할당량 상승·1일차, 미달 게임 오버, 중복 전환 방지, 런/일차 전환에서 장부·런 정산 완료 기록 보존 | 1~3일 효율·인정액 절사·이월 |
| VT-08 / DG-015 | 사망·시체 회수 | 시험 프로필의 잔고 100% 손실·시체 회수 시 사망자에게 50% 내림 복구가 한 번만 적용, 이미 납부한 금액·정산 전 수익 제외, 은닉 노출과 순손실 확인 | D25의 시험 비율 최종 확정·그 외 수치 |
| VT-09 / DG-007~008 | 던전 2종·절차 생성 | 폐교/피라미드 진입·귀환·연결 유효성·막힘 처리 | 생성 기준·구역 경계 전환 방식 PQ-03 |
| VT-10 / DG-016~017, DG-019 | 음성·은닉·표현 | 거리/상태별 음성 및 전달 금지 정책 채택 시 수신 차단, 히든 슬롯·UI 허용 정보, 숨기는 동작 공개와 내용·가치 비공개, 의도된 감속·장비 변화·무게 단계 추측과 상세 데이터 노출 구분 | 서비스·표시 세부 범위 |
| VT-11 / DG-001, DG-005, DG-014~015 | 장애·종료·재시작 | 신규 참가 잠금 구간 거절, 동일 ID 재접속 시 장부·소유권 복원(2026-11-05 사용자 결정 2: 이전 접속을 끊고 새 접속 수락. 작업리스트 1.1.0 S01·T55 원문의 '거절'을 대체), 호스트 이탈과 저장 실패·출발 로딩 실패 복구 | 저장 형식. 수동 저장 없음은 작업리스트 1.1.0 S30 확정(개발용 치트 명령은 Shipping 제외) |
| VT-12 / DG-001~019 | 성능·패키지 | 4인 굴착/붕괴/물리/AI 동시 측정, 깨끗한 환경에서 실행 | 최소 사양·프레임/대역폭 목표 |
| VT-13 / DG-008, DG-018 | 몬스터·트랩 기능 | 감지·추격·분리 반응, 트랩 피해, 원격 상태 일치, 굴착 후 경로 갱신을 L3/M2에서 관찰 | 몬스터 선정·종별 조건·선택 기능 채택 |

시스템 작업리스트 1.1.0의 T01~T93은 실행 전 수용 시나리오다. 특히 T31/T37/T45/T67은 시간 종료·중량 초과와 운행 중 처리, T51은 단독 구조 점유, T54/T62~T64는 조기 달성·마감 경합과 완료 후 저장, T55/T59는 재접속·공통 로딩 실패(준비·마감 유지, 일차 비소비)를 검사한다. T65는 일반 상승 2회 확정 상한(1회 비교 프로필 없음, 문 재개방 실패 시도는 횟수 미포함)이고 T14의 100% 손실·50% 복구는 D25 잠정 시험 프로필이다. T68~T75는 P2 외형·하우징 검증이다. T76~T93은 1.1.0에서 추가된 시나리오다. 위 VT는 이 시나리오를 묶는 검증 분류이며 실행 완료를 뜻하지 않는다.

T55 기대 결과의 "연결된 동일 ID의 추가 접속은 거절"은 2026-11-05 사용자 결정 2에 따라 **이전 접속을 끊고 새 접속을 받는다**로 대체해 판정한다(작업리스트 원문은 거절이며 기획이 다음 버전에서 고칠 예정). 탐사 중 신규 참가 거절과 호스트 이탈 시 종료 안내는 원문 그대로다. 전멸 시 장비 손실 범위는 작업리스트가 "시험 기준"이라고만 적어 판정 기준이 없으므로, 해당 관찰은 통과로 판정하지 않고 [PQ-21](../planning/DECISIONS.md) 결과를 기다린다.

상세 수용 조건은 시스템 작업리스트 1.1.0의 인수 시나리오(T) 시트를 기준으로 하며, [게임 기획](../GAME_DESIGN.md)은 DG↔S/T 색인과 Q 예시를 제공한다. 수치 미결은 통과로 판정하지 않으며, 미결 값과 독립적인 불변식은 먼저 검증할 수 있다. 경합은 공통 기준의 최소 10회와 실제 시드·지연·손실 조건을 기록한다.

## 장비·행동 표현 구현 후 추가 관찰

2026-11-04 추가. [장비 상태와 행동 표현](../design/DEVELOPMENT_SYSTEM_DESIGN.md#장비-상태와-행동-표현)의 후속 구현 수용 항목이며, 새 게임 규칙이나 테스트 통과 기록이 아니다. 1인·PIE 3인 예비 검사 후 M2(호스트+원격 3, 별도 프로세스)에서 실제 지연·손실 설정을 기록한다.

| 연결 VT | 검사 | 수용 관찰 |
|---|---|---|
| VT-04·05·10·11 | 장비 획득·교체·버리기, 동시 획득, 양손 운반 중 사용, 동일 ID 재접속·관련성 복귀 | 서버의 장착 상태와 사용 판정·소유자 UI·허용된 동료 외형 일치. 동시 획득 단일 성공, 거절 요청은 장부 불변. 인벤토리·히든 상세가 다른 클라이언트에 전달되지 않음 |
| VT-02·08·10·13 | 굴착·장비 행동의 서버 판정과 원격 연출 | 호스트·조작자·다른 클라이언트에서 허용된 동작이 보임. 효과 누락·중복이 지형/피해를 바꾸지 않음. 예측 채택 시 확인 응답의 중복 재생과 거절 정리 검사. 헤드리스·전용 서버 모드 자동 검사 시 표현 객체 없이 판정 가능(전용 서버 출시 구조 채택을 뜻하지 않음) |
| VT-10·11 | UI 재생성·장비 에셋 지연 로드·초기 기본값·연속 교체 | 현재 상태로 표현 복원, 오래된 로드 완료가 새 장비를 덮지 않음. C++ 호스트도 갱신되며 OnRep 순서에 의존하지 않음 |
| VT-01·10 | AimOffset·제자리 회전 도입 시 상하 조준·방향 반전·±180도·이동/낙하/사망 전환 | 로컬·호스트의 원격 캐릭터·Simulated Proxy 각각에서 기준축·스무딩 일치, 회전 상태 잔류·발 떨림 없음. 스켈레톤/모션 미준비는 미검증으로 보고 |
| VT-02·12 | 좌표 양자화 채택 시 굴착 경계·멀리 떨어진 좌표·반복 전송 | 변경 전후 판정/표현 오차가 정한 허용 범위 이내. 실제 전송량과 빈도 비교, 허용 오차·범위 미정이면 채택 완료로 처리하지 않음 |

## 2026-11-04 기획 답변(초안) 반영 후 추가 관찰

[결정 목록](../planning/DECISIONS.md#2026-11-04-기획-답변초안)의 기획 기준과 시스템 작업리스트 1.1.0·2026-11-05 사용자 결정을 구현한 뒤의 수용 관찰이다. 기획 답변과 작업리스트는 Notion 승인본이 아니며, 아래 항목은 실행 결과가 아니다. 문 닫힘 단계(DoorClosing)·문 닫힌 순간 목록 고정·문 닫힘 중 참가 잠금은 구현(실행 검증 일부)이다. 같은 ID 교체·관전 합류·사슬 소집·참가 실패 6종·붓기 납부·신호기·저장 시점·게임 오버 흐름은 2026-11-05에 구현했고 자동 테스트만 통과했다(격리 사본 Editor 빌드, `DIGETHER.Game` 165개 실패 0). 아래 관찰은 M2(실제 다인)·실제 BP·맵 연출로 아직 실행하지 않았으며, 모두 통과했다는 기록은 없다. 자동 테스트가 덮는 T는 [자동 테스트로 일부 덮인 T](#자동-테스트로-일부-덮인-t-2026-11-05)에 있다.

| 연결 VT | 검사 | 수용 관찰 | 수준 |
|---|---|---|---|
| VT-01·11 | 같은 ID 교체(2026-11-05 사용자 결정 2, 기획 답변 C2): 로비·탐사 중 같은 계정으로 두 번째 접속 | 이전 접속이 끊기고 새 접속이 같은 PlayerState·남은 Pawn·장부에 연결됨. 초기 지급·자리 배정 반복 없음. 이전 연결의 늦은 RPC가 반영되지 않음. 이전 접속을 끊지 못하면 ② 문구로 거절 | M2(PIE·별도 프로세스), Steam 세션 등록 해제·재등록 순서는 M3 |
| VT-01·11 | 참가 잠금 구간(T76): 로비, 출발 게이지 완료 직후·문 닫힘 중, 출발 로딩, 탐사, 정산, 실패 사건, 결과 크레딧 각각에서 신규 접속 | 게이지 완료 전 로비만 수락, 나머지는 해당 사유(예: ③ 탐사 중)로 거절. 게이지 완료 전 수락·이후 확정인 참가자는 빈자리가 있으면 관전 합류(그날 출발 인원·수익 제외) 또는 MVP 임시 거절과 이유 안내 | L2(규칙 단위), M2 |
| VT-01·10 | 참가 실패 문구 6종(2026-11-05 사용자 결정 3) | ①계정 확인 실패 ②같은 계정 접속 중 ③탐사 중과 정원 초과·버전 불일치·방 종료가 각각 구별된 문구로 표시. 최종 문구가 기획 승인 전이면 문구 확정은 미검증으로 보고 | L2, M2(Steam 계정 확인 실패는 M3) |
| VT-04 | 엘리베이터 문 닫힘 구간(갱도 `ELV_DOOR_CLOSE_TIME`·거점 `LOBBY_DOOR_CLOSE_TIME`, 개발 임시 3초) | 게이지 완료 시 문이 자동으로 닫히고 완전히 닫힌 순간 목록 고정, 닫히는 중 걸어서 탑승 포함(문턱 밀어내기 없음). 닫히는 중 무게 초과는 문 재개방·게이지 0·상승 횟수 불변. 닫히는 중 시간 종료는 갱도 대기 처리, 문 닫힘 완료와 시간 종료가 같은 처리 시점이면 문 닫힘 우선. 거점은 문 닫힘 완료 순간 로비의 엘리베이터 밖 참가자를 사슬 소집(MVP: 암전 후 순간이동)해 출발 인원에 포함 | L2, M2 |
| VT-10 | 음성 거리(G 시험값) | 3m 안 원음, 25m 밖 무음, 거리 감쇠와 방향감, 같은 구역 벽 추가 감쇠 없음, 갱도↔던전 차단, 매몰자 먹먹함·최대 거리 절반 | M3(실제 Steam 음성, 다른 PC) |
| VT-05 | 분배 방식 선택(B3) | 탈출 시점형 선택 시 Q01~Q04 비교 예시 값과 일치. 합산형 기본값 회귀 없음 | L1~L2 |

## T 시나리오와 VT 대응

T 번호는 시스템 작업리스트 1.1.0 인수 시나리오(T01~T93)의 식별자다. 아래는 모든 T를 검증 묶음에 연결한 색인이다. 동일 시나리오가 여러 도메인에 걸치면 관련 VT를 함께 실행하며, 테스트 결과가 아니라 구현 후 관찰 계획이다.

| VT | 연결 T | 관찰 초점 |
|---|---|---|
| VT-01 | T01, T43, T44, T55, T76, T77 | 접속·자동 출발·엘리베이터 복귀·같은 ID 재접속(교체)·참가 잠금 구간·관전 합류·거점 퇴장 |
| VT-02 | T02, T03, T19, T50, T81, T85 | 굴착 가능 대상·동시 수정·보강·충돌·국소 붕괴 예고·목재 2종 |
| VT-03 | T19, T32, T51, T80, T82 | 매몰·1인 사망·단독 구조 점유·단계 갱신·맨손 구조 |
| VT-04 | T04, T05, T06, T07, T31, T35~37, T39, T41, T44~48, T52~53, T56, T65, T67, T83, T93 | 소유·운반·탈출 명세·중량·운행과 긴급 탈출·상승 중 피해·밖 레버 |
| VT-05 | T08~13, T15~18, T22, T36~37, T52, T57~58, T79, T85, T86 | 일반/히든 지급·장비·상점·해금·중복 거래·신호기·초과 금화·기본·공용 장비 |
| VT-06 | T13, T17, T21, T42, T52, T60, T68~71, T79, T87 | 개인 정보·관전·음성·외형 공개 경계·검은 정산 화면 |
| VT-07 | T25~30, T33~34, T38, T54, T58, T62~64, T78 | 일차·조기 달성·마감·게임 오버·새 게임·4일차 동시 마감 납부 |
| VT-08 | T08~16, T31~32, T37, T41~42, T47, T51, T53, T83, T84, T90 | 사망·전멸·시체 복구·소지품 전이·거점 피해 0 |
| VT-09 | T01~03, T20, T56~57, T92 | 생성 구역·이동·지형·갱도 접근·던전 경계 |
| VT-10 | T13, T21, T39~40, T52, T60, T68~71, T82, T87~90 | 음성·은닉·광원·외형·안내·지진 스캐너 |
| VT-11 | T12, T22~24, T33~34, T38, T55, T59, T64~65, T72~77, T86, T91 | 반복 입력·저장·저장 실패·Steam ID 재접속·빌드 버전 차단·출발 실패 복구·배치 |
| VT-12 | T24, T40, T61, T65~67 | 지연·튜닝·이동 기준·프로필과 성능 측정 |
| VT-13 | T20, T49, T51, T83, T92 | 몬스터 경로·공격·상태 및 표시 |

작업리스트 1.1.0에서 추가된 T76~T93의 대응은 다음과 같다.

| T | 시나리오 | VT |
|---|---|---|
| T76 | 관전 합류 | VT-01, VT-11 |
| T77 | 거점 퇴장·장비 남김 | VT-01, VT-11 |
| T78 | 4일차 동시 마감 납부 | VT-07 |
| T79 | 신호기·초과 금화 | VT-05, VT-06 |
| T80 | 매몰 단계 갱신·이탈 | VT-03 |
| T81 | 국소 붕괴 예고 | VT-02 |
| T82 | 맨손 구조·위치 표시 | VT-03, VT-10 |
| T83 | 상승 중 피해·몬스터 | VT-04, VT-08, VT-13 |
| T84 | 전멸 시체 복구 | VT-08 |
| T85 | 목재 2종 | VT-02, VT-05 |
| T86 | 기본·공용 장비와 긴급 삽 | VT-05, VT-11 |
| T87 | 검은 정산 화면 | VT-06, VT-10 |
| T88 | 기본 안내·툴팁 | VT-10 |
| T89 | 지진 스캐너 | VT-10 |
| T90 | 거점 공격·치장 | VT-08, VT-10 |
| T91 | 저장 실패 | VT-11 |
| T92 | 던전 몬스터 경계 | VT-09, VT-13 |
| T93 | 밖 레버 화물 단독 상승 | VT-04 |

### 자동 테스트로 일부 덮인 T (2026-11-05)

아래 T는 **자동 테스트 일부, M2 미검증**이다. 격리 사본 Editor 빌드에서 `DIGETHER.Game` 자동 테스트 165개가 실패 0으로 끝났다(2026-11-05). 테스트는 규칙·단일 프로세스 월드 단위이며 원문 기대 결과 전체나 실제 다인 접속·BP·맵 연출·음성을 확인하지 않는다. 수용 완료로 처리하지 않고 위 VT의 M2 관찰을 따로 실행한다. 테스트 이름은 `DIGETHER.Game.` 접두를 뺐다.

| T | 관련 자동 테스트 | 덮지 않는 것 |
|---|---|---|
| T17 | `Economy.PourIntegration.ConcurrentAndDisconnect`, `Economy.PourIntegration.ReplicatedPropertiesHideTotals` | 실제 클라이언트 화면의 비공개 확인 |
| T18 | `Economy.PourIntegration.OverflowFloorCoinPickup`, `Economy.Efficiency` | 입력 에셋(IA/IMC) 연결, PQ-34 기획 확인 |
| T23 | `Framework.SaveIntegration.HostExitSavesHubButNotExploration` | 실제 앱 종료·재실행 |
| T28 | `Run.GameOver.FlowSuctionWaitAndCreditsOnce`, `Framework.SaveIntegration.GameOverSettlementFailureCreditsNewGame` | 우물 Actor 배치, 침수·흡입 연출 BP |
| T29 | `Run.GameOver.ResultStatsT29`, `Save.GameOver.ResultStatsRoundTrip`, `Framework.SaveIntegration.GameOverSettlementFailureCreditsNewGame` | 크레딧 WBP 표시 |
| T33 | `Run.GameOver.CreditPlaybackSameForAllAndFastForward`, `Framework.SaveIntegration.GameOverSettlementFailureCreditsNewGame` | 여러 클라이언트의 재생 동기, 크레딧 전원 음성(미구현) |
| T34 | `Run.GameOver.LeaverNotWaitedAndDeadlineForcesSuction`, `Framework.SaveIntegration.GameOverLeaverNotWaitedAndKeptInResult` | 실제 방 나가기·앱 종료 |
| T38 | `Run.GameOver.FlowSuctionWaitAndCreditsOnce`, `Save.Slots.SubsystemWritesOnlySelectedSlotAndLists`, `Framework.SaveIntegration.SavedGameOverStartsSameSlotNewGame` | 슬롯 선택 WBP |
| T55 | `Online.JoinRules.LobbyDuplicateReplace`, `Online.JoinRules.GameplayDuplicateReplaceThenRejoin`, `Framework.SaveIntegration.DuplicateLoginReplacesPreviousConnectionDuringRun`, `Framework.SaveIntegration.DuplicateLoginInHubReleasesSeatThenJoins` | 실제 두 프로세스 접속, Steam 세션 처리(M3) |
| T58 | `Economy.PourIntegration.CompletionStartsDayOne` | 신호기·금괴 연출 |
| T59 | `Framework.SaveIntegration.CommonLoadingFailureKeepsPreparationDeadlineAndDay` | 실제 로딩 계층이 없어 호출 경로 없음 |
| T62 | `Economy.PourIntegration.CompletionCancelsDepartureLever` | 경고 연출 BP |
| T63 | `Economy.PourIntegration.CompletionCancelsDepartureLever`, `Economy.PourIntegration.DeadlineBeforePourRejects` | 실제 다인 동시 입력 |
| T76 | `Online.JoinRules.SpectatorJoinDuringLock`, `Framework.SaveIntegration.JoinAcceptedBeforeGaugeConfirmedAfterSpectates` | 수락 시점 정의(PQ-25), 실제 접속 |
| T77 | `Framework.SaveIntegration.HubDepartureReleasesSeatAndRemovesBody`, `Framework.SaveIntegration.HubLeaverReturnsDuringRunAsSpectatorThenJoinsNextDeparture` | 장비 바닥 남김(인벤토리 미구현) |
| T78 | `Economy.PourIntegration.SameFramePourBeforeLever` | 실제 다인 동시 입력 |
| T79 | `Economy.PaymentSignal`, `Economy.PourIntegration.OverflowFloorCoinPickup`, `Economy.PourIntegration.SignalSaveRestoreAndGameOver` | 신호기 HUD·외형, PQ-34 기획 확인 |
| T84 | `Economy.DeathAndCorpse`, `Economy.Settlement` | 실제 탐사의 시체 운반·상승 |
| T91 | `Framework.SaveIntegration.AutosaveFailureRetriesWithoutBlockingDeparture`, `Framework.SaveIntegration.HostExitSavesHubButNotExploration` | 실패 알림 UI |
| T93 | `Elevator.AscentLeverPair`, `Elevator.CargoOnlyAscent`, `Framework.SaveIntegration.OutsideAscentLeverSendsElevatorWithoutPuller`, `Framework.SaveIntegration.AscentLeversAcceptOnlyFirstPull` | 화물 적재 기능 미구현이라 화물 회수는 규칙 단위만, `L_GameTest` 밖 레버 실제 조작 |

T66의 캐릭터 기준값은 VT-12에서, T57 갱도 해금은 VT-05/09에서 확인한다. T68~75는 P2 외형·하우징 범위로 별도 수용하며 도입 전 완료로 처리하지 않는다. 원문 전체 조건을 대조하고 시험 프로필·환경·미결을 함께 기록한다.
## 결과 양식

검사 ID / 요구 ID / 기획 개정 / 엔진·플러그인 / 대상 커밋과 미커밋 diff 또는 해시 / 명령·단계 / PC·프로세스·인원 / 예상·실제 / 통과·실패·skip·미실행 수 / 종료 코드 / 로그 경로 / 알려진 한계.

로컬 도구 명령은 위 실행기를 사용하며 예상 테스트 이름·개수는 해당 실행 시 소스에서 수집한다. DG 시나리오의 자동 테스트·실제 환경은 구현 단계에 확정한다. 공유 결과는 `docs/verification/reviews/`의 날짜별 기록 또는 Issue/CI 아티팩트로 보존한다. 테스트 0개·필수 단계 누락·타임아웃을 구분한다.

M3 이후에도 출시 대상·라이선스·패키징·알려진 결함·배포 및 복구 책임을 확정한 [릴리스 양식](../templates/RELEASE_OPERATIONS_TEMPLATE.md)을 채워야 한다. 이번 작업은 배포를 수행하지 않는다.
