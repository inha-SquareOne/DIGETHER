# 데이터·저장 기능 카탈로그

기준일 2026-11-04 · UE 5.8.4 (CL 58902052) · 상태: **조사 자료, 채택 결정 아님**. 색인은 [기능 카탈로그](README.md).

이미 정한 사용 규칙(Primary Data Asset·태그·소프트 레퍼런스·Developer Settings·Data Validation)은 [표준 엔진 기능](../STANDARD_FEATURES.md)과 [데이터 작업](../DATA_WORKFLOW.md)이 원본이다. 여기서는 반복하지 않고 "엔진에 무엇이 더 있는지, 어디에 있는지, 무엇이 위험한지"만 적는다. 근거 표시는 `소스`(로컬 5.8.4 헤더·uplugin 직접 확인) · `문서`(Epic 문서 열람) · `미확인`이다. 프로젝트의 `미사용`은 읽은 C++·설정에서 직접 사용을 확인하지 못했다는 뜻이며 Blueprint·에셋 내부와 실행 상태는 미확인이다.

## 요약

- **Primary Data Asset + Asset Manager**는 이미 쓰는 중(`DigetherItem`·`DigetherTuningProfile`). 몬스터 정의(`DigetherMonster`)는 클래스는 있지만 Asset Manager 등록이 아직 없다([상세](#asset-manager)).
- **USaveGame + `UGameplayStatics` 비동기 저장**이 영구 저장의 엔진 기본 경로다. 스키마 버전 관리는 엔진이 알아서 해 주지 않는다. `ULocalPlayerSaveGame`에 버전·후처리 훅이 있고, 일반 `USaveGame`은 직접 버전 필드를 둬야 한다.
- **String Table + Localization Dashboard(PO)**가 현지화 기본 경로다. 프로젝트에 String Table 1개와 Game 현지화 대상 설정이 있고, Gather·PO 결과 파일은 아직 없다.
- **Data Registry는 Beta 플러그인**(게임 C++ 직접 사용 미발견, GameFeatures·GameplayAbilities의 전이 의존 존재, 실제 로드 미확인). 현재 규모에서는 Asset Manager + DataTable로 충분한지 먼저 본다.
- **Instanced Struct는 5.8에서 코어(CoreUObject) 헤더**다. StructUtils 플러그인은 Experimental·5.5 deprecated 표시이고 모듈 껍데기만 남았다. 다형 데이터 한 칸이 필요할 때 후보.
- **콘솔 변수 `ECVF_Cheat`**는 Shipping에서 숨김·무효(`DISABLE_CHEAT_CVARS`). 프로젝트는 아직 `TAutoConsoleVariable`을 쓰지 않고 자체 치트 등록 틀(`DigetherDev`)을 쓴다.

## DIGETHER에서 쓰이는 곳

| 기능 | 연결 | 현재 프로젝트 상태 |
|---|---|---|
| S11~S24 아이템·정산·튜닝 수치 | 정의 데이터 | **사용**: `UDigetherItemDefinition`·`UDigetherTuningProfile`(UPrimaryDataAsset), `Content/Data/Items`·`Tuning` |
| S25 몬스터 | 몬스터 정의 | `UDigetherMonsterDefinition` 클래스는 있고 `DefaultGame.ini` 스캔 등록은 없음. 에셋 내부·Asset Registry는 미확인 |
| 분류(아이템 종류·특성) | GameplayTag | **사용**: 네이티브 선언 4개(`DigetherGameplayTags.cpp`). `Config/DefaultGameplayTags.ini`·`Config/Tags`는 없음 |
| S30 저장(일차 전환 자동 저장, 수동 저장은 호스트만) | USaveGame | 프로토타입은 `UDigetherPrototypeSaveGame`(`Version=2`) 동기 저장/로드. 정식 `Game/Save/`는 v3만 읽고 쓰며 v2와 별도 슬롯이라는 [사용자 결정](../../design/CODE_STRUCTURE_PLAN.md)이 있음. D29의 허용 단계·PQ-01·PQ-07 등 남은 정책은 [DECISIONS](../../planning/DECISIONS.md) 참고 |
| S31 설정(그래픽·사운드·조작) | UGameUserSettings | **사용**: `UDigetherGameUserSettings`(`GameUserSettingsClassName` 지정), 설정 패널 위젯 |
| 프로젝트 설정(메뉴·맵·위젯 경로) | UDeveloperSettings | **사용**: `UDigetherFrontendSettings`(소프트 경로) |
| 치트·디버그 | 콘솔 | 자체 `DigetherDev::Register` + `UDigetherCheatManager`. 콘솔 변수 미사용 |
| 데이터 검증 | IsDataValid | **사용**: 3개 정의 클래스 + 자동 테스트 `DigetherDataDefinitionTest` |
| 현지화(S31 D20 언어) | String Table·Dashboard | `ST_Session` + `FText::FromStringTable`. Game 대상·9개 문화 설정 존재(현재 해당 ini diff 없음). [시트 번역 안내](../../guides/LOCALIZATION_SHEETS.md) |

## 기능 목록

| 기능 | 모듈·플러그인 (타입, 안정성) | 하는 일 | DIGETHER 쓸모 | 프로젝트 상태 | 근거 |
|---|---|---|---|---|---|
| UDataAsset / UPrimaryDataAsset | Engine (Runtime, 코어) | 에디터 편집 정의 데이터. Primary는 `FPrimaryAssetId`·`AssetBundleData` 제공 | 아이템·몬스터·튜닝 정의 | 사용 | 소스 `Engine/Source/Runtime/Engine/Classes/Engine/DataAsset.h` |
| UAssetManager | Engine (Runtime, 코어) | Primary Asset 스캔·ID 조회·비동기 로드·번들·쿠크 규칙 | 정의 목록 열거·필요 시 로드 | 사용(2종 등록) | 소스 `.../Engine/AssetManager.h`, `AssetManagerSettings.h`; 문서 asset-management-in-unreal-engine |
| FStreamableManager | Engine (Runtime, 코어) | 소프트 경로 비동기/동기 로드, 핸들 | 큰 에셋 지연 로드 | 위젯 클래스 `LoadSynchronous`만 사용 | 소스 `.../Engine/StreamableManager.h` |
| TSoftObjectPtr / TSoftClassPtr | CoreUObject | 하드 참조 없이 에셋·클래스 참조 | 위젯 클래스·무거운 메시 | 사용 | 소스 프로젝트 `DigetherFrontendSettings.h` |
| UDataTable / FTableRowBase | Engine | 행 단위 표. CSV/JSON 임포트, `FDataTableRowHandle` | 수치 표(상점·확률 등) | 미사용 | 소스 `.../Engine/DataTable.h`; 문서 data-driven-gameplay-elements |
| UCompositeDataTable | Engine | 부모 표 여러 개를 합쳐 읽는 표(뒤 표가 덮어씀) | 기본표+모드/난이도 덮어쓰기 | 미사용 | 소스 `.../Engine/CompositeDataTable.h` |
| UCurveTable / UCompositeCurveTable | Engine | 이름 붙은 곡선 묶음(Simple/Rich) | 난이도·레벨별 수치 곡선 | 미사용 | 소스 `.../Engine/CurveTable.h`, `CompositeCurveTable.h` |
| UCurveFloat / FRuntimeFloatCurve | Engine | 단일 곡선 에셋 / 구조체 안 곡선 | 감쇠·보간 곡선 | 미사용 | 소스 `.../Engine/Classes/Curves/CurveFloat.h` |
| GameplayTags | GameplayTags (Runtime) | 계층 태그, ini·DataTable·네이티브 등록 | 아이템 분류·상태 조건 | 사용(네이티브만) | 소스 `Engine/Source/Runtime/GameplayTags/`; 문서 using-gameplay-tags |
| UDeveloperSettings | DeveloperSettings (Runtime) | 프로젝트 설정 화면에 자동 노출되는 설정 클래스 | 맵·위젯 경로, 기능 스위치 | 사용 | 소스 `.../DeveloperSettings/Public/Engine/DeveloperSettings.h` |
| UDeveloperSettingsBackedByCVars | DeveloperSettings | 설정 값을 콘솔 변수와 동기화 | 설정과 CVar를 한 곳에서 관리 | 미사용 | 소스 `.../DeveloperSettingsBackedByCVars.h` |
| TAutoConsoleVariable / FAutoConsoleVariableRef / FAutoConsoleCommand | Core | 콘솔 변수·명령. `ECVF_Cheat` 등 플래그 | 디버그 스위치·튜닝 | 미사용(자체 치트 틀) | 소스 `.../Core/Public/HAL/IConsoleManager.h` |
| Config 시스템 (`UCLASS(Config=)`, `UPROPERTY(Config)`) | CoreUObject | ini 계층 읽기/쓰기, `SaveConfig`·`ReloadConfig` | 사용자 설정·프로젝트 기본값 | 사용 | 소스 `.../CoreUObject/Public/UObject/Object.h` |
| UGameUserSettings | Engine | 해상도·품질·VSync·프레임 제한 저장/적용, 서브클래스로 확장 | 설정 화면 값 저장 | 사용 | 소스 `.../Engine/Classes/GameFramework/GameUserSettings.h` |
| USaveGame + UGameplayStatics Save/Load | Engine | 객체를 플랫폼 저장 슬롯으로 직렬화. 동기·비동기(일반 파일 백엔드는 .sav) | 일차 자동 저장·호스트 수동 저장 | 프로토타입 동기 사용, 정식 v3 전용·별도 슬롯 결정(구현 미완료) | 소스 `Engine/Source/Runtime/Engine/Classes/Kismet/GameplayStatics.h`, `Engine/Source/Runtime/Engine/Public/SaveGameSystem.h`; 문서 [Saving and Loading](https://dev.epicgames.com/documentation/en-us/unreal-engine/saving-and-loading-your-game-in-unreal-engine) |
| UAsyncActionHandleSaveGame | Engine | 비동기 Save/Load의 Blueprint 노드와 `Completed` 성공/실패 통지 | Blueprint에서 비동기 저장을 연결할 때 후보 | C++ 직접 사용 미발견, Blueprint 사용 미확인 | 소스 `Engine/Source/Runtime/Engine/Classes/GameFramework/AsyncActionHandleSaveGame.h` |
| ULocalPlayerSaveGame | Engine | 로컬 플레이어 연결 SaveGame. 버전·PreSave/PostLoad 훅·진행 상태 | 개인 설정/진행 저장 | 미사용 | 소스 `.../GameFramework/SaveGame.h` |
| ISaveGameSystem | Engine | 플랫폼 저장 백엔드(비동기 API 포함) | 플랫폼 저장 백엔드 교체 지점. Steam Cloud 연동은 별도 확인 | 커스텀 백엔드 구현 미발견, SaveGame API는 플랫폼 백엔드를 경유 | 소스 `Engine/Source/Runtime/Engine/Public/SaveGameSystem.h`, `Engine/Source/Runtime/Engine/Private/GameplayStatics.cpp` |
| FCustomVersion | Core | GUID별 정수 버전을 아카이브에 기록 | 커스텀 직렬화 구조체의 버전 | 미사용 | 소스 `.../Core/Public/Serialization/CustomVersion.h` |
| FArchive / FMemoryWriter 계열 | Core·CoreUObject | 바이너리 직렬화 직접 제어 | 저장 포맷을 직접 정할 때 | 미사용 | 소스 `.../Core/Public/Serialization/Archive.h` |
| Json / JsonUtilities | Json, JsonUtilities (Runtime) | JSON 읽기/쓰기, `UStruct`↔JSON 변환 | 사람이 읽는 내보내기·외부 도구 연동 | Json 모듈 의존만 | 소스 `.../JsonUtilities/Public/JsonObjectConverter.h`; 프로젝트 `Build.cs` |
| JsonBlueprintUtilities | 플러그인 (Runtime+UncookedOnly, **Beta**, Experimental=false, 기본 꺼짐) | 블루프린트용 JSON 노드 | 비추천 후보(아래) | 미사용 | 소스 `Engine/Plugins/JsonBlueprintUtilities/JsonBlueprintUtilities.uplugin` |
| Data Validation | 플러그인 DataValidation (Editor, 기본 켜짐, Beta/Experimental 표시 없음) | `IsDataValid`·Editor Validator·저장 시 검증·커맨드렛 | 정의 데이터 오류 조기 검출 | 사용(플러그인 활성) | 소스 `Engine/Plugins/Editor/DataValidation/` |
| Data Registry | 플러그인 DataRegistry (Runtime+UncookedOnly, **Beta**, 기본 꺼짐) | 읽기 전용 구조체 데이터의 전역 조회, 소스 우선순위·비동기 | 데이터 소스가 여러 곳일 때 | 게임 C++ 직접 사용 미발견, GameFeatures·GameplayAbilities 전이 의존 존재, 실제 로드 미확인 | 소스 `Engine/Plugins/Runtime/DataRegistry/DataRegistry.uplugin`, `Engine/Plugins/Runtime/GameFeatures/GameFeatures.uplugin`, `Engine/Plugins/Runtime/GameplayAbilities/GameplayAbilities.uplugin`; 문서 data-registries |
| FInstancedStruct / TSharedStruct / FStructView | CoreUObject (코어 헤더) | 구조체 값 소유 / 공유 소유 / 수명을 소유하지 않는 뷰. `FInstancedStruct`는 `NetSerialize` 제공 | 서로 다른 효과·이벤트 페이로드. 뷰는 원본 수명 안에서 사용 | 미사용 | 소스 `Engine/Source/Runtime/CoreUObject/Public/StructUtils/InstancedStruct.h`, `SharedStruct.h`, `StructView.h` |
| StructUtils 플러그인 | 플러그인 (Runtime, **Experimental**, `DeprecatedEngineVersion 5.5`) | 헤더가 코어로 이전된 뒤의 잔여 모듈 | 활성화 불필요(아래) | 미사용 | 소스 `Engine/Plugins/Experimental/StructUtils/StructUtils.uplugin` |
| String Table (UStringTable) | Engine | 키→텍스트 표 에셋. CSV 가져오기/내보내기 | 반복 UI 문구·번역 원본 | 사용(`ST_Session`) | 소스 `.../Engine/Public/Internationalization/StringTable.h`; 문서 using-string-tables |
| Localization Dashboard | 에디터 모듈 LocalizationDashboard | Gather→PO Export→Import→Compile, 대상(Target)·문화 관리 | 번역 파이프라인 | Game 대상 설정 존재, Gather 결과 미확인 | 소스 `Engine/Source/Editor/LocalizationDashboard/`; 문서 localization-tools |
| Gather/Export 커맨드렛 | UnrealEd | `GatherText`·`InternationalizationExport`·`GenerateTextLocalizationResource` 등 | CI·스크립트로 현지화 자동화 | 미사용 | 소스 `.../UnrealEd/Classes/Commandlets/` |
| FInternationalization / KismetInternationalizationLibrary | Core / Engine | 런타임 문화 전환(`SetCurrentCulture`), 지원 문화 조회 | 언어 설정 화면 | 미사용 | 소스 `.../Engine/Classes/Kismet/KismetInternationalizationLibrary.h` |

## 기능별 상세

### Primary Data Asset
- 핵심: `UPrimaryDataAsset::GetPrimaryAssetId`(기본은 가장 가까운 네이티브 부모 클래스 이름이 타입), `AssetBundleData`, `UpdateAssetBundleData`. 번들은 `TSoftObjectPtr` 프로퍼티의 `AssetBundles` 메타로 지정(소스, 문서).
- 설정: Project Settings > Game > Asset Manager, `DefaultGame.ini [/Script/Engine.AssetManagerSettings] PrimaryAssetTypesToScan`.
- 네트워크: 정의 에셋은 서버·클라이언트 모두 같은 쿠크 결과를 가져야 한다. ID(`FPrimaryAssetId`/`FName`)만 복제하고 정의 본체는 각자 로드하는 형태가 일반적이다(후보).
- 주의: `UDigetherItemDefinition`은 `UCLASS(Const)`. 변경 시 클라이언트 쿠크 포함 여부 확인.

### Asset Manager
- API(소스 `AssetManager.h`): `LoadPrimaryAsset(s)`, `LoadPrimaryAssetsWithType`, `ChangeBundleStateForPrimaryAssets`, `PreloadPrimaryAssets`, `UnloadPrimaryAsset(s)`, `GetPrimaryAssetIdList`, `GetPrimaryAssetPath`, `GetStreamableManager`. **`LoadPrimaryAsset(s)`는 명시적으로 Unload할 때까지 유지되므로 반환 핸들을 보관할 필요가 없다. `PreloadPrimaryAssets`는 반환 핸들을 보관해야 프리로드가 유지된다.**
- 쿠크 규칙 `EPrimaryAssetCookRule`: `Unknown`(기본), `NeverCook`, `ProductionNeverCook`(=`DevelopmentCook`), `DevelopmentAlwaysProductionNeverCook`, `DevelopmentAlwaysProductionUnknownCook`, `AlwaysCook`(소스 `AssetManagerTypes.h`). `bOnlyCookProductionAssets`를 켜면 개발 전용 에셋이 섞인 쿠크는 실패한다(문서).
- 프로젝트: `DefaultGame.ini`에 `GameFeatureData`(툴셋 의존), `DigetherItem`, `DigetherTuningProfile` 3종이 `AlwaysCook`으로 등록. **`DigetherMonster`(클래스 `UDigetherMonsterDefinition`)는 이 ini의 스캔 등록이 없다.** 몬스터 정의를 열거할 때 스캔 등록 또는 명시적 등록 경로를 확인해야 한다. Blueprint·Asset Registry의 동적 등록 여부는 미확인이다.
- 에디터: 플러그인 `AssetManagerEditor`가 프로젝트에 활성(Asset Audit·Size Map용).

### 소프트 레퍼런스·비동기 로드
- `FStreamableManager::RequestAsyncLoad`·`RequestSyncLoad`·`LoadSynchronous`·`CreateCombinedHandle`(소스).
- 프로젝트의 `ADigetherFrontendPlayerController`는 화면 클래스를 `LoadSynchronous`로 불러온다(메뉴 진입 시 1회). 게임플레이 중 호출은 피한다([표준 기능](../STANDARD_FEATURES.md#소프트-레퍼런스와-비동기-로드)).
- 소프트 경로로만 참조된 클래스는 쿠크에 빠질 수 있다. 프로젝트는 `DirectoriesToAlwaysCook=/Game/Frontend`로 해결했다(`DefaultGame.ini`).

### DataTable·CurveTable·Composite
- `UDataTable`: 행 구조체는 `FTableRowBase` 상속. `FindRow<T>`, `FDataTableRowHandle`. 임포트 옵션 `bIgnoreExtraFields`·`bIgnoreMissingFields`·`ImportKeyField`(소스). 반환된 행 포인터를 함수 밖으로 보관하지 않는다(재임포트 반영, 문서).
- **`bStripFromClientBuilds`**: 켜면 클라이언트 빌드에서 표가 제거된다(소스 `NeedsLoadForClient`). 리슨 서버의 클라이언트가 읽어야 하는 표에는 켜지 않는다.
- `UCompositeDataTable`: `AppendParentTables`/`GetParentTables`. 공식 data-driven 문서에는 합성 표 설명이 없어 소스로만 확인했다.
- `UCurveTable`: `ECurveTableMode::SimpleCurves`/`RichCurves`. 곡선이 필요하면 Data Registry는 곡선도 담는다(문서).
- 같은 값을 정의 에셋과 표에 이중으로 두지 않는다([데이터 작업](../DATA_WORKFLOW.md#무결성)).

### GameplayTags
- 선언 매크로(소스 `NativeGameplayTags.h`): `UE_DECLARE_GAMEPLAY_TAG_EXTERN`, `UE_DEFINE_GAMEPLAY_TAG`, `UE_DEFINE_GAMEPLAY_TAG_COMMENT`(프로젝트가 사용), `UE_DEFINE_GAMEPLAY_TAG_STATIC`(파일 안에서만 쓸 때, DECLARE와 짝으로 쓰지 않는다 — 문서).
- 등록 위치(`UGameplayTagsSettings`, 소스): `ImportTagsFromConfig`(ini/`Config/Tags`), `GameplayTagTableList`(`FGameplayTagTableRow` DataTable 목록), 네이티브 코드. `GameplayTagRedirects`로 이름 변경 리다이렉트.
- 복제 옵션: `FastReplication`(인덱스 복제, **서버·클라이언트의 태그 집합이 같아야 함**), `bDynamicReplication`(연결별 인덱스, 비용이 약간 높고 집합이 달라도 됨), `CommonlyReplicatedTags`, `NumBitsForContainerSize`, `NetIndexFirstBitSegment`. 이 프로젝트는 호스트와 클라이언트가 같은 빌드를 쓰지만 설정은 아직 건드리지 않았다.
- `WarnOnInvalidTags`: 저장된 태그 참조가 사전에 없으면 로드 경고. `ClearInvalidTags`는 5.5부터 deprecated(소스).
- 프로젝트의 명시 선언은 `DigetherGameplayTags.cpp`의 네이티브 4개다. 프로젝트 태그 ini가 없다는 사실만으로 플러그인까지 포함한 전체 태그 사전이 4개라고 단정하지 않는다. 에디터에서 새 태그를 추가하면 `DefaultGameplayTags.ini` 또는 선택한 `Config/Tags/*.ini` 원본을 소스 관리한다.

### UDeveloperSettings·Config·GameUserSettings
- `UCLASS(Config=Game, DefaultConfig)` → `DefaultGame.ini`에 저장. `GetCategoryName`/`GetSectionName`/`GetCustomSettingsWidget` 재정의 가능(소스).
- `UObject::SaveConfig`, `TryUpdateDefaultConfigFile`, `ReloadConfig`, `UpdateGlobalUserConfigFile`(소스 `Object.h`). 런타임 변경 저장은 `Saved/Config/<플랫폼>/`의 사용자 ini로 가고, `Default*.ini`는 에디터/소스 통제 대상이다.
- `UGameUserSettings`: `ApplySettings`/`ApplyNonResolutionSettings`/`SaveSettings`/`LoadSettings`/`SetToDefaults`/`ConfirmVideoMode`/`RevertVideoMode`/`SetFrameRateLimit`/`SetOverallScalabilityLevel`/`RunHardwareBenchmark`, 버전 갱신 `UpdateVersion`(소스). 프로젝트 서브클래스는 `Config=GameUserSettings, ConfigDoNotCheckDefaults`로 마스터/효과음/음악 볼륨·마우스 감도를 더했다. `GameUserSettingsClassName`은 `DefaultEngine.ini`에서 지정.
- 5.x deprecated: `WindowPosX/Y`는 5.6부터 `WindowPositions` 배열로 대체(소스). 설정 화면 연결은 [UI](UI.md#설정-화면).

### 콘솔 변수
- 클래스(소스 `IConsoleManager.h`): `TAutoConsoleVariable<T>`, `FAutoConsoleVariableRef`, `FAutoConsoleCommand`(+`WithWorldAndArgs`, `WithWorldArgsAndOutputDevice`).
- 플래그: `ECVF_Cheat`(최종 릴리스 빌드에서 숨김·변경 불가), `ECVF_ReadOnly`, `ECVF_Scalability`, `ECVF_SetBy*` 우선순위(Constructor < Scalability < GameSetting < ProjectSetting < … < Commandline). `DISABLE_CHEAT_CVARS`는 `UE_BUILD_SHIPPING`에서 참, `UE_BUILD_TEST`에서는 `ALLOW_CHEAT_CVARS_IN_TEST`가 없으면 참(`Core/Public/Misc/Build.h`).
- 프로젝트: `DefaultGame.ini [ConsoleVariables]`에 CommonUI 3개만 있다. 치트는 `DigetherDev::Execute`(Shipping 차단)와 `UCheatManager`로 구현돼 있어 CVar와 역할이 겹친다. 어느 쪽을 기준으로 할지는 결정 사항.
- 네트워크: 콘솔 변수는 로컬 값이다. 서버 판정이 걸린 값은 RPC/복제로 전달한다([네트워크](NETWORKING.md)).

### SaveGame
- API(소스 `GameplayStatics.h`): `CreateSaveGameObject`, `SaveGameToSlot`, `AsyncSaveGameToSlot`, `LoadGameFromSlot`, `AsyncLoadGameFromSlot`, `DoesSaveGameExist`, `DeleteGameInSlot`, `SaveGameToMemory`/`LoadGameFromMemory`, `SaveDataToSlot`/`LoadDataFromSlot`. 비동기는 직렬화를 게임 스레드에서, 파일 쓰기를 작업 스레드에서 하고 콜백은 게임 스레드로 돌아온다. 공식 문서도 비동기 저장을 권장한다(문서).
- 파일: 일반 파일 백엔드는 `Saved/SaveGames/<슬롯>.sav`에 `FFileHelper::SaveArrayToFile`로 쓴다(`Engine/Public/SaveGameSystem.h`). `Engine/Source/Runtime/Core/Private/Misc/FileHelper.cpp`는 전달된 경로에 writer를 만들고 Serialize/Close한다. **이 두 함수 자체에는 임시 파일을 쓴 뒤 교체하는 절차가 없다.** 플랫폼 파일 계층·OS까지 포함한 장애 원자성은 미확인이다. 중단·용량 부족 대비는 [영구 저장 규칙](../DATA_WORKFLOW.md#영구-저장)대로 프로젝트가 따로 설계한다.
- 직렬화 방식(소스 `GameplayStatics.cpp`): 헤더(타입 태그 `SAVG`, 패키지 파일 버전, **현재 등록된 커스텀 버전 전체**)를 쓰고, 본문은 `FObjectAndNameAsStringProxyArchive`로 객체 참조·이름을 문자열로 바꿔 `Serialize`한다. 주석에 따르면 **`SaveGame` 프로퍼티 플래그는 검사하지 않고 비-Transient 프로퍼티를 모두 쓴다.** 저장하지 말아야 할 런타임 필드는 `Transient`로 표시한다.
- 버전 관리: `USaveGame` 자체에는 스키마 버전 필드가 없다(문서에도 버전 안내 없음). 선택지는 ① 자체 `int32 Version` 필드(프로토타입이 사용) ② `ULocalPlayerSaveGame`의 `GetLatestDataVersion`/`SavedDataVersion`/`HandlePostLoad`(로드 후 변환)/`HandlePreSave`/`HandlePostSave` ③ 커스텀 직렬화에서 `FCustomVersionRegistration` + `Ar.UsingCustomVersion`/`Ar.CustomVer`. 프로퍼티 추가·삭제를 UObject 태그 직렬화가 어디까지 흡수하는지는 이번에 확인하지 못했다(미확인) — 마이그레이션 시험이 필요하다.
- `ULocalPlayerSaveGame`: `AsyncLoadOrCreateSaveGameForLocalPlayer`는 로드 실패 시 새 객체를 만들어 돌려준다. **손상 파일을 기본값으로 덮어쓰지 않는다는 프로젝트 규칙과 충돌할 수 있다**([DATA_WORKFLOW](../DATA_WORKFLOW.md#영구-저장)). 쓸 때 실패 처리를 따로 둔다.
- 리슨 서버: `USaveGame`은 복제되지 않는다. 호스트 PC에서만 저장·로드하고(S30 "수동 저장은 호스트만"), 클라이언트는 결과를 복제 상태로 받는다. 개인 설정은 각자 로컬 파일.
- 플랫폼 백엔드: `ISaveGameSystem`(동기+`*Async`)로 교체 가능. Steam Cloud 연동 방식은 D-003에서 미정.
- 프로토타입: `UDigetherPrototypeSaveGame`은 동기 `SaveGameToSlot`/`LoadGameFromSlot`을 시험 슬롯에 쓴다(`DigetherPrototypeGameMode.cpp`). 2026-11-04 사용자 결정은 정식 `Game/Save/`에서 **v2를 읽지 않고 v3만 읽고 쓰며 별도 슬롯으로 보존**하는 것이다([코드 구조 계획](../../design/CODE_STRUCTURE_PLAN.md)). 구버전 변환이 미정이라는 과거 설명과 구분한다.

### 직렬화 대안
- `FArchive`/`FMemoryWriter`로 직접 포맷 정의: 버전·압축·체크섬을 전부 직접 책임. USaveGame 위에 얹는 것이 아니라 대체하는 방식.
- JSON(`FJsonSerializer`, `FJsonObjectConverter::UStructToJsonObjectString`): 사람이 읽고 diff 가능, 용량·속도는 불리. 구조체 필드를 이름으로 읽으므로 필드 추가에 관대한 편(후보). 프로젝트는 `Json` 모듈에 이미 의존.
- 어느 쪽이든 Steam Cloud 용량·충돌 해결 정책은 별도 결정.

### Data Validation
- `UObject::IsDataValid(FDataValidationContext&)`(소스 `Object.h`). 검사 클래스 `UEditorValidatorBase`: `CanValidateAsset`/`ValidateLoadedAsset`(5.4에서 구 시그니처 `CanValidate_Implementation`·`ValidateLoadedAsset_Implementation(UObject*, TArray<FText>&)` deprecated). 여러 에셋을 함께 보는 규칙은 Validator 서브클래스로.
- `UDataValidationSettings`: `bValidateOnSave` 기본 true, 변경 목록 검증, `DataValidationCommandlet`(CI용). 엔진 기본 검증기로 `UEditorValidator_Localization`(L10N 폴더 지역화 에셋이 원본과 타입이 맞는지)이 있다(소스).
- 플러그인 `DataValidation`은 Editor 전용이라 `IsDataValid`는 `#if WITH_EDITOR` 안에 둔다(프로젝트 관례).

### Data Registry
- `.uplugin`: `IsBetaVersion true`, `EnabledByDefault false`, 모듈 `DataRegistry`(Runtime, PreDefault)·`DataRegistryEditor`(UncookedOnly), `GameplayTagsEditor` 의존(소스).
- 기능: `FDataRegistryType`/`FDataRegistryId`, `UDataRegistrySubsystem`, `AcquireItem`(비동기)·`GetCachedItem`, 소스 순서(`DataRegistrySource_DataTable`·`_CurveTable` 내장). 구조체 포인터를 오래 보관하지 말 것(문서).
- 후보 조건: 데이터가 여러 표·모드·외부에서 합쳐질 때. 정의 수십 개 규모면 Asset Manager + DataTable로 충분한지 먼저 판단한다.

### Instanced Struct
- `FInstancedStruct`: `Engine/Source/Runtime/CoreUObject/Public/StructUtils/InstancedStruct.h`. 함께: `FInstancedStructContainer`, `TSharedStruct`/`FConstSharedStruct`(`SharedStruct.h`), `FStructView`, `FInstancedPropertyBag`(`PropertyBag.h`). `NetSerialize`가 있어 복제 가능한 프로퍼티로 쓸 수 있다(소스).
- StructUtils 플러그인(`Experimental`, `DeprecatedEngineVersion 5.5`)은 모듈 헤더 외에 소스가 없어 헤더가 코어로 이전된 것으로 읽힌다(추정). 프로젝트는 `uproject`에 플러그인을 켜지 않아도 된다.
- 쓸모 후보: 이벤트·효과 정의에 타입별 파라미터 칸, 한 배열에 서로 다른 구조체. 복제 비용·쿠크된 `UScriptStruct` 참조는 시험해야 한다(미확인).

### Localization
- 구성(문서 localization-overview): 작성 지점에서 텍스트를 수집(Gather)해 manifest/archive를 만들고, 번역(PO)을 컴파일한 LocRes만 런타임이 읽는다. String Table도 같은 방식으로 수집된다. 문화는 IETF 태그, 중국어는 `zh-Hans`/`zh-Hant`.
- String Table: 에디터 에셋 또는 `LOCTABLE_NEW`/`SETSTRING` 매크로·`LOCTABLE_FROMFILE_X` CSV. CSV는 `Key`·`SourceString` 열 필수, 나머지 열은 메타데이터. **CSV 파일 자체는 패키지에 자동 포함되지 않는다**(문서). 참조는 `FText::FromStringTable(Id, Key, Policy)`. 프로젝트 `DigetherSessionText`는 초기화 시점 문제로 `FindOrFullyLoad`를 쓴다.
- Dashboard(문서 localization-tools): 도구 > Localization Dashboard에서 Game 대상 + Gather 경로 + 문화 설정 → Gather/Export/Import/Compile. 생성 INI는 `Config/Localization`. 외부 번역에는 PO를 권장한다. 신규 대상의 로딩 정책을 맞추지 않으면 런타임에 로드되지 않는다.
- 런타임 전환(소스 `KismetInternationalizationLibrary.h`): `SetCurrentCulture`/`SetCurrentLanguage`/`SetCurrentLocale`/`SetCurrentLanguageAndLocale`(두 번째 인자 `SaveToConfig`), `GetLocalizedCultures`, `GetSuitableCulture`, 에셋 그룹별 문화 `SetCurrentAssetGroupCulture`(음성 언어 분리에 쓸 수 있음). 패키징 문화는 Project Settings > Packaging의 `CulturesToStage`(문서).
- 프로젝트: `DefaultGame.ini`가 9개 문화(ko, en, zh-Hans, zh-Hant, ja, it, fr, pt-BR, pt-PT)를 스테이징하고, `DefaultEditor.ini`에 Game 대상이 `Source/DIGETHER/Game/` 코드와 `Content/Frontend|Game|Data` 에셋을 수집하도록 설정돼 있다. 현재 두 ini의 미커밋 diff는 없으며, Gather·Compile 결과는 이번에 실행·검증하지 않았다. 시트 왕복 도구는 `scripts/localization`, 절차는 [시트 번역 안내](../../guides/LOCALIZATION_SHEETS.md). 지원 언어 최종 확정(D20)은 [DECISIONS](../../planning/DECISIONS.md)에 아직 "미정"으로 남아 있어 안내서의 "선택"과 표기가 다르다.
- 폰트 커버리지는 [UI](UI.md#폰트와-현지화-텍스트).

## 보류·비추천 후보와 이유

- **Data Registry**: Beta. 현재 정의 종류·규모에서 Asset Manager로 해결되는지 확인 전에는 도입 이유가 약하다.
- **JsonBlueprintUtilities**: Beta, 블루프린트 JSON 노드. 저장 포맷 기준이 못 된다.
- **StructUtils 플러그인 활성화**: 불필요(코어 헤더 사용). Experimental·deprecated 표시를 켜지 않는다.
- **`ULocalPlayerSaveGame`의 "로드 실패 시 새 객체"**: 손상 파일을 조용히 초기화하는 흐름이 되기 쉽다. 쓰려면 실패 시 중단 정책을 먼저 정한다.
- **게임 규칙 수치를 `UDeveloperSettings`에**: 설정은 환경·경로·기능 스위치용이다([표준 기능](../STANDARD_FEATURES.md#설정과-디버그)).
- **클라이언트가 읽는 DataTable에 `bStripFromClientBuilds`**: 클라이언트에서 표가 사라진다.

## 확인 한계

- 이번에 에디터·빌드를 실행하지 않았다. 모든 항목은 헤더·uplugin·문서 열람 기준이며 프로젝트 동작 검증이 아니다.
- 일반 파일 백엔드와 FileHelper 자체에 임시 파일 교체 절차가 없는 것은 확인했다. 플랫폼·OS까지 포함한 저장 쓰기 원자성, 프로퍼티 추가·삭제 시 `USaveGame` 역호환 범위, `FInstancedStruct` 복제 실측은 확인하지 못했다.
- 이번 재대조에서 Epic 5.8 본문을 연 페이지는 [Asset Management](https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-management-in-unreal-engine), [Gameplay Tags](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-gameplay-tags-in-unreal-engine), [Saving and Loading](https://dev.epicgames.com/documentation/en-us/unreal-engine/saving-and-loading-your-game-in-unreal-engine)다. `application_version=5.8` 쿼리 URL은 도구 접근 오류여서 쿼리 없는 5.8 페이지로 확인했다. 나머지 문서 표시는 초기 조사 참고이며 이번 재열람으로 집계하지 않았다.
- 기존 [UNREAL_FEATURE_COVERAGE](../../design/UNREAL_FEATURE_COVERAGE.md)는 5.8.2 시점(CL 56702186) 조사이고 이번 로컬 설치는 5.8.4(CL 58902052)다. 5.8.2와 사실이 달라진 항목은 발견하지 못했다(대조 범위는 이 영역의 헤더만).
- 한국어 폰트·문화별 에셋(L10N) 동작, Steam Cloud 저장 연동은 조사하지 않았다.
