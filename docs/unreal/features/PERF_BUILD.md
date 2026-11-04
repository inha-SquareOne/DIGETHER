# 성능·빌드·배포 기능 카탈로그

기준일 2026-11-04 · UE 5.8.4 (CL 58902052) · 상태: **조사 자료, 채택 결정 아님**. 색인은 [기능 카탈로그](README.md).

근거 표기: `소스`=로컬 `Engine/...` 직접 확인, `문서`=Epic 문서를 실제로 열어 확인, `미확인`=이번에 확인 못 함. 성능 조사 절차·예산 규칙은 [성능 조사와 변경](../PERFORMANCE.md), 검증 구분은 [검증 계획](../../verification/VERIFICATION_PLAN.md)(VT-12), 패키징 실적은 [개발 도구 안내](../../tools/DEVELOPER_TOOLS.md#후속-실기-검증), Steam 프로토타입은 [PROTOTYPE_STEAM](../../prototype/PROTOTYPE_STEAM.md), 출시 양식은 [릴리스 운영 템플릿](../../templates/RELEASE_OPERATIONS_TEMPLATE.md)을 따른다. 성능 수치·예산은 프로젝트에서 **미정**이므로 이 문서는 수치를 적지 않는다.

## 요약
- 측정 1순위는 **Unreal Insights**(Timing·Memory·Networking·Asset Loading). 한 번의 트레이스에 CPU/GPU/메모리/네트워크 채널을 묶을 수 있다. 굴착 계산·충돌 생성·내비 갱신·복제 비용을 나누어 볼 수 있다(UNREAL_FEATURE_COVERAGE도 같은 방향).
- 빠른 확인용은 `stat unit`/`stat game`/`stat net`과 `ProfileGPU`. 모두 **Shipping에서는 대부분 빠진다**(`Misc/Build.h`). 측정 구성은 Development(또는 Test)로 고정.
- 반복 비교는 **CSV Profiler**(`CsvProfile` 콘솔, `CSV_*` 매크로)가 후보. 자동화는 `AutomatedPerfTesting`(Experimental)이 있으나 보류.
- **Significance Manager**는 자체 기본 꺼짐이며 Beta/Experimental 값이 false인 플러그인이다. `.uproject` 명시 활성과 소스 사용처는 없고 실제 로드는 미검증이다. 몬스터·장식물 비용 단계화의 후보지만 채택은 측정 이후.
- 이번 범위에서 범용 Actor/UObject 풀은 확인하지 못했다. Niagara 컴포넌트 풀·Mass 액터 풀 인터페이스 외에 UMG `FUserWidgetPool`도 제공되며 프로젝트 UI 재사용 테스트가 이를 쓴다. 범용 풀의 부재를 단정하지 않으며 추가 풀링은 측정 후 판단한다.
- 패키징은 `RunUAT BuildCookRun` + Project Launcher. 프로젝트는 Win64 Development 전체 패키징 실적이 있다. **Shipping·Test 구성은 미검증**이고, Steam 쪽은 `bRelaunchInSteam` 미설정·AppID 480(개발용) 상태다.

## DIGETHER에서 쓰이는 곳
| 항목 | 상태 |
|---|---|
| Insights·stat·CSV | 도구 사용 기록 없음. [VT-12](../../verification/VERIFICATION_PLAN.md) 성능·패키지 검증은 예산 미정으로 대기 |
| Tick | `Source/DIGETHER`에서 `PrimaryActorTick.bCanEverTick` true 14곳·false 10곳, `PrimaryComponentTick.bCanEverTick` true 1곳·false 7곳. `SetActorTickInterval`·Tick 그룹 지정은 없음 |
| 네트워크 갱신 | 프로토타입 `DigetherExcavationTerrain`이 `SetNetUpdateFrequency(10.f)`, 몬스터 체력 컴포넌트가 `FlushNetDormancy()` 사용 |
| 풀링·Significance | UI의 `DigetherFrontendWidgetReuseTest`가 `FUserWidgetPool` 사용. Niagara/Mass 풀·Significance 소스 사용처는 미발견. SignificanceManager는 자체 기본 꺼짐, `.uproject` 명시 없음(실제 로드 미검증) |
| 패키징 | DefaultGame.ini에 `[/Script/UnrealEd.ProjectPackagingSettings]` 일부만 지정: `InternationalizationPreset=All`, `CulturesToStage` 9개, `DirectoriesToNeverCook=/Game/Editor/DevelopmentWorkbench`, `DirectoriesToAlwaysCook=/Game/Frontend`. Pak/IoStore/Zen/압축/청크는 엔진 기본값. 검증 기록: Win64 Development BuildCookRun exit 0, Cook 773개(5.8.2 실행, 5.8.4 재실행은 미확인) |
| 타깃 | `DIGETHER.Target.cs`(Game)·`DIGETHEREditor.Target.cs`(Editor), 추가 규칙 없음 |
| Steam | `SteamSockets` 활성(의존으로 `OnlineSubsystemSteam`·`SteamShared`). DefaultEngine.ini `[OnlineSubsystemSteam]`에 `bEnabled=true`, `SteamDevAppId=480`, `bUseSteamNetworking=true`, `bVACEnabled=false` |
| 스케일러빌리티·디바이스 프로파일 | 프로젝트에 `DefaultScalability.ini`·`DefaultDeviceProfiles.ini` 없음(엔진 기본) |
| 렌더링 설정 | Ray Tracing·Substrate 켜짐, 기본 RHI DX11(`DefaultGraphicsRHI_DX11`), D3D12는 SM6 지정 — 상세는 [RENDERING_ART.md](RENDERING_ART.md) |
| Crash Reporter | 프로젝트에서 설정 안 함. 엔진 기본 `IncludeCrashReporter=False` |
| DDC | 엔진 기본(로컬 Zen). 팀 공유 DDC 설정은 프로젝트에 없음 |

## 기능 목록
이 문서의 `미사용`은 별도 근거가 없으면 `DIGETHER/Source`·`Config`의 사용처·실행 기록 미발견을 뜻하며, Blueprint·에셋 내부 사용과 런타임 모듈 로드는 미검증이다. `EnabledByDefault`와 의존 활성화는 실제 로드·게임 사용과 구분한다.

| 기능 | 모듈·플러그인 (타입, 안정성) | 하는 일 | DIGETHER 쓸모 | 프로젝트 상태 | 근거 |
|---|---|---|---|---|---|
| Unreal Insights (Timing) | `TraceInsights`·`TraceServices`, 실행 파일 `UnrealInsights.exe`, `UnrealTraceServer.exe` | CPU/GPU 타임라인, 프레임·타이머·호출자 | 프레임 시간 분해 | 미사용 | 소스·문서 |
| Memory Insights / LLM | Insights + Core `LowLevelMemTracker`, 트레이스 채널 `memtag,memalloc,callstack,module` | 모든 할당 추적, 태그별 메모리 | 메모리 증가·누수 조사 | 미사용 | 소스·문서 |
| Networking Insights | `NetworkingProfiler` 뷰 + `Net` 채널 | 패킷·연결·복제 오브젝트 | 호스트 업링크 4인 측정 | 미사용 | 소스·문서 |
| Asset Loading / Cooking / Slate / Audio / IoStore Insights | `loadtime,assetloadtime`, `IoStoreInsights`(기본 켜짐), Slate·Audio 플러그인 | 로드 시간·쿡 시간·UI 갱신 | 로딩 중 끊김·쿡 시간 | 미사용 | 문서·소스 |
| 트레이스 제어 | Core `TraceAuxiliary` | 콘솔 `Trace.File/Send/Stop/Pause/Resume/Status/Enable/Disable/Bookmark/SnapshotFile`, 명령줄 `-trace=`, `-tracefile=`, `-tracehost=`, `-tracefiletimestamps` | 패키지에서 캡처 | 미사용 | 소스 |
| `stat` 명령 | Engine 통계 | `stat unit/unitgraph/fps/game/net/memory/llm/ai/anim/physics/…` | 즉석 확인 | 미사용 | 소스 |
| `ProfileGPU` | `RHI` `GPUProfiler` | 한 프레임 GPU 작업을 로그로 출력 | GPU 병목 1차 | 미사용 | 소스 |
| CSV Profiler | Core `ProfilingDebugging/CsvProfiler.h` | 프레임별 통계를 CSV로 기록 | 반복 비교 측정 | 미사용 | 소스 |
| `MemReport` | Engine 콘솔 명령 | 메모리 보고서 출력 | 메모리 스냅샷 | 미사용 | 소스 |
| Significance Manager | 플러그인 `SignificanceManager`(Runtime, 기본 꺼짐, Beta 아님) | 뷰포인트 기준 중요도 계산 후 동작 수준 조절 | 몬스터·동료 연출 비용 단계화 | 미사용(미활성) | 소스 |
| Tick 제어 | Engine `Actor.h`, `EngineBaseTypes.h` | `SetActorTickInterval`, `SetActorTickEnabled`, Tick 그룹, `bAllowTickBatching`, `bAllowTickOnDedicatedServer` | 불필요 Tick 감소 | 일부(bCanEverTick) | 소스 |
| 복제 빈도·휴면 | Engine `NetDriver`·Actor | `SetNetUpdateFrequency`, `SetNetDormancy`/`FlushNetDormancy`, `net.DormancyEnable` | 대역폭 | 일부 사용 | 소스 |
| ReplicationGraph | 플러그인(Runtime, **Beta**, 기본 꺼짐) | 복제 대상 선별 최적화 | 소규모 4인에서는 보류 | 미사용 | 소스 |
| 오브젝트 풀링 | Niagara `UNiagaraComponentPool`(`ENCPoolMethod`), `MassActorPoolableInterface`(MassGameplay **Experimental**), UMG `FUserWidgetPool` | 이펙트/Mass 액터/위젯 재사용 | 용도별 풀. 범용 Actor/UObject 풀 제공 여부는 미확인 | UI 재사용 테스트에서 FUserWidgetPool 사용, Niagara/Mass 풀 소스 사용처 미발견 | 소스 `Engine/Plugins/FX/Niagara/Source/Niagara/Public/NiagaraComponentPool.h`, `Engine/Source/Runtime/UMG/Public/Blueprint/UserWidgetPool.h`; 프로젝트 `DIGETHER/Source/DIGETHER/Game/Tests/DigetherFrontendWidgetReuseTest.cpp` |
| 비동기 로딩 | Engine `FStreamableManager::RequestAsyncLoad`, `UAssetManager::LoadPrimaryAssets`, Core `LoadPackageAsync` | 소프트 참조·Primary Asset 비동기 로드 | 정의 에셋·장비 지연 로드 | Primary Asset 규칙만 설정 | 소스 |
| 스트리밍 설정 | `[/Script/Engine.StreamingSettings]`, `s.*` CVar | 비동기 로드 스레드·시간 제한 | 로딩 끊김 조정 | 엔진 기본 | 소스 |
| Size Map·Asset Audit | `AssetManagerEditor`(프로젝트 활성) | 에셋 용량·참조 | 패키지 크기 조사 | 활성 | 소스 |
| Project Launcher | 플러그인 `ProjectLauncher`(Editor, **Beta**, 기본 켜짐), 모듈 `LauncherServices`·`LegacyProjectLauncher` | 빌드·쿡·배포 프로파일 | GUI 패키징 | 미확인 | 소스 |
| UAT `BuildCookRun` | `Engine/Build/BatchFiles/RunUAT.bat` | 빌드·쿡·스테이지·팩·아카이브 일괄 | CLI/CI 패키징 | **사용 기록 있음** | 소스·프로젝트 기록 |
| Cook 규칙 | `ProjectPackagingSettings`, Asset Manager | Always/NeverCook 디렉터리, `bCookAll`, `bCookMapsOnly`, `bSkipEditorContent` | 쿡 범위 제어 | Always/Never 사용 | 소스 |
| Pak / IoStore / Zen | `ProjectPackagingSettings` | `UsePakFile`, `bUseIoStore`, `bUseZenStore`(쿡 산출 저장소), `bCompressed`, `bGenerateChunks` | 배포 형식 | 기본값 사용 | 소스 |
| 빌드 구성 | UBT `UnrealTargetConfiguration`, 패키징 설정 | Debug/DebugGame/Development/Test/Shipping | 구성별 기능 차이 | Development만 검증 | 소스 |
| Steam 엔진 측 | `OnlineSubsystemSteam`(Runtime, 기본 꺼짐), `SteamShared`, `SteamSockets`, Steamworks SDK **1.64** | 로그인·세션·네트워킹 | Steam 출시 | 프로토타입(AppID 480) | 소스 |
| Crash Reporter | `CrashReportClient.exe`, `[CrashReportClient]` | 크래시 수집·업로드 | 출시 후 대응 | 미설정 | 소스 |
| 디버그 심볼 | UAT `-separatedebuginfo`/`-nodebuginfo`, `IncludeDebugFiles` | PDB 포함·분리 | 덤프 해석 | 미설정 | 소스 |
| Derived Data Cache | `DerivedDataCache`, `Zen` | 로컬/공유 DDC, Zen 서버 | 팀 셰이더·텍스처 캐시 | 엔진 기본 | 소스 |
| 셰이더 컴파일 | `ShaderCompileWorker`, 공유 머티리얼 셰이더 코드 | 셰이더 빌드·패키지 포함 | 첫 실행 대기·패키지 크기 | 기본값 | 소스 |
| PSO 프리캐싱·파이프라인 캐시 | Engine `PSOPrecache`, RHI `PipelineStateCache`·`PipelineFileCache`, RenderCore `ShaderPipelineCache` | 런타임 PSO 사전 컴파일, 파일 캐시 | 첫 등장 시 끊김 방지 | 기본값 | 소스 |
| Scalability | `Config/BaseScalability.ini`, Engine `Scalability` | `sg.*` 품질 그룹 0~3, Cine | 옵션 메뉴 | 엔진 기본 | 소스 |
| Device Profile | `BaseDeviceProfiles.ini`, `DeviceProfileEditor` | 장치 등급별 CVar 묶음 | 최소 사양 대응 | 엔진 기본 | 소스 |

## 기능별 상세

### Unreal Insights와 트레이스
- 문서: Timing(CPU/GPU, 프레임·타이머·호출 관계), Memory(모든 할당·재할당·해제와 콜스택), Networking(네트워크 트래픽 분석·최적화·디버깅), Asset Loading(로드 시간·순서·비동기 우선순위, `AssetLoadTime` 채널), Cooking, Slate/UMG, Audio Insights가 소개됨. 에디터 하단 Trace 위젯에서 시작하며 구성별 옵션은 페이지가 다른 문서로 위임함 — https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-insights-in-unreal-engine (명령줄·`Trace.File`은 그 페이지에 없고 소스로 확인).
- 소스(`Engine/Source/Runtime/Core/Private/ProfilingDebugging/TraceAuxiliary.cpp`): 채널 프리셋 `Default = cpu,gpu,frame,log,bookmark,screenshot,region`, `Memory = memtag,memalloc,callstack,module`, `Memory_Light = memtag,memalloc`. `BaseEngine.ini [Trace.ChannelPresets]`에 `Rendering`, `Loading`, `TaskGraph`, `ContextSwitching`. 네트워크는 `Net` 채널, 태스크는 `Task`, I/O는 `IoStore`.
- 명령줄: `-trace=<채널/프리셋>`, `-tracefile=`, `-tracehost=`, `-tracefiletimestamps`, `-tracefiletrunc`, `-traceautostart`. 콘솔: `Trace.File [경로] [채널]`, `Trace.Send <호스트> [채널]`, `Trace.Stop/Pause/Resume/Status/Enable/Disable/Bookmark/SnapshotFile/SnapshotSend`. `Trace.Start`는 소스 설명에 **deprecated("Trace.File 사용")**.
- 구성별: `UE_TRACE_ENABLED`는 `!UE_BUILD_SHIPPING && !IS_PROGRAM` + 데스크톱 플랫폼에서 1(`TraceLog/Public/Trace/Config.h`). 즉 **Shipping은 트레이스 불가**(`UE_TRACE_ENABLED_SHIPPING_EXPERIMENTAL` 별도 정의 시 예외, 이번엔 사용법 미확인). 패키지 측정은 Development/Test.
- Trace 서버: `UnrealTraceServer.exe`와 `UnrealInsights.exe`가 `Engine/Binaries/Win64`에 있다(파일 존재 확인). 공식 Unreal Insights 페이지는 Windows 기본 Store 위치를 `%LOCALAPPDATA%/UnrealEngine/Common/UnrealTrace/Store`로 안내한다(문서, 이번에 본문 직접 열람). 실제 설치 환경의 저장 위치·오버라이드는 미검증이다.
- 리슨 서버 측정: 호스트와 클라이언트가 각각 트레이스를 쓰므로 프로세스마다 파일명을 구분해야 한다. 같은 PC 다인은 CPU를 공유해 분리 장치와 결과가 다르다([성능 조사](../PERFORMANCE.md)).
- 확장 Insights: `GameplayInsights`(Rewind Debugger, [DEBUG_TEST](DEBUG_TEST.md)), `NetworkPredictionInsights`(EditorAndProgram, 기본 꺼짐), `MassInsights`(**Beta**), `RenderGraphInsights`, `NiagaraInsights`, `SlateInsights`.

### stat 명령, ProfileGPU, MemReport
- `stat unit`은 `UEngine::ToggleStatUnit`/`RenderStatUnit`로 구현된 엔진 통계(게임 스레드·렌더 스레드·RHI·GPU 시간)이며 `unitgraph`, `unitmax`, `unittime`, `fps`, `hitches`, `ai`, `levels`, `raw` 등과 함께 `UnrealEngine.cpp`의 `EngineStats` 목록에 등록됨(소스).
- 통계 그룹 선언 확인: Game, Scene Rendering, Net, Memory, Memory Platform, LLM Summary, LLM FULL, AI, Anim, Audio, Collision, Engine, Navigation, Physics, Particle Mem, Threading, Tickables, Quick. 각 `stat <그룹>` 명령 이름은 그룹 이름과 같다고 알려져 있으나 이번엔 명령 문자열을 직접 확인하지 않았다(미확인). `stat gpu`는 `HAS_GPU_STATS = (STATS || CSV_PROFILER_STATS) && !Shipping`(RHI) 조건이며 명령 등록은 소스에서 직접 찾지 못했다.
- `STATS` 매크로는 Shipping에서 `FORCE_USE_STATS`가 없으면 0이다(`Misc/Build.h`). `ProfileGPU`는 `WITH_PROFILEGPU = !(Shipping||Test) || (Test && ALLOW_PROFILEGPU_IN_TEST) || (Shipping && ALLOW_PROFILEGPU_IN_SHIPPING)` 일 때만(`Misc/Build.h`). 관련 CVar `r.ProfileGPU.Sort/Root/ThresholdPercent/TableFormatting/UnicodeOutput/ShowLeafEvents`.
- `MemReport`는 엔진 콘솔 명령(`UnrealEngine.cpp`). `HandleMemReportDeferredCommand`는 기본적으로 `FPaths::ProfilingDir()/MemReports/` 아래 `.memreport` 파일을 만들며, `FPaths::ProfilingDir()`는 프로젝트 `Saved/Profiling/`이다(`Core/Private/Misc/Paths.cpp`). 소스에서 `-LOG`(파일 대신 로그), `-CSV`, `TYPE=`, `NAME=`, `-FULL` 옵션을 확인했다. 명령 실행·실제 출력은 미검증이다.

### CSV Profiler
- 헤더 `Engine/Source/Runtime/Core/Public/ProfilingDebugging/CsvProfiler.h`: `CSV_DEFINE_CATEGORY`, `CSV_SCOPED_TIMING_STAT`, `CSV_CUSTOM_STAT`, `CSV_EVENT`. 콘솔 `CsvProfile`(시작/정지), 카테고리 켜기/끄기는 `[CsvProfiler] EnabledCategories/DisabledCategories`(소스, `CsvProfiler.cpp`).
- 활성: `CSV_PROFILER = WITH_ENGINE && (!Shipping || CSV_PROFILER_ENABLE_IN_SHIPPING)`(`CsvProfilerConfig.h`).
- 분석 도구: 엔진 설치본에 `Engine/Binaries/DotNET/CsvTools` 디렉터리가 있다(내용·사용법 미확인).
- 쓸모: 같은 시드·인원으로 전후 비교, 자체 통계(굴착 분할 수, 복제 대상 수) 기록. `AutomatedPerfTesting`(Gauntlet 기반, **Experimental**)은 이를 자동화하는 후보지만 도입 판단은 보류.

### Significance Manager
- 플러그인 `Engine/Plugins/Runtime/SignificanceManager`(Runtime, `EnabledByDefault=false`, `IsBetaVersion=false`). 헤더: `USignificanceManager::RegisterObject(Object, Tag, SignificanceFunction, PostSignificanceType, PostFunction)`, `UnregisterObject`, `Update(TArrayView<const FTransform> Viewpoints)`, `GetSignificance(Object)`. 사후 함수는 이전/새 중요도와 순위 변화를 받는다.
- 사용 형태: 태그별로 중요도 함수를 등록하고 매 프레임 뷰포인트(로컬 플레이어 카메라)로 `Update`를 호출한 뒤 중요도에 따라 Tick 간격·애니메이션 품질·이펙트를 조정한다.
- 리슨 서버 고려: 서버는 4인 위치를 뷰포인트로 줄 수 있고 클라이언트는 로컬 카메라만 쓴다. 서버 권한 판정(AI 감지, 굴착)에 중요도 기반 비활성화를 쓰면 규칙이 달라질 수 있어 **표현 전용(로컬)** 에 먼저 적용하는 편이 안전하다는 점은 설계 판단 사항이다.
- 대안: 거리 기반 `TickInterval`, LOD·Nanite 컬링([RENDERING_ART](RENDERING_ART.md)), Mass LOD(Experimental).

### Tick 최적화
- `Engine/Source/Runtime/Engine/Classes/GameFramework/Actor.h`: `SetActorTickEnabled(bool)`, `SetActorTickInterval(float)`, `GetActorTickInterval`, `SetTickGroup`. `EngineBaseTypes.h`의 `FTickFunction`: `bTickEvenWhenPaused`, `bStartWithTickEnabled`, `bAllowTickOnDedicatedServer`, `bAllowTickBatching`, `TickInterval`.
- Tick 그룹(`ETickingGroup`): `TG_PrePhysics`, `TG_StartPhysics`(Hidden), `TG_DuringPhysics`, `TG_EndPhysics`(Hidden), `TG_PostPhysics`, `TG_PostUpdateWork`, `TG_LastDemotable`(Hidden). 의존은 Tick prerequisite로 지정.
- **5.8 deprecated**: `FTickFunction::GetLastTickGameTime()` → `GetLastIntervalTickGameTime()`로 이름 변경("TickInterval이 있는 함수에만 올바른 시간 반환").
- 프로젝트: bCanEverTick true 15곳은 실제 필요 여부를 측정 후 점검. 컴포넌트가 이미 이벤트 기반이면 false로 둘 수 있다(채택 결정 아님).
- 네트워크: 서버 전용 컴포넌트는 `bAllowTickOnDedicatedServer`가 전용 서버용 설정이라 리슨 서버에는 효과가 없다(필드 이름 기준 추정, 미확인).

### 오브젝트 풀링
- 이번 소스 대조 범위에서 범용 Actor/UObject 풀은 확인하지 못했다. 용도별 제공 기능은 `UNiagaraComponentPool`(Niagara.uplugin의 `IsBetaVersion=false`, Experimental 키 없음), `IMassActorPoolableInterface`(MassGameplay **Experimental**), `FUserWidgetPool`(UMG), 복제 채널 풀 CVar `net.ActorChannelPool`이다. Beta/Experimental 표시 부재는 안정성 보증이 아니다. 프로젝트의 `DigetherFrontendWidgetReuseTest`는 `FUserWidgetPool`로 같은 위젯 인스턴스 재사용을 검사한다(소스, 이번에는 테스트 실행 안 함).
- 복제되는 풀링 액터는 `SetActorHiddenInGame`/충돌·Tick 비활성과 함께 휴면·재사용 시 상태 초기화가 필요하다. 굴착 지형은 ProceduralMesh를 쓰므로 풀 대상은 몬스터·픽업·이펙트 정도이며, 정산·소유 상태가 섞이는 액터는 신중해야 한다([DATA_SAVE.md](DATA_SAVE.md)).
- 판단 기준: 생성/파괴 빈도와 GC 비용을 Insights로 확인한 뒤에 도입.

### 비동기 로딩·패키지 크기
- API: `FStreamableManager::RequestAsyncLoad/RequestSyncLoad`(`Engine/Classes/Engine/StreamableManager.h`), `UAssetManager::LoadPrimaryAssets/LoadPrimaryAsset/GetPrimaryAssetIdList`, `LoadPackageAsync`(`UObjectGlobals.h`). 프로젝트의 정의 에셋은 `DigetherItem`·`DigetherTuningProfile` Primary Asset 규칙(`AlwaysCook`)으로 등록되어 있다.
- 설정: `BaseEngine.ini [/Script/Engine.StreamingSettings]`에 `s.AsyncLoadingThreadEnabled=True`, `s.EventDrivenLoaderEnabled=True`, `s.MinBulkDataSizeForAsyncLoading` 등. 추가 CVar `s.AsyncLoadingTimeLimit`, `s.UseBackgroundLevelStreaming`.
- 용량: Size Map·Reference Viewer(`AssetManagerEditor` 프로젝트 활성), `IncludeDebugFiles`·`bShareMaterialShaderCode`가 크기에 영향. 프로젝트 컬처 9개 스테이징·`InternationalizationPreset=All`은 ICU 데이터 크기에 영향(크기 영향 크기는 미측정).

### 패키징 (Project Launcher, BuildCookRun, Cook, IoStore/Zen, Pak 청크)
- UAT: `Engine/Build/BatchFiles/RunUAT.bat` → `BuildCookRun`(`Engine/Source/Programs/AutomationTool/Scripts/BuildCookRun.Automation.cs`). 확인한 인자: `-cook`, `-stage`, `-pak`, `-iostore`, `-zenstore`, `-compressed`, `-archive`, `-distribution`, `-prereqs`, `-nodebuginfo`, `-separatedebuginfo`, `-clientconfig`, `-skipcook`, `-skipstage`, `-skipbuild`, `-manifests`, `-createreleaseversion`, `-nocompileeditor`(`ProjectParams.cs`, 각 인자의 정확한 의미·조합 규칙은 `-help`로 확인 필요).
- 프로젝트 설정 클래스: `ProjectPackagingSettings`(`Engine/Source/Developer/DeveloperToolSettings/Classes/Settings/ProjectPackagingSettings.h`, UCLASS `config=Game, defaultconfig`). **클래스 이동**: 모듈이 `DeveloperToolSettings`로 바뀌었지만 `BaseEngine.ini`의 `ClassRedirects`가 옛 이름 `/Script/UnrealEd.ProjectPackagingSettings`를 새 클래스로 연결한다. 프로젝트 ini와 `BaseGame.ini`가 옛 섹션명을 쓰며 UAT도 같은 옛 경로를 읽는다(`DeploymentContext.cs`). 새 섹션명으로 옮기면 UAT 쪽 읽기와 어긋날 수 있으니 바꾸지 않는 편이 안전(영향 미확인).
- 속성(헤더 확인): `BuildConfiguration`(`PPBC_Debug/DebugGame/Development/Test/Shipping`), `FullRebuild`, `ForDistribution`, `IncludeDebugFiles`, `UsePakFile`, `bUseIoStore`, `bUseZenStore`("Use Zen Server as cooked output store"), `bMakeBinaryConfig`, `bGenerateChunks`, `bGenerateNoChunks`, `bChunkHardReferencesOnly`, `bCompressed`, `IncludeAppLocalPrerequisites`, `bShareMaterialShaderCode`, `bSharedMaterialNativeLibraries`, `IncludeCrashReporter`, `CulturesToStage`, `bCookAll`, `bCookMapsOnly`, `bSkipEditorContent`, `DirectoriesToAlwaysCook`. `GetUseZenStoreEffective() = bUseZenStore && bUseIoStore`.
- `BaseGame.ini` 기본값(소스): `UsePakFile=True`, `bUseIoStore=True`, `bUseZenStore=True`, `bCompressed=True`, `bGenerateChunks=False`, `IncludeCrashReporter=False`, `bShareMaterialShaderCode=True`, `BuildConfiguration=PPBC_Development`. 프로젝트가 덮어쓰지 않으면 이 값으로 패키징된다.
- Pak 청크: `bGenerateChunks`를 켜고 Asset Manager 규칙의 `ChunkId`로 할당(프로젝트 현재 `ChunkId=-1`). 헤더 주석은 스트리밍 설치 플랫폼용이라고 설명. Steam 배포에서 청크가 필요한지는 판단 대상(미확인).
- Project Launcher: 플러그인 `Engine/Plugins/Developer/ProjectLauncher`(Editor, **IsBetaVersion=true**, 기본 켜짐, "Configure custom project launch profiles"), 구형은 모듈 `LegacyProjectLauncher`. 에디터 메뉴 경로는 미확인.
- 문서: Packaging 페이지는 본문 추출 실패(미열람).
- 구성: 아래 "빌드 구성" 절 참조. 프로젝트 기록의 패키징 검증은 Win64 Development 한정, 깨끗한 다른 PC 설치·Shipping은 미검증.

### 빌드 구성 (Debug / DebugGame / Development / Test / Shipping)
- `UnrealTargetConfiguration`(`UnrealBuildTool/Configuration/UEBuildTarget.cs`): Debug, DebugGame("Development와 같되 게임 모듈만 최적화 끔"), Development, Test, Shipping.
- 구성별 차이(`Misc/Build.h`, `TargetRules.cs`, `EngineDefines.h`):
  - Shipping: 로깅·콘솔·checks 기본 꺼짐(`USE_LOGGING_IN_SHIPPING=0`, `ALLOW_CONSOLE_IN_SHIPPING=0`, `USE_CHECKS_IN_SHIPPING=0`), 트레이스·통계·Visual Logger·Draw Debug·Gameplay Debugger·Network Profiler 제외, 치트 매니저 비활성.
  - Test: Shipping에 가깝되 `ALLOW_CHEAT_CVARS_IN_TEST` 1, `ProfileGPU`는 `ALLOW_PROFILEGPU_IN_TEST`로 선택, `bBuildDeveloperTools` 기본 꺼짐(CQTest·FunctionalTesting 등 Developer 모듈 제외).
  - Development: 위 도구 대부분 사용 가능 → 성능 측정·다인 검증의 기본 구성.
- 주의: 프로젝트의 개발 전용 코드는 `#if !UE_BUILD_SHIPPING`(37곳)·`WITH_DEV_AUTOMATION_TESTS` 가드를 쓴다. Test 구성에서 켜지는 매크로가 다르므로 Test 빌드 시 가드를 재검토해야 한다(Test 빌드 미검증).

### Steam 배포 관련 엔진 측 설정
- 플러그인: `OnlineSubsystemSteam`(Runtime, 기본 꺼짐, PlatformAllowList 존재), `SteamShared`, `SteamSockets`(Runtime, 기본 꺼짐, Beta 아님). Steamworks SDK 버전 **1.64**(`Engine/Source/ThirdParty/Steamworks/Steamworks.build.cs`: `SteamVersionNumber = 1.64`, 경로 `Steamv164`). 엔진 업데이트마다 SDK 버전이 바뀔 수 있다.
- `steam_appid.txt`: `SteamShared`가 **비-Shipping 빌드에서만** `[OnlineSubsystemSteam] SteamDevAppId`를 읽어 `steam_appid.txt`를 쓴다(`SteamSharedModule.cpp`). Shipping은 쓰지 않으므로 Steam 클라이언트를 통한 실행과 실제 AppID가 필요하다.
- 재실행: `FOnlineSubsystemSteam`이 `[OnlineSubsystemSteam] bRelaunchInSteam`·`SteamDevAppId`가 없으면 경고를 남기고, `bRelaunchInSteam`이 참이면 `SteamAPI_RestartAppIfNecessary(AppId)`로 Steam을 통해 재실행한다(`OnlineSubsystemSteam.cpp`). **프로젝트 DefaultEngine.ini에는 `bRelaunchInSteam`이 없다**(`bEnabled`, `SteamDevAppId=480`, `bUseSteamNetworking`, `bAllowP2PPacketRelay`, `bVACEnabled=false`만) → 실행 시 경고 가능성(실제 로그는 미확인).
- 앱 ID 480은 Steam 공개 개발용이다(DefaultEngine.ini 주석과 PROTOTYPE_STEAM). 출시용 AppID·Depot·VAC·빌드 업로드(SteamPipe)는 엔진 외부이며 이 문서 범위 밖이다. SteamPipe 절차는 미확인.
- 네트워킹 계층(`NetDriverDefinitions`에 `SteamSocketsNetDriver`)은 [NETWORKING.md](NETWORKING.md), 세션·음성은 [ONLINE_VOICE.md](ONLINE_VOICE.md).

### Crash Reporter와 디버그 심볼
- `Engine/Binaries/Win64/CrashReportClient.exe`와 `CrashReportClientEditor.exe`가 설치본에 있다. 기본 설정 `BaseEngine.ini [CrashReportClient]`(`bIsAllowedToCloseWithoutSending=true`, `bIsAllowedToSendWithoutDetailedInfo=true`, `CrashConfigPurgeDays=2` 등). 패키지 포함 옵션 `IncludeCrashReporter`(기본 False). 업로드 서버(`DataRouterUrl` 등)는 프로젝트 소유 설정이며 이번엔 확인하지 않았다.
- 심볼: 패키징 설정 `IncludeDebugFiles`("Include Debug Files in Shipping Builds"), UAT `-separatedebuginfo`·`-nodebuginfo`. 빌드마다 PDB를 빌드 번호와 함께 보관하는 절차는 프로젝트 규칙이 없다(미결). 덤프 해석 절차는 미확인.
- 출시 후 대응 책임·배포 복구는 [릴리스 운영 템플릿](../../templates/RELEASE_OPERATIONS_TEMPLATE.md)에서 정한다.

### Derived Data Cache (공유 DDC, Zen)
- `BaseEngine.ini`: 기본 그래프 `Default=(ProjectPak, EnginePak, ZenLocal, Local, ZenShared, Shared, Cloud)`, 설치본 `Installed=…`, 명령줄 `-DDC=<그래프>` 또는 콘솔 `DDC.Graph`. 저장소 정의 `[StorageServers]`의 `Local`, `Shared`(`EnvHostOverride=UE-ZenSharedDataCacheHost`, `CommandLineHostOverride=ZenSharedDataCacheHost`), `Cloud`. 경로 오버라이드: 환경 변수 `UE-LocalDataCachePath`, 명령줄 `-SharedDataCachePath=\\서버\폴더`(설정 주석).
- Zen: `[Zen] AutoLaunch=true`, 데이터 경로 `%ENGINEVERSIONAGNOSTICINSTALLEDUSERDIR%Zen/Data`, 로컬 GC 14일(`--gc-cache-duration-seconds 1209600`). 실행 파일 `zenserver.exe`·`zen.exe`·`ZenDashboard.exe`가 설치본에 있음.
- 팀 공유 설정은 프로젝트 `DefaultEngine.ini`의 `[DerivedDataBackendGraph]` 대신 위 그래프·환경 변수를 쓰는 방식이 5.8 기본이다. 프로젝트 팀(기획·아트·개발 혼합)의 도입 여부와 위치는 결정 대상이며 현재 설정 없음.

### 셰이더 컴파일과 PSO 캐시
- 패키지 셰이더: `bShareMaterialShaderCode`(기본 True, 크기는 줄고 로딩 시간이 늘 수 있다는 헤더 설명), `bSharedMaterialNativeLibraries`(재시작 필요), `bDeterministicShaderCodeOrder`.
- PSO 프리캐싱: CVar `r.PSOPrecaching`(기본 1, `PipelineStateCache.cpp`), `r.PSOPrecache.Mode`(0 Full PSO, 1 Preload shaders), `r.PSOPrecache.ProxyCreationStrategy`(0 항상 생성, 1 컴파일 중이면 프록시 생성 지연(기본), 2 컴파일 중 대체 머티리얼 사용, ReadOnly), 검증 `r.PSOPrecache.Validation`, 진단 `r.PSOPrecache.DumpStats`. 플랫폼별 스레드 수 등은 `[/Script/Engine.PSOPrecacheSettingsManager]`(BaseEngine.ini에 `PCD3D_SM6`·`PCD3D_ES3_1` 항목만 있고 `SM5` 항목은 없음 → 프로젝트 기본 RHI DX11(SM5)에서의 동작은 미확인).
- **deprecated 표시**: `r.PSOPrecache.ProxyCreationWhenPSOReady`, `ProxyCreationDelayStrategy`의 변수명이 `GDeprecated_*`(`PSOPrecache.cpp`) → `ProxyCreationStrategy` 사용.
- 파이프라인 파일 캐시: `r.ShaderPipelineCache.Enabled`(`PipelineFileCache.cpp`), `r.ShaderPipelineCache.StartupMode`(`ShaderPipelineCache.cpp`: 0 일시정지 등 모드). 번들 PSO 수집·배포 절차는 이번에 확인하지 못했다.
- 에디터 개발 중 셰이더 대기는 DDC가 채워지기 전 반복되며, 공유 DDC가 없으면 팀원마다 재컴파일한다(DDC 절 참조).
- 끊김 방지 효과는 실측 필요. 카툰·로우폴리 머티리얼 수는 [RENDERING_ART.md](RENDERING_ART.md).

### Scalability와 Device Profile
- 스케일러빌리티: `Engine/Config/BaseScalability.ini` — `[ScalabilitySettings]`, `[ResolutionQuality]`, `[AntiAliasingQuality@0..3,Cine]`, `[ViewDistanceQuality@…]`, `[ShadowQuality@…]`, `[GlobalIlluminationQuality@…]`, `[ReflectionQuality@…]`, `[PostProcessQuality@…]`, `[TextureQuality@…]`, `[EffectsQuality@…]`, `[FoliageQuality@…]`, `[ShadingQuality@…]` 등. 그룹 CVar `sg.ResolutionQuality/ViewDistanceQuality/AntiAliasingQuality/ShadowQuality/GlobalIlluminationQuality/ReflectionQuality/…`(`Scalability.cpp`). 프로젝트 재정의는 `DefaultScalability.ini`(현재 없음).
- Device Profile: `BaseDeviceProfiles.ini`에 `[Windows DeviceProfile]`, `[WindowsEditor DeviceProfile]`. 선택 모듈 `WindowsDeviceProfileSelector`(`BaseWindowsEngine.ini`). 편집기는 모듈 `DeviceProfileEditor`. 프로젝트 재정의는 `DefaultDeviceProfiles.ini`(현재 없음).
- 쓸모: 최소 사양이 정해지면(미정) 옵션 메뉴·자동 감지(`GameUserSettings`) 설계의 기초. 프로젝트는 `GameUserSettingsClassName=/Script/DIGETHER.DigetherGameUserSettings`를 이미 지정했다(DefaultEngine.ini).

## 보류·비추천 후보와 이유
| 후보 | 이유 |
|---|---|
| ReplicationGraph | Beta, 기본 꺼짐. 4인 호스트형에서 필요성이 측정으로 입증된 뒤 검토 |
| MassGameplay 액터 풀 | Experimental 플러그인 |
| AutomatedPerfTesting | Experimental |
| `Trace.Start` 콘솔 명령 | 엔진이 "Trace.File 사용"으로 deprecated 표기 |
| `r.PSOPrecache.ProxyCreationWhenPSOReady` 계열 | 엔진 내부에서 `GDeprecated_*`로 취급 |
| 레거시 Network Profiler | Networking Insights로 대체, 뷰어 없음 |
| 범용 오브젝트 풀 직접 구현 | 측정 근거 없음. 대안 비교 후 사용자 선택([EXECUTION_POLICY](../../ai/EXECUTION_POLICY.md#대안-비교와-사용자-선택)) |
| Shipping 구성에서 Insights 사용 | `UE_TRACE_ENABLED=0` 기본 |

## 확인 한계
- 엔진 실행 없이 헤더·`.uplugin`·`.Build.cs`·ini·UBT/UAT 소스를 읽은 결과다. 명령의 출력 형식, 종료 코드, 실제 성능 이득은 확인하지 않았다.
- 문서 열람 성공: Unreal Insights 소개 1페이지. Packaging·Project Launcher·CSV Profiler·Significance Manager·PSO 문서는 열람하지 못했다(Packaging 페이지는 본문 추출 실패).
- 미확인: `stat <그룹>` 명령 문자열, `stat gpu` 등록 위치, LLM 활성 인자, `UE_TRACE_ENABLED_SHIPPING_EXPERIMENTAL` 사용법, Project Launcher 메뉴 경로, SteamPipe 절차, 번들 PSO 수집 절차, Crash Reporter 업로드 설정, DX11(SM5) 환경의 PSO 프리캐싱 효과, DDC 공유 구성 절차, CSVTools 사용법. MemReport 기본 출력 위치·옵션은 소스, Trace Store 기본 위치는 공식 문서로 확인했으며 실제 출력·저장 동작은 미검증이다.
- 프로젝트 기록의 패키징 결과는 5.8.2에서 수행됐다. 5.8.4(CL 58902052)로 패키징·플레이를 재실행한 기록은 이 조사에서 확인하지 못했다.
- 기존 문서와 다른 사실: [PROTOTYPE_STEAM](../../prototype/PROTOTYPE_STEAM.md)·[개발 도구 안내](../../tools/DEVELOPER_TOOLS.md)와의 충돌은 발견하지 못했다. 다만 프로젝트 ini에 `bRelaunchInSteam`이 없다는 점은 기존 문서에 언급되지 않았다.
