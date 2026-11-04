# 소리·오디오 기능 카탈로그

기준일 2026-11-04 · UE 5.8.4 (CL 58902052) · 상태: **조사 자료, 채택 결정 아님**. 색인은 [기능 카탈로그](README.md).

## 요약
- **MetaSounds는 `Metasound` 플러그인이 EnabledByDefault, Beta 표시 없음**이라 새 소리 자산의 기본 후보다. Sound Cue도 `deprecated` 표시 없이 엔진에 남아 있다(5.6 이후 일부 내부 멤버만 deprecated). 둘 중 선택은 자산 제작 방식 결정이다.
- **Sound Attenuation 안의 내장 Occlusion**(`bEnableOcclusion`, 라인 트레이스 + 저역필터 + 감쇠)과 **Reverb Send·Air Absorption·Listener Focus**가 "벽 너머 / 먼 소리" 공포 연출의 기본 도구다. 플러그인 없이 쓸 수 있다.
- **HRTF 공간화**는 엔진 기본(Panning) 외에 **Resonance Audio(Beta)**, **Microsoft Spatial Sound(Win64)** 플러그인이 있고, `Spatialization` 플러그인은 HRTF가 아니라 ITD 패너다. 프로젝트 설정의 `SpatializationPlugin`은 현재 비어 있다.
- **Audio Gameplay Volume은 Beta(플러그인·문서 모두)**이고 Audio Modulation은 EnabledByDefault가 꺼져 있어 `.uproject`에 명시해야 한다. 둘 다 현재 미사용.
- **소리 재생 호출은 자동 복제되지 않는다.** `PlaySoundAtLocation`·`SpawnSoundAtLocation`의 헤더가 복제를 처리하지 않는다고 명시한다. 컴포넌트 자체의 복제와 재생 상태 전파는 별개이며, 멀티플레이에서는 복제된 상태/이벤트를 각 클라이언트가 로컬로 재생하는 경로가 필요하다.
- AI 청각은 소리 재생과 별개다. 소음을 AI에 알리는 `UAISense_Hearing::ReportNoiseEvent`는 AIModule에 있다.

## DIGETHER에서 쓰이는 곳
- **오디오 자산·재생 코드**: 프로젝트 소스에서 사운드 재생·SoundClass·Submix 사용처를 찾지 못했다. 효과음·음악 에셋 내부와 실제 재생은 미검증이다.
- **옵션**: `UDigetherGameUserSettings`가 `FAudioDevice::SetTransientPrimaryVolume`으로 마스터 볼륨만 적용한다. 헤더 주석: 효과음·음악 SoundClass가 생기면 그 볼륨에 연결(지금은 저장만). 설정 패널에 사운드 탭 있음.
- **테스트**: 자동화 테스트 컴포넌트는 `AllowAudioPlayback(false)`로 오디오를 끄고 실행한다.
- **프로젝트 오디오 설정** (`DefaultEngine.ini` Windows 섹션): `AudioSampleRate=48000`, `AudioCallbackBufferFrameSize=1024`, `AudioNumBuffersToEnqueue=1`, `AudioNumSourceWorkers=4`, `AudioMaxChannels=0`, `SpatializationPlugin=`·`ReverbPlugin=`·`OcclusionPlugin=` 모두 비어 있음.
- **관련 기획**: S06 헤드랜턴·여진(DG-003/004) 연출, S25~S29 몬스터·근거리 음성(DG-016). 몬스터 청각은 [AI·내비게이션](AI_NAVIGATION.md), 음성 채팅 믹싱은 [온라인 음성](ONLINE_VOICE.md), 재생 위치 복제는 [네트워킹](NETWORKING.md)과 연결된다.
- 기존 [UNREAL_FEATURE_COVERAGE](../../design/UNREAL_FEATURE_COVERAGE.md) DG-016은 "Sound Attenuation만 설정해 음성 통신이 완성되지 않음"으로 정리했고 이 문서의 내용과 충돌하지 않는다.

## 기능 목록
이 문서의 `미사용`은 별도 근거가 없으면 `DIGETHER/Source`·`Config`의 사용처 미발견을 뜻하며, Blueprint·에셋 내부 사용과 런타임 모듈 로드는 미검증이다. `EnabledByDefault`와 의존 활성화는 실제 로드·재생 사용과 구분한다.

| 기능 | 모듈·플러그인 (타입, 안정성) | 하는 일 | DIGETHER 쓸모 | 프로젝트 상태 | 근거 |
|---|---|---|---|---|---|
| Sound Cue | Engine `USoundCue` + `USoundNode*` (Runtime, 표시 없음) | 노드 그래프로 랜덤·믹스·루프·거리 크로스페이드 | 단순 랜덤 효과음 | 미사용 | 소스 `Engine/Source/Runtime/Engine/Classes/Sound/SoundCue.h`, `SoundNode*.h` |
| MetaSounds | `Metasound` (Runtime+UncookedOnly, EnabledByDefault true, IsBetaVersion false) | 샘플 정확 DSP 그래프 사운드 소스. Builder API로 Blueprint에서 프로시저럴 제작 | 절차적 공포 앰비언스·반응형 소리 | 엔진 기본 활성(`.uproject` 명시 없음), 실제 로드·에셋 사용 미검증 | 소스 `Engine/Plugins/Runtime/Metasound/Metasound.uplugin`, `Source/MetasoundEngine/Public/MetasoundBuilderSubsystem.h` · 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/metasounds-in-unreal-engine |
| Sound Attenuation | Engine `USoundAttenuation` | 거리 감쇠, LPF, 공간화, 우선순위, 포커스, 리버브 센드, Occlusion | 모든 3D 소리의 기본 | 미사용 | 소스 `Classes/Sound/SoundAttenuation.h` · 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/sound-attenuation-in-unreal-engine |
| 내장 Occlusion | Sound Attenuation 안 | 청취자-음원 사이 트레이스 → LPF 주파수·볼륨 감쇠. `OcclusionTraceChannel`, `bUseComplexCollisionForOcclusion` | 벽 너머·붕괴 뒤 소리 먹먹함 | 미사용 | 소스 `SoundAttenuation.h` · 문서(attenuation 페이지) |
| 공간화(HRTF) | `ResonanceAudio` (Runtime, EnabledByDefault true, **IsBetaVersion true**, Win64) | 3D 오디오·룸 음향 시뮬(Google) | 헤드폰 입체감 후보 | 미사용(`SpatializationPlugin` 빈 값) | 소스 `Engine/Plugins/Runtime/ResonanceAudio/ResonanceAudio.uplugin` |
| Microsoft Spatial Sound | `MicrosoftSpatialSound` (Runtime, Win64, 안정성 표시 없음) | SASAPI 서비스 기반 공간화 | Windows 전용 HRTF/Atmos 후보 | 미사용 | 소스 `Engine/Plugins/Runtime/MicrosoftSpatialAudio/MicroSoftSpatialSound.uplugin` |
| Spatialization(ITD) | `Spatialization` (Runtime) | 기본 패닝 계열 ITD 패너. HRTF가 아님 | 단순 입체감 보강 | 미사용 | 소스 `Engine/Plugins/Runtime/Spatialization/Source/Spatialization/Public/ITDSpatializer.h` |
| Steam Audio(엔진 번들) | `SteamAudio` (**Deprecated**, Beta) | 엔진 번들판은 "삭제 예정, Valve 사이트 플러그인 사용" | 사용 금지 후보 | 미사용 | 소스 `Engine/Plugins/Runtime/Steam/SteamAudio/SteamAudio.uplugin` |
| Reverb / Audio Volume | Engine `AAudioVolume`, `UReverbEffect`, `FReverbSettings`, `FInteriorSettings` | 영역 리버브·앰비언트 존(외부 소리 차단/감쇠). `bEnabled`는 복제됨 | 광산/폐교/피라미드 공간 울림 | 미사용 | 소스 `Classes/Sound/AudioVolume.h`, `ReverbEffect.h` |
| Submix / Submix Effect | Engine `USoundSubmix`, `FSoundSubmixSendInfo`, `USoundEffectSubmixPreset` | 믹스 버스, 센드, 이펙트 체인 | 동굴 리버브·붕괴 시 전체 로우패스 | 미사용 | 소스 `Engine/Source/Runtime/Engine/Classes/Sound/SoundSubmix.h`, `SoundSubmixSend.h`, `SoundEffectSubmix.h` |
| Synthesis 서브믹스 이펙트 | `Synthesis` (Runtime, EnabledByDefault true, Beta 아님) | Flexiverb·Multiband Compressor·Delay·Filter·Convolution Reverb 등 프리셋(Reverb·EQ·Dynamics 프리셋은 엔진 쪽 선언일 수 있음, 위치 미구분) | 소리 압축기·잔향 | 엔진 기본 활성 | 소스 `Engine/Plugins/Runtime/Synthesis/Source/` (`USubmixEffect*Preset` 클래스 확인) |
| Sound Class / Sound Mix | Engine `USoundClass`, `USoundMix` | 분류별 볼륨·피치·리버브·음악·UI 속성, 믹스 일시 조정 | SFX/음악/음성 볼륨 슬라이더, 붕괴 시 믹스 덕킹 | 미사용(주석상 예정) | 소스 `Classes/Sound/SoundClass.h`, `SoundMix.h`, `Classes/Kismet/GameplayStatics.h` (`SetBaseSoundMix`, `PushSoundMixModifier`, `SetSoundMixClassOverride`) |
| Sound Concurrency | Engine `USoundConcurrency`, 해결 규칙 `EMaxConcurrentResolutionRule` | 동시 재생 수 제한 및 PreventNew/StopOldest/StopFarthest 등 규칙, RetriggerTime | 곡괭이 연타·파편 소리 폭주 제어 | 미사용 | 소스 `Classes/Sound/SoundConcurrency.h` (공식 문서 페이지는 목차만 반환) |
| Audio Modulation | `AudioModulation` (Runtime, **EnabledByDefault false**, Beta 아님) | Control Bus·Bus Mix·Patch·Generator로 볼륨·피치 등 파라미터 변조 | 공포 긴장도에 따른 전역 믹스, 덕킹 | 미사용(플러그인 비활성) | 소스 `Engine/Plugins/Runtime/AudioModulation/AudioModulation.uplugin` · 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/audio-modulation-overview-in-unreal-engine |
| Audio Gameplay Volume | `AudioGameplayVolume` (Runtime+Editor, **IsBetaVersion true**, EnabledByDefault 키 없음), `AudioGameplay` 활성 의존 | 게임플레이 볼륨으로 리버브·필터·서브믹스 센드/오버라이드·감쇠 컴포넌트 적용 | 영역별 음향 처리 | `.uproject` 명시 없음, 사용처 미발견·실제 로드 미검증 | 소스 `Engine/Plugins/AudioGameplayVolume/AudioGameplayVolume.uplugin`, `Source/AudioGameplayVolume/Public/{Reverb,Filter,SubmixSend,SubmixOverride,Attenuation}VolumeComponent.h`, `AudioGameplayVolumeSubsystem.h` · 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/audio-gameplay-volumes-in-unreal-engine (Beta 표기, 상세 없음) |
| Quartz | Engine AudioMixer 모듈 `UQuartzSubsystem`, `UQuartzClockHandle` | 오디오 클록 기준 정밀 박자·퀀타이즈 | 음악 박자 맞춤 연출이 있을 때만 | 미사용 | 소스 `Engine/Source/Runtime/AudioMixer/Public/Quartz/QuartzSubsystem.h`, `AudioMixerClockHandle.h` · 문서 페이지는 목차만 |
| Audio Insights | `AudioInsights`(EditorAndProgram+EditorNoCommandlet), `AudioInsightsRuntime`(Runtime), `AudioModulationInsights`(EditorAndProgram). 세 플러그인 EnabledByDefault true, Beta/Experimental false | 소리·서브믹스·미터·변조 프로파일링. 독립 실행·콘솔 빌드는 Unreal Insights 연결 | 디버그 | 기본 활성, 실제 로드·캡처 미검증 | 소스 `Engine/Plugins/{AudioInsights,AudioInsightsRuntime,AudioModulationInsights}/`의 동명 `.uplugin` · 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/audio-insights-in-unreal-engine |
| Audio Synesthesia | `AudioSynesthesia` (Runtime+Editor, **Beta**, 자체 EnabledByDefault false) | 오프라인 오디오 분석(라우드니스·FFT 등)을 Blueprint에 노출 | 소리 크기 기반 연출 후보 | 기본 활성 Metasound·Synthesis가 활성 의존, 실제 로드·분석 사용 미검증 | 소스 `Engine/Plugins/Runtime/AudioSynesthesia/AudioSynesthesia.uplugin`, `Engine/Plugins/Runtime/{Metasound,Synthesis}/`의 동명 `.uplugin` |
| WaveTable / Audio Widgets | `WaveTable` (Runtime+Editor, **Beta**, 자체 기본 꺼짐), `AudioWidgets` (RuntimeAndProgram+Runtime+Editor, Beta/Experimental false, 기본값 키 없음) | MetaSound·Modulation 부속 | MetaSound·Modulation의 의존 플러그인 | 기본 활성 Metasound가 둘 다 활성 의존, 실제 로드·게임 사용 미검증 | 소스 `Engine/Plugins/Runtime/{WaveTable,AudioWidgets,Metasound}/`의 동명 `.uplugin` |
| SoundFields | `SoundFields` (**Beta**, EnabledByDefault true) | Ambisonics 등 사운드필드 | 우선순위 낮음 | 엔진 기본 활성 | 소스 `Engine/Plugins/Runtime/SoundFields/SoundFields.uplugin` |
| AI Hearing | AIModule `UAISense_Hearing` | 소음 이벤트를 AI Perception에 전달 | 몬스터가 소리를 듣는 규칙 | 미사용(AIModule 의존만) | 소스 `Engine/Source/Runtime/AIModule/Classes/Perception/AISense_Hearing.h` (`ReportNoiseEvent(WorldContext, Location, Loudness, Instigator, MaxRange, Tag)`) |
| Subtitles and Closed Captions | `SubtitlesAndClosedCaptions` (**Beta**, EnabledByDefault false) | 자막·청각 접근성 | 접근성 후보 | 미사용 | 소스 `Engine/Plugins/SubtitlesAndClosedCaptions/SubtitlesAndClosedCaptions.uplugin` |

## 기능별 상세
### Sound Cue vs MetaSounds
- Sound Cue: `USoundCue`에 `UCLASS(... LazyOnDemand)`. 일부 멤버(`GetReferencedObjects`, `ParamsToSet`, `Transmitters`)가 5.6부터 `UE_DEPRECATED`이지만 클래스 자체는 deprecated가 아니다(소스). `GetReferencedObjects`의 대체 API가 `AddReferencedObjects`다.
- MetaSounds: `UMetaSoundSource`(`MetasoundSource.h`), Builder API(`MetasoundBuilderBase.h`, `MetasoundBuilderSubsystem.h`, `MetasoundDocumentBuilderRegistry.h`). 문서가 MetaSound Pages·Templates·Builder 항목을 안내한다. 두 시스템의 공식 비교·이전 권고는 문서에서 찾지 못했다(`미확인`).
- 에셋 제작 방식은 팀 결정 영역이며 이 문서는 선택을 정하지 않는다. `MetasoundExperimental`은 별도 Experimental 플러그인이라 기본 후보에서 제외한다.

### Sound Attenuation과 공포 연출(원거리·벽 너머)
- 소스에서 확인한 주요 속성: `bSpatialize`, `SpatializationAlgorithm`(Panning/Plugin-Spatialized), `bAttenuateWithLPF`, `bEnableListenerFocus`·`bEnableFocusInterpolation`, `bEnableOcclusion`·`bUseComplexCollisionForOcclusion`·`OcclusionTraceChannel`·`OcclusionLowPassFilterFrequency`·`OcclusionVolumeAttenuation`·`OcclusionInterpolationTime`, `bEnableReverbSend`·`EReverbSendMethod`(Linear/Custom Curve/Manual), `bEnablePriorityAttenuation`, `bEnableSubmixSends`, `EAirAbsorptionMethod`, `NonSpatializedRadiusStart/End`.
- 문서: Occlusion Trace Channel 기본은 Visibility. 충돌 응답을 Ignore로 두면 가림에서 제외한다. Complex Collision은 정확하지만 비싸다. 리버브는 월드 설정의 기본 리버브 또는 Audio Volume 안에서만 적용되고 Sound Class가 허용해야 한다. 바이노럴은 헤드폰을 전제하며 설정 위치는 Project Settings > Platforms > [플랫폼] > Audio > Spatialization Plugin이다.
- 굴착 지형 연동: Occlusion 트레이스는 충돌 채널을 쓰므로 ProceduralMesh 지형의 충돌이 최신이어야 의도한 먹먹함이 난다. 굴착으로 벽이 사라질 때의 반영 지연과 트레이스 비용은 측정하지 않았다(`미확인`). 관련 충돌 갱신은 [물리·충돌](PHYSICS_COLLISION.md), [지형 파괴](TERRAIN_DESTRUCTION.md).
- 먼 소리·간접 소리는 거리 감쇠 곡선 + LPF + Air Absorption + Reverb Send 조합으로 만들 수 있다. 구체 수치는 사운드 디자인 결정이다.

### 공간화 플러그인(HRTF)
- 확인한 후보: Resonance Audio(Beta, Win64), Microsoft Spatial Sound(Win64), 번들 Steam Audio(Deprecated, 사용하지 않는 것이 맞음). `Spatialization` 플러그인은 `ITDSpatializer`(`FSourceSpatializer`)로 이름과 달리 HRTF가 아니다.
- 프로젝트가 Steam 판매 방향이므로 Steam Audio를 쓰려면 엔진 번들이 아니라 Valve 배포판을 써야 한다는 것이 엔진 플러그인 설명의 내용이다. 제3자 플러그인은 이번에 확인하지 않았다.
- 활성화 위치: Project Settings > Platforms > Windows > Audio의 Spatialization Plugin(ini `SpatializationPlugin`). Occlusion/Reverb 플러그인 항목도 같은 섹션에 있다(비어 있음).

### Reverb·Audio Volume·Submix
- `AAudioVolume`: `Priority`, `bEnabled`(ReplicatedUsing), `Settings`(`FReverbSettings`), `AmbientZoneSettings`(`FInteriorSettings`), 서브믹스 설정. 겹침은 우선순위로 해결(소스 주석).
- `USoundSubmix`: `bAutoDisable`, 엔벨로프 팔로워, 앰비소닉스 설정 등. 센드 설정은 UObject 클래스가 아니라 `FSoundSubmixSendInfo`/`FSoundSubmixSendInfoBase` 구조체이며 `SendLevelControlMethod`(Manual 등)·`SendLevel`을 가진다. 엔진·Synthesis 소스 전체에서 `USubmixEffect*Preset` 클래스 11종(Reverb, Flexiverb, ConvolutionReverb, SubmixEQ, DynamicsProcessor, MultibandCompressor, Delay, StereoDelay, TapDelay, Filter, StereoToQuad)을 확인한 기존 조사 기록이 있다(이번에는 전체 11종을 재대조하지 않음).
- `UGameplayStatics::ActivateReverbEffect`·`SetBaseSoundMix`·`PushSoundMixModifier`·`SetSoundMixClassOverride`는 전역 연출 API다(소스 `GameplayStatics.h`).
- Audio Gameplay Volume 플러그인은 Reverb/Filter/Submix Send/Submix Override/Attenuation 볼륨 컴포넌트와 `UAudioGameplayVolumeSubsystem`을 제공한다(소스 `Engine/Plugins/AudioGameplayVolume/Source/AudioGameplayVolume/Public/`의 각 선언 헤더 직접 확인). Beta이므로 Audio Volume으로 충분한지 먼저 비교한다.

### Concurrency·Class·Mix·Modulation
- Concurrency 해결 규칙은 PreventNew, StopOldest, StopFarthestThenPreventNew, StopFarthestThenOldest, StopLowestPriority, StopQuietest, StopLowestPriorityThenPreventNew이고 `RetriggerTime`이 있다(소스 `SoundConcurrency.h`). 곡괭이·파편처럼 짧은 소리가 많은 장면에 쓰는 도구다.
- `SoundBase`에 `Priority`, `VirtualizationMode`(`EVirtualizationMode`)가 있어 보이스 한도 초과 시 가상화 동작을 고를 수 있다(소스).
- Modulation 플러그인 대상은 Sound Wave, MetaSound Source, Audio/Synth Component, Sound Class, Submix, 일부 Source Effect이다(문서). 켜려면 플러그인 활성화와 에디터 재시작이 필요하다(문서).

### 멀티플레이에서 소리 재생 위치
- 소스 근거: `UGameplayStatics::PlaySoundAtLocation`·`SpawnSoundAtLocation`은 헤더 주석에서 복제를 처리하지 않는다고 명시하며 구현은 로컬 오디오 장치를 사용한다(`Engine/Source/Runtime/Engine/{Classes/Kismet/GameplayStatics.h,Private/GameplayStatics.cpp}`). `UAudioComponent`는 `USceneComponent`를 상속하므로 컴포넌트 복제 자체의 지원 여부와 재생 호출 전파를 혼동하지 않는다. "다른 사람도 같은 소리를 듣게" 하려면 상태 복제(RepNotify)나 이벤트로 각 클라이언트가 재생하는 경로를 구성한다. 이벤트 신뢰성은 사건 성격과 네트워크 정책에 따라 별도 결정한다.
- 리슨 서버 호스트는 서버이자 플레이어라 서버 이벤트에서 로컬 재생을 겸한다. 중요한 게임 판정(소음으로 몬스터가 반응)은 소리 재생과 분리해 서버에서 `ReportNoiseEvent` 등으로 처리하고, 재생은 순수 연출로 둔다([UNREAL_FEATURE_COVERAGE](../../design/UNREAL_FEATURE_COVERAGE.md) 방침과 동일). 자세한 RPC 정책은 [네트워킹](NETWORKING.md).
- 로컬 플레이어 전용 소리(헤드랜턴 클릭, UI)는 `bIsUISound`/2D 재생(`PlaySound2D`)을 쓰고 복제하지 않는다(소스 시그니처 `bIsUISound`).

### AI Hearing과의 연결
- `UAISense_Hearing::ReportNoiseEvent`는 정적 함수이며 위치·`Loudness`·`Instigator`·`MaxRange`·`Tag`를 받는다(소스). 청각 반응 범위는 `HearingRange * Loudness` 제곱 비교 규칙이 헤더 주석에 있다. 소리 재생 이벤트와 별개 호출이므로 설계에서 "소리가 나는 것"과 "AI가 듣는 것"을 따로 정의한다. 호출 권한(서버만)과 지형·벽 차폐 반영은 [AI·내비게이션](AI_NAVIGATION.md)에서 다룬다.

### 음성 채팅과의 믹싱
- 프로젝트는 `EOSVoiceChat` 플러그인을 활성화했고 `bHasVoiceEnabled=false`(OnlineSubsystem 설정)이다. 번들 구현의 `FEOSVoiceChatUser::Set3DPosition` 본문은 `// Unimplemented`로 비어 있다(소스 `Engine/Plugins/Online/VoiceChat/EOSVoiceChat/Source/EOSVoiceChat/Private/EOSVoiceChatUser.cpp`). 플러그인 활성화나 위치 API 존재만으로 근거리 공간 음성이 완성되지 않는다. 제공자의 출력 경로와 Sound Class/Submix 볼륨 연동 가능 여부는 미확인이다. 자세한 내용은 [온라인 음성](ONLINE_VOICE.md).

### Quartz·Audio Insights
- Quartz는 `UQuartzSubsystem`(월드 서브시스템)과 `UQuartzClockHandle`(Blueprint 노출)로 구성되며 박자 동기 음악·연출에 쓴다. 공식 문서 페이지에서 세부를 얻지 못했다.
- Audio Insights: PIE와 독립 실행에서 동작하며 Windows 독립 빌드는 Unreal Insights에 연결한다(문서). 플러그인 타입은 `AudioInsights`가 EditorAndProgram, `AudioInsightsEditor`가 EditorNoCommandlet이다(소스).

## 보류·비추천 후보와 이유
- **Steam Audio(엔진 번들)**: 플러그인 설명이 Deprecated를 선언함. 사용하지 않는다.
- **Audio Gameplay Volume**: Beta. 일반 Audio Volume으로 부족한 이유가 생길 때까지 보류.
- **Resonance Audio**: Beta. HRTF가 필요한지는 사운드 디자인 목표가 정해진 뒤 비교. 번들 Steam Audio는 제외.
- **MetasoundExperimental, AudioMotorSim, MotoSynth, Harmonix**: Experimental 계열. 이 게임 요구와 무관하다고 판단해 상세 조사하지 않았다.
- **Quartz**: 박자 기반 연출 요구가 없어 우선순위 낮음.

## 확인 한계
- 에디터에서 재생·프로파일링하지 않았다. 모든 동작은 헤더·`.uplugin`과 공식 문서 서술에서 읽은 것이다.
- 기존 문서 조사에서 Sound Concurrency, Quartz, Audio Gameplay Volumes는 목차 수준으로만 반환되었다. 이번에는 Sound Attenuation 공식 페이지 본문을 직접 열었으며 Audio Gameplay Volume은 선언 헤더로 구성요소를 확인했다. 실제 볼륨 재생 동작은 미검증이다.
- 오디오 장치 부재(전용 서버) 시 동작, EOS 음성 재생 경로, 굴착 중 Occlusion 갱신 지연, Resonance Audio의 UE 5.8.4 실제 동작은 확인하지 못했다.
- `AudioMaxChannels=0`의 의미(엔진 기본 사용인지 제한 없음인지)는 확인하지 못했다.
