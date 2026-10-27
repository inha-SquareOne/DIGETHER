# DIGETHER 프로젝트 문서

기준일: 2026-10-23. DIGETHER 프로젝트 문서다. **기획 근거 확인**, **제안**, **미결**, **실행 검증**을 구분한다.

## 담당별 읽기

처음 참여하면 [기획·아트·개발 안내](../guides/README.md)에서 자기 담당을 고른다. 기획은 Notion을 중심으로, 파일 제작·개발은 GitHub를 중심으로 진행한다. GitHub에 익숙하지 않으면 [첫 사용 가이드](../guides/GITHUB_BEGINNER.md)를 먼저 읽는다. 역할별 안내는 아래 원본 문서를 연결하며 내용을 복제하지 않는다.

## 문서 지도

| 문서 | 용도·갱신 계기 | 운영 원본 |
|---|---|---|
| [기획 문서 가져오기](notion/README.md) | Notion 구성·이관·DB 연결 | 이관 안내는 GitHub |
| [게임 기획](notion/GAME_DESIGN.md) | 요구사항·핵심 루프·콘텐츠·수용 관찰 | 이관 후 Notion |
| [로드맵](notion/ROADMAP.md) | M1~M3 범위·진행 기준 | 이관 후 Notion |
| [결정 대기](notion/DECISIONS.md) | 기획·환경·기술 미결 결정 추적 | 이관 후 Notion, 채택 기술 결정은 ADR 연결 |
| [프로젝트 프로필](PROJECT_PROFILE.md) | 확정 환경·소스 관리·미정 설정 | GitHub |
| [온보딩](ONBOARDING.md) | 신규 참여자의 환경 준비·첫 실행 | GitHub |
| [Unreal MCP 연결](UNREAL_MCP_SETUP.md) | Claude·Codex 설정·승인·실제 연결 검증 | GitHub |
| [기술 구조 초안](ARCHITECTURE.md) | 시스템·상태·네트워크 책임 제안 | GitHub |
| [개발반 시스템 설계](DEVELOPMENT_SYSTEM_DESIGN.md) | DG-001~019 구현 책임·데이터 계약·실패 처리·구현 순서 제안 | GitHub |
| [Unreal 제공 기능과 구현 범위](UNREAL_FEATURE_COVERAGE.md) | UE 5.8.2 근거·기본 기능 사용 가이드·직접 개발 경계 | GitHub |
| [작업 경계](PROJECT_BOUNDARIES.md) | 작성·리뷰·통합·동시 편집 범위 | GitHub |
| [커밋 규칙과 Gitflow](../commit-convention.md) | 한국어 메시지·공동 작성자 제외·브랜치 운영 | GitHub |
| [개발 흐름](DEVELOPMENT_WORKFLOW.md) | Notion 요구 → 구현 → PR → 검증 | GitHub |
| [문서 관리](DOCUMENTATION.md) | Notion/GitHub 단일 원본·충돌 해결 | GitHub |
| [검증 계획](VERIFICATION_PLAN.md) | 변경별 검사와 출시 전 확인 | GitHub |
| [자산·의존성 대장](ASSET_REGISTER.md) | 외부 패키지·에셋 출처와 사용권 | GitHub |
| [기존 현황 기록](STATUS.md) | 기존 상태 기록 참고 | GitHub |
| [도구 분담 결정](decisions/ADR-001-document-ownership.md) | 사용자 결정과 영향 | GitHub |
| [엔진 결정](decisions/ADR-002-engine-version.md) | UE 5.8.2 채택 근거 | GitHub |
| [출처와 도입 범위](SOURCES.md) | 외부 안내·도구 출처 | GitHub |
| [문서 검증 기록](reviews/2026-10-23.md) | 초기 문서 검증 범위 | GitHub |
| [설치한 스킬](SKILLS.md) | Epic·Blender·BMAD·신시 스킬 선택과 실행 조건 | GitHub |
| [스킬 설치 검증](reviews/2026-10-23-skills.md) | 도입 당시 사본·설정 해석·의존성 확인 | GitHub |
| [문서·스킬 통합 점검](reviews/2026-10-23-document-skill-audit.md) | 중복·충돌 정리, 역할별 가이드와 실제 검증 범위 | GitHub |

담당자 확정 전에는 문서 작성자가 갱신안을 제시하고 팀이 검토한다. 작성 행위를 승인·권한 부여로 해석하지 않는다.

## 공통 자료와의 관계

[공통 안내](../README.md), [공통 문서 지도](../DOCUMENT_MAP.md), [템플릿](../templates/PROJECT_PROFILE_TEMPLATE.md)은 여러 프로젝트에서 재사용하는 기준이다. 참고 예시와 과거 검사 결과는 현재 프로젝트의 적용값이나 검증 결과가 아니다. 실제 적용값은 위 프로젝트 문서에서 찾는다.

현재 Notion 페이지 URL은 미등록이다. 온라인 원본이 생기기 전까지 초기 기획 파일을 사용하며, 이관 후 이 문서와 [프로필](PROJECT_PROFILE.md)에 실제 URL을 등록한다.
