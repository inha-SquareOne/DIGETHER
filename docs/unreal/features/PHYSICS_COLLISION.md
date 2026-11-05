# 물리·충돌 기능 카탈로그

기준일 2026-11-04 · UE 5.8.4 (CL 58902052) · 상태: **조사 자료, 채택 결정 아님**. 색인은 [기능 카탈로그](README.md).

범위: Chaos 물리 기본, 충돌 채널·오브젝트 타입·프로파일, 트레이스(동기·비동기), Hit/Overlap 이벤트, Physical Material·Surface Type, PhysicsHandle·Constraint, 시뮬레이션 복제(Networked Physics), Physics Field, Chaos Visual Debugger, 캐릭터 침투 처리, 성능 주의. 지형 메시 충돌 쿠킹은 [TERRAIN_DESTRUCTION.md](TERRAIN_DESTRUCTION.md), 캐릭터 이동은 [GAMEPLAY_FRAMEWORK.md](GAMEPLAY_FRAMEWORK.md), 복제 일반은 [NETWORKING.md](NETWORKING.md), 디버그·시험은 [DEBUG_TEST.md](DEBUG_TEST.md).

경로 약칭: `ENG` = `Engine/Source/Runtime/Engine`, `PLG` = `Engine/Plugins`. 모두 로컬 5.8.4 설치 기준 상대 경로다.

## 요약
- **프로젝트는 커스텀 충돌 채널·프로파일을 아직 정의하지 않았다.** `DefaultEngine.ini`에 `CollisionProfile` 섹션이 없고 코드는 `ECC_Visibility`(상호작용·굴착 조준 trace)·`ECC_Pawn`과 엔진 기본 프로파일(`BlockAll`·`BlockAllDynamic`·`NoCollision`·`OverlapAllDynamic`)만 쓴다. 굴착 조준, 도구 타격, 몬스터 감지, 투사체 등을 구분하려면 `ECC_GameTraceChannel1~18`에 의미를 붙이는 설계가 필요하다(헤더에 18개 존재, 아직 사용 안 함).
- **트레이스는 동기 호출이 기본이고 비동기 트레이스 API도 있다.** `UWorld::AsyncLineTraceByChannel/ObjectType/Profile`, `AsyncSweep*`, `AsyncOverlap*`(`EAsyncTraceType`: Test/Single/Multi). 서버 권위 굴착 판정은 동기 trace가 단순하다.
- **Physical Material 반환은 질의의 `bReturnPhysicalMaterial` 또는 이동 컴포넌트의 `bReturnMaterialOnMove` 설정이 필요하다.** `PrimitiveComponent.cpp`가 이동용 설정을 query params로 전달한다. 발소리·도구 반응은 `UPhysicalMaterial::SurfaceType`(프로젝트 설정 `PhysicalSurfaces`에 이름 등록)으로 분기한다. 굴착 메시가 PMC이면 섹션 번호가 삼각형 재질 인덱스가 되므로 섹션별 재질로 표면 타입을 나눌 수 있다. 현재 프로젝트는 미사용이다.
- **Networked Physics(Physics Prediction)는 Experimental 성격이다.** `bEnablePhysicsPrediction` 주석이 "experimental"이고 `EPhysicsReplicationMode::Resimulation`은 표시명이 "(WIP)"이다. 비용도 크다. 아이템이 "물리 충돌 없이 동작"하는 현재 기획 방향이면 필요 없다.
- **Chaos Visual Debugger(ChaosVD)는 Beta이며 Shipping 빌드에서는 기본으로 빠진다**(`bCompileChaosVisualDebuggerSupport` 기본: Shipping 제외 + trace 활성). 4인 호스트형 충돌·침투 재현 디버깅에 쓸 수 있다.
- **깔림·밀어내기는 엔진의 침투 해소(depenetration)와 별개로 서버 규칙이 필요하다.** CMC의 `MaxDepenetrationWithGeometry/Pawn`·`RepulsionForce`·`PushForceFactor`, `FHitResult.bStartPenetrating/PenetrationDepth`는 위치 보정 도구이지 피해/매몰 판정이 아니다. 프로젝트는 이미 `RequestWorldBurialPawn` 같은 서버 규칙으로 처리한다.

## DIGETHER에서 쓰이는 곳
| 관련 기능 | 프로젝트 상태 |
|---|---|
| 상호작용·조준 trace | `LineTraceSingleByChannel(…, ECC_Visibility, Params)`를 `Prototype/Player/DigetherPrototypeCharacter.cpp`·`PlayerController.cpp`·`GameMode.cpp`·`World/*.cpp`에서 사용. 캡슐은 `ECC_Visibility` Block(살아 있을 때), Floor는 Visibility Ignore. **이미 사용** |
| 굴착 서버 trace | `PlayerController.cpp`의 서버 굴착 trace(`SCENE_QUERY_STAT(PrototypeServerTerrain)`): 서버가 Visibility trace로 첫 차단 대상을 재판정. **이미 사용** |
| 안전 내려놓기·구조 위치 | `GameMode.cpp`: `SweepSingleByChannel`(구/캡슐), `OverlapBlockingTestByChannel`, `SweepSingleByProfile`, `bStartPenetrating`·`ImpactNormal.Z` 검사. `Portal.cpp`: `OverlapAnyTestByObjectType`(`ECC_Pawn`). **이미 사용** |
| 엘리베이터 탑승 영역 | `Game/Elevator/DigetherElevatorActor.cpp`: `QueryOnly`+모든 채널 Ignore+`ECC_Pawn` Overlap. **이미 사용** |
| 시체 | `Prototype/DigetherPrototypeCorpse.cpp`: `QueryOnly`, `ECC_Visibility`만 Block, `SetReplicateMovement(true)`, 물리 시뮬레이션 없음 |
| 지형 충돌 | PMC 복잡 충돌(`CTF_UseComplexAsSimple`), 프로파일 `BlockAll`. [TERRAIN_DESTRUCTION.md](TERRAIN_DESTRUCTION.md) |
| 템플릿 코드 | `Variant_Combat`·`Variant_Platforming`·`Variant_SideScrolling`(UE 템플릿 유래): `SweepMultiByObjectType`, `SetSimulatePhysics(true)`(래그돌·상자), `OnComponentHit`/`OnComponentBeginOverlap`. DIGETHER 기능과 별개이며 제품 채택 아님 |
| Physical Material·Surface Type | `Source`에서 `PhysicalMaterial`·`SurfaceType` 사용처 없음. **미사용** |
| 커스텀 채널·프로파일 | `DefaultEngine.ini`에 없음. **미정의** |
| 물리 시뮬레이션 복제 | 제품 코드에서 사용처를 확인하지 못함(`bReplicateMovement`는 Corpse·Character·일부 월드 액터에서 켬). 템플릿에는 래그돌 외 `CombatDummy`의 시뮬레이션 메시·Constraint도 있음 |
| 아이템 | 기획 방향은 "아이템은 물리 충돌 없이 동작"([UNREAL_FEATURE_COVERAGE.md](../../design/UNREAL_FEATURE_COVERAGE.md)의 운반 항목, 아이템 충돌 없음 문장), 강체 물리 운반은 현 기획 요구가 아님 |
| 내비 | 충돌 변경 후 내비 갱신은 [AI_NAVIGATION.md](AI_NAVIGATION.md) |

## 기능 목록
| 기능 | 모듈·플러그인 (타입, 안정성) | 하는 일 | DIGETHER 쓸모 | 프로젝트 상태 | 근거 |
|---|---|---|---|---|---|
| Collision Channel / Object Type | `ENG` `EngineTypes.h` (`ECollisionChannel`) | 엔진 채널: WorldStatic/WorldDynamic/Pawn/PhysicsBody/Vehicle/Destructible/Visibility/Camera, `ECC_GameTraceChannel1~18` 사용자용 | 굴착 가능 흙, 도구 타격, 상호작용, 몬스터 시야 등 의미 분리 | 엔진 채널만 사용 | 소스 `EngineTypes.h`, 문서 [충돌 개요](https://dev.epicgames.com/documentation/en-us/unreal-engine/collision-in-unreal-engine---overview) |
| Collision Profile | `Config/BaseEngine.ini`(엔진 기본 프로파일), Project Settings → Collision → Preset | 오브젝트 타입+채널별 응답 묶음. `SetCollisionProfileName` | 흙/암반/플레이어/몬스터/상호작용 볼륨 표준화 | 엔진 프로파일 이름만 사용 | 소스 `BaseEngine.ini`, 프로젝트 `Source` |
| Line/Sweep/Overlap 동기 | `UWorld` (`LineTraceSingle/MultiByChannel/ObjectType/Profile`, `Sweep*`, `Overlap*`, `OverlapBlockingTestByChannel`, `OverlapAnyTestByObjectType`) | 단발 쿼리. `FCollisionQueryParams`(`bTraceComplex`, `bReturnPhysicalMaterial`, `bReturnFaceIndex`, `bFindInitialOverlaps`, 무시 액터) | 서버 권위 조준·충돌 판정 | 사용 중 | 소스 `ENG/Classes/Engine/World.h`, `CollisionQueryParams.h`, 문서 [트레이스](https://dev.epicgames.com/documentation/en-us/unreal-engine/traces-with-raycasts-in-unreal-engine) |
| 비동기 트레이스 | `UWorld::Async*` (`EAsyncTraceType`: Test/Single/Multi), 결과 `FTraceDatum` | 요청한 바로 다음 프레임에 결과 사용 가능. 델리게이트 또는 `QueryTraceData`·`QueryOverlapData`로 수신 | 다수 감지 질의 분산. 서버 판정 즉시성이 필요하면 부적합 | 미사용 | 소스 `World.h`의 비동기 API 주석, `WorldCollision.h` |
| Hit 이벤트 | `UPrimitiveComponent::OnComponentHit`, `AActor::ReceiveHit`, `FBodyInstance::bNotifyRigidBodyCollision` | 이동 sweep·캐릭터 이동·물리 시뮬레이션의 차단 알림. 물리 시뮬레이션 Hit에는 Simulation Generates Hit Events 필요; sweep Hit의 `NormalImpulse`는 0 | 파편·시체 충돌음. 서버 판정 기준으로는 쓰지 않음 | 템플릿만 사용 | 소스 `ENG/Classes/Components/PrimitiveComponent.h`, 문서 충돌 개요 |
| Overlap 이벤트 | `OnComponentBeginOverlap/EndOverlap`, `SetGenerateOverlapEvents`, Overlap 응답 | 영역 진입/이탈 | 엘리베이터 탑승 영역 등. 이벤트 대신 질의로 상태를 확인하는 방식이 이미 있음 | 질의 방식 사용 | 소스, 문서 충돌 개요 |
| Physical Material / Surface Type | `PhysicsCore` `UPhysicalMaterial`(Friction/Restitution/Density/`SurfaceType`), `UPhysicsSettings::PhysicalSurfaces` | 재질 속성과 표면 타입. `HitResult.PhysMaterial` | 발소리·도구 타격음/파티클·도구 효율 판정 | 미사용 | 소스 `PhysicalMaterial.h`, `PhysicsSettings.h`, `HitResult.h`; 문서는 개요만 [물리 머티리얼](https://dev.epicgames.com/documentation/en-us/unreal-engine/physical-materials-in-unreal-engine) |
| Physical Material Mask | `ENG/Classes/PhysicalMaterials/PhysicalMaterialMask.h` | 한 렌더 재질에 여러 물리 재질 매핑. `UVChannelIndex`, `GenerateMaskData`, `GetPhysMatIndex`; 원본 `MaskTexture`는 EditorOnly 데이터 | 정적 메시 내 부분 표면. 런타임 PMC에는 섹션 분할이 더 직접적 | 미사용 | 소스 헤더 본문·Editor 분기 |
| SurfaceEffects 플러그인 | `PLG/Experimental/SurfaceEffects` (Runtime, **Experimental**) | "context-driven surface system" | 표면별 효과 매핑 | 미사용 | 소스 `.uplugin`(설명만) |
| Chaos 기본 | `ENG` + `Engine/Source/Runtime/Experimental/Chaos*`, 플러그인 `ChaosSolverPlugin`(Editor 모듈만, Beta, `EnabledByDefault=true`) | 기본 물리 엔진. `BodyInstance`, `bUseCCD`, `bSmoothEdgeCollisions`, `bContactModification` | 시뮬레이션 대상이 거의 없어 기본 설정으로 충분 | 기본 사용 | 소스 `BodyInstance.h`, `.uplugin` |
| 물리 틱 | Project Settings → Physics → Framerate (`bTickPhysicsAsync`, `bSubstepping*`) | 비동기 물리 틱. 헤더 주석: "still experimental" | 필요 시만. Physics Prediction은 이 설정이 켜져야 동작 | 기본값 | 소스 `PhysicsSettings.h` |
| Async Physics Tick | `AActor::bAsyncPhysicsTickEnabled`, `UActorComponent::AsyncPhysicsTickComponent`, `UAsyncPhysicsInputComponent` | 물리 틱에 맞춘 콜백. `ActorComponent.h`는 콜백이 게임 스레드에서 실행되며 게임/물리 스레드 전체 동기를 유발한다고 명시 | 물리 기반 조종에만 | 미사용 | 소스 `Actor.h`, `ActorComponent.h`, `ENG/Public/Physics/AsyncPhysicsInputComponent.h` |
| PhysicsHandleComponent | `ENG/Classes/PhysicsEngine/PhysicsHandleComponent.h` | 물체를 끌어 잡기(`GrabComponentAtLocation[WithRotation]`, `SetTargetLocation`, 보간 속도) | 시뮬레이션 물체를 들고 이동. 현 기획은 물리 없는 운반이므로 대상 아님 | 미사용 | 소스. 구 `GrabComponent`는 **deprecated 4.14** |
| PhysicsConstraintComponent | `ENG/Classes/PhysicsEngine/PhysicsConstraintComponent.h`, `ConstraintInstance.h` | 두 강체 사이 관절/스프링, `SetConstrainedComponents` | 매달린 문, 흔들리는 랜턴 연출 | 템플릿 `Variant_Combat/Gameplay/CombatDummy.cpp`에서 사용, 제품 기능 채택은 아님 | 소스 헤더·프로젝트 생성자 28~31행 |
| RadialForce / Thruster | `ENG/Classes/PhysicsEngine/RadialForceComponent.h`, `PhysicsThrusterComponent.h` | `URadialForceComponent::FireImpulse`·`ForceStrength`·`ImpulseStrength`로 반경 힘/충격, `UPhysicsThrusterComponent::ThrustStrength`로 로컬 -X 방향 추력 | 폭발 연출(굴착 규칙과 별개) | 미사용 | 소스 두 헤더의 API·주석 본문 |
| Physics Field (Chaos Fields) | `ENG/Classes/PhysicsField/PhysicsFieldComponent.h` + `Engine/Source/Runtime/Experimental/FieldSystem` (`FieldSystemPlugin` Beta, Editor 모듈만) | 월드 영역에 힘/속도/상태 필드 적용 | Geometry Collection 파괴 입력 등. [TERRAIN_DESTRUCTION.md](TERRAIN_DESTRUCTION.md) | 미사용 | 소스 |
| Networked Physics | `ENG` `Physics/NetworkPhysicsComponent.h`, `NetworkPhysicsSettingsComponent.h`, `EPhysicsReplicationMode`(Default/PredictiveInterpolation/Resimulation(WIP)/None), Project Settings → Physics → `PhysicsPrediction` | 시뮬레이션 복제. Resimulation은 서버와 어긋나면 되감아 재시뮬레이션 | 협동 중 서로 밀어야 하는 시뮬레이션 물체가 요구될 때만 | 미사용 | 소스 `PhysicsSettings.h`, `EngineTypes.h`, 문서 [개요](https://dev.epicgames.com/documentation/en-us/unreal-engine/networked-physics-overview) |
| 기본 물리 복제 | `AActor::ReplicatedMovement`(`FRepMovement.bRepPhysics`), `SetReplicateMovement`, `PhysicsReplicationMode` | 서버 시뮬레이션 위치/속도를 클라이언트에 전달·보정 | 시체/드롭을 시뮬레이션할 경우 | 미사용(비시뮬레이션 액터에 이동 복제만) | 소스 `Actor.h` |
| NetworkPrediction 플러그인 | `PLG/Runtime/NetworkPrediction` (Runtime, **Beta**, `EnabledByDefault=false`) | 일반 예측 프레임워크. Mover의 기반 | 이 문서 대상은 아님 | 미사용 | 소스 `.uplugin` |
| Mover / ChaosMover | `PLG/Experimental/Mover`, `ChaosMover` (**Experimental**) | CMC 후속 이동 프레임워크, Chaos 기반 캐릭터 이동 | CMC 대체 후보. 상세는 GAMEPLAY_FRAMEWORK | 미사용 | 소스 `.uplugin` |
| CharacterMovement 침투·밀기 | `UCharacterMovementComponent`: `MaxDepenetrationWithGeometry[AsProxy]`, `MaxDepenetrationWithPawn[AsProxy]`, `bEnablePhysicsInteraction`, `PushForceFactor`, `InitialPushForceFactor`, `RepulsionForce`, `TouchForceFactor`, `StandingDownwardForceScale`, `ApplyRepulsionForce` | 캡슐 침투 해소의 최대 보정 거리(cm), 물리 물체 밀기, 다른 폰 반발 | 구덩이 안 캐릭터 끼임·밀림 완화 | 기본 CMC(값 미조정) | 소스 `CharacterMovementComponent.h`의 거리 주석·`ForceUnits=cm` |
| 침투 질의 | `UMovementComponent::ResolvePenetration[Impl]`, `UPrimitiveComponent::ComputePenetration`(MTD), `FHitResult.bStartPenetrating/PenetrationDepth` | 침투 방향/깊이 계산 | 지형 갱신 직후 끼인 폰 복구 | 프로젝트가 `bStartPenetrating` 일부 사용 | 소스 |
| BodyInstance CCD·깊은 침투 | `FBodyInstance::bUseCCD`, `MaxDepenetrationVelocity` 오버라이드, 프로젝트 설정 `CollisionInitialOverlapDepenetrationVelocity`(참조 주석) | 고속 물체 터널링 방지, 초기 겹침 해소 속도 | 투사체/드롭 아이템이 얇은 메시를 통과하는 경우 | 미사용 | 소스 `BodyInstance.h` |
| Complex / Simple 충돌 | `ECollisionTraceFlag`(`CTF_UseComplexAsSimple`…), `FCollisionQueryParams::bTraceComplex` | 삼각 메시 충돌 vs 단순 도형 | 굴착 지형은 복잡 충돌(시뮬레이션 불가), 소품은 단순 도형 | 지형은 복잡 충돌 | 소스 `ProceduralMeshComponent.h` 주석, `CollisionQueryParams.h` |
| Chaos Visual Debugger | `PLG/ChaosVD` (**Beta**, `IsExperimentalVersion=false`, `EnabledByDefault=true`, 모듈 `ChaosVD`/`ChaosVDBuiltInExtensions`: EditorAndProgram, `ChaosVDBlueprint`: RuntimeAndProgram), 런타임 `ChaosVisualDebugger/ChaosVDRuntime` | 물리 시뮬레이션·쿼리 기록/재생. `p.Chaos.StartVDRecording`, `-StartCVDRecording`, `-CVDHostAddress` | 침투·끼임·터널링 재현 분석. 서버 기록 지원 | 미사용 | 소스 `.uplugin`·`TargetRules.cs`, 문서 [CVD 가이드(5.5판 페이지)](https://dev.epicgames.com/documentation/en-us/unreal-engine/chaos-visual-debugger-user-guide?application_version=5.5) |
| Chaos Insights | `PLG/ChaosInsights` (Beta, EditorAndProgram) | Unreal Insights용 Chaos 프로파일 | 물리 비용 분석 | 미사용 | 소스 `.uplugin` |
| Physics Control | `PLG/Animation/PhysicsControl` (Runtime, `IsBetaVersion=false`/`IsExperimentalVersion=false`) | 스켈레탈/스태틱 메시의 물리 제어, Rigid Body With Control | 캐릭터 물리 연출. 5.8 노트에 "ready for production" | 미사용 | 소스 `.uplugin`, 문서 [5.8 노트](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes?lang=en-US) |

## 기능별 상세
### 충돌 채널·오브젝트 타입·프로파일
- 모델(문서): 모든 충돌체는 Object Type과 채널별 응답(Block/Overlap/Ignore)을 갖는다. 서로 Block이어야 물리 충돌이 일어나고, Overlap과 Block 조합은 Overlap만 발생하며, Overlap과 Ignore 조합은 Overlap 이벤트가 없다고 문서가 적는다. 트레이스도 Trace Response를 따로 가진다.
- 사용자 채널: `ECC_GameTraceChannel1~18`(`EngineTypes.h`). Project Settings → Collision에서 이름을 붙이면 `DefaultEngine.ini`의 `[/Script/Engine.CollisionProfile]`에 `+DefaultChannelResponses=`/`+EditProfiles=`가 기록된다(일반 엔진 동작, 이번엔 프로젝트에서 확인할 설정이 없음). 5.8 노트: TraceType/ObjectType 스크립트 이름이 채널 DisplayName에서 자동 생성되도록 바뀜.
- 주의: 채널 번호는 `ECC_GameTraceChannelN`이 코드에서 숫자로 남으므로 `#define`/enum 래퍼를 두고 이름을 문서화한다. 네이티브 `DIGETHER` 모듈 한 곳에 상수화하는 것이 기존 템플릿 주석(`#define COLLISION_WEAPON ECC_GameTraceChannel1`)의 관례이다.
- 설계 쟁점(결정 아님): 지금은 모든 trace가 `ECC_Visibility`라서 시체가 `Visibility` Block(조준 대상), 바닥은 Visibility Ignore 같은 예외가 컴포넌트마다 코드에 흩어져 있다. 채널을 분리하면 이 예외가 프로파일로 모인다.
- 대안: Object Type 질의(`…ByObjectType`)로 채널 추가 없이 종류별 질의.

### 트레이스와 쿼리
- 동기: `LineTraceSingleByChannel` 등은 호출 즉시 결과. `SCENE_QUERY_STAT`로 stat 이름 부여(프로젝트가 사용). `bFindInitialOverlaps` 기본 true. 침투 시작 sweep은 `bStartPenetrating`이고 이 경우 `ImpactPoint=Location`으로 의미가 없다(헤더 주석).
- 복잡 충돌: `bTraceComplex=true`가 아니면 단순 충돌 기준. PMC 지형은 `UseComplexAsSimple`라 기본 trace에도 맞는다(프로젝트 검증).
- 비동기: `AsyncLineTraceByChannel(EAsyncTraceType, Start, End, Channel, Params, ResponseParam, FTraceDelegate*, UserData)` 형태. `FTraceHandle`로 결과 조회/델리게이트. 서버 권위 판정은 동기로 두고 AI 시야·감지 같은 다수 질의에서 검토.
- 서버 고려: 클라이언트 trace 결과는 판정에 쓰지 않는다(기존 굴착 설계: 서버가 첫 유효 충돌을 재결정). 리슨 서버 호스트 프레임 비용이 늘 수 있으므로 질의 빈도를 제한한다.
- 얇은 메시 통과·터널링은 `bUseCCD`나 sweep 사용으로 완화(미검증).

### Hit/Overlap 이벤트
- Hit: 물리 시뮬레이션 충돌에는 "Simulation Generates Hit Events"가 필요하고 정지 접촉 중 반복 호출을 필터링한다. 공개 헤더는 캐릭터 이동·sweep도 Hit 원인으로 명시하며, 이 경우 `NormalImpulse`는 0이다. 시뮬레이션 알림 설정을 모든 Hit의 필수 조건으로 일반화하지 않는다.
- Overlap: 양쪽이 `Generate Overlap Events`여야 효율적으로 감지(문서). 같은 객체에서 충돌+오버랩 이벤트 병용은 권장하지 않는다는 문서 문구.
- 네트워크: 이벤트는 각 머신에서 로컬로 발생한다. 게임플레이 결과(입장/피해)는 서버 이벤트만 신뢰하고 상태를 복제한다.

### Physical Material·Surface Type
- 정의: `UPhysicalMaterial`에 `SurfaceType`(`EPhysicalSurface`), 이름은 Project Settings → Physics → Physical Surfaces(`UPhysicsSettings::PhysicalSurfaces`).
- 조회: `FHitResult::PhysMaterial`(`TWeakObjectPtr<UPhysicalMaterial>`). 일반 query는 `FCollisionQueryParams::bReturnPhysicalMaterial`, 컴포넌트 이동 sweep은 `UPrimitiveComponent::bReturnMaterialOnMove`를 켠다(`PrimitiveComponent.cpp`에서 query params로 매핑). `UPhysicalMaterial::DetermineSurfaceType`.
- 굴착 지형: PMC는 삼각형에 섹션 번호를 `MaterialIndices`로 넣으므로 섹션 재질의 Physical Material이 쓰인다. 단일 섹션 흙이면 표면은 하나다. 지층/광물 구분 표면이 필요하면 섹션 분할이나 별도 질의(밀도 격자에서 지층 조회)를 선택해야 한다. DynamicMeshComponent의 면별 재질 반환은 확인하지 못했다.
- 대안: SurfaceEffects 플러그인(Experimental), 데이터 에셋 기반 표면 테이블(프로젝트 표준 [Primary Data Asset](../STANDARD_FEATURES.md) 사용).

### PhysicsHandle·Constraint·Field
- PhysicsHandle: 시뮬레이션 물체 대상. 현 기획에서 아이템은 물리 충돌 없이 서버 슬롯/소유권으로 관리하므로 해당 없음.
- Constraint: 연출용 관절. 서버 권위 판정에 쓰이면 복제가 필요하다(관절 상태는 별도).
- Physics Field/Field System: 파괴 입력. Beta 계열.

### Simulate Physics와 복제
- 기본: 서버에서 `SetSimulatePhysics(true)`, 액터 `SetReplicateMovement(true)`, 클라이언트는 `ReplicatedMovement.bRepPhysics`에 맞춰 위치/속도 보정(Default 모드). `AActor::PhysicsReplicationMode`로 모드 선택.
- 모드(문서+소스): Default(레거시, 클라이언트 지역 상호작용이 덮어써짐), PredictiveInterpolation(서버 권위 + 지역 예측 보정, Resimulation보다 저렴), Resimulation(서버와 어긋나면 되감아 재시뮬레이션, 이력 캐시로 CPU·메모리 크다. 소스 열거자 표시명 "WIP", 문서는 Network Physics 컴포넌트를 C++로 수동 구현해야 한다고 적음).
- 설정: Project Settings → Physics → `PhysicsPrediction`(`bEnablePhysicsPrediction`, `MaxSupportedLatencyPrediction` 기본 1000ms, `ResimulationSettings`, `PhysicsReplicationLODSettings`). **deprecated**: `bEnablePhysicsResimulation`(→`bEnablePhysicsHistoryCapture`), `ResimulationErrorThreshold`(→ `FPhysicsReplicationResimulationSettings.ResimulationErrorPositionThreshold`). CVar `np2.PredictiveInterpolation`, `np2.Resim`, `p.Resim`(문서).
- 5.8 노트(검색 요약, 본문 미열람): NetworkPhysicsComponent에 scheduled/client-proposed action, 재시뮬레이션 쿨다운 관련 변경이 있다고 알려져 있으나 이번엔 확인하지 못했다.
- 리슨 서버: 호스트의 물리 틱 부담. 호스트와 클라이언트가 서로 다른 프레임이므로 시뮬레이션 물체는 서버에서만 확정한다. 탐사 중 신규 참가가 없어 합류 중 보정 문제는 줄지만 재접속 시 위치 일치는 확인 대상.

### 캐릭터 깔림·밀어내기 (Penetration)
- 지형 재생성으로 캡슐이 흙 안에 놓이는 경우: CMC가 `MaxDepenetrationWithGeometry` 한도 안에서 이동 중 밀어내고(`ResolvePenetration`), 이동 불가면 끼인 상태가 남는다. 서버 쪽 `…AsProxy` 값은 시뮬레이티드 프록시용이다.
- 폰끼리 겹침: `MaxDepenetrationWithPawn`·`RepulsionForce`(`bEnablePhysicsInteraction` 조건). 구덩이에서 두 명이 겹칠 때 밀림 속도 조절에 쓸 수 있다.
- 프로젝트 원칙(기존 설계): 플레이어 캡슐 내부에 충돌을 생성해 끼우지 않고, 점유 공간을 비운 뒤 서버가 `RequestWorldBurialPawn`으로 매몰시킨다. 엔진 침투 해소는 이 규칙의 보조일 뿐이다.
- 질의: `ComputePenetration`, `FHitResult::PenetrationDepth`, `SweepSingleByChannel`의 `bStartPenetrating`.

### Chaos Visual Debugger
- 기록(문서, 5.5판): 에디터 PIE는 툴바 버튼, 게임/서버는 `p.Chaos.StartVDRecording`, 종료 `p.Chaos.StopVDRecording`, 파일은 `Saved/Profiling`. 데디/원격은 `-CVDHostAddress`와 `p.Chaos.StartVDRecording Server`. 데이터 채널 `p.Chaos.VD.SetCVDDataChannelEnabled`·`-CVDDataChannelsOverride`로 줄인다. 문서가 CVD를 Beta로 서술하고 `.uplugin`도 Beta다. 독립 실행형은 준비되지 않았다고 쓴다(5.5판 문서이므로 5.8 상태는 별도 확인).
- 빌드: 런타임 지원은 타깃 `bCompileChaosVisualDebuggerSupport`가 켜져야 `WITH_CHAOS_VISUAL_DEBUGGER=1`. 기본값은 Shipping 아님+trace 활성(`TargetRules.cs`). 패키지 Development 빌드에서 기록 가능성이 있으나 이번엔 실행하지 않았다.
- 5.8 노트: CVD에 카메라 추적, 지오메트리 패널, 삼각 메시 복잡도 보기 추가(문서).

### 성능 주의
- 복잡 충돌(삼각 메시)은 쿠킹·메모리·질의 비용이 단순 도형보다 크다. 지형은 청크 크기와 재생성 빈도로 관리하고 `bUseAsyncCooking`·`bDeferCollisionUpdates` 영향은 [TERRAIN_DESTRUCTION.md](TERRAIN_DESTRUCTION.md).
- `bSupportUVFromHitResults`(PhysicsSettings)가 켜지면 PMC 충돌 쿠킹이 UV를 복사한다(메모리).
- CCD는 고속 소형 물체에 한정. `bAsyncPhysicsTickEnabled`는 스레드 전체 동기를 유발한다는 헤더 경고.
- Overlap 이벤트를 켠 컴포넌트 수를 줄이고 `QueryOnly` 컴포넌트에서 `SetGenerateOverlapEvents`가 필요한 곳만 켠다. 프로젝트의 엘리베이터 볼륨은 이미 모든 채널 Ignore 후 `ECC_Pawn`만 Overlap.
- Insights/Stat로 측정한다. 이 문서는 수치를 측정하지 않았다.

## 보류·비추천 후보와 이유
- **Resimulation 복제·Network Physics 컴포넌트**: WIP 표시, 비용 큼, 현재 시뮬레이션 대상 없음.
- **NetworkPrediction·Mover·ChaosMover**: Beta/Experimental. 이동 영역에서 별도 판단([GAMEPLAY_FRAMEWORK.md](GAMEPLAY_FRAMEWORK.md)).
- **ChaosRigidPhysicsAsync**: Experimental, `EnabledByDefault=false`.
- **ChaosUserDataPT**: Beta(주석 "Write-only on game thread"). 필요 근거 없음.
- **Async Physics Tick**: 전체 스레드 동기 경고.
- **SurfaceEffects**: Experimental. 데이터 테이블/DataAsset 직접 구성이 단순할 수 있음(설계 판단).

## 확인 한계
- 충돌 개요·네트워크 물리·트레이스·물리 머티리얼 페이지는 열었으나 트레이스 문서는 비동기·복잡 충돌·재질 반환을 다루지 않았고 물리 머티리얼 문서는 개요뿐이다. Surface Type 설정 절차는 소스로 대체했다.
- CVD 문서는 5.5판 페이지이며 5.8 UI 차이는 미확인. 5.8 NetworkPhysics 변경은 검색 요약만 보았다.
- `Chaos Destruction`/`ChaosVD 개요` 페이지는 본문 추출 실패(목차만 반환).
- 프로젝트 `DIGETHER/Config/*.ini` 전체에서 collision·channel·profile·physics 문자열을 검색해 `CollisionProfile`·`PhysicsSettings` 섹션이 없음을 확인했다(오디오 채널 수와 튜닝 프로파일 항목만 일치). 읽기만 했고 변경하지 않았다.
- 실행·프로파일·패키지 시험은 하지 않았다. 헤더 존재 ≠ 동작 보장.
