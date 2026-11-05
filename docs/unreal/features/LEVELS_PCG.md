# 레벨·PCG 기능 카탈로그

기준일 2026-11-04 · UE 5.8.4 (CL 58902052) · 상태: **조사 자료, 채택 결정 아님**. 색인은 [기능 카탈로그](README.md).

## 요약
- **프로젝트에는 아직 절차 던전 조립·스트리밍·레벨 인스턴스 사용 코드를 확인하지 못했다.** 제품·프로토타입 맵은 `L_MainMenu`, `L_Lobby`, `L_GameTest`, `L_Prototype`이고 템플릿·에디터 맵도 별도로 있다. 현재 이동은 `OpenLevel`/`ServerTravel`(비심리스)이다. 던전 조립 방식·무로딩 여부(PQ-03)는 미결정이다.
- **던전 조립 후보는 크게 셋**: ① 모듈 방 액터를 서버가 시드로 스폰 ② `ULevelStreamingDynamic::LoadLevelInstance`로 방 레벨을 위치·회전 지정해 로드 ③ PCG 그래프로 배치. 어느 쪽이든 "서버가 생성 명세(시드·방 목록)를 정하고 클라이언트는 같은 결과를 만들거나 복제로 받는다"는 규칙이 먼저다.
- **PCG 그래프 결과 전체의 자동 복제는 확인하지 못했지만, 복제 액터를 스폰하는 권한 분기는 있다.** `PCGSpawnActor.cpp`는 템플릿의 `GetIsReplicated()`가 true이면 권한이 있는 실행 소스만 스폰한다. ISM·비복제 액터와 복제 액터를 구분해야 하며, 같은 시드의 노드별 결정성·생성 결과 동기화는 별도 검증 대상이다.
- **Level Instance**(`ALevelInstance`)는 월드 에셋 배치와 네트워크 초기화 경로를 제공한다. `CookedWorldAsset`·스폰 GUID는 `COND_InitialOnly` 복제 대상으로 등록되지만 이것만으로 기본 복제 활성이나 임의 월드의 런타임 스폰이 보장되지는 않는다. `SetWorldAsset`은 Editor 전용이며, 런타임 동작은 Embedded / Standalone 구성에 달려 있다.
- **World Partition·Data Layer·OFPA**는 큰 월드용이다. DIGETHER 맵이 작다면 필수가 아니다(기존 문서도 규모 근거 없이 필수화하지 않는다고 적었다). OFPA는 템플릿 맵에만 쓰이고 `L_GameTest`는 외부 액터 폴더가 없다.
- **맵 이동 시 지속 데이터**는 레벨 기능이 아니라 인계 설계 문제다. 엔진이 자동 보존하는 범위는 seamless 여부로 다르다(상세는 [NETWORKING.md](NETWORKING.md), 설계는 [개발 시스템 설계](../../design/DEVELOPMENT_SYSTEM_DESIGN.md)의 월드 인계).

## DIGETHER에서 쓰이는 곳
| 기능 | 관련 | 프로젝트 상태 |
|---|---|---|
| 절차 생성 갱도·던전 배치(S05, DG-007) | 탐사/귀환 연결 그래프, 실패 재생성 한도 | 미사용. PCG 의존 없음 |
| 폐교·피라미드 던전·구역 전환(DG-008, PQ-03) | 무로딩 요구와 로딩 필요 여부 충돌 | 결정 대기. 맵 하나(`L_GameTest`)로 시험 |
| 거점→탐사 이동 | 대기실→게임 맵 | `DigetherSessionSubsystem`이 `ServerTravel(GameMap+"?listen", true)` 호출, `DigetherLobbyGameMode`는 `bUseSeamlessTravel=false`. PlayerController·PlayerState 클래스가 달라 비심리스로 재접속 |
| 월드 인계 | 런 상태·정산·PlayerState | 맵 분리를 하기로 정하면 별도 설계([코드 구조 계획](../../design/CODE_STRUCTURE_PLAN.md): "월드 이동은 이 계획 범위 밖") |
| 런타임 내비메시 | 굴착 영역 | 레벨 스트리밍 시 내비 데이터 소유가 걸린다 → [AI_NAVIGATION.md](AI_NAVIGATION.md) |
| 시드 | 갱도·던전 생성 | 시드 정책 미결정(추정으로 확정하지 않음). `FRandomStream`은 프로토타입 코드에서만 쓰임(`DigetherPrototypeWorld.cpp` 시험 시드, `DigetherPrototypeMineScene.cpp` 고정 시드). 방 코드 생성에 `FMath::RandRange`(`DigetherRoomProtocol.cpp`) |

## 기능 목록
| 기능 | 모듈·플러그인 (타입, 안정성) | 하는 일 | DIGETHER 쓸모 | 프로젝트 상태 | 근거 |
|---|---|---|---|---|---|
| PCG 프레임워크 | PCG 플러그인 (Runtime `PCG`, `PCGCompute`, Editor `PCGEditor`; `IsBetaVersion=false`, Experimental 표시 없음, `EnabledByDefault=true`) | 그래프로 월드에 콘텐츠 배치. 에디터/런타임 | 방·소품·광석 배치 후보 | `.uproject`에 항목 없음, Build.cs 의존 없음. 활성 여부는 에디터에서 확인 필요 | 소스 `Engine/Plugins/PCG/PCG.uplugin` |
| PCG 런타임 생성 (`GenerateAtRuntime`) | PCG | 생성 소스(플레이어·스트리밍 소스 등) 근처에서 동적 생성/정리 | 대규모 필요 시 | 미사용 | 소스 `PCGComponent.h` 열거형 `EPCGComponentGenerationTrigger`, 문서 [생성 모드](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-generation-modes-in-unreal-engine) |
| PCG Generation Trigger (OnLoad / OnDemand / AtRuntime) | PCG | 생성 시점 선택 | OnDemand = 서버가 시드 정한 뒤 호출 | 미사용 | 소스 `PCGComponent.h` |
| PCG Partitioned / Hierarchical | PCG | 격자 분할 생성, 다중 스케일 | 큰 월드용 | 미사용 | 문서(생성 모드), 소스 `PCGComponent.h` `bIsComponentPartitioned` |
| PCG GPU 실행 | `PCGCompute` 모듈, 일부 노드 `bExecuteOnGPU` | 컴퓨트 셰이더로 포인트 처리 | 대량 포인트 | 미사용 | 소스 `PCG/Source/PCGCompute`, `Elements/PCGCopyPoints.h` 등 (문서 미확인) |
| PCG 실행 컨텍스트 노드 | PCG | 권한/전용서버 여부를 그래프에서 조회 | 서버 전용 분기 | 미사용 | 소스 `Elements/PCGGetExecutionContext.h` |
| PCG Spawn Actor의 복제 액터 권한 처리 | PCG Runtime | 템플릿 액터가 복제 대상이면 `HasAuthority()`일 때만 `SpawnActors` 실행; 비복제 액터는 로컬 스폰 가능 | 서버에서 복제 액터를 생성하는 그래프 후보. 그래프 데이터·ISM 자동 복제와는 별개 | 미사용 | 소스 `Engine/Plugins/PCG/Source/PCG/Private/Elements/PCGSpawnActor.cpp` 542·590~603행 |
| PCG 확장 플러그인 | PCGBiomeCore·PCGPrimitives 등 **Experimental**, PCGGeometryScriptInterop·PCGExternalDataInterop·PCGPythonInterop **Beta** | 바이옴·외부 데이터·지오메트리 스크립트 연동 | 낮음 | 미사용 | 소스 각 `.uplugin` |
| Level Streaming (`ULevelStreaming`) | Engine | 서브레벨 로드/표시 | 방 단위 로드 | 미사용 | 소스 `Engine/Classes/Engine/LevelStreaming.h` |
| 동적 레벨 인스턴스 `ULevelStreamingDynamic::LoadLevelInstance` | Engine | 같은 레벨을 위치·회전 지정해 여러 번 로드 | 방 레벨 조립 | 미사용 | 소스 `Engine/Classes/Engine/LevelStreamingDynamic.h` |
| `UGameplayStatics::LoadStreamLevel` / `UnloadStreamLevel` | Engine | 이름 기반 스트리밍 레벨 로드 (Latent) | 고정 서브레벨 | 미사용 | 소스 `Kismet/GameplayStatics.h` |
| Level Instance 액터 `ALevelInstance` | Engine `Public/LevelInstance/LevelInstanceActor.h` | 월드 에셋을 액터로 배치. `CookedWorldAsset`·스폰 GUID는 초기 복제 대상으로 등록; `SetWorldAsset`은 Editor 전용 | 방 모듈 편집. 런타임 임의 스폰·복제 활성은 별도 검증 | 미사용 | 소스 헤더·`Private/LevelInstance/LevelInstanceActor.cpp`, 문서 [Level Instancing](https://dev.epicgames.com/documentation/en-us/unreal-engine/level-instancing-in-unreal-engine) |
| Packed Level Actor | Engine | 정적 메시 모음을 인스턴싱 BP로 묶음 | 정적 방 외관 | 미사용 | 소스 `Public/PackedLevelActor/PackedLevelActor.h`, 문서(Level Instancing) |
| Level Instance 속성 오버라이드 | Engine | 인스턴스별 속성 덮어쓰기. `ULevelInstancePropertyOverrideAsset`의 편집·직렬화 API는 `WITH_EDITOR`, 원본 월드 참조는 `WITH_EDITORONLY_DATA` | 에디터에서 변형 방 저작. 런타임 임의 편집 경로는 미확인 | 미사용 | 소스 `Engine/Source/Runtime/Engine/Public/LevelInstance/LevelInstancePropertyOverrideAsset.h` 본문 |
| World Partition | Engine | 월드 격자 분할·거리 스트리밍 | 큰 월드 | 미사용 | 문서 [World Partition](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine), 소스 `Public/WorldPartition/WorldPartition.h` |
| One File Per Actor (OFPA) | Engine | 액터를 개별 `.uasset` 바이너리 파일로 저장 | 협업 충돌 범위 감소. 같은 액터 패키지는 병합 불가 | 템플릿·에디터 맵(`Content/__ExternalActors__`) | 문서(World Partition), 프로젝트 외부 액터 파일 확인 |
| Data Layers | Engine | 액터 그룹을 런타임 상태(Unloaded/Loaded/Activated)로 전환, 상태 복제 | 상태별 구성 | 미사용 | 소스 `WorldPartition/DataLayer/DataLayerInstance.h`, `WorldDataLayers.h` |
| LevelStreamingPersistence | `Engine/Plugins/Runtime/LevelStreamingPersistence/LevelStreamingPersistence.uplugin` (Runtime, `IsBetaVersion=false`, `IsExperimentalVersion=true`, `EnabledByDefault=false`) | 스트리밍 레벨별 월드 상태 지속 | 낮음 | 미사용 | 소스 `.uplugin` |
| `FRandomStream` | Core | 시드 고정 난수열 | 시드 기반 생성 | 프로토타입만 사용 | 소스 `Engine/Source/Runtime/Core/Public/Math/RandomStream.h` |
| Seamless Travel | Engine | 전환 맵을 거쳐 이동, 일부 액터·컨트롤러 유지 | 맵 분리 시 | 미사용(`bUseSeamlessTravel=false`) | 문서 [Multiplayer Travel](https://dev.epicgames.com/documentation/en-us/unreal-engine/travelling-in-multiplayer-in-unreal-engine), 소스 `GameModeBase.h`, `PlayerState.h` |

## 기능별 상세

### PCG 프레임워크
- 핵심: `UPCGComponent`(`Generate`, `Cleanup`, `GenerateLocal`, `Seed`, `GenerationTrigger`, `bIsComponentPartitioned`), `UPCGGraph`, `UPCGSettings`, 런타임 생성 스케줄러(`RuntimeGen/PCGRuntimeGenScheduler.h`), 생성 소스(`PCGGenSourcePlayer`, `PCGGenSourceWPStreamingSource`, `PCGGenSourceComponent`, `PCGGenSourceEditorCamera`).
- 생성 트리거(`EPCGComponentGenerationTrigger`): `GenerateOnLoad`(레벨에 로드될 때), `GenerateOnDemand`(요청 시, 예: Blueprint), `GenerateAtRuntime`(런타임 생성 스케줄러가 예약). 문서도 런타임 모드가 에디터·PIE·단독 빌드에서 동작한다고 적는다.
- 런타임 생성 요건(문서): 생성 소스 4종(에디터 뷰포트, 플레이어 컨트롤러, PCG Generation Source 컴포넌트, World Partition 스트리밍 소스). World Partition이 필수라는 문장은 없다. Hierarchical은 Partitioned를 요구한다. 생성 반경은 그리드 크기별로 설정하고 커질수록 반경도 커야 한다.
- 비 WP 레벨 파티셔닝: `PCGComponent.h` 주석(`bDisableIsComponentPartitionedOnLoad`)은 "비 WP 레벨에서 파티셔닝을 지원하기 전에 분할 컴포넌트를 폐기하던 처리"라고 설명한다. 현재 비 WP 레벨에서 파티션이 지원되는 쪽으로 읽히나 실제 동작은 확인하지 않았다.
- 5.8 deprecated: `bRuntimeGenerated`("No longer used"), `GeneratedResources`(ManagedResourceContainer로 이동), 일부 `Generate`/`Cleanup` bSave 인자 버전(5.6), 검사 관련 함수 군(5.6, `FPCGGraphExecutionInspection`로 이동), 그리드 크기 인자 버전(5.8).
- 네트워크·리슨 서버: `PCGSpawnActor.cpp`는 템플릿의 `GetIsReplicated()`와 실행 소스의 `HasAuthority()`를 확인해 복제 액터의 비권한 스폰을 막는다. 따라서 모든 스폰 결과가 로컬이라고 단정할 수 없다. 그래프 데이터·ISM의 자동 복제와 프로젝트 동기화는 미확인이다. 런타임 스케줄링 정책에 `EPCGSchedulingPolicyNetworkMode`(`Client`: 로컬 생성 소스만, `Server`: 로컬 아닌 소스, `All`: 모두)가 있으며 기본값은 `All`이다(`PCGSchedulingPolicyDistanceAndDirection.h`). 그래프의 `IsDedicatedServer`/`HasAuthority` 노드도 있다. 이 소스 필터와 액터 복제 권한은 별개 조건이며 리슨 서버·원격 플레이어별 결과는 실행 검증이 필요하다.
- 결정성 주의: `Seed`(기본 42)가 컴포넌트 속성이며 동일 시드 재현은 그래프의 모든 노드가 시드를 쓸 때만 성립한다. 플랫폼·부동소수점·생성 순서(스케줄러가 플레이어 거리로 결정)에 따라 결과가 달라질 가능성이 있다. 이번에 노드별 결정성은 확인하지 못했다(미확인). 기존 대응표도 PCG가 길 연결이나 동일 네트워크 결과를 자동 보장하지 않는다고 적는다.
- GPU: `PCGCompute` 모듈과 일부 노드의 GPU 실행 옵션이 있다. 문서·지원 하드웨어·Shipping 동작은 확인하지 않았다.
- 플러그인 의존: PCG는 `EditorScriptingUtilities`, `ComputeFramework`, `GeometryProcessing`, `MeshModelingToolset`에 의존한다(소스 `.uplugin`). 패키지 크기·로드 영향은 미측정.
- 설정: PCG World Actor(그리드 크기, Treat Editor Viewport as Generation Source), 플러그인 기본 설정 파일 `Engine/Plugins/PCG/Config/DefaultPCG.ini`(내용은 읽지 않음). CVar 목록은 확인하지 않았다.
- 대안: 서버가 직접 시드로 방 모듈을 스폰하는 C++ 생성기(아래), 레벨 인스턴스 배치.

### Level Streaming과 동적 레벨 인스턴스
- `ULevelStreamingDynamic::LoadLevelInstance(WorldContext, LevelName, Location, Rotation, bOutSuccess, OptionalLevelNameOverride, OptionalLevelStreamingClass, bLoadAsTempPackage)` 또는 `LoadLevelInstanceBySoftObjectPtr`(위치·회전 또는 `FTransform`). `FLoadLevelInstanceParams` 버전은 `bInitiallyVisible`, `bAllowReuseExitingLevelStreaming`, 생성 콜백을 가진다.
- 헤더 주석: 같은 레벨을 여러 인스턴스로 만들 수 있다. 로드할 `.umap`이 패키지에 포함되도록 Packaging의 맵 포함 목록에 넣어야 한다. 짧은 이름만 주면 디스크 검색이 매우 느리다.
- **네트워크**: 헤더 주석에 "서버와 클라이언트가 같은 이름을 가져야 하므로 `OptionalLevelNameOverride`가 필요하다"고 적혀 있다. 서버에서만 로드하면 클라이언트는 자동으로 같은 인스턴스를 얻지 못한다. 클라이언트에도 같은 이름·변환으로 로드시키는 별도 경로(생성 명세 복제 + 클라이언트 로드)가 필요하다. 플레이어별 가시성은 `ClientUpdateLevelStreamingStatus`/`ServerUpdateLevelVisibility`(PlayerController)로 오가며 `ULevelStreaming::CanReplicateStreamingStatus()` 기본값은 true다(소스). 클라이언트 쪽 로드 완료를 기다리지 않고 서버가 액터를 복제하면 안 보일 수 있으므로 로드 완료 확인 절차를 설계해야 한다(추정이 아니라 `ELevelStreamingState`의 `LoadedNotVisible`/`MakingVisible` 단계 존재에서 온 주의).
- 상태 열거형: `Unloaded`, `Loading`, `LoadedNotVisible`, `MakingVisible`, `LoadedVisible`, `MakingInvisible`, `FailedToLoad`, `Removed`. 델리게이트 `OnLevelLoaded`, `OnLevelShown`. `bShouldBlockOnLoad`(프레임 정지 로드) 옵션.
- 레벨 변환(`LevelTransform`)으로 위치·회전이 정해지므로, 방 레벨은 로컬 원점 기준으로 설계한다. 내비메시는 레벨 소유 규칙(`bSpawnNavDataInNavBoundsLevel`)을 따른다([AI_NAVIGATION.md](AI_NAVIGATION.md)).
- 5.2부터 `ULevelStreaming::ECurrentState` deprecated → `ELevelStreamingState` 사용(소스).
- 문서 Level Streaming Overview는 본문이 추출되지 않아(제목만 반환) 소스 근거로 대신했다.

### Level Instance·Packed Level Actor
- `ALevelInstance`: `WorldAsset`(에디터 데이터), `CookedWorldAsset`(복제), `DesiredRuntimeBehavior` 설정. `ELevelInstanceRuntimeBehavior`: `Partitioned`(UI 이름 Embedded), `LevelStreaming`(UI 이름 Standalone). 기본은 Partitioned(`GetDefaultRuntimeBehavior`).
- 문서: Embedded는 OFPA 인스턴스의 액터를 WP 격자에 넣어 인스턴스 자체는 에디터에만 존재하며 일부 비 OFPA 액터(예: 내장 WorldSettings)는 런타임에 사라진다. Standalone(Level Streaming Mode)은 표준 서브레벨처럼 로드되며 레벨 밀도가 높으면 런타임 비용이 크다. WP 월드가 아니면 자동 스트리밍 관리가 없고 수동 레벨 스트리밍만 가능하다.
- Data Layer를 상속한다(문서). Packed Level Actor는 정적 메시 위주 건물에 적합하며 인스턴싱으로 렌더 비용을 줄인다. 편집은 모든 복사본에 반영되고 Break는 되돌릴 수 없다(문서).
- 모듈 방 제작 후보: 방 하나 = 레벨 에셋 → 에디터에서 Level Instance로 배치해 수동 조립하거나, 런타임 로드(앞 절)로 서버가 시드에 따라 조립. 둘의 차이는 에디터 배치는 정적 구성, 런타임 로드는 동적 구성이다.
- 5.3 이후 `PackedLevelActor`의 BP 생성 API 일부 deprecated → `FPackedLevelActorUtils` 사용(소스).
- 네트워크: `GetLifetimeReplicatedProps`는 `CookedWorldAsset`과 `LevelInstanceSpawnGuid`를 `COND_InitialOnly`로 등록하고 `PostNetInit`은 클라이언트 인스턴스를 등록한다. 생성자는 `bReplicates=true`를 설정하지 않으며 `SetWorldAsset`은 `WITH_EDITOR` 안이다. 따라서 복제 활성화와 cook된 월드 참조 준비를 포함한 런타임 절차 조립 경로(로딩 타이밍·내비 포함)는 미확인이다.

### World Partition·Data Layers·OFPA
- 필요 조건(문서): WP는 한 지속 레벨을 격자 셀로 나누고 스트리밍 소스(플레이어 컨트롤러 등)에 따라 로드한다. OFPA는 액터를 개별 파일로 저장해 소스 컨트롤 충돌을 줄인다. 레벨 블루프린트가 참조한 액터는 항상 로드된다. 서로 참조하는 액터는 묶여 로드된다.
- 설정: 런타임 해시 셀 크기·로딩 범위·우선순위·Block on Slow Streaming(문서). CVar `wp.Runtime.EnableServerStreaming`, `wp.Runtime.EnableServerStreamingOut`, `wp.Runtime.BlockOnSlowStreaming`, `wp.Runtime.Debug*`(소스). 서버 스트리밍 모드 열거형 `EWorldPartitionServerStreamingMode`.
- 문서에는 멀티플레이 내용이 없다. 소스상 서버 스트리밍 설정이 별도로 있다는 점만 확인했다. 리슨 서버에서 호스트와 원격 클라이언트의 스트리밍 소스 처리는 확인하지 않았다.
- Data Layer: 런타임 상태 `Unloaded`/`Loaded`/`Activated`. 서버가 상태를 바꾸면 `AWorldDataLayers`가 활성·로드 데이터 레이어 이름을 복제한다(`ReplicatedUsing=OnRep_*`). 5.3부터 `UDataLayerSubsystem::SetDataLayerRuntimeState`류는 deprecated → `UDataLayerManager` 사용.
- 템플릿·에디터 맵에서 OFPA 사용: `Content/__ExternalActors__/{ThirdPerson, Variant_*, Editor}`. `L_GameTest`·`L_Lobby`·`L_MainMenu`는 해당 폴더가 없다. 실제 외부 액터 파일도 `.uasset` 바이너리다. 액터별 파일로 분리되어 같은 맵의 편집 충돌 범위를 줄이며, 동일 패키지의 병합·동시 편집 제한은 프로젝트 자산 안전 규칙([ASSET_SAFETY](../ASSET_SAFETY.md))을 따른다.
- 필요성: 현재 근거 없음. 던전 방이 모듈이고 맵이 작으면 도입 이득이 작다. 대형 월드·협업 파일 분할이 필요해질 때 비교.

### 절차 던전 조립 방식 후보 (비교용)
| 후보 | 구성 | 장점 | 주의 |
|---|---|---|---|
| A. 서버 모듈 방 스폰 | 방 BP 액터를 서버가 시드·그래프로 스폰, 복제로 클라이언트에 전달 | 엔진 기능 의존 적음, 연결성 검사 직접 구현, 이미 쓰는 서버 권위 규칙과 같음 | 액터 수·복제 비용, 내비 갱신, 로딩 없음 |
| B. 방 레벨을 `LoadLevelInstance`로 로드 | 방 = 서브레벨, 위치·회전 지정 | 방 단위 편집·메모리 해제, 레벨 로드로 큰 에셋 분리 | 서버·클라이언트가 같은 이름·변환으로 로드해야 함, 로드 완료 동기화, 내비 소유 |
| C. `ALevelInstance` 런타임 스폰 | cook된 월드 참조와 복제 설정을 준비한 레벨 인스턴스 액터 | 초기 속성 복제·클라이언트 등록 경로가 엔진에 있음 | `SetWorldAsset`은 Editor 전용. 런타임 절차 스폰 용도로 검증된 자료를 확인하지 못함 |
| D. PCG 그래프 | 포인트로 소품·방 배치 | 시각적 편집, 대량 배치; 복제 액터의 권한 스폰 분기 제공 | 그래프 데이터·ISM 자동 복제 미확인, 결정성·길 연결은 별도 검증, Beta/Experimental 연동 플러그인 주의 |
- 실제 선택은 [대안 비교와 사용자 선택](../../ai/EXECUTION_POLICY.md#대안-비교와-사용자-선택)을 따른다. 기획 확인이 필요한 값(방 크기·수, 구역 전환 연출, 로딩 허용)은 확정하지 않는다([결정 목록](../../planning/DECISIONS.md) PQ-03). D23은 몬스터 선정 미결로 구역 전환 결정이 아니다.

### 시드와 FRandomStream
- `FRandomStream`(Core): `Initialize(int32)`, `Reset()`, `GenerateNewSeed()`, `GetCurrentSeed()`, `RandRange`. 같은 시드에서 같은 순서로 호출하면 같은 수열이다. 호출 순서가 바뀌면 결과가 달라지므로 생성 단계마다 별도 스트림을 두는 방식이 안전하다(일반 원리이며 이번 소스 확인 항목 아님).
- 서버가 시드를 정하고 클라이언트에 복제/RPC로 전달하면 클라이언트가 같은 결과를 재생성하는 방식과, 서버만 생성하고 결과를 복제하는 방식이 있다. 어느 쪽을 택할지는 미결정이며, 클라이언트 재생성은 부동소수점·플랫폼·엔진 버전 차이에 민감하다. 재접속(같은 ID 허용) 시 시드·생성 명세를 다시 전달해야 한다.
- PCG는 컴포넌트 `Seed`와 그래프 내 시드를 쓴다(소스). `AISystem`의 AI 난수 스트림은 별개다(`UAISystem::RandomStream` 정적).

### 멀티플레이에서 스트리밍 레벨과 복제 주의
- 서브레벨의 액터를 서버가 로드하면 클라이언트가 해당 레벨을 이미 로드·표시한 뒤에야 그 액터 복제가 의미를 가진다. 이를 위해 `PlayerController`가 레벨 가시성을 서버에 보고하는 경로가 있다(`ServerUpdateLevelVisibility`).
- 네트워크 관련성·휴면 같은 일반 사항은 [NETWORKING.md](NETWORKING.md)를 따른다.
- 신규 참가 불가·같은 ID 재접속 허용 정책 때문에, 재접속 클라이언트는 이미 조립된 던전 구성을 받아야 한다. 현재 정책은 [네트워크 권위](../NETWORK_AUTHORITY.md)에서 확인한다.
- 비심리스 이동은 클라이언트가 끊겼다가 같은 서버에 다시 접속하는 방식이다(문서).

### Seamless Travel과 맵 이동 시 지속 데이터
- 문서: Seamless는 전환 맵(`UGameMapsSettings::TransitionMap`, 비워 두면 빈 맵 자동 생성)을 거치며 `AGameModeBase::bUseSeamlessTravel=true`로 켠다. 비심리스는 첫 로드·첫 접속·새 멀티플레이 종료 때 필수이다.
- 서버에서 지속되는 것(문서): 게임 모드(서버)와 `GetSeamlessTravelActorList`로 추가한 액터, 유효한 PlayerState를 가진 컨트롤러, 모든 PlayerController(서버), 로컬 PlayerController(서버·클라이언트).
- 상태 인계 훅: `APlayerState::CopyProperties`/`OverrideWith`/`SeamlessTravelTo`, `AGameModeBase::GetSeamlessTravelActorList`, `HandleSeamlessTravelPlayer`, `PostSeamlessTravel`, `APlayerController::GetSeamlessTravelActorList`(소스).
- 프로젝트: 대기실→게임은 `bUseSeamlessTravel=false`로 비심리스를 의도적으로 선택(컨트롤러·PlayerState 클래스가 달라서, 코드 주석). 게임 모드 쪽 seamless 설정은 확인하지 않았다. 런 간 데이터는 서버 `GameInstanceSubsystem`에 값 스냅샷으로 인계하는 후보가 설계 문서에 있다.
- 상세와 검증은 [NETWORKING.md](NETWORKING.md)를 따른다.

## 보류·비추천 후보와 이유
- **World Partition·HLOD 도입**: 맵 규모 근거 없음. 도입 시 OFPA·스트리밍 소스·내비 분할을 함께 다뤄야 한다.
- **PCG 확장 플러그인(Biome/Primitives)**: Experimental. GeometryScript·ExternalData·Python 연동은 Beta.
- **PCG를 길 연결 보장 도구로 사용**: 엔진이 보장하지 않는다. 별도 연결성 검사 필요.
- **LevelStreamingPersistence**: Experimental.
- **클라이언트 단독 절차 생성에 게임플레이 판정 의존**: 결과 동일성 보장 확인 전에는 서버 권위 명세를 쓴다.
- **레벨 이동으로 구역 전환 확정**: PQ-03 결정 전까지 확정하지 않는다.

## 확인 한계
- 읽은 Epic 문서: PCG 생성 모드, Level Instancing, World Partition, Multiplayer Travel. PCG 개요와 Level Streaming 개요는 본문이 추출되지 않아 소스로 대신했다. Data Layers·Level Instance 속성 오버라이드·PCG GPU 문서는 읽지 않았다.
- PCG의 노드별 결정성, GPU 지원 범위, 리슨 서버 동작, 런타임 생성 비용은 확인·측정하지 않았다.
- 서버 `LoadLevelInstance` 후 클라이언트가 어떻게 같은 인스턴스를 얻는지는 헤더 주석의 이름 일치 요구만 확인했고 동작은 실행하지 않았다.
- PCG는 `.uproject`에 없으나 descriptor `EnabledByDefault=true`이므로 실제 활성 상태는 에디터 플러그인 창에서 확인해야 한다(Launcher 설치 소스는 `PCG` 모듈이 이 프로젝트 모듈 의존에 없음을 보여 준다).
- 기존 [대응표](../../design/UNREAL_FEATURE_COVERAGE.md)와의 차이는 없다. 다만 그 표는 PCG 런타임 생성을 세부 확인하지 않았고, 이 문서는 5.8 소스에서 `GenerateAtRuntime` 트리거와 생성 소스 구성을 추가로 확인했다. 존재 사실은 프로젝트 검증과 다르다.
