# Unreal 에디터 개발 편의 설계

기준: UE 5.8.2 · 2026-10-28 · 사용자가 에디터 편의 기능의 설계·구현을 요청한 범위다. 게임 규칙·온라인 제공자·영구 저장 정책을 새로 결정하지 않는다. 실제 적용 상태와 사용법은 [개발 도구 안내](../tools/DEVELOPER_TOOLS.md), 기능별 원본은 [개발반 시스템 설계](DEVELOPMENT_SYSTEM_DESIGN.md)를 따른다.

이 문서의 설치 범위 이후, 사용자의 추가 요청으로 전체 Development 패키지·로컬 템플릿 다인·시험 액터의 스냅샷 디스크 복원을 실행했다. 그 [후속 실기 검증](../tools/DEVELOPER_TOOLS.md#후속-실기-검증)은 아래 설치 완료 판정과 별도이며 게임 규칙·저장 정책을 확정하지 않는다.

## 1. 해결할 불편과 기존 기능




Unity 도구 이름은 필요한 작업의 예시다. 동일 제품을 Unreal에 설치한다고 가정하지 않는다.

| 필요한 작업 | UE 5.8.2에서 사용할 기반 | 이번 범위 |
|---|---|---|
| Odin처럼 속성 정리·조건 표시·버튼·검증 | UPROPERTY의 Category/EditCondition/GetOptions, CallInEditor, Details, Data Validation; 기존 T07 Inspector·T05 검증 | 기존 워크벤치 목록과 사용 가이드에서 연결한다. 새 허브 안에 편집기를 복제하지 않는다 |
| Editor Enhancer처럼 반복 메뉴 탐색 줄이기 | 기존 Workbench 도크와 엔진 탭·설정 화면 | 프로젝트 어댑터에 개발 도구 허브를 추가한다 |
| Auto Save처럼 작업 손실 줄이기 | UEditorLoadingSavingSettings와 엔진 자동 저장·복구 | 유효한 개인 자동 저장 설정을 보여주고 원본 설정 화면으로 연결한다. 별도 저장 타이머나 강제 저장을 만들지 않는다 |
| Maintainer처럼 데이터·참조 문제 찾기 | Data Validation, Asset Search, Reference Viewer, Size Map, 기존 T09/T10 | 검사·검색 진입과 한계를 안내한다. 미사용 추정으로 에셋을 자동 삭제하거나 redirector를 일괄 수정하지 않는다 |
| 배치 실험과 복원 | Actor Palette, Level Snapshots, 기존 T11 Undo 배치 | Epic 플러그인을 Editor 용도로 활성화하고 시험 맵 검수 절차를 제공한다 |
| 애니메이션·아트 디버깅 | Animation Insights/Rewind Debugger, UV Editor | 기존 간접 활성 기능을 프로젝트에 명시하고 허브/사용법에서 발견하기 쉽게 한다 |

설치 엔진 descriptor와 공식 문서에서 기능을 확인했다. Asset Search·Actor Palette·Level Snapshots·UV Editor는 Beta다. 새 도구의 존재·모듈 로드와 실제 편집 결과 검증을 구분한다. 원본 에셋을 바꾸는 시험은 이번 설치 작업에서 수행하지 않는다.

## 2. 개발 도구 허브

`DigetherWorkbench`의 Editor 모듈에 `Digether.DeveloperTools` 설명자를 등록한다. 기존 `Digether.ConnectionStatus`는 게임 provider 연결 상태이며 별도로 유지한다. 범용 `DevelopmentWorkbench`가 DIGETHER를 의존하게 만들지 않는다.

허브는 다음 두 부분으로 구성한다.

- **현재 환경:** 실제 엔진 버전, 프로젝트 플러그인의 발견·활성 상태, 사용자 자동 저장 켜짐/맵/콘텐츠/간격, GameFeatureData AssetManager 규칙 존재를 읽는다. 설정값 존재를 기능 검증 통과로 표시하지 않는다.
- **바로 열기:** Asset Search, Actor Palette 1, Level Snapshots, Rewind Debugger, 자동 저장 설정, 프로젝트 Asset Manager 설정, Output Log. Data Validation과 에셋별 Reference Viewer/Size Map은 정확한 엔진 메뉴 안내를 제공한다. 전체 프로젝트 검사를 창 열기만으로 실행하지 않는다.

원본 API/탭 ID는 설치된 엔진 소스로 확인한다. 지원 플러그인이 없거나 비활성, 탭 등록이 없는 경우 버튼을 비활성화하거나 실패 이유를 패널에 표시한다. `ISettingsModule::ShowViewer`는 반환값이 없으므로 호출 전에 container/category/section 존재를 확인한다. 임의 문자열 명령·쉘·브라우저 실행은 추가하지 않는다. 자동 저장 설정의 변경은 엔진 설정 화면에서 사용자가 한다. 허브 생성·새로고침은 설정·패키지를 저장하지 않는다.

매 Tick마다 에셋·플러그인을 전수 검색하지 않는다. 생성 시와 사용자가 누르는 새로고침에서 상태를 읽고, 작업 직전 탭/설정 접근 가능 여부는 재확인한다. 새로고침 없이 다른 창에서 바뀐 값은 이전 조회 시점의 값임을 표시한다.

## 3. 수명과 의존성

패널은 Editor 전용 Slate 위젯이다. 원본 UObject를 소유·복제하지 않는다. 조회한 값은 패널 표시용 스냅샷이며 게임 서비스·세이브가 아니다. 활성 플러그인 조회는 IPluginManager, 자동 저장은 UEditorLoadingSavingSettings, AssetManager는 실제 설정/규칙을 사용한다. 설정 화면은 ISettingsModule의 공개 API, 일반 탭은 FGlobalTabmanager, Rewind Debugger는 LevelEditor의 탭 매니저를 사용하되 실패 반환을 처리한다.

등록 성공 여부를 도구별로 저장하고, 종료 시 자신이 등록한 ID만 해제한다. 기존 연결 상태 등록에 실패해도 새 도구의 등록/해제를 잘못 생략하지 않는다. 패널에 장기 타이머·전역 델리게이트·백그라운드 작업을 추가하지 않는다. 런타임 `DIGETHER` 모듈은 이 도구를 의존하지 않는다.

새로 명시하는 Epic 편의 플러그인은 `.uproject`의 `TargetAllowList=[Editor]`로 제한한다. 에디터 실행 파일의 `-game`은 Editor 타깃이라는 점과, 이 선언이 기존 모든 플러그인의 패키징 안전성을 증명하지 않는다는 점을 유지한다. 전체 패키징 검증은 별도다.

| 플러그인 선언 | 변경 전 상태 | 이번 허용 타깃 |
|---|---|---|
| AssetSearch, ActorPalette, LevelSnapshots | 엔진 기본 비활성·기존 실행 미로드 | Editor |
| DataValidation, EditorScriptingUtilities, AssetManagerEditor, BlueprintHeaderView, GameplayInsights, UVEditor | 엔진 기본값 또는 기존 플러그인 의존으로 로드 | Editor |

간접 활성 도구도 변경 전/후 Game receipt의 플러그인·빌드 산출물을 비교하며, 9개 전부를 Editor 로드 검사에 포함한다. Editor 도구와 이번 범위에 없는 게임 실행 중 추적 모듈의 제외는 의도한 범위다. 실제 게임 기능이 사용하는 런타임 의존이 사라지거나 의존 충돌이 발생하면 해당 제한을 철회하고 원인을 확인한다. Game 빌드 성공을 Cook·패키지 성공으로 확대하지 않는다.

허브의 바로가기 대상 플러그인 모듈은 Build.cs에 직접 링크하지 않는다. 공통 Projects/Slate/Settings 등으로 발견·탭·설정 경로를 조회하여 누락 상태도 표시할 수 있어야 한다. 엔진 조회와 상태 판정은 분리하고 누락·비활성·미등록·자동 저장 꺼짐 입력을 합성 값으로 검사한다.

공유 `.uproject`의 활성화는 해당 버전을 사용하는 팀원의 Editor에도 적용된다. Asset Search 색인과 추적 녹화 비용은 허브의 비용과 별개이며 필요할 때 사용한다. 개인의 검색 설정·녹화는 별도지만 공유 플러그인 비활성화는 의존성과 팀 변경을 검토한다. Level Snapshot 산출물의 팀 저장 위치·커밋 정책은 미정이며 이번 작업에서는 생성하지 않는다. 임시 시험·개인 스냅샷을 승인 없이 공유 콘텐츠로 커밋하지 않는다.

## 4. 기존 개발 루틴의 원인 수정

- GameFeaturesToolset의 기존 의존성이 요구하는 `GameFeatureData` 등록을 엔진 `AddDefaultGameDataRule`과 같은 규칙으로 DefaultGame.ini에 추가한다. 이 설정은 Runtime/Cook에도 적용되고 `AlwaysCook` 규칙을 포함한다. Game 타깃의 GameFeatures 의존과 AssetManager 초기화 로그를 별도로 확인한다. 실행하지 못하면 해당 범위는 미검증으로 남긴다. 새 게임 모듈화 정책을 채택하는 변경은 아니다.
- 자동검사·DataValidation은 개인 `EditorPerProjectUserSettings.ini`의 복사본을 실행별 결과 폴더에 만들고 `-EditorPerProjectUserSettingsIni=`로 읽기·쓰기 경로를 격리한다. 그 프로세스에서만 MCP 자동 서버 시작을 명령줄 ini override로 끈다. override 단독 사용은 설정 재저장 때 원래 키가 누락될 수 있어 허용하지 않는다. 해당 원본 파일의 SHA256 전후 일치로 보존을 검증하며, 다른 모든 Editor 설정 파일의 불변까지 보장하지 않는다.
- 기본 Unity 빌드에서 T01/T08 cpp의 익명 namespace 색상 상수가 충돌한다. 각 파일 고유 이름/namespace로 구분해 원인 지점을 고치고 기본 빌드로 재검증한다. non-unity 강제로 오류를 숨기지 않는다.
- 빌드 실행기는 종료 코드 외에 이번 UBT 성공·완료 출력을 요구한다. 자동검사는 소스 기대 이름과 실제 보고서 이름·상태·개수를 대조한다. 데이터 검사는 대상 0개·무효·미검사를 통과시키지 않는다. 모든 실행은 별도 결과 폴더를 쓴다.
- 문서 링크 검사는 코드 예시의 가짜 HTML id를 앵커로 인정하지 않으며, 중첩 라벨·이스케이프된 백틱의 실제 링크를 놓치지 않도록 회귀 사례를 추가한다. 외부 HTTP와 지원하지 않는 문법의 검증 여부를 명시한다.

## 5. 구현 파일 경계

| 영역 | 변경 범위 |
|---|---|
| 환경 설정 | `DIGETHER/DIGETHER.uproject`, `DIGETHER/Config/DefaultGame.ini` |
| 개발 도구 허브 | `Plugins/DigetherWorkbench/Source/DigetherWorkbenchEditor`의 모듈 등록·Build.cs·새 패널/작은 상태 모델·관련 테스트 |
| Unity 빌드 원인 수정 | `DevelopmentWorkbench/Source/WorkbenchEditor/Private/Core/SWorkbenchItemEditorPanel.cpp`와 필요 시 `SWorkbenchCsvPanel.cpp`의 내부 이름만 |
| 로컬 검사 | `scripts/Invoke-UnrealChecks.ps1`, `scripts/check-doc-links.mjs`, 각각의 기존 회귀 검사 |
| 팀 안내 | 온보딩·개발 도구·프로필·검증 계획·설계의 실제 상태 및 연결 |

기존 미커밋 설계·설정 변경과 바이너리 에셋을 보존한다. 이 작업의 사용자 지정 모델·교차 검증 과정은 로컬 작업 기록에 남기며 팀 공통 모델 정책으로 고정하지 않는다.

## 6. 완료 판정

1. L1 문서: 공개 문서의 상대 파일·앵커 검사, 설계의 미결 정책과 현재 모듈 정합성 확인.
2. L1 코드: UE 5.8.2 Editor 기본 Unity 빌드와 Game 빌드. 수정 파일이 Adaptive Unity에서 제외되어 충돌이 숨지 않도록 설치 UBT가 지원하는 옵션을 확인해 Adaptive Unity를 끈 빌드도 수행한다. 새 Editor 의존의 Game 유입 여부를 확인한다.
3. L2: 기존 Workbench 검사 이름 전부와 신규 허브의 의미 있는 상태·실패/등록 수명 검사. 누락된 플러그인·미등록 탭·자동 저장 꺼짐을 정상 상태로 오인하지 않는지 확인한다.
4. 실행기 회귀: PS5.1/7, 유효 결과·0개·보고서 누락·실패·no-op build 출력·문서 parser 경계. 실제 엔진으로 Automation/DataValidation을 실행한다.
5. Editor 통합: 허브 표시, 상태 조회, 명시한 9개 플러그인 로드, 가능한 바로가기 진입. 기존 T05/T07의 목록·사용 가이드 연결을 확인한다. 패키지 자동 저장·수정이 없음을 확인한다. 이 작업에서는 Level Snapshot 에셋을 생성·저장하지 않는다. 실제 UI 조작이 불가능하면 모듈 로드/자동검사와 구분해 미검증으로 남긴다. 열린 패널의 종료·등록 해제와 Live Coding 재등록은 실행한 범위만 보고한다.
6. 실제 게임 DG 기능, 멀티플레이 M2/M3, 전체 Cook/패키지·다른 PC, Level Snapshot 복원과 Actor Palette 복잡한 BP 복사는 이번 도구 설치 검증으로 통과 처리하지 않는다.

## 공식 기능 근거

- [Data Validation](https://dev.epicgames.com/documentation/unreal-engine/data-validation-in-unreal-engine?lang=en-US)
- [Editor Scripting](https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-and-automating-the-unreal-editor)
- [Asset Search](https://dev.epicgames.com/documentation/unreal-engine/API/PluginIndex/AssetSearch), [Actor Palette](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/ActorPalette)
- [Level Snapshots](https://dev.epicgames.com/documentation/unreal-engine/level-snapshots-in-unreal-engine): 액터/속성 복원과 에셋 원본의 소스 관리 역할을 구분한다.
- [Rewind Debugger](https://dev.epicgames.com/documentation/unreal-engine/animation-rewind-debugger-in-unreal-engine?application_version=5.8), [UV Editor](https://dev.epicgames.com/documentation/en-us/unreal-engine/uv-editor-in-unreal-engine)

로컬 근거: 각 플러그인의 `.uplugin`·메뉴 등록 소스, `EditorLoadingSavingSettings.h`, `GameFeaturesEditorModule.cpp`. 웹 5.8 문서와 설치 패치 5.8.2의 확인 범위를 구분한다.
