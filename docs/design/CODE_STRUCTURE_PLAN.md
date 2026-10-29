# 코드 구조 평가와 표준 기능 적용 계획

기준일: 2026-10-29 · **계획, 미구현** · 대상: `prototype` 브랜치 `DIGETHER/Source/DIGETHER/Prototype/`

이 문서는 [전체 시스템 설계](../SYSTEM_DESIGN.md)와 [개발반 구현 설계](DEVELOPMENT_SYSTEM_DESIGN.md)를 코드로 옮길 때의 물리 구조와 단계별 적용 순서다. 공통 기능 규칙은 [표준 엔진 기능](../unreal/STANDARD_FEATURES.md)을 따른다. 아래 클래스 이름은 후보이며 구현·빌드·검증 전이다.

## 1. 평가 결론

**설계의 도메인별 소유와 정의 데이터 분리는 지금 코드보다 유지보수·확장에 유리하다. 다만 컴포넌트로 나누기만 해서는 그 효과가 나지 않는다. 상태 인계, 저장 변환, 도메인 간 의존 방향을 먼저 정해야 한다.** 이 문서는 그 빈칸을 채운 점진적 개선 계획이다.

### 설계가 잘 맞는 점

- 도메인별 단일 소유자(세션·런·지형·생존·아이템·엘리베이터·경제·저장)는 기능 하나를 고칠 때 볼 범위를 줄인다.
- 정의 데이터와 런타임 인스턴스를 나누면 새 아이템·몬스터·튜닝을 코드 수정 없이 추가할 수 있다.
- 서버 권위와 소유자 전용 뷰의 경계가 명확해 기능이 늘어도 정보 노출 규칙이 흔들리지 않는다.
- 공통 상호작용 인터페이스와 순수 계산 함수(정산 산식, 기존 `DIGETHER.Prototype.Settlement` 테스트)는 확장 지점과 테스트 지점이 된다.

### 현재 코드의 한계 (2026-10-29 확인)

| 한계 | 근거 | 영향 |
|---|---|---|
| GameMode에 도메인 집중 | `DigetherPrototypeGameMode.cpp` 1,931줄, `Request*` 함수 22개, private 상태 필드 48개(엘리베이터 운송·예치·구조·재접속 레지스트리·저장) | 기능 추가마다 같은 파일 충돌, 도메인별 격리 검사가 어려움 |
| 진행 상태가 bool 묶음 | `bHoldElevator`·`bHoldEmergency`·`bTimeoutPending`·`bEmptyReturn`·`bTransitRollbackPending` 등 | 불가능한 상태 조합이 생길 수 있음([개발 패턴](../unreal/ENGINEERING_PATTERNS.md)) |
| 결과 메시지가 `FString` | GameMode에 `OutMessage =` 69곳, `ClientPrototypeMessage(FString)` | 번역 불가, 결과 종류를 코드로 판별 불가 |
| 아이템이 bool 필드·정의 ID 없음 | `FPrototypeItem`의 `bHidden`·`bTwoHanded`·`bEquipment`·`bEmergencyDevice` | 새 아이템 종류마다 구조체·저장·UI 수정 |
| 장치 기능이 enum 기반 | Station이 `EPrototypeStationKind` 6종으로 동작을 고르고(`DigetherPrototypeWorld.cpp`), 종류별 처리 함수는 GameMode에 있음 | 새 장치마다 Station과 GameMode 수정 |
| 튜닝 스냅샷·버전 없음 | GameMode `EditDefaultsOnly` 9개, `DigetherPrototypeRules.h` 산식 고정 | 런 고정값·버전 기록(R01·T65) 불가 |
| 구식 입력 | Character의 `BindKey`·`BindAxisKey` | 키 재설정·게임패드·상황별 입력 전환 어려움 |
| 경계 ID가 클라이언트 요청에 없음 | `PeriodGeneration`은 서버 전용. 구매는 `ServerInteract` 경로로 기간 ID 없이 들어옴. `HoldRunId`는 서버 수신 시점 기록 | 기간·런 전환 직후 도착한 이전 요청을 구별하지 못함(납부는 세션 ID·기간 세대 검증이 이미 있음) |

## 2. 목표 구조

### 폴더와 이름

`Source/DIGETHER/Game/` 아래에 도메인별 폴더를 두고 새 코드는 `Digether` 접두사를 쓴다. 런타임 모듈은 지금 규모에서 하나로 유지하고, 의존 방향과 공개 API로 경계를 지킨다. 기존 `Prototype/` 클래스는 단계마다 새 구성 요소를 호출하는 얇은 연결층이 되었다가 마지막에 정리한다.

```text
Game/
  Core/        게임플레이 태그, 결과 코드, 로그 카테고리, 공용 값 타입
  Data/        정의 자산(아이템·장비·튜닝 프로필), Asset Manager 연동
  Session/     참가·재접속 레지스트리
  Run/         기간·런·일차 흐름(조율자)
  Elevator/    운행 상태머신과 편별 회수
  Inventory/   아이템 인스턴스와 슬롯
  Economy/     지갑·구매·납부·정산(순수 계산 함수 포함)
  Life/        체력·매몰·사망·구조
  Interaction/ 상호작용 인터페이스와 장치 컴포넌트
  Save/        저장·불러오기·스키마 버전·변환
  Input/       Enhanced Input 연결
  Terrain/     굴착 밀도 지형·메시·복제(현재 `Prototype/World/DigetherExcavation*`)
  Online/      세션 생성·검색·참가(현재 `Prototype/Online/`)
  UI/          HUD·위젯(문구는 String Table)
```

현재 프로토타입 기능의 이전 위치: 시체(`Corpse`)는 `Life/`, 포탈·광산 장면(`Portal`·`MineScene`)은 `Interaction/`과 월드 배치, 달리기·헤드램프 요청은 Character와 `Input/`, 납부 입력 위젯은 `UI/`로 옮긴다.

### 배치

| 도메인 | 배치 | 수명·주의 |
|---|---|---|
| 세션 레지스트리 | GameMode에 붙는 서버 전용 Actor Component | GameMode는 월드 수명이다. 이 계획은 거점과 탐사가 **같은 월드**(현재 `L_Prototype` 하나)인 구조만 다룬다. 연결이 끊긴 플레이어의 레코드는 끊긴 시점 스냅샷이 아니라, 그동안의 피해·사망·탈출·정산을 계속 반영한 최신 상태로 유지한다(현재 코드와 같은 책임) |
| 런·기간 조율자 | GameMode에 붙는 서버 전용 Actor Component | 규칙 판정·타이머·종료·정산 순서를 맡는다. GameMode가 규칙을 판정한다는 [구현 규칙](DEVELOPMENT_SYSTEM_DESIGN.md#구현-규칙)과 같은 배치다 |
| 런·기간 공개 상태 | GameState에 붙는 Actor Component | 조율자가 확정한 단계·일차·목표·남은 시간만 복제해 보여 준다. 판정 로직을 두지 않는다 |
| 엘리베이터 | 엘리베이터 액터의 Actor Component, enum 상태머신 | 공개 진행 상태·무게 단계만 복제 |
| 인벤토리·지갑·생존 | PlayerState에 붙는 Actor Component | Pawn 재생성에는 유지된다. PlayerState 교체(재접속·travel)에는 아래 인계 계약을 따른다 |
| 경제·슬롯·상태 전이 규칙 | 순수 C++ 함수·값 타입(엔진 월드 의존 없음) | 자동 테스트로 고정 |
| 저장 | `UGameInstanceSubsystem` | 호스트에서만 저장·불러오기. 세션 종료·월드 해제 때 월드 객체 참조를 정리 |
| 상호작용 장치 | 공통 인터페이스 + 기능별 Component(상점·예치·출발·긴급 탈출) | 새 장치는 Component 추가로 만든다 |
| GameMode | 로그인·스폰·구성 요소 연결 | 규칙 판정은 각 도메인에 위임 |

### 복제 계약

- 복제할 컴포넌트는 생성자에서 `SetIsReplicatedByDefault(true)`를 켜고, 소유 액터도 복제 액터여야 한다. 컴포넌트는 기본적으로 복제되지 않는다.
- 같은 컴포넌트 안에서 공개 값과 개인 값을 나눈다. 예: 생존 컴포넌트의 생존 상태·매몰 단계는 모두에게(팀원·관전자 표시용), 체력 상세·지갑·인벤토리 상세는 `COND_OwnerOnly`. 컴포넌트 전체를 소유자 전용으로 만들지 않는다. 현재 `DigetherPrototypePlayerState`의 공개/개인 구분을 그대로 옮긴다.
- 복제 값은 서버 함수만 바꾼다. 클라이언트는 OnRep로 표현만 갱신한다([네트워크 권위](../unreal/NETWORK_AUTHORITY.md)).

### 상태 인계 계약

- PlayerState 컴포넌트마다 값 구조체로 상태를 꺼내고 넣는 함수 쌍(`CaptureState`/`RestoreState`)을 둔다. 엔진 기본 `APlayerState::CopyProperties`는 사용자 컴포넌트 상태를 복사하지 않는다.
- 재접속: 세션 레지스트리가 연결 해제 시 각 컴포넌트의 상태를 받아 보관하고, 같은 ID가 돌아오면 새 PlayerState의 컴포넌트에 복원한다(현재 GameMode의 보관·복원 경로를 이 계약으로 옮긴다).
- 연결이 끊긴 동안의 변경: 사망 손실·정산 반영 같은 도메인 규칙은 컴포넌트가 아니라 값 구조체를 받는 순수 함수로 둔다. 조율자는 플레이어가 접속 중이면 컴포넌트의 값에, 끊긴 상태면 세션 레지스트리가 보관한 값에 같은 함수를 적용한다. 그래서 보관 값은 끊긴 시점 스냅샷이 아니라 최신 상태로 유지되고, 재접속 때 그대로 복원한다. 보관 값에 대한 명령도 아래 검사 → 변경 → 알림 순서를 따른다.
- **월드 이동은 이 계획 범위 밖이다.** 거점·탐사를 다른 맵으로 나누기로 하면, 먼저 게임·기간·런 상태, 회수 건, 정산 완료 기록, 세션 레지스트리, PlayerState 컴포넌트 상태 전체를 값 스냅샷으로 서버 GameInstanceSubsystem에 넘기는 설계를 따로 만든다([전체 설계의 인계 계약](../SYSTEM_DESIGN.md#세션게임런월드-인계)). 엔진 기본 보존 범위는 seamless travel 여부에 따라 다르므로, 두 경로별 복원과 `CopyProperties` 재정의를 그 설계에서 정하고 검증한다. 이전 월드의 Pawn·Controller 포인터는 넘기지 않는다.
- 초기화 범위는 사건별로 따로 정한다. 지갑 컴포넌트 자체는 항상 유지되며, 아래 "잔고"는 그 안의 값이다. 수치는 [전체 설계](../SYSTEM_DESIGN.md)와 D25 시험값을 따른다.

| 사건 | 잔고·기록 | 소지품 | 생존 상태 |
|---|---|---|---|
| 사망 | 잔고 손실 적용(현재 시험값 100%), 사망 기록·사망 직전 잔고 보존 | 드롭(시체·월드로) | 사망·관전 |
| 일반 탈출 | 유지 | 장비·회수품 보존(편별 회수 건으로) | 탈출·관전 |
| 긴급 탈출 | 유지 | 긴급 장치만 소비, 회수품·시체 없이 | 탈출·관전 |
| 거점 귀환·부활 | 정산·복구액 반영 | 장비 유지, 런 전용 상태 초기화 | 활동으로 초기화 |
| 새 게임 | 이전 결과는 저장에 고정, 새 값으로 시작 | 시작 지급만 | 초기화 |

### 의존 방향과 확정 순서

- 명령은 위에서 아래로만 간다. 런 조율자(GameMode 컴포넌트)가 엘리베이터·경제·인벤토리·생존에 명령하고 확정 결과를 GameState 공개 상태에 쓴다. 하위 도메인은 서로를 직접 호출하지 않는다.
- 결과 통지는 아래에서 위로 간다. 하위 도메인은 확정된 결과를 타입 있는 델리게이트로 알린다. 공용 데이터는 `Core`의 값 타입만 공유한다.
- 여러 도메인을 바꾸는 서버 처리(구매, 정산 등)는 조율자의 한 함수에서 **모든 검사 → 모든 변경 → 알림** 순서로 실행한다. 검사가 하나라도 실패하면 아무것도 바꾸지 않는다. 규칙상 이어지는 연쇄 결과(예: 사망 → 소지품 드롭 → 잔고 손실)는 알림을 받은 쪽이 아니라 조율자가 같은 함수 안에서 이어서 명령해, 중간 상태가 복제·저장되지 않게 한다([전체 설계](../SYSTEM_DESIGN.md)의 같은 실행 안 반영 기준). 알림을 받은 쪽(공개 상태·UI·로그)은 표현만 갱신하고 다른 도메인 상태를 바꾸지 않는다.
- 경제 산식, 슬롯 판정, 엘리베이터 상태 전이, 저장 변환은 월드 없이 검사할 수 있는 순수 함수로 둔다.

## 3. 단계별 적용 계획

단계 이름 P0~P7은 이 계획의 구조 이전 단계이며, 전체 설계의 기능 우선순위 표기(S34·S35의 P2)와 다른 구분이다. 기능 개발 순서 ①~⑦과는 병행하되, 같은 도메인의 새 기능은 그 도메인의 추출(P4) 뒤에 얹는다.

각 단계는 따로 빌드·검증할 수 있게 나눈다. 동작을 바꾸지 않는 이전을 먼저 하고, 새 기능은 이전 뒤에 얹는다. 단계 사이에는 기존 구조로 값을 복사하는 연결 함수를 남겨 이전 단계와 함께 동작하게 한다.

| 단계 | 내용 | 바뀌는 파일·에셋 | 완료 조건 |
|---|---|---|---|
| P0 기반 | `GameplayTags` 모듈 의존, 네이티브 태그(최상위는 [태그 기준](DEVELOPMENT_SYSTEM_DESIGN.md#정의-데이터와-태그)의 `Item.*`·`Interact.*`·`Terrain.*`·`Player.State.*`·`Result.*`), 결과 코드(`Result.Fail.*` 태그 + `FText`), `LogTemp` 정리 | Build.cs, `Game/Core/`, `Config/Tags/` | 기존 동작 동일, 빌드·기존 테스트 통과 |
| P1 정의 데이터 | `UDigetherItemDefinition`(값·무게·특성 태그·소프트 메시), `UDigetherTuningProfile`(런 시작 시 고정할 값: 제한 시간·용량·운행 횟수·사망/복구 비율 등). Asset Manager 타입 등록(`AlwaysCook`), `IsDataValid`. 픽업(`DigetherPrototypeWorld.cpp`)이 정의를 읽어 기존 아이템 구조로 복사, GameMode 런 초기화가 프로필 스냅샷을 사용, Rules 산식이 비율을 인자로 받음. 기존 BP·맵의 GameMode 기본값과 픽업 편집값을 DA로 옮김. 프로필에 정의 버전 필드를 둔다. 튜닝 원본은 DA로 시작하고, 텍스트 원본(CSV/JSON, R14) 비교는 P7에서 한다 | `Game/Data/`, `DefaultGame.ini`, World·GameMode·Rules, 새 `DA_` 에셋, BP·맵 | 옮긴 값이 이전 값과 일치, 런 시작 시 버전과 함께 스냅샷, 패키지에서 정의 로드 |
| P2 아이템 인스턴스·저장 변환 | 인스턴스 = `InstanceId`(기존 `Id` 보존) + `DefinitionId` + 슬롯 상태. 히든 여부는 정의 특성이 아니라 **슬롯 상태**로 둔다. 저장 스키마 v3, 구버전 구조체를 읽기용으로 유지. **v2 읽기 → 검증 → 명시적 변환 → 전체 반영** 순서. 정의를 찾을 수 없는 항목은 레거시 정의로 매핑하고 경고, 미래 버전·손상 파일은 덮어쓰지 않고 오류. 변환 전 원본 보존 | Types, SaveGame, Save, 인벤토리·HUD, 아이템 생성 경로(픽업·시작 도구·구매·긴급 장치·사망 드롭) | v2 불러오기 후 값 일치, v3 재저장 → 프로세스 재시작 → 재로드 후에도 일치, 모든 생성 경로가 `DefinitionId`를 채움, 손상·미래 버전 거부, 원본 보존. 위험도 중상 |
| P3 Enhanced Input | `IA_`·`IMC_` 에셋(탐사·거점·관전·UI), Character 바인딩 교체 | `Game/Input/`, Character, 새 입력 에셋 | 실제 입력 경로로 모든 조작 동작, UI 진입·종료, 누르던 행동 취소, 관전·재빙의 뒤 Context 복원 |
| P4 GameMode 분해 | 아래 추출 순서와 계약 | `Game/*`, GameMode 축소 | 추출마다 1인·4인 회귀, 상태 보존, 비공개 데이터 비전달 확인 |
| P5 요청 경로 | RPC `Server_`·`Client_` 접두사. 구매·납부·해금·출발·**회수·승강** 요청에 클라이언트가 본 `PeriodId`/`RunId`를 담고 서버가 현재 값과 비교. 불러오기 후 클라이언트 ID 갱신. 서버 빈도 제한, 거절은 결과 코드로. 납부 입력 세션(서버 발급 세션 ID·납부대·기간 세대, [미로·폐교 흐름](../prototype/MAZE_SCHOOL_FLOW.md))은 화면 입력의 수명 관리로 유지하며 일반 요청의 준비/확정 단계로 보지 않는다 | PlayerController, 관련 Component | 이전 런·기간의 늦은 최초 요청 거절, 진행 중 행동의 늦은 완료 거절, 연타 차단 |
| P6 장치·문구 | `EPrototypeStationKind` 분기를 장치 Component로, 화면 문구를 String Table로, 새 위젯은 CommonUI 기준([표준 엔진 기능](../unreal/STANDARD_FEATURES.md))으로 만들고 `CommonUI`·`CommonInput` 모듈 의존 추가 | World, HUD, Build.cs, 새 String Table | 새 장치를 GameMode·Station 수정 없이 추가 |
| P7 튜닝 레지스트리(후속) | 파라미터 레지스트리(단위·범위·런타임 변경 여부·버전, R09~R10), 호스트 런타임 변경과 클라이언트 동기화(R04) | 별도 설계 | 이 계획 범위 밖. P1은 정의 분리와 런 고정값까지만 다룬다 |

### P4 추출 순서와 연결 계약

| 순서 | 추출 | 입력 | 출력·통지 | 남겨 둘 연결 |
|---|---|---|---|---|
| 1 | 저장(Subsystem) | GameMode가 모은 저장 데이터 | 저장·불러오기 결과 코드 | `RequestSave`/`RequestLoad`가 Subsystem 호출 |
| 2 | 세션 레지스트리 | 로그인·로그아웃, PlayerState | 재접속 복원 완료 | `PostLogin`/`Logout`이 컴포넌트 호출 |
| 3 | 경제(지갑·구매·납부) | 조율자 명령, 정의·튜닝 | 잔고 변경 통지 | `RequestPurchase`/`RequestDeposit` 위임 |
| 4 | 인벤토리 | 줍기·버리기 명령 | 슬롯 변경 통지 | `RequestPickupItem`/`RequestDrop` 위임 |
| 5 | 생존·구조 | 피해·매몰·구조 명령 | 사망·탈출 확정 통지 | `RequestDamage`/`RequestRescue` 등 위임 |
| 6 | 엘리베이터 | 출발·운행 명령 | 편 성공·취소 통지 | `RequestElevator`·`UpdateElevator` 위임, bool 상태를 enum으로 |
| 7 | 런 조율자 | 위 통지 | 종료·정산·일차 전환 | GameMode 본체의 `Tick`·`FinishRun`을 조율자 컴포넌트로 이전 |

## 4. 위험과 대응

- **이름 변경과 데이터 이전은 다른 일이다.** 같은 타입의 이름 변경은 `DefaultEngine.ini` CoreRedirects로 처리한다. 타입 변환(bool→태그)과 소유 객체 이동(GameMode 속성→DA, 컴포넌트)은 CoreRedirects로 안 된다. 기존 BP 기본값·맵 인스턴스 값을 먼저 수집하고 새 위치로 옮긴 뒤 값을 비교한다. 완료 조건은 BP 컴파일, 맵 로드, 재저장, cook이다. 바이너리 안의 실제 참조·기본값은 아직 확인하지 않았다.
- **RPC 이름 변경**: 현재 Controller RPC는 `BlueprintCallable`이 아니어서 BP 직접 호출 가능성은 낮지만, 변경 전 BP 참조를 확인한다. 에디터를 닫고 빌드한다.
- **바이너리 에셋 충돌**: `.uasset`·`.umap` 수정과 새 `DA_`·`IA_` 에셋 생성은 담당자·잠금을 확인한다([에셋 안전](../unreal/ASSET_SAFETY.md)).
- **저장 호환**: P2는 실제 사용자 세이브 대신 임시 경로로 시험한다([데이터 작업](../unreal/DATA_WORKFLOW.md)).
- **동작 회귀**: 각 단계 전후 같은 검사를 돌려 비교한다.
- **범위 확장**: Gameplay Ability System·Game Features·Push Model/Iris는 넣지 않는다(별도 결정).

## 5. 단계별 검증

- **빌드**: Editor·Game Development 타깃. 헤더 의존을 바꾼 단계는 non-unity 빌드.
- **자동 테스트**: 등록 이름 `DIGETHER.Prototype.Settlement`·`DIGETHER.Prototype.Excavation.Field`·`DIGETHER.Prototype.DepositInput`과 단계별 새 테스트(`DIGETHER.Game.*`: 태그·정의 검증, 경제 산식, 슬롯 판정, 엘리베이터 상태 전이, 저장 변환). 현재 `scripts/Invoke-UnrealChecks.ps1`은 `Workbench.` 테스트만 고르므로 `DIGETHER.` 필터를 추가하거나 직접 Automation 명령으로 실행한다. 실행기를 고칠 때는 실행 필터와 기대 테스트 수집 범위를 함께 바꾼다. 단계마다 기대 테스트 개수를 기록하고 **0개 실행은 실패**로 본다.
- **데이터 검증**: DataValidation 대상을 정의 에셋(`DA_`)으로 지정해 실행한다.
- **실행**: `DigetherPrototypeProbe` 1인 시나리오와 별도 프로세스 4인 M2(경합·재접속·비공개 데이터 비전달). Probe는 입력을 우회하므로 P3은 실제 입력 경로 검사를 따로 한다. 패키지에서 정의 로드를 확인한다.
- 각 단계 결과와 미검증 범위를 작업 기록에 남긴다. 이 문서 작성은 구현·검증 완료가 아니다.
