# AI·내비게이션 기능 카탈로그

기준일 2026-11-04 · UE 5.8.4 (CL 58902052) · 상태: **조사 자료, 채택 결정 아님**. 색인은 [기능 카탈로그](README.md). 프로젝트 상태의 `미사용`·`없음`은 읽은 C++·설정 범위이며 Blueprint·에셋 내부 사용과 실행 상태는 미확인이다.

## 요약
- **이미 정해진 것**: 몬스터 이동은 런타임 내비메시 `RuntimeGeneration=Dynamic` + 맵의 제한된 NavMeshBoundsVolume, 몬스터 AI는 Behavior Tree(2026-11-04 사용자 결정, [코드 구조 계획](../../design/CODE_STRUCTURE_PLAN.md)). 설정은 `DefaultEngine.ini`에 들어갔지만 굴착 후 경로 갱신은 **아직 실행으로 확인하지 않았다**(커밋 29eabbb).
- **굴착 후 경로 갱신이 가장 큰 위험**: 프로젝트의 메시 교체는 충돌 활성 상태도 전환한다. `SetCollisionEnabled` → 내비 관련성 변경 시 `FNavigationSystem::UpdateComponentData` → 내비 옥트리 갱신 호출 경로를 확인했다(소스). 실제 타일 재생성·지연·경로 일치는 실측 대상이다. 아래 "굴착 후 경로 갱신" 참고.
- **AIController는 서버에만 존재한다**(`AIController.h` 주석). 감지·BT·길찾기는 서버 전용으로 짜고, 클라이언트에 보일 상태(애니메이션·소리)만 복제한다.
- **AI Perception**(Sight/Hearing/Damage/Team)은 AIController에 컴포넌트를 붙여 쓴다. Hearing은 `MakeNoise`/`ReportNoiseEvent`로 서버에서 보고한다. 음성 감지(PQ-13)는 소리 보고를 호스트가 받을 수 있는지가 먼저다.
- **StateTree는 이미 활성**(StateTree·GameplayStateTree 플러그인, Build.cs 의존). BT와 병행 가능하며 BT 안에서 StateTree를 돌리는 태스크도 있다(소스). 결정은 BT이므로 StateTree는 보조 후보로만 본다.
- **EQS는 코어 AIModule에 있고 `.uplugin`·문서에 Experimental 표시가 없다**. 편집기 플러그인 `EnvironmentQueryEditor`는 UncookedOnly·기본 활성이다.
- **Mass AI·ZoneGraph·Smart Object 연계 플러그인의 상태는 각각 다르다**. Mass AI·ZoneGraph·GameplayBehaviorSmartObjects·GameplayInteractions는 Experimental이다. SmartObjects 자체는 `IsBetaVersion=false`이고 Experimental 키가 없다. 1~4인 협동 몬스터 소수 규모라 우선순위는 낮다.

## DIGETHER에서 쓰이는 곳
| 기능 | 관련 | 프로젝트 상태 |
|---|---|---|
| 몬스터 AI (DEV-049), 최초 몬스터 1~2종 | S03, S14, S25~S26, DG-018, D23(종 선정 대기, BT 노드 구조 미설계) | 정의·체력 코어만 있음(`Game/Monster/`). AIController·BT 자산 **없음** |
| 굴착 후 몬스터 경로 갱신 (DEV-052) | S05~S07, DG-007 | `RecastNavMesh RuntimeGeneration=Dynamic` 설정만 반영. 실행 검증 없음 |
| 음성·소리 감지 | S25, S28, T21, T60, PQ-13 | 미사용. 음성 활동이 호스트에 도달하는지 미검증 |
| StateTree | 템플릿 | `Variant_Combat/AI/CombatAIController`가 `UStateTreeAIComponent` 사용(UE 템플릿 잔재). 게임 로직 아님 |
| AIModule 의존 | Build.cs | `AIModule`, `StateTreeModule`, `GameplayStateTreeModule` 의존. NavigationSystem 모듈 직접 의존 없음 |
| 내비 제외 | 엘리베이터·시험 씬 | `SetCanEverAffectNavigation(false)` 호출(엘리베이터 바닥·탑승 볼륨·레버, 프로토타입 씬 부품) |

## 기능 목록
| 기능 | 모듈·플러그인 (타입, 안정성) | 하는 일 | DIGETHER 쓸모 | 프로젝트 상태 | 근거 |
|---|---|---|---|---|---|
| AAIController | AIModule (Runtime). 서버 전용 | Pawn 빙의, 이동 요청, BT 실행 | 몬스터 두뇌 | 템플릿 클래스만 | 소스 `Engine/Source/Runtime/AIModule/Classes/AIController.h` |
| AIPerceptionComponent | AIModule | 감각 설정·자극 기억·델리게이트 | 시야·청각·피해 감지 | 미사용 | 소스 `.../Classes/Perception/AIPerceptionComponent.h`, 문서 [AI Perception](https://dev.epicgames.com/documentation/unreal-engine/ai-perception-in-unreal-engine) |
| AISense Sight | AIModule | 반경·시야각·마지막 위치 자동 성공 | 작업등 어둠과의 관계는 별도 설계 필요 | 미사용 | 소스 `AISenseConfig_Sight.h`, `AISense_Sight.h` |
| AISense Hearing | AIModule | 소음 이벤트 감지 | 굴착·발소리·도구 소리 | 미사용 | 소스 `AISense_Hearing.h`, `AISenseConfig_Hearing.h` |
| AISense Damage | AIModule | 피해 이벤트 감지 | 피격 반응 | 미사용 | 소스 `AISense_Damage.h` |
| AISense Team | AIModule | 팀원 정보 공유 이벤트 | 몬스터 무리 경보 | 미사용 | 소스 `AISense_Team.h` |
| AISense Touch / Prediction | AIModule | 접촉 / 예측 위치 | 보조 | 미사용 | 소스 `AISense_Touch.h`, `AISense_Prediction.h` |
| AIPerceptionStimuliSourceComponent | AIModule | 자극원 등록(Pawn 아닌 액터 포함) | 플레이어·소음원 등록 | 미사용 | 소스 `AIPerceptionStimuliSourceComponent.h` |
| Behavior Tree·Blackboard | AIModule | 이벤트 구동 의사결정 트리 | **결정된 AI 방식** | 미사용 | 소스 `.../Classes/BehaviorTree/`, 문서 [BT 개요](https://dev.epicgames.com/documentation/en-us/unreal-engine/behavior-tree-in-unreal-engine---overview) |
| StateTree | StateTree 플러그인 (Runtime/UncookedOnly/DeveloperTool, `IsBetaVersion=false`·Experimental 키 없음, 버전 0.1, 기본 비활성) | 계층 상태기계 | BT 대안·보조 | 프로젝트 명시 활성 | 소스 `Engine/Plugins/Runtime/StateTree/StateTree.uplugin`, 문서 [StateTree](https://dev.epicgames.com/documentation/en-us/unreal-engine/state-tree-in-unreal-engine) |
| UStateTreeAIComponent | GameplayStateTree (Runtime, Beta·Experimental 아님) | AIController용 StateTree 실행 | StateTree를 쓸 때의 진입점 | 템플릿에서 사용 | 소스 `Engine/Plugins/Runtime/GameplayStateTree/.../Components/StateTreeAIComponent.h` |
| BT에서 StateTree 실행 | GameplayStateTree | `BTTask_RunStateTree`, `RunDynamicStateTree` | BT 틀 안에 StateTree 일부 | 미사용 | 소스 `.../GameplayStateTree/.../BehaviorTree/Tasks/` |
| EQS | AIModule 코어 + EnvironmentQueryEditor (UncookedOnly, 기본 활성) | 후보 위치·액터를 생성·시험·점수화 | 엄폐/탐색 지점, 우회 위치 | 미사용 | 소스 `.../Classes/EnvironmentQuery/`, 문서 [EQS](https://dev.epicgames.com/documentation/en-us/unreal-engine/environment-query-system-in-unreal-engine) |
| Smart Objects | SmartObjects 플러그인 (Runtime/Editor/UncookedOnly, 기본 비활성, `IsBetaVersion=false`·Experimental 키 없음) | 월드 오브젝트가 제공하는 행동 슬롯·예약 | 몬스터가 쓰는 지점(둥지·문) 후보 | 미사용 | 소스 `Engine/Plugins/Runtime/SmartObjects/SmartObjects.uplugin`, 문서 [Smart Objects](https://dev.epicgames.com/documentation/en-us/unreal-engine/smart-objects-in-unreal-engine)(랜딩 페이지만) |
| GameplayBehaviorSmartObjects / GameplayInteractions | **Experimental** | Smart Object+GameplayBehavior 연결 / 플레이어·NPC 상호작용 | 낮음 | 미사용 | 소스 각 `.uplugin` |
| GameplayBehaviors | **Beta** (Experimental 폴더) | fire-and-forget 행동 | 낮음 | 미사용 | 소스 `Engine/Plugins/Experimental/GameplayBehaviors/GameplayBehaviors.uplugin` |
| NavigationSystemV1 / RecastNavMesh | NavigationSystem (Runtime 모듈), AISupport(기본 활성)가 로드 보장 | 내비 데이터 생성·길찾기 | 몬스터 경로 | 설정만 | 소스 `.../NavigationSystem/Public/NavigationSystem.h`, `NavMesh/RecastNavMesh.h`, `AISupport.uplugin` |
| Runtime Generation (Static / DynamicModifiersOnly / Dynamic) | `ANavigationData` 속성 | 런타임 재생성 범위 | **Dynamic 결정됨** | `DefaultEngine.ini` 반영 | 소스 `NavigationData.h` |
| NavigationInvokerComponent | NavigationSystem | 액터 주변 타일만 생성 | 큰 맵에서 비용 제한 | 미사용(볼륨 방식 결정) | 소스 `NavigationInvokerComponent.h`, `NavigationSystem.h` |
| NavModifierComponent / NavModifierVolume / NavArea | NavigationSystem | 구역에 비용·금지 영역 지정 | 위험 구역 기피, 통행 불가 | 미사용 | 소스 `NavModifierComponent.h`, `NavAreas/` |
| ANavLinkProxy / Smart Link | AIModule | 점프·문 등 끊긴 경로 연결, 켜고 끄기 | 굴착 구멍 건너기, 문 | 미사용 | 소스 `AIModule/Classes/Navigation/NavLinkProxy.h`, `NavLinkCustomComponent.h` |
| 자동 NavLink 생성 | RecastNavMesh `bGenerateNavLinks` | 점프 링크 자동 생성 | Experimental 표기 | 미사용 | 소스 `RecastNavMesh.h` 주석 "Experimental" |
| Detour Crowd / RVO | Detour Crowd는 AIModule, CharacterMovement RVO는 Engine | 서로 다른 에이전트 회피 방식. `bUseRVOAvoidance`는 서버에서만 실행 | 몬스터 다수일 때 비교 | 미사용 | 소스 `Engine/Source/Runtime/AIModule/Classes/Navigation/CrowdFollowingComponent.h`, `Engine/Source/Runtime/Engine/Classes/GameFramework/CharacterMovementComponent.h` |
| Navigation Query Filter | NavigationSystem | 내비 영역별 비용·제외 여부와 Include/Exclude 플래그를 쿼리별로 지정 | 몬스터 종류별 통행 조건 후보 | 미사용 | 소스 `Engine/Source/Runtime/NavigationSystem/Public/NavFilters/NavigationQueryFilter.h` |
| Mass AI·MassGameplay | **Experimental** | 대규모 에이전트 | 이 규모엔 과함 | 미사용 | 소스 `MassAI.uplugin`, `MassGameplay.uplugin` |
| ZoneGraph, NavCorridor, WorldConditions | **Experimental** | 레인 기반 이동, 경로 회랑, 월드 조건 | 낮음 | 미사용 | 소스 각 `.uplugin` |
| HTNPlanner | **Beta** | HTN 계획기 | 낮음 | 미사용 | 소스 `HTNPlanner.uplugin` |
| Gameplay Debugger | GameplayDebugger 모듈 | 인게임 AI·내비·Perception·BT·EQS 범주 디버그 | 몬스터 디버그 기본 도구 | 미사용 | 소스 `Engine/Source/Runtime/GameplayDebugger/`, `AIModule/Private/GameplayDebugger/` |
| Visual Logger | Engine | 시간축 로그·도형 기록, AI 컴포넌트가 자동 기록 | 재현이 어려운 AI 버그 | 미사용 | 소스 `Engine/Public/VisualLogger/VisualLogger.h`, `EngineDefines.h` |
| PawnSensingComponent | AIModule | 구형 감지 | **deprecated(5.5)** 사용 금지 | 미사용 | 소스 `PawnSensingComponent.h` |

## 기능별 상세

### AIController와 서버 전용 실행
- `AAIController`: `RunBehaviorTree`, `UseBlackboard`, `MoveToActor`, `MoveToLocation`, `SetFocus`(`AIController.h`). `bWantsPlayerState` 기본은 끔(필요하면 켠다).
- 네트워크: 컨트롤러는 "네트워크 게임에서 서버에만 존재"한다고 헤더에 적혀 있다(`AIController.h`). 리슨 서버의 호스트가 권위이고 클라이언트에는 AIController·Perception·BT가 없다. 몬스터 Pawn의 위치·상태·피격 연출 복제는 [네트워크 권위](../NETWORK_AUTHORITY.md)와 [NETWORKING.md](NETWORKING.md)를 따른다.
- `bAllowClientSideNavigation`: 헤더 주석은 "false이면 클라이언트로 접속할 때 내비 충돌을 만들지 않는다"이다(`NavigationSystem.h`). 서버 전용 길찾기면 켤 이유가 없다. 기본값은 확인하지 않았다.
- 프로젝트가 이미 쓰는 서버 권위 규칙(피해는 서버 판정)과 맞는다.

### AI Perception
- 컴포넌트 위치: 문서는 AIPerception을 Pawn이 아니라 AIController에 붙이라고 한다.
- 클래스: `UAIPerceptionComponent`(`ConfigureSense`, `SetSenseEnabled`, `GetCurrentlyPerceivedActors`, `GetKnownPerceivedActors`, `ForgetAll`, `SetDominantSense`), 델리게이트 `OnPerceptionUpdated`, `OnTargetPerceptionUpdated`, `OnTargetPerceptionInfoUpdated`, `OnTargetPerceptionForgotten`.
- 감각 설정: `UAISenseConfig_Sight`(`SightRadius`, `LoseSightRadius`, `PeripheralVisionAngleDegrees`, `AutoSuccessRangeFromLastSeenLocation`, `PointOfViewBackwardOffset`, `NearClippingRadius`, `DetectionByAffiliation`), `UAISenseConfig_Hearing`(`HearingRange`, `DetectionByAffiliation`), `UAISenseConfig_Damage`, `_Team`, `_Touch`, `_Prediction`.
- 소음 보고: C++ `UAISense_Hearing::ReportNoiseEvent(World, Location, Loudness, Instigator, MaxRange, Tag)`, `Actor::MakeNoise(Loudness, Instigator, Location, MaxRange, Tag)`. 헤더 주석: `MaxRange=0`이면 거리 제한 없이 청취자 자신의 `HearingRange`만 적용, `MaxRange`가 있으면 `Loudness`가 곱해진다. `Actor::MakeNoise`는 `UAISense_Hearing`이 등록하는 기본 델리게이트를 거친다(`AISense_Hearing.cpp`). Hearing 감각을 가진 청취자가 하나라도 있어야 델리게이트가 등록되는지는 코드로 확정하지 않았다(미확인).
- 피해 보고: `UAISense_Damage::ReportDamageEvent(World, Damaged, Instigator, Amount, EventLoc, HitLoc, Tag)`. 문서는 Any/Point/Radial Damage 이벤트가 Damage 감각으로 들어간다고 적지만 연결 방식은 적지 않았다. 프로젝트의 "표준 피해 입력"(몬스터 체력 컴포넌트)과의 연결은 직접 설계·확인이 필요하다.
- 팀: `FAITeamStimulusEvent` + `UAIPerceptionSystem::OnEvent`(`AISense_Team.h`, `AIPerceptionSystem.h`). 팀 식별은 `IGenericTeamAgentInterface`(`GenericTeamAgentInterface.h`).
- 설정: 프로젝트 설정 "AISystem"의 `bForgetStaleActors`(기본 꺼짐, 끄면 만료돼도 기억), `DefaultSightCollisionChannel`, `AcceptanceRadius` 등. ini 섹션 `[/Script/AIModule.AISystem]`(`AISystem.h`, `config=Engine, defaultconfig`). 시야 성능 값은 `AISense_Sight.h`의 `MaxTracesPerTick`, `MaxTimeSlicePerTick` 등.
- 제약: Sight는 트레이스 기반으로 비용이 든다. 문서는 시야가 트레이스를 쓰는지 직접 말하지 않지만 소스에 `MaxTracesPerTick`가 있다. `HearingRange`/`LoSHearingRange`/`bUseLoSHearing` 중 **`LoSHearingRange`·`bUseLoSHearing`은 5.2부터 deprecated**(`AISenseConfig_Hearing.h`). `UpdatePerceptionWhitelist`는 5.0 deprecated(`UpdatePerceptionAllowList` 사용). `RefreshStimulus`는 5.6 deprecated.
- 어두운 환경: 시야 밝기·조명 영향은 엔진 Sight에 없다. 작업등과 연동하려면 자체 규칙이 필요하다(추정이 아니라 `AISenseConfig_Sight`의 속성 목록에 조명 항목이 없다는 소스 확인).
- 대안: 직접 구현한 거리·시야각 검사, 서버 쪽 `AIPerceptionStimuliSourceComponent`로 플레이어 등록.

### Behavior Tree·Blackboard
- 클래스: `UBehaviorTree`, `UBlackboardData`, `UBehaviorTreeComponent`, `UBlackboardComponent`(`AIModule/Classes/BehaviorTree/`). 기본 노드: Composite(Selector/Sequence/SimpleParallel), 태스크(`BTTask_MoveTo`, `Wait`, `MakeNoise`, `PlayAnimation`, `RunEQSQuery`, `RunBehavior` 등), 데코레이터(`Blackboard`, `Cooldown`, `DoesPathExist`, `ConeCheck` 등), 서비스(`DefaultFocus`, `RunEQS`).
- 문서: 이벤트 구동, 조건은 데코레이터로, Observer Aborts로 하위 우선순위 중단.
- 설정: `[/Script/AIModule.AISystem]`의 `bAddBlackboardSelfKey`, `DefaultBlackboard`.
- `BTTask_MoveTo`는 `bProjectGoalLocation`, `bRequireNavigableEndLocation`, `bAllowStrafe`, `bTrackMovingGoal` 등을 가진다(`BTTask_MoveTo.h`). 굴착 후 목표가 내비 밖일 때 동작은 실측해야 한다.
- 제약: BT 에디터 편집(기획자 직접 편집 의도)과 C++ 노드 구분은 [코딩 컨벤션](../../CODING_CONVENTION.md)의 Blueprint·C++ 경계를 따른다. BT 노드 구조는 D23에서 미설계.
- 5.x deprecated: `BehaviorTreeComponent`의 일부 함수가 5.0·5.1·5.4에서 대체됨(`RequestBranchDeactivation`, `SuspendBranchActions`, `ApplyAllSearchUpdates`로).

### StateTree·GameplayStateTree
- 모듈: `StateTreeModule`, `GameplayStateTreeModule`(Build.cs 이미 의존). `GameplayStateTree` 플러그인은 "StateTree for AI/Gameplay Behaviors"이며 `StateTree`에 의존한다. 두 플러그인 모두 `IsBetaVersion=false`, Experimental 표시 없음(소스 `.uplugin`). StateTree의 `VersionName`은 `0.1`이다. 하위 의존 `PropertyBindingUtils`는 **Beta**(소스 `.uplugin`).
- AI용 진입점: `UStateTreeAIComponent`(스키마 `StateTreeAIComponentSchema`), 태스크 `StateTreeMoveToTask`, `StateTreeRunEnvQueryTask`, `StateTreeAITask`, 조건 `StateTreeAIConditionBase`.
- `bStartLogicAutomatically`, `SetStartLogicAutomatically`, `StartLogic()` 제공. 프로젝트 템플릿은 자동 시작을 끄고 `OnPossess`에서 시작한다.
- `StateTreeRef`는 5.1에서 deprecated, `StateTreeReference` 사용(`StateTreeComponent.h`).
- 문서는 StateTree의 성숙도를 명시하지 않는다. BT와의 선택은 프로젝트 결정(BT)이 우선이다.

### EQS
- 클래스: `UEnvQuery`, 생성기(`SimpleGrid`, `OnCircle`, `Donut`, `PathingGrid`, `PerceivedActors`, `ActorsOfClass`, `Cone` 등), 시험(`Distance`, `Dot`, `Trace`, `Pathfinding`, `Overlap`, `GameplayTags`, `Volume`, `Random`, `Project`). BT에서는 `BTTask_RunEQSQuery`, `BTService_RunEQS`로 호출.
- 안정성: 문서 페이지가 5.8 문서이며 Experimental 표시가 없다. `EnvironmentQueryEditor.uplugin`은 `EnabledByDefault=true`, Type `UncookedOnly`, `AISupport` 의존. 코어는 `AIModule`이다.
- 설정: `bAllowControllersAsEQSQuerier`(AISystem, `AISystem.h`), 콘솔 `ai.debug.EQS.RefreshInterval`.
- 쓸모: "플레이어 시야에서 벗어난 지점", "굴착로 안의 매복 위치" 같은 질의. 질의 비용은 서버에서 몬스터 수에 비례한다.

### Smart Objects
- `SmartObjects` 플러그인(`EnabledByDefault=false`, Beta/Experimental 표시 없음). 의존: `GameplayAbilities`, `TargetingSystem`, `WorldConditions`(**Experimental**), `PropertyBindingUtils`(**Beta**) 등.
- 클래스: `USmartObjectSubsystem`, `USmartObjectComponent`, `USmartObjectDefinition`, `USmartObjectUserComponent`.
- 문서 페이지는 랜딩 수준이며 플러그인·멀티플레이 정보가 없다. 네트워크 복제 동작은 미확인.
- 프로젝트 쓸모는 아직 요구가 없다. 필요가 입증될 때만 조사.

### Navigation System / RecastNavMesh
- 설정 위치: Project Settings > Engine > Navigation Mesh(`[/Script/NavigationSystem.RecastNavMesh]`), Navigation System(`[/Script/NavigationSystem.NavigationSystemV1]`), Navigation 에이전트(`SupportedAgents`). 현재 프로젝트는 `RuntimeGeneration=Dynamic`만 설정했고 `TileSizeUU`·`AgentRadius` 등은 기본값이다.
- 주요 속성(`RecastNavMesh.h`): `TileSizeUU`(최소 300), `bFixedTilePoolSize`/`TilePoolSize`(문서: 스트리밍 시 고정 풀 권장), `AverageLayersPerTile`, `AgentRadius`, `AgentHeight`, `AgentMaxSlope`, `NavMeshResolutionParams`별 `CellSize`·`CellHeight`·`AgentMaxStepHeight`, `MaxSimultaneousTileGenerationJobsCount`, `TileSetUpdateInterval`, `bDoFullyAsyncNavDataGathering`, `bUseVoxelCache`, `bSortNavigationAreasByCost`, `bIsWorldPartitioned`. `CellSize`/`CellHeight`/`AgentMaxStepHeight` 단독 속성은 **deprecated**(해상도별 파라미터로 대체). `MaxSimplificationError`는 5.5부터 사용 안 함. `NavLinkJumpDownConfig`는 5.7 deprecated, `NavLinkJumpConfigs` 사용.
- 타일 크기 감각: 기본 `CellSize` 25cm, `AgentMaxStepHeight` 35cm(`FNavMeshResolutionParam` 기본값, 소스). 굴착 구체가 작을수록 영향받는 타일 수는 적지만, 타일 하나를 다시 만드는 비용은 타일 안 지오메트리 양에 비례한다. 실제 ms는 미측정이다.
- 콘솔(소스에서 확인한 변수): `ai.nav.GNavmeshSynchronousTileGeneration`, `ai.nav.NavmeshUseOodleCompression`, `ai.nav.AllowLinkGeneration`, `ai.nav.UseTightBoundExpansion`, `ai.debug.nav.DirtyAreaAroundPlayer`, `ai.debug.nav.LogInvokers`, `ai.debug.nav.DumpNavMeshMemory`, `ai.debug.nav.DumpOctreeElements`, `ai.navigation.MaxDirtyAreasCountWhileSuspended`. 일반적인 "재빌드 CVar"는 위 목록 외에 확인하지 못했다.
- 시험 보조: `RebuildNavigation` 콘솔 명령(미확인, 소스 직접 확인 못함), `NavigationSystemV1::Build()`/`RebuildAll()`은 헤더에서 확인.
- 배치: NavMeshBoundsVolume이 있어야 한다(문서 BT 개요). 볼륨 범위를 몬스터가 다닐 굴착 영역에 한정하는 것이 결정 사항이다.
- `bAutoCreateNavigationData`, `bSpawnNavDataInNavBoundsLevel`(레벨 스트리밍 시 내비 데이터가 어느 레벨에 속하는지), `bShouldDiscardSubLevelNavData`(`NavigationSystem.h`).
- 5.8 deprecated: `LogNavDataRegistrationResult`(ANavigationData* 인자 버전 사용), `LinkId` 기반 함수(5.3).

### Runtime Generation 3종
| 값 | 소스 주석 | 굴착에 대한 의미 |
|---|---|---|
| `Static` | 런타임 생성 없음 | 굴착 지형이 바뀌어도 내비 불변 |
| `DynamicModifiersOnly` | 내비 모디파이어 갱신만 지원 | 지오메트리 변화는 반영하지 않음. 굴착으로 새 통로가 생기는 경우 부족 |
| `Dynamic` | 지오메트리 변경과 모디파이어 모두 지원 | **결정됨**. 굴착 후 재생성 후보. 비용 측정 필요 |

### 굴착 후 경로 갱신
- 현재 굴착 지형: `DigetherExcavationTerrain`이 `UProceduralMeshComponent`를 쓰고 `bUseComplexAsSimpleCollision=true`, 메시를 바꿔 끼우며 충돌을 켠다(프로젝트 소스). 이 컴포넌트에 `SetCanEverAffectNavigation`을 명시한 곳은 없다. 기본값(true)에 의존한다.
- `UMeshComponent` 생성자는 `bCanEverAffectNavigation=true`를 설정하며 PMC는 이를 상속한다(소스 `Engine/Source/Runtime/Engine/Private/Components/MeshComponent.cpp`). PMC `UpdateCollision`은 충돌 쿠킹 뒤 `RecreatePhysicsState`를 호출한다. 이 호출만으로 내비 갱신을 보장한다고 보지는 않는다.
- 프로젝트의 `Prototype/World/DigetherExcavationTerrain.cpp`는 이전 메시를 `NoCollision`, 새 메시를 `QueryAndPhysics`로 전환한다. `Engine/Source/Runtime/Engine/Private/PrimitiveComponentPhysics.cpp`의 `SetCollisionEnabled` → `OnComponentCollisionSettingsChanged`는 등록된 비템플릿 컴포넌트의 내비 관련성이 바뀌면 `FNavigationSystem::UpdateComponentData`를 호출한다. `PrimitiveComponent.cpp`의 기본 관련성 조건은 Query 충돌 + Pawn 또는 Vehicle 채널 Block이며, `Engine/Source/Runtime/NavigationSystem/Private/NavigationSystem.cpp`는 이 델리게이트를 `UpdateComponentInNavOctree`에 연결한다(소스). 이 조건부 호출 경로와 실제 Dynamic 타일 재생성 성공은 구분한다. **굴착 후 경로·타일 완료 시점은 실행으로 확인할 것**.
- 수동 갱신 후보 API(헤더 확인): `FNavigationSystem::UpdateComponentData(UActorComponent&)`, `UpdateActorAndComponentData(AActor&)`, `OnComponentBoundsChanged(Comp, NewBounds, DirtyArea)`(`NavigationSystemBase.h`), `UNavigationSystemV1::UpdateComponentInNavOctree`, `AddDirtyArea(Box, Flags, DebugReason)`(`NavigationSystem.h`). 변경된 구역만 더럽히면 해당 타일만 다시 만든다.
- 비용 제어: `MaxSimultaneousTileGenerationJobsCount`, 변경 구역 한정 `AddDirtyArea`, 굴착 확정 후 한 번에 모아 갱신. 동기 생성 CVar `ai.nav.GNavmeshSynchronousTileGeneration`는 디버그용(기본 비동기).
- 내비 갱신 완료 감지: `UNavigationSystemV1::OnNavigationGenerationFinishedDelegate`(`NavigationSystem.h`).
- 이동 중인 경로가 내비 변경으로 무효화되어 재경로되는지, 목표가 굴착 구멍 안일 때 `PathFollowingComponent`가 어떻게 동작하는지는 이번에 확인하지 못했다(미확인).
- 대안: ① `Dynamic`+볼륨(결정됨) ② 굴착 시 `ANavLinkProxy`/스마트 링크로 통로 보강 ③ 내비 없이 굴착 셀 기반 자체 길찾기. ②③은 현재 결정이 아니다.

### Navigation Invoker
- `UNavigationInvokerComponent`: `TileGenerationRadius`, `TileRemovalRadius`, `SupportedAgents`, `Priority`(Experimental). 사용하려면 프로젝트 설정에서 `bGenerateNavigationOnlyAroundNavigationInvokers`를 켜야 한다(`NavigationSystem.h`). 켜면 월드 전체가 대상이 되어 옥트리 동작이 달라진다는 헤더 경고가 있다.
- 결정(제한된 볼륨)과 다르므로 채택 후보가 아니다. 맵이 커지면 비교 후보.

### NavModifier·NavArea·NavLink
- `UNavModifierComponent`(`AreaClass`, `AreaClassToReplace`, `FailsafeExtent`, `bIncludeAgentHeight`), `ANavModifierVolume`, `USplineNavModifierComponent`. 영역 클래스: `NavArea_Null`(통행 불가), `NavArea_Obstacle`, `NavArea_LowHeight`, 사용자 정의.
- 지원 정도: `RuntimeGeneration`이 `DynamicModifiersOnly` 이상이어야 런타임 모디파이어가 반영된다(소스 주석).
- `ANavLinkProxy`: 점/구간 링크 + `SmartLinkComp`(런타임 켜고 끄기, `bSmartLinkIsRelevant`), 도달 이벤트 후 `ResumePathFollowing()`으로 제어를 돌려준다(`NavLinkProxy.h`).

### Crowd / Detour avoidance
- `ADetourCrowdAIController`, `UCrowdFollowingComponent`, `UCrowdManager`. 몬스터가 여러 마리 겹칠 때 후보. 일부 속성(`AvoidanceGroup`, `GroupsToAvoid`, `GroupsToIgnore`)은 `_DEPRECATED`이며 `IRVOAvoidanceInterface` 사용. 이동 컴포넌트의 RVO 회피도 별도 존재. 디버그 CVar `ai.crowd.*`.

### Mass AI
- `MassGameplay`·`MassAI`·`MassCrowd`는 모두 `IsExperimentalVersion=true`, 기본 비활성. 수십~수천 에이전트용이며 DIGETHER의 몬스터 규모(최초 1~2종)에는 근거가 없다. 상태만 기록한다.

### Gameplay Debugger와 Visual Logger
- Gameplay Debugger: `GameplayDebugger` 모듈, AI 범주(`AI`, `BehaviorTree`, `EQS`, `Navmesh`, `Perception` 등, `AIModule/Private/GameplayDebugger/`). 활성 키 등은 `UGameplayDebuggerConfig`(`ActivationKey`). 컴파일 조건은 `Target.bUseGameplayDebugger`(`GameplayDebugger.Build.cs`)이며 Shipping 빌드에서 켜지는지는 타깃 설정에 달렸다. 기본 키는 문서로 확인하지 않았다.
- 네트워크: 서버 정보를 클라이언트 디버거로 복제하는 `GameplayDebuggerCategoryReplicator`가 있다. 리슨 서버 호스트 외에서 AI 상태를 보려면 이 경로가 필요하다.
- Visual Logger: `UE_VLOG` 등. `ENABLE_VISUAL_LOG = PLATFORM_DESKTOP && !NO_LOGGING && UE_ENABLE_DEBUG_DRAWING`(Shipping·Test에서는 에디터가 아니면 꺼짐, `EngineDefines.h`). 크래시 후 재생이 아니라 PIE·에디터 기록용이다.
- 공통 규칙: 프로젝트 로그 카테고리는 [로그 카테고리](../../design/LOG_CATEGORIES.md) 문서를 따른다.

## 보류·비추천 후보와 이유
- **PawnSensingComponent**: 5.5 deprecated. AI Perception 사용.
- **Mass AI·ZoneGraph·MassCrowd**: Experimental, 요구 규모 불일치.
- **HTNPlanner·GameplayBehaviors**: Beta. 현재 요구 없음.
- **Navigation Invoker**: 결정된 볼륨 방식과 다름. 맵이 커지면 재검토.
- **StateTree를 몬스터 두뇌로 병행**: 사용자 결정(BT)과 충돌. 양쪽 도입은 도구·디버그·스킬 비용을 늘린다. 필요하면 BT에서 StateTree 일부만 실행하는 방식이 소스에 있다.
- **자동 NavLink 생성**: 헤더가 Experimental로 표시.
- **클라이언트 측 AI·내비**: 서버 전용 설계와 충돌. 클라이언트 연출은 복제된 상태로 한다.

## 확인 한계
- 이번 재대조에서 연 Epic 5.8 문서: AI Perception, EQS, BT 개요, StateTree, Smart Objects(랜딩만). Navigation Mesh 설정은 초기 조사 참고이며 이번 재열람으로 집계하지 않았다. Gameplay Debugger·Visual Logger·Crowd·Mass·Nav Modifier 문서는 이번에 열지 않았다.
- 문서의 Runtime Generation 3종 설명은 추출되지 않아 헤더 주석으로 대신했다.
- 프로젝트의 충돌 활성 전환에서 내비 옥트리 갱신으로 이어지는 조건부 호출은 확인했지만 실제 타일 재생성, 갱신 지연 시간, 타일 비용(ms), Perception 비용은 **측정하지 않았다**.
- 리슨 서버 호스트 기준 Perception·BT 성능, 4인 접속 시 몬스터 수 한계는 미확인.
- `RebuildNavigation` 콘솔 명령과 Gameplay Debugger 기본 키는 소스에서 직접 확인하지 못했다.
- 기존 [Unreal 제공 기능 대응표](../../design/UNREAL_FEATURE_COVERAGE.md)는 DG-018에 "AIController+Perception+BT+Navigation"을 두고 StateTree/EQS를 "필요 입증 시 추가 후보"로 적었다. 이 문서와 모순은 없다. 다만 그 문서가 StateTree를 "추가 후보"로 두는 데 비해 프로젝트는 이미 StateTree·GameplayStateTree 플러그인을 활성화했고 템플릿 AI가 그것을 쓰고 있다.
- 여기 적힌 지원 사실은 프로젝트에서 동작을 확인했다는 뜻이 아니다. 검증은 [검증 계획](../../verification/VERIFICATION_PLAN.md)을 따른다.
