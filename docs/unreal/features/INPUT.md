# 입력 기능 카탈로그

기준일 2026-11-04 · UE 5.8.4 (CL 58902052) · 상태: **조사 자료, 채택 결정 아님**. 색인은 [기능 카탈로그](README.md).

범위: Enhanced Input(Action·Mapping Context·Modifier·Trigger·사용자 설정·리바인딩), 입력 모드(Game/UI), CommonInput(게임패드 아이콘 포함), GameInput 플러그인, 입력 디버깅, 접근성(홀드/토글). 캐릭터·카메라는 [GAMEPLAY_FRAMEWORK.md](GAMEPLAY_FRAMEWORK.md), UI 화면은 [UI.md](UI.md), 설정 저장은 [DATA_SAVE.md](DATA_SAVE.md), 입력 시험은 [DEBUG_TEST.md](DEBUG_TEST.md)를 본다. 프로젝트 기준은 [표준 기능](../STANDARD_FEATURES.md#enhanced-input)이다.

경로 약칭: `EI` = `Engine/Plugins/EnhancedInput/Source/EnhancedInput`, `CI` = `Engine/Plugins/Runtime/CommonUI/Source`, `ENG` = `Engine/Source/Runtime/Engine`. 모두 로컬 5.8.4 설치 기준 상대 경로다.

## 요약
- **Enhanced Input이 기본 경로다.** 플러그인 `EnabledByDefault: true`, `IsBetaVersion: false`, 공식 문서도 실험 표시가 없다. 프로젝트는 `DefaultInput.ini`에서 이미 `EnhancedPlayerInput`/`EnhancedInputComponent`를 기본으로 지정했다.
- **현재 코드는 두 갈래다.** 템플릿 `DIGETHERCharacter`는 Enhanced Input(`BindAction`+`AddMappingContext`)을 쓰지만, 실제 프로토타입 `ADigetherPrototypeCharacter`는 `BindKey`/`BindAxisKey`를 직접 쓴다. [표준 기능](../STANDARD_FEATURES.md)이 피하라고 한 구식 바인딩이며 키 재설정·게임패드·컨텍스트 전환이 안 된다. 정식 Pawn에서 정리 대상이다.
- **리바인딩은 엔진 기능이 있다.** `UEnhancedInputUserSettings`(SaveGame 기반)와 `UPlayerMappableKeySettings`, 키 프로필로 매핑·저장·초기화를 제공한다. 5.8에서는 `UPlayerMappableInputConfig`가 5.3부터 deprecated이므로 쓰지 않는다. 프로젝트는 아직 컨텍스트를 사용자 설정에 등록하지 않는다.
- **5.8의 입력 모드 태그 필터링(GameplayTag)이 있다.** 필터 대상 Mapping Context는 현재 입력 모드와 쿼리가 맞을 때만 처리된다. 기본 옵션은 프로젝트 기본 쿼리 사용이며, `Engine/Config/BaseInput.ini`는 쿼리와 입력 모드 모두 `EnhancedInput.Modes.Default`를 지정한다. 빈 쿼리는 일치하지 않는다(아래 소스 경로).
- **CommonUI·CommonInput은 이미 활성이다.** 프로젝트는 `CommonGameViewportClient`와 `BP_UIInputData`를 설정했고 메뉴 화면이 `FUIInputConfig`로 입력 모드를 선언한다. 단 `bEnableEnhancedInputSupport`는 기본 `false`이며 프로젝트에서 켜지 않았다.
- **GameInputWindows는 Beta이며 GameInput 기반 플러그인 자체는 `IsBetaVersion:false`다.** 두 플러그인 모두 기본 비활성이며 프로젝트 descriptor에 명시되지 않았다. 기본 입력 경로를 바꿀 이유가 확인되기 전에는 후보로만 둔다.

## DIGETHER에서 쓰이는 곳
사용 여부는 `DIGETHER.uproject`·`DIGETHER/Source`·`Config`를 확인한 범위다. `미사용`·참조 미발견은 Blueprint·에셋 내부 부재나 실제 플러그인 비활성을 뜻하지 않는다.

| 관련 기능 | 프로젝트 상태 |
|---|---|
| S01~S05 이동·시점·조준 상호작용 | Prototype은 `BindKey`/`BindAxisKey`(`Prototype/Player/DigetherPrototypeCharacter.cpp:476~505`) 직접 바인딩. 기존 `IA_*`/`IMC_*` 에셋(`DIGETHER/Content/Input/`의 `IA_Jump`·`IA_Look`·`IA_MouseLook`·`IA_Move`, `IMC_Default`, `IMC_MouseLook`)은 템플릿 `DIGETHERCharacter`/`DIGETHERPlayerController`가 사용. 템플릿 Variant 폴더에도 `IA_*`/`IMC_*`가 있다 |
| 정식 Pawn | 미결정. `ADigetherTestPawn`(`ADefaultPawn`)은 입력을 직접 정의하지 않는다 |
| 관전·레버·긴급 탈출 입력 | Gameplay PlayerController는 입력 바인딩 없이 콘솔 `Exec`(`ProcessConsoleExec`)와 서버 RPC만 있다(`DigetherGameplayPlayerController.h`) |
| S30~S33 설정·접근성 | `UDigetherGameUserSettings`가 마우스 감도·Y 반전을 `GameUserSettings.ini`에 저장(`Game/Settings/`). 템플릿 Character가 읽어 적용(`DIGETHERCharacter.cpp:120`). **키 재설정 C++ 구현(2026-11-06)**: `DefaultInput.ini bEnableUserSettings=True`, `UDigetherKeyBindingLibrary`(행 조회·바뀐 키만 `MapPlayerKey`·`AsyncSaveSettings`), `UDigetherUserOptionsSubsystem`이 DIGETHER Options의 `RebindableMappingContexts`를 등록, 설정 패널 키 탭(겹치는 키는 교환). 행이 보이려면 IA에 Player Mappable Key Settings가 있어야 하며 에셋 지정 전이라 실기 미검증 |
| UI 입력 | `DefaultEngine.ini`에 `GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient`, `DefaultGame.ini`에 `[/Script/CommonInput.CommonInputSettings] InputData=BP_UIInputData`. 메인 메뉴·로비 화면은 `FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture)` |
| 입력 모드 전환 | `ADigetherFrontendPlayerController::EndPlay`에서 `SetInputMode(FInputModeGameOnly())`, Prototype 납부 입력은 `FInputModeGameAndUI`+`SetIgnoreMoveInput` |
| 입력 플러그인 | `uproject`에 `EnhancedInput`은 명시되지 않았으나 엔진 기본 활성이며 모듈 의존(`DIGETHER.Build.cs`: `EnhancedInput`, `InputCore`, 비공개 `CommonInput`)이 있다. `GameInput*`·`PlayerInputDebugger`는 기본 비활성이고 프로젝트 명시 없음. `InputDebugging`은 기본 활성·Runtime 모듈 Shipping 제외이며 실제 로드는 미확인 |
| 시험 | `Game/Tests/DigetherFrontendWidgetReuseTest.cpp`가 `ICommonInputModule::GetSettings().LoadData()`를 호출. 입력 자체 자동 시험은 확인하지 못했다 |

기존 [UNREAL_FEATURE_COVERAGE.md](../../design/UNREAL_FEATURE_COVERAGE.md)와의 차이: 대응표는 Enhanced Input의 `AddMappingContext`/`BindAction` 존재만 적었다(5.8.2 시점). 5.8.4 헤더에서 사용자 설정·입력 모드 태그·월드 서브시스템(Experimental) 등 추가 항목을 이번에 확인했다. 사실 충돌은 발견하지 못했다. 같은 문서의 "Enhanced Input descriptor `IsBetaVersion=false`"는 5.8.4에서도 같다.

## 기능 목록
근거 표기: `소스` 로컬 5.8.4 헤더·`.uplugin`, `문서` Epic 공식 문서, `미확인`.

| 기능 | 모듈·플러그인 (타입, 안정성) | 하는 일 | DIGETHER 쓸모 | 프로젝트 상태 | 근거 |
|---|---|---|---|---|---|
| `UInputAction` | EnhancedInput (Runtime, 비Beta) | 입력 의도 데이터 에셋. 값 형식 Bool/1D/2D/3D, `bConsumeInput`, `bTriggerWhenPaused`, `AccumulationBehavior` | 이동·상호작용·도구 | 템플릿 에셋 사용, Prototype 미사용 | 소스 `EI/Public/InputAction.h`; 문서 enhanced-input |
| `UInputMappingContext` (IMC) | EnhancedInput | 키→Action 배치 묶음, 우선순위, 입력 모드 쿼리 | 탐사/관전/UI 상황별 배치 | 템플릿 사용 | 소스 `InputMappingContext.h`; 문서 |
| Modifier (`UInputModifier*`) | EnhancedInput | 데드존·스칼라·부정·스위즐·응답 곡선·스무딩·월드 변환·`FOVScaling` | WASD→2D, 감도, 게임패드 데드존 | 템플릿 에셋 | 소스 `InputModifiers.h:16~412` (Smooth, DeadZone, Scalar, ScaleByDeltaTime, Negate, ResponseCurveExponential/User, FOVScaling, ToWorldSpace, SwizzleAxis) |
| Trigger (`UInputTrigger*`) | EnhancedInput | Down/Pressed/Released/Hold/HoldAndRelease/Tap/RepeatedTap/Pulse/ChordAction/ChordBlocker | 길게 누르기 상호작용(레버·구출) | 미사용 | 소스 `InputTriggers.h:262~521` |
| `UEnhancedInputComponent::BindAction` | EnhancedInput | `ETriggerEvent`(Started/Triggered/Completed 등)에 핸들러 연결, `BindActionValueLambda` | Pawn/Controller 입력 | 템플릿 사용 | 소스 `EnhancedInputComponent.h:373~` |
| `UEnhancedInputLocalPlayerSubsystem` | EnhancedInput (Runtime) | 로컬 플레이어별 컨텍스트 추가·제거, 입력 모드, 주입(`InjectInputForAction`), `GetUserSettings` | 컨텍스트 전환 | 템플릿 `DIGETHERPlayerController`가 사용 | 소스 `EnhancedInputSubsystems.h:21`, `EnhancedInputSubsystemInterface.h:114~336` |
| `UEnhancedInputWorldSubsystem` | EnhancedInput (Runtime, **Experimental 표기**) | 월드 단위 입력 처리(로컬 플레이어 없는 입력) | 해당 없음 | 미사용(`bEnableWorldSubsystem` 기본 false) | 소스 `EnhancedInputDeveloperSettings.h` (DisplayName "Enable World Subsystem (Experimental)"), `EnhancedInputDeveloperSettings.cpp:21` |
| 입력 모드 태그 필터링 | EnhancedInput | IMC의 `InputModeFilterOptions`·`InputModeQueryOverride`/`GetInputModeQuery()`와 현재 입력 모드 태그로 매핑 필터 | 상황별(탐사/관전/UI) 분리 | C++·Config의 별도 설정 없음. 기본 필터 활성, BP별 쿼리 미확인 | 소스 `EI/Public/InputMappingContext.h`, `EI/Private/InputMappingContext.cpp`, `EI/Private/EnhancedInputSubsystemInterface.cpp`, `Engine/Config/BaseInput.ini` |
| Player Mappable Key (`UPlayerMappableKeySettings`) | EnhancedInput | Action/매핑에 이름·표시명·카테고리·지원 키 프로필 부여 | 리바인딩 UI 항목 | 미사용 | 소스 `PlayerMappableKeySettings.h:17~72` |
| `UEnhancedInputUserSettings` | EnhancedInput (Runtime, 비Beta) | 사용자 키 변경 저장(SaveGame 슬롯 `EnhancedInputUserSettings`), 키 프로필, `MapPlayerKey`/`UnMapPlayerKey`/`ResetAllPlayerKeysInRow`, `SaveSettings`/`AsyncSaveSettings`, `RegisterInputMappingContext` | 키 재설정(S30~S33) | 미사용 | 소스 `UserSettings/EnhancedInputUserSettings.h:53~915`, `EnhancedInputDeveloperSettings.h:77~95` |
| Developer Settings 기본 컨텍스트 | EnhancedInput | `DefaultMappingContexts`를 모든 로컬 플레이어에 자동 적용 | 공통 UI·디버그 컨텍스트 | 미사용 | 소스 `EnhancedInputDeveloperSettings.h:54` (제약: 게임 루트 콘텐츠만) |
| `UEnhancedInputPlatformSettings` | EnhancedInput | 플랫폼별 컨텍스트 리디렉트(문서), `InputDataClasses`는 5.7부터 deprecated | PC 단일이면 불필요 | 미사용 | 소스 `EnhancedInputPlatformSettings.h:51,77,88`; 문서 |
| `UPlayerMappableInputConfig` | EnhancedInput | 이전 방식 프리셋 | **쓰지 않음** | 미사용 | 소스 `PlayerMappableInputConfig.h:23` (5.3 deprecated) |
| `UInputTriggerCombo` | EnhancedInput | 콤보 트리거 | **쓰지 않음** | 미사용 | 소스 `InputTriggers.h:571` (5.8 deprecated) |
| 입력 모드(`SetInputMode`) | Engine (Runtime) | `FInputModeGameOnly`/`UIOnly`/`GameAndUI`로 포커스·커서·마우스 캡처 | 게임↔UI | **사용**(Prototype·Frontend) | 소스 `ENG/Classes/GameFramework/PlayerController.h:102~212,1649` |
| `UInputSettings` / `DefaultInput.ini` | Engine (Runtime) | 축 설정(감도·데드존), 마우스 캡처·잠금 기본값, 기본 입력 클래스 | 전역 입력 환경 | **사용** | 소스 `ENG/Classes/GameFramework/InputSettings.h:47~223`; 프로젝트 `DefaultInput.ini` |
| CommonInput (`UCommonInputSubsystem`) | CommonUI (Runtime, `IsBetaVersion false`) | 현재 입력 장치(마우스/게임패드/터치) 추적, 게임패드 종류, 장치 전환 이벤트 | 게임패드 아이콘·프롬프트 전환 | 모듈 의존+설정 사용 | 소스 `CI/CommonInput/Public/CommonInputSubsystem.h:25~73`, `CommonUI.uplugin` |
| CommonUI 입력 (`FUIInputConfig`, `ECommonInputMode`, `UCommonUIActionRouterBase`) | CommonUI (Runtime) | 활성 위젯이 Menu/Game/All 입력 모드·마우스 캡처 선언, 액션 바인딩, 홀드 데이터 | 메뉴·HUD 입력 라우팅 | **사용**(메뉴·로비) | 소스 `CI/CommonUI/Public/Input/UIActionBindingHandle.h:90~105`, `CommonInputModeTypes.h:11`, `CommonUIActionRouterBase.h` |
| 컨트롤러 아이콘 데이터 (`UCommonInputBaseControllerData`) | CommonUI | 장치·키별 아이콘/브러시 데이터 에셋, 플랫폼별 연결 | 게임패드 키 아이콘 | `BP_UIInputData`만 설정(내용 미확인) | 소스 `CommonInputBaseTypes.h:157,216`, `CommonInputSettings.h:82~85` |
| CommonInput Enhanced Input 연동 | CommonUI | CommonUI 액션을 Enhanced Input 액션으로 구동 | 키 재설정과 UI 프롬프트 일치 | **끔**(기본 false) | 소스 `CommonInputSettings.h:63,114` |
| GameInput | PLG/Runtime/GameInput (Runtime, Win64, `EnabledByDefault false`, 비Beta) | 차세대 Windows 입력 API 기반 장치 처리(범용 장치·햅틱) | 특수 장치 지원 필요 시 | 미사용 | 소스 `GameInput.uplugin` |
| GameInputWindows | PLG/GameInputWindows (RuntimeNoCommandlet, Win64, `EnabledByDefault false`, **Beta**) | GameInput의 Windows 백엔드 | 보류 | 미사용 | 소스 `GameInputWindows.uplugin` |
| `UInputDeviceSubsystem` | Engine (EngineSubsystem) | 장치 속성(진동·라이트바 등) 활성화, 장치 매핑 | 컨트롤러 진동 | 미사용 | 소스 `ENG/Classes/GameFramework/InputDeviceSubsystem.h:122` |
| `showdebug EnhancedInput` | EnhancedInput | HUD 오버레이에 활성 컨텍스트·액션 값·사용자 설정 출력 | 입력 디버깅 | 미사용(HUD 필요) | 소스 `EI/Private/EnhancedInputModule.cpp:332~` |
| `Input.+action` 등 콘솔 명령 | EnhancedInput | 액션·키 값을 매 프레임 강제 주입 | 입력 없는 자동 시험 | 미사용 | 소스 `EnhancedInputModule.cpp:253~` |
| InputDebugging | `Engine/Plugins/Runtime/InputDebugging` (Runtime+Editor, 비Beta, `EnabledByDefault:true`, Runtime 모듈 Shipping 제외) | `Input.ListAllHardwareDevices`·`Input.LogAllConnectedDevices` 등 장치 진단 | 장치 인식 문제 조사 | 기본 활성. 프로젝트 명시 설정 없음, 실제 로드·호출 미확인 | 소스 `Engine/Plugins/Runtime/InputDebugging/InputDebugging.uplugin`, `Engine/Plugins/Runtime/InputDebugging/Source/InputDebugging/Private/InputDeviceDebugTools.cpp:58,65` |
| PlayerInputDebugger | PLG/Experimental (Editor, **Beta/Experimental**) | 에디터에서 입력 스택 시각화 | 개발 보조 | 미사용 | 소스 `PlayerInputDebugger.uplugin` |
| CQTest EnhancedInput | `Engine/Plugins/Tests/CQTestEnhancedInput` (모듈 2개 모두 `DeveloperTool`, **Beta**, `EnabledByDefault` 키 없음) | Enhanced Input 자동 시험 유틸 | 입력 자동 시험 후보 | 프로젝트 descriptor·C++에서 사용 참조 찾지 못함. 실제 활성 상태 미확인 | 소스 `Engine/Plugins/Tests/CQTestEnhancedInput/CQTestEnhancedInput.uplugin` (`CQTestEnhancedInput`, `CQTestEnhancedInputTests`) |

## 기능별 상세

### Input Action·Mapping Context·Modifier·Trigger
- 핵심 클래스: `UInputAction`, `UInputMappingContext`, `UInputModifier`·`UInputTrigger`(헤더는 `EI/Public/`). 트리거 이벤트는 문서 기준 Started/Ongoing/Triggered/Completed/Canceled. 트리거 종류는 Explicit(하나 성공)·Implicit(모두 성공)·Blocker(실패 강제)로 분류된다(문서).
- 설정 위치: Content Browser의 `IA_`/`IMC_` 에셋([표준 기능](../STANDARD_FEATURES.md#에셋-이름)의 접두사). IMC 우선순위는 `AddMappingContext(IMC, Priority)`. 같은 키에 여러 컨텍스트가 매핑되면 높은 우선순위가 처리한다(문서). `bConsumeInput`(기본 true)으로 낮은 우선순위의 같은 키 매핑이 소비된다.
- 네트워크·리슨 서버: 입력은 **로컬 전용**이다. 서버는 입력 자체를 모른다. 게임 결과는 서버 RPC로 확정한다([네트워크 권위](../NETWORK_AUTHORITY.md)). 리슨 서버 호스트도 같은 로컬 입력 경로를 거친다. CMC가 입력 방향을 `AddMovementInput`으로 받아 서버로 전달하는 경로는 [GAMEPLAY_FRAMEWORK.md](GAMEPLAY_FRAMEWORK.md)의 이동 절.
- 제약·주의·deprecated: `UInputTriggerCombo`·`FInputComboStepData`·`FEnhancedInputKeys::ComboKey`는 5.8 deprecated(`EnhancedInputModule.h:20`, 키는 "Combo Key (Deprecated)"). 홀드와 탭을 한 Action에 섞으면 트리거 순서·소비 때문에 의도와 다르게 동작할 수 있으므로 별도 Action으로 나누는 것이 일반적이다(`미확인`: 이번에 동작 시험은 하지 않음). `Hold`는 Started 후 임계 시간에 Triggered, `HoldAndRelease`는 놓을 때 Triggered(`InputTriggers.h:318,347`).
- 대안·함께 쓰는 기능: 구식 `BindKey`/`BindAxis`/`ActionMappings`. `PlayerController`의 `GetDeprecatedInput*Scale`은 5.0에서 deprecated이며 Scalar Modifier로 대체하라고 헤더가 안내한다(`PlayerController.h:496~521`).

### 로컬 플레이어 서브시스템과 컨텍스트 전환
- 핵심: `UEnhancedInputLocalPlayerSubsystem`(`ULocalPlayer::GetSubsystem`), `AddMappingContext`/`RemoveMappingContext`/`HasMappingContext`, `FModifyContextOptions`(`bIgnoreAllPressedKeysUntilRelease`·`bForceImmediately`·`bNotifyUserSettings`), `RequestRebuildControlMappings`, `InjectInputForAction`.
- 설정: 코드로 컨텍스트 추가는 보통 `PlayerController::BeginPlay`(로컬 컨트롤러만) 또는 `OnPossess` 시점. 프로젝트 템플릿은 `DIGETHERPlayerController::SetupInputComponent`에서 추가한다(`DIGETHERPlayerController.cpp:44~56`). Developer Settings의 `DefaultMappingContexts`(Project Settings > Enhanced Input)는 코드 없이 전역 적용하나 게임 루트 콘텐츠만 가능하다.
- 네트워크: 서브시스템은 `ULocalPlayerSubsystem`이므로 로컬 플레이어가 없는 전용 서버·원격 플레이어의 서버 컨트롤러에는 해당 인스턴스가 없다. 리슨 서버 호스트의 로컬 플레이어에는 존재할 수 있다. `IsLocalPlayerController()`와 `GetLocalPlayer()`를 확인 후 접근한다. PlayerController 교체 시(`PlayerControllerChanged`) 컨텍스트 재적용은 미검증이다.
- 제약: 컨텍스트 전환 시 `bIgnoreAllPressedKeysUntilRelease` 기본값이 true라 전환 직전 눌린 키는 놓을 때까지 무시된다. 관전→복귀처럼 키를 누른 채 전환하는 흐름에서 의도를 확인한다.

### 입력 모드 태그 필터링 (5.8)
- 핵심: `UInputMappingContext::InputModeFilterOptions`(기본 `UseProjectDefaultQuery`), 커스텀 쿼리 `InputModeQueryOverride`(`FGameplayTagQuery`), 조회 `GetInputModeQuery()`; `IEnhancedInputSubsystemInterface::SetInputMode/AppendTagsToInputMode/AddTagToInputMode/RemoveTagFromInputMode`. Developer Settings의 `bEnableInputModeFiltering`은 기본 true이며 `DefaultMappingContextInputModeQuery`, `DefaultInputMode`가 있다.
- 쓸모: "탐사 중" "관전" "UI 열림" 같은 상태를 태그로 표현하고 컨텍스트가 그 상태에서만 활성이 되게 한다. 프로젝트는 이미 GameplayTags 의존이 있다. 컨텍스트 우선순위만으로도 같은 효과를 낼 수 있으므로 필요성은 별도 판단이 필요하다.
- 제약: 소스 `EI/Private/EnhancedInputSubsystemInterface.cpp`의 `RebuildControlMappings`는 필터가 활성이고 쿼리가 일치하지 않으면 해당 컨텍스트를 건너뛴다. `Engine/Source/Runtime/GameplayTags/Private/GameplayTagContainer.cpp`의 `FGameplayTagQuery::Matches`는 빈 쿼리에 false를 반환한다. 엔진 `Engine/Config/BaseInput.ini`는 기본 쿼리와 입력 모드에 `EnhancedInput.Modes.Default`를 지정한다. 프로젝트 Config 재정의는 찾지 못했으며 BP별 값·게임 중 전환 동작은 미검증이다.

### 사용자 설정과 리바인딩 저장
- 핵심: `UEnhancedInputUserSettings : USaveGame`, `UEnhancedPlayerMappableKeyProfile`(키 프로필), `FMapPlayerKeyArgs`, `MapPlayerKey`/`UnMapPlayerKey`/`ResetAllPlayerKeysInRow`/`ResetKeyProfileToDefault`, `ApplySettings`/`SaveSettings`/`AsyncSaveSettings`, `RegisterInputMappingContext(s)`, 이벤트 `OnSettingsChanged`/`OnKeyMappingUpdated`.
- 설정: Project Settings > Enhanced Input > User Settings(`bEnableUserSettings` 기본 true, `UserSettingsClass`, `DefaultPlayerMappableKeyProfileClass`). 대상 Action 매핑에는 `UPlayerMappableKeySettings`(Name·DisplayName·DisplayCategory·SupportedKeyProfileIds)를 설정해 "재설정 가능"으로 표시한다. 저장은 SaveGame 슬롯 `EnhancedInputUserSettings`(로컬 플레이어별, `LoadOrCreateSettings(ULocalPlayer*)`).
- 네트워크: 로컬 설정이며 복제하지 않는다. 호스트의 키 설정이 다른 플레이어에 영향을 주지 않는다.
- 제약·deprecated: 5.6에 프로필 ID 관련 API가 `FGameplayTag` 기반에서 `FString` 기반(`ProfileIdString`, `GetActiveKeyProfileId` 등)으로 바뀌고 이전 API가 deprecated이다(`EnhancedInputUserSettings.h:86,327,438,481,673,706,722~776,858~893`). 5.7에 `OnKeyMappingRegistered`가 `OnKeyMappingRegisteredToProfile`로 교체됐다(`:604`). 예전 튜토리얼 코드(태그 프로필 ID, `UPlayerMappableInputConfig`)를 그대로 쓰지 않는다. 컨텍스트를 `RegisterInputMappingContext`로 등록해야 설정 UI와 저장 대상이 된다. 프로젝트는 현재 `UDigetherGameUserSettings`(`GameUserSettings.ini`)에 감도 등을 저장하므로 키 매핑을 별도 슬롯에 둘지 같은 저장소로 모을지는 [DATA_SAVE.md](DATA_SAVE.md)와 함께 결정 대상이다.
- 대안: 직접 JSON 저장은 Enhanced Input이 이미 해결한 충돌 검사·프로필·초기화를 다시 구현한다.

### 입력 모드(Game/UI)와 마우스 캡처
- 핵심: `APlayerController::SetInputMode(FInputModeGameOnly | FInputModeUIOnly | FInputModeGameAndUI)`, `SetShowMouseCursor`, `SetIgnoreMoveInput/LookInput`, `FlushPressedKeys`. 전역 기본값은 `DefaultInput.ini`의 `DefaultViewportMouseCaptureMode`·`DefaultViewportMouseLockMode`(프로젝트: `CapturePermanently_IncludingInitialMouseDown`, `LockOnCapture`).
- 제약·주의: UI가 열렸을 때 게임 입력 컨텍스트를 제거할지, 입력 모드만 바꿀지 정해야 한다. `bShouldFlushPressedKeysOnViewportFocusLost`(프로젝트 true)는 포커스를 잃으면 눌린 키를 비운다. Prototype은 이를 이용해 `FlushPressedKeys`를 오버라이드해 로컬 입력을 리셋한다(`DigetherPrototypePlayerController.cpp:76`). Enhanced Input에서는 `bSendTriggeredEventsWhenInputIsFlushed`로 플러시 때 Cancelled/Triggered 이벤트를 보낼지 정한다(Developer Settings).
- CommonUI를 쓰면 위젯 활성화가 입력 모드를 결정한다(`GetDesiredInputConfig`가 `FUIInputConfig` 반환). 직접 `SetInputMode`와 섞으면 마지막 호출이 이긴다.

### CommonInput·CommonUI와 게임패드 아이콘
- 핵심: `UCommonInputSubsystem`(`GetCurrentInputType`·`OnInputMethodChangedNative`·`GetCurrentGamepadName`·`SetGamepadInputType`·`ShouldShowInputKeys`), `UCommonInputSettings`(`InputData`, `bEnableEnhancedInputSupport`, 입력 방식 스래싱 방지 `bEnableInputMethodThrashingProtection`/`InputMethodThrashingLimit`=30, `bAllowOutOfFocusDeviceInput`, `bEnableAutomaticGamepadTypeDetection`), `UCommonUIInputData`(클릭/뒤로 액션), `UCommonUIHoldData`, `UCommonInputBaseControllerData`(장치별 아이콘), `UCommonActionWidget`, `UCommonBoundActionBar`.
- 설정: Project Settings > Common Input Settings(`DefaultGame.ini [/Script/CommonInput.CommonInputSettings]`). 컨트롤러 아이콘 데이터는 플랫폼 설정(`FPerPlatformSettings PlatformInput`)에 연결한다. Enhanced Input 연동은 `bEnableEnhancedInputSupport`(재시작 필요, 기본 false)를 켜고 액티베이터블 위젯의 Enhanced Input 액션 필드를 쓴다(`CommonActivatableWidget.h:230,234` 조건부 노출).
- 네트워크: 로컬 UI 입력이며 복제하지 않는다.
- 제약: CommonUI 문서의 Enhanced Input 연동 페이지는 본문이 추출되지 않아 사용법은 소스 헤더 범위에서만 확인했다. 프로젝트의 `BP_UIInputData` 내용(Back/Click 액션, 아이콘 데이터)은 uasset이라 열어 보지 않았다. 현재 프로젝트는 메뉴가 CommonUI로 입력을 받고 게임 본편 입력은 Enhanced Input을 쓰는 이원 구조이므로, 키 재설정 UI와 CommonUI 키 프롬프트(예: "E 상호작용")를 어떻게 일치시킬지 결정 대상이다.
- 대안: Enhanced Input만으로 아이콘을 직접 조회(`IEnhancedInputSubsystemInterface`의 키 조회 + 자체 아이콘 테이블). 구현 비용이 크다.

### GameInput 플러그인
- 구성: `GameInput`(`Runtime/GameInput`, 모듈 `GameInputBase`, Win64, 비Beta, `EnabledByDefault false`)과 `GameInputWindows`(Beta, `GameInput` 의존, `GameInputWindowsLibrary` 모듈 의존). 장치 식별·데드존·원시 리포트 번역 설정(`GameInputDeveloperSettings.h`), 햅틱 엔드포인트 설정이 있다.
- 쓸모: 레거시 XInput/RawInput으로 처리하기 어려운 범용 장치 후보. DIGETHER의 지원 입력 장치는 D20에서 미결이므로([결정 목록](../../planning/DECISIONS.md)) 필요성도 아직 확정하지 않는다.
- 주의: Beta 표시, 활성 시 기본 입력 경로가 바뀌는지, Steam Input·Steam 오버레이와의 상호작용은 이번에 확인하지 않았다(`미확인`). 엔진 설치에서 `SteamInput` 이름의 플러그인·모듈은 찾지 못했다(검색 실패 ≠ 미지원).

### 입력 디버깅
- `showdebug EnhancedInput`: `AHUD::OnShowDebugInfo`에 등록돼 활성 컨텍스트·액션 상태·사용자 설정을 표시한다. 같은 모듈이 `WorldSubsystemInput`·`InputSettings` 디버그 이름도 다룬다(`EnhancedInputModule.cpp:332~403`). 표시에는 `AHUD`가 필요하므로 HUD 클래스가 없는 맵(`DefaultEngine.ini`에 HUD 지정 없음; Prototype만 `ADigetherPrototypeHUD`)에서는 기본 `AHUD`로 동작하는지 PIE로 확인해야 한다(`미확인`).
- 콘솔: `EnhancedInput.RebuildControlMappings`(`EnhancedInputLibrary.cpp:30`), `Input.+action`/`Input.-action`/`Input.+key`/`Input.-key`(액션·키 값 강제), `EnhancedInput.Debug.ShowKeyboardKeys`·`ShowGamepadKeys`·`ShowTouchKeys`(디버그 표시 CVar), `EnhancedInput.EnableDefaultMappingContexts`·`EnhancedInput.OnlyTriggerLastActionInChord`(Developer Settings 연동 CVar).
- `APlayerController::Debug_GetMostRecentInputStack()`(`PlayerController.h:1691`)로 입력 컴포넌트 스택을 읽을 수 있다. 장치 진단은 `InputDebugging` 플러그인 명령(`Input.ListAllHardwareDevices` 등). 에디터 시각화는 `PlayerInputDebugger`(Beta/Experimental 표기).
- 시험: `InjectInputForAction`·`StartContinuousInputInjectionForAction`으로 입력 없는 시험을 만들 수 있다(`EnhancedInputSubsystemInterface.h:153~211`).

### 접근성: 홀드/토글
- **확인한 `EI/Public/InputTriggers.h`에는 토글 전용 표준 트리거가 없다.** Down/Pressed/Released/Hold/HoldAndRelease/Tap/RepeatedTap/Pulse/ChordAction/ChordBlocker와 deprecated Combo를 확인했다. 다른 플러그인·사용자 설정의 지원 부재를 뜻하지 않는다. 홀드↔토글 선택(예: 웅크리기·조준·레버)은 Pressed 이벤트로 상태를 전환하거나 사용자 설정 값에 따라 Hold/Pressed 경로를 선택하는 구현 후보가 있다. 실제 항목·동작은 미결이다.
- 접근성 후보: 홀드 시간 조정(Hold 트리거 `HoldTimeThreshold`는 에셋 값이라 런타임 사용자 설정과 연결하려면 코드 필요), `UInputModifierScalar` 감도, 데드존, 키 재설정(위 사용자 설정). CommonUI는 `UCommonUIHoldData`로 UI 홀드를 표현한다.
- 프로젝트의 접근성 범위는 S30~S33에 "제공"만 적혀 있고 구체 항목은 결정되지 않았다. 항목은 기획이 정한다.

## 보류·비추천 후보와 이유
- **`BindKey`/`BindAxisKey`/`ActionMappings` 직접 사용**: 프로젝트 [표준 기능](../STANDARD_FEATURES.md)이 피하라 했고 키 재설정이 불가하다. Prototype 코드는 정식 Pawn에서 교체 대상.
- **`UPlayerMappableInputConfig`**: 5.3 deprecated. `UEnhancedInputUserSettings` 사용.
- **`UInputTriggerCombo`**: 5.8 deprecated.
- **Enhanced Input World Subsystem**: Experimental 표기. 로컬 플레이어 없는 입력이 필요하지 않다.
- **GameInputWindows**: Beta. 필요가 확인될 때까지 보류.
- **PlayerInputDebugger**: Beta/Experimental, 에디터 전용 도구.
- **`PlatformSettings.InputDataClasses`(Enhanced Input)**: 5.7 deprecated.

## 확인 한계
- 문서 근거: `https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-unreal-engine`(5.8)을 열어 개요·안정성 표기를 확인했다. 키 재설정 상세는 문서가 간략히만 다뤄 소스 헤더 근거를 썼다. CommonUI·Enhanced Input 연동 페이지(`using-commonui-with-enhanced-input-in-unreal-engine`)는 본문 추출 실패.
- 동작 검증은 하지 않았다. PIE·멀티플레이·게임패드 시험 없음. 지원 사실은 프로젝트 검증이 아니다.
- `Content/Input`의 uasset(`IA_*`/`IMC_*`/`BP_UIInputData`)은 바이너리라 내용(트리거·모디파이어 구성)을 열지 않았다. 이름과 코드에서의 사용만 확인했다.
- 키 재설정 UI 구성, CommonUI 키 프롬프트와 Enhanced Input 액션의 연결 방식, Steam Input·Steam 오버레이 입력, 게임패드 지원 범위는 기획·후속 조사 대상이며 이번에 확인하지 않았다.
- `showdebug EnhancedInput`의 HUD 없는 맵 동작, 입력 모드 필터링의 프로젝트 BP 설정·런타임 전환은 `미확인`. 엔진 기본 쿼리와 빈 쿼리의 소스 분기는 위에서 확인했다.
