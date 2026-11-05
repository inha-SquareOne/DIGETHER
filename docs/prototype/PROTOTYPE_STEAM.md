# Steam 480 외부망 프로토타입 테스트

사용자 요청에 따라 그레이박스 테스트에 Steam App ID **480**을 사용한다. 정식 출시 App ID·플랫폼 정책의 확정은 아니다. 엔진은 UE 5.8.4이며 보이스는 계속 제외한다.

## 연결 방식

내장 OnlineSubsystemSteam 로비와 SteamSockets P2P를 사용한다. Steam 로그인 후 방을 만들고, 같은 방 코드로 찾거나 Steam 친구 초대로 참가한다. 공인 IP 직접 입력이나 같은 LAN은 필요하지 않다. 실제 외부망 성공 여부는 별도 PC·계정의 접속 결과로 판정한다.

480은 여러 개발자가 공유하는 테스트 App ID이므로 `DIGETHER_PROTO=maze-school-v1`, 정확한 방 코드, 빌드 ID로 검색 결과를 검증한다. 방 코드는 비밀번호가 아니며, 같은 코드의 방이 여러 개라면 자동 참가가 선택한 첫 결과를 사용할 수 있으므로 서로 다른 시험에는 새 코드를 사용한다. Steam 검색의 지역 범위 때문에 방이 검색되지 않으면 친구 초대를 사용한다.

Steam 모드에서는 연결의 Steam UniqueId를 플레이어 식별자로 사용하고 임의 `PrototypePlayerId` URL을 우선하지 않는다. SteamAuth 패킷 핸들러도 활성화한다. 식별자 형식 검사만으로 인증·외부 접속 검증이 완료됐다고 판단하지 않는다. 기존 탐사 중 신규 참가 금지·기존 참가자 재접속 규칙은 유지한다.

## 실행

두 PC 모두 Steam에 **서로 다른 계정**으로 로그인하고 동일한 최신 Steam 테스트 패키지를 사용한다. `prototype-04` 등 기존 IP 패키지와 섞지 않는다.

```powershell
# 저장소에서 호스트 실행
./scripts/Launch-PrototypeSteam.ps1 -Mode Host -Room DIGETHER-TEST

# 다른 PC에서 패키지에 포함된 실행기 사용
./scripts/Launch-PrototypeSteam.ps1 -Mode Join -Room DIGETHER-TEST

# 자동 검색 대신 게임을 열고 Steam 초대를 수락할 때
./scripts/Launch-PrototypeSteam.ps1 -Mode Invite -Room DIGETHER-TEST
```

실행기는 `-GameExe`로 패키지의 `DIGETHER/Binaries/Win64/DIGETHER.exe`를 직접 지정할 수도 있다. Steam 프로세스 존재와 시험 App ID를 확인한 뒤 창을 연다. 게임 화면의 **Steam 호스트 열림**과 방 코드를 확인한 후 상대가 참가한다. 단순 프로세스 시작·로비 생성은 게임 서버 접속 준비 완료와 다르다.

친구 초대는 두 사람이 같은 패키지를 실행한 뒤 호스트의 Shift+Tab Steam 오버레이에서 직접 보낸다. 자동 참가 검색은 최대60초이며 실패하면 화면과 로그에 이유를 표시한다. 게임 콘솔 명령은 `SteamHost 방코드`, `SteamFind 방코드`, `SteamJoin 0`, `SteamLeave`다. 호스트 종료 시 참가자 연결도 종료되며 호스트 이전은 제공하지 않는다.

최종 패키지 폴더 전체를 전달해야 한다. exe만 전달하면 안 된다. 테스트용 `steam_appid.txt`는 실행기에서 480으로 준비한다. 정식 Steam 배포 이미지에는 이 테스트 파일을 포함하지 않는다.

## 구현과 로컬 회귀

- `UDigetherPrototypeGameInstance`: 비동기 생성/검색/참가/초대/종료, delegate·타이머 수명과 오류 상태를 관리한다. 실제 SteamSockets NetDriver·NetMode가 확인될 때만 연결 완료를 표시한다.
- `DefaultEngine.ini`: GameInstance·Steam·NetDriver·인증 핸들러를 설정한다. SteamSockets 플러그인은 내장 Steam OSS 의존성을 활성화한다.
- `Launch-Prototype.ps1` 및 기존 자동 다인 실행기는 `-nosteam`을 명시해 Null/IP 회귀 경로를 사용한다. Steam 초기화 실패를 Steam 연결 성공으로 간주하지 않는다.
- Steam 테스트 세이브는 `PrototypeSaveSlot=Steam480`을 사용하며 실행 로그·UserDir는 패키지의 `DIGETHER/Saved/SteamPrototype/` 아래에 둔다.

## 확인할 항목

호스트 로비 생성·SteamSockets listen 시작, 다른 네트워크의 참가와 왕복 조작·상태 복제, 탐사 중 신규 참가 거절, 같은 Steam 계정 재접속, 호스트 종료, 잘못된 방/다른 빌드 거절을 구분해서 확인한다. 한 PC의 하나의 Steam 계정으로 외부망 두 계정 검증을 대신하지 않는다.

2026-10-28 이전 큐브 방식 빌드 `prototype-steam480-02`의 Editor 빌드·Win64 패키징이 성공했다. 개인 Editor 설정 원본 해시도 동일하다. 실제 Steam 로그인으로 방 `DG-452D6C78`을 만들고 `SteamSocketsNetDriver` listen 시작·한국어 호스트 열림 상태를 확인했다. 종료·재생성도 확인했으며 종료 로그에 ensure/Error/Fatal이 없다. Steam SDR 상태는 `avail=OK config=OK anyrelay=OK`다. 이는 호스트 준비 증거이며 다른 PC 참가 성공 증거는 아니다. 사용자는 현재 **호스트와 빌드만 준비**하도록 확정했다.

이전 큐브 방식 전달 파일은 `DIGETHER/Saved/Packages/DIGETHER-Steam480-DG-452D6C78.zip`이다. 전체 압축을 푼 뒤 `Windows/Join-Steam.cmd`를 실행한다. `Host-Steam.cmd`, `Invite-Steam.cmd`, 한국어 접속 안내와 런타임을 포함하며 로컬 로그·세이브는 제외했다. 저장소 실행기와 전달용 실행기는 동일하고 빌드 표식의 exe SHA256을 확인한다.

Opus5.5 xhigh 읽기 전용 검토 2회를 거쳐 실행기 후보·상대 경로, 로컬 검사기 Steam 분리, 세션 인터페이스 종료 수명, SteamSockets 사전 검사 항목을 반영했다. 4인 이동 검사의 공통 대기 차선 충돌도 수정했으며 `prototype-steam480-01`에서 21단계·경합10회·재접속·정산을 통과했다. 이 4인 결과를 최종02에서 실행한 결과로 바꾸어 표시하지 않는다.

이전 steam480-02에서도 `-nosteam` 1인7단계·2인8단계 회귀가 통과했다. 검사 프로세스 모두 exit0·잔류0이며 실제 Steam 호스트와 동시에 실행했다. Steam 외부 참가·초대·Steam ID 재접속 검증을 대체하지 않는다.

## 실제 굴착 전환

`prototype-excavation-03`은 둥근 실제 지형 제거와 광산 구조를 포함하며 Editor 빌드·패키징 및 설정 격리 검사를 통과했다. `excavation-v1`로 방을 구분하므로 이전 steam-v1과 섞이지 않는다. 당시 실행기는 이 패키지를 기본으로 찾았다. 현재 기본 패키지는 아래 갱도·폐교 빌드다. 동일 패키지의 1인 렌더 굴착·복귀·벽 보강, 로컬 4인 10회 동시 요청·동일 ID 재접속과 지형 hash 일치를 확인했다. 새 Steam 호스트 DG-452D6C78도 SteamSockets listen 및 SDR OK까지 확인했다. 외부망 참가 성공을 뜻하지 않는다. 새 전달 파일은 `DIGETHER/Saved/Packages/DIGETHER-Excavation-Steam480-DG-452D6C78.zip`이다.

## 갱도·폐교 최신 전달 빌드

2026-10-29 기본 패키지는 `prototype-maze-school-05`이며 `maze-school-v1`로 이전 굴착 프로토콜과 구분한다. 전달 파일은 `DIGETHER/Saved/Packages/DIGETHER-MazeSchool-Steam480-DG-452D6C78.zip`이다. 전체 압축을 풀고 `Windows/Host-Steam.cmd` 또는 `Join-Steam.cmd`를 실행한다. 한국어 플레이 설명은 `Windows/README_KO.txt`다.

최종 EXE SHA256은 `B0CAAAF597C1C880F919137174EC66327005FA486EA996D4BCE9A5CABED7B619`이다. ZIP SHA256은 `657105AC3E2B241E163252AF4A025BDD0CF732EFB76D37A4A390795901AB9298`이며 로그·세이브·PDB는 제외했다. 잘못 인코딩될 수 있는 한글 파일명 대신 영문 파일명의 한국어 안내를 포함했다.

동일 게임/네트워크 코드의04에서 로컬4인 완주13단계를 통과했고, 포커스 글자색만 수정한05에서 1인 렌더 완주13단계와 굴착 회귀6단계를 확인했다. 이 결과는 Steam 외부망 참가 성공을 뜻하지 않는다. 사용자의 외부망 검증 범위는 호스트·전달 빌드 준비다.

## 공식 근거

- [Epic UE5.8 Steam OSS](https://dev.epicgames.com/documentation/unreal-engine/online-subsystem-steam-interface-in-unreal-engine): 테스트 App ID480, 로비 설정, Steam 클라이언트와 App ID 파일.
- [Epic UE5.8 Steam Sockets](https://dev.epicgames.com/documentation/unreal-engine/using-steam-sockets-in-unreal-engine): P2P 통신·NAT 통과, 전용 NetDriver와 동일 드라이버 빌드 간 연결.

실제 지원 API와 SteamAuth의 비활성화 경로는 설치된 UE5.8.2 엔진 소스에서도 확인했다. 공식 기능 지원과 두 PC의 실제 연결 검증은 별개다.
