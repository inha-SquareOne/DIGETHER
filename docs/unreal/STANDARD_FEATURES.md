# 표준 엔진 기능 사용 기준

현업 Unreal 프로젝트에서 널리 쓰는 엔진 기본 기능과 그 사용 규칙이다. 같은 문제를 직접 구현하기 전에 아래 기능을 먼저 쓴다. 쓰지 않으면 이유를 구현 브리프나 ADR에 남긴다. 여기 적은 기능은 엔진 기본·공식 플러그인이지만, 프로젝트에 실제로 켜져 있는지와 빌드·cook 결과는 프로젝트 [프로필](../templates/PROJECT_PROFILE_TEMPLATE.md)과 실제 검사로 따로 확인한다. 프로젝트별 적용 현황은 프로젝트 설계 문서에 적는다.

| 문제 | 표준 기능 | 피할 것 |
|---|---|---|
| 종류·상태·분류 구분 | GameplayTag | 문자열·`FName` 비교, 분류용 bool 묶음 |
| 아이템·장비·적 같은 정의 데이터 | Primary Data Asset + Asset Manager | 코드 상수, 하드코딩 경로 |
| 큰 에셋 참조와 로드 | 소프트 레퍼런스 + 비동기 로드 | 모든 것을 하드 레퍼런스로 연결 |
| 입력 | Enhanced Input | `BindKey`·`BindAxis` 등 구식 입력 바인딩 |
| 전역 서비스 | Subsystem | 싱글턴 액터, 전역 static 상태 |
| 프로젝트 설정·디버그 스위치 | Developer Settings, 콘솔 변수 | 하드코딩 플래그 |
| 데이터 오류 검출 | Data Validation | 런타임에서야 드러나는 오류 |
| 로그 | 기능별 로그 카테고리 | `LogTemp` |
| UI 화면 전환·게임패드 | CommonUI | 위젯마다 다른 입력 처리 |
| AI·흐름 로직과 디버깅 | StateTree/Behavior Tree, Gameplay Debugger, Visual Logger | Tick 안의 긴 if 분기 |

## GameplayTag

- 태그는 `Item.Type.Ore`처럼 점으로 나눈 계층으로 짓는다. 최상위 분류는 프로젝트가 정하고 새 최상위를 임의로 늘리지 않는다.
- 태그 원본은 `Config/DefaultGameplayTags.ini` 또는 `Config/Tags/*.ini`에 둔다. C++에서 쓰는 태그는 헤더의 `UE_DECLARE_GAMEPLAY_TAG_EXTERN`, 구현 파일의 `UE_DEFINE_GAMEPLAY_TAG`로 네이티브 선언해 오타를 컴파일 단계에서 잡는다. 모듈에 `GameplayTags` 의존을 추가한다.
- 여러 태그는 `FGameplayTagContainer`, 조건은 `FGameplayTagQuery`로 표현한다. 하위 태그 포함 여부(`HasTag`)와 정확히 일치(`HasTagExact`)를 구분해 쓴다.
- 이름을 바꿀 때는 태그 리다이렉트를 등록해 기존 에셋 참조를 유지한다.
- 동시에 참일 수 없는 상태는 enum 상태머신을 원본으로 둔다([개발 패턴](ENGINEERING_PATTERNS.md)). 태그는 분류·조회와 조건 표현에 쓴다.
- 복제되는 태그 컨테이너도 서버만 바꾼다([네트워크 권위](NETWORK_AUTHORITY.md)).

## Primary Data Asset과 Asset Manager

- 게임 정의(아이템·장비·적·레벨 구성·튜닝 프로필)는 `UPrimaryDataAsset` 하위 클래스로 만든다. 정의(고정값)와 런타임 인스턴스(위치·소지자·남은 내구도)는 분리한다.
- 정의 타입은 프로젝트 설정의 Asset Manager `PrimaryAssetTypesToScan`에 타입·폴더와 함께 등록하고 cook 규칙을 명시한다. 규칙의 기본값 `Unknown`은 항상 포함(`AlwaysCook`)과 다르다. 등록과 별도로 패키지 빌드에서 실제 로드를 확인한다.
- Blueprint 하위 클래스가 생겨도 타입 이름이 흔들리지 않도록 필요하면 `GetPrimaryAssetId`를 재정의한다.
- 정의는 `FPrimaryAssetId`로 찾고 `UAssetManager`의 비동기 로드로 불러온다. 큰 하위 에셋은 `AssetBundles` 메타데이터로 묶어 필요할 때만 로드한다.
- 수치 표·곡선이 많으면 DataTable/CurveTable을 함께 쓰되 같은 값을 두 곳에 두지 않는다([데이터 작업](DATA_WORKFLOW.md)).
- 감사·크기 확인은 Asset Audit, Reference Viewer, Size Map을 쓴다.

## 소프트 레퍼런스와 비동기 로드

- 항상 필요하지 않은 메시·텍스처·사운드·클래스는 `TSoftObjectPtr`·`TSoftClassPtr`로 참조한다. 하드 레퍼런스는 참조한 에셋 전체를 함께 로드해 메모리와 로딩 시간을 늘린다.
- 로드는 `FStreamableManager`나 Asset Manager의 비동기 요청으로 하고, 완료 시점에 대상 객체가 아직 유효한지 확인한다. 게임 스레드를 멈추는 동기 로드는 초기화·에디터 도구에만 쓴다.

## Enhanced Input

- 입력은 Input Action(동작)과 Input Mapping Context(키 배치)로 정의한다. 키를 코드에 쓰지 않는다.
- 로컬 플레이어의 Enhanced Input 서브시스템에 상황별 Mapping Context를 우선순위와 함께 추가·제거한다(탐사, 관전, UI 등).
- `UEnhancedInputComponent::BindAction`으로 바인딩하고, 길게 누르기·연타 같은 판정은 Trigger·Modifier 에셋으로 표현한다.
- 키 재설정·게임패드 지원은 Enhanced Input의 사용자 설정 기능과 CommonUI 입력 라우팅을 먼저 검토한다.
- 입력은 의도일 뿐이다. 게임 결과는 서버 RPC로 확정한다.

## Subsystem

- 수명에 맞는 Subsystem을 고른다. 엔진 전체는 `UEngineSubsystem`, GameInstance 수명(맵 이동 동안 유지, PIE 인스턴스마다 따로 생성)은 `UGameInstanceSubsystem`, 월드(맵) 단위는 `UWorldSubsystem`, 로컬 플레이어별은 `ULocalPlayerSubsystem`, 에디터 도구는 `UEditorSubsystem`이다. 온라인 세션은 GameInstance보다 먼저 끝날 수 있으므로 세션 종료 시 정리를 따로 구현한다.
- 같은 기능에 C++용과 Blueprint용 요청 함수를 따로 만들지 않는다. 함수 하나를 두고, Blueprint 비동기 노드가 필요하면 그 함수를 감싼다.
- Subsystem 메모리는 복제되지 않는다. 다른 플레이어에게 보여야 하는 값은 복제 액터로 전달한다.

## 설정과 디버그

- 프로젝트 단위 설정은 `UDeveloperSettings` 하위 클래스로 만들어 프로젝트 설정 화면에서 편집한다. 게임 밸런스 값은 설정이 아니라 정의 데이터에 둔다.
- 디버그 표시·치트·강제 실행은 콘솔 변수(`TAutoConsoleVariable`)나 콘솔 명령으로 켜고, 출시 빌드에서 빠지도록 빌드 가드를 둔다([개발 패턴](ENGINEERING_PATTERNS.md#빌드-구성)).
- 로그는 기능별 카테고리를 `DECLARE_LOG_CATEGORY_EXTERN`/`DEFINE_LOG_CATEGORY`로 선언해 쓴다. `LogTemp`는 임시 확인 뒤 지운다.

## 데이터 검증

- 정의 에셋은 `IsDataValid`를 재정의해 범위·필수 참조·중복을 검사한다. 여러 에셋을 함께 보는 규칙은 에디터 검증기(Editor Validator)로 만든다.
- 저장·제출 전 검증 오류가 있으면 고친 뒤 진행한다. 검증 통과가 게임 동작 검증을 대신하지는 않는다([테스트](TESTING.md)).

## UI·AI·디버깅 도구

- UI 화면 쌓기·뒤로 가기·게임패드 포커스는 CommonUI의 Activatable Widget과 스택을 쓴다. UI는 표시만 하고 게임 상태를 직접 바꾸지 않는다.
- AI·흐름 로직은 StateTree 또는 Behavior Tree로 만든다. 어느 것을 쓸지는 프로젝트 결정을 따른다.
- 실행 중 상태 확인은 Gameplay Debugger, 시간축 기록은 Visual Logger, 성능은 Unreal Insights를 쓴다([성능](PERFORMANCE.md)).

## 에셋 이름

에셋은 종류 접두사를 붙인다: `BP_` Blueprint, `WBP_` 위젯, `ABP_` 애니메이션 Blueprint, `AM_` 몽타주, `BS_` 블렌드 스페이스, `SM_` 스태틱 메시, `SK_` 스켈레탈 메시, `T_` 텍스처, `M_` 머티리얼, `MI_` 머티리얼 인스턴스, `NS_` Niagara, `DA_` Data Asset, `DT_` DataTable, `IA_` Input Action, `IMC_` Mapping Context. 기존 에셋 이름 변경은 참조·리다이렉터 처리와 담당자 확인 뒤에 한다([에셋 안전](ASSET_SAFETY.md)).

## 별도 결정이 필요한 것

다음은 현업에서 쓰이지만 도입 비용·구조 영향이 커서 프로젝트 결정(ADR) 뒤에 쓴다.

- Gameplay Ability System(능력·효과·속성 관리)
- Game Features 플러그인 기반 기능 분리
- 복제 최적화 방식(Push Model, Iris)
- 데디케이티드 서버 빌드(소스 빌드 엔진 필요)
