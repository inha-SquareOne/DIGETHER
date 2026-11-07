# 프로젝트 프로필

기준일: 2026-11-06. 사용자 결정·게임 기획·저장소 관찰을 구분한다. 환경 변경 시 갱신하며 실제 빌드/환경 책임자는 미정이다.

| 항목 | 현재 값·근거·상태 |
|---|---|
| 프로젝트·팀 | DIGETHER / Team Square One |
| 목적 | 1~4인 협동 굴착 호러. 상세 요구는 [기획](../GAME_DESIGN.md) |
| 엔진 | **Unreal Engine 5.8.4**, 사용자 명시 결정(2026-11-03, [ADR-003](../decisions/ADR-003-engine-version-5.8.4.md)). 작업 PC의 Build.version에서 5.8.4 / CL 58902052 확인. 이전 기준 5.8.2(CL 56702186)의 조사·검증 기록은 그 시점 사실로 보존한다. 로컬 실행 증거와 팀 전체 환경·호환성은 구분한다. [조사 범위](../design/UNREAL_FEATURE_COVERAGE.md) |
| 엔진 배포 방식 | Launcher / 소스 빌드 중 미결 |
| 프로젝트 파일·모듈 | `DIGETHER/DIGETHER.uproject`, 런타임 모듈 `DIGETHER`, `DIGETHER`/`DIGETHEREditor` 타깃. Source·Config·Content 존재. Third Person 템플릿 및 Combat/Platforming/SideScrolling 변형 포함 |
| 개발·출시 환경 | 이번 문서 작업은 Windows에서 수행. 출시 방향은 **Steam 판매**(2026-10-29 사용자 결정). 지원 OS·SDK·컴파일러와 다른 플랫폼 출시는 미정 |
| 코드·플러그인 | 템플릿 C++/Blueprint, `DevelopmentWorkbench` Core와 `DigetherWorkbench` Editor 어댑터 존재. descriptor에 ModelingToolsEditorMode·StateTree 계열·MCP·CommonUI·EOSVoiceChat 등이 명시돼 있다. Editor 전용 편의 플러그인 9개 적용·mount·실제 활성 상태, 두 Workbench 모듈 로드와 허브 표시·새로고침·바로가기 7종의 실제 UI를 확인했다. [개발 도구 안내](../tools/DEVELOPER_TOOLS.md)를 따르며 게임 시스템·제공자 채택은 별도다 |
| 네트워크 | 제공된 기획은 1~4인 호스트형 협동을 명시. 사용자 요청으로 프로토타입 외부망 시험에 Steam App ID480·내장 Steam 로비/SteamSockets를 적용한다. [시험 안내](../prototype/PROTOTYPE_STEAM.md). 출시는 Steam으로 정했고 정식 App ID·세션 서브시스템(Steam/EOS)·음성 제공자·Steam Cloud는 미결 |
| 후입·이탈·저장 | 시스템 작업리스트 1.1.0: 신규 참가는 정상 거점에서만(출발 게이지 완료~결과 크레딧 차단, 관전 합류), 거점 이탈 시 캐릭터 제거·장비 바닥, 탐사 중 끊김은 캐릭터 유지·재접속 복원, 호스트 이탈 시 방 종료, 수동 저장 없음·완료 시점 자동 저장·저장 슬롯 여러 개. 같은 ID는 이전 접속 종료 후 새 접속 수락([사용자 결정 2026-11-05](../planning/DECISIONS.md#사용자-결정-2026-11-05)). 11-06 전달 [기획 답변](../planning/PLANNING_RESPONSE_2026-11-06.md)은 문 닫힘 후 로딩 중 끊겨도 출발 인원·캐릭터 유지, 크레딧 이탈자의 새 게임 복귀는 신규 참가로 명시했다. 저장은 탐사 정상 종료·목표 달성·호스트 정상 거점 종료·5일차 실패 귀환의 4시점이며 시설 거래마다 저장하지 않는다. 거점 바닥 물품은 종류·수량·위치·방향 저장, 끼임·맵 이탈은 불러올 때 엘리베이터 안으로 복구가 요구다. 팀장 결정·팀 승인 전이며 일반 바닥 물품 저장·복구 구현과 실제 다인 검증은 별도. Notion 승인과 저장 형식 최종 확정은 미결 |
| 소스 관리 | GitHub: https://github.com/inha-SquareOne/DIGETHER (로컬 origin 확인) |
| 브랜치 | [Gitflow 채택](../COMMIT_CONVENTION.md). 로컬 main·develop 및 feature 작업 브랜치 운영. 원격 추적 HEAD는 origin/main이며 서버 보호 규칙·리뷰 계정은 미확인 |
| 기획 관리 | Notion 사용은 사용자 확정. 워크스페이스·페이지 URL 미등록 |
| 대용량 파일 | 2026-11-06 사용자 선택으로 LFS를 쓰지 않고 로직 폴더 에셋은 Git, 아트·맵·제작 원본은 팀 PC SVN, CI는 같은 PC의 Jenkins([ADR-004](../decisions/ADR-004-git-code-svn-assets.md)). Git 쪽 설정 로컬 적용·미커밋, SVN 첫 등록 미실행. 이전: 2026-10-27 사용자 승인으로 `.uasset`·`.umap` Git LFS 설정. 기존 LFS 기록은 보존. SVN 서버 개설(사용자 보고). [개발 흐름](DEVELOPMENT_WORKFLOW.md#파일-분류-git과-svn) |
| 제작 방향 | Cartoon Rendering + Low Poly. 구체 예산·성능 목표·최소 사양 미정 |
| 검사 | 2026-10-28 Editor 기본/강제 Unity·Game 빌드 성공, 설정 격리 후 최종 Automation 64개 성공(경고 있는 테스트 2개), DataValidation 대상 3개 valid·경고/invalid/검사 불가 0개를 확인했다. 독립 비교에서 두 실행 전후 원본 사용자 설정 SHA-256이 같았다. [최신 결과와 한계](../tools/DEVELOPER_TOOLS.md#확인-결과와-남은-검증)를 따르며 2026-10-27의 [과거 범위](../../DIGETHER/Plugins/DevelopmentWorkbench/README.md#확인한-범위와-한계-2026-10-27)와 구분한다. Live Coding 재등록·실제 저장/복원·게임 M2/M3·전체 패키징은 미검증 |
| 소유권 | 기획 역할은 기획 기준, 기술·리뷰·통합·계정 매핑은 [경계표](PROJECT_BOUNDARIES.md)에서 미정 관리 |
| 도구 사본 | 총 스킬 84종(공통 14 + Epic 3 + Blender 22 + BMAD 41 + 신시 이식 2 + Notion 조회 1 + 세션 전환 1), 기존 Unreal 역할 7종. [스킬 목록·검증](../tools/SKILLS.md) 참조 |

개인 계정·키·PC 절대 경로는 팀 환경값으로 쓰지 않는다. [엔진 ADR](../decisions/ADR-003-engine-version-5.8.4.md)과 [결정 대기](../planning/DECISIONS.md)를 함께 갱신한다.
