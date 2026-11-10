# DIGETHER 프로젝트 문서

기준일: 2026-10-28(Notion 서술은 2026-11-10 정정). DIGETHER 프로젝트 문서다. **기획 근거 확인**, **제안**, **미결**, **실행 검증**을 구분한다.

## 담당별 읽기

처음 참여하면 [기획·아트·개발 안내](../guides/README.md)에서 자기 담당을 고른다. 기획은 Notion을 중심으로, 파일 제작·개발은 GitHub를 중심으로 진행한다. GitHub에 익숙하지 않으면 [첫 사용 가이드](../guides/GITHUB_BEGINNER.md)를 먼저 읽는다. 역할별 안내는 아래 원본 문서를 연결하며 내용을 복제하지 않는다.

## 이 폴더의 문서(프로젝트 운영 정보)

기획서·시스템 기획서·아키텍처 등 핵심 문서와 전체 폴더 구성은 [문서 안내](../README.md)에 있다. 이 폴더에는 프로젝트 운영 정보만 둔다.

| 문서 | 용도·갱신 계기 | 운영 원본 |
|---|---|---|
| [프로젝트 프로필](PROJECT_PROFILE.md) | 확정 환경·소스 관리·미정 설정 | GitHub |
| [작업 경계](PROJECT_BOUNDARIES.md) | 작성·리뷰·통합·동시 편집 범위 | GitHub |
| [개발 흐름](DEVELOPMENT_WORKFLOW.md) | Notion 요구 → 구현 → PR → 검증 | GitHub |
| [문서 관리](DOCUMENTATION.md) | Notion/GitHub 단일 원본·충돌 해결 | GitHub |
| [자산·의존성 대장](ASSET_REGISTER.md) | 외부 패키지·에셋 출처와 사용권 | GitHub |
| [출처와 도입 범위](SOURCES.md) | 외부 안내·도구 출처 | GitHub |
| [기존 현황 기록](STATUS.md) | 기존 상태 기록 참고(보존용, 갱신하지 않음) | GitHub |

담당자 확정 전에는 문서 작성자가 갱신안을 제시하고 팀이 검토한다. 작성 행위를 승인·권한 부여로 해석하지 않는다.

## 공통 자료와의 관계

[공통 안내](../README.md), [공통 문서 지도](../common/DOCUMENT_MAP.md), [템플릿](../templates/PROJECT_PROFILE_TEMPLATE.md)은 여러 프로젝트에서 재사용하는 기준이다. 참고 예시와 과거 검사 결과는 현재 프로젝트의 적용값이나 검증 결과가 아니다. 실제 적용값은 위 프로젝트 문서에서 찾는다.

Notion에는 개발 작업 보드가 있다([담당 연결](CODE_OWNERSHIP.md)). 이 보드는 작업 목록이며 기획 원본이 아니다. 기획 원본 페이지의 URL은 이 저장소에 등록돼 있지 않다. 원본 URL이 등록되기 전까지 [초기 기획 파일](../planning/README.md)을 사용하며, 이관 후 이 문서와 [프로필](PROJECT_PROFILE.md)에 실제 URL을 등록한다.
