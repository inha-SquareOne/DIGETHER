# 코딩 컨벤션

DIGETHER의 C++·Blueprint·에셋 작성 규칙이다. 기본은 Epic의 [Unreal C++ 코딩 표준](https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine)이고, 아래는 이 프로젝트의 선택과 자주 틀리는 부분이다. 구조·수명 원칙은 [개발 패턴](unreal/ENGINEERING_PATTERNS.md), 네트워크 규칙은 [네트워크 권위](unreal/NETWORK_AUTHORITY.md), 엔진 기능 선택은 [표준 엔진 기능](unreal/STANDARD_FEATURES.md)을 따른다.

## 적용 범위

- **새로 쓰는 코드**는 이 문서를 모두 따른다.
- **기존 프로토타입 코드**(`Source/DIGETHER/Prototype/`)는 고칠 때 손대는 부분부터 맞춘다. 이름 변경·`LogTemp` 정리 같은 일괄 이전은 [코드 구조 계획](design/CODE_STRUCTURE_PLAN.md)의 단계에서 한다.
- **Epic 템플릿 코드**(`DIGETHERCharacter` 등)는 템플릿 형태를 유지하고, 새 기능은 별도 클래스로 만든다.
- **재사용 플러그인**(`DevelopmentWorkbench`)은 프로젝트 이름 대신 플러그인 이름을 접두사·테스트 이름에 쓴다(예: 테스트 `DevelopmentWorkbench.*`). 프로젝트 전용 어댑터(`DigetherWorkbench`)는 이 문서를 따른다.

## 1. 이름

### 타입과 접두사

| 대상 | 접두사 | 예 |
|---|---|---|
| UObject 파생 | `U` | `UDigetherItemDefinition` |
| Actor 파생 | `A` | `ADigetherPrototypeCharacter` |
| UMG 위젯(UUserWidget 파생) | `U` + 이름 끝 `Widget` | `UDigetherDepositWidget` |
| Slate 위젯 | `S` | `SDigetherDeveloperToolsPanel` |
| 구조체·일반 C++ 클래스 | `F` | `FDigetherItemInstance` |
| enum | `E`, `enum class` + `uint8` | `EDigetherLife` |
| 인터페이스 | `U`(UInterface)/`I`(구현용) 쌍 | `UDigetherInteractable` / `IDigetherInteractable` |
| 템플릿 | `T` | `TArray`, `TObjectPtr` |
| bool 변수 | `b` | `bRunFinalized` |

- 새 프로젝트 타입은 접두사 뒤에 `Digether`를 붙인다(`ADigetherRunCoordinator`). 기존 `FPrototypeItem`·`EPrototypeLife`처럼 `Digether`가 없는 프로토타입 타입은 이전 대상이다. 새 정식 코드에는 `DigetherPrototype`를 쓰지 않는다.
- 이름은 PascalCase다. 지역 변수·매개변수도 PascalCase(`DeltaSeconds`)를 쓴다.
- 출력 매개변수는 `Out`을 붙인다(`FString& OutMessage`). bool 출력은 `bOut`으로 시작한다(`bool& bOutSucceeded`).
- 약어는 단어처럼 쓴다(`RunId`, `HudText`). 의미 없는 축약(`Mgr`, `Tmp`)은 쓰지 않는다.
- 파일 이름은 주 클래스 이름에서 접두사를 뺀 것이다(`ADigetherPrototypeGameMode` → `DigetherPrototypeGameMode.h/.cpp`).

### 함수

- 동사로 시작한다(`RequestPurchase`, `CompleteReturn`). bool을 돌려주면 질문 형태로 짓는다(`CanInteract`, `IsHazardTarget`).
- RPC는 실행 위치 접두사를 붙인다: `Server_`(클라→서버), `Client_`(서버→소유 클라), `Multicast_`(서버→모두). 구현부는 `_Implementation`이다. 기존 `ServerInteract` 형식은 [코드 구조 계획](design/CODE_STRUCTURE_PLAN.md) P5에서 바꾼다.
- 복제 알림 함수는 `OnRep_프로퍼티이름`이 기본이고 `UFUNCTION()`을 붙인다. 여러 프로퍼티가 한 콜백을 함께 쓰면 의미 있는 묶음 이름을 쓴다(예: `OnRep_CellState`).
- 델리게이트 타입은 `FOn…`(예: `FOnRunFinished`), 바인딩 함수는 `Handle…`로 짓는다.

### GameplayTag·로그·테스트

- 태그는 점으로 나눈 계층(`Item.Type.Ore`, `Result.Fail.NotEnoughMoney`)이며, C++ 상수는 `TAG_` 접두사(`TAG_Item_Type_Ore`)로 네이티브 선언한다. 선언 매크로는 [표준 엔진 기능](unreal/STANDARD_FEATURES.md#gameplaytag)을 따른다.
- 로그 카테고리는 `LogDigether기능`(예: `LogDigetherExcavation`)이다. 새 코드에 `LogTemp`를 남기지 않는다.
- 자동 테스트 이름은 `DIGETHER.<영역>.<기능>`이다(예: `DIGETHER.Prototype.Settlement`, 새 코드는 `DIGETHER.Game.Economy`).

## 2. 파일과 폴더

- 런타임 코드는 `DIGETHER/Source/DIGETHER/` 아래 기능 폴더에 둔다. 새 정식 코드는 `Game/<도메인>/`(Core·Data·Session·Run·Elevator·Inventory·Economy·Life·Interaction·Save·Input)에 둔다.
- 에디터 전용 코드는 에디터 모듈·플러그인(`DigetherWorkbench` 등)에 두고, 런타임 모듈이 에디터 모듈에 의존하지 않게 한다.
- 헤더는 `#pragma once`로 시작한다. 직접 쓰는 타입의 헤더만 포함하고, 포인터·참조로만 쓰는 타입은 전방 선언한다. `.generated.h`는 헤더의 마지막 include다.
- 구현 파일에 대응하는 헤더가 있으면 그 헤더를 첫 include로 둔다.
- 다른 모듈에서 쓸 공개 클래스·함수에는 모듈 API 매크로를 붙인다(예: `DIGETHER_API`, `WORKBENCHEDITOR_API`).
- 한 파일에 주 클래스 하나를 둔다. 작은 보조 구조체·enum은 같은 헤더에 둘 수 있다.
- 클래스 안의 순서는 `public` → `protected` → `private`로 쓴다.

## 3. 서식

- 들여쓰기는 탭이다.
- 여는 중괄호는 다음 줄에 둔다(한 줄짜리 접근자는 예외). `if`·`for`·`while` 본문이 한 줄이어도 중괄호를 쓴다.
- 포인터·참조 기호는 타입에 붙인다(`AActor* Target`, `const FString& Name`).
- 줄 끝(CRLF/LF)은 기존 파일을 따르고, 편집기가 파일 전체의 줄 끝을 바꾸지 않게 한다.
- 위 중괄호 규칙은 앞으로 쓸 코드의 기준이다. 기존 프로토타입에는 같은 줄 중괄호와 중괄호 없는 단문 조건이 남아 있으며, 고치는 부분부터 맞춘다. 파일 전체 서식 정리는 별도 커밋으로 한다.
- 프로젝트 포맷 설정 파일(.clang-format)은 아직 없다. 도입하면 이 절을 그 파일로 대체한다.

## 4. C++ 작성 규칙

### 참조와 타입

- UObject 멤버 참조는 `UPROPERTY()`가 붙은 `TObjectPtr<>`로 둔다. 소유하지 않는 참조는 `TWeakObjectPtr<>`, 늦게 로드할 에셋은 `TSoftObjectPtr<>`/`TSoftClassPtr<>`를 쓴다. UObject에 `std::unique_ptr`·`shared_ptr`을 쓰지 않는다.
- 컨테이너·문자열은 엔진 타입(`TArray`, `TMap`, `TSet`, `FString`, `FName`, `FText`)을 쓴다. 화면에 보이는 문구는 `FText`(번역 대상), 내부 식별은 `FName`·GameplayTag로 한다.
- 단위를 이름에 드러낸다(`WeightKg`, `RunSeconds`). 밸런스 수치를 코드 상수로 두지 않는다([데이터 작업](unreal/DATA_WORKFLOW.md)). 그 밖의 의미 있는 숫자도 이름 있는 상수로 둔다.
- 동시에 참일 수 없는 상태는 bool 여러 개 대신 `enum class` 상태머신으로 표현한다.
- 바꾸지 않는 매개변수·멤버 함수·지역 값에는 `const`를 붙인다.
- `auto`는 람다, 길고 뻔한 반복자 타입, 타입을 적기 어려운 템플릿 코드에만 쓴다(Epic 기준).
- 복사하면 안 되는 일반 C++ 클래스(리소스 소유 등)는 복사 생성자·대입을 `= delete`로 막는다.

### 오류와 검사

- 예외(`throw`)·RTTI(`dynamic_cast`)를 쓰지 않는다. UObject 변환은 `Cast<>`를 쓴다.
- **예상 가능한 실패**(잔고 부족, 대상 없음 등)는 분기하고 결과 코드·반환값으로 알린다.
- **절대 깨지면 안 되는 조건**은 `check`로 확인한다. `check`는 출시 빌드에서 실행되지 않으므로 안에 부작용(함수 호출로 상태 변경 등)을 넣지 않는다.
- **복구할 수 있는 비정상**은 `ensure`로 기록하고, 이어서 안전한 분기로 처리한다.
- 일반 포인터는 `nullptr` 비교, UObject는 `IsValid()`로 유효성을 확인한다(파괴 대기 객체까지 걸러낸다).

### 수명과 빌드 구성

- 타이머·델리게이트·비동기 콜백은 `EndPlay`·소멸 시 해제하고, 콜백 안에서 대상 객체가 아직 유효한지 확인한다.
- 디버그·치트 코드는 `#if !UE_BUILD_SHIPPING`으로 감싸고, 출시 빌드에서 빠져도 게임 상태가 바뀌지 않게 한다.
- 에디터에서만 실행하는 코드는 `#if WITH_EDITOR`로 감싼다. 런타임 클래스의 에디터 전용 **데이터**(UPROPERTY 포함)는 `#if WITH_EDITORONLY_DATA`로 감싼다. UPROPERTY를 `WITH_EDITOR`로 감싸면 UHT 오류가 난다.
- 상세 점검 항목은 [unreal-cpp-guidelines 스킬](../.agents/skills/unreal-cpp-guidelines/SKILL.md)을 따른다.

## 5. 네트워크 코드

- 서버 권위 상태는 서버에서만 바꾼다. Actor는 `HasAuthority()`, ActorComponent는 소유 Actor의 권한(`GetOwner()->HasAuthority()`)으로 확인한다. Subsystem은 호출하는 쪽(서버 RPC·GameMode 등)이 권한을 확인한 경로로만 부른다. 로컬 UI·카메라 같은 표현은 이 검사 대상이 아니다.
- 지속 상태는 복제 프로퍼티로, 일회성 사건만 RPC로 보낸다. 상태를 바꾸는 Server RPC는 `Reliable`이 기본이고, 연출 RPC는 `Unreliable`이 기본이다. 매 프레임 보내는 입력은 Reliable로 보내지 않는다.
- 개인 정보는 `COND_OwnerOnly` 등 조건 복제로 보내는 대상부터 제한한다.
- 상세 규칙은 [개발반 구현 설계의 구현 규칙](design/DEVELOPMENT_SYSTEM_DESIGN.md#구현-규칙)을 따른다.

## 6. Blueprint와 프로퍼티 노출

- Blueprint는 C++ 클래스를 상속해 값·에셋 연결·연출을 맡는다. 게임 규칙 판정과 복제 상태 변경은 C++에 둔다.
- 함수 노출: 동작은 `BlueprintCallable`, 조회는 `BlueprintPure`.
- 프로퍼티는 용도별로 지정자를 고른다.

| 용도 | 지정자 예 |
|---|---|
| 기획·아트가 고치는 클래스 기본값(튜닝·에셋 연결) | `EditDefaultsOnly, BlueprintReadOnly, Category="..."` |
| 레벨에 배치한 개체마다 다른 값 | `EditAnywhere, BlueprintReadOnly, Category="..."` |
| 런타임 상태(복제 포함) | `Replicated`/`ReplicatedUsing` + `BlueprintReadOnly`, 편집 지정자 없음 |
| 컴포넌트 참조 | `VisibleAnywhere, BlueprintReadOnly` |

- `Category`는 `"Digether|기능"` 형식으로 붙여 디테일 창을 정리한다.
- BP가 C++ 상태를 직접 쓰게(`BlueprintReadWrite`) 하는 것은 이유가 있을 때만 한다.
- private 멤버를 BP에 보이려면 getter 함수를 열거나, 의도적으로 `meta=(AllowPrivateAccess="true")`를 붙인다. 이것 없이 private 멤버에 `BlueprintReadOnly`를 붙이면 UHT 오류가 난다.
- 이벤트 그래프가 길어지면 함수·매크로로 나누고, 같은 로직을 여러 BP에 복사하지 않는다.
- C++ 선언(UPROPERTY·UFUNCTION·RPC)을 바꾸는 빌드는 에디터를 닫고 한다. 이름을 바꾸면 `DefaultEngine.ini`에 CoreRedirects를 추가한다.

## 7. 에셋 이름

에셋 종류 접두사는 [표준 엔진 기능의 에셋 이름](unreal/STANDARD_FEATURES.md#에셋-이름)을 따른다(`BP_`, `WBP_`, `SM_`, `T_`, `M_`, `MI_`, `DA_`, `IA_`, `IMC_` 등). 접두사 뒤는 PascalCase로 쓴다(`BP_PrototypeCharacter`, `DA_Item_Ore`). 이름 변경과 이동은 [에셋 안전](unreal/ASSET_SAFETY.md)의 담당·잠금 확인을 거친다.

## 8. 주석

- 코드가 무엇을 하는지가 아니라 **왜** 그렇게 하는지, 지켜야 할 제약을 쓴다. 주석은 한국어로 써도 되고, 식별자는 영어로 쓴다.
- 임시값·시험값은 그렇다고 밝힌다(예: "테스트 픽스처이며 최종 기획 값이 아니다").
- 작성 기준은 [write-comments 스킬](../.agents/skills/write-comments/SKILL.md)을 따른다.

## 9. 검사

코드 변경은 [검증 계획](verification/VERIFICATION_PLAN.md)의 수준에 맞춰 빌드·자동 테스트·필요한 다인 검사를 한다. 헤더·모듈 의존을 바꾸면 non-unity 빌드를 함께 확인한다.
