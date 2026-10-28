# DIGETHER 개발 도구 안내

기준일: 2026-10-28 · 상태: **편의 플러그인·허브 UI·전체 Development 패키징·로컬 템플릿 다인·스냅샷 디스크 복원 확인**. UE 5.8.2를 사용한다. 구조와 완료 기준은 [에디터 개발 편의 설계](../design/EDITOR_PRODUCTIVITY_DESIGN.md), 게임 기능의 구현 경계는 [Unreal 제공 기능](../design/UNREAL_FEATURE_COVERAGE.md)을 따른다. 실행 환경과 한계는 [후속 실기 검증](#후속-실기-검증)을 확인한다.

## 사용할 도구 고르기

| 하고 싶은 일 | 먼저 사용할 기능 | 사용 범위와 한계 |
|---|---|---|
| 속성 정리·조건부 표시·선택지·버튼 | 엔진 Details의 UPROPERTY 메타데이터·CallInEditor, Workbench T07 Inspector | Odin에서 하던 작업의 엔진 대응 기능이다. T07은 선택 원본 직접 편집이므로 T01/T08 초안과 구분한다 |
| 반복 메뉴를 한곳에서 열기 | **DIGETHER 개발 도구** (`Digether.DeveloperTools`) | 환경·플러그인·자동 저장 상태·새로고침·바로가기 7종의 실제 UI 확인 |
| 작업 손실 대비 | 엔진 Auto Save·복구와 Git/LFS | 허브는 개인 자동 저장 설정을 읽고 원본 설정 화면을 연다. 별도 저장 타이머·강제 저장은 없다 |
| 데이터 오류 찾기 | Data Validation, Workbench T05 | 적용 가능한 검증기와 대상 에셋 범위를 확인한다. 창을 열었다는 사실은 검사 통과가 아니다 |
| 에셋·참조·용량 조사 | Asset Search, Reference Viewer, Size Map, Workbench T09/T10 | Maintainer류의 조사 작업에 해당한다. 참조 검색만으로 동적 로드까지 포함한 삭제 안전성을 보장하지 않는다 |
| 다른 레벨의 배치 참고 | Actor Palette | 시험 맵에서 복사를 확인한다. 복잡한 Blueprint와 모든 의존 관계의 완전 복제를 보장하지 않는다 |
| 레벨 액터 상태 비교·복원 | Level Snapshots | 선택 액터/속성의 상태 기록이다. 원본 Material·Landscape 등 에셋 전체 백업이나 Git/LFS를 대체하지 않는다 |
| 애니메이션·UV 조사 | Gameplay Insights의 Rewind Debugger, UV Editor | 필요한 기록·대상 에셋으로 별도 확인한다. 플러그인 활성화는 실제 디버깅·편집 성공과 다르다 |

기존 11종 Workbench 도구의 초안·적용·저장·Undo 계약은 [DevelopmentWorkbench 안내](../../DIGETHER/Plugins/DevelopmentWorkbench/README.md)를 따른다. 합성 예제의 수치를 실제 게임 규칙으로 사용하지 않는다.

## 편의 플러그인 적용 범위

이번에 신규 활성화한 항목은 Asset Search·Actor Palette·Level Snapshots다. 기존에 엔진 의존성으로 로드되던 편의 항목도 프로젝트 descriptor에 명시했다. 다음 9개에 `.uproject`의 `TargetAllowList=[Editor]`를 적용했고, Automation 로그의 mount와 실제 허브 화면에서 9개 모두 활성 상태를 확인했다. Editor/Game 빌드와 허브 새로고침·바로가기 7종의 실제 UI 조작도 확인했다.

| 플러그인 ID | 역할 | 적용 구분·주의 |
|---|---|---|
| `AssetSearch` | 에셋 검색 | 신규 활성화 · Beta |
| `ActorPalette` | 레벨 액터 배치 참고·복사 | 신규 활성화 · Beta |
| `LevelSnapshots` | 액터·속성 스냅샷 | 신규 활성화 · Beta |
| `DataValidation` | 에셋 데이터 검증 | 기존 로드 항목 명시 |
| `EditorScriptingUtilities` | 에디터 작업 자동화 기반 | 기존 로드 항목 명시 · Beta |
| `AssetManagerEditor` | 에셋 관리·참조·용량 도구 | 기존 로드 항목 명시 |
| `BlueprintHeaderView` | Blueprint의 C++ 형태 보기 | 기존 로드 항목 명시 |
| `GameplayInsights` | 애니메이션 분석·Rewind Debugger 기반 | 기존 로드 항목 명시 |
| `UVEditor` | UV 편집 | 기존 로드 항목 명시 · Beta |

이미 있는 Modeling Tools·StateTree 계열·MCP·CommonUI·EOSVoiceChat과 프로젝트의 Workbench 플러그인은 별도다. 편의 플러그인 적용으로 온라인·음성 제공자를 확정하지 않는다. Editor 타깃 제한은 실제 Game 빌드·Cook·패키징 검사를 대체하지 않는다. `UnrealEditor.exe -game`도 실행 파일은 Editor 타깃이다.

플러그인을 처음 추가한 뒤에는 Editor를 닫고 **BuildEditor**를 수행한 다음 프로젝트를 다시 연다. `Edit > Plugins`에서 활성 상태를 확인하고, 도구 목록에 없으면 모듈 로드 오류와 Output Log를 확인한다. 설치 엔진 descriptor·메뉴 소스와 공식 근거의 확인 범위는 [설계의 공식 기능 근거](../design/EDITOR_PRODUCTIVITY_DESIGN.md#공식-기능-근거)에 있다.

## 개발 도구 허브 사용 순서

1. **도구 → 개발 워크벤치 열기**에서 **DIGETHER 개발 도구**를 연다. 기존 **DIGETHER 연결 상태**는 게임 provider 연결용 별도 패널이다.
2. 환경에서 실제 엔진 버전·플러그인 발견/활성·개인 자동 저장·GameFeatureData Asset Manager 규칙을 확인한다. 표시값은 마지막 조회 시점의 값이다. 다른 창에서 바꿨으면 새로고침한다.
3. Asset Search·Actor Palette 1·Level Snapshots·Rewind Debugger·Output Log 또는 자동 저장/Asset Manager 설정 바로가기를 선택한다. 등록되지 않은 탭이나 사용할 수 없는 플러그인의 실패 이유를 확인한다.
4. Data Validation과 에셋별 Reference Viewer/Size Map은 패널의 엔진 메뉴 안내를 따른다. 검사 범위·로그를 확인한 뒤 결과를 판정한다.

허브 열기·새로고침은 설정이나 패키지를 저장하지 않는다. 자동 저장 설정은 엔진 설정 화면에서 사용자가 변경한다. 자동 수정·강제 저장·미사용 에셋 삭제·redirector 일괄 수정은 제공하지 않는다. Actor Palette 복사와 Level Snapshot 복원은 편집 예약이 없는 시험 맵에서 별도로 검증한다.

## 로컬 검사 실행

저장소 루트에서 [Invoke-UnrealChecks.ps1](../../scripts/Invoke-UnrealChecks.ps1)을 실행한다. Windows PowerShell 5.1 이상이 필요하다. 엔진 루트는 자신의 UE 5.8.2 설치 위치를 `-EngineRoot` 또는 `UE_ENGINE_ROOT`로 지정한다. 자동으로 다른 엔진을 고르지 않는다.

다음 명령은 `UE_ENGINE_ROOT`를 지정한 후 실행한다. 각 명령에 `-EngineRoot '자신의 엔진 설치 경로'`를 붙여도 된다.

```powershell
.\scripts\Invoke-UnrealChecks.ps1 BuildEditor -DryRun
.\scripts\Invoke-UnrealChecks.ps1 BuildEditor
.\scripts\Invoke-UnrealChecks.ps1 BuildGame
.\scripts\Invoke-UnrealChecks.ps1 Automation
.\scripts\Invoke-UnrealChecks.ps1 ValidateData
```

| 모드 | 실행·판정 범위 |
|---|---|
| `BuildEditor` | `DIGETHEREditor Win64 Development` 기본 Unity 빌드 |
| `BuildGame` | `DIGETHER Win64 Development` 빌드. Cook·패키징은 별도 |
| `Automation` | 소스의 `Workbench.` 테스트 이름과 보고서의 이름·상태·개수 대조. C++ 변경 후에는 먼저 BuildEditor 실행 |
| `ValidateData` | 기본 `/Script/Engine.DataTable` 대상 Data Validation. 전체 프로젝트 검사가 아니다. 다른 대상은 `-AssetType`으로 지정 |

`-DryRun`은 명령을 출력하며 엔진 실행·검증·결과 파일 생성을 하지 않는다. 실행마다 `DIGETHER/Saved/DeveloperChecks/` 아래 고유 폴더에 `execution.json`·표준 출력/오류·엔진 로그 또는 Automation 보고서를 남긴다. 빌드 로그는 UBT 성공·완료 출력까지 확인한다. 테스트 0개·소스와 다른 목록·결과 누락, 데이터 대상 0개·무효·미검사는 통과로 판정하지 않는다. 제한 시간은 `-TimeoutSeconds`로 지정한다.

Automation·ValidateData는 실행 폴더에 `EditorPerProjectUserSettings.ini` 복사본을 만들고 `-EditorPerProjectUserSettingsIni=실행폴더의복사본경로`로 지정한다. MCP 자동 시작 제한의 명령줄 ini override는 이 검사 프로세스에 전달한다. 이번 독립 검증에서는 실행 전후 원본 `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`의 SHA-256을 별도로 비교했다. 원본 해시 비교는 실행기의 자동 판정 기능에 포함되지 않으며, 모든 ini·에셋의 불변을 검증한 결과도 아니다. MCP 사용법과 기존 연결 증거는 [Unreal MCP 연결](UNREAL_MCP_SETUP.md)을 따른다.

문서 변경 후에는 Node.js로 로컬 링크·앵커를 검사한다. 외부 HTTP 페이지의 접근 가능성은 이 검사 범위에 포함되지 않는다.

```powershell
node .\scripts\check-doc-links.mjs
```

## 확인 결과와 남은 검증

- **2026-10-27 과거 결과:** Workbench 자동검사 60개, non-unity Editor 빌드 등의 확인 범위는 [플러그인 검증 기록](../../DIGETHER/Plugins/DevelopmentWorkbench/README.md#확인한-범위와-한계-2026-10-27)에 보존한다. 이번 변경의 통과 결과로 합산하지 않는다.
- **2026-10-28 원인 수정 재검증:** GameFeatureData Asset Manager 규칙 누락으로 DataValidation 실행이 exit 1·오류 2개였고, 규칙 추가 후 최신 실행은 exit 0이었다. DataTable 대상 3개·valid 3개·경고/invalid/검사 불가 0개이며, Workbench 사용자 검증기의 대상은 기본 합성 예제를 제외한 2개다. 전체 에셋 검증이나 Cook 성공을 뜻하지 않는다.
- **2026-10-28 빌드:** 기본 Unity Editor 빌드 성공(7.08초), `-DisableAdaptiveUnity`로 두 Workbench 모듈의 Unity 컴파일을 강제한 추가 빌드 성공(20.15초), Game 빌드 성공(26.93초)을 확인했다. Game receipt의 BuildPlugins는 272개에서 270개로 줄었고 BlueprintHeaderView·UVEditor만 제외됐다. RuntimeDependencies도 두 descriptor만 제거됐으며 추가 항목은 없고, Game BuildProducts에 WorkbenchEditor는 없다. 전체 Cook·패키징 검사는 별도다.
- **2026-10-28 Automation:** 에디터를 종료하고 설정 복사본을 지정한 최종 실행은 기대 64개·실행 64개·성공 64개·실패 0개다. 경고가 있는 테스트는 2개이며, Asset Search의 임시 `/WorkbenchTest` 패키지 해시 경고 5개와 등록 중복 거부 시험 경고 4개가 있었다. DataValidation 최종 실행도 대상 3개·valid 3개·경고/invalid/검사 불가 0개였다. 두 실행 전후 독립 비교에서 원본 사용자 설정의 SHA-256이 완전히 같았고 `bAutoStartServer=True`를 유지했다. 이 증거는 해당 원본 ini 파일의 검사 전후 상태에 한정한다.
- **2026-10-28 실행기·문서:** 사용자 설정 격리 보완 후 PowerShell 5.1·7의 실행기 회귀 검사 각 37개와 Node 문서 검사 회귀를 통과했다. 이전 각 30개 결과와 구분한다. DryRun이 폴더·파일을 만들지 않는 것도 실제 확인했다. 문서 L1은 86개 문서·로컬 링크 482개·앵커 15개·오류 0개였으며 외부 URL 75개는 검사에서 제외했다. `git diff --check`도 통과했다.
- **Editor 표시·상태:** WorkbenchEditor·DigetherWorkbenchEditor 로드와 허브 표시를 확인했다. 실제 화면에서 엔진 5.8.2, 편의 플러그인 9개 활성, 자동 저장·맵·콘텐츠 켜짐과 10분 간격, Primary Asset Type 규칙 3개 중 GameFeatureData 항목 존재가 조회값과 일치했다. 자동 저장 값은 검증 PC에서 관찰한 개인 설정이며 팀 기본값이 아니다.
- **Editor 실제 UI:** Search·Actor Palette·Level Snapshots·Rewind Debugger 레벨 에디터 탭·Editor Preferences의 General/LoadingSaving·Project Settings의 Game/AssetManager·Output Log까지 바로가기 7종을 확인했다. 새로고침 후 화면 조회 시각은 2:37:45에서 2:40:23으로 바뀌었다. All Saved 상태를 유지했고, 허브가 열린 상태에서 정상 종료의 `LogExit: Exiting`을 확인했다.
- **별도 검증:** Live Coding 재등록·엔진 Auto Save의 실제 복구·DG 게임 기능·게임 전체 M2/M3·다른 PC·Actor Palette의 복잡한 BP 복사는 미검증이다. 전체 패키지와 제한된 Level Snapshot 복원은 아래 후속 실행으로 확인했다. 검사 수준과 결과 양식은 [검증 계획](../verification/VERIFICATION_PLAN.md)을 따른다.

## 후속 실기 검증

2026-10-28 사용자의 추가 요청으로 수행했다. 이전 설치·허브 검사와 구분하며, 현재 게임 템플릿과 정해진 시험 에셋의 결과다.

| 대상 | 실제 결과 | 범위 |
|---|---|---|
| 전체 패키징 | 최종 Win64 Development BuildCookRun exit 0, Cook 773개, Pak/IoStore/Stage/Archive 완료 | CookAll, 기존 NeverCook 유지. Shipping·다른 플랫폼·깨끗한 다른 PC의 설치 검사는 아니다 |
| 패키지 맵 기동 | ThirdPerson·Combat·Platforming·SideScrolling 4개 모두 예상 맵 로드·플레이 시작·exit 0, Error/Fatal 0 | NullRHI, 각 150고정틱. 그래픽·수동 입력 검사는 아니다 |
| 로컬 멀티플레이 | 리슨 1→2→4인에서 전원 PlayerId·authority/ownership·위치 수렴. 4역할 각각 이동·정지. 한 클라이언트 정상 이탈 후 3인, 새 프로세스 재접속 후 4인 | 같은 PC·loopback·ThirdPerson 템플릿. 최종 9단계 통과, 소유 5프로세스 exit 0·강제 종료 없음. 계정 복구·장부·실제 게임 규칙의 M2/M3 통과는 아니다 |
| Level Snapshots | Epic 속성/액터 이름 복원 검사 2개 성공. 별도 에디터 2개 프로세스에서 캡처·변이·복원·저장 후 디스크 로드·재변이·재복원 성공 | StaticMeshActor 2개. 위치·회전·크기·label·hidden·cast_shadow, 추가 액터 제거와 삭제 액터 재생성 확인. 두 프로세스 exit 0·오류/경고 0 |
| 원본 보존 | 스냅샷 검사 전후 Content 패키지 829개 변경 0, 원본 개인 설정 ini SHA-256 동일 | 해당 파일 집합의 비교다. 기존 사용자 에디터를 유지했다 |

패키지 기동에서 기존 CommonUI 입력 라우팅 오류를 재현했다. `DefaultEngine.ini`의 `[/Script/Engine.Engine]`에 `GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient`를 지정했고, 최종 패키지의 네 맵 기동에서 재발하지 않았다. 검사 경고를 끄는 설정은 추가하지 않았다.

### 다시 실행하기

[패키지 다인 검사 실행기](../../scripts/validation/Invoke-PackagedMultiplayerValidation.ps1)에 **최신 소스로 빌드한 Development 패키지의 실제 게임 exe**를 지정한다. 엔진의 `UnrealEditor.exe -game` 실행과 구분한다.

```powershell
.\scripts\validation\Invoke-PackagedMultiplayerValidation.ps1 -GameExe '패키지\Windows\DIGETHER\Binaries\Win64\DIGETHER.exe'
```

실행기는 고유 결과 폴더·역할별 UserDir·loopback 포트를 사용하며 `result.json`에 단계·관측값·종료 결과를 남긴다. 새 sampleSequence와 전원 상태를 요구하고, 정지 후 위치 허용 오차는 기본 2cm다. 이 값은 시험 기준이며 제품 성능 목표가 아니다. 테스트용 [NetworkProbe](../../DIGETHER/Source/DIGETHER/Tests/DigetherNetworkProbe.cpp)는 개발 자동검사 컴파일 가드와 명시적 명령줄 opt-in을 모두 요구한다. 일반 실행에서는 ticker·파일 감시·입력을 시작하지 않는다. 기존 `AddMovementInput` 경로를 사용하며 새 RPC나 게임 규칙은 추가하지 않는다.

[스냅샷 왕복 fixture](../../scripts/validation/validate_level_snapshot_roundtrip.py)는 파일 상단 명령으로 `capture`와 `restore`를 **서로 다른 별도 에디터 프로세스**에서 순서대로 실행한다.

- `-ExecutePythonScript=...`, `-ScriptErrorsAreFatal`, `--allow-isolated-editor`가 필요하다. `-run=pythonscript` Commandlet에는 액터 제거에 필요한 `GUnrealEd`가 없어 이 검사를 실행할 수 없다.
- 원본 개인 ini를 고유 실행 폴더로 복사하고 `-EditorPerProjectUserSettingsIni=복사본`을 사용한다. MCP 자동 시작 제한도 그 프로세스에만 전달한다.
- `-UserDir=고유폴더/EditorUser/`로 Asset Search DB와 사용자 저장 경로를 격리한다. fixture의 `--run-dir`은 실효 Saved 경로인 `고유폴더/EditorUser/Saved/` 아래여야 한다. 같은 UserDir을 두 단계에서 직렬로 재사용한다.
- `/Game/DigetherSnapshotQA<고유값>/`만 Saved의 시험 Content로 중첩 마운트한다. 저장 전에 map·snapshot의 해석 경로가 예상 Saved 경로와 정확히 같은지 검사한다. 공유 Content에는 쓰지 않는다.
- 저장 후 검증기가 다음 tick에 실행되므로 마운트는 해당 프로세스 종료까지 유지한다. 검증기를 비활성화하거나 복원 단언을 약화하지 않는다.
- 두 단계의 JSON·성공 마커·종료 코드와 엔진 로그를 함께 확인한다. 첫 단계에서 복원해 저장한 맵은 두 번째 프로세스에서 재로드를 확인한다. 두 번째 재복원 후 저장한 맵의 세 번째 프로세스 재로드까지 검사한 것은 아니다.

현재 기획의 세션·후입·이탈·영구 저장 정책과 리슨/전용 서버 선택은 이 검사로 확정하지 않는다. M3는 다른 PC·실제 계정/서비스 환경에서 별도로 수행해야 한다. 스냅샷 결과도 복잡한 Blueprint·Landscape·외부 에셋 전체 복원을 보장하지 않는다.
