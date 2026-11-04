# 디버그·테스트 기능 카탈로그

기준일 2026-11-04 · UE 5.8.4 (CL 58902052) · 상태: **조사 자료, 채택 결정 아님**. 색인은 [기능 카탈로그](README.md).

근거 표기: `소스`=로컬 `Engine/...` 헤더·`.uplugin`·`.Build.cs`·`.cpp` 직접 확인, `문서`=Epic 문서를 실제로 열어 확인, `미확인`=이번에 확인 못 함. 기존 사용 규칙은 반복하지 않고 링크한다: [Unreal 테스트](../TESTING.md) · [개발 도구 안내](../../tools/DEVELOPER_TOOLS.md) · [로그 분류](../../design/LOG_CATEGORIES.md) · [검증 계획](../../verification/VERIFICATION_PLAN.md) · [표준 기능](../STANDARD_FEATURES.md).

## 요약
- **Automation Simple Test**가 프로젝트 주력(소스에 121개 매크로 사용). Spec·Complex·CQTest는 미사용. 월드가 필요한 테스트는 **CQTest**(`Engine/Source/Developer/CQTest`)가 후보다. 리슨 서버+클라이언트 PIE 테스트(`AsListenServer`, `WithClients`)를 제공한다.
- 호스트형 4인 시나리오의 자동화 후보는 둘이다. 에디터 내부 PIE 다인은 CQTest `NETWORK_TEST_CLASS`, 별도 프로세스·패키지는 **Gauntlet**(`UE.Networking`에 `ListenServer` 노드 존재, 플러그인은 Beta·기본 꺼짐).
- **발견**: `scripts/Invoke-UnrealChecks.ps1 Automation`은 `Workbench.` 접두 테스트만 실행·대조한다. `DIGETHER.Game.*` 54개는 이 실행기 범위 밖이다(소스 읽기 근거, 실행은 안 해 봄).
- 5.8에서 로그 매크로 `UE_LOG`의 설명이 "`UE_LOGF` 선호"로 바뀌었다(UTF-8 포맷 문자열). 프로젝트는 `UE_LOG` 91곳, `UE_LOGF`·`UE_LOGFMT` 0곳.
- 치트: `UCheatManager` + `CheatClass`는 이미 사용. 모듈식 확장은 `UCheatManagerExtension`. Shipping 기본값은 로깅·콘솔 비활성이다(`Misc/Build.h`).
- 시간축 디버깅은 Visual Logger + Rewind Debugger(`GameplayInsights` 플러그인, 프로젝트에 Editor 한정으로 활성). 서버 권한 상태 확인은 Gameplay Debugger(`'`키)의 사용자 정의 카테고리가 후보.

## DIGETHER에서 쓰이는 곳
| 항목 | 상태 |
|---|---|
| 자동 테스트 | 사용 중. `Source/DIGETHER`에 `IMPLEMENT_SIMPLE_AUTOMATION_TEST` 57개(`DIGETHER.Game.*` 54, `DIGETHER.Prototype.*` 3, 전부 `EditorContext\|EngineFilter`), `DevelopmentWorkbench` 58, `DigetherWorkbench` 6. `WITH_DEV_AUTOMATION_TESTS` 참조는 `DIGETHER/Source/DIGETHER`에서 34줄 |
| 명령줄 실행 | 사용 중. `Invoke-UnrealChecks.ps1`이 `-ExecCmds=Automation RunTests Workbench.`, `-TestExit=Automation Test Queue Empty`, `-ReportExportPath`를 사용 |
| 로그 | `LogDigetherDev`·`LogDigetherEconomy` 등 도메인별 카테고리 8종 이상(`Game/*/Digether*Log.h`). `UE_LOG(..., TEXT())` 91곳 |
| 치트 | `UDigetherCheatManager`(Exec 5개) + `DigetherDev::Register` 처리기 틀. `DigetherGameplayPlayerController`가 `CheatClass`로 연결. 프로토타입 컨트롤러는 미연결 |
| 콘솔 명령·CVar | 코드로 등록한 것 없음(`FAutoConsole*` 0). `[ConsoleVariables]`(DefaultGame.ini)에 CommonUI 3개 |
| Visual Logger·Gameplay Debugger·Draw Debug | 코드 사용 없음 |
| Rewind Debugger | `GameplayInsights` 활성(`TargetAllowList=[Editor]`). 실제 기록·재생 성공은 미검증([도구 안내](../../tools/DEVELOPER_TOOLS.md)) |
| 다인 | PIE 다인 설정 미지정(DefaultEditor*.ini에 Play 설정 없음). 다인 확인은 패키지 실행기 [Invoke-PackagedMultiplayerValidation.ps1](../../../scripts/validation/Invoke-PackagedMultiplayerValidation.ps1)과 `DigetherNetworkProbe`(개발 전용, 명령줄 opt-in)로 수행 |
| Live Coding | 재등록 미검증(검증 계획). 에디터 종료 후 BuildEditor가 기본 절차 |

## 기능 목록
이 문서의 `미사용`은 별도 근거가 없으면 `DIGETHER/Source`·`Config`의 사용처 미발견을 뜻하며, Blueprint·에셋 내부 사용과 런타임 모듈 로드는 미검증이다. 플러그인 활성 설정과 도구 실행·성공 기록은 구분한다.

| 기능 | 모듈·플러그인 (타입, 안정성) | 하는 일 | DIGETHER 쓸모 | 프로젝트 상태 | 근거 |
|---|---|---|---|---|---|
| Simple Automation Test | Core `Misc/AutomationTest.h` (엔진 내장) | 함수 하나의 단위 테스트 | 규칙·계산·상태 전이 | **사용 중** | 소스 |
| Complex Automation Test | 동일 | 입력 목록마다 테스트를 전개(`GetTests`) | 데이터 정의 일괄 검사 후보 | 미사용 | 소스 |
| Spec / BDD (`DEFINE_SPEC`, `BEGIN_DEFINE_SPEC`) | 동일 | Describe/It 구조, Latent 지원 | 상태 공유 많은 테스트 | 미사용 | 소스 |
| Latent 명령 (`DEFINE_LATENT_AUTOMATION_COMMAND*`, `ADD_LATENT_AUTOMATION_COMMAND`) | 동일 | 여러 프레임에 걸친 단계 | 월드·타이머 테스트 | 미사용 | 소스 |
| Networked Automation Test (`IMPLEMENT_NETWORKED_AUTOMATION_TEST`) | 동일 | `NumParticipants`(필요 기기 수)를 선언하는 다기기 테스트 | 원격 실행기 전제, 이 프로젝트에선 낮음 | 미사용 | 소스(사용법 미확인) |
| CQTest | Developer 모듈 `CQTest` (플러그인 아님, 개발 도구 빌드 한정) | `TEST_CLASS`/`TEST_METHOD`/`BEFORE_EACH`, `ASSERT_THAT`, 명령 빌더(`Do/StartWhen/Until`), 스포너 | 월드·컴포넌트 단위 통합 테스트 | 미사용(Build.cs 의존 없음) | 소스 |
| CQTest PIE 네트워크 (`NETWORK_TEST_CLASS`, `PIENetworkComponent`) | 동일, `WITH_EDITOR` 한정 | 서버(`AsListenServer`/`AsDedicatedServer`)+클라이언트 N개 PIE를 띄워 `ThenServer/ThenClients/UntilClients` | **리슨 서버 복제 테스트의 에디터 내 자동화** | 미사용 | 소스 |
| CQTest 확장 플러그인 | `CQTestEnhancedInput`(Beta, DeveloperTool), `CQTestExperimental`(**Experimental**) | 입력 주입, 실험 기능 | 입력 시뮬레이션 | 미사용 | 소스 |
| Functional Test | Developer 모듈 `FunctionalTesting` + 플러그인 `FunctionalTestingEditor`(Editor, 기본 꺼짐, Beta 아님) | 레벨에 `AFunctionalTest` 배치, `FinishTest`·`AssertEqual_*`·`TimeLimit` | 시험 맵 기반 시나리오(굴착·붕괴·엘리베이터) | 미사용 | 소스 |
| Low-level tests (Catch2) | `LowLevelTestsRunner` + UAT `LowLevelTests` | 엔진 모듈 단위의 독립 실행 테스트 | 게임 모듈에 쓰려면 별도 구성 필요, 낮음 | 미사용, 프로젝트 적용·실행 미확인 | 소스 `Engine/Source/Developer/LowLevelTestsRunner/LowLevelTestsRunner.Build.cs`의 Catch2 구성 |
| Python Automation Test | `PythonAutomationTest`(Editor, **Beta**, 기본 꺼짐) | Python으로 작성한 자동화 | 에셋 배선 검사 | 미사용(PythonScriptPlugin은 활성) | 소스 |
| Automation 명령줄 | `AutomationController` | `Automation List/RunTests/RunFilter/RunAll/Quit/SoftQuit/AllowZeroTestResults/SetTagFilter` | CI·로컬 검사 | **사용 중** | 소스·문서 |
| Session Frontend Automation 창 | `AutomationWindow` | Tools > Test Automation에서 테스트 선택·실행 | 수동 확인 | 사용 기록 미확인 | 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/run-automation-tests-in-unreal-engine (로컬 에디터 메뉴 실행 미검증) |
| Gauntlet | 플러그인 `Gauntlet`(Experimental 폴더, Runtime, **Beta**, 기본 꺼짐) + UAT `Gauntlet` | 여러 프로세스(서버+클라이언트)를 띄우고 컨트롤러로 상태 감시 | 별도 프로세스 4인 스모크 | 미사용 | 소스·문서 |
| AutomatedPerfTesting | 플러그인(Runtime, **Experimental**) | Gauntlet 기반 성능 테스트 컨트롤러 | 성능 반복 측정 → [PERF_BUILD](PERF_BUILD.md) | 미사용 | 소스 |
| NetcodeUnitTest | 플러그인(UncookedOnly, 기본 꺼짐) | 엔진 넷코드 취약점 시험 프레임 | 엔진 자체 시험용, 보류 | 미사용 | 소스 |
| 로그 매크로 | Core `Logging/LogMacros.h`, `StructuredLog.h` | `UE_LOG`, `UE_LOGF`, `UE_LOGFMT`, `UE_CLOG*` | 사건 추적(`Run`·`Settle` 등) | `UE_LOG`만 사용 | 소스 |
| Message Log | Core `FMessageLog` + Developer `MessageLog` | 심각도별 토큰 메시지·에디터 메시지 창 | 에셋 검증 보고 | 직접 사용처 미발견·에디터 간접 사용 미검증 | 소스 `Engine/Source/Runtime/Core/Public/Logging/MessageLog.h`, `Engine/Source/Developer/MessageLog/Public/MessageLogModule.h` |
| Visual Logger | Engine `VisualLogger/`, 에디터 `LogVisualizer` | 시간축에 로그·도형 기록(`UE_VLOG*`) | 몬스터·정산 사건 재현 | 미사용 | 소스 |
| Gameplay Debugger | Runtime `GameplayDebugger`(+에디터 `GameplayDebuggerEditor`) | 게임 중 `'`키 오버레이, 사용자 카테고리 | 서버 상태를 클라이언트에서 확인 | 미사용 | 소스 |
| Rewind Debugger | `GameplayInsights` 플러그인(EditorAndProgram+Editor+Runtime, IsBetaVersion false, Experimental 키 없음, 기본 꺼짐) | Insights 트레이스를 되감아 오브젝트 상태·VLog 확인 | 몬스터·엘리베이터 사후 분석 | **명시 활성**(Editor 타깃 한정), 실제 기록·재생 미검증 | 소스 `Engine/Plugins/Animation/GameplayInsights/GameplayInsights.uplugin`, `DIGETHER/DIGETHER.uproject` |
| Cheat Manager | Engine `GameFramework/CheatManager.h` | Exec 함수, `CheatClass` | 개발 치트 | **사용 중** | 소스 |
| Cheat Manager Extension / `GameFeatureAction_AddCheats` | Engine, `GameFeatures`(**Beta**) | 모듈식 치트 추가 | 패키지별 치트 분리 후보 | 미사용 | 소스 |
| 콘솔 명령·CVar 등록 | Core `HAL/IConsoleManager.h` | `FAutoConsoleCommand*`, `TAutoConsoleVariable`, `ECVF_*` | 튜닝 값 실시간 변경 | 미사용 | 소스 |
| Draw Debug | Engine `DrawDebugHelpers.h` | 선·구·문자 즉시 그리기 | 굴착 범위·감지 반경 표시 | 미사용 | 소스 |
| 멀티플레이 PIE | `LevelEditorPlaySettings` (UnrealEd) | Net Mode, 클라이언트 수, 한 프로세스 여부, 고정 FPS, 네트워크 에뮬레이션 | 빠른 다인 반복 | 기본값 | 소스 |
| 네트워크 에뮬레이션 | Engine `Net/NetEmulationHelper` | 콘솔 `NetEmulation.PktLag/PktLoss/PktOrder/PktDup/PktJitter/PktEmulationProfile/Off`, RPC 드롭 | 지연·손실 조건 재현 | 미사용 | 소스 |
| `net.*` CVar | Engine(NetDriver 등) | 덤프·검증·디버그 그리기 | 복제·관련성 문제 조사 | 미사용 | 소스 |
| Networking Insights | `TraceInsights`(Insights 앱·에디터) + `NetChannel` 트레이스 | 연결·패킷·복제 오브젝트 단위 대역폭 | 호스트 업링크 조사 | 미사용 | 소스·문서 |
| 레거시 Network Profiler | Engine `Net/NetworkProfiler.h` | `NETWORK_PROFILER` 훅 | 뷰어(C#)가 설치본에 없음 → 보류 | 미사용 | 소스(뷰어 없음 확인) |
| Live Coding | Developer `Windows/LiveCoding` | 에디터 실행 중 C++ 핫 패치 | 반복 속도 | 설정 확인 필요 | 소스 |
| Chaos Visual Debugger | 플러그인 `ChaosVD`(**Beta**, 기본 켜짐) | 물리 시뮬레이션 기록·재생 | [PHYSICS_COLLISION.md](PHYSICS_COLLISION.md) 참조 | 미사용 | 소스 |

## 기능별 상세

### Automation Test (Simple / Complex / Spec / Latent)
- 핵심: `Engine/Source/Runtime/Core/Public/Misc/AutomationTest.h`. 플래그 `EAutomationTestFlags`: 실행 문맥(`EditorContext`, `ClientContext`, `ServerContext`, `CommandletContext`) 중 하나 이상 + **필터 정확히 하나**(`SmokeFilter`/`EngineFilter`/`ProductFilter`/`PerfFilter`/`StressFilter`/`NegativeFilter`)가 필요하며, 아니면 `static_assert`로 컴파일이 실패한다(소스). 프로젝트는 전부 `EditorContext | EngineFilter`.
- `IMPLEMENT_*`·Spec 매크로는 `WITH_AUTOMATION_WORKER`로 정적 테스트 인스턴스 등록을 분기한다. 꺼진 분기도 테스트 클래스 선언을 만들므로 빈 정의가 아니다(`AutomationTest.h`). `Misc/Build.h`의 worker 기본값은 `!UE_BUILD_SHIPPING`이며 프로젝트의 `#if WITH_DEV_AUTOMATION_TESTS` 소스 제외 가드와는 다르다. Shipping 제외는 [TESTING.md](../TESTING.md#테스트-모듈)의 기준을 따른다.
- 설정 위치: 없음(코드 기반). 이름은 `DIGETHER.<영역>.<동작>`.
- 네트워크: 단위 테스트는 월드 없이 규칙 함수만 부른다. 서버/클라이언트 동작은 아래 CQTest PIE 또는 Gauntlet.
- 제약: Complex는 `GetTests`로 이름 목록을 만들어야 하며, 프로젝트 실행기의 "소스 이름 목록 대조"(정규식이 `IMPLEMENT_SIMPLE_...`만 인식)와 맞지 않는다. Complex·Spec을 도입하면 실행기 대조 로직도 바꿔야 한다.
- 대안: CQTest(클래스 기반, 태그 필터), Functional Test(맵 기반).

### Automation 명령줄과 보고서
- 소스: `Engine/Source/Developer/AutomationController/Private/AutomationCommandline.cpp`(5.8.4 직접 확인). 명령: `List`, `RunTest`와 `RunTests`(둘 다 허용), `RunFilter`, `RunAll`, `Quit`, `SoftQuit`, `IgnoreLogEvents`, `EnableStereoTests`, `SetTagFilter`, `ApplyTagFilter`, `AllowZeroTestResults`.
- **0개 선택 동작(TESTING.md가 "미검증"으로 둔 부분)**: 일치하는 테스트가 없으면 `AllowZeroTestResults`가 없을 때 명령줄 오류로 처리하고, 있으면 경고만 남긴다(`AutomationCommandline.cpp`). 종료 코드 값은 이번에 실행하지 않아 미확인.
- 보고서: `-ReportExportPath=<폴더>`이면 `<폴더>/index.json`을 쓴다(`AutomationControllerManager.cpp`). 옛 인자 `-ReportOutputPath`는 경고 후 새 이름으로 안내한다. 문서: 같은 인자와 `-ResumeRunTest`(첫 "not-run" 테스트부터 재개, 진행 중이던 테스트는 실패 처리) — https://dev.epicgames.com/documentation/en-us/unreal-engine/run-automation-tests-in-unreal-engine
- `-TestExit="<문구>"`: 로그에 문구가 나오면 엔진 루프가 종료한다(`Runtime/Launch/Private/LaunchEngineLoop.cpp`의 `FOutputDeviceTestExit`). 문구는 대소문자 무시 부분 일치이며 명령줄 자체 출력은 무시한다. 위 문서 페이지에는 `TestExit` 설명이 없다.
- 프로젝트 주의: 실행기의 필터 `Workbench.`는 `DevelopmentWorkbench.`·`DIGETHER.Workbench.` 테스트를 고르는 용도다. `DIGETHER.Game.*`는 별도 실행이 필요하다(필터 변경은 실행기·기대 목록 대조 로직과 함께 결정). 이 항목은 [검증 계획](../../verification/VERIFICATION_PLAN.md)에 반영 여부 확인 필요.

### CQTest
- 헤더: `Engine/Source/Developer/CQTest/Public/CQTest.h`(`TEST_CLASS`, `TEST_METHOD`, `BEFORE_ALL/EACH`, `AFTER_EACH/ALL`, `ASSERT_THAT`, 태그 변형 `*_WITH_TAGS`), `Commands/TestCommandBuilder.h`, `Components/ActorTestSpawner.h`·`MapTestSpawner.h`, `Components/PIENetworkComponent.h`.
- 모듈 의존: `Core, CoreObject, Engine, NetCore, Slate, DeveloperSettings`(+에디터 빌드는 `UnrealEd`·`LevelEditor`). `CQTest.Build.cs` 확인. **Developer 모듈**이라 `bBuildDeveloperTools`가 꺼진 Test·Shipping 게임 타깃에는 빌드되지 않는다(`TargetRules.cs`). 게임 모듈에서 쓰려면 조건부 의존이 필요하다.
- PIE 네트워크 테스트: `ENABLE_PIE_NETWORK_TEST = WITH_EDITOR && (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)`. 빌더 `AsListenServer()`/`AsDedicatedServer()`, `WithClients(n)`(서버 제외 클라이언트 수), `WithGameMode(...)`. 검증 함수 `ThenServer/ThenClients/ThenClient(i)/UntilServer/UntilClients`(기본 타임아웃 `CQTest::DefaultTimeout`). 상태 구조체는 `FBasePIENetworkComponentState` 상속.
- 설정: Project Settings의 `CQ Test Settings`(`UCQTestSettings`, `config=Engine`) — CVar `TestFramework.CQTest.CommandTimeout`, `.Network`, `.MapTest`.
- 제약: Iris 복제 시스템을 분기해 다룬다(`IsUsingIrisReplication`). 이 프로젝트의 복제 방식(NetDriver는 `SteamSocketsNetDriver`)에서 실제로 도는지는 미확인. 에디터 PIE 안이라 Steam 세션·실제 소켓 경로는 검증하지 못한다 → M2 대체 불가([VERIFICATION_PLAN](../../verification/VERIFICATION_PLAN.md)).
- 대안: Gauntlet(별도 프로세스), 기존 패키지 실행기.

### Functional Test
- 클래스: `Engine/Source/Developer/FunctionalTesting/Classes/FunctionalTest.h`(`AFunctionalTest`: `PreparationTimeLimit`, `TimeLimit`, `FinishTest`, `SetTimeLimit`, `AssertEqual_Float` 등), `FunctionalTestingManager.h`, `FunctionalTestLevelScript.h`.
- 방식: 테스트 맵에 `AFunctionalTest` 액터를 배치해 Blueprint 또는 C++로 단계 작성. 에디터 UI 지원은 플러그인 `FunctionalTestingEditor`(Editor 타입).
- 제약: Developer 모듈이라 패키지에서 쓰려면 개발 도구 빌드가 필요(미확인). 맵 단위 실행이어서 시험 맵 소유·잠금 규칙([ASSET_SAFETY](../ASSET_SAFETY.md))과 맞물린다. 실행 이름 규칙은 미확인.

### Gauntlet
- 구성: 플러그인 `Engine/Plugins/Experimental/Gauntlet`(Runtime, `IsBetaVersion=true`, `EnabledByDefault=false`)의 `GauntletTestController`(명령줄 `-gauntlet=<컨트롤러>`로 생성) + UAT 쪽 C#(`Engine/Source/Programs/AutomationTool/Gauntlet`, 실행 클래스 `RunUnreal`). 역할 `UnrealTargetRole`: Editor, EditorGame, EditorServer, Client, Server, Host, CookedEditor.
- `Unreal/Automation/UE.Networking.cs`에 `DedicatedServer`·`ListenServer` 테스트 노드가 있다(구현 세부 미확인). 엔진 샘플 노드는 `Unreal/Game/Samples`.
- 문서: 여러 프로세스(서버+다수 클라이언트) 세션 실행 틀이며 게임 쪽 자동화 코드 없이도 쓸 수 있고, 플러그인의 TestController는 다단계 스모크용 — https://dev.epicgames.com/documentation/en-us/unreal-engine/gauntlet-automation-framework-in-unreal-engine (실행 명령은 그 페이지에 없어 미확인).
- 제약: 상태가 Beta이고 Launcher 설치본 기준 UAT 소스는 있으나 실제 실행은 이 프로젝트에서 검증된 적 없다. 기존 PowerShell 실행기(리슨 1→2→4인 통과 기록)와 역할이 겹친다. 도입은 기존 실행기를 대체할 이점이 있을 때만 판단.

### 로그: 매크로·카테고리·verbosity
- `Engine/Source/Runtime/Core/Public/Logging/LogMacros.h`: `UE_LOG`(TCHAR 포맷), **`UE_LOGF`**(ASCII/UTF-8 포맷, 메모리 이점 — 5.8 주석이 UE_LOGF 선호), `UE_CLOG` 조건부. `UE_LOG_REF`·`UE_LOG_CLINKAGE`는 "사용 금지, 추후 deprecated".
- `StructuredLog.h`: `UE_LOGFMT(Category, Verbosity, "…{Name}…", ("Name", Value)…)`, `UE_CLOGFMT`, `UE_LOGFMT_NSLOC`. 필드 이름 `[A-Za-z0-9_]+`, 필드 값은 `SerializeForLog` 또는 `operator<<(FCbWriter&…)`로 직렬화, 최대 16 필드. [로그 분류](../../design/LOG_CATEGORIES.md)의 `키=값` 규칙을 구조화 필드로 옮길 후보지만 **현 규칙은 `UE_LOG`+텍스트**이므로 도입은 대안 비교 대상이다.
- 카테고리: `DECLARE_LOG_CATEGORY_EXTERN`/`DEFINE_LOG_CATEGORY`/`DEFINE_LOG_CATEGORY_STATIC`(`LogCategory.h`). 프로젝트 규칙과 동일.
- 구성별 기본값(`Misc/Build.h`): Shipping에서 `USE_LOGGING_IN_SHIPPING=0`(로깅 제거, `NO_LOGGING`), `ALLOW_CONSOLE_IN_SHIPPING=0`, `USE_CHECKS_IN_SHIPPING=0`. 패키지 로그로 조사하려면 Development/Test 구성이 필요하다.
- 런타임 조정: 프로젝트 실행기가 `-LogCmds=LogDataValidation Log` 형태를 이미 사용(소스: 스크립트). 콘솔 `Log <카테고리> <수준>` 형태는 미확인.

### Visual Logger
- 헤더: `Engine/Source/Runtime/Engine/Public/VisualLogger/VisualLogger.h`. 매크로 `UE_VLOG`, `UE_VLOG_UELOG`, `UE_VLOG_SEGMENT`, `_LOCATION`, `_BOX`, `_CAPSULE`, `_ARROW`. 뷰어는 Developer 모듈 `LogVisualizer`.
- 활성 조건: `ENABLE_VISUAL_LOG = PLATFORM_DESKTOP && !NO_LOGGING && UE_ENABLE_DEBUG_DRAWING`, 후자는 `!(Shipping||Test) || WITH_EDITOR`(`EngineDefines.h`). 즉 Development 패키지에서도 켜지고 Shipping에서는 빠진다.
- **5.8 변경**: `VisualLoggerDefines.h`에 `UE_DEBUG_RECORDING_ENABLED`·`UE_DEBUG_RECORDING_USING_VLOG`·`UE_DEBUG_RENDERING_ENABLED`가 생겼고, 기록이 VLog 또는 Insights 트레이스로 갈 수 있다. `CONNECT_WITH_VLOG`·`CONNECT_OBJECT_WITH_VLOG`는 **5.8 deprecated 매크로**("더 이상 쓰지 않음"); `REDIRECT_TO_VLOG`는 유지.
- 네트워크: 기록은 기록한 프로세스 로컬이다. 호스트와 클라이언트의 로그를 맞추려면 각각 저장해 비교해야 한다(추정, 소스에서 복제 경로를 찾지 못함 → 미확인).
- 문서 페이지는 이번에 본문 추출이 실패했다. 사용법은 소스 근거만 있다.
- 대안: 텍스트 로그(`Seq`·`Settle` 키), Rewind Debugger(`RewindDebuggerVLog` 모듈이 VLog를 타임라인에 표시).

### Gameplay Debugger
- 모듈 `Engine/Source/Runtime/GameplayDebugger`. 기본 활성 키 `'`(Apostrophe, `GameplayDebuggerConfig.cpp`). 사용자 카테고리: `FGameplayDebuggerCategory`(`CollectData()`는 **권한 쪽(서버/스탠드얼론)에서 호출**, `DrawData()`는 매 틱 표시, `SetDataPackReplication`으로 복제 필드 지정), 등록은 `IGameplayDebugger::RegisterCategory`(`GameplayDebuggerModule.h`).
- 컴파일 조건: `WITH_GAMEPLAY_DEBUGGER`는 `bUseGameplayDebugger`가 켜졌을 때 1이며 기본값은 `bBuildDeveloperTools || (구성이 Test·Shipping 아님)`(`TargetRules.cs`). Shipping·Test 게임 타깃에서는 꺼진다.
- 쓸모: 서버만 아는 상태(몬스터 감지 상태, 엘리베이터 상태)를 클라이언트 화면에서 확인.
- 제약: 사용자 카테고리를 만들면 `GameplayDebugger` 모듈 의존과 `#if WITH_GAMEPLAY_DEBUGGER` 가드가 필요하다. 문서 페이지는 이번에 본문 추출 실패.

### Rewind Debugger / Gameplay Insights
- 플러그인 `Engine/Plugins/Animation/GameplayInsights`(표시 이름 "Animation Insights", `EnabledByDefault=false`, `IsBetaVersion=false`, Experimental 키 없음, 설명 "Allows debugging of animation systems via Unreal Insights"). 모듈: `GameplayInsights`(EditorAndProgram), `GameplayInsightsEditor`·`RewindDebugger`·`RewindDebuggerVLog`(Editor), `RewindDebuggerRuntime`·`RewindDebuggerVLogRuntime`(Runtime). Beta/Experimental 표시 부재는 안정성 보증이 아니다.
- 프로젝트: `.uproject`에서 `TargetAllowList=[Editor]`로 활성. Runtime 모듈이 포함된 플러그인이므로 Game 패키지에 들어가는지는 패키지 receipt로 따로 확인해야 한다(미확인).
- 사용법(트레이스 채널, 기록 대상 등록)은 이번에 문서·소스에서 확인하지 못했다.

### Cheat Manager와 확장
- `Engine/Source/Runtime/Engine/Classes/GameFramework/CheatManager.h`: `UCheatManager`(`Within=PlayerController`), 내장 Exec 치트 다수, `FOnCheatManagerCreated`. `APlayerController::CheatClass`/`CheatManager`(`PlayerController.h`): Shipping에서는 항상 비활성, 에디터는 항상 활성, 그 외는 싱글플레이 기본 활성이며 `EnableCheats` 콘솔로 강제 가능. 변경은 `APlayerController::EnableCheats` 또는 `AGameModeBase::AllowCheats` 재정의.
- **deprecated(5.8)**: `UCheatManager::DebugCameraControllerRef`("`FDebugCameraManager` 사용").
- `UCheatManagerExtension`(`Within=CheatManager`, `AddedToCheatManager`/`RemovedFromCheatManager`)로 시스템별 치트를 모듈식으로 추가한다. `UGameFeatureAction_AddCheats`(GameFeatures 플러그인, **Beta**)가 이를 Game Feature에 연결한다.
- 프로젝트 틀: [DigetherDevCommands.h](../../../DIGETHER/Source/DIGETHER/Game/Dev/DigetherDevCommands.h)의 `Register/Execute`가 처리기를 모으고 `UDigetherCheatManager`의 Exec 함수가 호출한다. 명령 추가는 예약 목록과 Exec 함수를 함께 늘린다. 로그·명령 응답 규칙은 이미 [로그 분류](../../design/LOG_CATEGORIES.md)에 있으므로 반복하지 않는다.
- 리슨 서버 고려: 치트 매니저는 해당 `PlayerController` 안에 있다. 서버 권한 상태(정산·시간)를 바꾸는 치트가 클라이언트에서 호출될 때 서버로 전달되는 경로는 이번에 확인하지 않았다 → 처리기 구현 시 서버 RPC/권한 검사를 별도 설계해야 한다(미확인).

### 콘솔 명령·CVar·Exec
- 헤더: `Engine/Source/Runtime/Core/Public/HAL/IConsoleManager.h`. 등록: `RegisterConsoleVariable[Ref]`, 편의 클래스 `FAutoConsoleCommand*`, `TAutoConsoleVariable`. 플래그 `ECVF_Default`, `ECVF_Cheat`(일부 타깃에서 비활성), `ECVF_Scalability`, `ECVF_RenderThreadSafe`, `ECVF_Preview` 등.
- Exec 함수(`UFUNCTION(Exec)`)는 `PlayerController`·`CheatManager`·`GameMode` 등 콘솔 입력 라우팅 대상 객체에서만 동작한다(라우팅 대상 목록은 미확인).
- 설정 위치: `[ConsoleVariables]`(프로젝트 DefaultGame.ini에 CommonUI 3개 존재).
- 대안 선택은 [로그 분류](../../design/LOG_CATEGORIES.md)의 개발 명령 틀 vs 직접 `FAutoConsoleCommand`. 치트 틀이 Shipping을 거절하는 보호가 있으므로 새 명령은 틀에 붙이는 쪽이 기존 결정과 맞다.

### Draw Debug
- `Engine/Source/Runtime/Engine/Public/DrawDebugHelpers.h`: `DrawDebugLine/Sphere/String/...`, `FlushPersistentDebugLines`. `ENABLE_DRAW_DEBUG = UE_ENABLE_DEBUG_DRAWING`(Shipping·Test에서 빈 인라인 함수로 대체, 에디터는 항상 켜짐).
- 호출은 `UWorld*` 필요. 서버/클라이언트 어느 쪽 월드에 그리는지 직접 구분해야 한다. 리슨 서버 PIE에서는 호스트 화면에 그려진다.
- 복제 이동 디버그: CVar `Net.RepMovement.DrawDebug`(소스).

### 멀티플레이 PIE 설정
- 클래스 `ULevelEditorPlaySettings`(`Engine/Source/Editor/UnrealEd/Classes/Settings/LevelEditorPlaySettings.h`), 카테고리 `Multiplayer Options`. 에디터 설정 UI 경로는 미확인.
- `PlayNetMode`: `PIE_Standalone`, **`PIE_ListenServer`("Play As Listen Server")**, `PIE_Client`("Play As Client", 서버 자동 생성). `PlayNumberOfClients`: 1~10. `RunUnderOneProcess`: 끄면 클라이언트마다 별도 프로세스(여러 창). 켜면 한 프로세스(빠르지만 전역 상태 공유로 문제가 생길 수 있다는 설명이 소스 주석에 있음).
- `bLaunchSeparateServer`, `ServerPort`, `AdditionalServerGameOptions`(예 `?listen`), `AdditionalServerLaunchParameters`, `ClientWindowWidth/Height`, `ServerFixedFPS`·`ClientFixedFPS`(RunUnderOneProcess일 때), `NetworkEmulationSettings`(클라이언트 2 이상일 때 표시: 프로파일, 패킷 손실 등).
- 리슨 서버에서 Number of Players를 4로 두면 호스트 포함 4인인지는 이번에 확인하지 못했다(CQTest 주석은 "리슨 서버를 제외한 클라이언트 수"로 센다) → 설정 후 실제 접속 인원을 로그로 확인할 것.
- 제약: PIE는 Steam 세션(`SteamSocketsNetDriver`)을 쓰지 않는다. 프로젝트 기록의 M2 판정은 별도 프로세스 실행기를 따른다([TESTING.md](../TESTING.md#다중-프로세스-스모크)).

### 네트워크 디버깅
- 지연·손실: 콘솔 `NetEmulation.PktLag <ms>`, `PktLoss`, `PktOrder`, `PktDup`, `PktLagVariance/Min/Max`, `PktJitter`, `PktEmulationProfile <이름>`, `Off`, RPC 드롭 `DropAnyUnreliable`/`DropNothing`/`DropUnreliableOfActorClass`(`NetEmulationHelper.cpp`). 프로파일은 네트워크 설정의 `NetworkEmulationProfiles`. 호스트형 방의 불안정 연결 재현 후보.
- 조사용 CVar(소스 `NetDriver.cpp` 등): `net.DormancyEnable`, `net.DormancyValidate`, `net.DumpRelevantActors`, `net.DumpActiveNetActors`, `net.ListNetGUIDExports`, `net.PackageMap.DebugAll`/`DebugObject`, `net.RPC.Debug`, `net.Replication.DebugProperty`, `net.DebugDraw`, `net.PrintNetConnections`, `net.DisableBandwithThrottling`(철자 그대로), `net.MaxRPCPerNetUpdate`. 이동 보정 표시는 `p.NetShowCorrections`(`CharacterMovementComponent.cpp`). 값 의미는 각 CVar 도움말을 `help <이름>`으로 확인.
- Networking Insights: Insights 앱의 `NetworkingProfiler` 뷰(`Developer/TraceInsights/Private/Insights/NetworkingProfiler`), 트레이스 채널 `Net`(`NetTraceReporter.cpp`: 연결·패킷·복제 오브젝트 수명·연결별 통계). `UE_NET_TRACE_ENABLED`는 `!UE_BUILD_SHIPPING && UE_TRACE_ENABLED`. 상세도 조정 콘솔 `NetTrace.SetTraceVerbosity`. 문서: Networking Insights가 네트워크 트래픽 분석·최적화·디버깅용이라고 확인 — https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-insights-in-unreal-engine
- 레거시 Network Profiler: `USE_NETWORK_PROFILER = !(Shipping||Test)`로 훅은 남아 있으나 설치본에서 뷰어를 찾지 못했다 → 새 조사에는 Networking Insights.
- 복제 설계·관련성은 [NETWORKING.md](NETWORKING.md), 권한 규칙은 [NETWORK_AUTHORITY](../NETWORK_AUTHORITY.md).

### Live Coding
- 모듈 `Engine/Source/Developer/Windows/LiveCoding`. 설정 `ULiveCodingSettings`: `bEnabled`, `Startup`(`ConfigRestartRequired`), `bEnableReinstancing`, 새 C++ 클래스 자동 컴파일, 모듈 목록. 콘솔 `LiveCoding`, `LiveCoding.Compile`, `LiveCoding.CompileSync`, 명령줄 `-LiveCoding`(소스 `LiveCodingModule.cpp`).
- 주의(프로젝트 기준): 재등록·Auto Save 복구는 미검증. `UCLASS`·`UPROPERTY`·생성자·CDO 변경은 핫 패치로 반영되지 않을 수 있어 에디터를 닫고 BuildEditor가 안전하다고 알려져 있으나 이번 5.8.4 문서·실험으로는 확인하지 못했다(미확인). 빌드 검증 절차는 [도구 안내](../../tools/DEVELOPER_TOOLS.md#로컬-검사-실행)를 따른다.

### Chaos Visual Debugger
- 플러그인 `ChaosVD`(EditorAndProgram, **IsBetaVersion=true**, 기본 켜짐). 물리 사용처와 사용법은 [PHYSICS_COLLISION.md](PHYSICS_COLLISION.md) 참조. 이 문서에서는 다루지 않는다.

## 보류·비추천 후보와 이유
| 후보 | 이유 |
|---|---|
| `IMPLEMENT_NETWORKED_AUTOMATION_TEST` | 원격 기기 실행기 전제. 현 구성은 로컬 프로세스 실행기로 충족 |
| NetcodeUnitTest | 엔진 넷코드 취약점 시험용. 게임 규칙 검증과 거리가 멂 |
| AutomatedPerfTesting | Experimental. 성능 반복이 필요해질 때 재평가 → [PERF_BUILD](PERF_BUILD.md) |
| 레거시 Network Profiler | 뷰어가 설치본에 없음 |
| `UE_LOG_REF`·`UE_LOG_CLINKAGE` | 엔진 주석이 사용 금지·deprecated 예정 |
| `UCheatManager::DebugCameraControllerRef` | 5.8 deprecated |
| `CQTestExperimental` | Experimental 표시 |
| Gauntlet 즉시 도입 | Beta, 기존 실행기와 중복. 이점이 입증되면 재검토 |

## 확인 한계
- 이번 조사는 헤더·`.uplugin`·`.Build.cs`·일부 `.cpp` 읽기이며 **엔진 실행은 하지 않았다**. 지원 사실이 프로젝트 실제 검증을 뜻하지 않는다.
- 이번 재대조에서 Run Automation Tests·Unreal Insights 공식 페이지 본문을 직접 열었다. 기존 조사에는 Gauntlet 소개 열람과 Visual Logger·Gameplay Debugger·Packaging·Multiplayer PIE 본문 추출 실패가 기록되어 있으며, 이 페이지들은 이번에 다시 열지 않았다.
- 미확인: Functional Test 실행 이름·`FunctionalTestingEditor` 사용법, Low-level tests의 게임 프로젝트 적용, Gauntlet 실제 실행 명령, Rewind Debugger 사용법, 치트의 서버 전달 경로, Live Coding 제약, 에디터 설정 UI 경로, `Log` 콘솔 명령 형식, 0개 테스트 종료 코드 값, CQTest PIE 네트워크 테스트의 Steam 소켓 환경 동작.
- 기존 문서와 다른 점: [TESTING.md](../TESTING.md)가 "미검증"으로 둔 0개 선택 동작을 소스에서 확인했다(위 Automation 절). 기타 사실 충돌은 발견하지 못했다.
