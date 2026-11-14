# DIGETHER Unreal 제공 기능과 개발반 구현 범위

기준일: 2026-10-28 · 상태: **DG 게임 기능은 기술 조사와 적용 제안, 에디터 편의 도구는 적용·빌드·자동검사·허브 UI 확인**. 도구 사용법과 실제 검사 범위는 [개발 도구 안내](../tools/DEVELOPER_TOOLS.md)를 따른다. 현재 엔진 기준은 UE 5.8.4이며([ADR-003](../decisions/ADR-003-engine-version-5.8.4.md)) 아래 5.8.2 표기는 조사 시점의 기록이다. 표의 구현 상태 서술(접속·음성·저장)은 2026-11-10 코드 대조로 정정했고, DG 기능별 구현·검증 범위는 [구현 현황](IMPLEMENTATION_STATUS.md)에 둔다.

개발반이 엔진 기능을 재사용할 부분과 게임 규칙을 작성할 부분을 구분한다. 전체 책임·수명과 데이터 인계는 [전체 시스템 설계](../SYSTEM_DESIGN.md), 착수용 기술 계약은 [개발반 시스템 설계](DEVELOPMENT_SYSTEM_DESIGN.md), 기획 근거는 2026-10-28 자료를 반영한 [이관 전 기획 초안](../GAME_DESIGN.md)이다. 온라인 Notion 원문·승인 개정은 이번 작업에서 조회하지 않았다. 다음 표의 선택은 채택 확정이나 기능 구현 완료가 아니다. 영역별 UE 5.8.4 기능 목록과 근거는 [기능 카탈로그](../unreal/features/README.md)(2026-11-04)를 따르며, 같은 지원 주장(API·플러그인 상태 등)이 이 문서의 5.8.2 시점 확인과 다르면 카탈로그의 5.8.4 직접 확인을 우선해 이 문서를 갱신한다. 과거 실행 검증 기록과 프로젝트 결정은 이 우선 규칙의 대상이 아니다.

## 1. 확인 수준과 버전

- 이 작업 환경의 `Engine/Build/Build.version`에서 **5.8.2 / Changelist 56702186 / ++UE5+Release-5.8**을 확인했다. 팀 전체 설치·SDK·빌드 성공을 뜻하지 않는다.
- 2026-10-25 최초 조사에서 설치된 엔진의 관련 헤더와 `.uplugin`을 읽었다. 당시 없었던 `.uproject`는 현재 `DIGETHER/DIGETHER.uproject`에 존재한다. Workbench의 과거 빌드·실행 증거와 이번 편의 도구 검증을 아래 DG 게임 기능의 구현 결과와 구분한다.
- 아래 Epic 공식 페이지는 열람/검색 결과에서 UE 5.8로 표시됐다. 웹 문서는 패치 5.8.2의 모든 조합을 보장하지 않는다. 로컬 소스의 존재, 문서의 지원, 프로젝트 실행 성공을 별도로 기록한다.
- Enhanced Input·PCG·GameplayAbilities의 로컬 descriptor는 `IsBetaVersion=false`였다. GeometryScripting은 Beta/Experimental 모두 false였다. 이는 모든 하위 기능·플랫폼이 안정적이라는 판정이 아니다. Online Services 공식 문서는 **Beta**라고 명시한다. descriptor에 표시가 없는 항목을 안정 버전으로 추정하지 않는다.

## 2. 기능별 제공 경계

표의 '직접 작성'은 엔진을 대체한다는 뜻이 아니라 엔진 위에 DIGETHER 규칙을 구현한다는 뜻이다. 근거 E 번호는 5절에 연결된다.

| 요구 | UE가 제공하는 기반 | 개발반이 직접 작성·연결할 것 | 적용 전 제약·판정 |
|---|---|---|---|
| DG-001 이동·입력 | Character/CharacterMovement의 이동·예측·보정, Enhanced Input의 Action/Mapping Context [E01, E02] | 상호작용 입력, 도구 사용, 깔림 이동 제한; 히든을 포함한 중량에 따른 이동 영향, 조작 UI | 별도 이동 복제 시스템부터 만들지 않는다. 특수 이동은 예측/보정 확장 검사 |
| DG-001 접속·런 | GameMode/GameState/PlayerState, Actor 복제·RPC [E03, E04]; 온라인 세션 인터페이스 [E05] | 런 단계, 입장 가능 상태, 준비·종료·이탈 처리, 서비스 비동기 결과/취소 | 시스템 작업리스트 1.1.0은 호스트형 방, 신규 참가 차단(출발 게이지 완료~결과 크레딧, 관전 합류), 거점 이탈 시 캐릭터 제거·장비 바닥, 탐사 중 끊김 재접속 복원을 명시한다. 같은 ID는 이전 접속 종료 후 새 접속 수락(사용자 결정 2026-11-05. `PreLoginAsync`로 구현, 자동 테스트 통과·M2 미검증. 엔진 `KickPlayer`는 Pawn을 먼저 파괴하므로 그대로 쓰지 않음). 세션은 Online Subsystem 세션 인터페이스 위의 `UDigetherSessionSubsystem`으로 구현했고 시험용 Steam App ID 480을 쓴다. 정식 App ID와 세션 서브시스템(Steam/EOS) 확정은 미결이며 검색 성공과 맵 입장 성공은 별도. 동일 ID 재접속과 엔진 기본 종료 동작의 차이는 전체 시스템 설계의 재접속 기술 계약 참조 |
| DG-002,006 굴착·벽 파괴 | Geometry Scripting의 메시 연산·DynamicMesh, Chaos의 사전 파쇄 Geometry Collection [E06, E07] | 굴착 가능 영역·깊이·도구 규칙, 수정 이력/버전, 충돌 반영, 서버 검증·복제·복구 | 완성형 온라인 굴착 게임 시스템을 제공하는 근거는 확인하지 못함. 메시 편집과 동기화는 별도 |
| DG-003,004 여진·보강(보강은 2026-11-14 런 플로우 v1.1.0에서 Could) | 타이머·GameState의 GetServerWorldTimeSeconds(클라이언트 추정 서버 시각)·복제 상태, Chaos 파괴/필드·Niagara/오디오 연동 [E03, E07] | 여진 일정·예고, 후반 기준, 보강 유효 범위, 손상·보수와 붕괴 조건, 붕괴 대상 판정 | 물리 파편 접촉만으로 깔림/피해 결과를 각 클라이언트가 결정하지 않음 |
| DG-005 깔림·구출 | CharacterMovement, 타이머·Actor/Component [E01, E03] | 생존/깔림/사망 상태, 지속 피해, 구출 진행·취소·거리 검사 | 1인 3단계 매몰은 사망, 전원 매몰은 주기 피해, 구조는 매몰 흙 굴착·여러 명 동시(2026-11-14 캐릭터 기획서). GAS는 확장 후보이며 필수 채택 아님 |
| DG-007 갱도 맵 선택·로드 | 레벨 로드·스트리밍 [E16], PCG 그래프·런타임 생성과 메시·액터 배치(도구 후보, [E08]) | 탐사마다 사전 제작 갱도 맵 여러 개 중 하나를 선택·로드, 기존 규칙에 따른 던전·물품 배치, 연결·귀환 가능성 검증과 준비 실패 처리 | 런 플로우 v1.0.1 6.1 기준으로 갱도 형태를 매번 새로 만들지 않음. PCG는 채택 전 후보이며 플레이 가능한 길·동일 네트워크 결과를 자동 보장하지 않음 |
| DG-008 던전·구역 경계 | Actor·레벨 스트리밍·서버 travel [E03, E16] | 폐교/피라미드 구성, 진입·귀환 목적지, 이동 중 상태 보존과 실패 처리 | 같은 월드 스트리밍/서버 travel 비교 후 채택. World Partition은 규모 근거 없이 필수화하지 않음 |
| DG-009,010 운반·엘리베이터 | Actor 부착·상호작용 및 상태 복제 [E03, E04] | 물건 ID·단일 소유권·적재 목록, 논리 중량 합산, 탈출 판정 | 아이템은 물리 충돌 없이 동작한다. 기획 중량은 별도 계산하고 실제 이동·연출은 프로토타입으로 검증 |
| DG-011~014 정산·상점·납부·4일 주기 | DataAsset, 복제/RPC, GameMode 기반 [E03, E04, E10] | 지갑·원자적 거래·중복 방지·가격 검증·효율/반올림·할당량 분기 | 엔진 온라인 Commerce는 게임 내 개인 지갑/납부 규칙을 대신하지 않음. 경제 값과 인정액 산식은 D22·D27 |
| DG-015 사망·시체 회수 | Actor 수명·상태 복제 [E03, E04] | 손실 확정·시체 식별·복구 자격·1회 복구, 사망/정산 경합 | 사망 직전 잔고의 50% 내림을 사망자에게 복구하는 것은 D25 시험 기준. 시체와 복구 권리 데이터를 분리 |
| DG-016 근거리 음성 | 엔진 VoIP/VOIPTalker 및 Voice Chat Interface·제공자 플러그인 후보 [E11] | 인증/채널 수명, 거리·상태 규칙, 장치 UI, 끊김/퇴장 정리 | 구현은 엔진 기본 VoIP(2026-11-04 사용자 결정). 마이크 모드·서버 수신 제한·수신 측 거리 감쇠·관전자 분리의 1차 구현이 `Game/Voice/`에 있고 실제 음성 전달은 다른 PC 검증(M3) 전이다. 2026-11-11 기존 구역 볼륨 차단·서버 매몰 투영·발화자별 LPF/거리 처리를 코드에 연결했다. 실제 매몰 원본·제작 맵 배치·M3는 별도다([연결 가이드](../unreal/VOICE_ENVIRONMENT.md)). 거리 시험값은 2026-11-04 기획 답변(초안) G(제안): 3m 원음·25m 최대·자연 감쇠·방향감, 갱도↔던전 차단, 매몰자 먹먹함·거리 절반. 공간 음성 동작을 실제로 검증. Sound Attenuation만 설정해 음성 통신이 완성되지 않음 |
| DG-017 히든 슬롯 | 조건부 프로퍼티 복제·소유 연결 [E04] | 슬롯 용량/허용품·이동·정산, 소유자 정보 뷰, 허용된 추측과 금지된 상세 노출 구분 | PlayerState는 전체에 존재하므로 비밀 필드를 무조건 공개 복제하면 안 됨 |
| DG-018 몬스터·트랩 | AIController·AI Perception·Behavior Tree·Navigation [E12] | 몬스터별 감지 반응·추격/분리·피해, 트랩 발동, 굴착 후 재탐색/막힘 처리 | 감지·경로 도구가 행동 패턴을 만들어주지 않음. 피해는 서버 판정 |
| DG-019 UI·카툰 표현 | UMG, Materials/Post Process, 애니메이션·Niagara [E13, E07] | 허용 정보만 보여주는 화면, 외곽선/해칭 재질, 장비·상태 연출, 아트 통합 | 카툰 최종 룩은 제작물. 지원 렌더러·AA·투명체·목표 사양에서 아트 승인 필요 |
| 횡단: 데이터·저장 | DataAsset·SaveGame 직렬화/슬롯 API [E10, E14] | 승인 수치 바인딩·검증, 저장 스키마/버전·마이그레이션·복구 범위 | 시스템 작업리스트 1.1.0 S30: 수동 저장 없음(D29 닫힘), 탐사 정상 완료 후·할당량 달성·호스트 정상 거점 종료·5일차 실패 귀환 자동 저장, 저장 슬롯 여러 개(Notion 승인 전). SaveGame은 서버 DB·보안·거래 원자성·클라우드 저장 보장이 아님 |
| 횡단: 복제 규모 | Actor 휴면·거리 컬링·갱신 빈도·소유자 관련성 [E04] | 대상별 휴면/깨우기 시점, 재진입 시 최신 상태 적용, 연출 파편 비복제 | Legacy/Iris 선택에 따라 구성이 다름. 대상 수·대역폭 목표는 미정, 개발반 시스템 설계의 복제 규모 절 참조 |
| S32 / R01~R14 튜닝 | DataAsset·표·커브와 노드형 편집 도구 후보 [E10, E12] | 파라미터 ID·단위·범위·버전, 테스트 빌드의 호스트 조정과 동기화, 프로필 및 비교 가능한 원본 | 구체 파이프라인·Gameplay Debugger/Visual Logger의 UE 5.8.2 지원은 미검증. R05~R08은 BT 설계 완료가 아닌 편집·디버그 환경 요청 |
| 횡단: 검증·성능 | Unreal Insights, Automation Test·Functional Test [E15] — 사용 기준은 [Unreal 테스트 기준](../unreal/TESTING.md) | VT-01~13 시나리오, 거래/전환 회귀, 패키지·실제 4PC 증거 | 프로파일러 존재는 예산 달성을 뜻하지 않음. 성능 수치는 미정 |

## 3. 개발자가 따라갈 최소 적용 순서

다음 절차는 현재 프로젝트에서 DG 게임 기능을 연결할 때의 가이드이며, 이 문서의 조사만으로 실행된 단계가 아니다. 플러그인은 Editor의 `Edit > Plugins`에서 필요한 항목을 확인하고, 실제 타깃의 runtime/editor 모듈을 구분해 `.uproject`와 Build.cs에 반영한다. 현재 편의 플러그인 구성은 [개발 도구 안내](../tools/DEVELOPER_TOOLS.md#편의-플러그인-적용-범위)를 확인한다.

### A. 입력·캐릭터·상태 복제

1. ACharacter 파생 클래스와 기본 CharacterMovement로 이동부터 검증한다. Enhanced Input의 Input Action/Mapping Context를 만들고 로컬 플레이어의 `AddMappingContext`, 입력 컴포넌트의 `BindAction`으로 연결한다. 두 심볼은 설치된 5.8.2 헤더에서 확인했다.
2. 로컬 입력은 소유 PlayerController/복제 컴포넌트의 Server RPC로 게임 요청을 보낸다. 대상 월드 액터를 사용자가 소유한다고 가정하지 않는다.
3. 서버가 거리·상태·빈도를 현재 상태로 확인해 결과 상태를 변경한다. `ReplicatedUsing`과 `GetLifetimeReplicatedProps`로 지속 상태를 전달하고 OnRep/호스트 공통 표현 함수를 작성한다. RPC는 반환값이 없으므로 결과는 복제된 상태 변화로, 거절 사유는 소유 클라이언트용 `Client_` RPC로 전달한다.[E04]
4. 공개 상태와 소유자 전용 뷰를 분리한다. 조건부 복제는 OwnerOnly 등 실제 연결 조건을 확인해 적용한다. GameInstance는 각 프로세스에 독립적으로 존재하며 복제되지 않는다.[E03]
5. VT-01/06에서 원격 클라이언트 입력, UI 늦은 생성, 재수신, 비소유자의 원시 수신 필드를 검사한다. 기본 이동도 호스트 화면만 보고 완료 처리하지 않는다.

### B. 굴착·붕괴 기술 비교

| 후보 | 기본 기능 활용 | 추가 개발과 위험 | 선택 근거로 만들 실험 |
|---|---|---|---|
| 셀/청크 상태 + 메시 표현 | Actor/Component·메시·충돌 | 셀 자료구조·표면 생성·청크 경계·전송·저장 직접 작성 | 같은 구간 동시 굴착, 청크 경계, 수정 후 충돌/AI, 장시간 누적 비용 |
| 사전 제작 파괴 구간 | Geometry Collection/Chaos 또는 구간별 액터 교체 | 허용 파괴 단위·보강/통행 판정·복제 규칙 | 기획의 자유도 충족 여부, 잔해 정리·깔림·통로 재개방 |
| DynamicMesh + Geometry Scripting | `ApplyMeshBoolean`, `UDynamicMeshComponent::UpdateCollision` | 연산 비용·비동기 결과 수명·버전·충돌/내비 반영·네트워크 전송 | 연속 절삭의 메시 품질, 충돌 갱신 지연, 동일 수정 복원·패키지 실행 |
| 외부 지형 플러그인 | 후보 선정 후 조사 | 버전·라이선스·멀티플레이·플랫폼·유지 비용 | 위 실험과 같은 조건. 현재 제품 선정/도입 없음 |

초도 구현은 첫 행(셀/청크 상태 + 메시 표현)을 `UProceduralMeshComponent`(ProceduralMeshComponent 플러그인)로 채택했다([굴착 구현 설계](EXCAVATION_IMPLEMENTATION.md)). 나머지 행은 넓은 갱도로 확장할 때의 비교 후보로 남긴다.

`ApplyMeshBoolean`은 GeometryScriptingCore의 BlueprintCallable 선언, `UpdateCollision`과 `bUseAsyncCooking`은 GeometryFramework 헤더에서 확인했다. 이 API들은 안전한 작업 큐·충돌 준비 완료·복제를 자동 구성하지 않는다. Geometry Scripting에는 Editor 전용 기능도 있으므로 실제 사용하는 함수마다 runtime 모듈과 패키징 가능 여부를 확인한다.[E06]

실험 순서는 작은 파괴 구간 → 충돌/통행 → 서버 수정 번호와 클라이언트 적용 → 보강/붕괴 → AI 재탐색 → 4인 지연·손실 → 패키지다. 화면에 구멍만 생긴 상태를 통행 가능한 굴착으로 판정하지 않는다. 클라이언트마다 메시/물리 연산이 같을 것이라 가정하지 말고 권위 결과 또는 재구성 명세와 복구 경로를 정한다. 큰 메시 전체를 매번 Reliable RPC로 보내는 구조는 채택 전에 대역폭을 측정한다.

### C. 갱도 맵 선택·배치·던전·AI

1. 먼저 서버가 탐사마다 선택한 사전 제작 갱도 맵과 기존 규칙에 따른 던전·물품 배치 명세를 정한다. 갱도 형태를 매번 새로 생성하는 요구가 아니다(런 플로우 v1.0.1 6.1). 배치에 시드를 쓰더라도 시드만 같으면 모든 플랫폼의 결과가 같다는 가정은 금지한다.
2. PCG 플러그인의 그래프/컴포넌트는 던전·물품 배치 도구 후보로 검토하며 채택은 별도다. 게임 경로의 연결성·귀환 가능성은 별도 검사 후 런 준비 완료를 공개한다. 실패 시 무한 재시도하지 않는다.[E08]
3. Navigation Mesh의 Runtime Generation은 Static / Dynamic Modifiers Only / Dynamic 차이를 확인한다. 새 통로 지오메트리를 만드는 굴착에 Modifiers Only만으로 충분하다고 가정하지 않는다.[E12]
4. AIController + AI Perception + Behavior Tree로 대표 몬스터 하나를 구성하고 서버에서 감지·추격·피해를 판정한다. 복제할 표현 상태를 따로 지정한다. 트랩은 대상 중복 접촉·쿨다운/재발동 규칙을 검증한다.
5. VT-02/09/13에서 갱도 맵 선택·로드, 두 던전 왕복, 배치·준비 실패, 붕괴 후 경로 갱신 중 행동을 관찰한다. World Partition/StateTree/EQS는 필요가 입증될 때 추가 후보로 조사한다.

### D. 운반·경제·비공개 정보

1. DataAsset 기반 회수품 정의에 안정적인 정의 ID·승인 개정·중량·가치 참조를 둔다. 각 생성 물건의 Instance ID와 정의 ID를 구분한다.[E10]
2. 아이템은 충돌 없이 동작한다는 현재 기획을 유지한다. 서버가 소유와 슬롯을 확정하고 운반·설치·놓기 표현을 연결한다. 강체 물리 운반은 현재 기획 요구로 가정하지 않는다.
3. 무게·탈출·정산 결과는 물리 프레임과 분리한 권위 목록에서 계산한다. 구매/납부/사망은 같은 지갑 갱신 경로에서 직렬 처리하며 중복 요청에 재차감하지 않는다. 거래의 검증부터 커밋까지 비동기 대기로 쪼개면 예약/취소 경로가 필요하다.
4. 공개 GameState에는 승인된 공용 값만 둔다. 개인 지갑·납부 내역·히든 슬롯 상세는 소유자 뷰로 제한한다. 로그·UI·공개 합계 변화·시체 복구액·중량으로 추론되는 범위를 기획과 검토한다. 리슨 호스트의 메모리까지 비밀을 보장한다고 하지 않는다.
5. VT-04~08에서 동시 획득·구매/납부·사망/정산·중복 회수·음수/초과·잘못된 ID를 검사한다. SaveGame 저장을 추가해도 런타임 거래 원자성과 영구 장애 복구는 별도다.

### E. 온라인 세션·근거리 음성

2026-11-04 사용자 결정으로 엔진 VoIP + `UVOIPTalker` 경로를 선택했다. 아래 비교와 절차는 선택 전 조사 기록이며, Voice Chat Interface + 제공자 경로는 채택하지 않았다(`.uproject`에 `EOSVoiceChat` 플러그인이 켜져 있으나 `Game/` 코드는 쓰지 않는다. 유지 여부는 확인 대상). 조사 당시 음성 후보는 엔진 VoIP + `UVOIPTalker` 경로와 Voice Chat Interface + 제공자 경로였다. 로컬 `Net/VoiceConfig.h`에서 VOIPTalker와 부착 대상·감쇠 설정을 확인했다. 전자는 해당 Online Subsystem의 실제 VoIP 지원과 서버 중계 경로를 검증하고, 후자는 제공자 서버의 채널/권한 제어 가능 범위를 검증한다. VOIPTalker 자체는 전송·인증 서비스가 아니다. VoiceChat 모듈은 ClientOnly, EOSVoiceChat은 ClientOnlyNoCommandlet로 선언돼 있어 전용 서버에서 같은 클라이언트 모듈을 로드하는 설계는 사용하지 않는다.[E11]

살아 있는 탐사자와 관전자의 음성 경로는 분리하며, 거리 감쇠와 서비스의 실제 전달 제어를 검증한다. 전달 금지 요구가 있으면 서버 중계 필터 또는 제공자 서버의 권한/채널 제어에서 집행하고, 로컬 볼륨 0을 정보 보호로 판정하지 않는다. 엔진 경로의 `UNetDriver::ReplicateVoicePacket`/`ShouldReplicateVoicePacketFrom`, `APlayerController::GameplayMutePlayer`는 조사 후보이며 실제 선택한 서비스에서 거리 규칙을 집행하는지 별도 확인한다. VT-10/M3에 금지 수신자가 음성 패킷/데이터를 받지 않는지 검사를 포함한다.

1. 호스트형 입장과 관전 음성 규칙을 반영하고 플랫폼·제공자를 정한다. Online Subsystem과 Beta Online Services 중 제공자 지원과 배포 조건에 맞는 한 경로를 선택한다. 서로 다른 인터페이스의 샘플을 혼합하지 않는다.[E05]
2. 생성/검색/참가/종료와 실패/취소를 서비스 어댑터에 모은다. UI 요청 세대와 현재 월드를 확인해 늦게 온 성공 콜백이 종료된 런을 다시 열지 않게 한다.
3. Voice Chat 플러그인과 실제 제공자를 선택한다. 사용자 로그인→채널 참가→장치/음소거 설정→위치/거리 규칙 적용→채널 퇴장→로그아웃 수명을 연결한다. 인터페이스 존재만으로 제공자 계정·인증·채널 생성이 끝난 것이 아니다.[E11]
4. 공간 채널/위치 API 지원은 제공자별로 확인한다. 지원하지 않으면 해당 경로를 채택하지 않거나 별도 공간 처리 설계를 비교한다. 비밀 인증값을 프로젝트 공유 문서에 넣지 않는다.
5. 검증 M3의 실제 4PC에서 거리·사망/깔림 정책·장치 변경·재접속·퇴장 후 잔류 음성을 검사한다. 로컬 PIE는 외부 서비스 검증을 대체하지 않는다.

### F. UI·아트·저장·측정

- UMG 위젯은 서버 상태를 읽어 표시하고 요청만 보낸다. 위젯 제거/재생성 때 구독을 정리하고 현재 상태를 다시 적용한다. 로컬 `UUserWidget` 헤더 존재를 확인했으며 실제 화면 구성은 개발반 작업이다.
- Post Process Material의 외곽선/해칭과 아트 재질을 작은 승인 장면에서 비교한다. 전용 서버에서 위젯·오디오·재질 객체가 없어도 게임 판정이 가능해야 한다.[E13]
- 진행 저장을 구현할 때 `USaveGame` 파생 데이터에 스키마 버전과 저장 범위를 정의하고 저장/로드 실패를 처리한다. 런 데이터 보존과 영구 세이브를 혼동하지 않는다.[E14]
- Insights로 굴착 계산·충돌 생성·내비 갱신·AI·물리의 비용을 나누어 측정한다. 기준 맵/시드·참여자 수·수정 횟수를 고정해 비교하고 승인된 예산으로 판정한다.[E15]

## 4. 채택 전에 남길 증거

기능별로 `DG ID / 엔진 빌드(현재 5.8.4) / 플러그인·모듈 / 복제 시스템(Legacy 또는 Iris, 프로젝트 설정 확인) / 적용 설정·API / 기본 지원 범위 / 추가 코드 / 미결 결정 / 최소 재현 맵 / 검증 VT / 패키지 결과`를 기록한다. 위 DG 기능별 제안의 실행 결과는 이번 도구 작업에서 검증하지 않았다. Workbench·에디터 플러그인 검사는 DG 수용 검증과 별도로 [검증 계획](../verification/VERIFICATION_PLAN.md)에 연결한다.

GAS·Iris·Replication Graph·Mass·World Partition·외부 voxel 플러그인은 이번 문서로 도입되지 않는다. 기존 기반으로 충족하지 못하는 구체 요구와 측정 근거가 생기면 비교한다. 특히 GAS의 설치 사실만으로 굴착·인벤토리·경제·사망 정책이 제공되는 것은 아니다.

## 5. 공식 근거 색인

확인일 2026-10-25. 다음 링크는 Epic 공식 문서이며, 설계 제안은 해당 기능을 DIGETHER 요구에 적용한 판단이다.

- E01 [CharacterMovement 네트워크 이동](https://dev.epicgames.com/documentation/en-us/unreal-engine/understanding-networked-movement-in-the-character-movement-component-for-unreal-engine)
- E02 [Enhanced Input](https://dev.epicgames.com/documentation/unreal-engine/enhanced-input-in-unreal-engine)
- E03 [Gameplay Framework](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-framework-in-unreal-engine), [GameMode / GameState](https://dev.epicgames.com/documentation/en-us/unreal-engine/game-mode-and-game-state-in-unreal-engine)
- E04 [프로퍼티·조건부 복제](https://dev.epicgames.com/documentation/en-us/unreal-engine/replicate-actor-properties-in-unreal-engine), [RPC](https://dev.epicgames.com/documentation/en-us/unreal-engine/remote-procedure-calls-in-unreal-engine)
- E05 [Online Subsystem](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-in-unreal-engine), [Online Services — Beta](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-services-in-unreal-engine), [Sessions](https://dev.epicgames.com/documentation/unreal-engine/sessions-interface-in-unreal-engine)
- E06 [Geometry Scripting 소개](https://dev.epicgames.com/documentation/unreal-engine/introduction-to-geometry-scripting-in-unreal-engine), [사용 가이드](https://dev.epicgames.com/documentation/unreal-engine/geometry-scripting-users-guide-in-unreal-engine)
- E07 [Chaos Destruction](https://dev.epicgames.com/documentation/unreal-engine/destruction-overview)
- E08 [PCG 개요](https://dev.epicgames.com/documentation/unreal-engine/procedural-content-generation-overview), [PCG 생성 모드](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-generation-modes-in-unreal-engine)
- E09 [Networked Physics](https://dev.epicgames.com/documentation/unreal-engine/networked-physics-overview), [NetworkPhysicsComponent API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UNetworkPhysicsComponent)
- E10 [Data Assets](https://dev.epicgames.com/documentation/en-us/unreal-engine/data-assets-in-unreal-engine)
- E11 [Voice Chat Interface](https://dev.epicgames.com/documentation/unreal-engine/voice-chat-interface-in-unreal-engine), [EOS Voice Chat](https://dev.epicgames.com/documentation/unreal-engine/voice-chat-with-epic-online-services), [UVOIPTalker API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UVOIPTalker). 근거리 음성의 규칙 집행 위치는 기능 존재와 별도로 결정한다.
- E12 [AI Perception](https://dev.epicgames.com/documentation/unreal-engine/ai-perception-in-unreal-engine), [Navigation Mesh 설정](https://dev.epicgames.com/documentation/unreal-engine/navigation-mesh-settings-in-the-unreal-engine-project-settings)
- E13 [Post Process Materials](https://dev.epicgames.com/documentation/unreal-engine/post-process-materials-in-unreal-engine). UMG 웹 페이지는 도구 열람 실패; `Engine/Source/Runtime/UMG/Public/Blueprint/UserWidget.h`로 기반 클래스 존재만 확인했다.
- E14 [Saving and Loading](https://dev.epicgames.com/documentation/en-us/unreal-engine/saving-and-loading-your-game-in-unreal-engine)
- E15 [Unreal Insights](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-insights-in-unreal-engine), [Run Automation Tests](https://dev.epicgames.com/documentation/en-us/unreal-engine/run-automation-tests-in-unreal-engine). 테스트 종류·실행 명령 형식·0개 실행 판정은 [Unreal 테스트 기준](../unreal/TESTING.md)을 따른다.
- E16 [Multiplayer Travel](https://dev.epicgames.com/documentation/en-us/unreal-engine/travelling-in-multiplayer-in-unreal-engine), [Level Streaming 개요](https://dev.epicgames.com/documentation/en-us/unreal-engine/level-streaming-overview-in-unreal-engine). 후자 본문은 도구에서 추출되지 않아 구체 적용은 엔진/Editor 확인 대상이다. 로컬 GameModeBase.h의 seamless 설정/목록 API, PlayerState.h의 CopyProperties/OverrideWith/SeamlessTravelTo, GameMode.h의 inactive player 기능 존재를 확인했다. 런 간 장부 수명은 전체 시스템 설계의 세션·게임·런·월드 인계 절을 따른다.
로컬 조사 재현: 설치 루트 아래 `Engine/Build/Build.version`, `Engine/Plugins/{EnhancedInput,PCG,Runtime/GeometryScripting,Runtime/GameplayAbilities,Online/OnlineServices,Online/VoiceChat}`의 descriptor와 Source를 확인한다. API는 `EnhancedInputSubsystemInterface.h`, `EnhancedInputComponent.h`, `GeometryScript/MeshBooleanFunctions.h`, `Components/DynamicMeshComponent.h`, `PhysicsEngine/PhysicsHandleComponent.h`에서 확인했다. 2026-10-26에 `GameFramework/Actor.h`에서 `SetNetDormancy`·`FlushNetDormancy`·`SetNetCullDistanceSquared`·`SetNetUpdateFrequency`·`IsNetRelevantFor`·`bOnlyRelevantToOwner`·`bNetUseOwnerRelevancy` 선언을 추가로 확인했다(`NetCullDistanceSquared` 직접 접근은 5.5부터 deprecated). 개인 설치 절대 경로를 팀 공통 설정으로 채택하지 않는다.
