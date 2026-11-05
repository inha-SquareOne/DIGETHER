# 게임플레이 프레임워크 기능 카탈로그

기준일 2026-11-04 · UE 5.8.4 (CL 58902052) · 상태: **조사 자료, 채택 결정 아님**. 색인은 [기능 카탈로그](README.md).

범위: GameInstance·Subsystem, GameMode/GameState/PlayerState/PlayerController/Pawn/Character, CharacterMovementComponent와 Mover, 카메라·1인칭 표현, HUD, 상호작용 트레이스 기초, Actor/Component 수명, Timer·Latent, World/Level, ChildActor, Interface. 입력은 [INPUT.md](INPUT.md), 충돌 세부는 [PHYSICS_COLLISION.md](PHYSICS_COLLISION.md), 복제·RPC는 [NETWORKING.md](NETWORKING.md), 프로젝트 권위 규칙은 [네트워크 권위](../NETWORK_AUTHORITY.md)를 따른다.

경로 약칭: `ENG` = `Engine/Source/Runtime/Engine`, `PLG` = `Engine/Plugins`. 모든 경로는 로컬 5.8.4 설치 기준 상대 경로다.

## 요약
- **Character + CharacterMovementComponent(CMC)가 기본 후보다.** 5.8에서도 안정 경로이며 예측·보정·스무딩이 내장돼 있다. 커스텀 이동은 `PhysCustom`·`FSavedMove_Character`·`FCharacterNetworkMoveData` 확장으로 한다. 프로젝트 Prototype이 이미 CMC 기본 복제를 재사용한다.
- **Mover 플러그인은 Experimental이다.** `.uplugin` `IsExperimentalVersion: true`, 공식 문서도 Experimental이라 하고 "use caution when shipping"이라 경고한다. CMC 후속으로 명시되나 CMC는 계속 지원된다고 문서가 밝힌다. 출시 주력으로 쓰기 전 별도 결정이 필요하다.
- **1인칭 표현은 엔진 내장 기능이 있다.** `UPrimitiveComponent::FirstPersonPrimitiveType`(None/FirstPerson/WorldSpaceRepresentation)과 `UCameraComponent::FirstPersonFieldOfView`·`FirstPersonScale`이 5.8 헤더에 있다. 1인칭 도구가 벽에 파고드는 문제와 타인 시점 그림자·월드 표현 분리에 쓸 수 있다. 프로젝트는 아직 쓰지 않는다.
- **Gameplay Cameras 플러그인은 Experimental, 기본 활성(`EnabledByDefault: true`)이다.** 데이터 기반 카메라 시스템이지만 1인칭 단순 카메라에는 과하다. 기본 `UCameraComponent`+`APlayerCameraManager`로 시작하는 쪽이 비용이 낮다.
- **Subsystem 종류 선택이 구조를 좌우한다.** GameInstance(세션·설정 수명), World(맵 단위 서비스), LocalPlayer(입력·UI), Engine(전역). 프로젝트는 `UGameInstanceSubsystem` 1개(`UDigetherSessionSubsystem`)만 쓴다.
- **5.8 변경 주의:** CMC의 이동 기반(Movement Base) API가 `FMovementBaseInterfaceData` 기반으로 바뀌며 기존 `UPrimitiveComponent*` 기반 API가 대량 deprecated(5.8). `GetMovementBase()` 등 사용 코드를 새로 쓸 때 주의한다.

## DIGETHER에서 쓰이는 곳
사용 여부는 `DIGETHER.uproject`·`DIGETHER/Source`·`Config`를 확인한 범위다. `미사용`·참조 미발견은 Blueprint·에셋 내부 부재나 실제 플러그인 비활성을 뜻하지 않으며, 런타임 로드·게임 실행은 별도 검증이다.

| 관련 기능 | 프로젝트 상태 |
|---|---|
| S01~S05 (방·출발·이동·시점·상호작용) | `ADigetherGameplayGameMode`(`AGameModeBase`) + `ADigetherGameplayGameState/PlayerState/PlayerController`(`Game/Framework/`) **이미 사용**. 정식 Pawn은 미결정이며 임시 `ADigetherTestPawn`(`ADefaultPawn`, `Game/Framework/DigetherTestPawn.h`)을 쓴다([구조 계획](../../design/CODE_STRUCTURE_PLAN.md)) |
| 이동·카메라 프로토타입 | `Prototype/Player/DigetherPrototypeCharacter.cpp`: `ACharacter` 하위, 기본 CMC(걷기/달리기/웅크리기), `UCameraComponent`(`bUsePawnControlRotation=true`). `SpringArm`은 템플릿 `DIGETHERCharacter`에만 있다. `FirstPersonPrimitiveType`·`FirstPersonFieldOfView`는 **미사용** |
| 방·로비(호스트형) | `UDigetherSessionSubsystem : UGameInstanceSubsystem`(`Game/Online/`), 로비 GameMode는 `bUseSeamlessTravel=false`, `GameInstanceClass`는 엔진 기본(`DefaultEngine.ini`) |
| 맵별 GameMode | `GameModeMapPrefixes`로 `L_MainMenu`/`L_Lobby` 지정(`DefaultEngine.ini`). 기본 GameMode는 아직 템플릿 `BP_ThirdPersonGameMode` |
| HUD | Prototype만 `AHUD` 하위(`ADigetherPrototypeHUD::DrawHUD`, 캔버스 직접 그리기). 정식 UI는 CommonUI 위젯([UI.md](UI.md)) |
| 상호작용 | Prototype이 `UInterface`(`UDigetherPrototypeInteractable`)와 `LineTraceSingleByChannel(ECC_Visibility)`을 사용. `ECC_GameTraceChannel` 커스텀 채널 설정은 `DefaultEngine.ini`에서 발견하지 못했다 |
| 타이머·지연 스폰 | `DIGETHER/Source/DIGETHER`의 `*.cpp`에서 `SetTimer(` 20행(템플릿 포함), `SpawnActorDeferred`(`DigetherPrototypeWorld.cpp`) 사용 |
| 시험 | 서버 권위 규칙은 [네트워크 권위](../NETWORK_AUTHORITY.md), 시험은 [검증 계획](../../verification/VERIFICATION_PLAN.md) |

기존 [UNREAL_FEATURE_COVERAGE.md](../../design/UNREAL_FEATURE_COVERAGE.md)와의 차이: 대응표는 CMC·GameMode 계열을 5.8.2 시점으로 서술한다. 이번 5.8.4 헤더에서 확인한 추가 사실(Mover 상태, 1인칭 렌더링, CMC 5.8 deprecated, `OnPostLogin`)은 이 문서가 처음 정리한다. 서로 모순되는 사실은 발견하지 못했다.

## 기능 목록
근거 표기: `소스` 로컬 5.8.4 헤더·`.uplugin`, `문서` Epic 공식 문서(열람 URL은 하단 "문서 근거"), `미확인`.

| 기능 | 모듈·플러그인 (타입, 안정성) | 하는 일 | DIGETHER 쓸모 | 프로젝트 상태 | 근거 |
|---|---|---|---|---|---|
| `UGameInstance` | Engine (Runtime) | 엔진 시작~종료 유지, 맵 이동 불변, 복제 안 됨, 서버·클라마다 별도 | 세션·설정 수명 | 엔진 기본 클래스 사용 | 소스 `ENG/Classes/Engine/GameInstance.h`; 문서 gameplay-framework |
| `UGameInstanceSubsystem` | Engine (Runtime) | GameInstance 수명 서비스, 자동 생성 | 세션·원격 설정 | **사용** (`UDigetherSessionSubsystem`) | 소스 `ENG/Public/Subsystems/GameInstanceSubsystem.h`; 문서 programming-subsystems |
| `UWorldSubsystem` / `UTickableWorldSubsystem` | Engine (Runtime) | 월드(맵) 단위 서비스, `OnWorldBeginPlay`·`DoesSupportWorldType` | 맵별 런 서비스 후보 | 미사용 | 소스 `ENG/Public/Subsystems/WorldSubsystem.h` (문서 페이지는 World 서브시스템을 다루지 않음) |
| `ULocalPlayerSubsystem` | Engine (Runtime) | 로컬 플레이어 수명, `PlayerControllerChanged` | 입력·UI(Enhanced Input·CommonInput이 이 계열) | 엔진 것만 간접 사용 | 소스 `ENG/Public/Subsystems/LocalPlayerSubsystem.h` |
| `UEngineSubsystem` | Engine (Runtime) | 엔진 모듈 수명 | 전역 도구·디바이스 | 미사용 | 소스 `ENG/Public/Subsystems/EngineSubsystem.h` |
| `AGameModeBase` | Engine (Runtime) | 서버 전용 규칙·스폰·로그인 | 현재 사용 | **사용** | 소스 `ENG/Classes/GameFramework/GameModeBase.h`; 문서 game-mode-and-game-state |
| `AGameMode` (+`MatchState`) | Engine (Runtime) | 대기·진행·종료 매치 상태 기계, 비활성 플레이어 보존 | 프로젝트는 런 상태를 자체 컴포넌트로 관리 중 | 미사용 | 소스 `ENG/Classes/GameFramework/GameMode.h` |
| `AGameStateBase` / `AGameState` | Engine (Runtime) | 전원에게 복제되는 공용 상태, `PlayerArray`, `GetServerWorldTimeSeconds` | 공유 런 상태 | `AGameStateBase` 사용 | 소스 `ENG/Classes/GameFramework/GameStateBase.h` |
| `APlayerState` | Engine (Runtime) | 플레이어별 복제 상태, `CopyProperties`/`OverrideWith`(재접속·트래블), `bIsInactive` | 같은 ID 재접속 시 상태 복원 | `APlayerState` 하위 사용 | 소스 `ENG/Classes/GameFramework/PlayerState.h` |
| `APlayerController` | Engine (Runtime) | 입력·HUD·카메라 소유, `ClientRestart`·`SetInputMode`·`SetViewTargetWithBlend` | 요청 RPC 경로, 관전 | **사용** | 소스 `ENG/Classes/GameFramework/PlayerController.h` |
| `APawn` / `ACharacter` | Engine (Runtime) | 월드 표현. Character=캡슐+메시+CMC | 플레이어 몸체 | Prototype `ACharacter` 사용, 정식 Pawn 미결정 | 소스 `ENG/Classes/GameFramework/Pawn.h`, `Character.h` |
| `UCharacterMovementComponent` | Engine (Runtime) | 걷기/낙하/비행/수영/NavWalking/Custom, 클라 예측·서버 검증·보정, 커스텀 중력 방향 | 이동 기본 후보 | Prototype 사용 | 소스 `ENG/Classes/GameFramework/CharacterMovementComponent.h`; 문서 networked-movement |
| CMC 확장 (`FSavedMove_Character`·`FCharacterNetworkMoveData`·`MOVE_Custom`) | Engine | 커스텀 입력 플래그·이동 모드의 예측 | 달리기·깔림 제한 등 이동 확장 | 미사용 | 소스 `CharacterMovementComponent.h`, `CharacterMovementReplication.h`; 문서 networked-movement |
| `Mover` | PLG/Experimental/Mover (Runtime, **Experimental**) | NetworkPrediction/Chaos 기반 롤백 이동, 모드·레이어드 무브·모디파이어 | CMC 후속 후보(보류) | 미사용 | 소스 `Mover.uplugin`, `Mover/README.md`; 문서 mover-in-unreal-engine |
| `ChaosMover` / `MoverExamples` / `MoverIntegrations` | PLG/Experimental (Runtime, **Experimental**) | 물리 기반 Mover, 예제(출시 비권장), Mass 등 연동 | 보류 | 미사용 | 소스 각 `.uplugin` |
| `UCameraComponent` + `APlayerCameraManager` | Engine (Runtime) | 카메라, 뷰 타깃 블렌드, `UCameraModifier`·카메라 쉐이크 | 1인칭 기본 카메라 | Prototype 사용 | 소스 `ENG/Classes/Camera/CameraComponent.h`, `PlayerCameraManager.h` |
| `USpringArmComponent` | Engine (Runtime) | 카메라 붐, 충돌 프로브, 지연 | 3인칭·관전 시점 | 템플릿 Character만 | 소스 `ENG/Classes/GameFramework/SpringArmComponent.h` |
| Gameplay Cameras | PLG/Cameras/GameplayCameras (Runtime+Editor, **Experimental**, 기본 활성) | 데이터 기반 카메라 리그·블렌드·StateTree 디렉터, `UGameplayCameraComponent` | 연출형 카메라 필요 시 | 미사용 | 소스 `GameplayCameras.uplugin`, `Public/GameFramework/*`; 문서 추출 실패 |
| 1인칭 렌더링 (`FirstPersonPrimitiveType`, `FirstPersonFieldOfView`, `FirstPersonScale`) | Engine (Runtime) | 1인칭 도구를 별도 FOV·축소 스케일로 그려 벽 관통 방지, 월드 표현(타인 시점·그림자) 분리 | 1인칭 도구(곡괭이·작업등) | 미사용 | 소스 `PrimitiveComponent.h:153,728`, `CameraComponent.h:49`, `CameraTypes.h:56`, `GameplayStatics.h:1473`; 문서 추출 실패 |
| `AHUD` | Engine (Runtime) | 캔버스 직접 그리기, `showdebug` 대상 | 디버그 표시 | Prototype 사용 | 소스 `ENG/Classes/GameFramework/HUD.h` |
| 트레이스 (`LineTraceSingleByChannel` 등) | Engine (Runtime) | 상호작용 대상 탐색 | 조준 상호작용(S04) | Prototype 사용 | 소스 `Prototype/*.cpp`; 세부는 [PHYSICS_COLLISION.md](PHYSICS_COLLISION.md) |
| Actor/Component 수명 (`PreInitializeComponents`→`BeginPlay`→`EndPlay`) | Engine (Runtime) | 초기화 순서, `SpawnActorDeferred`+`FinishSpawning` | 스폰 시 값 선주입 | 사용 | 소스 `ENG/Classes/GameFramework/Actor.h` 주석, `Engine/World.h:3851` |
| `FTimerManager` | Engine (Runtime) | 지연·반복 호출, `FTimerHandle` | 구출 진행·쿨다운 | **사용**(C++ `SetTimer(` 20행, 템플릿 포함) | 소스 `ENG/Public/TimerManager.h`, `ENG/Private/World.cpp`(`UWorld::GetTimerManager`) |
| Latent (`FLatentActionManager`) | Engine (Runtime) | Blueprint Delay류 지연 액션 | BP 흐름 | 미확인(사용 여부) | 소스 `ENG/Classes/Engine/LatentActionManager.h` 존재 확인 |
| `UChildActorComponent` | Engine (Runtime) | 컴포넌트로 다른 Actor 클래스를 포함 | 보류(아래 주의) | 미사용 | 소스 `ENG/Classes/Components/ChildActorComponent.h` |
| `UInterface` | CoreUObject | C++/BP 공용 계약 | 상호작용 대상 공통 인터페이스 | **사용** | 소스 `Prototype/DigetherPrototypeTypes.h:149` |
| `ModularGameplay` / `GameFeatures` | PLG/Runtime (ModularGameplay: Runtime, GameFeatures: Runtime+Editor, 둘 다 **Beta**, 기본 비활성) | 컴포넌트 주입·기능 모듈화 | 별도 ADR 대상([표준 기능](../STANDARD_FEATURES.md#별도-결정이-필요한-것)) | 게임 기능 사용은 C++에서 찾지 못함. Editor는 `AllToolsets`→`GameFeaturesToolset`→`GameFeatures`→`ModularGameplay` 의존 경로 있음(실제 로드 미확인) | 소스 `PLG/Runtime/ModularGameplay/ModularGameplay.uplugin`, `PLG/Runtime/GameFeatures/GameFeatures.uplugin`, `PLG/Experimental/Toolsets/{AllToolsets,GameFeaturesToolset}/*.uplugin` |

## 기능별 상세

### GameInstance와 Subsystem 종류
- 핵심: `UGameInstance`, `UGameInstanceSubsystem`·`UWorldSubsystem`·`ULocalPlayerSubsystem`·`UEngineSubsystem`(헤더는 `ENG/Public/Subsystems/`). 접근은 `GetGameInstance()->GetSubsystem<T>()`, `ULocalPlayer::GetSubsystem<T>()`, `GEngine->GetEngineSubsystem<T>()`, `UWorld::GetSubsystem<T>()`(월드).
- 설정 위치: Project Settings > Maps & Modes > Game Instance Class(`[/Script/EngineSettings.GameMapsSettings] GameInstanceClass`). 프로젝트는 엔진 기본값이다.
- 네트워크: GameInstance·Subsystem은 복제되지 않는다. 서버·클라이언트마다 별도 인스턴스다. 다른 플레이어에게 보여야 하는 값은 복제 Actor(GameState/PlayerState)로 보낸다.
- 제약·주의: `UWorldSubsystem`은 `DoesSupportWorldType`/`ShouldCreateSubsystem`으로 생성 월드를 거른다. 서버 전용 로직은 클라이언트에도 생성될 수 있으므로 `ShouldCreateSubsystem`에서 네트워크 모드를 확인한다(`미확인`: 구체 판별 API는 이번에 확인하지 않음). Subsystem 문서 페이지는 World를 다루지 않는다.
- 대안: 전역 싱글턴 Actor는 [표준 기능](../STANDARD_FEATURES.md)이 피할 것으로 정했다.

### GameMode / GameState / PlayerState
- 핵심: `AGameModeBase`(`InitGame`, `PreLogin`/`PreLoginAsync`, `Login`, `PostLogin`→`OnPostLogin`, `HandleStartingNewPlayer`, `ChoosePlayerStart`, `GetSeamlessTravelActorList`, `SwapPlayerControllers`, `HandleSeamlessTravelPlayer`). `AGameMode`는 `MatchState`, `ReadyToStartMatch`, `AddInactivePlayer`/`FindInactivePlayer`/`OverridePlayerState`, `bDelayedStart`를 더한다. `APlayerState`는 `CopyProperties`·`OverrideWith`·`SeamlessTravelTo`·`OnDeactivated`/`OnReactivated`·`bIsInactive`.
- 설정 위치: World Settings의 GameMode Override, `GameModeMapPrefixes`(프로젝트가 사용), `GlobalDefaultGameMode`. `AGameMode` 사용 시 GameState도 `AGameState`로 맞추라고 문서가 권고한다.
- 네트워크·리슨 서버: GameMode는 서버에만 존재하고 복제되지 않는다(문서). 클라이언트가 알아야 할 값은 GameState에 둔다. 리슨 서버 호스트도 클라이언트 역할을 겸하므로 GameMode 값이 호스트 로컬 UI에서만 보이는 실수를 피한다. 재접속은 `AGameMode`의 비활성 플레이어 보존 기능이 있으나 이번에 동작을 검증하지 않았다. 프로젝트 요구(같은 ID 재접속)는 [UNREAL_FEATURE_COVERAGE.md](../../design/UNREAL_FEATURE_COVERAGE.md)의 재접속 기술 계약을 따른다.
- 제약·주의·deprecated: `AGameModeBase::DispatchPostLogin`은 5.6부터 deprecated이며 `OnPostLogin`이 `PostLogin` 끝에서 실행된다. 프로젝트의 `PostLogin` 오버라이드는 유효하다. `APlayerState::Score`는 5.8 헤더에도 존재한다(`GetScore` 접근자 사용).
- 대안: 프로젝트는 `AGameMode` 상태 기계 대신 자체 런 상태(`UDigetherRunCoordinatorComponent`)를 쓴다. 두 방식을 섞으면 `bDelayedStart`·`MatchState` 기본 흐름과 충돌할 수 있으므로 `AGameModeBase`를 유지하는 현재 선택이 일관적이다.

### PlayerController와 Pawn
- 핵심: `PlayerController`의 `SetViewTargetWithBlend`, `ClientRestart`, `AcknowledgePossession`, `ClientSetHUD`, `bAutoManageActiveCameraTarget`, `PlayerCameraManagerClass`. `APawn`의 `PossessedBy`/`UnPossessed`/`OnRep_Controller`/`OnRep_PlayerState`/`PawnClientRestart`/`IsLocallyControlled`/`GetPawnViewLocation`. `RemoteViewPitch16`(5.6에서 `RemoteViewPitch` 대체).
- 네트워크: PlayerController는 서버와 소유 클라이언트에 존재하며, 다른 클라이언트에 대한 관련성을 허용하지 않는다(소스 `ENG/Private/PlayerController.cpp`: `IsNetRelevantFor`가 `this == RealViewer` 반환). 클라이언트 요청 RPC는 소유 PlayerController나 Pawn 경로를 통과해야 한다([네트워크 권위](../NETWORK_AUTHORITY.md)).
- 제약·주의: Pawn이 늦게 도착하거나 재스폰될 때 입력 컨텍스트·카메라를 `OnPossess`/`PawnClientRestart`/`OnRep_PlayerState` 중 어느 시점에 적용할지 정해야 한다. 초기 상태를 한 시점 OnRep에만 의존하지 않는다(네트워크 권위 문서의 규칙).
- 5.8 deprecated: `APawn::GetMovementBase`와 `ACharacter`의 `UPrimitiveComponent*` 기반 이동 기반 API → `GetMovementBaseObject`/`GetMovementBaseInterfaceData`(`Pawn.h:59`, `Character.h:90–200,515,797`).

### CharacterMovementComponent
- 핵심: `UCharacterMovementComponent`(`SetMovementMode`, `MOVE_Custom`+`CustomMovementMode`, `PhysCustom`, `OnMovementModeChanged`, `UpdateFromCompressedFlags`, `ControlledCharacterMove`), `FSavedMove_Character`(`SetMoveFor`/`PostUpdate`/`CanCombineWith`/`CombineWith`/`PrepMoveFor`/`GetCompressedFlags`, `FLAG_Custom_0~3`), `FNetworkPredictionData_Client_Character`, `FCharacterNetworkMoveData`/`FCharacterNetworkMoveDataContainer`(`ClientFillNetworkMoveData`·`Serialize`, `SetNetworkMoveDataContainer`), 응답 컨테이너(`SetMoveResponseDataContainer`).
- 설정: Character의 CMC 프로퍼티(MaxWalkSpeed 등), `[/Script/Engine.GameNetworkManager]`의 `ClientErrorUpdateRateLimit`·`MAXPOSITIONERRORSQUARED`(문서), `NetworkSmoothingMode`. CVar: `p.NetUsePackedMovementRPCs`(소스), `p.NetShowCorrections`(소스 `CharacterMovementComponent.cpp:420`).
- 네트워크·리슨 서버: 소유 클라이언트가 이동을 예측 실행하고 서버가 재현·검증한다. 어긋나면 `ClientAdjustPosition`으로 보정 후 저장된 이동을 재생한다(문서). 비소유 클라이언트는 `ReplicatedMovement`+스무딩. 속도·달리기처럼 **예측에 영향 주는 입력은 `GetCompressedFlags`/커스텀 이동 데이터로 서버에 같이 보낸다.** 서버에서만 `MaxWalkSpeed`를 바꾸면 클라이언트 예측과 어긋나 보정이 반복된다. 현재 Prototype은 달리기를 `MaxWalkSpeed` 대입으로 처리한다(`DigetherPrototypeCharacter.cpp:254`). 정식 Pawn 구현에서 커스텀 이동 확장 여부를 검토 대상으로 둔다(채택 여부는 미결정).
- 제약·주의: 5.8.4 헤더에서 `GravityDirection`/`SetGravityDirection`을 확인했다. 도입 버전은 이번 소스만으로 판단하지 않는다. 이동 기반 API 변경은 위 deprecated 항목. 구식 RPC 경로는 `SUPPORT_DEPRECATED_CHARACTER_MOVEMENT_RPCS`+`p.NetUsePackedMovementRPCs=0`으로만 유지된다(문서).
- 대안: 이동을 `ADefaultPawn`(임시 시험 Pawn)처럼 `FloatingPawnMovement`로 두면 클라이언트 이동의 서버 전달이 없다(프로젝트 문서에 명시). Mover는 아래.

### Mover (Experimental)
- 핵심: `UMoverComponent`(`PLG/Experimental/Mover/Source/Mover/Public/MoverComponent.h`), 모드(`MovementMode.h`), `LayeredMove*.h`, `InstantMovementEffect.h`, `MovementModifier.h`, `MovementModeTransition.h`, `DefaultMovementSet/`. 백엔드 라이종: `MoverNetworkPredictionLiaisonComponent`(Network Prediction), `MoverStandaloneLiaisonComponent`, ChaosMover의 라이종.
- 설정: Mover 권장 설정은 Network Prediction의 Preferred Ticking Policy=Fixed, Simulated Proxy Network LOD=Interpolated, Enable Fixed Tick Smoothing=true(README).
- 네트워크: 모든 클라이언트와 서버가 공통 타임라인에서 시뮬레이션하고 서버가 상태를 방송하며 클라이언트가 롤백·재시뮬레이션한다(README). Mover는 Enhanced Input을 직접 다루지 않고 프로젝트가 입력을 `InputCmd`로 변환해 제출한다(README).
- 제약: `.uplugin` `IsExperimentalVersion: true`, README "APIs and data formats are subject to change at any time", 문서 "use caution when shipping". `MoverExamples`는 "Not intended to be used directly in a shipping product". 의존: NetworkPrediction, MotionWarping, PoseSearch, Water, ChaosVD 플러그인 참조(`Mover.uplugin`)이며 활성 상태·빌드 영향은 확인하지 않았다.
- 대안: CMC. 문서는 Mover가 CMC를 대체하려는 방향이지만 CMC도 당분간 지원된다고 밝힌다.

### 카메라와 1인칭 표현
- 핵심: `UCameraComponent`(`FieldOfView`, `FirstPersonFieldOfView`, `FirstPersonScale`, `bUsePawnControlRotation`), `APlayerCameraManager`(`ViewPitchMin/Max`, `ModifierList`·`UCameraModifier`, `CameraStyle`), `UPrimitiveComponent::SetFirstPersonPrimitiveType`, `UGameplayStatics::TransformWorldToFirstPerson`(`GameplayStatics.h:1473`), `FMinimalViewInfo::bUseFirstPersonParameters`.
- `EFirstPersonPrimitiveType`: `None` / `FirstPerson`(카메라의 1인칭 FOV·스케일 적용 대상) / `WorldSpaceRepresentation`(타인이 보는 월드 표현. 내부적으로 `bOwnerNoSee=true`, `bCastHiddenShadow=false` 설정, 주석상 VSM 그림자 정확성에 필요).
- 설정: 카메라 컴포넌트의 Camera Settings > First Person FOV·Scale(각각 `bEnableFirstPersonFieldOfView`/`bEnableFirstPersonScale` 조건), 프리미티브의 Rendering > First Person Primitive Type. 프로젝트 렌더 설정은 `r.Shadow.Virtual.Enable=0`이라 위 VSM 주석의 직접 적용 여부는 확인하지 않았다.
- 네트워크: 카메라·1인칭 렌더링은 로컬 표현이다. 타인 시점 표현(`WorldSpaceRepresentation` 메시)은 별도 월드 표현을 모든 클라이언트가 보도록 구성해야 한다.
- Gameplay Cameras: `UGameplayCameraComponent`, `UGameplayCameraSystemComponent`, `GameplayCamerasPlayerCameraManager`, `ActivateCameraRigFunctions`, StateTree 연동(`Directors/StateTreeCameraDirectorTasks.h`). `.uplugin` `IsExperimentalVersion: true`, `VersionName 0.1`, 모듈 중 Editor 전용 모듈 존재, 의존 플러그인: EnhancedInput, StateTree, TemplateSequence. 프로젝트에는 StateTree·EnhancedInput이 이미 있다. 공식 문서 페이지 본문은 추출되지 않아 사용법은 확인하지 못했다.
- 대안: 기본 카메라 + `UCameraModifier`(카메라 쉐이크). 3인칭 관전은 `USpringArmComponent`+`SetViewTargetWithBlend`.

### HUD와 UI 경계
- `AHUD`는 캔버스로 직접 그리는 낡은 계열이며 `showdebug`와 `AHUD::OnShowDebugInfo` 델리게이트의 대상이다. Enhanced Input의 `showdebug EnhancedInput`도 이 경로를 쓴다([INPUT.md](INPUT.md)). 정식 UI는 [표준 기능](../STANDARD_FEATURES.md)대로 CommonUI 위젯이며 HUD는 디버그 용도로 남기는 것이 일관적이다.

### 상호작용 트레이스 기초
- 핵심: `UWorld::LineTraceSingleByChannel`/`SweepSingleByChannel`, `FCollisionQueryParams`(`AddIgnoredActor`), `APawn::GetPawnViewLocation`, `APlayerController::GetHitResultUnderCursor*`(`GetHitResultUnderCursor`는 deprecated 표시, `ByChannel`/`ForObjects` 사용).
- 설정: 전용 트레이스 채널은 Project Settings > Collision의 `ECC_GameTraceChannel1~18`. 현재 프로젝트는 `ECC_Visibility`를 쓰며 채널 정의는 `DefaultEngine.ini`에서 발견하지 못했다.
- 네트워크: 클라이언트의 조준 결과는 요청일 뿐이고 서버가 거리·시야·상태를 다시 확인한다([네트워크 권위](../NETWORK_AUTHORITY.md)). 충돌 설정과 프로필은 [PHYSICS_COLLISION.md](PHYSICS_COLLISION.md).

### Actor/Component 수명, 지연 스폰
- 순서(`Actor.h` 주석): 생성자 → `OnConstruction` → `PreInitializeComponents` → 컴포넌트 `InitializeComponent` → `PostInitializeComponents` → `BeginPlay`(네트워크·자식 액터에서는 지연 가능) → `EndPlay`(`EEndPlayReason`).
- `SpawnActorDeferred`+`FinishSpawning`: 액터 객체를 만든 뒤 Construction Script 실행을 미룬 사이에 서버 쪽 초기 값을 주입한다. 복제 프로퍼티의 초기 상태 전달·클라이언트 초기화 순서는 별도 확인한다. `Actor.h`의 `bExchangedRoles`·`PostNetInit`·`OnRep_Owner`가 관련 지점이다.
- 5.7 이후 Iris 관련 복제 시작/종료 훅이 바뀌었다(`Actor.h:3493,3499` deprecated 안내). Iris 사용은 별도 결정([NETWORKING.md](NETWORKING.md)).
- `bAllowTickBeforeBeginPlay`, 컴포넌트 `bWantsInitializeComponent`·`bAutoActivate`·`SetIsReplicated`를 주의한다.

### Timers·Latent
- `FTimerManager::SetTimer(Handle, Obj, Fn, Rate, bLoop, FirstDelay)`, 람다·`FTimerDelegate::CreateWeakLambda`(프로젝트 사용). 핸들은 멤버로 보관하고 `EndPlay`에서 정리한다. 소스 `ENG/Private/World.cpp`의 `UWorld::GetTimerManager`는 GameInstance가 있으면 그 타이머 관리자를 반환하므로, 레벨 전환만으로 모든 타이머가 취소된다고 가정하지 않는다. 콜백 대상·델리게이트 수명과 명시적 해제를 확인한다.
- Latent 액션(`FLatentActionManager`)은 Blueprint Delay·MoveComponentTo 등에 쓰이며 C++에서도 `AddNewAction`으로 등록할 수 있다(소스 `ENG/Classes/Engine/LatentActionManager.h`). 타이머·StateTree와의 선택은 필요한 수명·취소 동작에 따라 비교한다.

### World / Level
- `UWorld`, `ULevel`, `UWorldPartition`, 레벨 스트리밍은 [LEVELS_PCG.md](LEVELS_PCG.md)에서 다룬다. 여기서는 프레임워크 관점만 적는다: GameInstance는 맵 이동 후에도 유지되고 GameMode는 레벨마다 새로 만들어진다(문서). 일반 이동에서 GameState·PlayerController가 다시 만들어지는지는 이번에 확인하지 않았다(`미확인`). Seamless travel은 `bUseSeamlessTravel`+`GetSeamlessTravelActorList`+`PlayerState::CopyProperties`로 상태를 옮긴다(소스). 프로젝트 로비 GameMode는 현재 `bUseSeamlessTravel=false`.

### ChildActor 주의
- `UChildActorComponent`는 에디터에서 `ChildActorTemplate`·`FChildActorComponentInstanceData`로 인스턴스 데이터를 보존하지만 복제·소유권·BeginPlay 타이밍이 일반 Actor와 달라지기 쉽다. 이번에 헤더 구조만 확인했고 복제 동작은 검증하지 않았다(`미확인`). 프로젝트는 사용하지 않는다. 컴포넌트 합성으로 충분하면 Actor Component를 우선한다.

### Interface
- C++ `UINTERFACE`/`I…` 쌍과 Blueprint 인터페이스. 상호작용 대상 공통 계약에 적합하다. 프로젝트 구조 계획은 "공통 상호작용 인터페이스+기능별 Component"를 이미 정했다([구조 계획](../../design/CODE_STRUCTURE_PLAN.md)). 인터페이스 함수 호출은 서버 권위 판정을 대신하지 않는다.

## 보류·비추천 후보와 이유
- **Mover**: Experimental. 출시 주력으로 쓰기 전 별도 결정과 CMC와의 비교 시험이 필요하다.
- **MoverExamples**: 출시 제품에 직접 쓰는 것을 의도하지 않는다고 `.uplugin` 설명에 명시되어 있다.
- **Gameplay Cameras**: Experimental. 1인칭 기본 카메라에는 과하다. 연출 카메라가 필요할 때만 평가한다.
- **ModularGameplay/GameFeatures**: 둘 다 Beta이며 구조 영향이 커서 게임 기능 도입은 ADR 대상. Editor 도구의 전이 의존 활성과 게임 기능 채택을 구분한다.
- **`AGameMode` 매치 상태 기계**: 프로젝트는 자체 런 상태 컴포넌트가 있다. 두 상태 기계를 병행하면 이중 원본이 된다.
- **ChildActorComponent**: 복제·수명이 일반 Actor와 달라 특별한 이유가 없으면 비추천.
- **`GetHitResultUnderCursor`(deprecated 표시)** 및 `GetMovementBase` 등 5.8 deprecated API.

## 확인 한계
- 문서 근거로 열어 확인한 페이지: gameplay-framework, game-mode-and-game-state, programming-subsystems, understanding-networked-movement-in-the-character-movement-component, mover-in-unreal-engine, enhanced-input (열람 화면은 UE 5.8). URL 앞부분은 모두 `https://dev.epicgames.com/documentation/en-us/unreal-engine/`이며 뒤는 `gameplay-framework-in-unreal-engine`, `game-mode-and-game-state-in-unreal-engine`, `programming-subsystems-in-unreal-engine`, `understanding-networked-movement-in-the-character-movement-component-for-unreal-engine`, `mover-in-unreal-engine`, `enhanced-input-in-unreal-engine`. gameplay-cameras, first-person rendering, commonui-with-enhanced-input 페이지는 본문이 추출되지 않아 소스 근거로 대신했다.
- 동작 검증은 하지 않았다(PIE·멀티플레이 시험 없음). 지원 사실은 헤더·문서에서 확인한 것이며 프로젝트의 실제 검증이 아니다.
- Mover의 의존 플러그인 활성 시 빌드·패키지 영향, Gameplay Cameras와 프로젝트 StateTree 조합, 1인칭 렌더링과 프로젝트 셰이더(툰) 호환은 확인하지 못했다.
- CMC 확장 시 현재 Prototype 달리기 구조의 보정 빈도는 측정하지 않았다.
- 시험·채택 순서는 이 문서가 정하지 않는다.
