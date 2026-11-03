# DIGETHER 문서 안내

사람이 먼저 읽을 핵심 문서는 이 폴더 바로 아래에 있다. 상세 문서는 종류별 하위 폴더에 있다. 같은 기준은 한 곳에만 두고 다른 문서에서는 연결한다.

## 핵심 문서

| 문서 | 내용 |
|---|---|
| [게임 기획서](GAME_DESIGN.md) | 게임 개요·핵심 루프·요구사항(이관 전 초안, 운영 원본은 Notion) |
| [시스템 기획서](SYSTEM_DESIGN.md) | 전체 게임 흐름·시스템 책임·상태 수명·데이터 계약 |
| [아키텍처](ARCHITECTURE.md) | 기술 구조 요약과 상세 설계 연결 |
| [로드맵](ROADMAP.md) | 제작 단계별 범위·진행 기준 |
| [코딩 컨벤션](CODING_CONVENTION.md) | C++·Blueprint·에셋 이름과 코드 작성 규칙 |
| [커밋 규칙과 Gitflow](COMMIT_CONVENTION.md) | 커밋 메시지·브랜치 운영 |
| [온보딩](ONBOARDING.md) | 신규 참여자의 환경 준비·첫 실행 |

담당별 사용 순서는 [기획·아트·개발 가이드](guides/README.md)에서 시작한다. GitHub가 처음이면 [GitHub 첫 사용](guides/GITHUB_BEGINNER.md)을 먼저 읽는다.

## 상세 문서 폴더

| 폴더 | 종류 | 대표 문서 |
|---|---|---|
| [planning/](planning/README.md) | 기획 상세: Notion 이관 안내, 결정 대기, 기능별 기획 | [결정 목록](planning/DECISIONS.md), [기지 기능](planning/BASE_FEATURE_DESIGN.md) |
| design/ | 기술 설계 상세: 구현 계약·코드 구조·기능별 구현 설계·엔진 기능 조사 | [개발반 구현 설계](design/DEVELOPMENT_SYSTEM_DESIGN.md), [코드 구조 계획](design/CODE_STRUCTURE_PLAN.md), [굴착 구현](design/EXCAVATION_IMPLEMENTATION.md), [에디터 편의 설계](design/EDITOR_PRODUCTIVITY_DESIGN.md), [UE 기능 활용 범위](design/UNREAL_FEATURE_COVERAGE.md) |
| decisions/ | 기술·운영 결정 기록(ADR) | [도구 분담](decisions/ADR-001-document-ownership.md), [엔진 버전](decisions/ADR-003-engine-version-5.8.4.md) |
| prototype/ | 프로토타입 구성·실행·시험 계약 | [그레이박스 프로토타입](prototype/PROTOTYPE.md), [Steam 외부망 시험](prototype/PROTOTYPE_STEAM.md), [미로·폐교 흐름](prototype/MAZE_SCHOOL_FLOW.md) |
| verification/ | 검증 계획과 과거 검증 기록(reviews/) | [검증 계획](verification/VERIFICATION_PLAN.md) |
| tools/ | 개발 도구·외부 연결·스킬 | [개발 도구](tools/DEVELOPER_TOOLS.md), [Unreal MCP](tools/UNREAL_MCP_SETUP.md), [스킬 목록](tools/SKILLS.md) |
| [project/](project/README.md) | 프로젝트 운영 정보: 환경 프로필·작업 경계·개발 흐름·문서 운영·자산 대장 | [프로필](project/PROJECT_PROFILE.md), [작업 경계](project/PROJECT_BOUNDARIES.md), [문서 운영](project/DOCUMENTATION.md) |
| [unreal/](unreal/README.md) | Unreal 공통 기술 기준(프로젝트 무관) | [표준 엔진 기능](unreal/STANDARD_FEATURES.md), [네트워크 권위](unreal/NETWORK_AUTHORITY.md) |
| [ai/](ai/README.md) | AI 작업 실행·검증·인계 규칙과 역할 원본 | [AI 실행 정책](ai/EXECUTION_POLICY.md), [응답 방식](ai/RESPONSE_STYLE_GUIDE.md) |
| [guides/](guides/README.md) | 기획·아트·개발 역할별 사용 안내 | — |
| common/ | 여러 프로젝트가 함께 쓰는 문서 체계·도구 관리·출처 | [문서 유지](common/DOCUMENTATION_POLICY.md), [문서 배치와 양식](common/DOCUMENT_MAP.md), [도구 관리](common/INTEGRATION.md), [AI 역할 목록](common/AGENTS_OVERVIEW.md), [외부 자료·라이선스](common/SOURCES.md) |
| templates/ | 새 문서 작성 양식(예시 값은 프로젝트 결정이 아님) | [프로필 양식](templates/PROJECT_PROFILE_TEMPLATE.md) |

## 문서를 추가할 때

- 사람이 처음부터 읽어야 하는 문서만 이 폴더 바로 아래에 둔다. 나머지는 위 표의 종류에 맞는 폴더에 넣는다.
- 새 폴더를 만들기 전에 기존 폴더에 맞는 곳이 있는지 확인한다. 문서를 옮기면 링크를 함께 고치고 아래 검사를 돌린다.
- 작성·갱신 원칙은 [문서 유지](common/DOCUMENTATION_POLICY.md)를 따른다.

## 스킬

전체 스킬은 [프로젝트 스킬 목록](tools/SKILLS.md)에서 관리한다. 스킬 파일은 `.agents/skills/`가 관리 원본, `.claude/skills/`가 동일한 동기화 사본이다. 역할별로 필요한 스킬만 선택하며 설치 사실을 도구 연결이나 실행 성공으로 간주하지 않는다.

## 로컬 링크 검사

저장소 루트에서 `node scripts/check-doc-links.mjs`를 실행하면 루트 진입 문서와 `docs/`의 Markdown 인라인 링크·제목 앵커를 검사한다. [검사 스크립트](../scripts/check-doc-links.mjs)에 파일·폴더 인자를 주어 범위를 좁힐 수 있다. 코드 예시와 외부 URL은 제외하며 참조형 링크는 검사하지 않는다. 회귀 검사는 `node scripts/test-check-doc-links.mjs`로 실행한다. 스킬·도구별 역할 사본·설치 원본은 예시와 경로 기준이 다르므로 기본 검사 범위에 넣지 않는다.
