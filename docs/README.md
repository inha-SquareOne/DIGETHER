# DIGETHER 문서 안내

팀원이 처음 읽을 문서는 [담당별 가이드](guides/README.md)다. 필요한 기준은 한 곳에서 관리하고 다른 문서에서는 연결한다.

| 독자 | 시작 문서 | 주로 하는 일 |
|---|---|---|
| 기획반 | [기획 가이드](guides/planning/README.md) | Notion 기획·변경 요청·완성 결과 확인 |
| 아트반 | [아트 가이드](guides/art/README.md) | 제작·검수·원본과 에셋 전달 |
| 개발반 | [개발 가이드](guides/development/README.md) | 엔진 지원 조사·구현·통합·검증 |
| GitHub 첫 사용자 | [GitHub 첫 사용](guides/GITHUB_BEGINNER.md) | 웹으로 요청하고 Desktop으로 파일 변경 전달 |

## 기준과 상세 문서

| 문서 | 역할 |
|---|---|
| [프로젝트 문서 지도](project/README.md) | 환경·기획·기술·현황의 실제 문서 찾기 |
| [문서 운영](project/DOCUMENTATION.md) | Notion 기획과 GitHub 기술 문서의 원본 경계 |
| [문서 유지](DOCUMENTATION_POLICY.md) | 중복 없이 문서를 추가·수정하는 방법 |
| [문서 배치와 양식](DOCUMENT_MAP.md) | 폴더별 독자·용도, 필요한 양식 선택 |
| [AI 실행 정책](ai/EXECUTION_POLICY.md) | 권한·근본 원인 해결·Unreal 기본 기능 우선 |
| [AI 작업 규칙](ai/README.md) | 검증·분업·인계 등 작업별 상세 절차 |
| [Unreal 기술 기준](unreal/README.md) | 코드·네트워크·에셋·데이터·테스트 기준 |
| [도구 관리](INTEGRATION.md) | 스킬·역할 동기화와 지침 로드 확인 |
| [AI 역할 목록](AGENTS_OVERVIEW.md) | 역할 7개의 범위와 인계 |
| [외부 자료·라이선스](SOURCES.md) | 외부 저작물 고지 |

## 공용화한 스킬

현재 전체 스킬은 [프로젝트 스킬 목록](project/SKILLS.md)에서 관리한다. 기본 작업 스킬 14종은 전체 83종의 일부다. 스킬 파일은 `.agents/skills/`가 관리 원본, `.claude/skills/`가 동일한 동기화 사본이다. 역할별로 필요한 스킬만 선택하며 설치 사실을 도구 연결이나 실행 성공으로 간주하지 않는다.

`templates/`는 작성 양식이다. 자리표시자와 예시는 현재 프로젝트의 결정이 아니며 실제 값은 `project/`에서 확인한다. 기획 내용은 이관 전 초안과 이관 후 Notion 원본을 구별한다.
