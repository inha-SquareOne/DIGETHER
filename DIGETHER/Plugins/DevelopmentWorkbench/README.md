# DevelopmentWorkbench

UE **5.8.2**용 Editor 플러그인이다. `WorkbenchEditor`는 게임 Runtime과 PCG에 의존하지 않는다. 게임별 연결은 별도 `DigetherWorkbench` 플러그인에서 담당한다.

11종 도구를 구현했다. 2026-10-28 Editor 기본/강제 Unity·Game 빌드와 설정 격리 후 최종 자동검사 64개 성공을 확인했다. 이 중 경고가 있는 테스트는 2개다. DIGETHER 개발 도구 허브 표시·상태 조회·새로고침·바로가기 7종의 실제 UI도 확인했다. 2026-10-27 Core 독립 빌드·자동검사 60개와 멀티 실행의 4인 재검증 등은 아래 과거 기록으로 보존한다. 합성 예제의 가격·중량·확률·룸 규격은 게임의 확정 수치가 아니다.

DIGETHER 전용 **개발 도구** 허브와 Epic 편의 플러그인·자동 저장·검사 실행기 사용법은 [개발 도구 안내](../../../docs/tools/DEVELOPER_TOOLS.md), 책임과 검증 기준은 [에디터 개발 편의 설계](../../../docs/design/EDITOR_PRODUCTIVITY_DESIGN.md)를 따른다. 프로젝트 어댑터는 Core의 11종 도구와 별도로 등록한다.

## 도구 범위

| 도구 | 역할 |
|---|---|
| T01 아이템 | 합성 DataTable 조회·검색·편집 초안 |
| T02 스폰 | 단일 풀 가중 복원 추출의 정확 확률·기대값·난수 비교 |
| T03 분석 | 합성 잠재가치 에피소드·A/B 짝 비교·원시 결과 내보내기 |
| T04 룸 | 입력 manifest의 문 규격·연결·도달성 검사 |
| T05 검증 | 합성 아이템의 공통 규칙과 Data Validation 연결 |
| T06 멀티 실행 | 별도 프로세스 실행·로그·중지, 준비 신호 미연결 시 수동 관찰 |
| T07 Inspector | 선택한 원본의 직접 Details 편집과 명시 프로필 |
| T08 CSV | 기준 스냅샷을 가진 스칼라 변경안 가져오기·검증 |
| T09 에셋 | 이름 변경 계획·충돌 및 참조 후보 확인 |
| T10 검색 | 엔진 Find in Blueprints·Reference Viewer 진입 |
| T11 배치 | 선택 액터의 재현 가능한 배치 변경안 |

T01/T08 초안은 원본에 즉시 쓰지 않는다. 적용과 저장은 별도 단계다. T07은 원본 직접 편집이며 같은 초안 계약을 제공하지 않는다. 기본 Blueprint `GetOptions`와 인스턴스 `CallInEditor`는 엔진 기능을 사용한다.

## 시작 순서

1. Editor의 **도구 → 개발 워크벤치 열기**에서 도구 목록을 연다.
2. T01에서 **메모리 합성 테이블**을 선택하면 디스크를 쓰지 않고 편집·검증 흐름을 시험할 수 있다. **합성 에셋 만들기/열기**는 시험 경로에 저장 가능한 에셋을 만든다.
3. 행을 선택하고 초안을 수정한다. 검증 결과와 변경 전후를 확인한 뒤 **적용 (메모리, Undo 가능)**을 누른다.
4. 저장 가능한 에셋은 **저장 (패키지)**으로 디스크에 기록한다. 원본이 다른 편집기에서 바뀌었으면 적용·저장이 거부될 수 있으므로 현재 변경을 확인하고 재조회한다.
5. T08은 도구가 내보낸 기준 정보를 포함한 CSV를 수정한 후 미리보기·적용·저장한다. 누락 열과 빈 셀은 보존이며, 명시 빈 문자열은 `<BLANK>`, Name None은 `<NULL>`이다.

기본 합성 테이블은 검증 UI를 시험하기 위한 의도적 오류 행을 포함한다. 이 특정 예제 에셋은 Workbench 사용자 검증기에서 제외된다. DataValidation commandlet의 타입별 대상에는 포함될 수 있으므로 commandlet 대상 수와 사용자 검증기 대상 수를 구분한다. 다른 합성 행 구조 테이블에는 공통 검증기가 적용된다.

## 프로젝트 연결

- 실게임 데이터·시뮬레이션 계산·생성기·멀티 준비 신호는 아직 연결되지 않았다.
- 룸 그래프 검사는 실제 충돌·Navigation 통행 검증과 다르다.
- 멀티 실행 프로세스가 살아 있다는 사실은 접속 성공이나 M2 통과가 아니다.
- 에셋 참조가 없다는 결과는 동적 로드까지 포함한 삭제 안전 판정이 아니다.
- `/Game/Editor/DevelopmentWorkbench/`는 시험 에셋 경로다. DIGETHER는 이 경로를 NeverCook으로 지정한다. 다른 프로젝트로 플러그인을 옮길 때도 시험 에셋과 실제 게임 데이터를 분리한다.

## 확장

`Public/WorkbenchModule.h`의 `FWorkbenchToolDescriptor`와 `IWorkbenchModule::RegisterTool`로 C++ 패널을 등록한다. 등록 성공을 소유한 모듈만 종료 시 `UnregisterTool`을 호출한다. `Public/Analytics`와 `Public/Tools`의 값 기반 알고리즘은 테스트와 패널이 공유한다.

## 빌드

저장소 루트에서 실행한다. 자신의 UE 5.8.2 설치 루트를 `UE_ENGINE_ROOT` 또는 `-EngineRoot`로 지정한다. 실행기 요구 사항·로그·결과 판정은 [개발 도구 안내](../../../docs/tools/DEVELOPER_TOOLS.md#로컬-검사-실행)를 따른다.

```powershell
.\scripts\Invoke-UnrealChecks.ps1 BuildEditor
.\scripts\Invoke-UnrealChecks.ps1 BuildGame
```

새 모듈을 처음 추가한 뒤에는 Editor를 닫고 빌드한 후 다시 연다. 빌드 성공만으로 저장·재로드·멀티플레이·cook 검증을 대신하지 않는다.

## 현재 재검증 상태 (2026-10-28)

T01/T08 내부 색상 상수 충돌을 수정한 뒤 Editor 기본 Unity 빌드와 `-DisableAdaptiveUnity` 강제 Unity 빌드, Game 빌드가 성공했다. 에디터를 종료하고 사용자 설정 복사본을 지정한 최종 Automation은 기대·실행·성공 각 64개, 실패 0개이며 2개 테스트에 경고가 있었다. 임시 패키지의 Asset Search 해시 경고 5개와 중복 등록 거부 시험 경고 4개다. 최종 DataValidation은 대상 3개 valid·경고/invalid/검사 불가 0개이며 Workbench 사용자 검증기는 2개를 검사했다. 두 실행 전후 원본 사용자 설정 SHA-256이 같다는 독립 비교와 실제 허브 UI 결과는 [개발 도구 안내](../../../docs/tools/DEVELOPER_TOOLS.md#확인-결과와-남은-검증)를 따른다. Live Coding 재등록·실제 저장/복원·멀티플레이·전체 Cook/패키징은 별도 검증이며 과거 실패 기록은 그대로 보존한다.

## 확인한 범위와 한계 (2026-10-27)

- Editor Win64 Development non-unity 최종 빌드 성공, 자동검사 60개 성공·실패/경고/미실행 0개. PCG를 비활성화한 별도 HostProject에서도 Core를 NoPCH/NoSharedPCH/DisableUnity로 빌드했다.
- 11개 패널 진입, 합성 DataTable 저장, 확률·분석·룸 오류, CSV 내보내기·잘못된 입력 거부, Blueprint 조건 그룹·버튼·컴파일 후 해제, 에셋 이름 변경·재로드, 검색/참조 창, 배치 적용·Undo·재로드를 확인했다. 파일 저장·충돌·CSV 상세 경계는 자동검사도 포함한다.
- 리슨 1·2·4인 실행, 4인 접속 로그, 클라이언트 비정상 종료 시 전체 회수, 탭 종료 시 회수와 격리 PIE 2인을 확인했다. 전용 서버 실동작, 포트 점유·자동 timeout의 실제 UI 재현, 서로 다른 PC의 M2/M3는 미검증이다.
- DataValidation은 별도 테이블 1개에서 정상/오류 경로를 확인했다. Cook 산출물의 AssetRegistry 594개 패키지에서 시험 경로 제외를 확인했다. 기존 GameFeatureData 설정 오류 및 동시 실행한 MCP 포트 충돌로 두 commandlet 전체는 exit 1이므로 전체 Cook·패키징 성공으로 보지 않는다.
- T03 export·취소 버튼의 실제 UI 경로와 모든 플랫폼/엔진 조합을 포괄 검증하지 않았다. 합성 데이터의 결과를 실제 게임 수익이나 이동 가능 판정으로 사용하지 않는다.
