# UI 기능 카탈로그

기준일 2026-11-04 · 프로젝트 사용처 갱신 2026-11-06 · UE 5.8.4 (CL 58902052) · 상태: **조사 자료, 채택 결정 아님**. 색인은 [기능 카탈로그](README.md).

사용 규칙(CommonUI 스택, `BindWidget`, 배치는 `WBP_`, 코드로 위젯 트리를 그리지 않음)은 [표준 엔진 기능](../STANDARD_FEATURES.md#uiai디버깅-도구)이 원본이다. 여기서는 엔진이 무엇을 제공하는지만 적는다. 사용자 메모에 따라 **디자인 시안 전까지 기능과 동작이 우선**이며, 이 문서도 모양이 아니라 구조·동작 후보를 다룬다. 근거 표시는 `소스`(로컬 5.8.4 헤더·uplugin 직접 확인) · `문서`(Epic 문서 열람) · `미확인`이다.

## 요약

- **CommonUI**(Activatable Widget + Stack + 입력 라우팅)는 프로젝트가 이미 메뉴·로비에 쓰는 기준 경로다. 플러그인은 Beta 표시가 없다(`IsBetaVersion false`).
- **UMG ViewModel(MVVM) 플러그인은 Beta**이고 자체 기본값은 꺼짐이다. 다만 기본 활성 `Metasound`가 이를 활성 의존하므로 `.uproject` 미기재만으로 프로젝트 비활성이라고 할 수 없다. 실제 모듈 로드·HUD 바인딩 사용은 미검증이며 도입은 별도 결정.
- **인게임 HUD**는 AHUD Canvas(프로토타입)와 UMG 위젯 두 경로가 있다. 새 화면은 UMG(CommonUI) 쪽이 표준 기능 규칙과 맞는다.
- **월드 공간 위젯(`UWidgetComponent`)**은 전용 서버에서 위젯을 만들지 않는다(소스). 리슨 서버 호스트 화면에는 만들어지므로 로컬 전용 규칙이 필요하다.
- **로딩 화면**은 엔진 `MoviePlayer` 모듈이 기본 후보다. Lyra의 `CommonLoadingScreen`류는 이번에 확인한 엔진 플러그인 목록에서 찾지 못했다.
- **접근성**: 엔진에 UI 색각 보정/시뮬레이션 함수와 자막 설정 함수가 있다. 텍스트 크기 옵션은 엔진 기본 기능을 찾지 못했다(프로젝트 구현 영역).

## DIGETHER에서 쓰이는 곳

| 기능 | 연결 | 현재 프로젝트 상태 |
|---|---|---|
| 메인 메뉴·방 만들기/참가·대기실 | S01, 세션 UI | **사용**: `UDigetherFrontendLayoutWidget`(스택 포함), `UDigetherMainMenuScreenWidget`·`UDigetherLobbyScreenWidget`(Activatable), 패널 위젯, `UDigetherButtonWidget`(CommonButtonBase), `UDigetherTabListWidget` |
| 화면 클래스 경로 | 설정 | **사용**: `UDigetherFrontendSettings`(소프트 클래스 경로, `DefaultGame.ini`). `/Game/Frontend`는 항상 쿠크 |
| 설정 화면(S31) | GameUserSettings | **사용**: `UDigetherSettingsPanelWidget`(그래픽·사운드·조작 탭) |
| CommonUI 입력 데이터 | 게임패드·키 아이콘 | `BP_UIInputData` + `DT_UIActions`(`CommonInputSettings.InputData`). 컨트롤러 아이콘 데이터 에셋은 없음 |
| CommonUI 뷰포트 | 입력 라우팅 | `GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient` 설정됨 |
| 인게임 HUD(S25~S29 중 HUD·결과) | 정보 표시 | **사용**: `Game/`의 `UDigetherRunHudWidget`·`UDigetherRunResultWidget`·`UDigetherGameOverCreditsWidget`. 로컬 Controller의 `UDigetherPlayerScreenComponent`가 `AddToPlayerScreen`으로 표시. 프로토타입 Canvas HUD는 별도 유지 |
| 월드 공간 위젯 | 이름표·상호작용 표시 | `Variant_Combat` 템플릿의 `ACombatCharacter`·`ACombatEnemy`가 `UWidgetComponent` LifeBar를 생성. 정식 게임 사용은 미검증 |
| MVVM·Slate 직접 작성 | — | 미사용(`Slate` 모듈은 의존에 있음) |
| 로딩 화면·자막·색약 | 접근성/전환 | 미사용 |
| 시안 | — | 디자인 시안 없음. 위젯은 자리표시·스타일 에셋(`BP_ButtonStyle_*`, `BP_TextStyle_*`) 중심 |

## 기능 목록

이 문서의 `미사용`은 별도 근거가 없으면 `DIGETHER/Source`·`Config`의 사용처 미발견을 뜻하며, Blueprint·에셋 내부 사용과 런타임 모듈 로드는 미검증이다. `EnabledByDefault`와 의존 활성화는 실제 로드·화면 사용과 구분한다.

| 기능 | 모듈·플러그인 (타입, 안정성) | 하는 일 | DIGETHER 쓸모 | 프로젝트 상태 | 근거 |
|---|---|---|---|---|---|
| UUserWidget / WidgetTree / `BindWidget` | UMG (Runtime) | 위젯 블루프린트의 C++ 부모, 자식 위젯 필수 바인딩(`BindWidget`·`BindWidgetOptional`·`BindWidgetAnim`) | 모든 화면의 기본 | 사용 | 소스 `Engine/Source/Runtime/UMG/Public/Blueprint/UserWidget.h` |
| 위젯 애니메이션 | UMG | `UWidgetAnimation`, `PlayAnimation*`(핸들 `FWidgetAnimationHandle` 반환), 큐 재생 `QueuePlayAnimation*` | 화면 전환·강조 연출 | 미사용 | 소스 `.../UMG/Public/Animation/WidgetAnimation.h`, `UserWidget.h` |
| UInvalidationBox / URetainerBox | UMG | 정적 위젯 묶음의 레이아웃·페인트 캐시 / 렌더 타깃 갱신·효과 머티리얼 합성 | HUD 비용 절감 후보·크레딧 원근 표현 | InvalidationBox 사용처 미발견. RetainerBox는 `WBP_HubGameOverCredits`에 적용, shader·WBP 저장과 1인 PIE 원근 스크롤 표시 확인. 성능·cook 미검증 | 소스 `.../UMG/Public/Components/InvalidationBox.h`, `RetainerBox.h`; 프로젝트 [크레딧 원근 연결 스크립트](../../../scripts/editor/create_credits_perspective.py) |
| UListView / UDynamicEntryBox / `FUserWidgetPool` | UMG | 가상화 목록, 동적 항목, 위젯 풀 | 방 목록·로비 슬롯 | 사용(방 목록, 풀 재사용 테스트) | 소스 `.../Components/ListView.h`, `DynamicEntryBox.h`, `Blueprint/UserWidgetPool.h` |
| UCommonActivatableWidget + Stack/Queue | CommonUI 플러그인 (Runtime, Beta 표시 없음) | 활성/비활성 수명, 입력 구성(`GetDesiredInputConfig`)·포커스 대상, 뒤로 가기, 스택/큐 컨테이너 | 화면 전환·팝업 | 사용 | 소스 `Engine/Plugins/Runtime/CommonUI/Source/CommonUI/Public/CommonActivatableWidget.h`, `Widgets/CommonActivatableWidgetContainer.h` |
| CommonUI 위젯 모음 | CommonUI | `CommonButtonBase`·`CommonTextBlock`·`CommonTabListWidgetBase`·`CommonListView`/`TileView`/`TreeView`·`CommonAnimatedSwitcher`·`CommonLazyImage`·`CommonRichTextBlock`·`CommonNumericTextBlock`·`CommonLoadGuard`·`CommonWidgetCarousel` | 스타일 에셋 기반 버튼·탭·리스트 | 일부 사용 | 소스 `.../CommonUI/Public/` |
| 입력 라우팅·액션 바 | CommonUI | `UCommonUIActionRouterBase`, `CommonActionWidget`(현재 입력 장치의 키 아이콘 표시), `CommonBoundActionBar`, `CommonAnalogCursor`(패드로 마우스 커서 흉내) | 게임패드 메뉴 | 부분 사용(버튼 액션) | 소스 `.../CommonUI/Public/Input/` |
| CommonInput 서브시스템 | CommonUI 모듈 CommonInput (LocalPlayerSubsystem) | 현재 입력 유형(마우스/게임패드/터치) 감지·변경 이벤트·잠금·스래싱 방지 | 키 아이콘/프롬프트 전환 | 암묵 사용 | 소스 `.../CommonInput/Public/CommonInputSubsystem.h` |
| 플랫폼 입력 데이터 | CommonInput | `UCommonUIInputData`(Click/Back/Hold 액션), `UCommonInputBaseControllerData`(컨트롤러별 키→브러시 맵), `UCommonInputPlatformSettings`(플랫폼별 목록) | 키·패드 아이콘 | InputData만 있음 | 소스 `.../CommonInput/Public/CommonInputBaseTypes.h`, `CommonInputSettings.h` |
| CommonUI 가시성 | CommonUI | `CommonVisibilitySwitcher`, `CommonHardwareVisibilityBorder`, `UCommonUIVisibilitySubsystem`(플랫폼 특성 태그) | 입력 장치별 UI 숨김/표시 | 미사용 | 소스 `.../CommonUI/Public/CommonUIVisibilitySubsystem.h` |
| UMG ViewModel (MVVM) | 플러그인 ModelViewViewModel (Runtime+UncookedOnly+Editor, **Beta**, 자체 기본 꺼짐) | `UMVVMViewModelBase`, 필드 알림, 위젯↔뷰모델 바인딩(One/Two-way), 전역 뷰모델 컬렉션 | HUD·인벤토리 갱신 | 기본 활성 Metasound가 활성 의존, 바인딩 사용처 미발견·실제 로드 미검증 | 소스 `Engine/Plugins/Runtime/ModelViewViewModel/ModelViewViewModel.uplugin`, `Engine/Plugins/Runtime/Metasound/Metasound.uplugin`; 문서 umg-viewmodel |
| Slate (`SWidget`) | Slate/SlateCore (Runtime) | UMG 아래 레이어. 커스텀 컨트롤, `UNativeWidgetHost`로 UMG에 삽입 | UMG로 안 되는 커스텀 그리기 | 모듈 의존만, 직접 작성 없음(납부창이 `SWidget` 접근자 사용) | 소스 `.../UMG/Public/Components/NativeWidgetHost.h` |
| AHUD / Canvas | Engine | `DrawHUD`에서 Canvas로 텍스트·박스 그리기 | 개발용 디버그 표시 | 프로토타입 HUD | 소스 프로젝트 `DigetherPrototypeHUD.h` |
| UWidgetComponent | UMG | 위젯을 렌더 타깃에 그려 월드 공간/화면 공간에 표시 | 이름표·장비 상태 표시 | Variant_Combat 템플릿 LifeBar에서 생성, 정식 게임 사용 미검증 | 소스 `Engine/Source/Runtime/UMG/Public/Components/WidgetComponent.h`, `Engine/Source/Runtime/UMG/Private/Components/WidgetComponent.cpp`; 프로젝트 `DIGETHER/Source/DIGETHER/Variant_Combat/CombatCharacter.cpp`, `AI/CombatEnemy.cpp` |
| 포커스·네비게이션 | UMG / Slate | `UWidget::SetUserFocus`/`SetFocus`, `UWidgetNavigation` 규칙(Escape/Explicit/Custom…), `FNavigationConfig` | 게임패드 메뉴 이동 | 사용(CommonUI 경유) | 소스 `.../Blueprint/WidgetNavigation.h`, `Slate/Public/Framework/Application/NavigationConfig.h` |
| DPI 스케일 | Engine `UUserInterfaceSettings` | `UIScaleRule`(ShortestSide/LongestSide/Horizontal/Vertical/Custom), 곡선, `ApplicationScale`, 하드웨어/소프트웨어 커서 | 해상도 대응 | 기본값 | 소스 `.../Engine/Classes/Engine/UserInterfaceSettings.h` |
| 색각 보정/시뮬레이션 | UMG / SlateRHIRenderer | `UWidgetBlueprintLibrary::SetColorVisionDeficiencyType(Type, Severity, Correct, ShowCorrection)` | 색약 옵션(Slate 합성 단계) | 미사용 | 소스 `.../UMG/Public/Blueprint/WidgetBlueprintLibrary.h`, `SlateCore/Public/Rendering/SlateRenderer.h` |
| 자막 | Engine + 플러그인 SubtitlesAndClosedCaptions (Runtime, **Beta**, 기본 꺼짐) | `UGameplayStatics::SetSubtitlesEnabled/AreSubtitlesEnabled`, 플러그인은 `QueueSubtitle` 등과 자막 위젯 | 음성 대사·소리 정보 표시 | 미사용 | 소스 `.../Kismet/GameplayStatics.h`, `Engine/Plugins/SubtitlesAndClosedCaptions/` |
| 접근성 속성 | UMG / SlateCore | 위젯별 `AccessibleBehavior`·`AccessibleText`, 접근성 메시지 핸들러 | 화면 낭독 연동 후보 | 미사용 | 소스 `.../UMG/Public/Components/Widget.h`, `SlateCore/Public/Widgets/Accessibility/` |
| MoviePlayer 로딩 화면 | Runtime 모듈 MoviePlayer | `GetMoviePlayer()->SetupLoadingScreen(FLoadingScreenAttributes)`, `PlayMovie`/`StopMovie`, Slate 위젯/영상 | 맵 이동 로딩 화면 | 미사용 | 소스 `Engine/Source/Runtime/MoviePlayer/Public/MoviePlayer.h` |
| PreLoadScreenMoviePlayer / WindowsMoviePlayer | 플러그인 (RuntimeNoCommandlet, Beta 표시 없음; WindowsMoviePlayer는 기본 켜짐) | 시작 영상/프리로드 화면 구현 | 시작 로고 영상 | 미사용 | 소스 `Engine/Plugins/Runtime/PreLoadScreenMoviePlayer/*.uplugin`, `WindowsMoviePlayer/*.uplugin` |
| `UGameViewportSubsystem` | UMG | 뷰포트에 위젯 Z-order·슬롯 관리 | 레이어 순서 | 프로젝트 직접 호출 없음. Game HUD·결과·크레딧은 `AddToPlayerScreen` 경유 | 소스 `.../UMG/Public/Blueprint/GameViewportSubsystem.h`; 프로젝트 `Game/UI/DigetherPlayerScreenComponent.cpp` |

## 기능별 상세

### UMG 기본(UserWidget·BindWidget·애니메이션)
- 핵심: `NativeOnInitialized`(한 번만, `BindWidget` 연결 후) · `NativePreConstruct` · `NativeConstruct`/`NativeDestruct`. `UUserWidget`은 `meta=DisableNativeTick`이 기본이라 Tick을 쓰려면 명시한다(소스). 갱신은 이벤트로 하고 Property Binding·Tick은 피하라는 것이 공식 권고다(문서 umg-best-practices).
- 애니메이션: `BindWidgetAnim`/`BindWidgetAnimOptional` 메타로 C++에서 위젯 애니메이션을 받는다(UMGEditor가 해당 메타를 처리 — 소스 `AnimationTabSummoner.cpp`). 재생 함수가 `FWidgetAnimationHandle`을 돌려주므로 정지·완료 대기 시 핸들을 보관한다.
- 위젯 풀: `FUserWidgetPool`·`UListView`·`IUserObjectListEntry`로 항목 재사용. 프로젝트에 재사용 자동 테스트(`DigetherFrontendWidgetReuseTest`)가 있다.
- 주의: 프로토타입 `UDigetherDepositWidget`은 루트가 비면 코드가 기본 트리를 만든다. 표준 기능 규칙(트리를 코드로 그리지 않음)과 다르므로 정식 UI에서는 `WBP_` 배치로 옮긴다(프로토타입 한정).

### 성능: Invalidation·Retainer
- `Slate.EnableGlobalInvalidation`의 코드 기본값은 `false`이고 엔진 기본 ini에 재정의가 없다(소스 `SlateCoreClasses.cpp`, `Engine/Config/*.ini` 검색). 즉 이 빌드에서는 전체 무효화 방식이 아니라 위젯 단위 `UInvalidationBox`(`bCanCache`)로 캐시한다. 프로젝트 ini에도 설정이 없다.
- `UWidget::ForceVolatile`·`InvalidateLayoutAndVolatility`로 매 프레임 바뀌는 자식을 표시한다(소스). 캐시 대상 안에서 값이 자주 바뀌면 이득이 사라진다.
- `URetainerBox`는 자식을 렌더 타깃에 그려 갱신 주기를 낮춘다. 렌더 타깃 메모리·해상도 비용이 있어 실측 후 쓴다.
- 프로젝트는 [연결 스크립트](../../../scripts/editor/create_credits_perspective.py)로 `WBP_HubGameOverCredits`에 RetainerBox·AlphaComposite 효과 머티리얼을 적용했다. 2026-11-06 shader 재컴파일·WBP 저장, `GetTiltDegrees`→MID 파라미터 연결과 1인 PIE의 실제 원근 스크롤 표시를 확인했다. 갱신 주기 최적화·성능 개선·cook·다인 화면은 미검증이다.
- 공식 Invalidation Box 문서는 본문 추출에 실패해 소스 근거만 있다. 도구는 Widget Reflector(Ctrl+Shift+W)와 Slate 통계(문서 best-practices).
- 측정 방법과 예산은 [성능·빌드](PERF_BUILD.md).

### CommonUI
- 필수 설정: `GameViewportClientClassName`을 `CommonGameViewportClient`로(프로젝트 설정됨), 프로젝트 설정 > CommonInput에서 `InputData`(`UCommonUIInputData` 서브클래스: Click/Back 액션을 DataTable 또는 Enhanced Input 액션으로 지정). 프로젝트 `CommonInputSettings.InputData=/Game/Frontend/UI/Input/BP_UIInputData`.
- **Enhanced Input 연동은 기본 꺼짐**: `UCommonInputSettings::bEnableEnhancedInputSupport = false`(재시작 필요, 소스). 지금은 `DT_UIActions` DataTable 방식이다. 입력 전체 방향은 [입력](INPUT.md)과 맞춰 결정한다.
- 활성 위젯 규약: `GetDesiredInputConfig`(입력 모드·마우스 캡처 설정), `NativeGetDesiredFocusTarget`, `NativeOnHandleBackAction`, `bIsBackHandler`, `bAutoActivate`. 컨테이너는 `UCommonActivatableWidgetStack`(LIFO)과 `Queue`, `AddWidget<T>(Class)`는 풀에서 인스턴스를 꺼낸다(소스).
- 플랫폼 아이콘: `UCommonInputPlatformSettings`가 `UCommonInputBaseControllerData` 서브클래스 목록(`GamepadName`, `InputBrushDataMap`)을 갖는다. `CommonActionWidget`이 현재 입력 장치에 맞는 브러시를 보여 준다. 아직 컨트롤러 데이터 에셋이 없고, 지원 입력 장치 결정 D20이 미정이다([DECISIONS](../../planning/DECISIONS.md)).
- 프로젝트 콘솔 변수: `DefaultGame.ini [ConsoleVariables]`에 `CommonUI.CheckKeyboardFocusAndParentage` 등 3개가 켜져 있다. 이름만 확인했고 각 효과는 이번에 확인하지 못했다(미확인).
- 공식 CommonUI 문서 페이지는 목차만 추출되어 문서 근거를 얻지 못했다. 위 내용은 소스·프로젝트 코드 근거다.

### UMG ViewModel(MVVM)
- 플러그인 `ModelViewViewModel`: `IsBetaVersion true`, `IsExperimentalVersion false`, `EnabledByDefault false`, 모듈 5개(Runtime 1 + UncookedOnly 2 + Editor 2), `EnhancedInput` 플러그인 의존(소스). `Metasound.uplugin`은 기본 활성이고 MVVM을 `Enabled=true`로 의존하므로 자체 기본값만으로 비활성을 단정하지 않는다. 공식 문서도 Beta이며 출시 시 주의하라고 한다(기존 문서 근거 umg-viewmodel-for-unreal-engine, 이번 재대조는 소스).
- 핵심: `UMVVMViewModelBase`, `UE_MVVM_SET_PROPERTY_VALUE`·`UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED` 매크로(`MVVMViewModelBase.h`), 바인딩 One Time/One Way(ToWidget·ToViewModel)/Two Way, 뷰모델 생성 방식 Create Instance·Manual·Global Viewmodel Collection(`UMVVMGameSubsystem`)·Property Path(문서).
- 5.3부터 Blueprint용 `K2_BroadcastFieldValueChanged`·`K2_SetPropertyValue`가 deprecated이고 일반 블루프린트 setter를 쓰라고 한다. C++ `BroadcastFieldValueChanged(FFieldId)`와 `UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED` 매크로에는 이 폐기 표시가 없다(소스 `MVVMViewModelBase.h`).
- 후보 조건: 같은 모델을 여러 위젯이 보여 주거나, 값 변경 알림을 위젯마다 수동 연결하는 비용이 커질 때. 대안은 C++ 델리게이트 + `BindWidget`(현재 방식).
- 네트워크: 뷰모델은 로컬 객체다. 복제 상태(`PlayerState`/GameState)를 읽어 채우고, 뷰모델이 게임 상태를 직접 바꾸지 않는다.

### Slate를 쓰는 경우
- UMG가 `SWidget`을 감싼 것이므로 UMG 위젯으로 안 되는 커스텀 그리기·특수 입력만 Slate로 만든다. `UNativeWidgetHost`로 UMG에 끼운다. 프로젝트는 아직 직접 `SCompoundWidget`을 만들지 않았다(납부창이 입력 `SWidget` 포커스에 접근하는 정도).
- 코드로 Slate 트리를 짜는 것은 표준 기능 규칙에서 피하는 방식이다.

### HUD와 위젯
- `AHUD::DrawHUD`(Canvas)는 즉시 모드 그리기라 개발 확인용으로 간단하지만 입력·레이아웃·현지화·게임패드 포커스가 없다. 정식 정보 표시는 UMG 위젯으로 만들고, `PlayerController`(로컬)에서 생성한다. 프로젝트의 프론트엔드 컨트롤러는 `IsLocalController()`일 때만 레이아웃을 만든다.
- `Game/`의 [화면 컴포넌트](../../../DIGETHER/Source/DIGETHER/Game/UI/DigetherPlayerScreenComponent.cpp)는 로컬 Controller에 HUD·탐사 결과·게임 오버 크레딧을 `AddToPlayerScreen`으로 추가한다. HUD 기본 레이어 위에 결과(ZOrder 110)·크레딧(120)을 둔다. [HUD](../../../DIGETHER/Source/DIGETHER/Game/UI/Hud/DigetherRunHudWidget.cpp)는 공개 런·목표와 본인 지갑의 변경 알림을 구독하며, 늦은 GameState·PlayerState 복제나 교체 시 재연결한다. 다인 화면 검증과 성능 측정은 이 코드 확인과 별도다.
- 위젯이 보여 줄 값은 복제된 상태에서 읽는다. 다른 플레이어에게 숨겨야 할 값은 애초에 복제하지 않는다([네트워크 권위](../NETWORK_AUTHORITY.md), [네트워크](NETWORKING.md)).

### 월드 공간 위젯
- `UWidgetComponent`: `SetWidgetSpace`(World/Screen), `SetDrawSize`, `SetDrawAtDesiredSize`, `SetTwoSided`, `SetGeometryMode`(Plane/Cylinder), `SetTickMode`(Disabled/Enabled/Automatic), `SetManuallyRedraw`, `SetTickWhenOffscreen`, `SetOwnerPlayer`(소스).
- 전용 서버에서는 위젯을 만들지 않는다(`IsRunningDedicatedServer()` 분기 — 소스 `WidgetComponent.cpp`). 이 프로젝트는 리슨 서버라 호스트 PC에서는 모든 플레이어의 컴포넌트가 위젯을 만든다. 로컬 플레이어에게만 보일 값은 `SetOwnerPlayer`·로컬 조건으로 제한하는 설계가 필요하다(후보).
- 비용: 컴포넌트마다 렌더 타깃을 쓴다. 다수 사용 시 `SetManuallyRedraw`·`TickMode::Automatic`·크기 제한을 검토한다(실측 필요).

### 게임패드 포커스·네비게이션
- CommonUI 스택은 활성 위젯의 `GetDesiredFocusTarget`으로 포커스를 복원한다(`CommonUI.FallbackToDesiredOnAutoRestoreFailure`는 프로젝트가 켠 변수 이름만 확인). 위젯 단위 규칙은 `UWidgetNavigation`/`FWidgetNavigationData`(Escape/Explicit/Wrap/Stop/Custom 등)로 지정한다(소스).
- 마우스 커서 없이 패드로 일반 UMG를 조작해야 할 때 `FCommonAnalogCursor`가 후보다. 인게임 FPS 조작과 UI 모드 전환은 [입력](INPUT.md).

### DPI 스케일
- Project Settings > User Interface > DPI Scaling: `UIScaleRule`, 곡선, `ApplicationScale`(소스). 사용자가 런타임에 UI 배율을 바꾸게 하는 엔진 기본 UI는 확인하지 못했다(미확인) — 위젯 `ScaleBox`·`ApplicationScale` 변경 등을 프로젝트에서 구성해야 할 수 있다.
- `USafeZone`·`UScaleBox`·`USizeBox` 위젯 제공(소스 `Components/`).

### 폰트와 현지화 텍스트
- 위젯 텍스트는 `FText`로 둔다. 리터럴은 `LOCTEXT`/`NSLOCTEXT`, 표 참조는 String Table([데이터·저장](DATA_SAVE.md#localization)). 위젯 에셋의 텍스트도 Dashboard의 에셋 Gather 대상이다(`DefaultEditor.ini`에 `Content/Frontend/*` 포함).
- 엔진 기본 폰트: `Engine/Content/Slate/Fonts`에 Roboto 계열, `DroidSansFallback.ttf`, 태국어·아랍어 Noto가 있다(소스). **한글·일본어·중국어 간체/번체를 한 폰트로 표시할 수 있는지는 확인하지 못했다(미확인).** 프로젝트는 지원 문화가 9개로 설정돼 있어, 합성 폰트(`FCompositeFont`의 `FallbackTypeface`, 소스 `SlateCore/Public/Fonts/CompositeFont.h`)로 문화별 대체 글꼴을 구성하는 작업이 필요할 가능성이 크다(후보). 라이선스는 폰트 파일마다 확인한다.
- 줄바꿈·길이 변동(영어/독일어 등 긴 문자열)은 `USizeBox`·`UScaleBox` 같은 컨테이너 제약으로 자동 해결되지 않으므로 레이아웃 시험으로 확인한다. 시트 번역 후 표시 확인 절차는 [시트 번역 안내](../../guides/LOCALIZATION_SHEETS.md#순서) 7단계.

### 접근성
- **색각**: `UWidgetBlueprintLibrary::SetColorVisionDeficiencyType`은 Slate 렌더러에 보정/시뮬레이션 값을 전달한다(소스 `Engine/Source/Runtime/UMG/Private/WidgetBlueprintLibrary.cpp`). `SlateRHIRenderingPolicy.cpp`의 색각 보정 패스와 `SlateRHIRenderer.cpp`의 HDR UI 합성 분기가 있으므로 합성 주석만으로 3D 월드 화면은 영향이 없다고 단정할 수 없다. SDR/HDR별 실제 적용 범위는 미검증이다. 월드의 색 의존 표시는 색 외 단서(모양·아이콘)를 함께 두는 설계가 별도로 필요하다.
- **자막**: `UGameplayStatics::SetSubtitlesEnabled/AreSubtitlesEnabled`(엔진 기본 사운드 자막 켜기/끄기, 소스). 플러그인 `SubtitlesAndClosedCaptions`는 Beta이고 `ViewportWidgetOverlay` 플러그인에 의존한다(소스). 소리 정보 표시(공포 게임의 방향성 소리 힌트)는 오디오 설계와 같이 본다([오디오](AUDIO.md)).
- **텍스트 크기**: 엔진이 제공하는 전역 텍스트 크기 옵션은 찾지 못했다. 텍스트 스타일 에셋(`BP_TextStyle_*`)의 글꼴 크기를 설정값으로 곱하는 방식이 가능한 후보지만 이번에 검증하지 않았다(미확인).
- **화면 낭독**: UMG 위젯별 접근성 속성은 있다(`AccessibleBehavior`, `AccessibleText`). 플랫폼 지원 범위는 확인하지 못했다(미확인).

### 설정 화면
- 프로젝트 패턴: 위젯 값 → "적용" 시 `UDigetherGameUserSettings`의 setter 호출 → `ApplySettings`/`SaveSettings`(`DigetherSettingsPanelWidget`). 엔진 `UGameUserSettings`가 해상도·창 모드·VSync·프레임 제한·품질을 이미 제공한다([데이터·저장](DATA_SAVE.md)). 해상도 변경은 `ConfirmVideoMode`/`RevertVideoMode`로 되돌리기 확인 흐름을 만든다(소스).
- 키 재설정은 Enhanced Input 사용자 설정 영역이다([입력](INPUT.md)). 언어 선택은 `SetCurrentCulture(Culture, SaveToConfig)`를 쓴다(소스).
- 마우스 감도·볼륨은 엔진 항목이 아니라 프로젝트 서브클래스 필드다. 사운드 값 → SoundClass/Submix 연결은 아직 저장만 한다(`EffectsVolume`·`MusicVolume` 주석).

### 로딩 화면
- 엔진 기본: `IGameMoviePlayer::SetupLoadingScreen(FLoadingScreenAttributes)` — `WidgetLoadingScreen`(Slate), `MinimumLoadingScreenDisplayTime`, `bAutoCompleteWhenLoadingCompletes`, `bMoviesAreSkippable`, `bAllowEngineTick`, `PlaybackType`(소스). `OnPrepareLoadingScreen` 이벤트, `PlayMovie`/`StopMovie`/`IsMovieCurrentlyPlaying`.
- 맵 전환(`ServerTravel`/`ClientTravel`)에 이 로딩 화면을 언제 호출하는 것이 안전한지(`PreLoadMap` 훅 등)와 UMG 위젯을 `TakeWidget`으로 넘기는 방식은 이번에 확인하지 못했다(미확인).
- Lyra 샘플의 `CommonLoadingScreen` 플러그인은 `Engine/Plugins`에 없다(`*LoadingScreen*` 이름 검색에서 `XRBase`의 XR 전용만 나옴). 샘플 프로젝트는 이번에 열지 않았다.
- 프로젝트 맵 전환 기술([레벨·PCG](LEVELS_PCG.md), [온라인·음성](ONLINE_VOICE.md))이 정해진 뒤에 고른다.

### 멀티플레이 UI 주의
- 위젯은 로컬 전용 객체다. 서버/호스트가 클라이언트의 위젯을 갖지 않는다. 호스트 PC의 `PlayerController`는 모든 플레이어 몫이 존재하므로 생성은 `IsLocalController()`로 제한한다(프로젝트 코드가 그렇게 한다).
- 위젯 상태·뷰모델 값을 RPC로 보내지 않는다. 입력 의도만 서버 RPC로 보내고 결과는 복제 상태로 받는다([표준 기능](../STANDARD_FEATURES.md#enhanced-input)).
- 접속 직후·재접속 시 복제 상태가 늦게 도착한다. 위젯이 빈 값(0, 빈 이름)을 정상값으로 오인하지 않게 "아직 모름" 상태를 둔다(설계 후보). 프로젝트 문서 `docs/design/UNREAL_FEATURE_COVERAGE.md`의 "UI 늦은 생성·재수신" 검사 항목과 연결된다.

## 보류·비추천 후보와 이유

- **UMG ViewModel 즉시 도입**: Beta. 현재 화면 수·복잡도에서 필요성이 확인되지 않았다.
- **AHUD Canvas를 정식 HUD로**: 입력·현지화·포커스가 없다. 개발 디버그 표시용으로 한정.
- **Property Binding·위젯 Tick으로 값 갱신**: 공식 권고가 이벤트 기반이다(문서).
- **전역 Invalidation을 켜기**: 기본값 꺼짐. 프로젝트 전체 성능 측정 없이 바꾸지 않는다.
- **`CommonLoadingScreen`을 엔진 기능으로 가정**: 이번에 확인한 엔진 플러그인 목록에서 찾지 못했으며, 번들 지원을 확인한 근거가 없다.
- **Lyra UI 전체 이식**: 소스는 확인하지 않았고 범위가 크다. 필요한 개념만 차용하는 쪽이 비용이 작을 것으로 추정(미확인).

## 확인 한계

- 2026-11-04 엔진 기능 조사는 헤더·uplugin·문서 열람 기준이다. 2026-11-06 프로젝트 사용처는 코드·스크립트 대조와 위에 명시한 1인 PIE 표시 범위로 확인했다. 별도 근거를 적지 않은 기능의 실행·성능·패키지·다인 검증을 대신하지 않는다.
- 기존 조사 기록에는 UMG Viewmodel·UMG Best Practices 열람과 CommonUI·Invalidation Box 본문 추출 실패가 적혀 있다. 이번 UI 재대조는 로컬 소스·플러그인·프로젝트 코드·설정으로 수행했으며 이 문서 페이지들을 다시 열지 않았다. Slate/Retainer/WidgetComponent/MoviePlayer/접근성 공식 페이지도 이번에 열지 않았다.
- CommonUI 콘솔 변수 3개의 효과, 한국어 포함 CJK 폰트 커버리지, UI 텍스트 크기 옵션, 런타임 UI 배율 변경, 화면 낭독 플랫폼 지원, 로딩 화면의 맵 전환 호출 시점은 확인하지 못했다.
- 기존 [UNREAL_FEATURE_COVERAGE](../../design/UNREAL_FEATURE_COVERAGE.md)는 5.8.2(CL 56702186) 시점이고 이번 로컬 설치는 5.8.4다. DG-019(UMG) 행과 모순되는 사실은 발견하지 못했다.
- 위 "프로젝트 상태"는 현재 작업 트리(미커밋 변경 포함) 기준이다.
