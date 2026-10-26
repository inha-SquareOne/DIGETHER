# 프로젝트 프로필

기준일: 2026-10-27. 사용자 결정·게임 기획·저장소 관찰을 구분한다. 환경 변경 시 갱신하며 실제 빌드/환경 책임자는 미정이다.

| 항목 | 현재 값·근거·상태 |
|---|---|
| 프로젝트·팀 | DIGETHER / Team Square One |
| 목적 | 1~4인 협동 굴착 호러. 상세 요구는 [기획](notion/GAME_DESIGN.md) |
| 엔진 | **Unreal Engine 5.8.2**, 사용자 명시 결정. 조사 PC의 Build.version에서 5.8.2 / CL 56702186 확인. 팀 환경·실행·호환성 미검증. [조사 범위](UNREAL_FEATURE_COVERAGE.md) |
| 엔진 배포 방식 | Launcher / 소스 빌드 중 미결 |
| 프로젝트 파일·모듈 | `DIGETHER/DIGETHER.uproject`, 런타임 모듈 `DIGETHER`, `DIGETHER`/`DIGETHEREditor` 타깃. Source·Config·Content 존재. Third Person 템플릿 및 Combat/Platforming/SideScrolling 변형 포함 |
| 개발·출시 환경 | 이번 문서 작업은 Windows에서 수행. 팀 지원 OS·출시 플랫폼·SDK·컴파일러 미정 |
| 코드·플러그인 | 템플릿 C++/Blueprint 존재. descriptor에 ModelingToolsEditorMode(Editor), StateTree, GameplayStateTree 활성화. 게임 시스템 설계의 채택과 실행 검증은 별도 |
| 네트워크 | 1~4인 온라인 협동은 기획 근거 있음. 리슨/전용 서버, 세션·음성 제공자는 미결 |
| 후입·이탈·저장 | 후입, 재접속, 호스트 이탈, 세이브·프로필 저장 범위 미결 |
| 소스 관리 | GitHub: https://github.com/inha-SquareOne/DIGETHER (로컬 origin 확인) |
| 브랜치 | 로컬 현재 브랜치 main. 원격 기본 브랜치·보호 규칙·리뷰 계정은 미확인 |
| 기획 관리 | Notion 사용은 사용자 확정. 워크스페이스·페이지 URL 미등록 |
| 대용량 파일 | 2026-10-27 사용자 승인으로 `.uasset`·`.umap` Git LFS 설정. 로컬 필터·객체 검증, 원격 업로드·용량·잠금은 미검증. [개발 흐름](DEVELOPMENT_WORKFLOW.md) |
| 제작 방향 | Cartoon Rendering + Low Poly. 구체 예산·성능 목표·최소 사양 미정 |
| 검사 | Git 제외 규칙·LFS·문서 링크 검사 수행. 엔진 빌드·에셋 로드·PIE·M2/M3는 이번 커밋 정리에서 미실행 |
| 소유권 | 기획 역할은 기획 기준, 기술·리뷰·통합·계정 매핑은 [경계표](PROJECT_BOUNDARIES.md)에서 미정 관리 |
| 도구 사본 | 총 스킬 83종(공통 14 + Epic 3 + Blender 22 + BMAD 41 + 신시 이식 2 + Notion 조회 1), 기존 Unreal 역할 7종. [스킬 목록·검증](SKILLS.md) 참조 |

개인 계정·키·PC 절대 경로는 팀 환경값으로 쓰지 않는다. [엔진 ADR](decisions/ADR-002-engine-version.md)과 [결정 대기](notion/DECISIONS.md)를 함께 갱신한다.
