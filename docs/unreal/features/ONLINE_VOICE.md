# 온라인 서브시스템·세션·음성 기능 카탈로그

기준일 2026-11-04 · UE 5.8.4 (CL 58902052) · 상태: **조사 자료, 목록 자체는 채택 결정 아님**. 색인은 [기능 카탈로그](README.md). 복제·레벨 전환·재접속은 [NETWORKING.md](NETWORKING.md)에 둔다. 세션 제공자, 정식 App ID, Steam Cloud 사용은 [결정 목록](../../planning/DECISIONS.md)에서 미결이다. 음성은 이후 **2026-11-04 사용자 결정으로 엔진 기본 VoIP를 선택**했고 2026-11-06에 1차 구현했다(실제 음성 송수신은 미검증, [구현 현황](#구현-현황-2026-11-06)). 프로젝트 상태 칸은 2026-11-10 코드 대조로 정정했다. 수신 제한·관전자 음성 방식은 2026-11-06 사용자 결정([아래](#수신-제한관전자-음성-방식-2026-11-06-사용자-결정))을 따른다. 이 문서는 누가 무엇을 듣는가 같은 게임 규칙을 새로 정하지 않는다.

근거 표시: `소스` = 로컬 5.8.4 헤더·`.uplugin`·`.Build.cs`·`BaseEngine.ini`를 이번에 직접 열어 확인(경로 `Engine/...`). `문서` = Epic 공식 문서를 이번에 열어 확인. `미확인` = 이번에 확인하지 못함.

## 요약
- **현재 경로는 OSSv1(Online Subsystem) Steam + SteamSockets**다. 세션은 Steam 로비(`bUseLobbiesIfAvailable`), 전송은 `SteamSocketsNetDriver`이며 프로토타입에서 호스트 listen 시작까지 기록이 있다. 다른 PC 참가는 프로젝트가 아직 검증하지 않았다([PROTOTYPE_STEAM](../../prototype/PROTOTYPE_STEAM.md)).
- **Online Services(OSSv2)는 공식 문서가 Beta라 부른다.** Steam용 직접 구현 플러그인은 로컬 엔진 검색에서 찾지 못했고 OSSv1을 감싸는 `OnlineServicesOSSAdapter`는 확인했다. Steam 판매 전제에서는 OSSv1 유지와 비교 대상이다.
- **음성은 엔진 기본 VoIP를 선택했고 2026-11-06 1차 구현했다(실제 음성 실행 검증 전).** 엔진에는 ① `IOnlineVoice` + 게임 NetDriver의 Voice 채널 + `UVOIPTalker` ② `IVoiceChat` 인터페이스 + 제공자(EOS RTC)가 있다. 프로젝트는 ①을 ini로 켰다. `EOSVoiceChat` 플러그인은 설정 없이 켜진 채 남아 있다.
- **호스트가 음성 패킷을 받는 쪽은 엔진 VoIP 경로다.** 서버는 Voice 채널로 받은 패킷을 `UNetDriver::ReplicateVoicePacket`(가상)로 다른 접속에 중계한다. 그래서 "누가 누구 음성을 받는가"를 호스트에서 훅으로 걸 수 있다. EOS RTC는 별도 서비스라 호스트가 음성 활동을 얻는지는 미확인이다(PQ-13).
- **CommonUser 세션 샘플 플러그인의 로컬 포함 여부는 미확인이다.** 로컬 `Engine/Plugins`에서 해당 플러그인 설명자를 찾지 못했으며, 다른 배포물이나 Lyra 포함 여부는 확인하지 않았다.
- **엔진 VoIP는 `FVoiceSettings`(`ComponentToAttachTo`, `AttenuationSettings`)로 공간 감쇠를 제공한다.** `IVoiceChat`에도 위치 채널 API가 있지만 **5.8.4 EOSVoiceChat의 `Set3DPosition`은 미구현**이다. 인터페이스 존재와 제공자의 구현을 구분한다.

## DIGETHER에서 쓰이는 곳
| 관련 기능 | 현재 프로젝트 상태 |
|---|---|
| S01 세션 | `UDigetherSessionSubsystem`(`Game/Online`): `IOnlineSession` 사용. Steam 모드는 로비, `-nosteam`은 LAN/Null. 최대 4인(`DigetherRoomProtocol::MaxPlayers`). 방 코드·이름을 세션 설정에 기록. 초대는 `IOnlineExternalUI`. Steam 초대와 방 코드를 모두 쓰는 방향은 2026-11-04 기획 답변(초안, 확정)이며 게임 안 친구 목록은 UI 시안 뒤 검토 |
| 접속 거절 | `DigetherJoinRules` + `PreLogin`: Steam 모드에서 Steam ID 요구, 만석·신규 참가 차단 구간 거절. 문 닫힘 중 참가 잠금과 참가 실패 문구 6종은 구현(실행 검증 일부). 같은 ID는 이전 접속 종료 후 새 접속 수락(사용자 결정 2026-11-05)이며 `PreLoginAsync`로 구현했다(자동 테스트 통과, M2 미검증. [NETWORKING.md](NETWORKING.md) 재접속 절) |
| S28 음성 | **엔진 기본 VoIP 선택(2026-11-04 사용자 결정), 1차 구현(2026-11-06, 실제 음성 미검증).** `DefaultEngine.ini [OnlineSubsystem] bHasVoiceEnabled=true`·`[Voice] bEnabled=true`, `DefaultGame.ini bRequiresPushToTalk=false`. 코드는 `Game/Voice/`(아래 [구현 현황](#구현-현황-2026-11-06)), 세션 설정 `bUseLobbiesVoiceChatIfAvailable=false`. `uproject`에 `EOSVoiceChat` 활성(설정 없음). D-009의 제공자 미결 표시는 이전 상태이며 PQ-13의 호스트 음성 활동 검증은 남아 있다. 거리 시험값은 아래 [기획 시험값](#기획-시험값-2026-11-04-답변-초안) |
| S30 저장 | Steam Cloud 사용 여부 미결. `UserCloud`는 OSS Steam에 구현돼 있다 |
| 플러그인 | 이 영역에서 `uproject`가 직접 활성화한 플러그인은 `SteamSockets`, `EOSVoiceChat`이다. `OnlineSubsystemSteam`·`SteamShared`·`OnlineSubsystem`·`OnlineSubsystemUtils`·`OnlineServices`·`EOSShared`·`VoiceChat`는 이 플러그인들의 의존성에 포함된다(소스 각 `.uplugin`의 `Plugins`). 실제 모듈 로드는 타깃·플랫폼 조건에 따른다 |
| 모듈 | `DIGETHER.Build.cs`: `CoreOnline`, `OnlineSubsystem`, `OnlineSubsystemUtils`. `OnlineSubsystemSteam`을 `DynamicallyLoadedModuleNames`로 넣지는 않았다(문서는 추가를 안내). 현재 동작 영향은 미확인 |

## 기능 목록
| 기능 | 모듈·플러그인 (타입, 안정성) | 하는 일 | DIGETHER 쓸모 | 프로젝트 상태 | 근거 |
|---|---|---|---|---|---|
| Online Subsystem 기반 | `OnlineSubsystem`·`OnlineBase`(Runtime, 기본 활성) | OSSv1 인터페이스 공통 | 모든 세션 코드의 기반 | 사용 | 소스 `Engine/Plugins/Online/OnlineSubsystem/OnlineSubsystem.uplugin` |
| Session 인터페이스 | OnlineSubsystem | 세션·로비 생성/검색/참가/종료, 초대 | 방 만들기·참가 | 사용 | 소스 `Interfaces/OnlineSessionInterface.h`; 프로젝트 `DigetherSessionSubsystem` |
| Identity 인터페이스 | OnlineSubsystem | 로그인 상태·고유 ID | 재접속 키(UniqueId) | 사용(로컬 사용자 ID 조회) | 소스 `Interfaces/OnlineIdentityInterface.h` |
| Friends / Presence | OnlineSubsystem (Steam 구현 있음) | 친구 목록·초대, 접속 상태 | 친구 초대 참가 | 미사용(초대는 External UI) | 소스 `OnlineSubsystemSteam.cpp` `GetFriendsInterface`/`GetPresenceInterface`가 구현 반환 |
| External UI | OnlineSubsystem (Steam 구현 있음) | Steam 오버레이 초대·프로필 UI | 초대 UI | 사용 | 소스; 프로젝트 `ShowInviteUI` |
| Achievements / Leaderboards | OnlineSubsystem (Steam 구현 있음) | 업적·순위표 | Steam 판매 시 후보 | 미사용 | 소스 `OnlineSubsystemSteam.cpp` |
| Stats 인터페이스 | OnlineSubsystem | 통계 쓰기·읽기 | **Steam 구현은 `nullptr`을 반환한다** | 해당 없음 | 소스 `OnlineSubsystemSteam.cpp` `GetStatsInterface` |
| User Cloud / Shared Cloud | OnlineSubsystem (Steam 구현 있음) | 사용자 파일 클라우드 저장 | Steam Cloud 후보 | 미사용 | 소스 `OnlineUserCloudInterfaceSteam.h` |
| 미지원 인터페이스(Steam) | — | Party, Groups, Time, TitleFile, Entitlements, Events, Sharing, User, Message, Chat, Turn-based, Tournament는 Steam OSS에서 `nullptr` | 필요하면 직접 구현 | — | 소스 `OnlineSubsystemSteam.cpp` |
| OnlineSubsystemSteam | `Plugins/Online/OnlineSubsystemSteam`(Runtime, 표시 없음, 기본 비활성, Win64/Mac/Linux/Android, Win64 arm64 제외) | Steam 로비·친구·업적·음성 등 | 현재 사용 중인 Steam 경로 | 사용(SteamSockets 의존으로 활성) | 소스 `.uplugin`, `.Build.cs`; 문서 |
| SteamShared | `Plugins/Runtime/Steam/SteamShared`(Runtime, Beta/Experimental 모두 false) | Steamworks DLL 로더(SDK v1.64) | 위 플러그인 의존 | 사용(간접) | 소스 `.uplugin`, `ThirdParty/Steamworks/Steamworks.build.cs` |
| SteamSockets | `Plugins/Runtime/Steam/SteamSockets`(Runtime, `IsBetaVersion=false`, 기본 비활성, Win64/Mac/Linux, Win64 arm64 제외) | Steam 릴레이·NAT 통과 P2P 전송 NetDriver | 호스트형 방 접속 | 사용 | 소스 `.uplugin`; 문서 |
| SocketSubsystemSteamIP | `Plugins/Online/SocketSubsystemSteamIP`(RuntimeNoCommandlet) | Steam ID 기반 IP형 연결, **NAT 통과 없음** | SteamSockets가 대신 | 미사용 | 소스 `.uplugin` 설명 |
| SteamAuth 패킷 핸들러 | OnlineSubsystemSteam | 접속 시 Steam 티켓 인증 | 위조 ID 접속 방지 | 사용(`[PacketHandlerComponents]`) | 소스 `OnlineAuthHandlerSteam.cpp`; 프로젝트 ini |
| Online Services (OSSv2) | `OnlineServices`(Runtime, **문서 Beta**, 기본 비활성) | 신 API: Auth/Sessions/Lobbies/Presence/Social/Stats/Achievements/Leaderboards/Commerce/UserFile/TitleFile/Privileges 등 | OSSv1 대체 후보. Steam 직접 구현 플러그인 포함 여부 미확인(로컬 검색 미발견) | 의존성에 포함(OnlineSubsystemUtils), 코드는 미사용 | 소스 `OnlineServices.uplugin`, `OnlineServicesInterface/Public/Online/*.h`; 문서 |
| OSSAdapter | `OnlineServicesOSSAdapter`(Runtime, 표시 없음) | OSSv1 위에 OSSv2 모양을 씌움(Auth/Sessions/Presence/Social/Stats 등. Lobbies 없음) | 단계적 이행용 | 미사용 | 소스 `.uplugin`, `Source/Private/Online/*OSSAdapter.cpp` 목록 |
| OnlineServicesEOS / EOSGS / EpicCommon / Null | Runtime, 표시 없음, Win64 등 | EOS 계정+게임 서비스 / 게임 서비스만 / 공통 / 서비스 없음 | EOS 선택 시 | 미사용 | 소스 `.uplugin` |
| OnlineSubsystemEOS | `Plugins/Online/OnlineSubsystemEOS`(Runtime, `IsBetaVersion=false`) | EOS용 OSSv1. 의존: EOSShared, EOSVoiceChat, SocketSubsystemEOS, EOSOverlayInputProvider, VoiceChat | EOS 로비·음성 선택 시 | 미사용 | 소스 `.uplugin` |
| EOSShared | `Plugins/Online/EOSShared`(Runtime) | EOS SDK 초기화·종료·설정(`[EOSShared]`, `[EOSSDK]`) | EOS 모든 경로의 기반 | `EOSVoiceChat` 의존으로 활성 | 소스 `.uplugin`, `EOSShared/Source` |
| SocketSubsystemEOS / NetDriverEOS | `Plugins/Online/SocketSubsystemEOS`(RuntimeNoCommandlet) | EOS P2P 소켓·NetDriver | EOS 전송 선택 시 | 미사용 | 소스 `.uplugin`, `Public/NetDriverEOS.h` |
| Online Beacons | `OnlineSubsystemUtils`(Runtime) | 게임 세션 밖의 가벼운 연결(예약·관전 비콘). 기본 `BeaconNetDriver`는 `IpNetDriver` | 예약/파티 사전 확인이 필요할 때 | 미사용 | 소스 `OnlineSubsystemUtils/Public/OnlineBeacon*.h`, `BaseEngine.ini` |
| Online Framework | `OnlineFramework`(Runtime 등, 기본 비활성) | Party, Lobby, Rejoin, Qos, PatchCheck 등 모듈 | Rejoin·Qos는 참고용 | 미사용 | 소스 `OnlineFramework.uplugin` |
| 세션용 Blueprint 프록시 | `OnlineSubsystemUtils`·`OnlineBlueprintSupport`(UncookedOnly) | Create/Find/Join/DestroySession, 업적·순위 호출 노드 | Blueprint 팀의 세션 작업 | 미사용(C++ 서브시스템 사용) | 소스 `OnlineSubsystemUtils/Classes/*CallbackProxy.h` |
| CommonUser 등 샘플 세션 코드 | — | — | — | 미확인(로컬 `Engine/Plugins`에서 해당 플러그인 설명자 미발견; Lyra 포함 여부도 미확인) | 미확인; 파일명 검색은 미지원 근거가 아님 |
| 엔진 VoIP (`IOnlineVoice`, `FVoiceEngineImpl`, Voice 채널) | OnlineSubsystem·OnlineSubsystemUtils·Engine | 로컬 마이크 캡처→게임 NetDriver Voice 채널→서버 중계→재생 | 호스트가 음성 흐름을 거치는 구조 | 채택(2026-11-04 사용자 결정), ini로 켬(2026-11-06) | 소스 `VoiceInterface.h`, `VoiceChannel.cpp`, `NetDriver.cpp` |
| Steam 음성 | OnlineSubsystemSteam | `FOnlineVoiceSteam`(`FOnlineVoiceImpl` 파생)+`FVoiceEngineSteam`(`FVoiceEngineImpl` 파생). Steam 음성은 엔진 VoIP 경로 위에 얹힌다 | Steam 판매 시 가장 직접적 | 엔진 VoIP를 켰으므로 Steam 모드에서 쓰이는 경로(Steam 실제 음성은 미검증) | 소스 `VoiceInterfaceSteam.h`, `VoiceEngineSteam.h` |
| `UVOIPTalker` | Engine(`Net/VoiceConfig.h`) | 플레이어 음성에 위치·감쇠·이펙트 부착, `GetVoiceLevel` | 근거리 음성(엔진 VoIP) | 사용(`UDigetherVoiceTalker`) | 소스 `Engine/Source/Runtime/Engine/Public/Net/VoiceConfig.h` |
| 서버 측 음성 라우팅 훅 | Engine | `UNetDriver::ReplicateVoicePacket`(가상), `UNetConnection::ShouldReplicateVoicePacketFrom`(비가상), `APlayerController::GameplayMutePlayer`/`ClientMutePlayer` | 누가 누구 음성을 받는지 서버에서 제한 | `GameplayMutePlayer`/`GameplayUnmutePlayer` 사용(`UDigetherVoiceRoutingComponent`). `ReplicateVoicePacket` 재정의는 미사용 | 소스 `NetDriver.h/.cpp`, `NetConnection.cpp`, `PlayerController.h` |
| VoiceChat 인터페이스 | `Plugins/Online/VoiceChat/VoiceChat`(**ClientOnly**) | `IVoiceChat`/`IVoiceChatUser`: 로그인, 채널 입장, 위치 채널, 장치, 음소거, 발화 이벤트 | 제공자 중립 음성 API | 간접 활성(`EOSVoiceChat` 의존) | 소스 `VoiceChat.uplugin`, `VoiceChat.h` |
| EOSVoiceChat | `Plugins/Online/VoiceChat/EOSVoiceChat`(**ClientOnlyNoCommandlet**, 기본 비활성, Android/Mac/Win64) | EOS RTC로 구현한 `IVoiceChat`; `Set3DPosition`은 미구현 | 위치 음성 API의 제공자 구현 한계 확인 대상 | 플러그인 활성, **설정 없음**; 선택한 음성 경로는 엔진 VoIP | 소스 `EOSVoiceChat.uplugin`, `EOSVoiceChat.cpp`, `Engine/Plugins/Online/VoiceChat/EOSVoiceChat/Source/EOSVoiceChat/Private/EOSVoiceChatUser.cpp` |
| EOS 로비 RTC 방 | OnlineSubsystemEOS | 세션 설정 `bUseLobbiesVoiceChatIfAvailable`가 EOS 로비의 `bEnableRTCRoom`이 됨 | 로비 단위 음성 | 미사용(값 false) | 소스 `OnlineSessionEOS.cpp` |
| 음성 설정 CVar | Engine | `voice.MicInputGain`, `voice.SilenceDetectionThreshold`, `voice.MicNoiseGateThreshold`, `voice.JitterBufferDelay`, `voice.NumChannels`, `voice.MuteAudioEngineOutput` | 마이크·재생 튜닝 | 미사용 | 소스 `VoiceConfig.cpp`, `VoiceEngineImpl.cpp` |
| Steam Audio | `Plugins/Runtime/Steam/SteamAudio`(**Beta**, "Deprecated" 명시) | 공간 오디오(음성 아님) | 사용하지 않는다 | 미사용 | 소스 `SteamAudio.uplugin` |

## 기능별 상세

### OSSv1 인터페이스와 Steam 구현 범위
- 인터페이스 헤더 목록(소스 `Engine/Plugins/Online/OnlineSubsystem/Source/Public/Interfaces`): Achievements, Chat, Entitlements, Events, ExternalUI, Friends, Groups, Identity, Leaderboard, Message, Party, Presence, Purchase, Session, SharedCloud, Sharing, Stats, StoreV2, Time, TitleFile, Tournament, TurnBased, UserCloud, User, Voice 등.
- Steam 구현이 값을 돌려주는 것(소스 `OnlineSubsystemSteam.cpp`): Session, Friends, SharedCloud, UserCloud, Leaderboards, Voice, ExternalUI, Identity, Purchase, StoreV2, Achievements, Presence. `nullptr`: Groups, Party, Time, TitleFile, Entitlements, Events, Sharing, User, Message, Chat, **Stats**, TurnBased, Tournament. 공식 문서도 "일부 인터페이스는 특정 서비스에만 있고 미지원 함수는 `false`를 돌려줄 수 있다"고 쓴다. (문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-in-unreal-engine)
- 공식 문서가 나열하는 Steam 지원 인터페이스는 Matchmaking(로비·GameServer API), Leaderboards, Achievements, Voice, UserCloud, SharedCloud, External UI다. (문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-steam-interface-in-unreal-engine)
- 기본 서비스 선택: `[OnlineSubsystem] DefaultPlatformService=Steam`. 이름을 주면 다른 서비스를 지연 로드한다. (문서·소스)

### OnlineSubsystemSteam 설정
- 프로젝트 `DefaultEngine.ini` (읽기만 함): `[OnlineSubsystem] DefaultPlatformService=Steam, bHasVoiceEnabled=true`(2026-11-04 조사 때는 false, 음성 1차 구현에서 켬), `[OnlineSubsystemSteam] bEnabled=true, SteamDevAppId=480, bUseSteamNetworking=true, bAllowP2PPacketRelay=true, bVACEnabled=false`, `[PacketHandlerComponents] +Components=OnlineSubsystemSteam.SteamAuthComponentModuleInterface`. 480은 공유 개발용 ID이며 출시 전 교체해야 한다(문서). 정식 App ID는 미결.
- 엔진 기본(소스 `BaseEngine.ini`): `bEnabled=false, SteamDevAppId=0, GameServerQueryPort=27015, bInitServerOnClient=false, bRelaunchInSteam=false, GameVersion=1.0.0.0, bVACEnabled=1, bAllowP2PPacketRelay=true, P2PConnectionTimeout=90`.
- 코드가 읽는 키(소스 `OnlineSubsystemSteam.cpp`): `SteamDevAppId`, `bRelaunchInSteam`(Steam에서 재실행), `bInitServerOnClient`. 문서: `bInitServerOnClient=true`는 Sessions(GameServer API)를 쓸 때만 필요하고 로비에는 불필요.
- 세션 설정 제약: Steam에서 `bUsesPresence`와 `bUseLobbiesIfAvailable`는 같은 값이어야 한다. 다르면 경고 후 동작이 달라진다. (소스 `OnlineSessionInterfaceSteam.cpp` 경고문) 프로젝트는 Steam 모드에서 둘 다 true, `-nosteam` 모드에서는 값이 다르다(Null/LAN이므로 Steam 코드 경로 아님).
- 문서: 로비는 참가 전에 핑·인원 같은 세부 정보를 노출하지 않는다. Steam 클라이언트가 실행 중이어야 한다. 출시 빌드에서 구독하지 않은 사용자는 종료된다. `steam_appid.txt`는 실행 파일 옆에 있어야 하며 엔진이 시작 시 만들고 정상 종료 때 지운다. (문서) 프로토타입 문서는 정식 배포 이미지에 이 시험 파일을 넣지 않는다고 이미 정해 두었다.
- Steam SDK 번들은 `ThirdParty/Steamworks/Steamv164`(SDK 1.64). (소스)

### SteamSockets
- `SteamSocketsNetDriver`는 `UNetDriver` 직접 상속(소스 `SteamSocketsNetDriver.h`). 플러그인 설명: "SteamSockets NetDriver 정의가 없으면 동작하지 않는다". (소스 `.uplugin`)
- 문서: 같은 SteamSockets 빌드끼리만 연결, 교차 플랫폼은 Windows·Mac·Linux, 비 Steam PC 스토어 빌드는 별도 설정, 리슨 서버는 접속 전 핑 계산 제공. (문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/using-steam-sockets-in-unreal-engine) 이 문서의 NetDriver 정의 위치(`[/Script/Engine.GameEngine]`)는 프로젝트·엔진 기본(`[/Script/Engine.Engine]`)과 다르다. 상세는 [NETWORKING.md](NETWORKING.md).
- Steam 판매 후 Steam 외 상점 동시 출시 시 NetDriver 정의를 빌드별로 나눠야 한다는 문서 경고가 있다. 다른 플랫폼 동시 출시는 미결.
- 방화벽·NAT 실제 성공은 프로젝트가 외부 PC로 시험한 적이 없다. 지원 사실과 실제 연결은 별개다.

### Online Services (OSSv2)
- 공식 문서 첫머리가 "Beta", "출하 시 주의"를 명시한다. (문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/online-services-in-unreal-engine) `OnlineServices.uplugin`은 `IsBetaVersion`을 표시하지 않았다. 문서가 Beta라 부르므로 `.uplugin` 표시만으로 안정판으로 읽지 않는다. (소스·문서)
- 인터페이스(소스 `OnlineServicesInterface/Public/Online`): Achievements, Auth, Commerce, Connectivity, ExternalUI, Leaderboards, Lobbies, Presence, Privileges, Schema, Sessions, Social, Stats, TitleFile, UserFile, UserInfo.
- 구현: `OnlineServicesEOS`(계정+게임 서비스), `OnlineServicesEOSGS`(게임 서비스만; Lobbies, Sessions, Stats, UserFile, TitleFile, PlayerReports, PlayerSanctions 헤더 확인), `OnlineServicesNull`, `OnlineServicesOSSAdapter`(OSSv1 감쌈). **Steam 전용 OSSv2 구현은 로컬 엔진에서 찾지 못했다.**
- `OnlineSubsystemUtils.uplugin`이 `OnlineServices` 플러그인을 의존으로 가진다. 프로젝트가 OSSv2 코드를 쓰지 않아도 플러그인 자체는 로드될 수 있다. 영향은 미확인.
- 서로 다른 인터페이스(OSSv1/OSSv2)의 샘플을 섞지 않는다는 기존 정리는 [UNREAL_FEATURE_COVERAGE](../../design/UNREAL_FEATURE_COVERAGE.md) 참조.

### EOS 계열
- `OnlineSubsystemEOS`: `IsBetaVersion=false`, 플랫폼 Win64/Mac/Linux/LinuxArm64/Android. 의존 플러그인에 EOSVoiceChat이 포함되어 있다. (소스)
- `EOSShared`는 EOS SDK 수명을 관리한다. 설정 키(소스 `EOSShared` 문자열): `[EOSShared]`·`[EOSSDK]`의 `ProductName`, `ProductId`, `SandboxId`, `DeploymentId`, `ClientId`, `ClientSecret`, `ClientEncryptionKey`, `bEnableOverlayIntegration`, `bEnableRTC`, `TickIntervalSeconds` 등. 값은 EOS 개발자 포털에서 발급받는다. 이 문서에 값을 적지 않는다.
- Steam 판매에서 EOS를 쓰면 Steam 계정과 EOS 사용자 ID 매핑(Connect 연동)과 EOS 오버레이 충돌 확인이 필요하다. (미확인; 소스에서 `EOSShared.uplugin`이 `SteamShared`를 선택 의존으로 가진다는 것만 확인)
- 결정 상태: 세션 서브시스템 Steam vs EOS는 미결([DECISIONS](../../planning/DECISIONS.md) D-003). 음성 방식은 이후 2026-11-04 사용자 결정으로 엔진 기본 VoIP를 선택했으므로 세션 제공자 결정과 구분한다.

### 엔진 VoIP 경로 (Steam 음성 포함)
- 켜는 조건(소스): `[OnlineSubsystem] bHasVoiceEnabled=true`(엔진 기본값 true), `BaseEngine.ini [Voice] bEnabled=false`(엔진 기본). 프로젝트 `DefaultEngine.ini`는 현재 두 값을 모두 true로 둔다(조사 당시에는 앞의 값을 false로 덮고 `[Voice]`는 덮어쓰지 않았다). `FVoiceModule`이 `GConfig [Voice] bEnabled`를 읽는다(소스 `Engine/Source/Runtime/Online/Voice/Private/VoiceModule.cpp`). 두 값 모두 정해야 한다. `[/Script/Engine.GameSession] bRequiresPushToTalk`(`DefaultGame.ini`)도 읽는다.
- 한도: `MaxLocalTalkers`, `MaxRemoteTalkers`(엔진 기본 1/16), `VoiceNotificationDelta`, `PollingIntervalInMs`. (소스 `BaseEngine.ini` `[OnlineSubsystem]`)
- 흐름(소스): 클라이언트 `ProcessLocalClientPackets`가 서버 연결의 Voice 채널로 보내고, 서버의 `UVoiceChannel::ReceivedBunch`가 `SerializeRemotePacket`로 로컬 음성 처리(호스트 재생)에 넘긴 뒤 `UNetDriver::ReplicateVoicePacket`로 다른 접속에 중계한다. 중계 조건은 수신자 PC의 `MuteList` 핸드셰이크 완료 여부와 `IsPlayerMuted`다(`UNetConnection::ShouldReplicateVoicePacketFrom`).
- 서버 훅: `ReplicateVoicePacket`이 `virtual`이므로 NetDriver 서브클래스로 대상 필터링(거리·관전 등 정책)을 넣을 수 있다. 단 프로젝트 NetDriver가 Steam 소켓 드라이버라 서브클래스를 어느 쪽에 둘지 설계가 필요하다(미확인). 대안은 서버에서 `GameplayMutePlayer`/`GameplayUnmutePlayer`(PC 단위 음소거)로 청취 대상을 바꾸는 방식이다. (소스 `PlayerController.h`)
- 재생: `UVOIPTalker`는 `RegisterWithPlayerState`로 플레이어 음성에 연결하고 `FVoiceSettings{ComponentToAttachTo, AttenuationSettings, SourceEffectChain}`로 3D 위치·감쇠를 준다. 컴포넌트를 지정하지 않으면 공간화되지 않는다. `VoipListenerSynthComponent`(OnlineSubsystemUtils)가 재생 합성 컴포넌트다. (소스)
- 호스트가 몬스터 감지용으로 발화 여부·크기를 알 수 있는 후보: `IOnlineVoice::IsRemotePlayerTalking`, `GetAmplitudeOfRemoteTalker`, `UVOIPTalker::GetVoiceLevel`. 호스트는 모든 패킷이 거쳐 가는 노드다. 실제 값이 호스트에서 유효한지 시험은 PQ-13 항목이다. (소스·미검증)
- 주의: Steam 판매 시 Steam OSS 음성의 품질·지연·샘플레이트는 실측하지 않았다. 4인 규모의 대역폭 영향도 미측정.

### VoiceChat 인터페이스 / EOSVoiceChat
- 모듈 타입: `VoiceChat` = **ClientOnly**, `EOSVoiceChat` = **ClientOnlyNoCommandlet**, EOSVoiceChat의 플랫폼 허용 목록은 Android/Mac/Win64(Linux 없음)다. 전용 서버 타깃에는 포함되지 않는다. 클라이언트 실행 파일의 리슨 서버는 모듈 타입상 제외되지 않지만 실제 초기화·송수신은 미검증이다. (소스 각 `.uplugin`)
- API(소스 `VoiceChat.h`): 초기화/연결, `Login`(플랫폼 사용자, 이름, 자격), `JoinChannel(이름, 자격, EVoiceChatChannelType, 콜백, 3D 속성)`, `Set3DPosition`, `TransmitToAllChannels/None/SpecificChannels`, `SetPlayerMuted`, `SetPlayerVolume`, `IsPlayerTalking`, `OnVoiceChatPlayerTalkingUpdated`, 장치 열거·선택, 녹음/렌더 전후 후크 델리게이트.
- 인터페이스의 3D 채널 속성: 감쇠 모델(None/InverseByDistance/LinearByDistance/ExponentialByDistance), MinDistance, MaxDistance, Rolloff. 다만 **로컬 5.8.4 `FEOSVoiceChatUser::Set3DPosition` 본문은 `// Unimplemented`인 빈 함수**다. `FVoiceChatChannel3dProperties`·`Set3DPosition` 선언만으로 EOS RTC의 공간 감쇠나 위치 보고 동작을 지원한다고 판단할 수 없다. (소스 `VoiceChat.h`, `EOSVoiceChatUser.cpp`)
- EOS RTC 설정: `[EOSVoiceChat]`의 `bEnabled`(코드 기본 true), `PlatformConfigName`, `ProductId`, `SandboxId`, `DeploymentId`, `ClientId`, `ClientSecret`, `ClientEncryptionKey`, `RTCBackgroundMode`, `RTCSettings`. 프로젝트에는 이 섹션이 없다. 설정이 비었을 때 초기화 결과는 미확인. `InsecureGetLoginToken`/`InsecureGetJoinToken`은 이름 그대로 개발용(클라이언트 비밀 사용)이며 출시에는 서버가 발급하는 토큰 경로가 필요하다. (소스 `EOSVoiceChat.cpp`, `VoiceChat.h`)
- EOS 방 단위 음성은 두 방법이다. ① OSS EOS 세션에서 `bUseLobbiesVoiceChatIfAvailable`를 켜면 로비 RTC 방이 생긴다(소스 `OnlineSessionEOS.cpp`). ② 게임이 직접 `JoinChannel`로 방을 관리한다.
- 호스트가 다른 사람의 음성 활동을 알 수 있는가는 이 API상 **채널에 속한 각 클라이언트가 이벤트를 받는 구조**로 보이며, 서버 권위로 모든 발화를 판정하려면 별도 검증이 필요하다. 미검증(PQ-13).

### 기획 시험값 (2026-11-04 답변 초안)

2026-11-04 기획 답변(초안, G **제안**)의 시험값이다. 플레이테스트에서 조정하며 Notion 승인본이 아니다. 근거리 음성을 쓰는 모든 곳(같은 구역의 생존자끼리, 로비, 실패 연출)에 같은 값을 쓴다. 상태별 규칙은 기획 공유 사양 2026-11-05를 따른다(기획 기준(구현 진행 중)): 같은 구역 생존자는 근거리 음성(벽·층 추가 감쇠 없음), 갱도↔던전은 입구 바로 안팎에서도 양방향 차단, 관전자(사망·1차 탈출·긴급 탈출)는 관전자끼리 대화하고 생존자 목소리를 듣지만 생존자는 관전자 목소리를 듣지 못함, 검은 정산 화면까지 구분 유지·거점 복귀 순간 해제, 로비·실패 연출·흡입 대기는 근거리, 결과 크레딧 시작 순간 전원 음성, 생존자 화면에는 발화자 이름·마이크 아이콘·위치 화살표 없음(관전자 화면에는 다른 관전자 발화자만), 매몰자는 먹먹하게 처리하고 던전 경계 차단이 감쇠보다 우선, 마이크가 없거나 끊겨도 세션 유지. 기획 문장은 음성 구현 방식을 "아직 미정"으로 적었지만, 프로젝트의 구현 방식은 **2026-11-04 사용자 결정으로 엔진 기본 VoIP**이며 이 답변이 그 결정을 바꾸지 않는다.

| 항목 | 시험값 | ID |
|---|---|---|
| 원래 크기로 들리는 거리 | 3m까지 | `VOICE_RANGE_FULL`(새로 만들 예시 이름) |
| 최대 거리 | 25m, 그보다 멀면 들리지 않음 | `VOICE_RANGE_MAX`(시트 기존) |
| 줄어드는 방식 | 가까울 때 빠르게, 멀수록 천천히 작아지는 자연 감쇠. 곡선(R11)과 런타임 조정(R04)으로 만들면 시험 중 바로 맞출 수 있음 | `VOICE_FALLOFF_CURVE`(예시) |
| 방향감 | 켬. 화면에 말하는 사람 표시 없이 목소리로 동료 위치를 찾는 것이 기획 의도 | — |
| 벽·층 | 같은 구역 안에서는 더 줄이지 않음. 갱도와 던전 사이는 거리와 무관하게 들리지 않음 | — |
| 매몰된 사람 | 먹먹하게(고음 감소), 최대 거리 절반(약 12m) | `VOICE_BURIED_RANGE_RATIO`(예시) |

예시 ID 이름은 개발 규칙에 맞춰 바꿀 수 있다. 엔진 VoIP 경로에서는 거리·방향감은 수신자 `UVOIPTalker`의 감쇠 설정, 구역 차단은 서버 중계 필터 후보와 연결된다(아래 고려점). 실제 동작은 M3로 확인한다.

### 수신 제한·관전자 음성 방식 (2026-11-06 사용자 결정)

Notion 승인본이 아닌 사용자 결정이며 1차 구현했다([구현 현황](#구현-현황-2026-11-06)).

- **수신 제한**: 서버가 수신자 PC마다 `GameplayMutePlayer`/`GameplayUnmutePlayer`로 들을 수 없는 발화자를 음소거한다. 결정은 구역 이동·사망·탈출·관전 대상 변경·정산 화면·거점 복귀·크레딧 시작 같은 상태 변화 때 다시 계산하는 것이다. 1차 구현은 상태 변화 지점에서 부르지 않고 0.25초 타이머로 전체를 다시 계산하므로, 상태가 바뀐 직후 최대 한 주기 동안 이전 수신 제한이 남는다. `ReplicateVoicePacket` 재정의(NetDriver 상속)는 거리 밖 패킷 차단이 필요해질 때만 추가 검토한다. 호스트 자신의 청취에도 음소거가 적용되는지는 실행 확인이 필요하다.
- **관전자 → 관전자**: 위치·구역과 무관하게 전원 들림, 거리 감쇠·방향감 없음(수신 측 `UVOIPTalker`에 `ComponentToAttachTo` 미지정).
- **생존자 → 관전자**: 관전 대상이 들을 수 있는 생존자만 들림(대상과 같은 구역, 갱도↔던전 차단 유지). 서버가 `SpectateTargetPlayerKey` 기준으로 계산한다. 감쇠·방향감은 관전 대상 위치 기준(근거리 시험값과 같음)이며, 관전자 PC의 청취 위치를 `SetAudioListenerOverride`로 관전 대상에 붙이고 대상이 바뀔 때 다시 붙인다.
- **관전자 → 생존자**: 들리지 않음(기존 규칙).
- **마이크 입력**: 기본은 항상 열린 마이크, 설정에서 개인별로 누르고 말하기 또는 토글(같은 키를 한 번 누르면 켜짐, 다시 누르면 꺼짐, 11-06 답변 14 마이크 온오프 요청에 대한 2026-11-06 사용자 선택)을 고를 수 있다(2026-11-06 사용자 결정). 누르고 말하기 임시 기본 키는 V(2026-11-06 사용자 결정, 기획 확정 아님). 기존 설정 값 `UDigetherGameUserSettings::MicrophoneMode`(기본 `AlwaysOn`, 설정 화면 콤보 있음)를 음성에 연결한다. 엔진 `bRequiresPushToTalk`는 호스트가 접속 시 모든 참가자에게 `ClientEnableNetworkVoice`로 보내는 게임 전체 값이라 개인 설정으로 쓰지 않는다. `false`로 두고 클라이언트가 자기 설정에 따라 `ClientEnableNetworkVoice` 뒤에 다시 적용하며, 누르고 말하기일 때 `StartTalking`/`StopTalking`을 호출한다(소스 `GameModeBase.cpp`, `PlayerController.cpp`).

### 구현 현황 (2026-11-06)

| 부분 | 위치 | 상태 |
|---|---|---|
| 판정 규칙 | `Game/Voice/DigetherVoiceRules` | 관전자끼리 전원, 관전자→생존자 차단, 같은 구역 생존자, 관전 대상 구역 기준, 크레딧 전원. 자동 테스트 `DIGETHER.Game.Voice.Hearing`·`MuteDiff` 통과 |
| 서버 수신 제한 | `UDigetherVoiceRoutingComponent`(GameplayGameMode), 명단 `DigetherGameplayVoice.cpp` | 0.25초마다 듣는 PC별 원하는 음소거와 비교해 `GameplayMutePlayer`/`GameplayUnmutePlayer` 적용 |
| 클라 재생·마이크 | `UDigetherVoiceComponent`(Gameplay·Lobby PC) | 생존자 목소리는 몸에 붙인 감쇠(3m/25m, 자연 감쇠, 방향감), 관전자·몸 없는 참가자(대기실 포함)는 감쇠 없음. 관전 중 거리 기준 위치는 `SetAudioListenerAttenuationOverride`로 관전 대상. 마이크 모드는 `ClientEnableNetworkVoice` 대체로 적용하고 0.25초마다 설정 변경 반영 |
| 누르고 말하기 입력 | 같은 컴포넌트의 `PushToTalkAction`·`VoiceMappingContext` | 에셋 비어 있으면 DIGETHER Options `FallbackPushToTalkKey`(V)로 실행 중 액션 생성, 이 경우 키 재설정 목록에는 없음(저장이 에셋 경로 기준). 에셋 지정·Player Mappable 설정 후 키 재설정 가능. 콘솔 `Digether.VoicePushToTalk 1|0` 유지 |
| 동료별 음량·나만 음소거 | `UDigetherVoiceComponent::SetPlayerVoiceVolume`·`SetPlayerVoiceMuted`, `UDigetherVoiceTalker::OnTalkingBegin`의 오디오 컴포넌트 `SetVolumeMultiplier` | 이 PC만·접속 중만 유지(저장 안 함), 재생 음량 = 설정 목소리 볼륨 × 플레이어별(0~2, 시험값). 실제 음성으로 미검증 |
| 마이크 장치 선택 | 미지원 | 엔진 VoIP가 캡처를 기본 장치(`CreateVoiceCapture("")`)로 만들고 바꾸는 공개 API가 없음(소스 `VoiceEngineImpl.cpp:363`). 윈도우 기본 입력 장치를 따름 |
| 호스트 발화 감지 시험(PQ-13) | 콘솔 `Digether.VoiceProbe 1`(Shipping 제외) | 호스트 로그 `LogDigetherVoice`에 참가자별 `talking`·`amplitude`·`hostMuted` 기록. 미실행 |

한계와 확인할 것:
- **갱도·던전 구역 구분이 코드에 없다.** `ResolveVoiceZone`은 모두 같은 구역(0)을 돌려준다. 구역 판정이 생기면 그 값을 연결한다.
- **매몰자 먹먹함 미구현.** 게임 코드에 매몰 상태가 없다. 상태가 생기면 `UVOIPTalker::Settings.SourceEffectChain`(고음 감소)과 감쇠 반경 0.5배로 연결한다.
- **호스트가 음소거한 발화자는 호스트에서 발화 감지가 안 될 수 있다.** 엔진 `FOnlineVoiceImpl::SerializeRemotePacket`이 로컬 음소거 대상 패킷을 재생 목록에 넣지 않고 `IsRemotePlayerTalking`도 거짓이 된다(소스). 몬스터 음성 감지가 모든 발화자를 봐야 하면 별도 경로(NetDriver `ReplicateVoicePacket`의 패킷 진폭 등)가 필요하다. 시험 로그의 `hostMuted=1` 행으로 확인한다.
- **호스트 로컬 음소거는 온라인 세션이 있어야 적용된다.** `FOnlineVoiceImpl::MuteRemoteTalker`는 세션이 없으면 건너뛴다(소스). 세션 없이 여는 PIE에서는 호스트 청취 제한이 동작하지 않을 수 있다.
- `UVOIPTalker` 설정은 다음 발화 시작부터 적용된다(엔진 규칙). 말하는 도중 관전 전환·몸 교체는 그 발화가 끝난 뒤 반영된다.
- **음성은 온라인 세션이 있어야 동작한다.** 엔진 Null·Steam 세션이 생성·참가 때 로컬·원격 발화자를 등록한다(소스 `OnlineSessionInterfaceNull.cpp`, `OnlineSessionInterfaceSteam.cpp`). `?listen` + IP 직접 접속 시험에서는 `StartLocalProcessing` 실패를 확인했고, `RoomHost`/`RoomJoin` 세션 경로에서는 등록·처리 시작 성공을 확인했다(2026-11-06 같은 PC 2프로세스, 실제 발화 없음).
- 실제 마이크 음성 송수신·두 PC 이상 Steam 연결(M3) 미검증.

### 근거리 음성 구현 시 고려점 (정책은 확정하지 않음)
- 엔진 VoIP 경로: 서버 중계 필터(`ReplicateVoicePacket`)와 수신자 재생 감쇠(`UVOIPTalker`)를 분리해 생각한다. 서버 필터는 네트워크 비용·정보 노출을, 클라 감쇠는 들리는 방식을 정한다.
- VoiceChat 인터페이스는 채널 구분과 Positional/NonPositional 채널 타입을 노출한다. EOSVoiceChat은 `Set3DPosition`이 미구현이므로 공간 음성을 제공한다고 가정하지 않는다. 채널 수·전환 지연·입장 비동기도 실행 검증 대상이다.
- 맵 이동 시(비Seamless) 음성 세션이 끊기는지, 재접속 시 채널 복귀가 자동인지는 미확인이다. [네트워크 권위](../NETWORK_AUTHORITY.md)의 "외부 서비스 잔류 검사"가 적용된다.
- 이탈한 사람의 음성 상태(비활성 PlayerState)와 UniqueId 매핑은 [NETWORKING.md](NETWORKING.md) 재접속 절을 따른다.
- 입력 장치 선택, 푸시투토크, 마이크 시험 UI는 [UI 카탈로그](UI.md)와 [입력 카탈로그](INPUT.md)에서 별도로 본다(해당 문서 존재 여부는 README 색인 기준).
- 접근성·개인정보: 음성 녹음은 하지 않는다는 정책이 필요하면 `StartRecording`·후크 델리게이트 사용 범위를 정해야 한다.

### Steam Cloud와 SaveGame 연결점
- OSS Steam은 `IOnlineUserCloud`(`EnumerateUserFiles`, `ReadUserFile`, `WriteUserFile`, `ClearFile`)와 `IOnlineSharedCloud`를 구현한다. 파일 내용을 바이트 배열로 넘기는 방식이다. (소스 `OnlineUserCloudInterfaceSteam.h`)
- `USaveGame`/로컬 슬롯 저장과 Steam Cloud는 별개 계층이다. 로컬 저장 파일(SaveGame 직렬화 바이트)을 Cloud에 동기화하려면 파일명 규칙, 충돌 해결, 쓰기 중 종료 시 이전 유효 파일 보존을 직접 설계해야 한다([SYSTEM_DESIGN](../../SYSTEM_DESIGN.md)의 저장 항목). Steam 자체 자동 클라우드(Steamworks 설정으로 경로 동기화)와 이 API 중 무엇을 쓰는지는 결정이 필요하다. Steam 파트너 사이트의 클라우드 쿼터 설정은 이번에 확인하지 않았다. (미확인)
- 저장 계층은 [DATA_SAVE.md](DATA_SAVE.md)가 다룬다.

## 보류·비추천 후보와 이유
- **Steam Audio 플러그인**: 자기 설명에 "Deprecated"와 Beta가 적혀 있고 음성 기능이 아니다.
- **SocketSubsystemSteamIP**: NAT 통과가 없다고 스스로 설명한다. 현재 프로젝트는 SteamSockets를 사용하며 외부 PC 연결은 별도 검증 대상이다.
- **Online Services를 지금 도입**: Beta이며 Steam 전용 구현 플러그인은 로컬 검색에서 미발견이다. OSSv1 이행 필요가 생기면 `OnlineServicesOSSAdapter`부터 비교.
- **Multi-Server/Party 기능**: 1~4인 호스트형 방과 무관.
- **EOS 전환**: 엔진 VoIP가 선택됐지만 `EOSVoiceChat`은 설정 없이 켜져 있다. 비활성화는 의존성과 초기화 영향 확인 후 별도 변경으로 다루며 이 조사에서 설정을 바꾸지 않는다.

## 확인 한계
- Epic 문서는 Online Subsystem 개요, Steam 인터페이스, Steam Sockets, Online Services 개요만 본문을 읽었다. `using-voice-chat-in-unreal-engine`은 목차만 추출됐고 EOS 음성 설정 문서는 열지 못했다. 음성 설정 서술은 소스 근거다.
- EOS 경로(OnlineSubsystemEOS/OnlineServicesEOS)는 `.uplugin`과 일부 헤더·설정 키만 읽었고 실행하지 않았다.
- Steam 음성의 실제 코덱·샘플레이트·품질, 호스트가 얻는 발화 정보, 맵 이동 중 음성 유지는 실행으로 확인하지 못했다.
- Steam Cloud API 실제 동작과 Steamworks 파트너 설정은 확인하지 못했다.
- `OnlineSubsystemSteam` 모듈을 `DIGETHER.Build.cs`에 넣지 않은 현재 상태가 패키지에서 충분한지는 이 조사 범위 밖이다(프로토타입 패키지가 호스트 listen까지 성공한 기록만 있다).
- 이 문서는 지원 사실이며 채택·구현·검증 완료를 뜻하지 않는다.
