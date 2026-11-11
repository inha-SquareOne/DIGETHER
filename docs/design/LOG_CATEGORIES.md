# 로그 분류 기준 (DEV-070)

기준일: 2026-11-04(카테고리 표와 추적 필드 적용 상태는 2026-11-10 코드 대조로 정정). 정산 중복·동기화 불일치를 추적하기 위한 로그 카테고리와 필드 규칙이다. 코딩 규칙의 원본은 [코딩 컨벤션](../CODING_CONVENTION.md)이며, 이 문서는 카테고리 이름 표와 추적용 필드를 정한다. 아래 "제안" 표시는 아직 코드에 없는 이름이다.

## 규칙

1. 카테고리 이름은 `LogDigether<기능>`이다. 새 코드에 `LogTemp`를 쓰지 않는다.
2. 각 도메인은 자기 폴더에 자기 카테고리를 둔다. 여러 파일에서 쓰면 도메인 폴더의 `Digether<기능>Log.h`에 `DECLARE_LOG_CATEGORY_EXTERN`을, 한 파일에서만 쓰면 `.cpp`의 `DEFINE_LOG_CATEGORY_STATIC`을 쓴다. 다른 도메인 카테고리로 로그를 남기지 않는다.
3. 개발 치트툴 모듈 `DIGETHERDevTools`(Shipping 미빌드)는 개발 지원 카테고리 `LogDigetherDev` 하나만 둔다(치트 명령 응답, 개발 도구 메시지). 도메인 이벤트 로그를 여기에 모으지 않는다.
4. 수준: 정상 흐름의 기록은 `Log`, 개발 명령 응답·한 번 남는 상태 변화는 `Display`, 조용히 넘어간 비정상은 `Warning`, 규칙 위반·거절·불일치는 `Error`. 매 프레임 로그는 `Verbose` 이하로 둔다.
5. 추적이 필요한 사건(정산, 동기화, 거절)은 문구 뒤에 `키=값`을 공백으로 이어 쓴다. 비밀값·표시 이름·IP 주소는 남기지 않는다(재접속 식별 기준은 [전체 설계](../SYSTEM_DESIGN.md)).

## 추적 필드

| 키 | 뜻 | 쓰는 곳 |
|---|---|---|
| `Run` | 런 ID | 모든 런 중 사건 |
| `Profile` / `ProfileVer` | 런 시작 때 고정한 튜닝 프로필 ID와 버전([튜닝 대조표](TUNING_ENVIRONMENT_STATUS.md)) | 런 시작, 정산 |
| `Settle` | 정산 사건 ID. 같은 ID의 완료가 두 번 나오면 중복 정산 의심 | 정산 시작·완료·거절 |
| `Seq` | 같은 대상의 변경 순번. 서버와 클라이언트가 같은 순번을 보는지 대조 | 동기화 갱신 |
| `Actor` | 참가 ID(표시 이름 아님) | 참가자 단위 사건 |
| `Result` | 결과 코드 이름 | 거절·실패 |

필드 이름은 정산·동기화를 구현하는 패키지가 확정한다. 이 표에 없는 키를 쓰면 이 문서에 먼저 추가한다.

적용 상태(2026-11-10 코드 대조): 이 표의 추적 필드는 아직 계약대로 쓰이지 않는다. `Settle`·`Seq`·`ProfileVer`는 `Game/` 코드에 없다. 런 시작 로그는 `Profile=<ID> Version=<번호>` 형식이고(`Game/Framework/DigetherGameplayGameMode.cpp`), 정산 로그에는 프로필 버전이 없다. `Run`은 런 단계 로그와 개발용 상태 덤프에만 나오고 납부 결과 로그는 `Operation`·`Result`·`Station`·`Period` 같은 이 표에 없는 키를 쓴다. 로그 형식을 이 표에 맞출지, 표를 현재 형식에 맞출지는 정하지 않았다.

## 카테고리 이름 표

| 카테고리 | 위치 | 상태 | 용도 |
|---|---|---|---|
| `LogDigetherDev` | `DIGETHERDevTools/Public/DigetherDevRegistry.h`(2026-11-11 `Game/Dev`에서 이동) | 구현 | 치트 명령 응답, 개발 도구 메시지 |
| `LogDigetherSession` | `Game/Online/DigetherSessionSubsystem.cpp` | 구현 | Steam 세션 생성·참가 |
| `LogDigetherFrontend` | `Game/Frontend/DigetherFrontendPlayerController.cpp` | 구현 | 메인 메뉴·대기실 화면 흐름 |
| `LogDigetherLobby` | `Game/Frontend/Lobby/DigetherLobbyGameMode.cpp` | 구현 | 대기실 GameMode |
| `LogDigetherFramework` | `Game/Framework/DigetherFrameworkLog.h` | 구현 | 게임 진입점: 접속·재접속, 런 흐름, 붓기·납부 결과, 저장·불러오기 처리, 게임 오버, 개발 명령 덤프. 현재 로그 대부분이 이 카테고리에 있다 |
| `LogDigetherRun` | `Game/Run/DigetherRunLog.h` | 구현 | 런 조율·게임 오버 흐름 컴포넌트 |
| `LogDigetherLife` | `Game/Life/DigetherLifeLog.h` | 구현 | 생존 상태 컴포넌트 |
| `LogDigetherElevator` | `Game/Elevator/DigetherElevatorLog.h` | 구현 | 엘리베이터·레버 |
| `LogDigetherSave` | `Game/Save/DigetherSaveLog.h` | 구현 | 저장 파일 읽기·쓰기·검증 |
| `LogDigetherVoice` | `Game/Voice/DigetherVoiceLog.h` | 구현 | 마이크 송신 전환, 수신 제한 갱신, 음성 시험 명령 출력 |
| `LogDigetherHub` | `Game/Hub/DigetherHubStateActor.cpp` | 구현 | 거점 상태·시설 거래 |
| `LogDigetherMonster` | `Game/Monster/DigetherMonsterLog.h` | 구현 | 몬스터 체력 컴포넌트 경고. 상태 전이·감지 로그는 AI 구현 전이라 없다 |
| `LogDigetherEconomy` | `Game/Economy/DigetherEconomyLog.h` | 선언만 | 장부·구매·납부·정산 사건(`Settle`, `Run`, `Profile` 포함)용. 아직 이 카테고리로 남기는 로그가 없고 납부 결과는 `LogDigetherFramework`에 남는다 |
| `LogDigetherSync` | `Game/Online/` | 제안 | 서버-클라이언트 동기화 갱신과 버전 경고(`Seq`, `ProfileVer`) |
| `LogDigetherSoil`, `LogDigetherWallTest` | `Game/Excavation/DainLab/` | 구현 | 굴착 시험용 흙벽·검증 Actor |
| `LogDigetherNetworkProbe` | `Source/DIGETHER/Tests/DigetherNetworkProbe.cpp` | 구현 | 네트워크 검사용 프로브 |
| `LogDigetherPrototype*`, `LogDigetherExcavation` | `Prototype/` | 기존(수정 금지) | 프로토타입 동작. 새 `Game/` 코드가 재사용하지 않는다 |
| `LogDIGETHER`, `LogTemplateCharacter`, `LogCombatCharacter` | 템플릿 | 기존 | 템플릿 잔재. 새 코드에서 쓰지 않는다 |

## 정산 중복·동기화 불일치를 추적하는 방법

- 정산은 시작과 끝 모두 `Settle`을 남긴다. 같은 `Settle`의 완료가 두 번이면 중복이고, 시작 없이 완료만 있으면 순서 오류다.
- 클라이언트가 받은 갱신은 `Seq`를 남긴다. 서버가 보낸 마지막 `Seq`와 클라이언트가 적용한 마지막 `Seq`를 개발 명령 응답으로 대조한다(명령은 `DIGETHERDevTools` 모듈의 `DigetherDev::Register`로 붙인다).
- 런 시작 로그의 `ProfileVer`와 정산 로그의 `ProfileVer`가 다르면 진행 중 소급 변경이 있었다는 뜻이다(R01·T65, [전체 설계](../SYSTEM_DESIGN.md)).

## 검증 상태

`LogDigetherDev`의 이름과 치트 등록부는 `DIGETHER.Dev.*` Automation으로 확인한다. "제안"·"선언만" 카테고리와 추적 필드는 적용 전이므로 위 추적 방법은 아직 쓸 수 없고 실행 검증도 없다. 카테고리 표의 "구현"은 선언과 로그 호출이 코드에 있다는 뜻이며 로그 내용의 검증이 아니다.
