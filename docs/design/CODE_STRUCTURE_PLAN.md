# 코드 구조 평가와 표준 기능 적용 계획

기준일: 2026-10-29 · **계획** · 대상: `DIGETHER/Source/DIGETHER/Game/`(평가 기준은 `Prototype/`) · 현황 서술은 2026-11-10 코드 대조로 정정

**2026-11-04 결정(사용자): `Prototype/`은 더 이상 수정하지 않는다.** 새 정식 코드는 `Game/` 아래에 프로토타입과 독립적으로 동작하도록 새로 작성한다. 프로토타입은 동작 참고와 회귀 비교 기준으로만 보존하고 새 기능을 받지 않는다. 이에 따라 아래 P2~P6에서 프로토타입 파일을 고치는 부분은 `Game/` 쪽 새 구현으로 바꿔 읽는다(3장 참고). 정리·삭제 시점은 별도로 정한다.

진행 상태(2026-11-10 코드 대조): **P0·P1은 적용·커밋됐다**(2026-11-03). 네이티브 태그 4개(`Game/Core/DigetherGameplayTags`), 정의 자산 `UDigetherItemDefinition`·`UDigetherTuningProfile`(`Game/Data/`), DA `DA_Tuning_Prototype`·`DA_Item_PrototypeCargo`, 런 시작 스냅샷, Asset Manager 등록이 들어 있다. 그 뒤 `Game/`에 진입점(`Framework/`)·저장·경제·생존·엘리베이터·런 조율·거점·세션·음성·몬스터 체력의 코드와 자동 테스트가 생겼다. 도메인별 구현·검증 범위는 [구현 현황](IMPLEMENTATION_STATUS.md)에 둔다. 아래 계획과 실제 구현이 갈린 곳은 다음과 같으며, 어느 쪽으로 맞출지는 결정 대기다.

- 규칙 조율: 계획은 GameMode가 연결만 맡는 구조인데 실제로는 `ADigetherGameplayGameMode`가 런 흐름·저장·붓기·게임 오버·접속 판정을 구현 파일을 나눠 직접 맡는다([실행 진입점](#실행-진입점)).
- 세션 레지스트리·재접속: 별도 컴포넌트가 없고 엔진 비활성 PlayerState와 저장 명단 레코드의 두 경로다([상태 인계 계약](#상태-인계-계약)).
- 결과 코드: `Result.*` 태그 대신 도메인별 enum(`EDigetherEconomyResult`·`EDigetherRunResult`·`EDigetherSaveResult` 등)을 쓴다. 태그는 여전히 `Item.*` 4개이고 `Config/Tags/`는 없다.
- 아이템 인스턴스(P2)·공통 상호작용 인터페이스(P6): 미구현이다. 거점 소지품은 `Game/Hub/`의 `FName ItemId` + 수량이며 `UDigetherItemDefinition`과 연결돼 있지 않다.
- 요청 경로(P5): `Server_`·`Client_` 접두사는 적용됐다. `PeriodId`/`RunId`를 담는 요청은 없고 거점 시설 요청만 `FGuid RequestId`를 쓴다.

이 문서는 [전체 시스템 설계](../SYSTEM_DESIGN.md)와 [개발반 구현 설계](DEVELOPMENT_SYSTEM_DESIGN.md)를 코드로 옮길 때의 물리 구조와 단계별 적용 순서다. 공통 기능 규칙은 [표준 엔진 기능](../unreal/STANDARD_FEATURES.md)을 따른다. 아래 클래스 이름 가운데 구현 표시가 없는 것은 후보다.

2026-11-06 기획 적용 경계: [답변 05·15](../planning/PLANNING_RESPONSE_2026-11-06.md)에 따라 시설 거래별 자동 저장은 제외하고 공식 4시점(탐사 정상 종료·목표 달성·호스트 정상 거점 종료·5일차 실패 귀환)에서 진행을 보존한다. 거점 바닥 물품은 종류·수량·위치·방향과 불러오기 시 안전 위치 복구를 다뤄야 하며, 기존 바닥 금화 저장으로 전체 물품 저장이 구현됐다고 보지 않는다. 제단 헌납은 채택 범위에서 제외하고 시험 목적지 교체는 정식 갱도 목록을 받은 뒤 연결한다. 답변의 팀장 결정은 팀 승인 전이며 아래 과거 단계 상태·검증 기록을 새 완료 증거로 바꾸지 않는다.

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

`Source/DIGETHER/Game/` 아래에 도메인별 폴더를 두고 새 코드는 `Digether` 접두사를 쓴다. 런타임 모듈은 지금 규모에서 하나로 유지하고, 의존 방향과 공개 API로 경계를 지킨다. 새 정식 런타임 코드는 `Prototype/` 클래스를 포함하거나 호출하지 않으며(예외: P1 브리지를 검사하는 기존 `Game/Tests/DigetherDataDefinitionTest.cpp`는 프로토타입 헤더·BP를 읽는 회귀 테스트로 남긴다. 이 테스트는 `Game/` 독립 실행의 증거가 아니다), 프로토타입 클래스도 `Game/` 쪽 새 구성 요소를 호출하도록 바꾸지 않는다(2026-11-04 결정). 프로토타입의 산식·흐름은 참고해서 `Game/`에 새로 작성한다.

아래는 2026-11-10 현재 실제 폴더다. `Game/` 폴더 목록의 원본은 이 블록이며 다른 문서는 여기를 링크한다.

```text
Game/
  Core/        게임플레이 태그, 권한 검사 도우미(결과 코드는 도메인별 enum으로 각 폴더에 있음)
  Data/        정의 자산(아이템·튜닝 프로필), Asset Manager 연동
  Dev/         개발용 치트·콘솔 명령 등록 틀, 개발 로그
  Economy/     지갑·납부(붓기)·할당량·바닥 금화(순수 계산 함수 포함)
  Elevator/    운행 상태머신·무게 단계·레버
  Excavation/  굴착. `Dain/`(흙 격자 코어·굴착 Actor), `DainLab/`(시험용 GameMode·Character·검증 Actor). 프로토타입 쪽은 `Prototype/World/DigetherExcavation*`
  Framework/   실행 진입점(GameMode·GameState·PlayerState·PlayerController·시험 Pawn)과 런 흐름·저장·붓기·게임 오버·접속·관전·음성 연결
  Frontend/    메인 메뉴·대기실 GameMode와 컨트롤러, 프런트엔드 설정
  Hub/         거점 시설·상점 카탈로그·거점 상태(개인 소지품·배송·해금)
  Input/       입력 안내 문구 조회(Input Action·Mapping Context 연결은 Framework의 PlayerController)
  Life/        체력·사망·관전 상태
  Monster/     몬스터 정의 자산·체력 규칙과 컴포넌트. 근거: Notion DIGETHER 작업 보드 DEV-050~052(2026-10-30 엑셀 사본이며 승인된 기획 원본 아님).
               2026-11-04 사용자 결정: 굴착로 이동은 런타임 내비메시 `RuntimeGeneration=Dynamic` + 맵의 제한된 NavMeshBoundsVolume,
               몬스터 AI(DEV-049)는 Behavior Tree(보드의 '기획자가 직접 편집하는 BT 에디터'와 같은 방식). 이 폴더에 이동·AI C++ 코드는 아직 없다
  Online/      세션 생성·검색·참가(`DigetherSessionSubsystem`), 참가 판정·같은 ID 교체 대기(프로토타입 쪽은 `Prototype/Online/`)
  Run/         기간·런·일차 흐름(조율자), 게임 오버 흐름
  Save/        저장·불러오기·스키마 버전·검증
  Settings/    사용자 옵션·키 재설정
  Tests/       `DIGETHER.Game.*` 자동 테스트
  UI/          HUD·위젯(세션 문구만 String Table이고 나머지는 코드 안 `FText`)
  Voice/       음성 수신 제한·감쇠·발화 표시
```

계획에 있었지만 아직 만들지 않은 폴더는 넷이다. `Session/`(참가·재접속 레지스트리: 현재 GameMode와 `Online/`의 참가 판정이 맡는다), `Inventory/`(아이템 인스턴스와 슬롯: 미구현), `Interaction/`(상호작용 인터페이스와 장치 컴포넌트: 미구현), `Terrain/`(굴착 밀도 지형: 실제 코드는 `Excavation/Dain/`에 있고 최종 경로는 [굴착 구현](EXCAVATION_IMPLEMENTATION.md)의 기준 계열 결정과 함께 정한다). 만들지, 현 위치를 기준으로 삼을지는 결정 대기다.

프로토타입 기능에 대응하는 `Game/` 위치(프로토타입 파일은 그대로 두고 새로 작성): 시체(`Corpse`)는 `Life/`, 포탈·광산 장면(`Portal`·`MineScene`)은 `Interaction/`과 월드 배치, 달리기·헤드램프 요청은 Character와 `Input/`, 납부 입력 위젯은 `UI/`.

### 실행 진입점

프로토타입 GameMode를 연결층으로 쓰지 않으므로 `Game/` 쪽에 별도 진입점이 필요하다. 클래스 이름은 후보다.

- GameMode·GameState·PlayerState·PlayerController를 `Game/Framework/`에 새로 둔다(2026-11-04 구현: `ADigetherGameplayGameMode`·`ADigetherGameplayGameState`·`ADigetherGameplayPlayerState`·`ADigetherGameplayPlayerController`. 템플릿의 `ADIGETHERGameMode`·`ADIGETHERPlayerController`와 이름이 대소문자만 달라 UHT·UObject 이름이 충돌하므로 `Gameplay`를 붙였다). 계획은 GameMode가 로그인·스폰·구성 요소 연결만 맡고 규칙은 아래 배치표의 컴포넌트가 맡는 것이다. 현재 구현은 다르다. `ADigetherGameplayGameMode`가 런 흐름(`DigetherGameplayRunFlow.cpp`)·저장(`DigetherGameplaySave.cpp`)·붓기(`DigetherGameplayPour.cpp`)·게임 오버·접속 판정·재접속·관전의 상태와 처리를 구현 파일을 나눠 직접 갖고, 런 조율 컴포넌트는 단계·타이머 상태만 맡는다(`DigetherRunCoordinatorComponent.h` 주석: 결과 적용은 호출자 책임). 문서를 현 구조에 맞출지, 컴포넌트로 분리할지는 결정 대기다. 개발 시험 맵은 `/Game/Game/Maps/L_GameTest`이며 공유 레벨이 아니다. 플레이어 Pawn은 정식 캐릭터가 나올 때까지 임시 시험 Pawn `ADigetherTestPawn`(엔진 `ADefaultPawn` 하위, 클라이언트 이동의 서버 전달 없음)을 쓴다.
- **실행 맵(2026-11-04 사용자 결정): 새 맵을 만든다.** 기존 맵(`L_Prototype` 등)은 바꾸지 않는다. 새 맵의 World Settings에서 `Game/` GameMode를 지정한다. 현재 실행 맵은 거점 `/Game/Game/Maps/L_Hub`이고 갱도는 같은 월드의 하위 레벨 `L_Mine`이다(`scripts/editor/build_mine_streaming_level.py`가 Always Loaded로 만든다. 맵 내부는 이 문서에서 열어 확인하지 않았다). 플레이어 Pawn은 아래 미결정 항목을 따른다. `.umap` 생성·수정은 에셋 작업이므로 담당·잠금을 확인한다.
- **두 실행 경로를 구분한다.** (1) 개발 시험용 직접 실행: 맵 + `?game=` 또는 맵의 GameMode 지정. (2) 정식 실행: `L_MainMenu` → 대기실 `L_Lobby` → `DigetherSessionSubsystem`이 설정의 GameMap에 `?listen`을 붙여 Seamless로 이동하는 경로(`DigetherLobbyGameMode`의 `bUseSeamlessTravel`, 2026-11-06 사용자 결정). `DefaultGame.ini`의 GameMap은 `L_Hub`이고 `MapsToCook`에 `L_GameTest`·`L_Hub`가 있다. cook 로드와 다인 검증 범위는 [구현 현황](IMPLEMENTATION_STATUS.md)을 따른다.
- **참가·재접속 계약**: 새 GameMode·Controller는 [전체 설계](../SYSTEM_DESIGN.md)의 인증 ID 기준 참가, 재접속 복원, 초기 지급 재실행 금지를 지킨다. 같은 ID가 다시 들어오면 이전 접속을 끊고 새 접속을 받는다(사용자 결정 2026-11-05, 기획 답변 C2. `PreLoginAsync`로 구현, 자동 테스트 통과, M2 미검증. 교체 대기 5초는 임시값). 교체 시 엔진 `KickPlayer`의 Pawn 파괴를 그대로 쓰지 않는다([네트워크 카탈로그](../unreal/features/NETWORKING.md)). 구현된 순서는 로그인 → `FindInactivePlayer` 재정의가 같은 ID의 비활성 PlayerState를 찾음 → 있으면 원본 PlayerState를 다시 붙이고 기존 Pawn 재빙의(신규 스폰·지급 없음), 없으면 저장 명단 레코드로 복원하거나 신규 스폰·지급이다. 연결 해제 시 Pawn을 남겨 두는 책임은 Controller의 `PawnLeavingGame` 재정의와 GameMode의 `AddInactivePlayer` 재정의가 나눠 맡는다(프로토타입 Controller와 같은 훅을 참고).
- **미결정**: 플레이어 Pawn·Character. 플레이어 기능은 다른 담당자 범위다. `Game/` 규칙이 의존할 사망 통지·소지품 무게 같은 접점과 Pawn 클래스 지정 방법을 플레이어 담당과 먼저 합의한다.
- 순수 계산 함수(경제 산식, 슬롯·상태 전이, 저장 변환)는 진입점이 없어도 자동 테스트로 먼저 구현·검증할 수 있다.

### 배치

| 도메인 | 배치 | 수명·주의 |
|---|---|---|
| 세션 레지스트리 | GameMode에 붙는 서버 전용 Actor Component | GameMode는 월드 수명이다. 이 계획은 거점과 탐사가 **같은 월드**인 구조만 다룬다(현재 `Game/`은 `L_Hub`와 그 하위 레벨 `L_Mine`, 프로토타입은 `L_Prototype`). 연결이 끊긴 플레이어의 레코드는 끊긴 시점 스냅샷이 아니라, 그동안의 피해·사망·탈출·정산을 계속 반영한 최신 상태로 유지한다. 현재 구현에는 이 컴포넌트가 없고 GameMode의 명단 멤버와 비활성 PlayerState가 같은 책임을 맡는다(아래 [상태 인계 계약](#상태-인계-계약)) |
| 런·기간 조율자 | GameMode에 붙는 서버 전용 Actor Component | 규칙 판정·타이머·종료·정산 순서를 맡는다. GameMode가 규칙을 판정한다는 [구현 규칙](DEVELOPMENT_SYSTEM_DESIGN.md#구현-규칙)과 같은 배치다. 현재 `UDigetherRunCoordinatorComponent`는 단계·타이머·종료 판정만 맡고 결과 적용(생존·경제·저장)은 GameMode가 한다 |
| 런·기간 공개 상태 | GameState에 붙는 Actor Component | 조율자가 확정한 단계·일차·목표·남은 시간만 복제해 보여 준다. 판정 로직을 두지 않는다 |
| 엘리베이터 | 엘리베이터 액터의 Actor Component, enum 상태머신 | 공개 진행 상태·무게 단계만 복제 |
| 인벤토리·지갑·생존 | PlayerState에 붙는 Actor Component | Pawn 재생성에는 유지된다. PlayerState 교체(재접속·travel)에는 아래 인계 계약을 따른다. 지갑(`UDigetherWalletComponent`)·생존(`UDigetherLifeStateComponent`)은 구현됐고 인벤토리는 미구현이다. 거점에서 산 소지품은 PlayerState가 아니라 월드 액터 `ADigetherHubStateActor`가 플레이어 ID별로 갖는다(소유 위치 통일은 결정 대기) |
| 경제·슬롯·상태 전이 규칙 | 순수 C++ 함수·값 타입(엔진 월드 의존 없음) | 자동 테스트로 고정 |
| 저장 | `UGameInstanceSubsystem` | 호스트에서만 저장·불러오기. 세션 종료·월드 해제 때 월드 객체 참조를 정리 |
| 상호작용 장치 | 공통 인터페이스 + 기능별 Component(상점·예치·출발·긴급 탈출) | 새 장치는 Component 추가로 만든다. 현재는 공통 인터페이스가 없고 장치 종류(거점 시설·정산기·레버·바닥 금화)마다 전용 Server RPC와 GameMode 함수가 있다 |
| GameMode | 로그인·스폰·구성 요소 연결 | 규칙 판정은 각 도메인에 위임. 현재 구현과의 차이는 [실행 진입점](#실행-진입점) 참고 |

### 복제 계약

- 복제할 컴포넌트는 생성자에서 `SetIsReplicatedByDefault(true)`를 켜고, 소유 액터도 복제 액터여야 한다. 컴포넌트는 기본적으로 복제되지 않는다.
- 같은 컴포넌트 안에서 공개 값과 개인 값을 나눈다. 예: 생존 컴포넌트의 생존 상태·매몰 단계는 모두에게(팀원·관전자 표시용), 체력 상세·지갑·인벤토리 상세는 `COND_OwnerOnly`. 컴포넌트 전체를 소유자 전용으로 만들지 않는다. 현재 `DigetherPrototypePlayerState`의 공개/개인 구분을 그대로 옮긴다.
- 복제 값은 서버 함수만 바꾼다. 클라이언트는 OnRep로 표현만 갱신한다([네트워크 권위](../unreal/NETWORK_AUTHORITY.md)).

### 상태 인계 계약

- PlayerState 컴포넌트마다 값 구조체로 상태를 꺼내고 넣는 함수 쌍(`CaptureState`/`RestoreState`)을 둔다. 엔진 기본 `APlayerState::CopyProperties`는 사용자 컴포넌트 상태를 복사하지 않는다.
- 재접속은 구현에서 두 경로로 갈렸다(`Game/Framework/DigetherGameplayReconnect.cpp`). 아래 두 항목의 원래 계획(레지스트리가 값을 보관하고 새 PlayerState에 복원)은 거점 이탈 경로에만 해당한다.
  - **탐사 중 이탈**: `AddInactivePlayer` 재정의가 PlayerState 원본과 그 컴포넌트를 복제 없이 `InactivePlayerArray`에 그대로 보존하고(복제 중단, 수명 제한 없음), 남은 Pawn도 유지한다. 같은 ID가 돌아오면 `FindInactivePlayer` 재정의가 원본을 새 연결에 다시 붙인다. 끊긴 동안의 사망·탈출·정산은 살아 있는 원본 컴포넌트에 그대로 적용되므로 보관 값에 순수 함수를 따로 적용하는 단계가 없다.
  - **거점(탐사 밖) 이탈**: 자리를 반환하고 PlayerState·Pawn을 제거한다. 최신 장부만 저장 명단 값 레코드(`FDigetherSavePlayerRecord`)로 남기고, 돌아오면 새 PlayerState에 `RestoreState`로 복원한다(시작 지급 없음).
- 참가자 명단이 용도별로 여러 곳에 있다: GameState `PlayerArray`, `InactivePlayerArray`, 런 참가자, 저장 명단 레코드, 관전 합류자, 로그인 컨트롤러 목록. 용도별 기준 명단을 한 표로 고정하는 일이 남아 있다.
- **거점·탐사 사이의 월드 이동은 이 계획 범위 밖이다.** 현재 넘어가는 구간은 대기실 `L_Lobby` → 거점 `L_Hub`의 Seamless 이동 하나이며, 대기실 PlayerState와 게임 PlayerState는 서로 다른 클래스다(`ADigetherGameplayPlayerState`가 `CopyProperties`·`OverrideWith`를 재정의하고 GameMode가 `HandleSeamlessTravelPlayer`를 재정의한다). 이 구간에서 무엇을 넘기고 버리는지는 아직 이 문서에 계약으로 적지 않았다. 거점·탐사를 다른 맵으로 나누기로 하면, 먼저 게임·기간·런 상태, 회수 건, 정산 완료 기록, 세션 레지스트리, PlayerState 컴포넌트 상태 전체를 값 스냅샷으로 서버 GameInstanceSubsystem에 넘기는 설계를 따로 만든다([전체 설계의 인계 계약](../SYSTEM_DESIGN.md#세션게임런월드-인계)). 엔진 기본 보존 범위는 seamless travel 여부에 따라 다르므로, 두 경로별 복원과 `CopyProperties` 재정의를 그 설계에서 정하고 검증한다. 이전 월드의 Pawn·Controller 포인터는 넘기지 않는다.
- 초기화 범위는 사건별로 따로 정한다. 지갑 컴포넌트 자체는 항상 유지되며, 아래 "잔고"는 그 안의 값이다. 수치는 [전체 설계](../SYSTEM_DESIGN.md)와 D25 시험값을 따른다.

| 사건 | 잔고·기록 | 소지품 | 생존 상태 |
|---|---|---|---|
| 사망 | 잔고 손실 적용(현재 시험값 100%), 사망 기록·사망 직전 잔고 보존 | 드롭(시체·월드로) | 사망·관전 |
| 일반 탈출 | 유지 | 장비·회수품 보존(편별 회수 건으로) | 탈출·관전 |
| 긴급 탈출 | 유지 | 긴급 장치만 소비, 회수품·시체 없이 | 탈출·관전 |
| 거점 귀환·부활 | 정산·복구액 반영 | 장비 유지, 런 전용 상태 초기화 | 활동으로 초기화 |
| 새 게임 | 이전 결과는 저장에 고정, 새 값으로 시작 | 시작 지급만 | 초기화 |

### 의존 방향과 확정 순서

- 명령은 위에서 아래로만 간다. 런 조율자(GameMode 컴포넌트)가 엘리베이터·경제·인벤토리·생존에 명령하고 확정 결과를 GameState 공개 상태에 쓴다(현재 구현에서 이 명령 역할은 GameMode가 직접 한다). 하위 도메인은 서로를 직접 호출하지 않는다.
- 결과 통지는 아래에서 위로 간다. 하위 도메인은 확정된 결과를 타입 있는 델리게이트로 알린다. 공용 데이터는 `Core`의 값 타입만 공유한다.
- 여러 도메인을 바꾸는 서버 처리(구매, 정산 등)는 조율자의 한 함수에서 **모든 검사 → 모든 변경 → 알림** 순서로 실행한다. 검사가 하나라도 실패하면 아무것도 바꾸지 않는다. 규칙상 이어지는 연쇄 결과(예: 사망 → 소지품 드롭 → 잔고 손실)는 알림을 받은 쪽이 아니라 조율자가 같은 함수 안에서 이어서 명령해, 중간 상태가 복제·저장되지 않게 한다([전체 설계](../SYSTEM_DESIGN.md)의 같은 실행 안 반영 기준). 알림을 받은 쪽(공개 상태·UI·로그)은 표현만 갱신하고 다른 도메인 상태를 바꾸지 않는다.
- 경제 산식, 슬롯 판정, 엘리베이터 상태 전이, 저장 변환은 월드 없이 검사할 수 있는 순수 함수로 둔다.

## 3. 단계별 적용 계획

단계 이름 P0~P7은 이 계획의 구조 이전 단계이며, 전체 설계의 기능 우선순위 표기(S34·S35의 P2)와 다른 구분이다. 기능 개발 순서 ①~⑦과는 병행하되, 같은 도메인의 새 기능은 그 도메인의 추출(P4) 뒤에 얹는다.

각 단계는 따로 빌드·검증할 수 있게 나눈다. 2026-11-04 결정으로 P2 이후 단계는 `Prototype/` 파일을 고치지 않는다. 표의 "바뀌는 파일"에 있는 프로토타입 파일(Types·SaveGame·Character·GameMode·PlayerController·World·HUD)은 같은 역할의 `Game/` 새 파일로 읽고, 완료 조건의 "기존 동작 동일·회귀"는 프로토타입 동작과 같은 시나리오를 `Game/` 진입점에서 비교하는 뜻으로 읽는다. 프로토타입과 새 구조 사이에 값을 복사하는 연결 함수는 두지 않는다. P0·P1에서 이미 프로토타입에 넣은 연결(정의 자산 브리지·런 스냅샷)은 그대로 둔다.

**P2 저장 변환(2026-11-04 사용자 결정): 새 저장 시스템은 v2 세이브를 읽지 않는다.** v2는 프로토타입 타입(`UDigetherPrototypeSaveGame`·`FPrototype*`)으로 저장된 프로토타입 시험 데이터로 보고, `Game/Save/`는 새 형식(v3)만 읽고 쓴다. 변환 도구나 v2 읽기용 구조체는 만들지 않고, 프로토타입으로 한 진행은 새 게임에서 이어지지 않는다. v3는 프로토타입 슬롯과 다른 슬롯 이름을 써서 기존 v2 파일을 덮어쓰지 않는다. 이 결정으로 원래 P2 행에 있던 v2 읽기·변환·원본 보존 항목은 표에서 뺐다. 미래 버전·손상 파일 거부, v3 재저장 → 재시작 → 재로드 일치, 모든 생성 경로의 `DefinitionId` 채움은 그대로 완료 조건이다.

저장 버전은 세 축이며 문서마다 다른 축을 "v3"·"v5"로 부르므로 구분한다(`Game/Save/DigetherSaveTuning.h`, `DigetherSaveGame.h`). 진행 값 스키마 버전 3, 슬롯 파일 이름 버전 3(스키마를 올려도 기존 저장을 찾도록 고정), 파일 바이트(wire) 형식은 쓰기 5·읽기 3~5다. 이 문서의 "v3"는 스키마 버전이다.

| 단계 | 내용 | 바뀌는 파일·에셋 | 완료 조건 |
|---|---|---|---|
| P0 기반 | `GameplayTags` 모듈 의존, 네이티브 태그(최상위는 [태그 기준](DEVELOPMENT_SYSTEM_DESIGN.md#정의-데이터와-태그)의 `Item.*`·`Interact.*`·`Terrain.*`·`Player.State.*`·`Result.*`), 결과 코드(`Result.Fail.*` 태그 + `FText`), `LogTemp` 정리. 적용된 것은 모듈 의존과 `Item.*` 태그 4개이며, 결과 코드는 구현에서 도메인별 enum으로 갔다(태그로 바꿀지 enum을 기준으로 삼을지 결정 대기) | Build.cs, `Game/Core/`, `Config/Tags/`(미생성) | 기존 동작 동일, 빌드·기존 테스트 통과 |
| P1 정의 데이터 | `UDigetherItemDefinition`(값·무게·특성 태그·소프트 메시), `UDigetherTuningProfile`(런 시작 시 고정할 값: 제한 시간·용량·운행 횟수·사망/복구 비율 등). Asset Manager 타입 등록(`AlwaysCook`), `IsDataValid`. 픽업(`DigetherPrototypeWorld.cpp`)이 정의를 읽어 기존 아이템 구조로 복사, GameMode 런 초기화가 프로필 스냅샷을 사용, Rules 산식이 비율을 인자로 받음. 기존 BP·맵의 GameMode 기본값과 픽업 편집값을 DA로 옮김. 프로필에 정의 버전 필드를 둔다. 튜닝 원본은 DA로 시작하고, 텍스트 원본(CSV/JSON, R14) 비교는 P7에서 한다 | `Game/Data/`, `DefaultGame.ini`, World·GameMode·Rules, 새 `DA_` 에셋, BP·맵 | 옮긴 값이 이전 값과 일치, 런 시작 시 버전과 함께 스냅샷, 패키지에서 정의 로드 |
| P2 아이템 인스턴스·저장 변환 | 인스턴스 = `InstanceId`(기존 `Id` 보존) + `DefinitionId` + 슬롯 상태. 히든 여부는 정의 특성이 아니라 **슬롯 상태**로 둔다. 저장 스키마 v3(v2는 읽지 않음, 위 결정). 정의를 찾을 수 없는 항목은 레거시 정의로 매핑하고 경고, 미래 버전·손상 파일은 덮어쓰지 않고 오류. 저장(스키마 3·검증·백업)은 구현됐고 아이템 인스턴스는 미구현이다 | Types, SaveGame, Save, 인벤토리·HUD, 아이템 생성 경로(픽업·시작 도구·구매·긴급 장치·사망 드롭) | v3 재저장 → 프로세스 재시작 → 재로드 후에도 일치, 모든 생성 경로가 `DefinitionId`를 채움, 손상·미래 버전 거부. 위험도 중상 |
| P3 Enhanced Input | `IA_`·`IMC_` 에셋(탐사·거점·관전·UI), Character 바인딩 교체. 거점 상호작용·붓기·일시정지는 `Game/` PlayerController가, 누르고 말하기는 음성 컴포넌트가 Input Action·Mapping Context로 연결했다. 이동·시점 등 캐릭터 입력은 정식 캐릭터 결정 대기 | `Game/Input/`, Character, 새 입력 에셋 | 실제 입력 경로로 모든 조작 동작, UI 진입·종료, 누르던 행동 취소, 관전·재빙의 뒤 Context 복원 |
| P4 도메인 구성 요소 구현 | 아래 구현 순서와 계약. 프로토타입 GameMode는 축소하지 않는다(2026-11-04) | `Game/*`, `Game/` 새 GameMode | 구성 요소마다 1인·4인 시나리오, 상태 보존, 비공개 데이터 비전달 확인 |
| P5 요청 경로 | RPC `Server_`·`Client_` 접두사. 구매·납부·해금·출발·**회수·승강** 요청에 클라이언트가 본 `PeriodId`/`RunId`를 담고 서버가 현재 값과 비교. 불러오기 후 클라이언트 ID 갱신. 서버 빈도 제한, 거절은 결과 코드로. 납부 입력 세션(서버 발급 세션 ID·납부대·기간 세대, [미로·폐교 흐름](../prototype/MAZE_SCHOOL_FLOW.md))은 화면 입력의 수명 관리로 유지하며 일반 요청의 준비/확정 단계로 보지 않는다. 현재 구현은 접두사만 따르고 `PeriodId`/`RunId` 인자가 없다. 거점 시설 요청은 `FGuid RequestId`를 받아 서버가 처리 기록으로 중복을 거르고, 붓기·레버·줍기는 서버의 현재 단계 검사만 받는다. 이 차이를 어느 쪽으로 맞출지는 PQ-35와 함께 결정 대기다 | PlayerController, 관련 Component | 이전 런·기간의 늦은 최초 요청 거절, 진행 중 행동의 늦은 완료 거절, 연타 차단 |
| P6 장치·문구 | `EPrototypeStationKind` 분기를 장치 Component로, 화면 문구를 String Table로, 새 위젯은 CommonUI 기준([표준 엔진 기능](../unreal/STANDARD_FEATURES.md))으로 만들고 `CommonUI`·`CommonInput` 모듈 의존 추가. 모듈 의존과 CommonUI 위젯은 적용됐다. String Table은 세션 문구(`ST_Session`)만이고, 장치는 종류별 전용 RPC·GameMode 함수라 완료 조건과 아직 반대 방향이다 | World, HUD, Build.cs, 새 String Table | 새 장치를 GameMode·Station 수정 없이 추가 |
| P7 튜닝 레지스트리(후속) | 파라미터 레지스트리(단위·범위·런타임 변경 여부·버전, R09~R10), 호스트 런타임 변경과 클라이언트 동기화(R04) | 별도 설계 | 이 계획 범위 밖. P1은 정의 분리와 런 고정값까지만 다룬다 |

### P4 구현 순서와 연결 계약

마지막 열은 동작을 참고할 프로토타입 함수다. 해당 함수는 수정하지 않는다.

| 순서 | 구성 요소 | 입력 | 출력·통지 | 참고할 프로토타입 동작 |
|---|---|---|---|---|
| 1 | 저장(Subsystem) | `Game/` GameMode가 모은 저장 데이터 | 저장·불러오기 결과 코드 | `RequestSave`/`RequestLoad` |
| 2 | 세션 레지스트리 | 로그인·로그아웃, PlayerState | 재접속 복원 완료 | `PostLogin`/`Logout`, `HandleControllerDisconnect` |
| 3 | 경제(지갑·구매·납부) | 조율자 명령, 정의·튜닝 | 잔고 변경 통지 | `RequestPurchase`/`RequestDeposit` |
| 4 | 인벤토리 | 줍기·버리기 명령 | 슬롯 변경 통지 | `RequestPickupItem`/`RequestDrop` |
| 5 | 생존·구조 | 피해·매몰·구조 명령 | 사망·탈출 확정 통지 | `RequestDamage`/`RequestRescue` 등 |
| 6 | 엘리베이터 | 출발·운행 명령 | 편 성공·취소 통지 | `RequestElevator`·`UpdateElevator`(bool 상태 묶음 대신 enum) |
| 7 | 런 조율자 | 위 통지 | 종료·정산·일차 전환 | GameMode `Tick`·`FinishRun` |

순수 계산 함수(경제 산식 등)는 순서와 관계없이 먼저 만들 수 있다. 인벤토리·생존은 플레이어 담당 범위와 겹치므로 접점 합의 뒤에 진행한다.

2026-11-10 현재 대응하는 구현: 1 `UDigetherSaveSubsystem`(`Game/Save/`), 2 별도 구성 요소 없음(GameMode의 `AddInactivePlayer`·`FindInactivePlayer` 재정의와 `Game/Online/DigetherJoinRules`), 3 `UDigetherWalletComponent`·`UDigetherQuotaStateComponent`와 GameMode의 붓기 처리(구매는 `Game/Hub/`), 4 미구현, 5 `UDigetherLifeStateComponent`(매몰·구조는 미구현), 6 `UDigetherElevatorComponent`·`UDigetherElevatorLeverComponent`, 7 `UDigetherRunCoordinatorComponent`와 GameMode의 런 흐름. 검증 범위는 [구현 현황](IMPLEMENTATION_STATUS.md)을 따른다.

## 4. 위험과 대응

- 아래 두 항목(이름 변경·RPC 이름 변경)은 프로토타입을 고치던 이전 계획의 위험이다. 2026-11-04 결정 뒤에는 P0·P1에서 이미 바꾼 부분에만 해당하고, 앞으로의 작업은 `Game/` 새 클래스와 새 에셋 연결 기준으로 본다.
- **이름 변경과 데이터 이전은 다른 일이다.** 같은 타입의 이름 변경은 `DefaultEngine.ini` CoreRedirects로 처리한다. 타입 변환(bool→태그)과 소유 객체 이동(GameMode 속성→DA, 컴포넌트)은 CoreRedirects로 안 된다. 기존 BP 기본값·맵 인스턴스 값을 먼저 수집하고 새 위치로 옮긴 뒤 값을 비교한다. 완료 조건은 BP 컴파일, 맵 로드, 재저장, cook이다. 바이너리 안의 실제 참조·기본값은 아직 확인하지 않았다.
- **RPC 이름 변경**: 현재 Controller RPC는 `BlueprintCallable`이 아니어서 BP 직접 호출 가능성은 낮지만, 변경 전 BP 참조를 확인한다. 에디터를 닫고 빌드한다.
- **바이너리 에셋 충돌**: `.uasset`·`.umap` 수정과 새 `DA_`·`IA_` 에셋 생성은 담당자·잠금을 확인한다([에셋 안전](../unreal/ASSET_SAFETY.md)).
- **저장 호환**: P2는 실제 사용자 세이브 대신 임시 경로로 시험한다([데이터 작업](../unreal/DATA_WORKFLOW.md)).
- **동작 회귀**: 각 단계 전후 같은 검사를 돌려 비교한다.
- **범위 확장**: Gameplay Ability System·Game Features·Push Model/Iris는 넣지 않는다(별도 결정).
- **규칙 중복(2026-11-04 결정의 대가)**: 프로토타입을 고치지 않으므로 같은 규칙이 `Prototype/`과 `Game/` 두 곳에 존재한다. 프로토타입은 새 기능·수치 변경을 받지 않는 고정 기준으로 두고, 기획 변경은 `Game/`에만 반영한다. 반대로 프로토타입 이름 변경·CoreRedirects·RPC 이름 변경 위험은 P2 이후에 생기지 않는다.
- **공유 정의 데이터로 인한 회귀 기준 변화**: 프로토타입은 `Game/Data/`의 정의 타입과 `DA_Tuning_Prototype`·`DA_Item_PrototypeCargo`를 읽고, `DigetherDataDefinitionTest`는 그 값을 고정 수치로 기대한다. 공유 타입의 동작이나 이 두 DA 값을 바꾸면 프로토타입 동작도 바뀐다. 이 두 DA는 회귀 기준으로 값 변경을 금지하고, 정식 데이터는 별도 ID의 새 DA를 쓴다. 공유 타입에 필드를 추가할 때는 기존 DA의 기본값이 그대로 유지되는지 확인한다.

## 5. 단계별 검증

- **빌드**: Editor·Game Development 타깃. 헤더 의존을 바꾼 단계는 non-unity 빌드.
- **자동 테스트**: 등록 이름 `DIGETHER.Prototype.Settlement`·`DIGETHER.Prototype.Excavation.Field`·`DIGETHER.Prototype.DepositInput`과 단계별 새 테스트(`DIGETHER.Game.*`: 태그·정의 검증, 경제 산식, 슬롯 판정, 엘리베이터 상태 전이, 저장 변환). 굴착 코어·Actor 테스트는 `DIGETHER.Dain.*`으로 등록돼 있다. `scripts/Invoke-UnrealChecks.ps1`은 2026-11-10에도 `Workbench.` 테스트만 고르므로(실행기 미수정) `DIGETHER.` 필터를 추가하거나 직접 Automation 명령으로 실행한다. 실행기를 고칠 때는 실행 필터와 기대 테스트 수집 범위를 함께 바꾼다. 단계마다 기대 테스트 개수를 기록하고 **0개 실행은 실패**로 본다.
- **데이터 검증**: DataValidation 대상을 정의 에셋(`DA_`)으로 지정해 실행한다.
- **실행**: `DigetherPrototypeProbe` 1인 시나리오와 별도 프로세스 4인 M2(경합·재접속·비공개 데이터 비전달). Probe는 프로토타입 GameMode 기준이므로 `Game/` 진입점의 1인 실행 수단은 개발 명령 `DigetherRunSmoke`(출발 → 상승 → 종료 → 귀환)이며 Shipping에서는 빠진다. Probe는 입력을 우회하므로 P3은 실제 입력 경로 검사를 따로 한다. 패키지에서 정의 로드를 확인한다.
- 각 단계 결과와 미검증 범위를 작업 기록에 남긴다. 이 문서 작성은 구현·검증 완료가 아니다.
