# 지형·파괴 기능 카탈로그

기준일 2026-11-04 · UE 5.8.4 (CL 58902052) · 상태: **조사 자료, 채택 결정 아님**. 색인은 [기능 카탈로그](README.md). 2026-11-10 부분 정정: "DIGETHER에서 쓰이는 곳" 표에 `Game/Excavation/Dain` 계열을 추가하고 여진·붕괴 행을 코드 기준으로 고쳤다. 엔진 기능 조사 내용은 기준일 그대로다.

범위: 런타임 메시 생성·갱신(ProceduralMeshComponent, DynamicMeshComponent, Geometry Scripting, MeshDescription), 충돌 쿠킹, Chaos 파괴(Geometry Collection·Fields·Anchor), Landscape, Nanite, 데칼, 비동기 생성, 지형 상태 복제. 내비 재생성은 [AI_NAVIGATION.md](AI_NAVIGATION.md), 물리·충돌 기본은 [PHYSICS_COLLISION.md](PHYSICS_COLLISION.md), 복제 일반은 [NETWORKING.md](NETWORKING.md), 저장은 [DATA_SAVE.md](DATA_SAVE.md).

경로 약칭: `ENG` = `Engine/Source/Runtime/Engine`, `PLG` = `Engine/Plugins`, `GFW` = `Engine/Source/Runtime/GeometryFramework`, `GCORE` = `Engine/Source/Runtime/GeometryCore`, `GS` = `PLG/Runtime/GeometryScripting/Source/GeometryScriptingCore`. 모두 로컬 5.8.4 설치 기준 상대 경로다.

## 요약
- **엔진에 내장 Voxel 지형 시스템은 확인하지 못했다.** `PLG` 전체 `.uplugin`에서 voxel을 이름·설명에 가진 플러그인이 없고, `Volumetrics`·`DataflowVolumetricPlugin`은 볼륨 렌더/Dataflow 용도(Editor 모듈)다. 검색 실패이지 미지원 단정은 아니다. DIGETHER는 이미 **자체 밀도 격자 + marching tetrahedra → ProceduralMeshComponent** 경로를 쓴다([굴착 구현 설계](../../design/EXCAVATION_IMPLEMENTATION.md)).
- **ProceduralMeshComponent(PMC)는 현 경로의 기반이다.** descriptor는 `IsBetaVersion=false`지만 `ProceduralMeshComponent.h`의 클래스 주석은 experimental 경고를 유지한다. 위상이 바뀌면 `CreateMeshSection*`로 재생성해야 하고(`UpdateMeshSection*`는 정점 위치·속성만), 현재 프로젝트 설정에서는 충돌 쿠킹(`CreatePhysicsMeshes`)까지 동기로 수행한다. `bUseAsyncCooking=true`는 쿠킹을 비동기로 돌리며 이전 쿠킹에 abort를 요청하는 큐가 있다.
- **DynamicMeshComponent(GeometryFramework, 엔진 모듈) + Geometry Scripting은 메시 연산 후보다.** `ApplyMeshBoolean`(구 도구 메시 뺄셈), `ApplyMeshSolidify/Morphology`(Voxel wrap/offset)가 Runtime 모듈에 있다. 단 GeometryProcessing 플러그인은 `IsBetaVersion=true`, 공식 사용 가이드는 Beta라고 쓴다. 연속 절삭 품질·시간은 측정 전이다.
- **엔진 C++ `FMarchingCubes`(GCORE)가 있다.** 자체 격자를 SDF로 보고 메시를 뽑는 C++ 전용 후보(`bParallelCompute`, `IsoValue`, `RootMode`). 청크 경계 공유 정점·결정론은 자체 marching tetrahedra가 이미 맞춰 놓은 계약이라 교체 시 재검증이 필요하다.
- **Landscape의 3D 굴착 적용은 확인하지 못했다.** `UpdateHeightfieldRegion`은 `WITH_EDITOR` 안이지만 같은 헤더에 런타임 raw height 데이터의 `WriteRuntimeData`·`CreateCollisionObject`도 있다. 따라서 이 Editor 분기만으로 런타임 변형 전체를 불가라고 단정할 수 없다. 높이맵 충돌 API 존재와 렌더·충돌이 일치하는 3D 터널 굴착 지원은 별개이며, 프로젝트 적용은 미검증이다.
- **던전 지정 벽 파괴 후보는 Geometry Collection이지만 플러그인이 Beta·기본 비활성**(`GeometryCollectionPlugin`)이고 프로젝트 uproject에 없다. 단순한 벽이면 "벽 액터를 파편 액터로 교체"도 후보다. 둘 다 비교 후 선택해야 한다.
- **복제는 메시가 아니라 정수 상태(밀도 스냅샷+revision)로 한다.** 현재 구현이 이미 그렇다. 스냅샷 배열(2,023개)이 `MaxRepArraySize` 기본 2,048에 근접해 확장 시 조각 복제가 필요하다는 점은 기존 설계 문서가 정리했다.

## DIGETHER에서 쓰이는 곳
| 관련 기능 | 프로젝트 상태 |
|---|---|
| S07·DG-002 굴착 | `Prototype/World/DigetherExcavationTerrain.{h,cpp}`: `UProceduralMeshComponent` 2개(A/B 교대), `bUseAsyncCooking=false`, `bUseComplexAsSimpleCollision=true`, 프로파일 `BlockAll`. 준비용 컴포넌트에서 `CreateMeshSection_LinearColor`→`GetBodySetup()`의 `bCreatedPhysicsMeshes`·`TriMeshGeometries` 확인 후 활성 컴포넌트 교체. **이미 사용**(단일 8m×8m 한정 볼륨, 03 패키지 검증 완료로 문서화됨, 성능·실패 주입은 미측정) |
| 지형 상태 복제 | `ADigetherExcavationTerrain`이 `Snapshot`(Schema·Revision·`TArray<uint8> Density`)을 `ReplicatedUsing=OnRep_Snapshot`로 복제, `bAlwaysRelevant=true`, `SetNetUpdateFrequency(10)`. 메시는 각 측에서 재생성. **이미 사용** |
| S07 굴착(다인 계열, 2026-11-06 이후) | `Game/Excavation/Dain/DigetherSoilField.{h,cpp}`(순수 계산: 가변 격자, 바깥 한 겹 빈 공간 고정, 구형 제거, 사면체 분할 표면)와 `DigetherDiggableSoil.{h,cpp}`: `UProceduralMeshComponent` 2개(`SoilMesh`·`SoilBack` 교대), `bUseAsyncCooking=false`, `bUseComplexAsSimpleCollision=true`, 준비 쪽에서 `CreateMeshSection_LinearColor`→`GetBodySetup()` 확인 후 교체. **복제 설정·복제 속성 없음**, 초기화·굴착은 서버 권한에서만 동작. 시험 코드는 `Game/Excavation/DainLab/`. 두 계열 중 어느 쪽이 제품 기준인지는 결정 대기다([굴착 구현 설계](../../design/EXCAVATION_IMPLEMENTATION.md#구현-계열-현황-2026-11-10)) |
| `DIGETHER.Build.cs` | `ProceduralMeshComponent` PrivateDependency, uproject에 플러그인 활성. `GeometryScriptingCore`·`GeometryFramework`·`GeometryCore`·`GeometryCollectionEngine` 의존은 **없음** |
| S09·DG-003 여진·붕괴 | 프로토타입 계열 액터가 여진 간격마다 마지막 굴착 지점 주변 밀도를 원래 값으로 즉시 되돌린다(물리 시뮬레이션 아님, `CheckTremor`→`ServerRestoreAt`). 기획 기준의 천장 판정·국소 예고 지연은 코드가 없고, 다인 계열에는 복원 연산이 없다. Chaos 파괴 **미사용** |
| DG-006 던전 벽 파괴 | **미구현·미결정**. 후보 비교는 [UNREAL_FEATURE_COVERAGE.md](../../design/UNREAL_FEATURE_COVERAGE.md) 기술 비교 표(B)에 있고 이 문서는 후보 정보만 보탠다 |
| 굴착 흔적 표현 | 데칼·Niagara 사용 코드 없음(`Source` grep). **미사용** |
| 내비 재생성 | 이 문서 범위 아님. [AI_NAVIGATION.md](AI_NAVIGATION.md) |

기존 문서와의 차이: ① 기존 설계 문서는 `CreateMeshSection`이라 쓰지만 실제 코드는 `CreateMeshSection_LinearColor`(BP 노출 이름 "Create Mesh Section")다. FColor 판 `CreateMeshSection`의 BP 노출은 `DeprecatedFunction`이다. ② 기존 대응표는 GeometryScripting을 "Beta/Experimental 모두 false"로 적었고 로컬 `.uplugin`도 그렇지만, **하위 GeometryProcessing·MeshModelingToolset은 `IsBetaVersion=true`이고 공식 가이드는 Beta라 명시**한다. 같은 플러그인 묶음의 안정성 표기가 엇갈리므로 표시가 false라는 이유로 안정판으로 보지 않는다.

## 기능 목록
| 기능 | 모듈·플러그인 (타입, 안정성) | 하는 일 | DIGETHER 쓸모 | 프로젝트 상태 | 근거 |
|---|---|---|---|---|---|
| ProceduralMeshComponent | `PLG/Runtime/ProceduralMeshComponent` (Runtime, `IsBetaVersion=false`, `EnabledByDefault=true`; 공개 클래스 주석에는 experimental 경고) | 섹션 단위 정점/인덱스 배열로 렌더+복잡 충돌 | 현 굴착 구현의 기반. 소규모 청크 재생성에 적합 | 사용 중 | 소스 `ProceduralMeshComponent.h/.cpp/.uplugin` |
| PMC 비동기 쿠킹 | 위와 동일 | `bUseAsyncCooking`: 게임 월드에서만 `CreatePhysicsMeshesAsync`, 이전 쿠킹에 abort 요청 | 충돌이 즉시 최신일 필요 없는 원거리 청크. 굴착 직후 서버 판정·캐릭터 충돌은 동기가 안전 | 미사용(false로 의도적 고정) | 소스 `UpdateCollision()`·`FinishPhysicsAsyncCook()` |
| PMC 컨벡스 단순 충돌 | 위와 동일 | `AddCollisionConvexMesh`·`SetCollisionConvexMeshes`, `bUseComplexAsSimpleCollision=false` | 굴러다니는 파편·시뮬레이션 대상용(삼각 메시 충돌은 시뮬레이션 불가) | 미사용 | 소스 헤더 주석 |
| PMC 섹션별 충돌·재질 | 위와 동일 | 섹션 `bEnableCollision`, 삼각형 `MaterialIndices`=섹션 번호 → 섹션 재질의 Physical Material | 흙/암반/지지대를 섹션으로 나눠 표면 타입을 구분 | 미사용(섹션 1개) | 소스 `GetPhysicsTriMeshData` |
| KismetProceduralMeshLibrary | 위와 동일 | 박스 생성, 탄젠트 계산, 그리드, `SliceProceduralMesh`(평면 절단) 등 | 보조. 슬라이스는 굴착 모델과 다름 | 미사용 | 소스 `KismetProceduralMeshLibrary.h` |
| DynamicMeshComponent / UDynamicMesh | `GFW`(엔진 Runtime 모듈), 기반 데이터 구조 `FDynamicMesh3`는 `GCORE/Public/DynamicMesh/DynamicMesh3.h`. 추가 메시 알고리즘은 GeometryProcessing 플러그인 `DynamicMesh` 모듈(`IsBetaVersion=true`) | 편집 가능한 메시 컴포넌트. `EditMesh`/`SetMesh`/`ProcessMesh`, `UpdateCollision`, `bUseAsyncCooking`, `bDeferCollisionUpdates`, `bEnableComplexCollision` | PMC보다 풍부한 메시 편집(부울·단순화·UV). 청크마다 컴포넌트를 두는 구조는 동일 | 미사용 | 소스 `DynamicMeshComponent.h`, `DynamicMesh3.h`, `GeometryProcessing.uplugin` |
| Geometry Scripting Core | `PLG/Runtime/GeometryScripting` (`GeometryScriptingCore` Runtime, Editor 모듈 별도; 플러그인 `IsBetaVersion=false`/`IsExperimentalVersion=false`, 의존 GeometryProcessing·MeshModelingToolset은 Beta, 공식 가이드는 Beta) | BP/C++ 메시 함수 라이브러리 | 도구 모양 부울·다듬기. 연속 절삭 품질은 실측 필요 | 미사용 | 소스 `.uplugin`·헤더, 문서 [가이드](https://dev.epicgames.com/documentation/en-us/unreal-engine/geometry-scripting-users-guide-in-unreal-engine) |
| GS 부울 | `MeshBooleanFunctions.h` (Runtime) | `ApplyMeshBoolean`(Union/Subtract/Intersect), `ApplyMeshSelfUnion`, `ApplyMeshPlaneCut/Slice/Mirror/IsoCurves` | 구형 도구 메시를 뺄셈하는 굴착 모델. 누적 연산이 느려질 위험 | 미사용 | 소스 |
| GS Voxel 함수 | `MeshVoxelFunctions.h` (Runtime) | `ApplyMeshSolidify`(winding number 기반 Voxel wrap, 내부 marching cubes), `ApplyMeshMorphology`(SDF offset) | 메시를 입력으로 하는 후처리. **밀도 격자를 직접 입력받아 메시로 바꾸는 함수는 확인하지 못했다** | 미사용 | 소스 |
| GS 에셋 입출력 | `MeshAssetFunctions.h` | `CopyMeshFromStaticMesh`, `CopyMeshToStaticMesh` 등 | `CopyMeshToStaticMesh`·`CopyMeshToSkeletalMesh`는 코드가 "Not currently supported at Runtime" 오류 반환. SourceModel LOD는 런타임 없음, RenderData LOD는 쿠킹 시 `bAllowCPUAccess` 필요 | 미사용 | 소스 `MeshAssetFunctions.cpp`, `GeometryScriptTypes.h` |
| GS Editor 전용 부분 | `MeshSimplifyFunctions.cpp`, `VolumeTextureBakeFunctions.cpp`, `CollisionFunctions.cpp` | 엔진 표준 단순화기, 볼륨 텍스처 베이크, 에셋 충돌 편집은 `WITH_EDITOR` | 함수마다 런타임 가능 여부가 다르다. 사용 함수별 패키지 시험 필요. 가이드도 일부 함수가 Editor 전용이라 쓴다 | — | 소스, 문서 가이드 |
| FMarchingCubes | `GCORE/Public/Generators/MarchingCubes.h` (엔진 Runtime 모듈, C++ 전용, UCLASS 아님) | `IsoValue`·`Bounds`·`CubeSize`·`bParallelCompute`·`RootMode`, 암시 함수에서 `FMeshShapeGenerator` 출력 | 자체 SDF→메시 후보(현재는 자체 marching tetrahedra). 청크 경계/정점 공유는 직접 관리 | 미사용 | 소스 |
| MeshDescription | `ENG` 형제 모듈 `MeshDescription`(Runtime), `StaticMeshDescription` | 메시 표현. `UStaticMesh::BuildFromMeshDescriptions`는 렌더 버퍼를 빌드하며 non-editor 빌드에서는 `FBuildMeshDescriptionsParams::bFastBuild=true` 필수 | 런타임 정적 메시 생성 후보. 반복 굴착의 비용·충돌 생성은 실측 필요 | 미사용 | 소스 `ENG/Classes/Engine/StaticMesh.h` |
| CustomMeshComponent | `PLG/Runtime/CustomMeshComponent` (Runtime, `IsBetaVersion=false`, `EnabledByDefault=true`) | `SetCustomMeshTriangles`·`AddCustomMeshTriangles`·`ClearCustomMeshTriangles`로 삼각형 렌더 메시 지정 | 표시용 후보. 공개 헤더에는 PMC의 collision-data-provider·충돌 생성 API가 없음 | 미사용 | 소스 `.uplugin`, `Source/CustomMeshComponent/Classes/CustomMeshComponent.h` |
| Chaos Geometry Collection | 런타임 `Engine/Source/Runtime/Experimental/GeometryCollectionEngine`; `PLG/Experimental/GeometryCollectionPlugin`은 Editor와 Runtime(`GeometryCollectionTracks`·`GeometryCollectionNodes`·`GeometryCollectionDepNodes`) 모듈 포함 (**Beta**, `EnabledByDefault=false`) | 사전 파쇄 메시 에셋, 클러스터·연결 그래프·피해 임계값·파괴 이벤트 | 던전 지정 벽 파괴 후보 | 미사용(uproject에 없음) | 소스, 문서 [파괴 개요](https://dev.epicgames.com/documentation/en-us/unreal-engine/destruction-overview), [GC 가이드](https://dev.epicgames.com/documentation/en-us/unreal-engine/geometry-collections-user-guide) |
| GC 복제 | `UGeometryCollectionComponent` | `bEnableReplication`, `bEnableAbandonAfterLevel`, `ReplicationAbandonAfterLevel`, `AbandonedCollisionProfileName`, `bNotifyBreaks` 외 이벤트 | 벽 파괴 상태의 동기화 옵션. 파편 물리의 결정론은 보장 안 됨 | 미사용 | 소스 |
| Fracture 도구 | `PLG/Experimental/Fracture` (**Beta**, 모듈 `FractureEngine` Runtime), `PlanarCut` | Voronoi/평면 절단 파쇄 (Fracture 모드) | 벽 에셋 제작 도구(Editor 작업) | 미사용 | 소스 `.uplugin`, 문서 GC 가이드 |
| Field System | `Engine/Source/Runtime/Experimental/FieldSystem` + `PLG/Experimental/FieldSystemPlugin` (**Beta**, Editor 모듈만) | `UFieldSystemComponent`: Radial Force/Velocity, Strain, Set Dynamic State(`ApplyStayDynamicField`), `AddFieldCommand`, `AddPersistentField` | 폭발·충격으로 벽 일부를 깨는 입력. Anchor는 Construction Field | 미사용 | 소스 |
| Anchor Field | Field System (위) | 영역 안 파쇄 조각을 Static으로 고정. 공식 가이드는 `FS_AnchorField_Generic`을 GC의 Initialization Fields에 지정하는 절차를 제공하며, C++에 `AddFieldCommand` Construction Field API도 있음 | 벽 가장자리를 붙잡아 두는 용도 | 미사용 | 소스 `FieldSystemComponent.h`, 문서 [Chaos Fields 가이드](https://dev.epicgames.com/documentation/en-us/unreal-engine/chaos-fields-user-guide-in-unreal-engine) |
| ChaosNiagara | `PLG/Experimental/ChaosNiagara` (**Beta**, Runtime, `EnabledByDefault=true`) | 파괴 데이터를 Niagara에 넘겨 2차 파편/먼지 | 파괴 연출 | 미사용 | 소스 |
| ApexDestruction | `PLG/Runtime/ApexDestruction` (Runtime, `EnabledByDefault=false`) | 구형 APEX 파괴 | 낡은 경로, Chaos가 기준 | 비추천 | 소스 `.uplugin`(설명 "APEX implementation"), 용도 판단은 추정 |
| Landscape | `ENG` 형제 `Landscape`, 항목 `LandscapeHeightfieldCollisionComponent` | 높이맵 지형, 레이어, 에디트 레이어, Nanite Landscape 컴포넌트 | 야외 외곽 후보. 3D 터널 굴착의 렌더·충돌 일치 경로는 미확인 | 미사용 | 소스 `LandscapeHeightfieldCollisionComponent.h`(`UpdateHeightfieldRegion`은 `WITH_EDITOR`), `LandscapeNaniteComponent.h` |
| Landscape 런타임 높이 충돌 데이터 | `Engine/Source/Runtime/Landscape/Classes/LandscapeHeightfieldCollisionComponent.h` | Editor 분기 밖 `WriteRuntimeData(FWriteRuntimeDataParams, ...)`·raw 높이/물리 재질 입력 `CreateCollisionObject(...)`·`RecreateCollision()` | 충돌 데이터 생성 API 조사 후보. 렌더 지형 갱신·게임 내 굴착 적용은 미확인 | 미사용 | 소스 헤더 300~335행의 선언·주석 |
| Landscape Patch | `PLG/Editor/LandscapePatch` (Runtime 모듈 `LandscapePatch` 포함, 표시 없음) | 메시에 붙여 Landscape를 에디트 레이어로 변형 | 에디터 제작용. 런타임 굴착 용도로 확인하지 못함 | 미사용 | 소스 `.uplugin`(설명), 런타임 동작은 미확인 |
| Mesh Terrain / MeshPartition | `PLG/Experimental/MeshPartition`·`MeshTerrainMode` (**Experimental**) | 메시 기반 대형 지형 저작, 비파괴 모디파이어 | 에디터 저작 중심. 런타임 굴착 용도 아님 | 비추천 | 소스 `.uplugin`, 문서 [5.8 릴리스 노트](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes?lang=en-US)("Mesh Terrain (Experimental)") |
| Nanite | `ENG` 렌더러, `NaniteDisplacedMesh`(**Beta**, `EnabledByDefault=false`, Runtime+Editor) | 정적 메시 가상 지오메트리 | 런타임 재생성 메시에는 적용 경로 미확인 | C++ 사용처 미발견, 에셋 내부 사용 미확인 | 소스 `Engine/Plugins/Experimental/NaniteDisplacedMesh/NaniteDisplacedMesh.uplugin`, `Engine/Source/Runtime/Landscape/Classes/LandscapeNaniteComponent.h`; `GFW` Public·PMC 소스의 문자열 검색에서 언급을 찾지 못한 것은 지원 여부의 근거가 아님 |
| Decal (SpawnDecal*) | `ENG` `UDecalComponent`, `UGameplayStatics::SpawnDecalAtLocation/Attached` | 디퍼드 데칼 투영(`DecalSize`, `FadeStartDelay/Duration`, `FadeScreenSize`, `SortOrder`) | 굴착 가장자리 흔적·그을음. 메시를 건드리지 않아 복제 비용 낮음(상태는 서버가 좌표로 복제 후 각 클라가 생성) | 미사용 | 소스 |
| 비동기 작업 | `Engine/Source/Runtime/Core/Public/Tasks/Task.h`·`Pipe.h` (`UE::Tasks`), `Core/Public/Async/AsyncWork.h` (`FAsyncTask`) | `UE::Tasks::Launch`로 태스크 시작, `FPipe`로 실행 겹침 방지, `FAsyncTask::StartBackgroundTask`로 스레드 풀 작업 | 격자→삼각형 변환(순수 데이터)을 워커에서, 컴포넌트 갱신은 게임 스레드. 프로젝트 적용·객체 접근 안전성은 미확인 | 미사용 | 소스 세 헤더의 API·수명 주석 본문 |
| 복제 상태 | 현재 `ReplicatedUsing=OnRep_Snapshot`; 청크 패치 후보 `FFastArraySerializer` | 서버 권위 상태 복제 | 청크 단위 패치 복제 후보 | 단일 스냅샷 사용 중, `FFastArraySerializer`는 이 지형 코드에서 미사용 | 소스 `DigetherExcavationTerrain.h/.cpp`. 상세는 [NETWORKING.md](NETWORKING.md) |

## 기능별 상세
### ProceduralMeshComponent
- 핵심: `PLG/Runtime/ProceduralMeshComponent/Source/ProceduralMeshComponent/Public/ProceduralMeshComponent.h`. 모듈 `ProceduralMeshComponent`(Runtime, LoadingPhase `PreDefault`), Editor 모듈 별도. 생성자에서 `bUseComplexAsSimpleCollision=true` 기본.
- 갱신 규칙: `UpdateMeshSection*`는 정점 수가 같은 위치/속성 갱신만, "topology를 바꿀 수 없다"(헤더 주석). 굴착처럼 위상이 바뀌면 `CreateMeshSection*`가 `UpdateCollision()`으로 충돌을 재생성한다. `UpdateMeshSection*`의 충돌 경로는 재쿠킹 대신 `BodyInstance.UpdateTriMeshVertices`로 기존 삼각 메시 정점을 갱신한다.
- 쿠킹: 동기면 `BodySetupGuid` 갱신→`InvalidatePhysicsData`→`CreatePhysicsMeshes`→`RecreatePhysicsState`를 호출한 프레임에 수행한다. 큰 청크 반복 재생성은 게임 스레드 히치 후보다(측정 전). 비동기는 `World->IsGameWorld()`에서만 켜지고 이전 큐 항목에 abort를 요청한다. `FinishPhysicsAsyncCook`은 큐에 남은 완료 항목이 성공하면 적용하고 그보다 오래된 항목을 제거하므로, 최신 요청만 적용된다고 단정할 수 없다.
- 쿠킹 성공 확인: `CreateMeshSection*`는 반환값이 없어 호출 복귀가 성공 증거가 아니다. 프로젝트는 `GetBodySetup()`의 `bCreatedPhysicsMeshes`/`bFailedToCreatePhysicsMeshes`/`TriMeshGeometries`를 확인한다.
- 설정: `bSupportUVFromHitResults`(PhysicsSettings)가 켜지면 충돌 쿠킹이 UV를 복사한다(메모리 비용). 레이트레이싱: CVar `r.RayTracing.Geometry.ProceduralMeshes`가 PMC 소스에 있다.
- 네트워크: PMC 컴포넌트 자체는 복제되지 않는다. 상태(밀도)를 복제하고 각 측에서 재생성한다.
- 주의: 삼각형 winding. 프로젝트 문서의 "원래 외적과 렌더 법선이 반대여야 Chaos cook 후 충돌 법선이 맞는다" 관측 참고(기존 설계 문서 §메시 정점 순서).
- 대안: DynamicMeshComponent(편집 기능 풍부, 복잡 충돌 `bEnableComplexCollision` 명시 필요), 정적 메시 청크 교체(프리컷).

### DynamicMeshComponent와 Geometry Scripting
- `GFW/Public/Components/DynamicMeshComponent.h`: `bUseAsyncCooking`(기본 false), `bEnableComplexCollision`(기본 false, 켜야 복잡 충돌이 생김), `bDeferCollisionUpdates`, `SetTransientDeferCollisionUpdates`, `UpdateCollision(bOnlyIfPending)`, `NotifyMeshUpdated`와 부분 갱신 함수들, `SetRenderMeshPostProcessor`. `CollisionType` 기본 `CTF_UseSimpleAsComplex`.
- **deprecated**: `EDynamicMeshComponentDistanceFieldMode`는 5.0부터 deprecated("Dynamic Mesh distance field support has been deprecated", `BaseDynamicMeshComponent.h`). Lumen 소프트웨어 SDF 트레이싱 의존 연출을 기대하지 않는다. `CopyMeshToStaticMesh`의 bUseSectionMaterials 없는 오버로드는 5.5 deprecated.
- Geometry Scripting 런타임 함수군(헤더 존재, 런타임 동작은 함수별 패키지 시험): Primitives(`Append*`, 계단 포함), Booleans, Voxel(`ApplyMeshSolidify`, `ApplyMeshMorphology`), Remesh, Simplify(클러스터/QEM은 런타임, UE 표준 단순화는 Editor), Deform, Normals/UV/Polygroup, Spatial 질의(BVH, Fast Winding Number 내부/외부), `CopyMeshFromComponent`, Mesh Pool(`GetGlobalMeshPool`).
- 스레딩: 가이드는 Actor Blueprint가 게임 스레드에서 돌고 일부 함수가 내부 태스크 스레드를 쓰지만 반환 전에 끝난다고 쓴다. `UDynamicMeshPool`은 게임 스레드 외 접근 대비 크리티컬 섹션을 둔다. UObject(`UDynamicMesh`)를 워커 스레드에서 직접 다루는 안전성은 확인하지 못했다.
- 네트워크: 컴포넌트·메시를 복제하지 않는다. 같은 입력(구 중심·반경 양자화)을 주고 각 측이 같은 결과를 낼 것이라 가정하지 않는다(부동소수점·알고리즘 버전). 결정론이 필요하면 정수 상태가 원본이어야 한다.
- 대안: 자체 SDF+`FMarchingCubes`(GCORE)로 메시 생성 후 PMC/DynamicMeshComponent로 표시.

### 밀도 격자 → 메시 (Voxel/Marching)
- 엔진 내장 Voxel 지형 시스템·청크 스트리밍은 확인하지 못했다. 보유 도구: `FMarchingCubes`(C++), GS Voxel 함수(메시 입력), Mesh Partition(Experimental, 저작용).
- 외부 Voxel 플러그인은 이번 조사 범위 밖이다. 도입은 별도 조사(기존 설계 문서가 이미 보류).
- 청크 경계 정점 단일 소유·결정론은 어느 도구든 직접 작성한다.

### Chaos 파괴 (Geometry Collection · Fields · Anchor)
- 흐름(문서): 소스 메시→Geometry Collection 생성→Fracture 모드에서 Uniform Voronoi 등으로 파쇄→클러스터링 시 시작 시 연결 그래프로 인접 조각을 묶고 충격(collision impulse)·Field가 연결 강도를 넘으면 해제. 소스 메시는 watertight이고 서로 교차하지 않아야 한다. 문서에서 상태 라벨은 발견하지 못했으나 플러그인은 Beta다.
- 런타임: `AGeometryCollectionActor`/`UGeometryCollectionComponent`(`GeometryCollectionEngine`, 엔진 Runtime). 피해 모델·`DamageThreshold`, 이벤트(`bNotifyBreaks`/`Collisions`/`Removals`/`Crumblings`), `ApplyPhysicsField`, `SetDynamicState`, 캐시, ISM 풀 렌더(`GeometryCollectionISMPool*`), 커스텀 렌더러.
- 복제: `bEnableReplication`(컴포넌트별), `ReplicationAbandonAfterLevel` 이하는 레벨 높은 입자의 갱신을 중단해 비용 절감. `ReplicationAbandonClusterLevel`은 **deprecated**(코드 내 `_DEPRECATED`)로 새 이름을 쓴다. 파편 물리가 클라이언트마다 동일한 결과를 낸다고 가정하지 않는다. 게임플레이에 영향을 주는 벽의 "열림/막힘" 상태는 서버가 별도 정수 상태로 확정하는 쪽이 안전하다(설계 판단).
- 사용 조건: GeometryCollectionPlugin이 `EnabledByDefault=false`, Beta이므로 활성화·쿠킹·패키징 경로를 먼저 시험한다. 5.8 릴리스 노트는 Chaos Destruction에 Dataflow 기본 템플릿, 변환 조회 BP 함수 등을 추가했다고 적었다.
- 사전 파쇄 벽 대안: ① Geometry Collection ② 벽 액터를 서버가 파편 액터(일반 StaticMesh 몇 개)로 교체하고 시뮬레이션 없이 고정 연출 ③ 숨김 충돌 제거+표시용 파티클. 이 선택은 [대안 비교와 사용자 선택](../../ai/EXECUTION_POLICY.md#대안-비교와-사용자-선택)에 따라 사용자가 한다.
- Anchor: 조각을 정적으로 고정하는 Construction Field. 공식 Chaos Fields 가이드의 `FS_AnchorField_Generic`·Initialization Fields 지정 절차를 확인했다. 프로젝트 에셋·실행은 미검증이다.

### Landscape
- 소스 근거: `UpdateHeightfieldRegion`은 `WITH_EDITOR` 블록이지만 `WriteRuntimeData`·raw 높이를 받는 `CreateCollisionObject`·`RecreateCollision`은 그 밖에 선언되어 있다. 높이 충돌 데이터 생성 경로는 존재한다. 이 선언만으로 렌더 높이 갱신·3D 굴착·패키지 동작까지 확인한 것은 아니다.
- Nanite Landscape 컴포넌트와 Virtual Heightfield Mesh(Experimental)는 렌더 최적화용이다.
- 쓸모: 3D 터널이 없는 외곽 지표. 굴착 대상 흙과 분리해 배치할 수 있다.

### 데칼과 굴착 흔적
- `UGameplayStatics::SpawnDecalAtLocation(WorldContext, Material, DecalSize, Location, Rotation, LifeSpan)`/`SpawnDecalAttached`. 투영 방식이라 PMC 표면에도 찍힌다고 기대되나 **어두운 광산·카툰 렌더링과의 품질은 미확인**.
- 상태 복제: 데칼 자체는 비복제 연출로 두고, 서버가 굴착 이벤트(좌표·반경·revision)를 보내 클라이언트가 만든다. 늦게 접속한 클라이언트는 이벤트를 놓치므로 흔적을 밀도 상태에서 재구성할 수 있어야 한다. 상한·페이드(`FadeStartDelay`, `FadeDuration`) 정책이 필요하다.
- 대안: 정점 색(PMC `VertexColors`)에 침식도를 굽거나 재질 파라미터.

### 비동기 생성과 게임 스레드 제약
- 분리 가능: 밀도 읽기→정점·인덱스 배열 생성(순수 데이터, `FMarchingCubes`는 내부 병렬). 불가능으로 가정: `UProceduralMeshComponent`·`UDynamicMeshComponent`·`UBodySetup` 접근, `RecreatePhysicsState`(게임 스레드).
- 도구: `UE::Tasks::Launch`(`Core/Public/Tasks/Task.h`), `FPipe`(직렬 파이프), `UBodySetup::CreatePhysicsMeshesAsync`/`AbortPhysicsMeshAsyncCreation`(`ENG/Classes/PhysicsEngine/BodySetup.h`).
- 프로젝트 정책(기존 설계): 비동기 전환은 "버전 취소·완료 통지 계약"이 검증된 뒤에만 검토한다. 이 문서는 도구 존재만 확인했다.

### 지형 데이터 복제 시 고려점
- 복제 대상은 메시가 아니라 밀도·revision(프로젝트 현 구조). 서버가 `Candidate`를 만들어 충돌 쿠킹 성공 시에만 `Snapshot`을 갱신하는 흐름이 이미 구현돼 있다.
- 한도: `MaxRepArraySize` 기본 2,048(기존 문서 근거). 배열이 이에 근접하므로 격자를 키우기 전에 조각 복제(FastArray 또는 청크 RPC)를 정한다. Iris/Replication Graph 사용 여부는 [NETWORKING.md](NETWORKING.md).
- 관련성: 현재 `bAlwaysRelevant=true`. 청크가 많아지면 관련성 정책이 필요하다.
- 합류·재접속: 지형 수신과 충돌 준비 완료 전 입장 제한은 후속 과제(기존 문서). 탐사 중 신규 참가는 금지이므로 재접속 스냅샷만 고려하면 된다.

## 보류·비추천 후보와 이유
- **Landscape를 3D 굴착 원본으로 채택**: 높이 충돌 데이터 API는 존재하지만 프로젝트의 렌더·충돌 동시 갱신과 터널 표현은 미검증.
- **MeshPartition/MeshTerrainMode**: Experimental, 저작 중심.
- **ApexDestruction**: 구형, `EnabledByDefault=false`. Chaos가 기준(용도 판단은 추정).
- **NaniteDisplacedMesh**: Beta, 정적 에셋 전용 용도(런타임 굴착 부적합 추정).
- **GeometryFlow**: `IsExperimentalVersion=true`, GeometryProcessing 의존 에디터 도구 성격.
- **ChaosRigidAsset·ChaosRigidPhysicsAsync**: Experimental.
- **외부 Voxel 플러그인**: 범위 밖. 별도 조사 전 비채택.

## 확인 한계
- 공식 문서는 열어서 확인한 페이지만 `문서`로 적었다. `chaos-destruction-overview`, `destruction-in-unreal-engine-5`, `chaos-visual-debugger-overview`는 본문 추출이 목차만 반환되어 소스로 대체했다. Landscape 개요는 런타임 수정 언급이 없었다. 이번 재검증에서 Chaos Fields 가이드의 Anchor Field 본문·설정 절차를 직접 열어 확인했다.
- 5.8 릴리스 노트는 357,273자 중 앞 부분 위주로 요약 질의했다. Geometry Scripting·Dynamic Mesh의 5.8 변경점 전수는 확인하지 못했다.
- 컴파일·쿠킹·패키징·성능은 이번에 실행하지 않았다. 헤더 존재 ≠ 패키지 동작.
- Geometry Scripting의 개별 함수 런타임 가능 여부는 위 Editor 분기 몇 건만 읽었다. 사용할 함수는 `.cpp`의 `WITH_EDITOR`와 패키지 시험으로 다시 확인한다.
- 데칼과 카툰 셰이딩 조합, Nanite와 동적 메시 조합, DynamicMeshComponent 청크의 Lumen 반응은 미확인.
