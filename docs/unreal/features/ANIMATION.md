# 애니메이션 기능 카탈로그

기준일 2026-11-04 · UE 5.8.4 (CL 58902052) · 상태: **조사 자료, 채택 결정 아님**. 색인은 [기능 카탈로그](README.md).

근거 표기: `소스` = 로컬 5.8.4 `Engine/...` 헤더·`.uplugin`·`.Build.cs`를 이번에 직접 확인. `문서` = Epic 공식 문서 페이지를 이번에 열어 확인. `미확인` = 이번에 확인하지 못함.

## 요약
- **프로토타입 캐릭터 C++은 스켈레탈 메시를 숨기고 정적 표현을 쓴다.** 템플릿에는 몽타주·노티파이 사용 코드가 있고, `DIGETHERCharacter.cpp`는 메시·AnimBP 참조를 파생 Blueprint에서 설정한다고 명시한다. BP·에셋 내부는 이번에 검사하지 않아 프로젝트 전체의 애니메이션 부재를 단정하지 않는다. 아래는 정식 캐릭터·몬스터를 만들 때 고를 후보 목록이다.
- **Animation Blueprint + 스레드 안전 업데이트**가 기본 골격. 값 수집은 `NativeThreadSafeUpdateAnimation`/Property Access를 쓰고, 워커 스레드에서 Controller·Actor를 만지지 않는다(프로젝트 규칙과 일치).
- **`ACharacter::PlayAnimMontage` 호출 자체는 일반 몽타주를 전파하지 않는다.** Character의 루트모션 복제 경로와 GAS의 몽타주 복제·예측 경로는 별개다. 일반 동작의 동료 화면 재생은 프로젝트가 복제 상태·OnRep 또는 일회성 연출 전송 등으로 설계해야 한다.
- **1인칭 팔·도구**: 엔진이 `EFirstPersonPrimitiveType`(FirstPerson / WorldSpaceRepresentation)과 카메라 `FirstPersonFieldOfView`·`FirstPersonScale`을 제공한다(5.8 소스). 팔 메시(1인칭)와 몸 메시(동료에게 보이는 월드 표현)를 분리하는 구성에 직접 쓰인다.
- **Linked Anim Layers**: 도구별 포즈 레이어를 런타임에 교체하기 좋다(프로젝트 설계도 조건부 후보로 언급).
- **성능**: 몬스터 수가 늘면 URO(`bEnableUpdateRateOptimizations`), 노티파이 LOD 필터, Animation Budget Allocator를 비교한다.
- **Notify 전용 서버 기본값**: 5.8.4 소스 생성자는 `bTriggerOnDedicatedServer(true)`다. 현재 공식 Notify 페이지는 활성화 시 전용 서버에서 실행된다고 설명하며 기본값을 명시하지 않는다(아래 상세).

## DIGETHER에서 쓰이는 곳
| 관련 | 기능 ID | 현재 프로젝트 상태 |
|---|---|---|
| 1인칭 도구 휘두르기·굴착 | S07~09 | 프로토타입 `ServerDig` → 서버 판정 → `ClientDigConfirmed`로 조작자에게만 연출. C++ 도구 메시는 소유자 전용 정적 메시이며 스켈레탈 메시는 숨긴다. 애니메이션 에셋 내부·할당은 미검증 |
| 동료 캐릭터 표현 | S02, S10 | 후속 구현 대상. [개발 시스템 설계](../../design/DEVELOPMENT_SYSTEM_DESIGN.md)의 "장비 상태와 행동 표현"·"캐릭터와 애니메이션" 규칙이 있다 |
| 조준(AimOffset)·제자리 회전·Linked Layer | S03 | 설계 규칙만 있고 미구현: 조준은 `GetBaseAimRotation()` + `NormalizeAxis`, Linked Layer는 정식 스켈레톤 준비 후 조건부 |
| 구조·사망 | S10 | 상태는 `DigetherLifeStateComponent`가 소유. 연출용 몽타주·래그돌은 후속 |
| 몬스터 | S25 | 체력은 `DigetherMonsterHealthComponent`가 소유("파괴·드롭·래그돌·AI 정지는 통지 수신자 책임"). 종별 행동·BT 미결, 몬스터 메시 없음 |
| 발소리·타격음 | S28 | 미구현. 노티파이/Anim 기반 후보는 아래, 오디오는 [AUDIO.md](AUDIO.md) |
| 템플릿 잔재 | — | `Variant_Combat`·`Variant_Platforming`에 `AnimNotify` 서브클래스와 `Montage_Play` 사용 예가 있다(소스, 프로젝트). DIGETHER 기능이 아니다 |
| 애니메이션 디버그 | — | `GameplayInsights`(Animation Insights)는 `.uproject`에 활성 명시. `ControlRig`·`IKRig`는 descriptor 기본 활성이고, `AllToolsets`→`AnimationAssistantToolset`→`ControlRig` 의존 경로도 있다. 이번 검증에서 실제 모듈 로드는 확인하지 않음 |

## 기능 목록
아래 프로젝트 상태의 `미사용`은 확인한 `DIGETHER/Source`·`Config`에서 정식 게임용 연결을 찾지 못했다는 뜻이며, 템플릿 예와 구분한다. Blueprint·에셋 내부 사용 여부와 실행·cook은 미검증이다. 플러그인 기본 활성과 게임 기능 채택도 구분한다.

| 기능 | 모듈·플러그인 (타입, 안정성) | 하는 일 | DIGETHER 쓸모 | 프로젝트 상태 | 근거 |
|---|---|---|---|---|---|
| Animation Blueprint | `Engine`, `AnimGraphRuntime` 모듈(엔진 기본) | AnimGraph/상태머신/블렌드 스페이스, `UAnimInstance` | 캐릭터·몬스터 기본 | 미사용 | 소스 `Engine/Source/Runtime/Engine/Classes/Animation/AnimInstance.h` |
| 스레드 안전 업데이트 | `Engine`: `NativeThreadSafeUpdateAnimation`, `BlueprintThreadSafeUpdateAnimation`, 프로젝트 설정 | 그래프 업데이트를 워커 스레드에서 실행 | 다수 캐릭터 성능. 프로젝트 규칙과 일치 | 미사용 | 소스 `AnimInstance.h`(`bUseMultiThreadedAnimationUpdate`), `Engine/Engine.h`(`bAllowMultiThreadedAnimationUpdate`) |
| Property Access | 런타임 `Engine` 모듈의 `FPropertyAccessLibrary`·`FAnimSubsystem_PropertyAccess` + `PropertyAccessEditor` 플러그인(UncookedOnly, 기본 활성, 비Beta) | 속성 경로 복사와 게임/워커 스레드별 복사 배치 | 값 전달 | 미사용 | 소스 `Engine/Source/Runtime/Engine/Public/PropertyAccess.h`, `Engine/Source/Runtime/Engine/Public/Animation/AnimSubsystem_PropertyAccess.h`, `Engine/Plugins/Runtime/PropertyAccess/PropertyAccessEditor.uplugin` |
| Anim Montage | `Engine`: `UAnimMontage`, `UAnimInstance::Montage_*`, `AnimNode_Slot` | 슬롯·섹션·노티파이가 있는 일회성 재생 | 도구 휘두르기·피격·구조 | 미사용(템플릿 예만) | 소스 `AnimInstance.h`, 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-montage-in-unreal-engine |
| 몽타주 복제 | `ACharacter::RepRootMotion`(루트모션 경로), GAS `UAbilitySystemComponent::PlayMontage`/`FGameplayAbilityRepAnimMontage` | Character는 루트모션 몽타주 상태를 복제하며 GAS는 별도의 몽타주 복제·예측 제공 | 동료 화면 재생은 프로젝트 설계 필요 | 설계 규칙만, GAS 채택 별도 | 소스 `Engine/Source/Runtime/Engine/Classes/GameFramework/Character.h`, `Engine/Source/Runtime/Engine/Private/Character.cpp`, `Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/AbilitySystemComponent.h`; 문서(montage 페이지 Multiplayer 절) |
| Linked Anim Layers / Linked Anim Graph | `Engine`: `LinkAnimClassLayers`, `AnimNode_LinkedAnimLayer`, `AnimNode_LinkedAnimGraph` | 레이어 인터페이스로 그래프 일부를 런타임 교체 | 도구별 포즈 | 미사용(조건부 후보) | 소스 `AnimInstance.h`, `AnimNode_LinkedAnimLayer.h`; 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/using-animation-blueprint-linking-in-unreal-engine |
| AimOffset / 회전 오프셋 | `AnimGraphRuntime`: `AnimNode_AimOffsetLookAt`, `AnimNode_RotationOffsetBlendSpace` | 조준 방향에 따른 가산 포즈 | 1인칭/동료 조준 | 미사용 | 소스 `Engine/Source/Runtime/AnimGraphRuntime/Public/AnimNodes/` |
| 간단 IK 노드 | `AnimGraphRuntime`: TwoBoneIK, FABRIK, CCDIK, LookAt, ModifyBone, SpringBone 등 | 손/발 IK, 시선 | 도구 잡기, 지형 발 맞춤 | 미사용 | 소스 `.../Public/BoneControllers/AnimNode_*.h` |
| IK Rig | `IKRig` (Runtime/UncookedOnly/Editor, `IsBetaVersion:false`, `IsExperimentalVersion:false`, EnabledByDefault) | 골격 솔버 + `AnimNode_IKRig`, `UIKRigComponent` | 손/발 IK, 리타게팅 기반 | 기본 활성, 실제 로드·게임 사용 미검증 | 소스 `Engine/Plugins/Animation/IKRig/IKRig.uplugin`; 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-in-unreal-engine |
| IK Retargeter | `IKRig` 플러그인(`Retargeter/IKRetargeter.h`, `AnimNode_RetargetPoseFromMesh`) | 서로 다른 스켈레톤 간 포즈/애니 변환 | 외부 애니메이션 에셋 재사용 | 미사용 | 소스 `.../IKRig/Source/IKRig/Public/Retargeter/`, `AnimNodes/AnimNode_RetargetPoseFromMesh.h`. 런타임 동작은 문서 미확인(페이지가 간략) |
| Full Body IK | `FullBodyIK` (Runtime, `IsBetaVersion:false`, `IsExperimentalVersion:false`, `Plugins/Experimental` 폴더) | PBIK 전신 IK | 필요 시 | 미사용 | 소스 `Engine/Plugins/Experimental/FullBodyIK/FullBodyIK.uplugin` |
| Control Rig(런타임) | `ControlRig` (Runtime/UncookedOnly/Editor, 표시 키 없음, EnabledByDefault) | `AnimNode_ControlRig`, `UControlRigComponent`, 프로시저럴 포즈 | 절차적 보정(도구 손 위치 등) | 기본 활성·Editor 도구 의존 경로 있음, 실제 로드·게임 사용 미검증 | 소스 `Engine/Plugins/Animation/ControlRig/ControlRig.uplugin`, `.../Public/AnimNode_ControlRig.h`, `ControlRigComponent.h`; 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/control-rig-in-unreal-engine (런타임 상세는 링크 페이지에 있고 이번엔 미확인) |
| Control Rig Modules | `ControlRigModules` (**Beta**) | 모듈형 리깅 | 제작용 | — | 소스 `.../ControlRigModules/ControlRigModules.uplugin` |
| Motion Matching / Pose Search | `PoseSearch` (Runtime/UncookedOnly, 표시 키 없음, EnabledByDefault:false, 의존 AnimationWarping·BlendStack·Chooser·MotionWarping 등) | 포즈 DB 검색으로 이동 애니 선택 | 3인칭 이동 품질. 1인 중심 게임엔 과함 | 미사용 | 소스 `Engine/Plugins/Animation/PoseSearch/PoseSearch.uplugin`; 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/motion-matching-in-unreal-engine |
| Chooser | `Chooser`(+`ProxyTable`) (Runtime, 표시 키 없음; 의존 BlendStack·GameplayInsights·GameplayTagsEditor) | 조건표 기반 에셋 선택(몽타주/애니/오브젝트), GameplayTag 열 지원 | 장비별 몽타주 선택, 몬스터 애니 선택 | 미사용 | 소스 `Engine/Plugins/Chooser/Chooser.uplugin`, `.../Public/GameplayTagColumn.h` |
| Animation Notify / Notify State | `Engine`: `UAnimNotify`, `UAnimNotifyState` | 타임라인 이벤트(발소리·타격 창) | 발소리·도구 타격 타이밍 | 템플릿 예만 | 소스 `Classes/Animation/AnimNotifies/AnimNotify.h`, `AnimNotifyState.h`; 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-notifies-in-unreal-engine |
| Evaluation Notifies | `EvaluationNotifies` (**Experimental**) | 평가 시점 코드를 가진 노티파이 | 보류 | — | 소스 `Engine/Plugins/Experimental/EvaluationNotifies/EvaluationNotifies.uplugin` |
| Root Motion | `Engine`: `ERootMotionMode`, `ACharacter::RepRootMotion`, `UCharacterMovementComponent` 루트모션 소스 | 애니 이동을 캐릭터 이동으로 | 근접 공격 전진. 네트워크 비용 큼 | 미사용 | 소스 `Character.h`, `CharacterMovementComponent.h`; 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/root-motion-in-unreal-engine |
| Motion Warping / Animation Warping | `MotionWarping`(**Beta**), `AnimationWarping`(안정 표시) | 루트모션 목표 맞춤 / 스트라이드·방향·경사 워핑 | 선택 | 미사용 | 소스 각 `.uplugin` |
| 1인칭 렌더링 구성 | `Engine`: `EFirstPersonPrimitiveType`, `UCameraComponent::FirstPersonFieldOfView/FirstPersonScale`, `bOwnerNoSee`/`bOnlyOwnerSee`, `SetLeaderPoseComponent` | 1인칭 팔 별도 렌더링, 월드 표현 분리, 포즈 공유 | 1인칭 팔·도구 | 미사용 | 소스 `Classes/Components/PrimitiveComponent.h`, `Camera/CameraComponent.h`, `Components/SkinnedMeshComponent.h` |
| URO | `Engine`: `bEnableUpdateRateOptimizations`, CVar `a.URO.*` | 화면 크기 등으로 애니 갱신 빈도 감소 | 다수 몬스터 | 미사용 | 소스 `SkinnedMeshComponent.h`, `Private/*` |
| Animation Budget Allocator | `AnimationBudgetAllocator` (Runtime, `IsBetaVersion:false`, `IsExperimentalVersion:false`) | 프레임당 애니 시간 예산 안에서 스켈레탈 메시 틱 조절 | 다수 몬스터 | 미사용 | 소스 `Engine/Plugins/Runtime/AnimationBudgetAllocator/*.uplugin`; 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-budget-allocator-in-unreal-engine |
| Animation Sharing | `AnimationSharing` (Runtime+Editor, `IsBetaVersion:false`, `IsExperimentalVersion:false`, 기본 활성, 의존 SignificanceManager) | 리더-팔로워 공유 애니 | 대량 군중용. 몬스터 수에 따라 | 게임 사용 참조 찾지 못함. 기본 활성·실제 로드 미확인 | 소스 `Engine/Plugins/Developer/AnimationSharing/AnimationSharing.uplugin` |
| AnimToTexture | `AnimToTexture` (**Experimental**) | 애니를 텍스처로 베이크(VAT) | 대량 객체 후보. Experimental | 미사용 | 소스 `Engine/Plugins/Experimental/AnimToTexture/AnimToTexture.uplugin` |
| Sequencer / Level Sequence | `MovieScene`·`LevelSequence` 엔진 모듈, 에디터 플러그인(`LevelSequenceEditor` 등) | 컷신·연출, 시퀀스 재생 | 도입·엔딩 연출 | 미사용 | 소스 `Engine/Source/Runtime/LevelSequence/Public/LevelSequenceActor.h` |
| Template/Actor Sequence | `TemplateSequence`(**Beta**), `ActorSequence`(**Beta**, EnabledByDefault) | 액터 단위 재사용 시퀀스 | 보류 | — | 소스 각 `.uplugin` |
| Mover | `Mover` (Runtime, **Experimental**, 의존 NetworkPrediction(Beta)·MotionWarping·PoseSearch 등) | CharacterMovement 대체 이동 프레임워크(롤백 네트워킹) | 보류. 이동 방식은 [GAMEPLAY_FRAMEWORK.md](GAMEPLAY_FRAMEWORK.md) | 미사용 | 소스 `Engine/Plugins/Experimental/Mover/Mover.uplugin`, `.../Runtime/NetworkPrediction/NetworkPrediction.uplugin` |
| UAF(Unreal Animation Framework) | `UAF` 외 다수 (**Experimental**) | 차세대 애니 그래프 | 보류 | — | 소스 `Engine/Plugins/Experimental/UAF/UAF/UAF.uplugin` |
| Contextual Animation | `ContextualAnimation` (**Experimental**) | 두 액터 상호작용 애니 | 구조 등 2인 연출 후보. Experimental | 미사용 | 소스 `.../ContextualAnimation/ContextualAnimation.uplugin` |

## 기능별 상세

### Animation Blueprint, 스레드 안전 업데이트, Property Access
- 핵심(소스 `AnimInstance.h`): `NativeInitializeAnimation`, `NativeUpdateAnimation`(게임 스레드), `NativeThreadSafeUpdateAnimation`(스레드 안전 갱신 자리), `BlueprintThreadSafeUpdateAnimation`(`meta=BlueprintThreadSafe`), `bUseMultiThreadedAnimationUpdate`(컴파일러가 AnimBP에서 설정).
- 설정: Project Settings > Engine > Anim Blueprints > `bAllowMultiThreadedAnimationUpdate`("전체 AnimBP 재컴파일 강제", 소스 `Engine.h`). `Engine/Source/Runtime/Engine/Private/GameEngine.cpp`의 `UEngine` 생성자 기본값은 true이며 프로젝트 `DIGETHER/Config/*.ini` 재정의는 찾지 못했다. 기본 병렬 업데이트 경로는 이 값과 인스턴스 `bUseMultiThreadedAnimationUpdate`가 모두 켜져야 한다. `AnimInstance.cpp::NeedsImmediateUpdate`에는 병렬 강제 CVar와 루트모션·디버그·병렬 작업 가능 여부 등 추가 조건이 있어 플래그만으로 워커 실행을 보장하지 않는다.
- 루트모션의 게임 스레드 즉시 갱신 조건은 `bNeedsValidRootMotion && RootMotionMode == RootMotionFromEverything`이다(소스 `Engine/Source/Runtime/Engine/Private/Animation/AnimInstance.cpp::NeedsImmediateUpdate`). 루트모션 사용 전체를 같은 비용 경로로 일반화하지 않는다.
- 프로젝트 규칙: 워커 스레드의 애니 업데이트에서 Controller·Actor·Component를 임의로 접근하지 않고, 값 수집은 게임 스레드 또는 Property Access/thread-safe 경로로 한다([개발 시스템 설계](../../design/DEVELOPMENT_SYSTEM_DESIGN.md) "캐릭터와 애니메이션"). 상태머신 전이는 State Alias·Conduit로 정리한다는 규칙도 있다.
- 호스트 화면의 원격 캐릭터 회전 표시: 액터 회전 vs 보간된 메시 회전 선택은 같은 설계 문서의 규칙을 따른다. 조준 각도를 별도 RPC로 매 프레임 보내지 않는다.
- 문서: [Animation Blueprints 개요](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-blueprints-in-unreal-engine)에서 Skeletal Mesh의 Animation Mode·Anim Class 할당 절차를 확인했다. 스레드 실행 조건은 위 소스를 근거로 한다.

### Montage와 복제
- 소스 `Engine/Source/Runtime/Engine/Private/Character.cpp`: `ACharacter::PlayAnimMontage`는 AnimInstance의 `Montage_Play`와 필요 시 `Montage_JumpToSection`을 호출하며 자체 네트워크 전파 코드는 없다. Character의 루트모션 동기화에는 `FRepRootMotionMontage RepRootMotion`(`ReplicatedUsing=OnRep_RootMotion`)이 쓰인다. 일반 몽타주 재생 전체를 자동 복제하는 API라는 뜻은 아니다. [Montage 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-montage-in-unreal-engine)도 일반 재생의 네트워크 전달과 루트모션 복제·보정을 구분한다. GAS의 별도 경로는 아래 3번과 같다.
- 후보 설계(조사 정리, 결정 아님)
  1. **복제 상태로 재생**: 서버가 "현재 장착/동작 상태 + 시작 서버 시각/시퀀스 번호"를 복제 프로퍼티로 두고 클라이언트가 `OnRep`에서 몽타주를 재생. 재접속·관련성 복귀 때 현재 상태에서 복원할 수 있어 프로젝트 지침("과거 RPC를 재생하지 않는다")과 맞는다.
  2. **Multicast 연출**: 일회성 효과(피격 반응 등)에만. 프로젝트 지침상 PlayerController의 Multicast 대신 관련 클라이언트에 복제되는 Character/Actor를 경유하고 Unreliable을 기본으로 한다.
  3. **GAS 몽타주 복제**: GAS를 쓰면 `UAbilitySystemComponent::PlayMontage`와 `GameplayAbilityRepAnimMontage`가 제공된다. GAS 도입은 별도 결정([GAMEPLAY_SYSTEMS.md](GAMEPLAY_SYSTEMS.md)).
- 몽타주 이벤트: `OnMontageEnded`/`OnMontageBlendingOut` 델리게이트, `Montage_SetEndDelegate`(소스). 오래된 완료 콜백이 새 몽타주를 덮지 않는지는 프로젝트가 확인해야 한다("타이머/콜백 무효화" 지침).
- 권위: 판정(피해·굴착 결과)은 서버가 확정하고 몽타주는 표현이다. 연출 누락이 판정을 취소하거나 연출 수신이 피해를 재적용하면 안 된다(설계 문서).
- 슬롯/섹션: 같은 Slot·Slot Group의 몽타주는 서로 덮어쓴다(문서). 상·하체 분리는 Slot Group + `LayeredBoneBlend`(소스 `AnimNode_LayeredBoneBlend.h`).

### Linked Anim Layers
- 소스: `UAnimInstance::LinkAnimClassLayers`/`UnlinkAnimClassLayers`, `GetLinkedAnimLayerInstanceByGroup`. 5.x에서 `SetLayerOverlay`/`ClearLayerOverlay`/`GetLayerSubInstanceByGroup`는 **4.24부터 deprecated**.
- 문서: Animation Layer Interface로 레이어 이름·그룹·입력을 정의하고, 메인 AnimBP의 Linked Anim Layer 노드에서 호출, 장비별 AnimBP를 `Link Anim Class Layers`로 연결/`Unlink`로 복원. 사용하지 않는 에셋은 로드되지 않아 메모리에 유리.
- 옵션(소스): `bUseMainInstanceMontageEvaluationData`(메인에서 재생한 몽타주를 링크된 레이어에도 적용), `bReceiveNotifiesFromLinkedInstances`/`bPropagateNotifiesToLinkedInstances`.
- 용도: 곡괭이/랜턴/들고 있는 회수품마다 포즈 레이어를 교체. 장비 변경 시 링크 교체는 서버 확정 장착 상태에서 파생해야 한다(복제 상태 기준).

### IK Rig, IK Retargeter
- 문서(IK Rig): 골격에 IK Goal과 Solver를 두고 AnimBP의 IK Rig 노드로 실행, Goal은 Blueprint 로직으로 구동 가능. IK Retargeter는 해당 페이지에 간략하여 런타임 설명은 확인하지 못했다.
- 소스: `UIKRigComponent`, `AnimNode_IKRig`, `AnimNode_RetargetPoseFromMesh`(런타임 리타게팅 노드 존재), `Retargeter/IKRetargeter.h`·`IKRetargetOps.h`.
- 플러그인 상태(소스): IK Rig는 `IsBetaVersion:false`/`IsExperimentalVersion:false`, `EnabledByDefault:true`. `IKRigUAF` 런타임 모듈은 `LoadingPhase:None`이고 그 `.Build.cs`에 UAF·UAFAnimGraph 의존이 있지만, `IKRig.uplugin`의 두 플러그인 참조는 `Enabled:false`, `Optional:true`다. 기본 IKRig 사용이 UAF 활성화를 요구한다고 해석하지 않는다.
- 용도: 1인칭 손 위치 맞추기, 동료 캐릭터 발 맞춤, 외부 애니메이션 스켈레톤 변환(제작 파이프라인은 [ART_PRODUCTION](../ART_PRODUCTION.md)).

### Control Rig(런타임)
- 소스: `AnimNode_ControlRig`(AnimBP에서 실행), `UControlRigComponent`, `UControlRig`. 플러그인 표시 키 없음, `EnabledByDefault:true`. 에디터 모듈은 `UncookedOnly`/`Editor`.
- 용도: 절차적 보정, 스퀴즈/흔들림 같은 도구 손맛. 비용은 RigVM 실행.
- 문서 한계: Control Rig 문서 페이지는 색인이라 런타임 사용법은 확인하지 못했다.

### Motion Matching · Pose Search · Chooser
- 문서(Motion Matching): Pose Search 플러그인을 켜고, 스키마(Pose Search Schema)·데이터베이스(Pose Search Database)·AnimBP의 Motion Matching 노드로 구성. 루트모션이 켜진 이동 애니가 필요. 일부 하위 기능(Crashing Legs 채널, Permutation Time 채널, VPTree 검색, 캐릭터 블루프린트 통한 몽타주 사용)은 문서가 Experimental로 표시.
- 소스: `PoseSearch.uplugin`에는 Beta/Experimental 키가 없다. **표시가 없다고 안정판이라고 보지 않는다**(위 문서의 하위 기능 표시 참고).
- 용도: 3인칭 이동 품질을 올릴 때. 1인칭 중심이면 이득이 작고 데이터 제작 비용이 크다.
- Chooser: 조건표로 몽타주/애니/클래스를 고르고 `GameplayTag`·`GameplayTagQuery` 열을 지원(소스). 장비 태그 → 몽타주 선택 같은 데이터 주도 선택에 후보.

### Animation Notify / Notify State
- 소스: `UAnimNotify::Notify(USkeletalMeshComponent*, UAnimSequenceBase*, const FAnimNotifyEventReference&)`, `UAnimNotifyState::NotifyBegin/Tick/End`. 문서(notify): 노티파이=한 시점, State=구간(시작·갱신·종료). 몽타주 전용 Montage Notify/Window도 있다.
- **서버 동작**: `FAnimNotifyEvent::bTriggerOnDedicatedServer` — 소스 주석 "비활성이면 전용 서버에서 건너뜀", **생성자 기본값 true**(`Engine/Source/Runtime/Engine/Public/Animation/AnimTypes.h`). [공식 Notify 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-notifies-in-unreal-engine)는 활성화 시 전용 서버에서 실행된다고 설명하며 기본값은 명시하지 않는다. 문서와 소스의 기본값 충돌로 볼 근거는 없다. 에셋별 설정은 에디터에서 별도 확인한다. 이 프로젝트는 리슨 서버라 호스트가 클라이언트 겸임이다.
- `bTriggerOnFollower`(싱크 그룹 팔로워는 기본 미발동), `MontageTickType`(`Queued`가 기본, `BranchingPoint`는 정밀도 대신 비용, 문서가 게임플레이 영향 결정에 권장), `NotifyTriggerChance`, `NotifyFilterType`/`NotifyFilterLOD`(LOD 기준 필터링) 등 트리거 옵션(소스).
- 발소리: 노티파이로 지면 판정·소리 재생 요청 → 오디오는 [AUDIO.md](AUDIO.md). 권위 판정과 분리(소리는 로컬 연출). 다른 방법으로 이동 속도·주기로 직접 계산도 가능.
- 도구 타격 타이밍: 노티파이는 타이밍 소스로 쓸 수 있으나 굴착·피해 판정은 서버가 확정하는 프로젝트 규칙을 지킨다. 서버 메시가 렌더링되지 않을 때 포즈 평가가 줄어들 수 있으므로(아래 `VisibilityBasedAnimTickOption`) 서버 측 타이밍이 노티파이에 의존한다면 확인이 필요하다.
- 템플릿 잔재 예시: `Variant_Combat/Animation/AnimNotify_DoAttackTrace`는 노티파이에서 소유 액터의 인터페이스를 호출한다(소스, 프로젝트). DIGETHER 기능이 아니다.

### Root Motion과 네트워크 주의
- 소스: `ERootMotionMode`, `ACharacter::IsPlayingNetworkedRootMotionMontage`(RootMotionFromMontagesOnly일 때 네트워크 경로), `UCharacterMovementComponent::ClientAdjustRootMotionPosition/SourcePosition`(보정 RPC). `ServerMoveDualHybridRootMotion` 등 구 RPC는 `DEPRECATED_CHARACTER_MOVEMENT_RPC`로 표시되고 `ServerMovePacked_ClientSend`로 대체됐다(소스).
- 문서: 루트모션 모드 4가지(No/Ignore/Everything/Montages Only)를 설명하며 추출 시 게임 스레드 비용을 경고한다. 실제 5.8.4 즉시 갱신 조건은 위 `NeedsImmediateUpdate` 소스처럼 RootMotionFromEverything와 유효 루트모션 필요 여부를 함께 검사한다. montage 페이지의 자동 루트모션 복제·보정 설명은 Character 경로로 읽고 GAS의 별도 몽타주 복제와 구분한다.
- 주의: 루트모션 몽타주는 클라이언트 예측·서버 보정 경로를 탄다. 굴착 중 이동·구조 중 위치 이동 같은 규칙과 부딪치면 CharacterMovement 보정으로 이어질 수 있다(이동 시스템은 [GAMEPLAY_FRAMEWORK.md](GAMEPLAY_FRAMEWORK.md)). 전진 공격 같은 기능에 한해 후보.

### 1인칭 팔·도구 애니메이션 구성 후보
소스로 확인한 엔진 기능만 근거이고 아래 조합 자체는 후보다.
- **A. 두 메시 분리(팔 전용 + 몸 전용)**: 1인칭 팔 `USkeletalMeshComponent`에 `FirstPersonPrimitiveType = FirstPerson`, 월드 표현 몸 메시에 `WorldSpaceRepresentation`(자동으로 `bOwnerNoSee=true`와 그림자 설정, 소스 주석). 소유자 화면에는 팔, 동료 화면에는 몸이 보인다. 카메라 `FirstPersonFieldOfView`/`FirstPersonScale`로 팔 왜곡 조정(`bEnableFirstPersonFieldOfView`/`bEnableFirstPersonScale` 필요).
- **B. 전신 메시 하나(1인칭에서도 몸 사용)**: 동료/소유자가 같은 포즈를 보지만 카메라 클리핑·그림자 처리가 필요. 별도 렌더링 처리는 [RENDERING_ART.md](RENDERING_ART.md).
- 포즈 공유: `SetLeaderPoseComponent`(구 `SetMasterPoseComponent`는 **5.1부터 deprecated**, 소스)로 한 메시가 다른 메시 포즈를 따라가게 할 수 있다. 팔·몸 분리 시 도구 메시 부착과 함께 비교.
- 도구 교체: Linked Anim Layer 또는 슬롯 몽타주 + Chooser. 도구 메시는 소유자 전용 표현(프로젝트 현재 방식과 동일 방향). 동료에게는 허용된 장비 외형만 복제(설계 문서).
- 애니메이션 입력: 조준·장비 상태는 서버 확정 복제 상태에서 파생. 원격 Controller 직접 읽기·매 프레임 조준 RPC는 금지(설계 문서).

### 성능: URO · Budget Allocator · Sharing
- URO: `bEnableUpdateRateOptimizations`(소스 `SkinnedMeshComponent.h`), CVar `a.URO.Enable`/`a.URO.Draw`/`a.URO.ForceAnimRate` 등(소스 `Private`).
- `VisibilityBasedAnimTickOption`(소스): `AlwaysTickPoseAndRefreshBones`(기본값, 생성자 확인), `AlwaysTickPose`, `OnlyTickMontagesAndRefreshBonesWhenPlayingMontages`, `OnlyTickMontagesWhenNotRendered`, `OnlyTickPoseWhenRendered`. 구 `EMeshComponentUpdateFlag`는 4.21부터 deprecated. 렌더링되지 않는 몬스터의 노티파이/몽타주 동작이 달라지므로 서버 로직 의존을 확인.
- Animation Budget Allocator: 문서 — 플러그인을 켜고, 메시 컴포넌트를 `USkeletalMeshComponentBudgeted`로 쓰며(C++ `SetDefaultSubobjectClass<USkeletalMeshComponentBudgeted>`), Auto Register/Auto Calculate Significance를 켜고 BeginPlay에서 활성화. CVar `a.Budget.Enabled`/`a.Budget.BudgetMs`(기본 1.0)/`a.Budget.Debug.Enabled`/`a.Budget.MaxTickRate`. 소스 `.uplugin`은 안정 표시.
- Animation Sharing: 리더-팔로워 공유로 대량 군중. 몬스터 수가 군중 규모일 때만.
- 서버 전용 빌드에서 애니 데이터를 제거하는 옵션 `bStripAnimationDataOnDedicatedServer`(소스 `AnimationSettings.h`)는 전용 서버가 채택될 때만 해당하며 현재 구조는 리슨 서버다.
- 실제 효과는 Unreal Insights(`GameplayInsights` 활성)로 측정한다. 이번에 측정하지 않았다.

### 몬스터 애니메이션 고려
- 몬스터 행동·종별 BT는 미결([AI_NAVIGATION.md](AI_NAVIGATION.md)). 애니메이션 쪽 후보: AnimBP 상태머신 + 몽타주(공격·피격), Chooser로 몽타주 선택, Linked Layer로 종별 이동 포즈 교체.
- 서버는 몬스터 위치·피해·사망을 확정하고, 래그돌/사망 몽타주는 통지 수신자가 표현한다(프로젝트 몬스터 컴포넌트 주석). 사망 상태는 복제 `HealthState`의 `OnRep`에서 클라이언트가 재생한다.
- 지형이 바뀌는 환경이므로 발 IK는 지형 트레이스가 필요하다(IK Rig 또는 Two Bone IK 후보). 비용·정확도는 검증하지 않았다.
- 수가 늘면 URO·Budget·Sharing을 위 순서로 비교. 호스트(리슨 서버)는 서버 + 렌더링을 함께 감당하므로 호스트 부하를 별도 측정해야 한다.

### Sequencer
- 소스: `ALevelSequenceActor::bReplicatePlayback` — "서버의 레벨 시퀀스 재생을 다른 클라이언트와 동기화"(`LevelSequenceActor.h`). 컷신 중 입력·카메라 제어, 재접속 시 재생 위치 복원은 프로젝트가 정해야 한다(미확인).
- 플러그인 상태: `ActorSequence`·`TemplateSequence`는 Beta(소스). `MovieRenderPipeline`은 오프라인 렌더용이라 게임플레이 연출과 별개.

### Mover / CharacterMovement 연결
- 현재 캐릭터 이동은 `ACharacter`/`UCharacterMovementComponent` 기반(템플릿·프로토타입). 애니메이션 속도·방향 값은 CharacterMovement에서 파생한다. 이동 선택 문서는 [GAMEPLAY_FRAMEWORK.md](GAMEPLAY_FRAMEWORK.md).
- Mover는 Experimental이며 `NetworkPrediction`(Beta)에 의존, UAF용 연동(`MoverAnimNext`, Experimental)이 별도 존재(소스 `.uplugin`). 현재 단계 채택 후보로 보지 않는다.

## 보류·비추천 후보와 이유
- **UAF·Mover·MoverAnimNext·ContextualAnimation**: Experimental. 안정성·네트워크 검증 전.
- **Motion Matching**: 1인칭 중심 게임에서 이득 대비 데이터 비용 큼. 일부 하위 기능은 문서에서 Experimental 표시.
- **AnimToTexture**: Experimental.
- **Control Rig Modules**: Beta, 제작 도구 성격.
- **루트모션 기반 이동 연출**: 네트워크 보정과 충돌 가능성. 필요한 곳만.
- **GAS 몽타주 복제만을 위한 GAS 도입**: 비용 큼([GAMEPLAY_SYSTEMS.md](GAMEPLAY_SYSTEMS.md)).

## 확인 한계
- 문서 확인: Animation Blueprints 개요, Montage, Linked Layers, Notifies, Motion Matching, Root Motion, IK Rig, Animation Budget Allocator 페이지는 열었다. Control Rig는 색인만 열려 런타임 상세는 소스 범위로 한정했다. IK Retargeter 런타임 상세, Chooser·Pose Search 사용 상세 문서는 열지 않았다.
- Notify `Trigger on Dedicated Server`의 엔진 생성자 기본값 true는 소스로 확인했다. 현재 공식 문서에는 기본값이 없어 문서·소스 충돌로 판정하지 않는다.
- `bAllowMultiThreadedAnimationUpdate`의 엔진 생성자 기본값 true와 프로젝트 Config 재정의 미발견은 확인했다. 실제 캐릭터 에셋·스켈레톤 구성, Blueprint의 AnimBP 할당·사용 및 런타임 병렬 실행은 확인하지 않았다.
- 성능·네트워크 동작은 측정·실행하지 않았다. 지원 사실은 프로젝트 검증을 뜻하지 않는다.
