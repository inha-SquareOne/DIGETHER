# 네트워크 복제·세션 이동·재접속 기능 카탈로그

기준일 2026-11-04 · UE 5.8.4 (CL 58902052) · 상태: **조사 자료, 채택 결정 아님**. 색인은 [기능 카탈로그](README.md). 세션·음성 제공자는 [ONLINE_VOICE.md](ONLINE_VOICE.md)에 따로 둔다. 프로젝트 상태 서술(GameMode 기반 클래스·재접속)은 2026-11-10 코드 대조로 정정했다.

근거 표시: `소스` = 로컬 5.8.4의 헤더·`.uplugin`·`.Build.cs`·`BaseEngine.ini`를 이번에 직접 열어 확인(경로는 `Engine/...`). `문서` = Epic 공식 문서를 이번에 열어 확인. `미확인` = 알려진 사실이나 이번에 확인 못 함. 프로젝트 사용 여부는 C++ 소스 grep과 ini 읽기 결과다.

## 요약
- **레거시 복제 + 프로퍼티 복제(ReplicatedUsing/OnRep, `COND_OwnerOnly`)가 이미 프로젝트 기본선이다.** 지갑(`COND_OwnerOnly`)·엘리베이터·할당량이 이 방식이며, [네트워크 권위](../NETWORK_AUTHORITY.md) 규칙과 맞다.
- **Iris는 Beta(`.uplugin`)인데 공식 문서는 Experimental이라 부른다.** 켜는 방법은 있으나(플러그인+CVar+NetDriver 설정) 프로젝트의 `SteamSocketsNetDriver`와의 호환은 미확인이다. 채택 전 별도 검증 대상이다.
- **Replication Graph는 Beta이며 현재 미채택이다.** 접속 인원만으로 필요성을 단정하지 않고 복제 액터 수·서버 CPU 측정으로 판단한다. 휴면·거리 컬링·`NetUpdateFrequency`도 비교 대상이다.
- **재접속은 `AGameMode`에만 비활성 PlayerState 기능이 있다.** 프로젝트의 게임 GameMode(`ADigetherGameplayGameMode`)는 `AGameMode`를 상속하고 `AddInactivePlayer`·`FindInactivePlayer`를 재정의해 쓴다(조사 당시 프로토타입은 `AGameModeBase`였다. 아래 [재접속](#재접속-비활성-playerstate) 절). `FindInactivePlayer`는 데스크톱에서 UniqueId가 유효하면 그 값을 비교한다(소스). 이름·IP 대조는 UniqueId가 무효일 때의 폴백이다.
- **프로젝트는 대기실→게임 이동을 Seamless `ServerTravel`로 한다(2026-11-06 사용자 결정).** 비Seamless는 Steam에서 실패했다: `USteamSocketsNetDriver::Shutdown`이 이전 대기 소켓을 `MarkForDeletion`으로 다음 Tick에 닫아, 같은 프레임의 새 맵 `InitListen`이 같은 P2P 포트(7777)를 열지 못한다(`NetDriverListenFailure`, 2026-11-06 두 PC 로그·엔진 소스). 전환 맵은 비워 엔진 빈 맵을 쓴다.
- **`NetUpdateFrequency` 등의 public 멤버 직접 접근은 5.5부터 deprecated**다. `SetNetUpdateFrequency()`·`SetNetCullDistanceSquared()`를 써야 한다(프로젝트는 이미 Setter 사용).

## DIGETHER에서 쓰이는 곳
| 관련 기능 | 현재 프로젝트 상태 |
|---|---|
| S01 세션·재접속 | 세션 생성·검색·참가는 `UDigetherSessionSubsystem`(Steam 로비, 최대 4인). 접속 거절 규칙은 `DigetherJoinRules`가 `PreLogin`에서 판정. 신규 참가는 정상 거점 준비 상태에서만 받고 출발 게이지 완료~결과 크레딧 동안 막으며, 게이지 전 수락·완료 뒤 확정은 빈자리가 있으면 관전 합류(시스템 작업리스트 1.1.0 S01. 문 닫힘 중 참가 잠금은 구현(실행 검증 일부). 게임 오버 중 잠금·관전 합류·로비 이탈자의 탐사 중 관전 복귀·참가 실패 6종(사유 키 전달)은 구현(자동 테스트 통과, M2 미검증)). 거점 퇴장·끊김은 캐릭터 제거·자리 반환(소지 장비 바닥 유지는 인벤토리 미구현), 탐사 출발 후 끊김은 캐릭터·자리 유지. 같은 ID 교체와 비활성 PlayerState 재접속은 구현(자동 테스트 통과, M2 미검증, [SYSTEM_DESIGN](../../SYSTEM_DESIGN.md) 재접속 기술 계약) |
| 네트워크 권위 | 복제 프로퍼티·Server/Client Reliable RPC를 `Game/` 아래에서 사용 중(지갑, 엘리베이터, 레버, 할당량, 컨트롤러) |
| 굴착 지형 복제 | 프로토타입 `ADigetherExcavationTerrain`: `bAlwaysRelevant=true`, `SetNetUpdateFrequency(10)` ([굴착 구현](../../design/EXCAVATION_IMPLEMENTATION.md)). `Game/Excavation/Dain/`의 굴착 Actor `ADigetherDiggableSoil`에는 복제 설정·복제 프로퍼티가 아직 없다 |
| 맵 이동 | 로비 GameMode `bUseSeamlessTravel=true`. 호스트가 `World->ServerTravel(GameMap + "?listen", true)`. 게임 GameMode `HandleSeamlessTravelPlayer`가 PC·PS를 게임 클래스로 교체하고 로그인 목록을 보충(PreLogin·Login·PostLogin 미경유), 시작 저장·캐릭터 생성은 `StartPlay`·`HandleStartingNewPlayer` |
| NetDriver | `DefaultEngine.ini`가 `GameNetDriver`를 `SteamSocketsNetDriver`(폴백 IpNetDriver)로 교체. SteamAuth 패킷 핸들러 사용 |
| 휴면 | 명시적 휴면 설정은 C++에서 발견하지 못했다. `UDigetherMonsterHealthComponent`가 체력 변경 전에 `FlushNetDormancy()`, 변경 뒤 `ForceNetUpdate()`를 호출한다 |
| 미사용 | Push Model, FastArraySerializer, NetSerialize 커스텀 구조체, 등록형 서브오브젝트, Iris, Replication Graph, Network Prediction, 네트워크 에뮬레이션 설정 |

## 기능 목록
| 기능 | 모듈·플러그인 (타입, 안정성) | 하는 일 | DIGETHER 쓸모 | 프로젝트 상태 | 근거 |
|---|---|---|---|---|---|
| Actor 복제 (`SetReplicates`, 컴포넌트 `SetIsReplicatedByDefault`) | Engine (Runtime) | 액터·컴포넌트를 클라이언트에 생성·갱신 | 기본 | 사용 | 소스 `Engine/Source/Runtime/Engine/Classes/GameFramework/Actor.h`, `Engine/Source/Runtime/Engine/Classes/Components/ActorComponent.h` |
| 프로퍼티 복제 `ReplicatedUsing`/OnRep, `DOREPLIFETIME_*` | Engine | 값 변경을 복제하고 클라에서 알림 | 상태 원본 전달 | 사용 | 소스 `Engine/Source/Runtime/Engine/Public/Net/UnrealNetwork.h` |
| 복제 조건 `ELifetimeCondition` | CoreUObject | 소유자 전용·초기 1회·시뮬레이션 전용 등 대상 제한 | 개인 장부는 `COND_OwnerOnly`로 전달 대상부터 제한 | `COND_None`, `COND_OwnerOnly` 사용 | 소스 `Engine/Source/Runtime/CoreUObject/Public/UObject/CoreNetTypes.h` |
| Push Model | NetCore (Runtime) | 변경 시 수동 Dirty 표시로 비교 비용 절감 | 값이 많은 액터에서 서버 CPU 절감 후보 | 미사용 | 소스 `Engine/Source/Runtime/Net/Core/Public/Net/Core/PushModel/PushModel.h` |
| RPC (Server/Client/NetMulticast, Reliable/Unreliable, WithValidation) | Engine | RPC 종류·호출 측·소유 연결에 따라 실행 위치 결정 | 클라→호스트 의도 전달 | Server/Client Reliable 사용 | 소스 `Engine/Source/Runtime/Engine/Classes/GameFramework/Character.h`(WithValidation 예), 프로젝트 `DigetherGameplayPlayerController.h` |
| NetRole / Authority | Engine | `ROLE_Authority/AutonomousProxy/SimulatedProxy`, `HasAuthority()` | 호스트도 리슨 서버로 Authority | 사용 | 소스 `Engine/Source/Runtime/Engine/Classes/Engine/EngineTypes.h`, `Engine/Source/Runtime/Engine/Classes/GameFramework/Actor.h` |
| 관련성 (`bOnlyRelevantToOwner`, `bAlwaysRelevant`, `bNetUseOwnerRelevancy`, `NetCullDistanceSquared`) | Engine | 연결별 복제 대상·거리 컷 | 굴착 지형은 항상 관련, 개인 액터는 소유자만 | `bAlwaysRelevant` 사용 | 소스 `Actor.h` |
| 휴면 (`SetNetDormancy`, `FlushNetDormancy`, `ENetDormancy`) | Engine | 변하지 않는 액터를 복제 후보에서 제외 | 정적 문·레버 같은 가끔 변하는 액터 | 체력 변경 시 `FlushNetDormancy` 사용; 명시적 휴면 설정은 C++에서 미발견 | 소스 `Actor.h`, `EngineTypes.h`; 프로젝트 `Game/Monster/DigetherMonsterHealthComponent.cpp` |
| `NetUpdateFrequency` / `MinNetUpdateFrequency` / `NetPriority` | Engine | 갱신 빈도·우선순위. 기본 100Hz / 2Hz / 1.0 | 대역폭 조절 | Setter 사용 | 소스 `Actor.h`, `Private/Actor.cpp`(생성자) |
| FFastArraySerializer | NetCore (Runtime) | 큰 배열의 항목 단위 증분 복제 + Add/Change/Remove 콜백 | 인벤토리·목록형 상태 | 미사용 | 소스 `Engine/Source/Runtime/Net/Core/Classes/Net/Serialization/FastArraySerializer.h` |
| 커스텀 `NetSerialize`/`NetDeltaSerialize` | Engine | 구조체 직렬화 직접 정의 | 굴착 델타 같은 압축 전송 | 미사용 | 소스 `Engine/Source/Runtime/Engine/Classes/Engine/NetSerialization.h` |
| 벡터 네트워크 양자화 (`FVector_NetQuantize`, `10`, `100`, `Normal`) | Engine (Runtime) | 위치·방향을 정해진 정밀도로 직렬화 | 굴착 좌표·상호작용 방향의 전송량과 판정 오차 비교 후보 | C++ 소스 명시 사용 미발견 | 소스 `Engine/Source/Runtime/Engine/Classes/Engine/NetSerialization.h`의 각 `NetSerialize` |
| 등록형 서브오브젝트 (`bReplicateUsingRegisteredSubObjectList`, `AddReplicatedSubObject`) | Engine | UObject·컴포넌트를 등록 목록으로 복제 | UObject 복제가 필요할 때. Iris는 이 방식 전제 | 미사용(기본값 레거시) | 소스 `Actor.h`, `ActorComponent.h`, `ActorComponent.cpp` |
| Iris 복제 시스템 | Iris 플러그인(`Plugins/Experimental/Iris`, Runtime, **Beta**) + IrisCore(엔진 모듈) | 새 복제 시스템(옵트인) | 대규모 대상. 4인 협동의 필요는 측정 후 판단 | 미사용 | 소스 `.uplugin`, `BaseEngine.ini`, `UnrealEngine.cpp`; 문서 |
| Replication Graph | `Plugins/Runtime/ReplicationGraph`(Runtime, **Beta**) | 대량 액터·접속용 그래프 기반 복제 드라이버 | 복제 액터 수·서버 CPU 측정 후 필요성 판단 | 미사용 | 소스 `.uplugin`; 문서 |
| 맵 이동 (`UWorld::ServerTravel`, `APlayerController::ClientTravel`, `UEngine::Browse`) | Engine | 서버 주도 맵 변경, 클라 따라 이동 | 대기실→탐사 | `ServerTravel` Seamless 사용 | 소스 `Engine/Source/Runtime/Engine/Private/World.cpp`, `PlayerController.cpp`, `UnrealEngine.cpp`; 문서 |
| Seamless Travel (`bUseSeamlessTravel`, `TransitionMap`, `GetSeamlessTravelActorList`, `PlayerState::CopyProperties`) | Engine / EngineSettings | 연결을 끊지 않고 전환 맵 경유 이동 | 거점↔탐사 반복 이동 시 재접속 부담 감소 후보 | 대기실→게임 사용(2026-11-06) | 소스 `GameModeBase.h`, `PlayerState.h`, `GameMapsSettings.h`; 문서 |
| 비활성 PlayerState (`AddInactivePlayer`/`FindInactivePlayer`/`OverridePlayerState`) | Engine (`AGameMode` 전용) | 이탈 PlayerState를 복제본으로 보관, 재입장 시 재연결 | 같은 ID 재접속 복원 | 사용. `ADigetherGameplayGameMode`(`AGameMode` 상속)가 `AddInactivePlayer`·`FindInactivePlayer`를 재정의해 복제본 대신 원본을 보존 | 소스 `Classes/GameFramework/GameMode.h`, `Private/GameMode.cpp` |
| 접속 승인 (`PreLogin`, `PreLoginAsync`, `AGameSession::ApproveLogin`, `KickPlayer`, `BanPlayer`) | Engine | 접속 전 거절·강제 퇴장 | 신규 참가 차단 구간 거절. 같은 ID는 이전 접속 종료 후 새 접속 수락(사용자 결정 2026-11-05, 기획 답변 C2. `PreLoginAsync`로 구현, 자동 테스트 통과, M2 미검증). `KickPlayer`는 Pawn을 먼저 파괴하므로 교체에 그대로 쓰지 않음(아래 재접속 절) | `PreLogin`·`PreLoginAsync` 사용 | 소스 `GameModeBase.h`, `GameSession.h`, `GameSession.cpp` |
| 캐릭터 이동 예측 (CharacterMovement, `ServerMovePacked`) | Engine | 클라 예측·서버 보정이 기본 내장 | 1인칭 이동은 별도 복제부터 만들지 않는다 | `ACharacter` 기반 | 소스 `Classes/GameFramework/CharacterMovementComponent.h` |
| Network Prediction 플러그인 | `Plugins/Runtime/NetworkPrediction`(Runtime, **Beta**) | 롤백형 예측 프레임워크 | 특수 이동·물리 상호작용 필요 시 | 미사용 | 소스 `.uplugin`, `readme.txt` |
| Mover 플러그인 | `Plugins/Experimental/Mover`(Runtime + UncookedOnly, **Experimental**) | 롤백 네트워킹 이동 | CMC 대체 후보(성숙도 낮음) | 미사용 | 소스 `.uplugin` |
| 네트워크 에뮬레이션 (`FPacketSimulationSettings`) | Engine (비-Shipping) | 지연·손실·중복·순서 변경 모의 | 4인 지연 시험 | 미사용 | 소스 `Classes/Engine/NetDriver.h`, `Private/Net/NetEmulationHelper.cpp`, `BaseEngine.ini` |
| NetDriver 정의 / SteamSocketsNetDriver | Engine, SteamSockets(Runtime) | 전송 계층 선택 | Steam 릴레이 P2P | 사용 | 소스 `DefaultEngine.ini`, `Plugins/Runtime/Steam/SteamSockets` |
| RPC DoS 감지 | Engine | 과다 RPC 탐지·차단 | 클라→호스트 요청 남용 방어 | 미사용(기본 꺼짐) | 소스 `BaseEngine.ini` `[GameNetDriver RPCDoSDetection]` |
| 패킷 암호화 핸들러 (AESGCM/DTLS/AES) | `Plugins/Runtime/PacketHandlers/*`(모두 Runtime·기본 비활성; AESGCM/AES `IsBetaVersion=false`, DTLS **Experimental**, AES 클래스 **Deprecated 5.1**) | 패킷 암호화; AES는 AESGCM 대체를 안내 | Steam 전송 위 추가 암호화 필요 시 AESGCM·DTLS 비교 | 미사용 | 소스 각 `.uplugin`, `AESHandlerComponent/Source/Public/AESHandlerComponent.h`, `AESGCMHandlerComponent/Source/Public/AESGCMHandlerComponent.h`, `DTLSHandlerComponent/Source/Public/DTLSHandlerComponent.h` |
| Multi-Server Replication | `Plugins/Runtime/MultiServerReplication`(Runtime, **Experimental**) | 서버 프로세스 간 연결 | 해당 없음 | 미사용 | 소스 `.uplugin` |

## 기능별 상세

### 프로퍼티 복제·복제 조건
- 핵심: `UPROPERTY(Replicated / ReplicatedUsing=OnRep_X)`, `GetLifetimeReplicatedProps`, `DOREPLIFETIME`·`DOREPLIFETIME_CONDITION`·`DOREPLIFETIME_CONDITION_NOTIFY`·`DOREPLIFETIME_WITH_PARAMS_FAST`(`FDoRepLifetimeParams`: Condition, RepNotifyCondition, `bIsPushBased`). `Engine/Source/Runtime/Engine/Public/Net/UnrealNetwork.h`. (소스)
- 조건 열거(소스 `CoreNetTypes.h`): `COND_None, InitialOnly, OwnerOnly, SkipOwner, SimulatedOnly, AutonomousOnly, SimulatedOrPhysics, InitialOrOwner, Custom, ReplayOrOwner, ReplayOnly, SimulatedOnlyNoReplay, SimulatedOrPhysicsNoReplay, SkipReplay, Dynamic(숨김), Never(숨김), NetGroup(숨김, 서브오브젝트 전용)`.
- 리슨 서버: C++에서 서버가 값을 대입해도 OnRep는 자동 호출되지 않는다. 호스트용 적용 호출이 따로 필요하다([네트워크 권위](../NETWORK_AUTHORITY.md) 규칙과 동일).
- `COND_OwnerOnly`는 액터의 Owner 연결에만 보낸다. 컴포넌트에 쓸 때 Owner 체인이 PlayerController로 이어지는지 확인해야 한다(지갑 컴포넌트가 PlayerState에 있다면 PlayerState의 Owner가 해당 PC인지 검증 대상. 미확인).

### Push Model
- `net.IsPushModelEnabled` CVar 확인 후 `MARK_PROPERTY_DIRTY_FROM_NAME` 계열 매크로로 변경을 알리고 `bIsPushBased=true`로 등록한다. `Engine/Source/Runtime/Net/Core/Public/Net/Core/PushModel/PushModel.h`가 `IsPushModelEnabled()`와 `MARK_PROPERTY_DIRTY_FROM_NAME`, `MARK_PROPERTY_DIRTY_FROM_NAME_STATIC_ARRAY_INDEX`, `MARK_PROPERTY_DIRTY_FROM_NAME_STATIC_ARRAY`를 정의한다. (소스)
- 문서는 Iris 켜기 단계에서 `net.IsPushModelEnabled=1`을 "`net.Iris.PushModelMode`가 1일 때 필요"하다고 한다(문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/introduction-to-iris-in-unreal-engine).
- 주의: Dirty 표시를 빠뜨리면 변경이 전송되지 않는다. 4인 규모의 이득은 측정으로 확인해야 한다.

### RPC
- 지정자: `Server`, `Client`, `NetMulticast`, `Reliable`/`Unreliable`, `WithValidation`. 엔진 예: `Character.h`의 `UFUNCTION(unreliable, server, WithValidation)`. (소스)
- 클라이언트가 소유 연결 없는 액터에서 호출한 Server RPC는 서버에 전달되지 않는다. 서버에서 직접 호출하는 경우와 구분한다([Epic RPC 실행 표](https://dev.epicgames.com/documentation/en-us/unreal-engine/remote-procedure-calls-in-unreal-engine), 문서). 호스트 판정은 [네트워크 권위](../NETWORK_AUTHORITY.md) 규칙대로 RPC 안에서 현재 상태를 다시 검사한다.
- `RPCDoSDetection`(`bRPCDoSDetection=false` 기본)은 켜면 누적 시간 한도 초과 시 단계별(Hitch→…→Kick) 조치를 한다. 허용 목록에 `ServerMove*`가 있다. (소스 `BaseEngine.ini`). 4인 호스트형에서 필요성·오탐은 미검증.

### 관련성·휴면·갱신 빈도
- `Actor.h`: `bOnlyRelevantToOwner`, `bAlwaysRelevant`, `bNetUseOwnerRelevancy`, `NetDormancy`, `NetCullDistanceSquared`, `NetPriority`, `ForceNetUpdate()`, `FlushNetDormancy()`. (소스)
- `ENetDormancy`: `DORM_Never, DORM_Awake, DORM_DormantAll, DORM_DormantPartial, DORM_Initial`. 휴면 액터 값을 바꿀 때는 `FlushNetDormancy()` 또는 `ForceNetUpdate()`를 호출한다. (소스)
- **Deprecated(5.5)**: `NetUpdateFrequency`, `MinNetUpdateFrequency`, `NetCullDistanceSquared` 직접 접근은 `Set/GetNetUpdateFrequency()` 등으로 대체. 기본값 `NetUpdateFrequency=100`, `MinNetUpdateFrequency=2`, `NetPriority=1`. (소스 `Actor.h`, `Actor.cpp`)
- 굴착 지형처럼 크기가 큰 상태를 `bAlwaysRelevant`로 모두에게 보내는 것은 현재 선택이다. 거리 기반 관련성으로 바꿀 때 [굴착 구현](../../design/EXCAVATION_IMPLEMENTATION.md)의 "관련성 복귀 시 스냅샷" 계약을 함께 검토해야 한다.

### FFastArraySerializer·커스텀 직렬화
- `FFastArraySerializer` + `FFastArraySerializerItem`; 항목 변경 시 `MarkItemDirty`, 삭제 시 `MarkArrayDirty`; 클라 콜백 `PostReplicatedAdd/Change`, `PreReplicatedRemove`. 구조체에 `WithNetDeltaSerializer` 트레이트가 필요하다. (소스)
- 현재 헤더는 `Engine/Source/Runtime/Net/Core/Classes/Net/Serialization/FastArraySerializer.h`다. `Engine/Source/Runtime/Engine/Classes/Engine/NetSerialization.h` 주석이 `Net/Serialization/FastArraySerializer.h`로 이동했다고 안내한다. (소스)
- 항목 수가 많고 일부만 바뀌는 목록(인벤토리, 구조 요청 목록 등)에 적합하다. 항목이 적은 상태에는 일반 배열 복제로 충분한지 먼저 본다(기준은 측정으로 정한다).
- 개별 `NetSerialize`는 `WithNetSerializer` 트레이트로 구조체에 연결한다(`NetSerialization.h` 상단 설명). (소스)

### 등록형 서브오브젝트
- `bReplicateUsingRegisteredSubObjectList`가 true면 `AddReplicatedSubObject(Obj, Condition)`/`RemoveReplicatedSubObject`로 등록한 것만 복제하고 `ReplicateSubobjects()`는 호출되지 않는다. false면 구형 `ReplicateSubobjects()` 오버라이드를 쓴다. (소스 `Actor.h`)
- 5.8.4 기본값은 false이고, 전역 기본값은 CVar `net.SubObjects.DefaultUseSubObjectReplicationList`(기본 false)로 바꾼다. (소스 `ActorComponent.cpp`) Iris 문서가 이 CVar=1을 필수로 적는다(문서 위 Iris URL). 
- 공식 문서 `replicated-subobjects-in-unreal-engine` 페이지는 이번 추출에서 목차만 나왔다. 서술은 소스 근거다.
- `COND_NetGroup`은 서브오브젝트 전용(소스 `CoreNetTypes.h`).

### Iris
- 플러그인: `Engine/Plugins/Experimental/Iris/Iris.uplugin` — `IsBetaVersion=true`, `IsExperimentalVersion=false`, `EnabledByDefault=false`, Runtime. **폴더는 Experimental이고 공식 문서 문구는 "Experimental"**이다(문서). 표시가 서로 달라 "Beta"로 단정하지 말고 둘 다 적는다. (소스·문서)
- 엔진 쪽: 코어 모듈 `IrisCore`(`Engine/Source/Runtime/Net/Iris`)가 엔진에 있고, 이 버전 `Core.Build.cs`/`ModuleRules.cs`는 `UE_WITH_IRIS=1`을 항상 정의한다("코드에서 제거 예정" 주석). 문서의 `bUseIris` Target 설정이 5.8.4에서도 필요한지는 미확인. (소스·문서)
- 켜는 방법(문서): `.uproject`에 Iris 플러그인, 모듈 `Build.cs`에 `SetupIrisSupport(Target)`, `DefaultEngine.ini [SystemSettings]`에 `net.Iris.UseIrisReplication=1`, `net.SubObjects.DefaultUseSubObjectReplicationList=1`(필수), 필요 시 `net.IsPushModelEnabled=1`, 커맨드라인 `-UseIrisReplication=0/1`로 토글. (문서)
- NetDriver별 허용은 `[/Script/Engine.Engine] IrisNetDriverConfigs`에서 한다. 엔진 기본값은 `GameNetDriver`에 `bCanUseIris=true`, `DemoNetDriver`는 false. 우선순위: 커맨드라인 > GameInstance `GetDesiredReplicationSystem` > 설정의 `PreferredReplicationSystem` > 엔진 CVar. IrisCore 모듈이 로드돼 있어야 한다. (소스 `UnrealEngine.cpp` `WillNetDriverUseIris`, `BaseEngine.ini`)
- Iris 사용 시 액터가 `RegisterReplicationFragments`, `BeginReplication`/`EndReplication` 훅을 가진다. (소스 `Actor.h`)
- **프로젝트의 `GameNetDriver`는 `USteamSocketsNetDriver`**이며 이 클래스는 `UNetDriver`를 직접 상속한다(소스 `SteamSocketsNetDriver.h`). Iris 지원 여부를 SteamSockets 소스에서 검색했을 때 "Iris" 문자열이 없었다. 이는 비호환 증거가 아니라 **호환 미확인**이다.
- 문서는 한계·미지원 기능 목록을 제공하지 않는다(해당 페이지 추출 결과). 도입 전 PIE 다인·패키지 4인 시험이 필요하다.
- 기존 [UNREAL_FEATURE_COVERAGE](../../design/UNREAL_FEATURE_COVERAGE.md)는 해당 문서만으로 Iris를 도입하지 않는다고 정리했고, [STANDARD_FEATURES](../STANDARD_FEATURES.md)는 도입에 별도 결정(ADR)을 요구한다. 영구적인 미도입 결정으로 해석하지 않는다.

### Replication Graph
- `Engine/Plugins/Runtime/ReplicationGraph/ReplicationGraph.uplugin`: `IsBetaVersion=true`, Runtime, `LoadingPhase=PreDefault`. (소스)
- 문서: 대규모(예: 100명·5만 액터)를 위한 설계, `IpNetDriver`의 `ReplicationDriverClassName` 설정 또는 `CreateReplicationDriverDelegate`로 활성화, 스플릿스크린 미지원으로 콘솔에서는 비활성. Iris와의 관계는 문서에 언급이 없다. (문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/replication-graph-in-unreal-engine)
- 프로젝트 NetDriver가 `SteamSocketsNetDriver`라 `[/Script/OnlineSubsystemUtils.IpNetDriver]` 설정 경로가 그대로 적용되지 않을 수 있다. `ReplicationDriverClassName`은 `UNetDriver` 멤버이므로(소스 `NetDriver.h`) 소켓 드라이버 섹션에 쓰는 방식이 가능한지는 미확인.

### 레벨 전환 (ServerTravel / Seamless Travel)
- 문서 요지: Seamless는 비차단이고 전환 맵을 경유한다. 비Seamless는 연결을 끊고 재접속한다. 처음 맵 로드, 첫 접속, 한 멀티플레이 종료 후 다른 게임 시작에는 비Seamless가 필요하다. (문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/travelling-in-multiplayer-in-unreal-engine)
- 활성화: `UGameMapsSettings::TransitionMap`(소스 `EngineSettings/Classes/GameMapsSettings.h`)과 `AGameModeBase::bUseSeamlessTravel=true`(소스 `GameModeBase.h`). 비워 두면 빈 맵이 만들어진다(문서).
- 기본 유지 액터: GameMode(서버), PlayerState가 있는 컨트롤러·모든 PlayerController(서버), 로컬 PlayerController. 추가는 `AGameModeBase::GetSeamlessTravelActorList`, `APlayerController::GetSeamlessTravelActorList`. (문서)
- PlayerState: `CopyProperties(NewPS)`, `SeamlessTravelTo(NewPS)`, `bFromPreviousLevel`/`IsFromPreviousLevel()`. 새 맵의 PlayerState 클래스가 달라도 `CopyProperties`로 값을 넘긴다. (소스 `PlayerState.h`) 
- 훅: `AGameModeBase::HandleSeamlessTravelPlayer`, `PostSeamlessTravel`. (소스) 로그아웃 이벤트는 비Seamless `ServerTravel` 중에도 발생한다(`GameModeBase.h` 주석).
- `UNetDriver::ServerTravelPause`(SteamSockets 기본 4.0초): 서버 이동 전 클라가 따라올 시간. (소스 `BaseEngine.ini`)
- **프로젝트 사정**: 2026-11-06 사용자 결정으로 로비 GameMode를 Seamless로 바꿨다. 클래스 교체는 엔진 `AGameMode::HandleSeamlessTravelPlayer`가 하고 PlayerState 값은 기본 `CopyProperties`(이름·UniqueId 등)로 넘어간다. 게임 맵의 접속 승인·같은 계정 교체 검사는 이후 새 참가자(Login 경로)에만 적용된다. LAN 2프로세스 이동·캐릭터 생성 확인, Steam 두 PC 재시험 대기. PIE에서는 엔진 설정 `net.AllowPIESeamlessTravel` 없이는 Seamless가 동작하지 않을 수 있다.

### 재접속 (비활성 PlayerState)
- `AGameMode`(MatchState 계열)만 `InactivePlayerArray`, `InactivePlayerStateLifeSpan`, `MaxInactivePlayers`, `AddInactivePlayer`, `FindInactivePlayer`, `OverridePlayerState`를 가진다. `Logout`에서 `AddInactivePlayer`, `PostLogin` 경로에서 `FindInactivePlayer`가 호출된다. (소스 `GameMode.h`, `GameMode.cpp`)
- `AddInactivePlayer`는 PlayerState를 `Duplicate()`해 `SetReplicates(false)`, `SetLifeSpan(InactivePlayerStateLifeSpan)`으로 보관한다. 같은 UniqueId의 이전 항목은 파괴하고, 최대 수를 넘으면 오래된 것부터 제거한다. 관전자·이전 레벨 PlayerState는 저장하지 않는다. (소스)
- `FindInactivePlayer`는 데스크톱에서 UniqueId가 유효하면 UniqueId로 비교하고, 무효면 저장된 네트워크 주소+이름으로 비교한다. 찾으면 기존 PlayerState를 새 PC에 붙이고 `OverridePlayerState`→`DispatchOverrideWith`를 부른다. (소스)
- `APlayerState`는 `bIsInactive` 복제, `OnDeactivated`/`OnReactivated`, `CopyProperties`/`OverrideWith`를 제공한다. (소스 `PlayerState.h`)
- **프로젝트 현황(2026-11-10 코드 대조)**: 조사 당시 프로토타입은 `AGameModeBase`라 위 기능이 없었고, 대안으로 ① `AGameMode` 전환 ② 직접 구현한 재접속 레지스트리를 비교 대상으로 적었다. 이후 `Game/`의 `ADigetherGameplayGameMode`는 `AGameMode`를 상속해 ①의 경로를 재정의해서 쓴다(`Game/Framework/DigetherGameplayReconnect.cpp`). 엔진 기본과 다른 점은 다음과 같다.
  - 탐사 중 이탈만 비활성 PlayerState로 보존한다. 엔진 기본처럼 `Duplicate()`하지 않고 원본을 `InactivePlayerArray`에 넣어(`SetReplicates(false)`, 수명·개수 제한 없음: `InactivePlayerStateLifeSpan=0`, `MaxInactivePlayers=MAX_int32`) 런 참가자 목록과 남은 Pawn이 같은 객체를 가리키게 한다.
  - 이 기능은 **PlayerState만 보관**하므로 끊긴 캐릭터를 남기는 요구는 Controller의 `PawnLeavingGame` 재정의가 따로 맡는다.
  - 탐사 밖(거점) 이탈은 비활성으로 보관하지 않는다. 자리를 반환하고 PlayerState·Pawn을 제거한 뒤 장부만 저장 명단 값 레코드로 남긴다.
  - `FindInactivePlayer` 재정의는 UniqueId만 비교한다(이름·IP 폴백 없음).
  - 복제를 끈 비활성 PlayerState가 다른 클라이언트 화면에서 어떻게 보이는지(남은 몸의 이름·생존 상태 표시)와 MatchState 흐름의 영향은 실행으로 확인하지 않았다.
- `PreLogin`/`PreLoginAsync`로 접속 전 거절(신규 참가 차단 구간, 정원 초과 등)을 한다. 프로젝트는 `PlayerArray`만 비교하므로 동시에 대기 중인 연결은 못 잡는다는 한계를 코드 주석에 이미 적었다.
- **같은 ID 교체**: 같은 계정이 다시 들어오면 이전 접속을 끊고 새 접속을 받는다([사용자 결정 2026-11-05](../../planning/DECISIONS.md#사용자-결정-2026-11-05) 2, 기획 답변 C2). 시스템 작업리스트 1.1.0 S01의 '이미 연결된 ID는 거절'은 기획이 다음 버전에서 고칠 항목이라 따르지 않는다. 구현(자동 테스트 통과, M2 미검증): `PreLoginAsync`에서 같은 ID의 이전 연결을 끊고 정리될 때까지 기다린 뒤 새 접속을 수락한다. 교체 대기 제한 5초는 임시값(`DigetherJoinRules.h`)이며, 여러 교체 요청이 겹쳐도 콜백을 모두 처리한다(`Online.JoinRules.ReplacementQueueKeepsEveryCallback`). 대기실은 `KickPlayer`, 게임 맵은 남긴 Pawn 재접속 계약을 지키려고 `UNetConnection::Close`를 쓴다. 주의:
  - 엔진 `AGameSession::KickPlayer`는 대상 PC의 Pawn을 먼저 `Destroy()`한 뒤 `ClientWasKicked`·PC `Destroy()`를 부른다(소스 `Engine/Source/Runtime/Engine/Private/GameSession.cpp` 369~371행). 그대로 쓰면 끊긴 캐릭터를 남기고 비활성 PlayerState로 재접속하는 계약을 깬다. 교체 경로는 Pawn을 남기고 이전 연결만 닫는 별도 처리가 필요하다.
  - 이전 연결 종료(Logout·세션 등록 해제)와 새 연결 등록(Steam 세션 `RegisterPlayer`)의 순서가 엇갈리면 같은 ID가 세션에서 빠지거나 두 번 남을 수 있다. 순서와 결과는 실행으로 확인하지 않았다(M2/M3 검증 대상).
  - 이전 접속을 끊지 못하면 '같은 계정이 이미 접속 중' 문구로 거절한다([참가 실패 안내 문구](../../SYSTEM_DESIGN.md#참가-실패-안내-문구)).

### 이동 예측
- `ACharacter`/`UCharacterMovementComponent`는 `ServerMovePacked` 등으로 클라 예측과 서버 보정을 내장한다. `NetworkSmoothingMode`, `NetworkMaxSmoothUpdateDistance`가 시뮬레이션 프록시 보정을 조절한다. (소스 `CharacterMovementComponent.h`)
- `Network Prediction` 플러그인은 Beta, `NetworkPredictionExtras`도 Beta이고 "출하 제품에 직접 쓰지 말 것"이라고 자기 설명에 적었다. `Mover`는 Experimental. 모두 CMC 기반으로 충족되지 않는 요구가 확인된 뒤 비교한다. (소스 `.uplugin`)

### 네트워크 에뮬레이션
- `FPacketSimulationSettings`: `PktLoss`, `PktLossMinSize/MaxSize`, `PktOrder`, `PktDup`, `PktLag`, `PktLagVariance`, `PktLagMin/Max`, `PktJitter`. `DO_ENABLE_NET_TEST`이므로 **Shipping 빌드에서는 없다**. (소스 `NetDriver.h`)
- 설정 위치: `DefaultEngine.ini [PacketSimulationSettings]`(드라이버 정의명 접두사 가능, 예 `GameNetDriverPktLag`), 커맨드라인 `-PktLag=100` 형태, URL 옵션, 콘솔 `Net PktLag=...`, 프로필 `NetEmulation.PktEmulationProfile <이름>`, `NetEmulation.Off`. 기본 프로필 `Average`, `Bad`, `BufferBloat`는 `[/Script/Engine.NetworkSettings]`. (소스 `NetEmulationHelper.cpp`, `NetDriver.cpp`, `BaseEngine.ini`)
- 신뢰성 시험용 `NetEmulation.DropAnyUnreliable`, `DropUnreliableRPC` 등도 있다. (소스)
- SteamSockets 드라이버에서의 적용은 `UNetDriver` 공통 경로로 보이나 실제 효과는 미확인(Steam 쪽 전송 계층이 별도 큐를 가질 수 있음).

### NetDriver·SteamSockets 설정
- 프로젝트 `DefaultEngine.ini`: `-NetDriverDefinitions=(…IpNetDriver…)` 후 `+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/SteamSockets.SteamSocketsNetDriver",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")`. `[PacketHandlerComponents] +Components=OnlineSubsystemSteam.SteamAuthComponentModuleInterface`. `BeaconNetDriver`는 엔진 기본(IpNetDriver) 그대로다. (소스: 프로젝트 ini, `BaseEngine.ini`)
- 엔진 기본 `[/Script/SteamSockets.SteamSocketsNetDriver]`: `ConnectionTimeout=80`, `InitialConnectTimeout=120`, `NetServerMaxTickRate=30`, `MaxNetTickRate=120`, `KeepAliveTime=0.2`, `MaxClientRate/MaxInternetClientRate=100000`, `ServerTravelPause=4`. (소스 `BaseEngine.ini`)
- 공식 문서: Steam Sockets 빌드는 Steam Sockets 빌드끼리만 연결, 교차 플랫폼은 Windows·Mac·Linux, P2P listen 서버는 패킷 릴레이 설정과 무관하게 Steam 통신망을 사용, `bAllowP2PPacketRelay` 기본 true. (문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/using-steam-sockets-in-unreal-engine) 자세한 Steam 설정은 [ONLINE_VOICE.md](ONLINE_VOICE.md).
- 문서는 SteamSockets용 NetDriver 정의를 `[/Script/Engine.GameEngine]` 아래 platform별 `Engine.ini`에 적으라 하나 프로젝트는 `[/Script/Engine.Engine]`에 쓰고 있다. 엔진 기본(`BaseEngine.ini`)도 `[/Script/Engine.Engine]`이다(소스). 문서 문구와 다르지만 동작 증거는 프로토타입 호스트 listen 시작 기록([PROTOTYPE_STEAM](../../prototype/PROTOTYPE_STEAM.md))이다.

## 보류·비추천 후보와 이유
- **Replication Graph**: Beta이며 대규모 대상용이다. 문서가 Iris와의 관계를 말하지 않는다. 4인에서는 측정 근거가 생길 때까지 보류.
- **Iris**: Beta(plugin)·Experimental(문서)이며 SteamSocketsNetDriver 호환이 미확인이다. 기존 결정(미도입)을 유지하고 필요 시 별도 실험 계획으로 다룬다.
- **Network Prediction / Mover**: Beta / Experimental. CMC로 부족한 요구가 확인될 때만 비교.
- **Multi-Server Replication**: Experimental이며 리슨 서버 협동과 무관.
- **NetMulticast로 지속 상태 전달**: 후입·재접속에 취약하므로 복제 상태를 원본으로 둔다([네트워크 권위](../NETWORK_AUTHORITY.md)).
- **이름·IP 기반 재접속 대조**: `FindInactivePlayer` 폴백이지만 [SYSTEM_DESIGN](../../SYSTEM_DESIGN.md)이 표시 이름·IP를 식별 근거로 쓰지 않는다고 정했다.

## 확인 한계
- Epic 공식 문서는 Iris 소개, Steam Sockets, Travelling, Replication Graph, RPC 페이지 본문을 읽었다. `replicated-subobjects-in-unreal-engine`와 `using-voice-chat-in-unreal-engine`는 이전 조사에서 목차만 추출됐다.
- Iris와 SteamSocketsNetDriver 호환, Iris 켠 상태의 RPC·FastArray·Push Model 동작, Replication Graph와 SteamSockets 조합은 코드·실행으로 확인하지 못했다.
- `MARK_PROPERTY_DIRTY_*` 이름과 암호화 핸들러 `.uplugin` 상태는 소스로 재확인했다. Net PktLag의 SteamSockets 효과는 미확인이다.
- 모든 항목은 로컬 소스 열람이며 게임 실행·패키지 시험이 아니다. 프로젝트에서 실제로 쓰는지는 소스 grep과 ini 읽기 결과이고, 동작 검증은 [검증 계획](../../verification/VERIFICATION_PLAN.md)을 따른다.
- 이전 조사 [UNREAL_FEATURE_COVERAGE](../../design/UNREAL_FEATURE_COVERAGE.md)는 5.8.2 시점이다. 이 문서는 5.8.4 로컬 파일 기준이며 Iris·Replication Graph 서술은 서로 모순되지 않는다.
