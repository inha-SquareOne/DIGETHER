# BMAD 게임 스킬 설치 근거

- 작업일: 2026-10-23. 소스: `D:/shinshi/.agents/skills/`의 `gds-*` 33개와 `bmad-*` 8개, 그리고 `D:/shinshi/_bmad/`의 실행 지원 파일.
- BMAD Core: 6.12.0, https://github.com/bmad-code-org/BMAD-METHOD/tree/v6.12.0
- Game Dev Studio: v0.7.2, https://github.com/bmad-code-org/bmad-module-game-dev-studio/tree/2486f5f5f3b8870baa6cee4615a870c0330f441c
- 버전과 SHA는 원본 설치 manifest의 신고값이다. 로컬 설치본 전체가 upstream과 일치한다고 검증한 것은 아니다. 출발 파일과 설치 파일 SHA-256을 `file-hashes.json`에 기록했다.
- 두 라이선스는 해당 고정 태그/커밋의 LICENSE를 직접 내려받아 `LICENSE-CORE`, `LICENSE-GDS`로 보존했다. 모두 MIT다.

게임 기획·아키텍처·개발·테스트·플레이테스트·스프린트·조사 흐름과 이들이 참조하는 코어 브레인스토밍·검토·도움말·커스터마이징 의존을 함께 선별했다. `skills.json`에 전체 41개 목록을 기록했다. 특정 엔진 전용으로 나누어진 패키지가 아니므로 Unity/기타 엔진 참고 자료도 원본의 일부로 남기되 실행 대상은 Unreal 5.8.2로 설정했다.

`skills/`, `.agents/skills/`, `.claude/skills/`에 동일한 패키지 세트를 배치했다. `_bmad/scripts/`에 uv 기반 설정 해석기와 지원 파일을 설치했다. upstream 설치 manifest의 오래된 `_bmad/core/<skill>`·`_bmad/gds/<skill>` 경로는 실제 `skills/<skill>/SKILL.md`로 보정했다.

프로젝트 설정은 DIGETHER·Unreal·한국어·중립 사용자 기본값으로 생성했다. 신시의 `config.user.toml`, `custom/`, 작업 출력은 복사하지 않았다. 새 `custom/`은 프로젝트 기준을 읽도록 하는 최소 override만 포함한다. 모든 스킬 진입점은 `_bmad/PROJECT_INTEGRATION.md`를 먼저 읽도록 연결했다. BMAD 기획 출력은 `planning-drafts`, 기술 출력은 `implementation-artifacts`로 구분하며 Notion 운영 원본을 자동 대체하지 않는다.

검증: `uv run _bmad/scripts/resolve_config.py --project-root D:/DIGETHER` 성공. customize.toml을 제공하는 39개 스킬의 resolver를 실제 실행하여 프로젝트 override와 Notion 경계를 확인했다. 41개 스킬의 세 설치 경로 전체 파일 일치 및 manifest 대상 파일 존재를 확인했다. 결과는 `validation.json`이다. 초기 검사 스크립트가 resolver JSON의 상위 키를 빠뜨린 검증 코드 오류는 수정 후 전체 재실행했다.

워크플로·에이전트 호출·엔진 실행·커밋·푸시·Notion 게시를 실행하지 않았다. 다음 턴의 런타임 발견 및 실제 작업별 호출은 별도 확인 대상이다. 재현용 `install_local.py`는 대상 `_bmad`가 있으면 덮어쓰지 않고 중단한다.
