# 렌더링·아트 기능 카탈로그

기준일 2026-11-04 · UE 5.8.4 (CL 58902052) · 상태: **조사 자료, 채택 결정 아님**. 색인은 [기능 카탈로그](README.md).

## 요약
- **카툰 룩의 1순위 후보는 Post Process Material(외곽선·해칭) + Custom Depth/Stencil**이다. 엔진 제공 경로이며 이번에 확인한 선언에는 Beta/Experimental 표시가 없었다. 이 사실은 안정성 보증이 아니다. 5.8에는 `MSM_` 열거형에 Toon 셰이딩 모델이 없다. 대신 **Substrate Toon BSDF + Toon Profile(Experimental)**이 새로 있다. 둘은 보완 후보이며 어느 쪽도 채택이 아니다.
- **프로젝트 렌더 설정이 "동적 광원 전용 + GI 없음"으로 되어 있다**: `r.AllowStaticLighting=False`(베이크 불가), `r.DynamicGlobalIlluminationMethod=0`(None), `r.ReflectionMethod=2`(Screen Space), `r.Shadow.Virtual.Enable=0`(VSM 끔), 기본 RHI는 DX11. 이 조합에서는 Lumen·Nanite·VSM이 동작하지 않는다. 어두운 광산에서 간접광을 원하면 설정 변경이 선행되어야 한다.
- 동적 굴착 메시(ProceduralMeshComponent)의 Mesh Distance Field 생성은 기존 소스 조사에서 확인하지 못했다. Lumen Scene의 실제 표현과 Hardware Ray Tracing 경로는 작은 장면에서 검증해야 한다(아래 상세).
- 헤드랜턴은 `USpotLightComponent`가 기본 후보이고 프로토타입에 이미 있다. 플레이어 최대 4명의 국소 광원·그림자 비용이 핵심이며 MegaLights(Experimental)는 비교 후보다.
- 어두운 장면에서는 Auto Exposure가 밝기를 끌어올려 공포감을 깨뜨릴 수 있다. 노출 방식·범위를 Post Process Volume에서 명시적으로 정해야 한다.
- 1인칭 도구는 5.8의 `EFirstPersonPrimitiveType`과 카메라 `FirstPersonFieldOfView/Scale`로 FOV·크기를 분리하는 내장 경로가 있다.

## DIGETHER에서 쓰이는 곳
- **S06 헤드랜턴·설치형 랜턴**: `ADigetherPrototypeCharacter`가 `USpotLightComponent`(Intensity 12000, AttenuationRadius 1400, Inner/Outer 20/42, 따뜻한 색)를 가진다. `ADigetherPrototypeMineScene`은 `UPointLightComponent`(그림자 끔)를 쓴다. 상태: 프로토타입 사용.
- **DG-019 카툰·로우폴리**: 최종 룩은 미결. 프로젝트 소스에 Post Process Material·Custom Depth 사용은 없다. 상태: 미사용.
- **머티리얼**: `UMaterialInstanceDynamic`을 MineScene·Portal에서 사용(색조·표면). Material Parameter Collection은 미사용.
- **S06~S10 굴착·붕괴 연출(흙먼지)**: Niagara 미사용. 프로젝트 Build.cs·소스에 Niagara 참조 없음(grep).
- **1인칭 도구**: 프로토타입 도구 메시를 카메라에 붙여 소유자에게만 보이게 한다(소스 주석). 내장 First Person 렌더링 기능은 미사용.
- 관련: [게임플레이 프레임워크](GAMEPLAY_FRAMEWORK.md)(카메라·FOV 분리), [지형 파괴](TERRAIN_DESTRUCTION.md)(동적 메시), [성능·빌드](PERF_BUILD.md), [소리](AUDIO.md).

## 프로젝트 현재 렌더 설정 (`DIGETHER/Config/DefaultEngine.ini`, 읽기만 함)
| 키 | 값 | 의미 |
|---|---|---|
| `r.DynamicGlobalIlluminationMethod` | 0 | None. Lumen GI 꺼짐. 열거형은 None/Lumen/ScreenSpace(Deprecated)/Plugin(소스 `EngineTypes.h`) |
| `r.ReflectionMethod` | 2 | Screen Space. 0 None, 1 Lumen |
| `r.Shadow.Virtual.Enable` | 0 | VSM 끔 → 기존 Shadow Map |
| `r.GenerateMeshDistanceFields` | True | Static Mesh 빌드 때 Distance Field 생성(Lumen 소프트웨어 트레이싱 전제) |
| `r.Lumen.TraceMeshSDFs` | 0 | Lumen을 켜도 Mesh SDF 대신 Global SDF 사용 |
| `r.AllowStaticLighting` | False | 라이트맵 베이크 불가. 모든 빛이 동적 |
| `r.Substrate` / `r.Substrate.ProjectGBufferFormat` | True / 0 | Substrate 켜짐, 블렌더블 GBuffer(엔진 기본 1) |
| `r.RayTracing` | True (+ `RayTracingProxies.ProjectEnabled`) | 하드웨어 레이 트레이싱 지원 컴파일 |
| `r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange` | true(중복 2줄) | 노출 범위를 EV100 단위로 확장 |
| `r.DefaultFeature.LocalExposure.Highlight/ShadowContrastScale` | 0.8 / 0.8 | 로컬 노출 기본값 |
| `DefaultGraphicsRHI` | DX11 (`D3D12TargetedShaderFormats=SM6`, `D3D11=SM5` 둘 다 타깃) | 실행 시 RHI 기본이 DX11 |
| 오디오 플랫폼 | `SpatializationPlugin=`·`OcclusionPlugin=`·`ReverbPlugin=` 비어 있음 | [소리](AUDIO.md) 참고 |
- `r.AntiAliasingMethod`, `r.ForwardShading`, `sg.*` 재정의는 없다 → 엔진 기본(`r.ForwardShading` 기본 0 = Deferred, 소스 `RenderUtils.cpp`). 프로젝트에 `DefaultScalability.ini`도 없다(소스: `DIGETHER/Config` 목록).
- 위 값은 설정 파일 기준이며 실제 에디터·패키지에서 어떤 경로가 실행되는지는 검증하지 않았다.

## 기능 목록
이 문서의 `미사용`은 별도 근거가 없으면 `DIGETHER/Source`·`Config`의 사용처 미발견을 뜻하며, Blueprint·에셋 내부 사용과 런타임 모듈 로드는 미검증이다. `EnabledByDefault`와 의존 활성화는 실제 로드·게임 사용과 구분한다.

| 기능 | 모듈·플러그인 (타입, 안정성) | 하는 일 | DIGETHER 쓸모 | 프로젝트 상태 | 근거 |
|---|---|---|---|---|---|
| Post Process Material (Blendable) | Engine, 머티리얼 도메인 Post Process (Runtime, 표시 없음) | 화면 후처리 머티리얼로 외곽선·해칭·색 양자화. 위치: Before/After Tonemapping, Before Translucency, Replacing the Tonemapper | 카툰 외곽선·해칭의 주 후보 | 미사용 | 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/post-process-materials-in-unreal-engine |
| Custom Depth / Stencil | Engine `UPrimitiveComponent` | 컴포넌트별로 CustomDepth 패스에 그리고 0~255 스텐실 값을 기록. 후처리가 읽어 외곽선·강조 | 몬스터·상호작용 대상 외곽선, 팀원 표시 | 미사용 | 소스 `Engine/Source/Runtime/Engine/Classes/Components/PrimitiveComponent.h` (`bRenderCustomDepth`, `CustomDepthStencilValue`, `CustomDepthStencilWriteMask`) |
| Substrate 머티리얼 | RenderCore/Renderer (Beta 문서 표기) | 레이어형 머티리얼 프레임워크. 5.8에서도 문서 페이지에 "Beta, 출시 시 주의" 표기 | 프로젝트가 이미 `r.Substrate=True`. Toon BSDF의 전제 | 설정 켜짐, 머티리얼 사용 여부 미확인 | 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/substrate-materials-in-unreal-engine · 소스 `Engine/Source/Runtime/RenderCore/Private/RenderUtils.cpp` (`r.Substrate` 엔진 기본 0, ReadOnly) |
| Substrate Toon BSDF + Toon Profile | Engine (`DevelopmentStatus = "Experimental"`) | Diffuse/Specular Ramp 곡선, 그림자 해칭 패턴, GI 스케일 등으로 셀 셰이딩 | 셀 셰이딩 내장 경로 후보. 실험 단계라 채택 전 작은 장면 검증 필요 | 미사용 | 소스 `Engine/Source/Runtime/Engine/Public/Materials/MaterialExpressionSubstrate.h` (`UMaterialExpressionSubstrateToonBSDF`), `Engine/Source/Runtime/Engine/Classes/Engine/ToonProfile.h` (`UToonProfile`, `MAX_TOON_PROFILE_COUNT 256`) |
| 레거시 Toon 셰이딩 모델 | — | `EMaterialShadingModel`에는 Unlit·DefaultLit·Subsurface 등만 있고 Toon 없음 | 단독 "Toon 셰이딩 모델" 선택지는 없다 | — | 소스 `Engine/Source/Runtime/Engine/Classes/Engine/EngineTypes.h` (MSM_*, `MSM_Strata` Hidden) |
| 머티리얼 인스턴스 / MID | Engine | 부모 머티리얼 파라미터 변형. 런타임 동적 인스턴스 | 광석 피해·색조·단계 표시 | MID 사용 중 | 소스 `Engine/Source/Runtime/Engine/Public/Materials/MaterialInstanceDynamic.h` |
| Material Parameter Collection | Engine | 여러 머티리얼이 공유하는 전역 스칼라·벡터 값 | 전역 어둠·붕괴 경고·카툰 톤 같은 월드 단위 값 | 미사용 | 소스 `Engine/Source/Runtime/Engine/Public/Materials/MaterialParameterCollection.h` |
| Lumen GI/Reflections | Renderer (표시 없음) | 완전 동적 GI·반사. 소프트웨어(SDF)·하드웨어(RT) 트레이싱 | 동적 광원만 있는 어두운 광산의 간접광 후보 | 설정상 꺼짐(GI None, 반사 SS) | 소스 `RenderUtils.cpp` (`DoesPlatformSupportLumenGI`: SM6 필요, Forward 불가) · 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-global-illumination-and-reflections-in-unreal-engine |
| 정적·베이크 조명 | Engine | 라이트맵 | 동적 굴착 지형에는 부적합(메시가 바뀜). 프로젝트는 아예 비활성 | 비활성 | 소스 설정 `r.AllowStaticLighting=False` |
| Virtual Shadow Maps | Renderer | 고해상도 가상 그림자 맵. Nanite 전제 설계 | Nanite 없이는 비용 큼. 현재 꺼짐 | 꺼짐 | 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine |
| 국소 조명 Point/Spot/Rect | Engine `UPointLightComponent`·`USpotLightComponent`·`URectLightComponent` | 헤드랜턴·랜턴·작업등 | 핵심 | Spot/Point 사용 | 소스 `Engine/Source/Runtime/Engine/Classes/Components/` (`LocalLightComponent.h`, `SpotLightComponent.h`, `RectLightComponent.h`) |
| IES 프로파일 | Engine `ULightComponent::IESTexture` | 빛 분포 프로파일(`UTextureLightProfile`) | 헤드랜턴 빔 모양 | 미사용 | 소스 `LightComponent.h` |
| Light Function | Engine `LightFunctionMaterial` | 머티리얼로 빛에 패턴·흔들림 | 랜턴 깜빡임·먼지 광선 패턴 | 미사용 | 소스 `LightComponent.h` (`LightFunctionMaterial`, `LightFunctionScale`) |
| Lighting Channels | Engine | 광원이 영향 줄 대상 채널 분리 | 1인칭 도구만 비추기·플레이어 구분 | 미사용 | 소스 `LightComponent.h`, `PrimitiveComponent.h` (`SetLightingChannels`) |
| MegaLights | Renderer (CVar 설명 "Experimental") | 확률적 샘플링으로 그림자 광원 다수를 일정 비용으로 | 플레이어 4명+랜턴 다수일 때 비교 후보. HW RT 필요 | 미사용 | 소스 `Engine/Source/Runtime/Renderer/Private/MegaLights/MegaLights.cpp` (`r.MegaLights.EnableForProject` 기본 0) · `LightComponent.h` (`bAllowMegaLights`) · 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/megalights-in-unreal-engine |
| Auto Exposure / Local Exposure | Engine PostProcessSettings | 눈 적응. 방식 Histogram/Basic/Manual, EV100 범위, 속도 | 어두운 장면 밝기 고정 여부 결정 | 범위 확장·로컬 노출 기본값 설정 | 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/auto-exposure-in-unreal-engine · 소스 `Engine/Source/Runtime/Engine/Classes/Engine/Scene.h` |
| Exponential Height Fog / Volumetric Fog | Engine `UExponentialHeightFogComponent` | 안개, 볼류메트릭 안개에서 Point/Spot이 빛줄기 기여 | 광산 먼지·헤드랜턴 빛줄기 | 미사용 | 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-fog-in-unreal-engine |
| Local Fog Volume | Engine `ULocalFogVolumeComponent` | 국소 영역 안개 | 갱도 일부 먼지·붕괴 직후 연기 | 미사용 | 소스 `Engine/Source/Runtime/Engine/Classes/Components/LocalFogVolumeComponent.h` |
| Decal | Engine `UDecalComponent` | 표면에 투영 재질. Fade 설정 | 핏자국·균열·긁힘 | 미사용 | 소스 `DecalComponent.h` · 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/decals-in-unreal-engine (개요만, 세부 미확인) |
| Niagara | `Niagara` 플러그인 (Runtime, EnabledByDefault, Beta 아님) | CPU/GPU 파티클 | 흙먼지·붕괴·불꽃 | 미사용(모듈 참조 없음) | 소스 `Engine/Plugins/FX/Niagara/Niagara.uplugin` · 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/niagara-visual-effects-system-in-unreal-engine (목차만) |
| Chaos Niagara | `ChaosNiagara` (Experimental 폴더, **IsBetaVersion true**, EnabledByDefault true) | 파괴 데이터 → Niagara 이벤트 | 미래 파괴 연출 후보 | 미사용 | 소스 `Engine/Plugins/Experimental/ChaosNiagara/ChaosNiagara.uplugin` |
| Nanite | Engine/Renderer | 가상화 지오메트리. 정적·스켈레탈·랜드스케이프 지원, 반투명 불가, SM6 필요 | 정적 소품 후보. 동적 굴착 메시에는 해당 없음 | 설정 미지정 | 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-virtualized-geometry-in-unreal-engine |
| 안티에일리어싱 TSR/TAA/FXAA/MSAA | Renderer | TSR·FXAA는 Deferred/Forward 모두, MSAA는 Forward 전용 | 외곽선 후처리와의 지터·번짐 평가 대상 | 엔진 기본 | 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/anti-aliasing-and-upscaling-in-unreal-engine |
| Forward Shading | Renderer (`r.ForwardShading`, 기본 0) | MSAA·가벼운 베이스패스. Lumen·VSM·MegaLights 불가 | 외곽선 안정성과 맞바꿀지 비교 대상 | 꺼짐 | 소스 `RenderUtils.cpp`(Lumen !Forward), `RendererSettings.h`(VSM 툴팁) · 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/forward-shading-renderer-in-unreal-engine |
| Scalability (`sg.*`) | Engine `Config/BaseScalability.ini` | 품질 그룹 단계 | 옵션 메뉴 연동 | 재정의 없음 | 소스 `Engine/Config/BaseScalability.ini` |
| Post Process Volume | Engine `APostProcessVolume`·`UPostProcessComponent`·`FPostProcessSettings` | 노출·블룸·Lumen 품질·MegaLights·블렌더블 | 구역별 룩(광산/폐교/피라미드) 전환 | 미사용 | 소스 `Engine/Classes/Components/PostProcessComponent.h`, `Engine/Classes/Engine/Scene.h` |
| 1인칭 렌더링 분리 | Engine `EFirstPersonPrimitiveType`, `UCameraComponent` | 도구 전용 FOV·스케일, 월드 대표 메시 그림자 | 1인칭 도구 벽 관통 방지 | 미사용 | 소스 `PrimitiveComponent.h`, `Engine/Classes/Camera/CameraComponent.h` |

## 기능별 상세
### 카툰·툰 셰이딩 구현 수단
- **Post Process Material**: 도메인 Post Process, `Emissive Color`만 출력(문서). Blendable Location 중 Before Tonemapping은 HDR 입력이라 TAA 지터 문제를 줄이고, After Tonemapping이 기본·성능 선호 위치다. `PostProcessInput0`를 SceneColor 대신 쓰라고 문서가 안내한다. Separate Translucency는 Scene Texture 노드에서 동작하지 않는 알려진 이슈가 있다(문서). 사용 비용·프레임은 검증하지 않았다.
- **Custom Depth/Stencil 외곽선**: `bRenderCustomDepth=true`, 스텐실 값을 쓰려면 프로젝트 설정 또는 `r.CustomDepth==3`이 필요하다(소스 주석). 읽는 후처리가 없으면 효과가 안 보인다(문서). 외곽선이 TAA에서 떨리면 패스를 앞으로 옮기라는 안내가 있다(문서).
- **Substrate Toon**: `UMaterialExpressionSubstrateToonBSDF`는 BaseColor·Metallic·Specular·Roughness·Normal·Emissive·PatternUVs·Anisotropy·Tangent 입력과 `UToonProfile`을 받는다. `UToonProfile`은 Diffuse/Specular Ramp, 램프 오프셋 텍스처, 그림자 Extinction과 해칭 패턴, Diffuse/Specular Indirect Scale 같은 항목을 가진다. 실험용 CVar `r.Substrate.Experimental.ToonUnifiedDiffuse`·`ToonReflectionQuantizationEnabled`는 ReadOnly 기본 0이다(소스). **프로젝트가 쓰는 `ProjectGBufferFormat=0`(블렌더블 GBuffer, "시각 효과와 충실도가 많이 손실", ClosuresPerPixel=1 강제)에서 Toon BSDF가 어디까지 동작하는지는 확인하지 못했다.**
- 대안·병행: 머티리얼 안에서 라이팅 비의존 Unlit/Emissive 방식 카툰(커스텀 노드), 메시 뒤집기 외곽선(Inverted Hull)은 Epic 문서가 아니라 일반 기법이라 이번에 `미확인`이다.
- 아트 확정은 [ART_PRODUCTION](../ART_PRODUCTION.md)의 같은 노출·광원 비교 검수와 [UNREAL_FEATURE_COVERAGE](../../design/UNREAL_FEATURE_COVERAGE.md)의 DG-019 "작은 승인 장면에서 비교"를 따른다.

### Lumen vs 정적/베이크 조명 (어두운 광산·동적 지형)
- 프로젝트는 정적 조명이 꺼져 있어(`r.AllowStaticLighting=False`) 라이트맵 선택지가 없다. 동적 굴착으로 메시가 바뀌므로 베이크는 어차피 맞지 않는다.
- Lumen 지원 조건(소스 `DoesPlatformSupportLumenGI`): 프로젝트 지원 + 플랫폼 지원 + **SM6**(Vulkan/Metal SM5 예외) + **Forward Shading 아님**. 프로젝트 기본 RHI는 DX11(SM5)이므로 현재 설정에서 Lumen은 실행되지 않는 것으로 해석된다(코드 조건 해석이며 실행 검증 안 함). DX12를 기본으로 바꾸는 일은 [성능·빌드](PERF_BUILD.md)와 Steam 사양 문서에서 따로 판단한다.
- Software Ray Tracing은 Mesh Distance Field가 필요하다(문서: "Generate Mesh Distance Fields" 필수). ProceduralMeshComponent 플러그인 소스에는 Distance Field·Lumen 카드 관련 코드가 없고 레이트레이싱 지오메트리 등록(`r.RayTracing.Geometry.ProceduralMeshes` 기본 1)만 있다(소스 `Engine/Plugins/Runtime/ProceduralMeshComponent/Source/ProceduralMeshComponent/Private/ProceduralMeshComponent.cpp`). 따라서 **동적 굴착 메시가 소프트웨어 Lumen 씬에 어떻게 표현되는지는 이 소스만으로는 알 수 없고, Hardware RT 경로가 더 가능성 있는 후보라는 추정**이다. 확인하려면 작은 굴착 장면에서 Lumen Scene 시각화로 비교해야 한다.
- 하드웨어 RT는 인스턴스 10만 이상에서 장면 갱신 비용이 커질 수 있다(문서). 굴착 메시 갱신 때마다 RT 지오메트리를 다시 만드는 비용은 `미확인`.
- 대안: GI 없이 직접광만 + 스카이라이트/앰비언트 최소값(공포 어둠에 어울릴 수 있음). 선택은 [대안 비교와 사용자 선택](../../ai/EXECUTION_POLICY.md#대안-비교와-사용자-선택) 절차를 따른다.

### 그림자: Virtual Shadow Maps vs Shadow Map
- 문서: VSM은 Nanite와 함께 쓰도록 설계되었고 비Nanite 지오메트리는 "훨씬 비싸다". 스켈레탈 애니메이션·WPO는 캐시 페이지를 매 프레임 무효화한다. 절차적 메시는 문서가 다루지 않는다. 다수 국소 광원 성능은 "작업 중"이라고 표기한다.
- 프로젝트는 VSM을 끈 상태. Forward Shading을 켜면 자동으로 Shadow Maps를 쓴다(소스 툴팁).
- 1인칭: `EFirstPersonPrimitiveType::WorldSpaceRepresentation`은 다른 플레이어가 보는 대표 메시로 VSM 그림자 처리를 위해 `bOwnerNoSee`를 내부적으로 켠다(소스 주석).

### 국소 조명과 헤드랜턴
- 핵심 API: `ULocalLightComponent`(Intensity Units), `USpotLightComponent`(`InnerConeAngle`/`OuterConeAngle`), `ULightComponent`(`IESTexture`, `bUseIESBrightness`, `LightFunctionMaterial`, `LightingChannels`, `bUseTemperature`, `CastTranslucentShadows`, `bAllowMegaLights`, `SetVolumetricScatteringIntensity`), `ULightComponentBase`(`bAffectTranslucentLighting`, `bCastVolumetricShadow`).
- 헤드랜턴 구현 고려: 프로토타입 Spot Light는 캡슐에 붙고 Tick에서 `GetBaseAimRotation()`으로 방향을 맞춘다(`DigetherPrototypeCharacter.cpp`). 카메라 부착은 별도 후보이며 도구 메시 그림자·1인칭 도구 라이팅 채널을 별도 판단해야 한다. 그림자 허용 여부와 반경이 비용을 좌우한다(프로토타입은 헤드랜턴 그림자 기본 허용, 광산 Point는 `SetCastShadows(false)`). 효과의 정량 비용은 측정하지 않았다.
- 네트워크: 광원 켜짐 상태·밝기 등급만 복제하고 `ULightComponent` 자체는 각 클라이언트가 로컬 컴포넌트로 만든다. 정확한 복제 구조는 [네트워킹](NETWORKING.md)에서 정한다(프로토타입은 `SetVisibility`로 켜짐 토글).
- MegaLights: 방향성 광원은 기본 비활성, Forward 렌더러와 비호환, 물·구름·Heterogeneous Volumes·Local Volumetrics 미지원(문서). 유효 조건이 HW RT라 DX12 전환이 선행된다.

### Exposure·Fog
- 어두운 장면: 히스토그램 자동 노출은 어두운 장면을 중간 회색으로 끌어올린다(문서). 공포 연출은 `AutoExposureMethod`를 Manual로 하거나 Min/Max를 좁혀 헤드랜턴 켬/끔에서 노출이 튀는 정도를 제어하는 방식을 비교할 수 있다. `FPostProcessSettings`에 방식·Min/Max 밝기·Bias·LocalExposure 오버라이드가 있다(소스 `Scene.h`). 문서는 Lumen GI 사용 시 Local Exposure를 항상 설정하라고 한다.
- Volumetric Fog: Point/Spot 광원이 빛줄기에 기여하고 그림자 광원은 비그림자 광원보다 약 3배 비용이다(문서). 해상도는 Shadow 스케일러빌리티가 정한다(문서).

### Decal·Niagara
- Decal: `UDecalComponent`의 SortOrder·FadeScreenSize·FadeStartDelay·FadeDuration(소스). 메시 디칼은 반투명이라 Nanite에서 불가(문서). 동적 지형 위 디칼 동작은 `미확인`.
- Niagara: 서버 전용 인스턴스에서는 시각 효과가 필요 없으므로 게임 규칙은 효과 유무에 의존하면 안 된다([UNREAL_FEATURE_COVERAGE](../../design/UNREAL_FEATURE_COVERAGE.md) 방침). 효과는 복제된 상태 변화에 따라 클라이언트가 재생한다. GPU/CPU 선택 기준과 붕괴 연출 구성은 문서 세부를 읽지 못해 `미확인`. C++에서 쓰려면 Build.cs에 Niagara 모듈 의존이 필요하다(일반 규칙, 이번 프로젝트 파일에는 없음).

### Nanite와 Low Poly·동적 메시
- 문서: 지원은 정적·스켈레탈·랜드스케이프 등, **반투명 불가**, SM6 필요. 삼각형이 작고 많은 메시에 유리하며 큰 삼각형이면서 가림 효과가 없는 한 번만 쓰이는 메시는 이득이 작다. 로우폴리 자체가 금지는 아니나 이득이 작을 수 있다는 것은 문서의 일반 서술에서 추정한 것이다.
- ProceduralMeshComponent는 문서 지원 목록에 없으므로 굴착 지형에는 쓸 수 없다고 보는 것이 안전하다(문서 부재로 판단, 소스 확인 안 함).

### 안티에일리어싱·Forward vs Deferred
- 문서: TSR·TAAU·FXAA는 Deferred와 Forward 모두, MSAA는 Forward에서만. Forward는 SSR·SSAO·Contact Shadows 등 화면공간 기능을 못 쓰고 동적 그림자 반투명 제한이 있으며 MSAA는 GPU 비용 약 25% 증가 사례가 있다(Epic 측정, 본 프로젝트와 무관). 외곽선 후처리를 쓰면 TAA/TSR 지터와 상호작용하므로 후보 조합을 작은 장면에서 비교한다.
- `FPostProcessSettings`와 `URendererSettings::DefaultFeatureAntiAliasing`(`r.AntiAliasingMethod`)이 설정 위치다(소스 `RendererSettings.h`). 열거값 목록은 이번에 확인하지 못했다.

### Scalability (`sg.*`)
- `BaseScalability.ini`의 그룹: AntiAliasing, ViewDistance, Shadow, GlobalIllumination, Reflection, PostProcess, Texture, Effects, Foliage, Shading. 단계 `@0`~`@3`과 `@Cine`가 있다(소스). `sg.GlobalIlluminationQuality`·`sg.PostProcessQuality`가 `Scalability.cpp`에 정의되어 있다. 프로젝트 재정의는 `DefaultScalability.ini`를 만들어 둔다(문서). `UDigetherGameUserSettings`가 UGameUserSettings 파생이므로 옵션 UI 연결은 [UI](UI.md)에서 다룬다.

## 보류·비추천 후보와 이유
- **정적/베이크 조명(Lightmass 계열)**: 동적 굴착 메시와 맞지 않고 프로젝트에서 이미 비활성. 비추천이 아니라 현재 구조상 선택지가 아님.
- **Virtual Shadow Maps**: Nanite 전제이고 굴착 메시는 Nanite 불가라 이득이 불확실. 현재 꺼짐.
- **MegaLights**: Experimental, HW RT 필수. 사양 기준이 정해지기 전에는 보류.
- **Substrate Toon BSDF**: Experimental. 채택 전에는 블렌더블 GBuffer 설정과의 호환 검증 필요.
- **Chaos Niagara**: Beta. 파괴 설계가 정해진 뒤 검토.
- **MSAA/Forward 전환**: 카툰 외곽선 안정성에 이득일 수 있으나 Lumen·VSM·MegaLights와 상호 배타라 렌더 기반 전체를 바꾸는 결정이다.

## 확인 한계
- 이번 재대조는 로컬 선언·플러그인·프로젝트 소스·설정과 Post Process Materials 공식 페이지 본문으로 수행했다. 그 밖의 `문서` 표시는 기존 조사 근거이며 이번에 해당 페이지들을 다시 열지는 않았다.
- 에디터 실행·셰이더 컴파일·프레임 측정은 하지 않았다. 모든 "동작한다/안 한다"는 소스 조건과 문서 서술에서 읽은 것이다.
- 문서 페이지 일부는 목차만 반환되어(Niagara 개요, Decals 개요, Quartz, Sound Concurrency) 세부는 `미확인`이다. `r.AntiAliasingMethod` 값 목록, Light Function과 Lumen의 상호작용, Volumetric Fog와 Lumen, Substrate와 Forward는 확인하지 못했다.
- Substrate Toon BSDF의 실제 외관과 요구 CVar 조합, ProceduralMesh와 Lumen Scene의 실제 표현은 에디터에서 확인해야 한다.
- UE 5.8.4 Launcher 설치본의 `Renderer/Private` 소스를 읽었으므로 CVar 기본값은 이 설치본 기준이다.
