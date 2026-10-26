# 현재 상태와 인수인계

## 2026-10-27 기본 브랜치와 PR

- `develop`과 `feature/minsung`의 원격 생성·푸시·upstream 연결을 완료했다. GitHub 기본 브랜치, 현재 작업 사본의 `origin/HEAD`와 저장소 로컬 `init.defaultBranch`는 `develop`으로 재조회했다.
- [PR #1](https://github.com/inha-SquareOne/DIGETHER/pull/1): `feature/minsung` → `develop`, 생성 후 OPEN·충돌 없음 확인. 병합과 브랜치 보호는 수행하지 않았다. 작업 브랜치는 `feature/minsung`을 유지한다.
- LFS 객체 755개(약 141MB) 원격 업로드 완료. `git lfs fsck`, PR diff 공백 검사, 변경 Markdown 18개의 로컬 링크 136개(누락 0개), session-handoff 스킬 사본 일치 확인을 통과했다.
- 기존 원격 main 이력은 유지했다. 이번 PR 준비에서 엔진 빌드·PIE·다인·패키징은 실행하지 않았다.

## 2026-10-27 Unreal MCP와 로컬 변경

- [MCP 연결 안내](UNREAL_MCP_SETUP.md): Claude·Codex 프로젝트 설정, 실행 에디터 서버 시작 및 자동 시작 설정 저장·재조회 완료.
- 직접 HTTP 도구 호출과 Splash 2개 에셋 로드·저장 상태 조회 성공. Codex 설정 인식, Claude 프로젝트 서버 승인 대기.
- 기존 DX11·반사/조명/그림자 설정 변경과 Splash 이미지·에셋을 사용자 요청으로 커밋한다. SecurityToken은 공유 설정에서 제외했다.
- 에디터 재시작·빌드·PIE·다인·패키징 미검증. 기존 GameFeatureData 등록 관련 로드 오류는 별도 진단 필요.

기준일: 2026-10-27. 이번 문서 준비 상태이며 게임 기능 상태와 구분한다. 이 파일은 시점이 명시된 인계 요약이며 개발 완료 판정 원본이 아니다. 개발 상태는 해당 GitHub Issue/PR과 검증 증거를, 제작 단계 수용은 증거가 연결된 Notion 마일스톤을 참조한다. 이번 공유 변경은 [PR #1](https://github.com/inha-SquareOne/DIGETHER/pull/1)과 문서 검증 기록을 근거로 한다.

| 영역 | 상태 | 근거·다음 행동 |
|---|---|---|
| 역할별 안내 | 로컬 작성 | [기획·아트·개발 가이드](../guides/README.md), [GitHub 첫 사용](../guides/GITHUB_BEGINNER.md) |
| 문서·정책 정리 | 중복·충돌 보정 | [통합 점검](reviews/2026-10-23-document-skill-audit.md). 확정/제안/미결·실제 실행 구분 |
| Notion 기획 | 가져오기 초안 준비 | 워크스페이스·페이지 URL 등록 및 팀 검토 필요 |
| Notion 조회 스킬 | 로컬 배치 | `notion-read-planning` 추가. 실제 연결·페이지 접근 후 본문 조회 필요 |
| GitHub | develop 기본 브랜치·PR 생성 완료 | [PR #1](https://github.com/inha-SquareOne/DIGETHER/pull/1), feature/minsung → develop. Issue·보호 규칙·병합 미실행 |
| 엔진 | 조사 PC의 5.8.2 설치 메타데이터·관련 헤더 확인 | 팀 설치 방식·SDK·실행·플러그인 조합은 미검증 |
| 개발반 설계 | 요구별 설계·엔진 재사용 안내 작성 | [시스템 설계](DEVELOPMENT_SYSTEM_DESIGN.md), [기능 경계](UNREAL_FEATURE_COVERAGE.md), [검증 기록](reviews/2026-10-25-development-design.md). 기획 승인·기술 채택·게임 실행은 별도 |
| 게임 프로젝트 | 기존 로컬 Third Person 템플릿을 버전 관리에 추가 | `DIGETHER/DIGETHER.uproject`, Source·Config·Content. 게임 기능 구현·실행 검증과 구분 |
| Git 파일 관리 | 중첩 Unreal 생성물 제외·에셋 LFS 적용 | `.gitignore`·`.gitattributes`. 로컬 무결성 검사·LFS 755개 원격 업로드 완료, 잠금·다른 PC 다운로드 미검증 |
| 게임 검증 | 미실행 | 빌드·Editor·다인·패키징 증거 없음 |
| 스킬 | 총 84종, 두 도구 경로 동기화 | [관리 목록](SKILLS.md). 설치와 실제 게임 워크플로 실행 구분 |

## 다음 작업

1. 기획 담당이 [Notion 문서](notion/README.md)를 가져오고 원본 URL·검토 상태를 등록한다.
2. [결정 대기](notion/DECISIONS.md)에서 플랫폼·네트워크 방식·솔로 구출·경제 경계 정책을 정한다.
3. 개발·통합·리뷰 담당과 GitHub 계정을 연결하고 LFS 원격 사용·에셋 잠금과 편집 절차를 확인한다.
4. 기존 `DIGETHER/DIGETHER.uproject`를 UE 5.8.2로 빌드·로드하고 첫 맵 `/Game/ThirdPerson/Lvl_ThirdPerson`의 실행 결과를 확인한다.
5. 기술 책임자가 [시스템 설계](DEVELOPMENT_SYSTEM_DESIGN.md)를 검토하고, 10절의 ADR 후보 순서대로 채택 근거를 만든다.
6. 같은 실험 맵에서 지형 후보를 비교하는 M1 기술 검증부터 진행한다. 아직 빌드나 플레이테스트가 성공했다고 판단하지 않는다.

후속 작업은 `git status`와 diff를 먼저 확인한다. 이번 작업에서 열어 둔 게임 Editor나 에셋 잠금은 없다.

## 2026-10-27 에이전트 역할 호출 점검

- [역할 7종](../AGENTS_OVERVIEW.md)의 Claude·Codex 복사본이 카탈로그·원본 본문과 일치한다(이름·설명·도구·권한·본문·링크). 새 역할은 만들지 않았다.
- Claude Code: 7종 모두 실제 호출 성공. 각 역할 제목으로 응답했고 도구가 카탈로그 허용 목록과 같았다.
- Codex 0.160.1: `ue_reviewer` 하위 에이전트 실제 생성 성공(하위 스레드 `agent_role` 확인). 나머지 6종은 Codex에서 개별 호출하지 않았다. `--ephemeral` 사용 시 생성 실패는 [통합 안내](../INTEGRATION.md)에 기록했다.
- 실제 Unreal 작업 위임(빌드·리뷰 결과 품질)은 미검증이다.

## 2026-10-27 파일 관리 검사

- 제외/포함 경로 20건 통과, 변경 문서 로컬 링크 101개 누락 없음.
- 프로젝트 847개 파일 중 에셋 753개: staged LFS 포인터·작업 파일·로컬 객체 SHA-256 및 크기 일치.
- 문서·Git 설정 공백 검사 통과. 기존 Unreal 템플릿의 공백 경고 101건은 원본 보존하며 기능 결함 판정과 구분한다.
- Android File Server 토큰은 Git 제외 로컬 설정으로 이동. 공유 설정에서 제거 확인.
- 엔진 빌드·Blueprint 컴파일·에셋 로드·PIE·다인·패키징 및 원격 LFS 업로드는 미실행.

## 2026-10-27 커밋 규칙 도입

- [커밋 규칙과 Gitflow](../commit-convention.md)를 사용자 요청으로 채택했다. 한국어 제목·본문, 공동 작성자 트레일러 제외를 적용한다.
- 로컬 develop을 생성했으며 작업 브랜치는 사용자 지정에 따라 `feature/minsung`으로 정정했다. 원격 브랜치 생성·보호·푸시·PR·병합은 미실행이다.
- 기존 Unreal 설정 두 파일의 미커밋 변경은 이번 문서 커밋에 포함하지 않는다.

- 사용자 정정: feature는 사람 이름을 쓰며 AI가 주제별로 임의 생성하지 않는다. 기존 커밋 메시지는 사용자 요청으로 전체 한국어화하며 파일 내용과 미커밋 작업을 보존한다. 원격 이력 변경은 미실행이다.

## 작업 기록과 전달 문서

작업·인수인계 기록은 로컬 `.claude/docs/pointer.md`에서 찾는다. `.claude/docs/`는 Git 제외 대상이므로 다른 PC에는 기록이 없을 수 있다. 작성 위치·시점·상태 갱신은 [인수인계 기준](../ai/SESSION_HANDOFF.md)을 따른다.

## 세션 전환 스킬

`session-handoff`를 Claude·Codex 경로에 동기화했다. 목표 변경 또는 확인된 현재 컨텍스트 70% 이상에서 write-handoff 작성·사용자 동의·새 세션 재개를 안내한다. 두 스킬 형식 검사와 사본 동일성 확인을 통과했다. 실제 사용량 신호·새 세션 생성은 미검증이며 도구 지원이 없는 경우 제한을 알린다.

## 응답 방식

[질문·답변과 결과 보고 기준](../ai/RESPONSE_STYLE_GUIDE.md)을 추가했다. Claude·Codex 공통으로 결론을 먼저 말하고 필요한 결정·근거·검증·한계를 이어 설명한다. 진입 지침과 실행 정책의 보고 순서를 연결했다.
