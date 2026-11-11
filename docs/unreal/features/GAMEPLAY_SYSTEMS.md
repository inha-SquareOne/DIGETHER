# 게임플레이 시스템 기능 카탈로그

기준일 2026-11-04 · UE 5.8.4 (CL 58902052) · 상태: **조사 자료, 채택 결정 아님**. 색인은 [기능 카탈로그](README.md).

근거 표기: `소스` = 로컬 5.8.4 `Engine/...` 헤더·`.uplugin`·`.Build.cs` 직접 확인. `문서` = Epic 공식 문서 페이지 열람. `미확인` = 확인하지 못함. 프로젝트의 `미사용`은 읽은 C++·설정에서 직접 사용을 확인하지 못했다는 뜻이며 Blueprint·에셋 내부와 실제 모듈 로드는 미확인이다.

## 요약
- **GAS(Gameplay Ability System)**: 속성·효과·태그·큐를 한 틀로 주는 가장 큰 후보. 플러그인 표시는 Beta/Experimental이 아니지만 도입 비용이 크고 프로젝트 문서는 "별도 결정"으로 둔다. 이미 만든 몬스터 체력 컴포넌트와 역할이 겹친다.
- **GameplayTag 쿼리**: 이미 네이티브 태그 4개를 쓴다. `FGameplayTagContainer`·`FGameplayTagQuery`로 "조건 표현"까지 확장하는 것이 비용 대비 효과가 크다. 단 `FGameplayTagCountContainer`·`FGameplayTagRequirements`는 GAS 모듈에 있다.
- **엔진 데미지(`TakeDamage`/`OnTakeAnyDamage`)**: 몬스터 체력이 이미 사용 중. 기본 피해 이벤트에는 팀킬 허용 판정이 없으며 팀 인터페이스와 피해 정책 연결은 프로젝트가 정해야 한다. 서버 권위 호출이 필수.
- **Subsystem·ActorComponent·Delegate 조합**: 프로젝트가 이미 쓰는 기본 구성. 추가 플러그인 없이 서비스·상태 분리가 가능하다.
- **Message Router·Inventory**: 설치 파일명 검색에서 해당 게임플레이 기능을 찾지 못했다. 엔진 미지원으로 확정하지 않는다. 확인된 메시지 후보는 Experimental `AsyncMessageSystem`이다.
- **주의(발견)**: `AllToolsets` 의존 연쇄가 `GameplayAbilities`·`GameFeatures`·`ModularGameplay`를 요구한다. 전이 활성화와 실제 모듈 로드·게임 사용은 구분한다.

## DIGETHER에서 쓰이는 곳
| 관련 기능 | 기능 ID | 현재 프로젝트 상태 |
|---|---|---|
| 몬스터 피해·체력 | S25~S27 | **사용 중**: `Game/Monster/DigetherMonsterHealthComponent`가 `AActor::OnTakeAnyDamage`를 구독, 체력 상태는 복제 프로퍼티. 피해 규칙은 순수 함수(`DigetherMonsterHealthRules`) + 자동 테스트 |
| 공격·팀킬 | S26 | 규칙 미구현(팀 정보 소스 미정). `IGenericTeamAgentInterface`는 사용자 Actor/Controller에도 구현 가능하며 피해 허용 규칙은 따로 필요 |
| 태그 분류 | S11~14 등 | **사용 중**: `Game/Core/DigetherGameplayTags` 네이티브 태그 4개(`Item.Type.*`, `Item.Trait.*`). `Build.cs`에 `GameplayTags` 의존. 쿼리 사용은 아직 없음 |
| 서비스(세션) | S01 | **사용 중**: `UDigetherSessionSubsystem : UGameInstanceSubsystem` |
| 상태 컴포넌트 | S10, S15~24 | **사용 중**: Life·Wallet·Quota·Run·Elevator 등 `UActorComponent` 기반 |
| 이벤트 통지 | 공통 | **사용 중**: Life 컴포넌트 `DECLARE_MULTICAST_DELEGATE_*`, 몬스터 컴포넌트 `DECLARE_DYNAMIC_MULTICAST_DELEGATE_*`. 델리게이트 규칙은 [개발 패턴](../ENGINEERING_PATTERNS.md) |
| 치트 | 개발용 | 개발 모듈 `DIGETHERDevTools` 등록부(2026-11-11), `Game/Dev/DigetherCheatManager`는 엔진 기본 치트용 빈 클래스. 상세는 [DEBUG_TEST.md](DEBUG_TEST.md) |
| GAS·GameFeatures·ModularGameplay | — | 게임 모듈 직접 의존 **없음**(`DIGETHER.Build.cs` 확인). `.uproject` 직접 목록에도 없음. 툴셋의 전이 의존 존재, 실제 로드 미확인 |
| 상호작용 | S04 | 프로젝트 자체 `UInterface` 방식 계획([개발 시스템 설계](../../design/DEVELOPMENT_SYSTEM_DESIGN.md) 구현 규칙). 엔진 플러그인 미사용 |

## 기능 목록
| 기능 | 모듈·플러그인 (타입, 안정성) | 하는 일 | DIGETHER 쓸모 | 프로젝트 상태 | 근거 |
|---|---|---|---|---|---|
| Gameplay Ability System | `GameplayAbilities` 플러그인: `GameplayAbilities` Runtime, `GameplayAbilitiesEditor` UncookedOnly. `IsBetaVersion=false`·Experimental 키 없음, `EnabledByDefault:false`, VersionName 1.0 | ASC·Ability·Attribute·Effect·Cue·Task 프레임워크 | 속성/버프/쿨다운이 많아질 때. 현재 규모에선 비용 큼 | 전이 의존 존재, C++ 직접 사용·실제 로드 미확인 | 소스 `Engine/Plugins/Runtime/GameplayAbilities/GameplayAbilities.uplugin`; 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-ability-system-for-unreal-engine |
| GAS Game Feature Actions | `AbilitySystemGameFeatureActions` (Runtime, **Beta** 0.1, `Plugins/Experimental`) | GameFeature로 능력/속성 부여 | GameFeatures 채택 시에만 | 미사용 | 소스 `Engine/Plugins/Experimental/AbilitySystemGameFeatureActions/*.uplugin` |
| GameplayTag 컨테이너·쿼리 | `GameplayTags` 엔진 모듈(Runtime), 에디터 `GameplayTagsEditor` | 계층 태그, `HasTag`/`HasAny`/`HasAll`/`HasTagExact`, `FGameplayTagQuery` | 상호작용 조건·아이템 조건 표현 | 태그 선언만 사용 | 소스 `Engine/Source/Runtime/GameplayTags/Classes/GameplayTagContainer.h`; 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/using-gameplay-tags-in-unreal-engine |
| 태그 카운트·요구 조건 | `FGameplayTagCountContainer`, `FGameplayTagRequirements` (GAS 모듈 `GameplayEffectTypes.h`) | 태그 수 추적·변경 델리게이트, Require/Ignore 조건 | GAS 없이 쓰려면 GameplayAbilities 모듈 의존 필요 | 미사용 | 소스 `Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/GameplayEffectTypes.h` |
| IGameplayTagAssetInterface | `GameplayTags` | 액터가 `GetOwnedGameplayTags` 제공, `HasAny/AllMatchingGameplayTags` | 상호작용 대상의 태그 조건 | 미사용 | 소스 `.../GameplayTags/Classes/GameplayTagAssetInterface.h` |
| 엔진 데미지 | `Engine` 모듈 | `AActor::TakeDamage`, `UGameplayStatics::ApplyDamage/ApplyPointDamage/ApplyRadialDamage`, `FDamageEvent`, `UDamageType`, `OnTake*Damage` | 단순 피해 경로. 몬스터가 이미 구독 | **사용 중** | 소스 `Engine/Source/Runtime/Engine/Classes/GameFramework/Actor.h`, `Engine/DamageEvents.h`, `GameFramework/DamageType.h`, `Kismet/GameplayStatics.h`, `Engine/Private/Actor.cpp` |
| 팀 식별 | `AIModule`: `IGenericTeamAgentInterface`, `FGenericTeamId` | Actor에 구현 가능한 팀 ID·태도 인터페이스. 기본 구현은 동일 ID Friendly·다른 ID Hostile | 팀 분류 재료. 피해 허용·팀킬 정책은 별도 | `AIModule` 의존 있음, 미사용 | 소스 `Engine/Source/Runtime/AIModule/Classes/GenericTeamAgentInterface.h`, `Engine/Source/Runtime/AIModule/Private/AIInterfaces.cpp` |
| Message Router (`GameplayMessageSubsystem`) | 설치 파일명 검색에서 미발견 | 엔진 내 동일 기능 제공 여부 미확인 | Lyra 샘플 구현과 별도 확인 필요 | 미확인 | 미확인: 파일명 검색은 소스 확인 근거가 아님 |
| Async Message System | `AsyncMessageSystem` (Runtime, **Experimental** 0.1, `Plugins/Experimental`) | 틱 그룹/스레드 기준 비동기 게임 메시지 큐 | 이벤트 버스 후보. Experimental이라 신중 | 미사용 | 소스 `Engine/Plugins/Experimental/AsyncMessageSystem/AsyncMessageSystem.uplugin`, `.../Public/AsyncGameplayMessageSystem.h` |
| Interaction Interface | `InteractionInterface` (Runtime `InteractableInterface`, **Experimental**, 의존 SmartObjects·GameplayAbilities) | Instigator/Target 인터페이스·컴포넌트, 타깃 탐색 Task | 상호작용(S04) 후보지만 의존 사슬이 큼 | 미사용 | 소스 `Engine/Plugins/Experimental/InteractionInterface/*.uplugin`, `.../Public/*.h` |
| Gameplay Interactions | `GameplayInteractions` (Runtime, **Experimental**, 의존 StateTree·SmartObjects·ContextualAnimation·NavCorridor·GameplayStateTree) | StateTree 기반 플레이어/NPC 상호작용 | AI·NPC 쪽. 플레이어 상호작용에는 과함 | 미사용 | 소스 `Engine/Plugins/Runtime/GameplayInteractions/GameplayInteractions.uplugin` |
| Gameplay Behaviors | `GameplayBehaviors` (Runtime/UncookedOnly/Editor, **Beta** 0.1, 기본 비활성, `Plugins/Experimental`, 의존 GameplayAbilities) | AI용 fire-and-forget 행동 | 몬스터 행동 후보 | 미사용 | 소스 `Engine/Plugins/Experimental/GameplayBehaviors/GameplayBehaviors.uplugin` |
| Smart Objects | `SmartObjects` (Runtime/Editor/UncookedOnly, `IsBetaVersion=false`·Experimental 키 없음, 기본 비활성, VersionName 0.1) | 레벨 배치 활동 슬롯 점유·예약 | AI 정찰 지점·장치 슬롯 후보. **모듈이 GameplayAbilities·TargetingSystem·MassEntity에 의존** | 미사용 | 소스 `Engine/Plugins/Runtime/SmartObjects/SmartObjects.uplugin`, `.../SmartObjectsModule.Build.cs`; 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/smart-objects-in-unreal-engine |
| Targeting System | `TargetingSystem` (Runtime, **Beta**, 의존 GameplayAbilities) | 프리셋 기반 즉시·비동기 타깃 선택(`UTargetingSubsystem`) | 조준 보조·타깃 선별. 현재 조준 상호작용은 직접 트레이스로 충분 | 미사용 | 소스 `Engine/Plugins/Experimental/GameplayTargetingSystem/TargetingSystem.uplugin`, `Engine/Plugins/Experimental/GameplayTargetingSystem/Source/GameplayTargetingSystem/Public/TargetingSystem/TargetingSubsystem.h` |
| Subsystem | `Engine` 모듈 | Engine/GameInstance/World/LocalPlayer Subsystem | 서비스 구성의 기본 | **사용 중**(세션) | 소스 `Engine/Source/Runtime/Engine/Public/Subsystems/*.h` |
| Actor Component 조합 | `Engine` | 기능을 `UActorComponent`로 분리 | 기본 구성 | **사용 중** | 소스 프로젝트 `Game/*/…Component.h` |
| Delegates | `CoreUObject`/`Core` | 단순/멀티캐스트/Dynamic 델리게이트 | 이벤트 통지 | **사용 중** | 소스 프로젝트 `DigetherLifeStateComponent.h`, `DigetherMonsterHealthComponent.h` |
| GameplayTasks | `GameplayTasks` 엔진 모듈 | `UGameplayTask`, `UGameplayTasksComponent` 비동기 작업 | GAS·AI Task의 기반. 단독 사용은 드묾 | 미사용 | 소스 `Engine/Source/Runtime/GameplayTasks/Classes/` |
| Inventory | 설치 파일명 검색에서 해당 게임플레이 기능 미발견 | 엔진 내 동일 기능 제공 여부 미확인 | 기존 프로젝트 인벤토리 규칙과 후보 비교 필요 | 프로젝트 `PrivateState`·아이템 정의 | 미확인: 파일명 검색은 소스 확인 근거가 아님 |
| Cheat Manager | `Engine`: `UCheatManager` | 개발 치트 | [DEBUG_TEST.md](DEBUG_TEST.md) 참조 | 사용 중 | 소스 `.../Classes/GameFramework/CheatManager.h` |
| Game Features | `GameFeatures` (Runtime PreDefault + Editor, **Beta** 1.0, 기본 비활성, 의존 ModularGameplay·DataRegistry 등) | 기능을 플러그인 단위로 활성/비활성 | 소규모 팀·단일 타이틀엔 이득 작음 | 전이 의존·`GameFeatureData` 스캔 규칙 존재, 실제 로드 미확인 | 소스 `Engine/Plugins/Runtime/GameFeatures/GameFeatures.uplugin`; 문서 https://dev.epicgames.com/documentation/en-us/unreal-engine/game-features-and-modular-gameplay-in-unreal-engine |
| Modular Gameplay | `ModularGameplay` (Runtime PreDefault, **Beta** 1.0, 기본 비활성) | `UGameFrameworkComponentManager`로 수신 등록한 Actor에 컴포넌트 주입·초기화 상태 관리 | GameFeatures 없이도 컴포넌트 주입 용도 | 전이 의존 존재, C++ 직접 사용·실제 로드 미확인 | 소스 `Engine/Plugins/Runtime/ModularGameplay/ModularGameplay.uplugin`, `Engine/Plugins/Runtime/ModularGameplay/Source/ModularGameplay/Public/Components/GameFrameworkComponentManager.h` |
| World Conditions | `WorldConditions` (Runtime/Editor/UncookedOnly, **Experimental**, 기본 비활성) | 캐시되는 월드 조건 | SmartObjects/StateTree 조건. 직접 쓸 일 적음 | 미사용 | 소스 `Engine/Plugins/Runtime/WorldConditions/WorldConditions.uplugin` |

## 기능별 상세

### Gameplay Ability System
- 핵심 클래스: `UAbilitySystemComponent`, `UGameplayAbility`, `UAttributeSet`, `UGameplayEffect`(+`GameplayEffectComponents/`), `UAbilityTask`, `UGameplayCueManager`·`UGameplayCueNotify_*`, `IAbilitySystemInterface`. 모두 `Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/` (소스). 구성 요소 정의는 [공식 개요](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-ability-system-for-unreal-engine)(문서: ASC·Ability·Attribute·Effect·Ability Task 정의 확인. 복제 모드·ASC 위치·주의점은 이 페이지에 없음).
- 설정 위치: Project Settings > Gameplay Abilities Settings(`UGameplayAbilitiesDeveloperSettings`, `defaultconfig`, 소스). 항목: AbilitySystemGlobals 클래스, 글로벌 어트리뷰트 기본값 CurveTable, 어트리뷰트 메타데이터 DataTable, GameplayCueManager 클래스, GameplayCue 경로. 변경에 재시작이 필요한 항목이 있음(`ConfigRestartRequired`).
- **복제 모드**(`EGameplayEffectReplicationMode`, 소스 `AbilitySystemComponent.h`): `Minimal` = 최소 GE 정보만(소스 주석: *소유된(Owned) ASC에는 동작하지 않음, Mixed 사용*), `Mixed` = 시뮬레이티드 프록시엔 최소, 소유자·자율 프록시엔 전체, `Full` = 모두에게 전체. 설정은 `SetReplicationMode`.
- **예측**: 소스 `GameplayPrediction.h` 주석 기준 능력 활성화·GE 적용(속성 수정자·태그)·Cue·몽타주·이동이 예측 대상. **예측하지 않는 것**: GE 제거, 주기적(DoT) 효과, GE Execution 계산(수정자만 예측). 능력 `NetExecutionPolicy`에 `LocalPredicted`가 있다. 리슨 서버에서 호스트 본인은 권위라 예측이 필요 없고 원격 클라이언트만 예측 경로를 탄다(일반 구조에서 도출, 프로젝트 검증 미확인).
- **Cue**: `OnActive`/`WhileActive`/`Executed`/`Removed` 이벤트(소스 `GameplayEffectTypes.h`). 연출 전용이라 [개발 시스템 설계](../../design/DEVELOPMENT_SYSTEM_DESIGN.md)의 "권위 판정은 연출에 의존하지 않는다" 규칙과 맞다. 다만 사운드·Niagara 자산 연결은 [AUDIO.md](AUDIO.md)·[RENDERING_ART.md](RENDERING_ART.md)와 함께 봐야 한다.
- **몽타주 연동**: ASC가 `PlayMontage`와 복제용 `GameplayAbilityRepAnimMontage`를 제공(소스). 애니메이션 쪽 설명은 [ANIMATION.md](ANIMATION.md).
- 디버그: `AbilitySystemDebugHUD.h`, `GameplayDebuggerCategory_Abilities.h`(초기 조사에서 파일 존재만 확인. API·사용법 미확인).
- **deprecated 주의(소스)**: `EGameplayAbilityInstancingPolicy::NonInstanced`는 5.5에서 game 코드 deprecated(`InstancedPerActor` 권장). `UAbilitySystemComponent`의 일부 public 멤버(예: 5.5의 `RemoveActiveGameplayEffect` 관련, 5.7의 태그 카운트 복제 상태 관련)도 deprecated 표시가 있다. GE의 `TargetEffectSpecs` 등 5.3 deprecated(대체: `UAdditionalGameplayEffectComponent`). 구 4.x 튜토리얼 코드를 그대로 쓰면 경고가 난다.
- **도입 비용·장단점(조사 결과의 정리, 결정 아님)**
  - 이점: 버프/디버프·쿨다운·자원 소모·스택·면역 같은 규칙이 데이터 에셋으로 조합된다. 태그 기반 상태 차단, 예측·복제·Cue가 한 틀에 있다. 에디터 디버그 도구가 있다.
  - 비용: 속성은 ASC 안에서 관리되므로 현재의 순수 규칙 + 자동 테스트 구조(`DigetherMonsterHealthRules`)와 다른 구조가 된다. 학습 곡선, C++ 보일러플레이트(AttributeSet 매크로), ASC 소유 위치(Pawn vs PlayerState)와 복제 모드 선택, 프로젝트의 "정보 노출 경계"(히든 가치, 엘리베이터 중량 단계) 규칙과의 정합 검증이 필요하다. 복제 모드(Minimal/Mixed/Full)마다 동료에게 전달되는 GE 정보 범위가 다르므로 [네트워크 권위](../NETWORK_AUTHORITY.md)의 공개 범위 원칙과 모드별로 대조해야 한다.
  - 프로젝트 설계는 "범위 밖·별도 결정"으로 정리([표준 엔진 기능](../STANDARD_FEATURES.md), [개발 시스템 설계](../../design/DEVELOPMENT_SYSTEM_DESIGN.md)). 지원 사실이 채택·검증을 뜻하지 않는다.
- 대안: 컴포넌트 + 순수 규칙 + 델리게이트(현 방식). 기능이 늘면 비교 대상.
- 플러그인 상태 요약: `GameplayAbilities`는 `IsBetaVersion:false`, `IsExperimentalVersion` 키 없음(소스). 같은 계열 `AbilitySystemGameFeatureActions`(Beta), `TargetingSystem`(Beta), `GameplayBehaviors`(Beta)는 별개로 표시된다.

### GameplayTag 쿼리 활용
- 정의·선언·설정 파일 방법은 [DATA_SAVE.md](DATA_SAVE.md)와 [표준 엔진 기능](../STANDARD_FEATURES.md#gameplaytag)을 따른다. 여기서는 조회만 정리한다.
- 컨테이너 조회(소스 `GameplayTagContainer.h`): `HasTag`(부모 포함 매칭), `HasTagExact`, `HasAny`/`HasAll`, `HasAnyExact`/`HasAllExact`, `Filter`/`FilterExact`, `MatchesQuery(FGameplayTagQuery)`.
- 쿼리 표현식(소스 `EGameplayTagQueryExprType`): `AnyTagsMatch`, `AllTagsMatch`, `NoTagsMatch`, `AnyExprMatch`, `AllExprMatch`, `NoExprMatch`. 문서에 따르면 빈 쿼리는 All/No에서 true(https://dev.epicgames.com/documentation/en-us/unreal-engine/using-gameplay-tags-in-unreal-engine).
- 액터 단위 조회는 `IGameplayTagAssetInterface`(`GetOwnedGameplayTags` 구현 → `HasMatchingGameplayTag` 등 제공).
- 복제: 컨테이너가 `UPROPERTY(Replicated)`일 때의 정책은 [네트워크 권위](../NETWORK_AUTHORITY.md). `GameplayTagContainerNetSerializer.h`가 Iris용 직렬화를 제공한다(소스 `GameplayTags/Public/`; Iris 사용 여부는 프로젝트 미결정).
- 주의: 태그는 분류·조건용이고 "동시에 참일 수 없는 상태"는 enum 상태머신이라는 프로젝트 규칙([개발 패턴](../ENGINEERING_PATTERNS.md)).

### 엔진 데미지 시스템과 팀킬 정보
- 호출 경로(소스): `UGameplayStatics::ApplyDamage` → `AActor::TakeDamage(float, FDamageEvent const&, AController*, AActor*)`. 점·반경 피해는 `FPointDamageEvent`(Damage, ShotDirection, `FHitResult HitInfo`)·`FRadialDamageEvent`. `TakeDamage`가 `ReceiveAnyDamage`/`OnTakeAnyDamage`/`OnTakePointDamage`/`OnTakeRadialDamage`를 로컬에서 방송한다. **이 호출은 네트워크로 전파되지 않는다.**
- **권위 검사 차이(소스 `Pawn.cpp`)**: `APawn::ShouldTakeDamage`는 `ROLE_Authority`·`CanBeDamaged()`·권위 GameMode 존재·`Damage != 0`을 요구한다. 일반 `AActor::TakeDamage`는 이 검사를 하지 않는다(프로젝트 몬스터 컴포넌트 주석도 같은 취지로 `CanBeDamaged`를 직접 확인). 서버에서만 호출해야 한다.
- `UDamageType`(소스 `DamageType.h`) 필드: `bCausedByWorld`, `bScaleMomentumByMass`, `DamageImpulse`, `DestructibleImpulse`, `DamageFalloff` 등. 피해 종류(굴착 파편, 낙석, 몬스터 근접 등)를 클래스로 구분할 수 있다. 이벤트에서 얻는 정보는 DamageType CDO·`EventInstigator`(AController)·`DamageCauser`(AActor)·점 피해의 `HitInfo`다.
- **팀킬 판정 재료**: 기본 피해 경로에 프로젝트의 팀킬 허용 정책은 없다. `EventInstigator`→`PlayerState` 식별, `DamageCauser`, `DamageType`을 사용할 수 있다. `AAIController`는 `IGenericTeamAgentInterface`를 기본 구현하고 `APlayerController`는 기본 구현하지 않지만, 사용자 Actor/Controller에 직접 구현할 수 있다(`GenericTeamAgentInterface.h`, `AIController.h`, `PlayerController.h`). 헤더의 "모든 팀 적대" 주석과 달리 `Engine/Source/Runtime/AIModule/Private/AIInterfaces.cpp`의 실제 기본 solver는 **동일 ID Friendly, 다른 ID Hostile**이다. `GetTeamAttitudeTowards`는 상대가 인터페이스 미구현이면 Neutral을 반환한다. 이 인터페이스가 피해 허용을 자동 적용하지는 않으므로 팀킬 정책(S26)은 별도로 정한다.
- 프로젝트 사용: `DigetherMonsterHealthComponent`가 `OnTakeAnyDamage`에 바인딩해 서버 적용 후 복제 상태로 전파. 템플릿 `Variant_Combat/CombatCharacter`도 `TakeDamage`를 재정의한다(템플릿 잔재).
- 대안: GAS의 `UAttributeSet`+GE(위 절). 단순 체력만이면 엔진 데미지 + 컴포넌트가 가볍다.

### Message Router와 이벤트 버스
- Lyra 샘플의 `GameplayMessageSubsystem`(Message Router)에 해당하는 플러그인·클래스는 설치된 5.8.4 `Engine/` 파일명 검색에서 발견되지 않았다. 파일명 검색만으로 엔진 미지원이나 샘플 전용을 확정할 수 없다. Lyra 샘플 구현은 미확인이다. 엔진 `Messaging` 모듈의 `FMessageRouter`와 동일한 게임플레이 API인지도 별도 확인이 필요하다.
- 엔진 내 대안: Experimental `AsyncMessageSystem`(`FAsyncGameplayMessageSystem`, `UAsyncMessageWorldSubsystem`, 바인딩 컴포넌트·엔드포인트 존재, 소스). Experimental이므로 채택 전 안정성 확인이 필요하다.
- 프로젝트 규칙은 "이벤트는 타입 있는 델리게이트"([개발 패턴](../ENGINEERING_PATTERNS.md)). 델리게이트로 충분한지 먼저 본다.

### Gameplay Interactions · Interaction Interface · Smart Objects
- **Smart Objects**: [공식 랜딩 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/smart-objects-in-unreal-engine)는 활동 묶음을 예약 시스템으로 사용한다고 설명한다. 슬롯별 점유 규칙은 별도 확인이 필요하다. `.uplugin`은 `IsBetaVersion=false`·Experimental 키 없음, VersionName 0.1이다(소스). **`SmartObjectsModule`이 `GameplayAbilities`·`TargetingSystem`(Beta)·`MassCore`·`MassEntity`를 PublicDependency로 둔다**(소스 `.Build.cs`). 의존 선언과 실제 모듈 로드·채택은 구분한다. AI 활용·위치 선택은 [AI_NAVIGATION.md](AI_NAVIGATION.md).
- **Interaction Interface**: `UInteractionTargetComponent : UBoxComponent, IInteractionTarget`, `UInteractionInstigatorComponent : UActorComponent, IInteractableInstigator`, 타깃 목록 변경 델리게이트를 가진 `InteractionTask_WaitForTargets`를 제공한다(소스 `Engine/Plugins/Experimental/InteractionInterface/Source/InteractableInterface/Public/`의 해당 헤더 3개). Experimental이며 SmartObjects·GAS 의존. 프로젝트가 "조준 상호작용 + 서버 검증 + 자체 `UInterface`"를 이미 계획했으므로 대체 가치는 낮다.
- **Gameplay Interactions**: StateTree 기반 NPC/플레이어 상호작용. Experimental. 서버 권위·소유권 정책은 플러그인이 정하지 않는다(미확인: 네트워크 동작은 이번에 읽지 않음).

### Subsystem으로 게임 서비스 구성
- 종류(소스): `UEngineSubsystem`, `UGameInstanceSubsystem`, `UWorldSubsystem`(`ShouldCreateSubsystem`, `DoesSupportWorldType`, `OnWorldBeginPlay`, `PostInitialize`, `PreDeinitialize`), `UTickableWorldSubsystem`, `ULocalPlayerSubsystem`. 
- 네트워크: Subsystem 객체는 복제되지 않는다. 호스트의 서버 전용 서비스는 `UWorldSubsystem`/`UGameInstanceSubsystem`에 두되 클라이언트에 보일 값은 GameState/PlayerState 등 복제 액터로 전달한다([표준 엔진 기능](../STANDARD_FEATURES.md#subsystem)). 월드 전환 시 인스턴스 수명 차이는 [GAMEPLAY_FRAMEWORK.md](GAMEPLAY_FRAMEWORK.md).
- 프로젝트 사용: `UDigetherSessionSubsystem`. 

### Actor Component 조합 · Delegates
- 기능을 `UActorComponent`로 분리하면 소유 액터 종류(플레이어, 몬스터, 장치)에 재사용된다. 프로젝트는 상태 원본 컴포넌트(`DigetherLifeStateComponent`, `DigetherMonsterHealthComponent` 등)에 복제 프로퍼티와 규칙을 두고 있다. 컴포넌트 복제는 소유 액터의 `bReplicates`와 컴포넌트 `SetIsReplicated`가 모두 필요하다(일반 엔진 규칙, 이번 헤더 확인 안 함 — 미확인).
- 델리게이트 선택: C++ 내부는 `DECLARE_MULTICAST_DELEGATE`(가볍고 리플렉션 없음), BP 노출은 `DECLARE_DYNAMIC_MULTICAST_DELEGATE`(UFUNCTION 바인딩, 비용 큼). 프로젝트 몬스터 컴포넌트는 Dynamic, Life 컴포넌트는 일반 멀티캐스트를 쓴다(소스, 프로젝트).

### Game Features · Modular Gameplay
- 문서(https://dev.epicgames.com/documentation/en-us/unreal-engine/game-features-and-modular-gameplay-in-unreal-engine): 두 플러그인을 켜고 `/Plugins/GameFeatures/` 아래 Content Only 플러그인과 `GameFeatureData`를 만들어 Add Cheats / Add Components / Add Data Registry / Add World Partition Content 액션을 구성. 릴리스 상태는 문서에 없음. `.uplugin`은 둘 다 `IsBetaVersion:true`(소스).
- **프로젝트 의존 상태**: `.uproject`는 `AllToolsets`를 명시 활성화하며, 툴셋의 전이 의존은 [게임플레이 프레임워크](GAMEPLAY_FRAMEWORK.md)에서 확인한다. `DefaultGame.ini`에도 `GameFeaturesToolset` 의존을 위한 `GameFeatureData` 스캔 규칙이 있다. 이번 대조에서 에디터 로그·실제 모듈 로드·패키지 포함 여부는 확인하지 않았다. 플러그인 직접 목록에 없다는 사실을 비활성으로 읽거나 의존 선언을 실행 검증으로 읽지 않는다.
- 쓸모: 소규모 단일 타이틀에서 기능 분리 이득이 작고 Beta 표시가 있다. 컴포넌트 주입만 필요하면 `ModularGameplay`의 `UGameFrameworkComponentManager`(소스 `ModularGameplay/Public/Components/`)가 후보지만 Beta다.

### 인벤토리·타깃팅
- Inventory: `Engine/` 파일명 검색에서 엔진 게임플레이 인벤토리 컴포넌트·플러그인은 나오지 않았다(결과는 Steamworks SDK 헤더 `isteaminventory.h`와 DMX GDTF 무관 파일뿐). 검색 실패이므로 "미지원 확정"이 아니라 "발견되지 않음"이다. 프로젝트는 자체 `PrivateState` 기반을 이미 사용한다.
- Targeting System: Beta. `UTargetingSubsystem::ExecuteTargetingRequest`·`StartAsyncTargetingRequestWithHandle`이 프리셋/핸들 기반 즉시·비동기 요청을 제공한다(소스 `Engine/Plugins/Experimental/GameplayTargetingSystem/Source/GameplayTargetingSystem/Public/TargetingSystem/TargetingSubsystem.h`). 의존: GameplayAbilities.

## 보류·비추천 후보와 이유
- **Message Router 대체로 AsyncMessageSystem 즉시 채택**: Experimental. 델리게이트로 충분하면 보류.
- **Interaction Interface / Gameplay Interactions 채택**: Experimental, SmartObjects·GAS까지 끌고 옴. 자체 `UInterface` 계획과 겹침.
- **Smart Objects를 상호작용 장치에 사용**: AI 슬롯 용도에 맞음. 플레이어 장치 상호작용에는 과하다(후보 판단, 검증 안 함).
- **GAS 전면 도입**: 지금 규모에선 기존 컴포넌트·순수 규칙·테스트가 이미 있다. 속성·버프·쿨다운이 다수 생기는 시점에 비교한다.
- **Game Features**: Beta, 팀 규모·단일 타이틀 기준 이득 작음. 현재 툴셋 전이 의존과 게임 기능 채택은 구분한다.

## 확인 한계
- 문서 확인: GAS 개요, Gameplay Tags, Smart Objects, Game Features 페이지는 열었으나 복제 모드·ASC 소유 위치·예측 주의는 공식 개요에 없어 소스 주석으로 대신했다. GAS 하위 페이지(Attribute/Effect)는 본문 추출 실패로 소스만 근거로 썼다.
- Message Router 전용 여부(Lyra)와 GAS 디버그 HUD 사용법은 확인하지 못했다.
- 컴포넌트 복제 요건, GameplayInteractions·InteractionInterface 네트워크 동작은 이번에 읽지 않았다.
- 패키지 빌드에서 `AllToolsets` 연쇄 플러그인의 포함 여부는 미확인. 지원 사실은 프로젝트 검증을 뜻하지 않는다.
