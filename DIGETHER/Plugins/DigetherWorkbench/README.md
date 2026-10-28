# DigetherWorkbench — DIGETHER 전용 Editor 어댑터

- 상태: **합성 예제 패널**. 실게임 item/economy/spawn/room/runtime provider는 연결되지 않았고, 승인된 기획 개정도 등록되지 않았다.
- 설계 기준: [`docs/design/DEVELOPMENT_SYSTEM_DESIGN.md`](../../../docs/design/DEVELOPMENT_SYSTEM_DESIGN.md)
- 의존: `DevelopmentWorkbench` 플러그인의 `WorkbenchEditor` 모듈(`Public/WorkbenchModule.h`의 `IWorkbenchModule`, `FWorkbenchToolDescriptor`).

## 의존 방향

- 허용: 이 어댑터 → 게임 Runtime. 승인된 데이터를 읽을 때 어댑터가 Runtime 모듈을 참조해도 된다.
- 금지: Runtime → Editor 모듈. 게임 Runtime은 이 플러그인이나 `WorkbenchEditor`를 참조하지 않는다.
- 현재 `DigetherWorkbenchEditor`는 Runtime 모듈을 아직 참조하지 않는다. Editor 모듈이므로 패키징 대상에서 빠진다.

## 현재 하는 일

`FDigetherWorkbenchEditorModule`이 시작 시 `IWorkbenchModule::RegisterTool`로 **"DIGETHER 연결 상태"**(ID `Digether.ConnectionStatus`) 한 개를 등록하고, 종료 시 해제한다. 패널은 `BuildDigetherConnectionStatusRows()`가 반환하는 6개 행(아이템 정의, 경제 규칙, 스폰 확률, 룸/던전 규격, 멀티 런타임 준비 신호, 기획 승인 개정)을 보여준다.

행 값은 실게임을 조회한 결과가 아니라 "provider가 아직 없다"는 사실을 고정 값으로 적은 것이다. 패널은 게임 데이터를 읽거나 쓰지 않는다. 합성 도구의 결과는 실게임 결과와 다르다고 패널에 안내한다.

## 가짜 provider를 만들지 않은 이유

아이템 스키마·가격/중량 단위, 스폰 선택 규칙, 하루 수익 정의, 룸 표현·문 규격, 리슨/전용 토폴로지는 결정 대기 상태다. 임의 스키마의 provider를 등록하면 합성 결과가 실게임 결과로 오인되므로, provider 부재를 그대로 보여주는 패널만 둔다.

## 실제 데이터 연결을 추가하려면

현재 Core가 제공하는 확장 지점은 **도구 등록**(`IWorkbenchModule::RegisterTool`)뿐이다. 별도의 data/sim provider 인터페이스는 Core에 없으므로 이 어댑터가 `IWorkbenchDataProvider` 같은 계약을 가정하면 안 된다.

1. 해당 도메인의 결정이 승인됐는지 먼저 확인한다. 미결이면 연결하지 않는다.
2. 승인된 스키마에 맞는 조회 코드를 어댑터 모듈에 추가한다. Runtime 데이터가 필요하면 `DigetherWorkbenchEditor.Build.cs`에 Runtime 모듈을 의존으로 추가한다(어댑터 → Runtime만 허용).
3. 연결된 도메인은 `DigetherConnectionStatusModel.cpp`의 해당 행을 실제 조회 결과(`bConnected`, `StatusLabel`, `DetailText`)로 채운다. 조회 실패 시 "미연결"로 되돌아가야 한다.
4. 시뮬레이션·검증 도구를 새로 붙일 때는 `FWorkbenchToolDescriptor`를 만들어 `RegisterTool`로 등록한다. Core의 스폰·밸런스 계산 헤더(`Public/Analytics/*`)는 입력 구조체를 직접 채워 호출한다.
5. `Private/Tests/DigetherConnectionStatusModelTest.cpp`의 "모든 도메인 미연결" 검증은 연결된 도메인이 생기면 깨지므로 그에 맞게 고친다.
6. `/Game/Editor/DevelopmentWorkbench/` 아래 에셋은 `DefaultGame.ini`의 `DirectoriesToNeverCook`으로 cook에서 제외된다. 실게임 데이터는 이 경로에 두지 않는다.

## 검증 상태

- UE 5.8.2 DIGETHEREditor Win64 Development non-unity 빌드와 Editor 로드 성공. 워크벤치 목록에 연결 상태 버튼 등록과 패널의 미연결 표시를 확인했다.
- 연결 상태 모델 자동검사 2개를 포함한 툴킷 자동검사 60개가 통과했다.
- 실게임 데이터 조회·provider 연결·M2/M3는 검증하지 않았다. 연결 상태 패널의 고정 미연결 행을 실제 게임 조회로 해석하지 않는다.
