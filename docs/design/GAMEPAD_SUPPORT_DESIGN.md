# 게임패드 지원 설계 — 2026-11-07

상태: **설계 초안, 채택·구현 결정 아님.** 기준 UE 5.8.4 (CL 58902052), Windows·Steam, 리슨 서버 협동. 엔진 기능 근거는 [입력 카탈로그](../unreal/features/INPUT.md)·[UI 카탈로그](../unreal/features/UI.md)를 보완한다.

## 1. 결론

- **현재 기획 결정 D20은 "MVP 입력은 키보드·마우스만"이다**([DECISIONS](../planning/DECISIONS.md)). 이 설계는 D20을 바꾸지 않는다. MVP에서는 패드를 막지 않는 구조(단계 G0)만 준비하고, 패드 지원(G1·G2)은 D20 개정 후 진행한다.
- 엔진만으로 Windows에서 보장되는 패드는 **XInput(Xbox 계열)** 뿐이다. PS·스위치 패드는 **Steam Input의 Xbox 에뮬레이션**에 맡기는 것을 권장한다.
- 엔진에 없어서 프로젝트가 만들어야 하는 것: 패드 종류 판별(Windows XInput은 종류를 알려 주지 않음), Xbox·PS 버튼 아이콘, 패드 감도·진동 옵션, 패드 해제 처리, 조준 보조, Steam Deck 온스크린 키보드.
- 현재 코드에서 패드를 **의도적으로 막는 곳은 키 재설정 3곳**이고, 그 밖에 키 이름이 박힌 안내 문구·마우스 전용 UI가 있다. 게임 본편 패드 조작은 **정식 캐릭터(Pawn)가 아직 없어서** 기준 코드가 없다.

## 2. 지원 단계 제안

| 단계 | 범위 | D20과의 관계 | Steam 표시 기준(참고) |
|---|---|---|---|
| **G0 패드 대비** | 패드 키를 노출하지 않되, 입력·안내 문구·재설정 구조가 장치를 구분할 수 있게 만든다 | D20 안에서 가능 | 해당 없음 |
| **G1 부분 지원** | Xbox(XInput)·Steam Input 경유 패드로 게임 본편과 주요 메뉴 조작. 패드 키 재설정, 감도·진동 옵션, Xbox 아이콘 | D20 개정 필요 | Partial Controller Support 후보 |
| **G2 전체 지원** | 모든 화면을 패드만으로 조작, 패드 종류별 아이콘, 텍스트 입력은 온스크린 키보드, Steam Deck 검증 대응 | D20 개정 + 출시 목표 결정 | Full Controller Support·Deck Verified 후보 |

Steam의 Full/Partial 공식 정의 원문은 확인하지 못했다(`미확인`). Steam Deck 호환 문서는 기본 컨트롤러 구성만으로 모든 콘텐츠 접근, 사용 중인 입력과 맞는 글리프, 텍스트 입력 시 온스크린 키보드 표시를 요구한다(https://partner.steamgames.com/doc/steamdeck/compat).

**권장:** 지금은 G0만 진행하고, G1 착수 여부는 기획 답변을 받은 뒤 정한다. G0 작업 대부분은 키보드 재설정에도 바로 쓸모가 있다(아래 4.3의 안내 문구 문제는 지금도 키를 바꾸면 틀린 키를 보여 주는 결함이다).

## 3. 현재 상태와 막히는 지점

경로는 `DIGETHER/Source/DIGETHER/` 기준이다. 바이너리 에셋(IA·IMC·`DT_UIActions`)의 키 지정은 열지 않았다.

| 영역 | 현재 | 패드 지원 시 문제 |
|---|---|---|
| 허브 Pawn | `ADigetherTestPawn`(`ADefaultPawn`, 임시). 엔진 legacy 축 매핑으로 패드 이동·시점이 들어올 가능성이 있음(추정) | Enhanced Input·재설정·감도를 거치지 않음. 정식 캐릭터 결정 전에는 패드 조작 기준이 없음 |
| 프로토타입 캐릭터 | `Prototype/Player/DigetherPrototypeCharacter.cpp:476-519` `BindKey`/`BindAxisKey`로 WASD·E·F·1~5 등 직접 지정, 키 비교 로직 포함 | 패드 대상 아님. 정식 캐릭터에서 Enhanced Input으로 새로 만든다 |
| 허브 컨트롤러 | `Game/Framework/DigetherGameplayPlayerController.cpp:47-156` Enhanced Input(Interact·Pour·Pause) | 코드 변경 없이 IMC에 패드 키만 추가 가능 |
| 음성 | `Game/Voice/DigetherVoiceComponent.cpp:178-222` `IMC_Voice`, 에셋 없을 때 V 대체 | IMC에 패드 키 추가. 대체 경로는 키보드 전용 |
| 키 재설정 필터 | `Game/Settings/DigetherKeyBindings.cpp:45` `IsBindableKey`가 `IsGamepadKey()`와 모든 축(Axis1D/2D/3D) 제외 | ① 행 구성(`:12-22`) ② 입력 캡처(`Game/UI/Settings/DigetherSettingsPanelWidget.cpp:449,463`) ③ 저장(`DigetherKeyBindings.cpp:118`) 세 곳이 같은 필터를 씀 |
| 재설정 교환 | `AssignWithSwap`(`DigetherKeyBindings.cpp:25-41`)이 행당 키 하나 | 장치(슬롯) 개념 없음 |
| 재설정 대기 | `NativeOnPreviewKeyDown`이 모든 키를 삼키고 Escape만 취소(`Game/UI/Settings/DigetherSettingsPanelWidget.cpp:438-454`). 캡처는 KeyDown·MouseButtonDown뿐 | 패드 사용자는 대기 상태에서 빠져나올 수 없음(코드 근거 판단). 스틱·트리거 축은 캡처 불가 |
| 조작 설정 | `UDigetherGameUserSettings` 마우스 감도·Y반전만. 적용처는 템플릿 `DIGETHERCharacter.cpp:120` 한 곳 | 패드 감도·데드존·진동 항목과 실제 적용 경로 없음 |
| 안내 문구 | `Game/Hub/DigetherHubInteractionComponent.cpp:135-175` "E · …", "우클릭 유지", "이동 WASD" 고정 | 패드뿐 아니라 키보드 재설정 후에도 틀린 키 표시 |
| UI 입력 | 메인 메뉴·대기실만 `GetDesiredInputConfig` 구현. 일시정지·시설·결과 화면은 직접 `SetInputMode` | 화면별로 입력 모드 처리 방식이 다름 |
| UI 위젯 | 시설 패널·결과 화면은 일반 `UButton`, 결과 화면은 뒤로 처리 없음, 시설 설명은 툴팁(마우스 전용), 크레딧 빨리감기는 클릭만 | 패드로 접근 불가한 기능이 생김 |
| 뒤로 키 | `bIsBackHandler` 사용, `bEnableEnhancedInputSupport` 꺼짐 → `DT_UIActions`가 결정 | 패드 B 지정 여부 미확인 |
| 아이콘 | 컨트롤러 데이터 에셋 없음. CommonUI 플러그인에도 Xbox/PS 아이콘 없음 | 아이콘 에셋·데이터를 새로 만들어야 함 |
| 시험 | `Game/Tests/DigetherUserOptionsTest.cpp:39`가 "패드 키 제외"를 고정 | 정책 변경 시 기대값을 함께 바꿔야 함 |

## 4. 설계

### 4.1 장치 백엔드

- **기본:** 엔진 기본 XInput(`XInputDevice` 플러그인, 기본 활성)만 쓴다. Steam 실행 시 Steam Input이 PS·스위치 등 비-XInput 패드를 Xbox 패드로 바꿔 넘긴다(https://partner.steamgames.com/doc/features/steam_controller/steam_input_gamepad_emulation_bestpractices).
- Steamworks 앱 설정의 Steam Input 옵트인(패드 종류별 체크·템플릿)은 App ID 확정 후 결정한다. 현재 App ID는 개발용 480이다.
- **GameInput 플러그인은 보류**한다. Windows 백엔드가 Beta이고, 켜도 XInput을 자동으로 끄지 않아 입력이 중복될 수 있다(엔진 `GameInputDeveloperSettings.h:371-377`).
- 엔진 `SteamController` 플러그인은 구식 Action/Axis 매핑 전용이고 글리프·종류 판별이 없어 쓰지 않는다.

### 4.2 게임 입력(Enhanced Input)

- 정식 캐릭터는 Enhanced Input만 쓴다(`BindKey`·`IsInputKeyDown` 금지, [표준 기능](../unreal/STANDARD_FEATURES.md)). 키 비교로 "어떤 키를 뗐는지" 판단하던 프로토타입 방식 대신 액션의 Started/Completed/Canceled를 쓴다.
- **IMC 구성(권장): 같은 IMC·같은 액션에 키보드·마우스 매핑과 패드 매핑을 함께 둔다.** 대안인 장치별 IMC(예: `IMC_Hub_KBM`·`IMC_Hub_Pad`)를 동시에 활성화하는 방식도 가능하지만 IMC 수와 우선순위 관리가 두 배가 된다. 활성 IMC 전체의 기본 매핑은 함께 적용되므로 어느 방식이든 두 장치를 동시에 쓸 수 있다(엔진 `InputMappingContext.cpp:113`, `EnhancedInputSubsystemInterface.cpp:964`).
- 단, **장치별로 사용자 키 프로필을 나누는 방식은 쓰지 않는다.** 재구성은 활성 프로필 하나만 적용하므로 "키보드 프로필 + 패드 프로필"을 동시에 합성할 수 없다(엔진 `EnhancedInputSubsystemInterface.cpp:957,988,998`).
- **패드 매핑에는 DeadZone 모디파이어를 명시한다.** Enhanced Input 기본 경로는 `DefaultInput.ini`의 legacy AxisConfig(패드 데드존 0.25 포함)를 패드 키에는 가져오지 않는다(엔진 `EnhancedInputSubsystemInterface.cpp:629-634`, CVar `input.GlobalAxisConfigMode`로 변경 가능).
- 시점(Look) 패드 매핑 구성: Radial DeadZone → 응답 커브 → 패드 감도 → `ScaleByDeltaTime`. 마우스 매핑은 마우스 감도만 적용한다. 매핑별 모디파이어는 액션 모디파이어보다 먼저 적용된다(엔진 `EnhancedActionKeyMapping.h:86-90`).
- 감도는 런타임 사용자 값이다. 원본 에셋의 Scalar 값을 고쳐도 사용자 설정과 자동으로 동기화되지 않으므로(재구성 때 모디파이어 복제, 엔진 `EnhancedInputSubsystemInterface.cpp:1061`) **설정값을 읽는 커스텀 모디파이어**(예: `UDigetherLookSensitivityModifier`, 마우스/패드 구분 필드)를 둔다. 모디파이어는 `PlayerInput → PlayerController → LocalPlayer`로 설정에 접근할 수 있다(엔진 `InputModifiers.cpp:391`의 FOVScaling이 같은 경로). 대안은 Pawn의 `DoLook`에서 곱하는 현재 템플릿 방식이며, 이 경우 Pawn이 입력 장치를 알아야 한다.
- 길게 누르기(레버·구출 등)는 Hold 트리거를 쓰고, 탭과 홀드를 한 액션에 섞지 않는다.

### 4.3 키 재설정의 장치 분리

엔진 기본은 장치를 구분하지 않는다. `DetermineHardwareDeviceForActionMapping` 기본 구현이 Invalid를 반환하고, 슬롯 번호는 **장치 ID와 MappingName 단위**로 매겨지므로 같은 MappingName을 공유하는 키보드·패드 매핑은 IMC 나열 순서대로 First·Second 슬롯을 받는다(엔진 `EnhancedInputUserSettings.cpp:1685-1720,1798-1801`).

| 안 | 방식 | 장점 | 단점 |
|---|---|---|---|
| **A (권장)** | `UEnhancedInputUserSettings` 하위 클래스에서 `DetermineHardwareDeviceForActionMapping`을 오버라이드해 기본 키 종류로 KBM/Gamepad 장치를 지정. UI는 `FPlayerMappableKeyQueryOptions.RequiredDeviceType`으로 거름 | 엔진 헤더가 권하는 확장점(`.h:864-871`). IMC 순서에 의존하지 않음 | 하위 클래스·`UserSettingsClass` 설정 추가. **기존 저장의 이전 절차 필요(아래)** |
| B | IMC에서 키보드 매핑을 항상 먼저 나열하는 규칙 + `KeyToMatch`(해당 장치의 아무 키)·`bMatchBasicKeyTypes=true`로 거름(`KeyToMatch`가 없으면 종류 필터가 동작하지 않음, 엔진 `.cpp:528`) | 코드 적음, 저장 이전 불필요 | 에셋 순서 규칙이 깨지면 슬롯이 뒤섞임. 검사 도구가 따로 필요 |

**A안의 기존 저장 이전(필수).** 클래스를 바꾸면 기존 프로필이 복사되고(엔진 `.cpp:796`), 재등록 때 장치 종류가 다른 기존 매핑(Invalid)은 갱신되지 않고 새 장치의 기본 매핑이 따로 추가된다(`.cpp:1740-1760`). 그대로 두면 기존 사용자 키는 장치 필터 UI에서 숨겨지고, 실제 입력에는 사용자 키와 새 기본 키가 함께 남을 수 있다. 따라서:
- 재등록 전에 Invalid 장치 매핑을 기본 키 종류로 판정해 KBM/Gamepad 장치·슬롯으로 옮기는 이전 단계를 둔다(설정 버전 번호로 1회 실행).
- 확인: 키를 바꿔 저장한 기존 슬롯을 새 빌드로 로드 → 사용자 키 유지, 새 기본 키 중복 활성 없음, 초기화(`ResetAllPlayerKeysInRow`)·저장·재실행 후 동일.
- 지금은 출시 전이라 이전 대신 저장 슬롯 초기화도 가능하다. 팀 내부 사용자 설정이 사라지는 비용과 비교해 구현 때 고른다.

공통 변경:
- `IsBindableKey`를 장치별 판정(`IsBindableKeyForDevice(Key, Device)`)으로 나누고, 행 구성·입력 캡처·저장 3곳이 같은 함수를 쓴다. G0에서는 Gamepad 장치를 "허용 안 함"으로 두어 D20을 지킨다.
- 행 데이터를 `행 = 액션, 칸 = 장치`로 바꾼다. 교환(`AssignWithSwap`)은 같은 장치 칸 안에서만 한다.
- 실제 입력 반영은 기본 키와 같은 종류의 키만 적용되므로(엔진 `.cpp:479-504`) 키보드 칸에 패드 키가 들어가지 않게 UI에서 막는다.
- 재설정 대기 중 패드 취소: 대기 상태에서 `Gamepad_FaceButton_Right`(또는 지정 Back 키)를 취소로 처리하고, 패드 칸 대기 중에는 Escape도 취소로 둔다. 확인·뒤로 버튼 자체를 재설정 대상에 넣을지는 결정 대상이다(넣으면 메뉴 조작 불능 위험).
- **확인(A) 버튼 캡처 경로.** CommonUI 아날로그 커서는 위젯 Preview보다 먼저 패드 A(Virtual Accept)를 UI 액션으로 처리하거나 좌클릭으로 합성한다(엔진 `CommonAnalogCursor.cpp:397-410`, `Slate/Private/Framework/Application/AnalogCursor.cpp:54`). 그래서 패드 칸 대기 중 A를 누르면 현재 필터에서는 좌클릭이 저장될 수 있고, 패드 전용 필터에서는 거부된 채 대기가 계속될 수 있다. 자유 커서를 끄는 것만으로는 해결되지 않는다. 대기 상태 동안 CommonUI보다 앞에서 원키를 받는 입력 전처리기(`IInputProcessor`, 대기 시작~종료 동안만 등록)를 두거나 Accept 합성을 일시 차단한다. A 재설정 허용 여부(GP-4)와 무관하게 "A 입력 시 좌클릭 저장 안 됨" 시험을 넣는다.
- **축 재설정 범위.** G1 패드 재설정은 **디지털 버튼으로 한정**하는 것을 권장한다. 스틱·트리거 축을 바꾸게 하려면 축 선택 UI, 아날로그 입력 캡처, 축 모디파이어(데드존·Swizzle·Negate) 보존 규칙이 따로 필요하다. 현재 IA·IMC의 축 구성은 `미확인`이다.
- 시험 `DigetherUserOptionsTest.cpp:39`는 "G0: 패드 장치 거부"로 바꾸고, 장치별 교환·필터 시험을 추가한다.

### 4.4 현재 입력 장치와 안내 문구·아이콘

- 현재 장치는 `UCommonInputSubsystem`(`GetCurrentInputType`, `OnInputMethodChangedNative`)으로 판단한다. 아날로그 축만 움직일 때는 전환하지 않고 스틱이 디지털 키로 바뀌는 기울기(약 0.24)에서 전환된다. 스래싱 방지는 **전환 간격이 3초 미만인 연속 전환**을 세어 한도(30)를 넘으면 키보드·마우스로 고정하고, 쿨다운(1초) 이후 들어오는 입력에서 해제를 다시 판단한다(엔진 `CommonInputSubsystem.cpp:193,242-277,354-364`).
- **안내 문구는 액션 기준으로 만든다.** "E · 줍기" 대신 `액션 + 현재 장치 → 키 이름/아이콘`을 조회하는 공용 함수(예: `UDigetherInputPromptLibrary`)를 두고, 장치 전환·키 재설정 시 다시 그린다. 이 함수는 Enhanced Input에 실제 적용된 키(`QueryKeysMappedToAction`)를 직접 읽는다.
- `CommonActionWidget`이 재설정을 반영하려면 **CommonUI EI 연동(`bEnableEnhancedInputSupport`)이 켜져 있고 위젯에 `EnhancedInputAction`이 지정돼 있어야 한다.** 매핑 재구축 알림 구독도 이 조건일 때만 한다(엔진 `CommonActionWidget.cpp:122,423-450`). 현재처럼 꺼져 있으면 `DT_UIActions` 기반 아이콘을 쓴다. 따라서 G0(연동 꺼짐)에서는 공용 조회 함수로 문구를 그리고, `CommonActionWidget` 사용은 4.5에서 연동을 켠 뒤로 둔다. 아이콘 표시에는 해당 키의 브러시가 컨트롤러 데이터에 등록돼 있어야 한다. 홀드 키 동적 해석은 CVar `CommonUI.bEnhancedInputDynamicallyResolveHoldKeysForInputAction`(기본 false)이 필요하다.
- **아이콘 에셋**: `UCommonInputBaseControllerData` 하위 데이터(키보드·Xbox·PS)와 `CommonInputPlatformSettings` 연결을 새로 만든다. 아이콘 그림은 아트 담당 확인이 필요하다.
- **패드 종류 판별** — Windows XInput은 모든 패드를 같은 식별자(`XInputInterface`/`XInputController`)로 보고한다(엔진 `Plugins/Runtime/Windows/XInputDevice/.../XInputInterface.cpp:117-118,148-152,238`).

| 안 | 방식 | 비용·위험 |
|---|---|---|
| a | Xbox 아이콘 하나만 사용 | 가장 쌈. PS 사용자에게 다른 버튼 모양 표시. Deck Verified 글리프 요건 미충족 가능 |
| **b (G1 권장)** | 설정에서 아이콘 종류 수동 선택(`SetGamepadInputType`) + 기본 Xbox | 적은 비용. Steam 문서가 허용하는 방식(글리프 선택 메뉴) |
| c (G2) | Steamworks `ISteamInput`으로 실제 패드 종류 판별 후 자동 선택 | Steam SDK 직접 호출 코드 필요. 엔진 OnlineSubsystemSteam에 연동 없음. Steam 미실행·PIE 대체 경로 필요 |

### 4.5 UI 패드 조작

- 확인 = `Gamepad_FaceButton_Bottom`, 뒤로 = `Gamepad_FaceButton_Right`(엔진 기본, 반전 설정 없음).
- 화면 입력 모드는 CommonUI `GetDesiredInputConfig`로 통일하고 직접 `SetInputMode` 호출(일시정지·시설·결과 화면)을 줄인다. 일시정지 메뉴를 CommonUI 스택으로 옮길지는 4.5 작업 때 판단한다.
- CommonUI Enhanced Input 연동(`bEnableEnhancedInputSupport`)

| 안 | 장점 | 단점 |
|---|---|---|
| **켬 (G1 권장)** | 키 지정·재설정·아이콘이 Enhanced Input 한 경로, 화면별 IMC를 위젯 활성 수명에 맞춰 추가·제거 | 엔진 재시작 필요, `BP_UIInputData`의 EI Click/Back 액션 지정, 기존 `DT_UIActions` 기반 동작 회귀 확인, 아래 콜백 계약 정리 |
| 끔 유지 | 현재 메뉴 동작 그대로 | `DT_UIActions`에 패드 키를 따로 관리, 게임 입력과 UI 키가 이원화 |

연동을 켜도 모든 UI 이벤트가 한 경로가 되지는 않는다. Generic UI 액션(확인·뒤로 등, `bIsGenericInputAction=true`)은 CommonUI 콜백을 실행하고 Enhanced Input 이벤트를 방송하지 않는다. 반대로 Generic이 아닌 액션은 Enhanced Input에 입력을 주입하면서 CommonUI 콜백을 건너뛴다(엔진 `CommonUITypes.h:233`, `UIActionRouterTypes.cpp:865`, `CommonActivatableWidget.cpp:36`). 켜기로 하면 다음을 함께 정한다.
- 확인·뒤로: Generic UI 액션으로 두고 CommonUI 콜백(`NativeOnHandleBackAction` 등)에서 처리.
- 게임 동작을 일으키는 화면 액션: Enhanced Input 이벤트 경로로 처리하고 화면 IMC 우선순위를 게임 IMC보다 높게.
- 화면 IMC의 활성 수명(위젯 활성화~비활성화)과 일시정지·시설 화면처럼 CommonUI 스택 밖에서 여는 화면의 처리.
- 현재 바이너리 에셋의 액션 설정은 `미확인`이므로 이는 현재 결함이 아니라 전환 시 정할 항목이다.

- 바꿀 화면 목록: 시설 패널·결과 화면의 `UButton` → `UDigetherButtonWidget`, 결과 화면 뒤로 처리, 시설 설명 툴팁 → 포커스 시 설명 영역 표시, 크레딧 빨리감기에 입력 액션 추가, 탭 이전/다음(LB/RB) 지정, 버튼 포커스 표시 스타일(CommonButton이 포커스 시 hover 처리하는지는 `미확인`).
- 설정 슬라이더: `USlider`는 확인으로 잠근 뒤 좌우로 `StepSize`만큼 움직인다. 감도(0.1~3)는 기본 0.01 간격이라 약 290단계이므로 항목별 단계를 정하거나 `UCommonRotator`(좌우 즉시 변경)로 통일한다. 콤보박스는 확인으로 열고 닫는다.
- 패드 자유 커서(`FCommonAnalogCursor` 가상 포인터)는 쓰지 않고 포커스 이동을 기본으로 한다.

### 4.6 조작 옵션 저장

`UDigetherGameUserSettings`(현재 `GameUserSettings.ini`)에 추가 후보: 패드 시점 감도(가로·세로), 패드 Y반전, 시점 데드존, 응답 커브 강도, 진동 켜기·세기, 아이콘 종류(4.4-b). 엔진 키 매핑 저장은 기존대로 `EnhancedInputUserSettings` SaveGame 슬롯이다. 항목·범위·기본값은 기획 결정이다.

### 4.7 진동

- `APlayerController::ClientPlayForceFeedback` + `UForceFeedbackEffect`를 쓴다. 이 RPC는 unreliable 클라이언트 RPC이며 로컬 효과만 낸다. 게임 결과와 무관하므로 진동 누락은 허용한다.
- XInput은 4채널을 강·약 모터 2개로 합치고, 가장 최근에 쓴 패드로만 보낸다. DualSense 트리거 저항·라이트바는 Windows 기본 경로에서 동작하지 않으므로 범위 밖이다(엔진 `XInputInterface.cpp:373-429,476-509`).
- 사용자 진동 옵션은 엔진에 없으므로 4.6 값으로 PC의 `bForceFeedbackEnabled`·`ForceFeedbackScale`을 로컬에서 설정한다.

### 4.8 패드 연결 해제

- `IPlatformInputDeviceMapper::GetOnInputDeviceConnectionChange()` 또는 `UGameInstance::OnInputDeviceConnectionChange`로 감지한다. `FCoreDelegates::OnControllerConnectionChange`는 5.8에 없다.
- 협동 온라인이므로 **월드를 멈추지 않고** 로컬 안내 화면만 띄운다. 이동 입력을 멈출지는 결정 대상이다. 엔진에 자동 처리는 없다.

### 4.9 텍스트 입력과 Steam Deck(G2)

- 방 이름·코드 등 텍스트 입력 칸이 있으면 패드 사용 시 Steam 온스크린 키보드(`ISteamUtils::ShowFloatingGamepadTextInput`/`ShowGamepadTextInput`)를 띄운다. 엔진 연동이 없어 Steamworks SDK(엔진에 헤더 포함) 직접 호출이 필요하다.
- 실제 텍스트 입력 칸 목록은 G2 착수 때 다시 센다.

### 4.10 네트워크

입력·장치 판별·아이콘·진동 설정은 모두 **로컬 전용**이며 복제하지 않는다. 패드 지원이 서버 권한·RPC 구조를 바꾸지 않는다. 리슨 서버 호스트도 같은 로컬 경로를 쓴다. 조준 보조를 넣으면 공정성 문제가 생기므로 별도 결정한다(엔진 기본 기능 없음, `TargetingSystem` 플러그인은 Beta·Experimental).

## 5. 작업 단계

| 단계 | 작업 | 선행 조건 | 확인 기준 |
|---|---|---|---|
| G0-1 | 상호작용·붓기·말하기 안내 문구를 액션 기준 조회로 교체(`DigetherHubInteractionComponent:135-170` 등). 이동·시점 안내("이동 WASD / 시점 마우스", `:175`)는 허브 Pawn이 legacy 축 입력(`ADefaultPawn`)이라 G0-4 이후 교체 | 없음(이동·시점은 G0-4) | 키보드 재설정 후 문구가 새 키를 표시. 자동 시험. **2026-11-07 구현**: `Game/Input/DigetherInputPrompt`(실제 적용 매핑 조회, G0은 키보드·마우스 키만), 허브 문구 8종, 키 설정 화면 표기 공용화. Game 시험 184개 성공, L_Hub 단독 실행에서 "E" 표시 확인. 재설정 후 실제 화면 갱신은 미확인 |
| G0-2 | `IsBindableKey` 장치별 분리, 행=액션·칸=장치 구조, 장치 내 교환, (A안이면) 기존 저장 이전. 패드 칸은 숨김 | 4.3 안 A/B 선택 | 기존 키보드 재설정 회귀 없음, 시험 39행 갱신, 키를 바꾼 기존 저장 슬롯 로드 후 사용자 키 유지·기본 키 중복 없음 |
| G0-3 | 감도 커스텀 모디파이어(마우스용)로 감도 적용 경로 일원화 | 정식 캐릭터 입력 구조 | 감도 변경이 실제 Pawn 시점에 반영 |
| G0-4 | 정식 캐릭터 입력을 Enhanced Input으로 작성 | 정식 캐릭터 결정 | `BindKey` 없음 |
| G1-1 | 모든 IMC에 패드 매핑(데드존 포함) 추가 | **D20 개정**, 기본 버튼 배치 결정 | Xbox 패드로 허브 전체 흐름 |
| G1-2 | 컨트롤러 데이터·Xbox 아이콘, 장치 전환 시 문구 갱신 | 아이콘 그림 | 장치 바꿀 때 즉시 전환 |
| G1-3 | UI 패드 조작 정리(4.5 목록), CommonUI EI 연동 결정 | 4.5 선택 | 패드만으로 메뉴·설정·일시정지·시설·결과 화면 |
| G1-4 | 패드 키 재설정 노출(디지털 버튼), 대기 중 원키 캡처 전처리기, 패드 옵션·진동 | 4.6 항목 결정, GP-4 | 재설정·저장·재시작 후 유지, A 입력이 좌클릭으로 저장되지 않음, B로 대기 취소 |
| G1-5 | 패드 해제 안내 | 4.8 결정 | 해제·재연결 반복 |
| G2-1 | Steam Input 판별(4.4-c), 온스크린 키보드, Steamworks Steam Input 설정 | App ID, 출시 목표 | Steam 실행본에서 PS 패드·Deck |

## 6. 검증 계획

- 현재: 문서 변경만 있으므로 L1 문서 검사.
- 단계 착수 시 [입력 카탈로그](../unreal/features/INPUT.md)·[UI 카탈로그](../unreal/features/UI.md)의 오래된 현황(컨텍스트 미등록·저장 분리 미결·"D20 미결" 표기 등)을 이 설계와 실제 구현 기준으로 갱신하는 것을 완료 조건에 넣는다.
- G0: 자동 시험(재설정 장치 분리·교환·필터, 문구 조회) + 키보드 회귀.
- G1 이상: 실제 패드가 필요하다. Xbox(XInput) 직접 연결, DualSense를 Steam Input 경유, 리슨 서버 호스트와 클라이언트 각각에서 패드 사용. 입력은 로컬이지만 상호작용 RPC 경로는 M2(2인 이상)로 확인한다. Steam Input·Deck은 Steam 실행본에서만 확인 가능하다(M3).
- 장치 없음·Steam 미실행을 성공으로 보고하지 않는다.

## 7. 결정 대기 목록

| ID | 질문 | 결정 주체 |
|---|---|---|
| GP-1 | 패드 지원 목표 단계(G1 부분 / G2 전체·Deck)와 시점. D20 개정 여부 | 기획·사용자 |
| GP-2 | 패드 기본 버튼 배치(상호작용·붓기·말하기·달리기·웅크리기·슬롯 전환 등) | 기획 |
| GP-3 | 패드 종류별 아이콘 범위(Xbox만 / 수동 선택 / 자동 판별) | 기획·사용자 |
| GP-4 | 4.3 재설정 장치 분리 A/B, 확인·뒤로 버튼 재설정 허용 여부 | 사용자 |
| GP-5 | CommonUI Enhanced Input 연동 켜기 | 사용자 |
| GP-6 | 패드 옵션 항목·범위·기본값(감도·데드존·커브·진동) | 기획 |
| GP-7 | 조준 보조 여부 | 기획 |
| GP-8 | 패드 해제 시 동작(안내만 / 이동 정지) | 기획 |
| GP-9 | Steamworks Steam Input 옵트인 구성 | 사용자(App ID 확정 후) |

## 8. 확인 한계

- 엔진 근거는 로컬 5.8.4 소스 읽기이며 실기 동작(패드 입력·진동·장치 전환)은 시험하지 않았다.
- IA·IMC·`DT_UIActions`·`BP_UIInputData`의 실제 키·모디파이어 구성은 바이너리라 열지 않았다.
- `ADigetherTestPawn`의 legacy 패드 축이 실제로 동작하는지는 추정이다.
- Steam Full/Partial 정의, Steamworks Steam Input 체크박스 라벨·기본값, GameInput에서 DualSense 분류, CommonButton 포커스 시각 처리는 `미확인`이다.
- A안 저장 이전(4.3)과 대기 중 원키 캡처(4.3)는 소스 읽기로 설계했으며 실제 동작은 구현 시 시험으로 확인해야 한다.

## 9. 검토 이력

- 2026-11-07 Codex(`gpt-6.1-sol`, reasoning effort `ultra`, 읽기 전용) 독립 검토. 높음 2(A안 기존 저장 이전 누락, 패드 A 캡처 경로 누락)·중간 5(프로필/IMC 일반화, CommonActionWidget 반영 조건, G0-1 선행조건, 축 재설정 범위, CommonUI EI 콜백 계약)·낮음 5(슬롯 단위·`KeyToMatch` 조건, 데드존·Scalar 표현, 스래싱 설명, 인용 경로·줄, 카탈로그 동기화) 지적을 모두 반영했다. 높음 2건의 엔진 근거(`CommonAnalogCursor.cpp:397-410`, `EnhancedInputUserSettings.cpp:1740-1760`, `EnhancedInputSubsystemInterface.cpp:629-634`)는 반영 전에 직접 확인했다.
