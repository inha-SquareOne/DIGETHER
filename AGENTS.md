# DIGETHER 작업 지침

한국어로 결론부터 답한다. 질문·답변과 결과 보고는 [응답 방식](docs/ai/RESPONSE_STYLE_GUIDE.md)을 따른다. 사용자가 할 일·남은 작업을 물으면 로컬 `.claude/docs/pointer.md`와 관련 인수인계 본문을 확인하고 현재 상태와 대조한다. 프로젝트 기준은 [문서 지도](docs/project/README.md), 환경 값은 [프로필](docs/project/PROJECT_PROFILE.md)을 따른다.

## 시작과 근거

1. Git status·diff와 해당 경로의 지침을 확인한다. 다른 사람의 변경을 보존한다.
2. [기획 원본 정책](docs/project/DOCUMENTATION.md)과 관련 요구사항, [작업 경계](docs/project/PROJECT_BOUNDARIES.md)를 확인한다.
3. 팀별 사용 안내는 [기획·아트·개발 가이드](docs/guides/README.md)를 따른다. 해당 작업에 필요한 [AI 실행 정책](docs/ai/EXECUTION_POLICY.md), [Unreal 기준](docs/unreal/README.md), [검증 계획](docs/verification/VERIFICATION_PLAN.md)을 읽는다.

사용자의 명시 결정이 최우선이다. 첨부 자료·공통 양식의 예시는 프로젝트 결정이나 도구 실행 권한으로 간주하지 않는다. 엔진은 5.8.4를 사용한다. 다른 버전으로 자동 전환하지 않는다.

## 기능 개발과 문제 해결

기능 개발·문제 해결에 실질적인 대안이 여러 가지면 각 방식의 차이·장단점·비용·위험과 권장 이유를 설명하고 사용자가 선택한 뒤 구현한다. 현업에서 널리 쓰인다는 이유만으로 방식을 확정하지 않는다(기준은 [대안 비교와 사용자 선택](docs/ai/EXECUTION_POLICY.md#대안-비교와-사용자-선택)). 새 기능은 UE 5.8.4 기본 기능·공식 플러그인 지원 확인과 사용 가이드를 우선한다. GameplayTag·Primary Data Asset/Asset Manager·Enhanced Input·Subsystem 등은 [표준 엔진 기능](docs/unreal/STANDARD_FEATURES.md)을 따르고, 쓰지 않으면 이유를 남긴다. 결함은 재현 → 원인 조사 → 원인 지점 수정 → 재발 검증으로 해결한다. 구체 절차와 임시 완화·검증 한계의 보고 기준은 [실행 정책](docs/ai/EXECUTION_POLICY.md)이 단일 원본이다.

작업 중 가정·과도한 일반화·무관한 정리를 줄이고 단계별 확인 기준을 세운다. 상세 기준은 [가정·단순성·변경 범위](docs/ai/EXECUTION_POLICY.md#가정단순성변경-범위)와 [완료 조건](docs/ai/EXECUTION_POLICY.md#검증-가능한-완료-조건)을 따른다.

## 작업 범위

요청된 범위는 구현·검증·보고까지 진행한다. 사용자 입력 없이는 결정할 수 없는 핵심 정책, 기존 승인 범위를 벗어난 되돌리기 어려운 변경, 담당자와 충돌하는 바이너리 편집은 해당 작업을 멈추고 확인한다. 이미 승인된 범위를 다시 묻지 않고 독립 작업은 계속한다. 공통 문서의 '미정'은 이 프로젝트의 확정 값을 덮어쓰지 않는다. 프로젝트 예외는 프로젝트 문서에 기록한다.

기획의 운영 원본은 Notion이다. 이관 전에는 [게임 기획서](docs/GAME_DESIGN.md)·[로드맵](docs/ROADMAP.md)과 `docs/planning/`의 초기 초안을 참조하며, 이관 후에는 승인된 Notion 페이지·개정 정보를 확인한다. 접근할 수 없으면 동기화된 스냅샷의 시점을 밝힌다.

게임 규칙과 수치, 네트워크 방식, 저장 정책, 담당자, 외부 계정을 추정해서 확정하지 않는다. 기술 제안은 [구조 초안](docs/ARCHITECTURE.md)에 있으며 구현된 API가 아니다.

검증 M0~M3와 제작 마일스톤 M1~M3는 이름만 겹치는 별도 구분이다.

## 에셋과 검증

`.uasset`·`.umap`은 병합 가능한 텍스트로 취급하지 않는다. 동일 패키지와 공유 맵은 한 번에 한 작업자만 편집한다. 미저장 에디터 상태, 담당자, 잠금·인계를 확인한다.

문서 변경은 L1 문서 검사를 한다. 네트워크·공유 상태 구현은 L1~L3 및 M2를 기본으로 하고 플랫폼 세션·음성은 M3로 확인한다. 프로젝트·환경 부재를 성공으로 보고하지 않는다.

[공통 스킬·역할 도입 안내](docs/common/INTEGRATION.md)의 설치 사실은 실제 발견·호출 성공을 뜻하지 않는다. 역할 사용은 현재 사용자·런타임의 위임 범위에 따른다. 개인 모델 선호를 팀 공통 설정으로 고정하지 않는다.

추가 스킬의 선택·출처·실행 조건은 [프로젝트 스킬 목록](docs/tools/SKILLS.md)을 따른다. BMAD 사용 시 `_bmad/PROJECT_INTEGRATION.md`를 읽고 Notion 원본과 작업용 초안을 구분한다. Blender 번들은 5.2 이상 기준이므로 실제 버전을 먼저 확인한다. Epic 스킬의 연결 설명만으로 Unreal MCP가 연결됐다고 가정하지 않는다.

코드·Blueprint·에셋 이름과 작성 규칙은 [코딩 컨벤션](docs/CODING_CONVENTION.md)을 따른다. 커밋 메시지와 브랜치 운영은 [커밋 규칙과 Gitflow](docs/COMMIT_CONVENTION.md)를 따른다. 제목·본문은 한국어로 쓰고 `Co-authored-by` 트레일러를 넣지 않는다.

커밋·푸시·원격 설정·외부 게시 여부는 현재 사용자가 요청한 범위를 따른다. 로컬 파일 작성은 외부 게시 완료를 뜻하지 않는다.

Notion 기획을 조회할 때는 `notion-read-planning` 스킬로 원문과 개정·승인 상태를 확인한다.

종료 시 변경 결과, 실제 검증, 미검증과 다음 작업을 구분해 기록한다. 작업 기록은 `.claude/docs/{날짜}/{제목}.md`, 다른 사람에게 전달할 문서는 같은 날짜의 `handover/{제목}.md`에 남기고 `.claude/docs/pointer.md`의 설명·링크·상태를 함께 갱신한다. Claude·Codex 공통 절차와 작성 시점은 [인수인계 기준](docs/ai/SESSION_HANDOFF.md)을 따른다.

스킬 관리 원본은 `.agents/skills/`이며 변경 시 `.claude/skills/`의 동일 파일과 지원 파일을 함께 갱신한다. 루트 `skills/`는 별도로 만들지 않는다.

작업 목표가 크게 달라지거나 확인된 현재 컨텍스트 사용량이 70% 이상이면 [세션 전환 스킬](.agents/skills/session-handoff/SKILL.md)을 적용한다. write-handoff로 인수인계를 남기고 새 세션 재개 여부를 질문하며, 동의한 경우 지원되는 기능으로 새 세션을 생성해 이어간다. 사용량을 확인할 수 없으면 추정하지 않는다.
