# DIGETHER 프로젝트 스킬

2026-10-23 추가 요청을 반영했다. `.agents/skills/`가 관리 원본이자 Codex 발견 경로이고 `.claude/skills/`는 Claude Code용 동기화 사본이다. 총 **83종**이며, 전역 스킬·계정 설정은 바꾸지 않았다. 다음 턴부터 새 스킬을 사용할 수 있고, 목록이 갱신되지 않는 클라이언트에서는 새 프로젝트 세션을 연다.

## 설치 목록

| 묶음 | 수 | 출처·설치 방식 |
|---|---:|---|
| 기존 공통 Unreal 작업 | 14 | 프로젝트 공통 작업 절차. 그중 이슈·인계·Meshy 3종만 신시의 유용한 세부 절차 보강 |
| Epic Unreal | 3 | [공식 저장소](https://github.com/EpicGames/unreal-engine-skills-for-claude-code-plugin), 커밋 `a6aa73ada02a9fb1f5415bec491f9942c519b297` |
| Blender 제작 | 22 | [blender-skills](https://github.com/EL4CTEO/blender-skills), 커밋 `71d031dab8bc6729504d11a78cbe91945401045f` |
| BMAD Game Dev Studio·Core | 41 | 신시 설치본 재사용: GDS 33 + Core 의존 8, Core 6.12.0 / GDS v0.7.2 |
| Notion 기획 조회 | 1 | 프로젝트 전용 `notion-read-planning`: 원문·개정·승인 상태 조회 |
| 신시 VFX 이식 | 2 | `create-game-vfx`, `register-vfx-prewarming`을 Unreal 기준으로 변환 |

Epic/Blender는 skill-installer의 GitHub 설치 스크립트로 커밋을 고정해 원본 폴더를 설치했다. 두 도구 발견 경로에는 지원 파일 전체를 함께 복사했다. Claude 플러그인 marketplace 등록·자동 훅·MCP 서버 설정은 추가하지 않았다. 이번 요청의 스킬은 로컬 파일 방식으로 사용할 수 있다.

## 어떤 스킬을 쓸까

| 작업 | 진입 스킬 |
|---|---|
| Notion 기획·요구사항 가져오기 | [notion-read-planning](../../.agents/skills/notion-read-planning/SKILL.md) |
| 실제 Unreal Editor 조회·조작 | `unreal-mcp` |
| Unreal ToolsetRegistry 도구 제작 | `create-toolset` |
| 엔진 안에 등록하는 Agent Skill 제작 | `unreal-skill` — `.agents/skills` 작성과는 별개 |
| Blender 작업 공통·라우팅 | `blender-core` |
| 모델·UV·리깅·애니메이션 | `blender-hardsurface-modeling`, `blender-sculpting`, `blender-retopology-uv`, `blender-character-rigging`, `blender-facial-shapekeys`, `blender-animation` |
| 재질·베이크·절차 모델·최적화 | `blender-pbr-materials`, `blender-texture-baking`, `blender-geometry-nodes`, `blender-game-optimization` |
| 장면·조명·렌더·환경 효과 | `blender-scene-setup`, `blender-lighting-lookdev`, `blender-rendering`, `blender-simulation`, `blender-time-of-day`, `blender-weather-fx` |
| 내보내기·QA·파이프라인 | `blender-export-pipelines`, `blender-asset-qa`, `blender-pipeline-automation`, `blender-addon-development`, `blender-workflow-recipes` |
| BMAD 다음 작업 선택 | `bmad-help` |
| 기획·서사·UI 초안 | `gds-create-game-brief`, `gds-gdd`, `gds-create-narrative`, `gds-ux` |
| 구조·작업 분해·실행 | `gds-game-architecture`, `gds-create-epics-and-stories`, `gds-create-story`, `gds-dev-story` |
| 테스트·플레이테스트·성능 | `gds-test-design`, `gds-test-framework`, `gds-test-automate`, `gds-test-review`, `gds-playtest-plan`, `gds-performance-test` |
| VFX 제작·도입·게임 화면 검수 | `create-game-vfx` |
| 기존 VFX 준비 목록 등록 | `register-vfx-prewarming` |

BMAD 전체 목록은 [41종 목록](../../provenance/bmad-install/skills.json)에 있다. 새 프로젝트 지침을 생성하거나 스프린트 파일을 작성하는 스킬도 기존 AGENTS·Notion·GitHub 원본 정책을 먼저 따른다. 설치는 실제 설계나 스프린트·테스트 실행이 아니다.

## 실행 조건과 프로젝트 경계

- **Unreal:** 프로젝트 기준은 5.8.2. Epic 문서가 설명하는 ModelContextProtocol·AllToolsets 플러그인의 설치·활성화와 실제 도구 노출은 아직 확인하지 않았다. 현재 `.uproject`도 없으므로 연결/게임 실행 완료를 주장하지 않는다. 스킬의 연결 전제는 실제 도구 조회로 확인한다.
- **Blender:** 번들은 5.2 LTS 이상 기준이다. 현재 자동 탐지·버전 조회 결과는 **5.1.0**이며 빈 백그라운드 프로세스의 환경 보고만 성공했다. 5.2 전용 API·리깅·내보내기 전체 호환은 검증하지 않았다. 작업 시 버전별 지원을 확인하며 설치 Blender를 자동 업그레이드하지 않는다.
- **기존 blend 보호:** upstream `run_in_blender.py`는 기본적으로 `--factory-startup`을 사용한다. 기존 `.blend`를 입력하는 작업은 인자 순서와 실제 로드된 `bpy.data.filepath`·내용을 확인한 뒤 작업 사본에서 실행한다. 이번 환경 보고에는 사용자 파일을 열지 않았다.
- **BMAD:** `_bmad/`의 resolver와 `uv`, Python 3.11 이상이 필요하다. `primary_platform: unreal`은 BMAD의 엔진 분류이며 출시 플랫폼 확정이 아니다. 프로젝트 설정은 Unreal·DIGETHER·한국어로 생성했고 `game_dev_experience: intermediate`는 설치 기본값이지 사용자 숙련도를 확인한 결과가 아니다. 현재 프로젝트에서 검증된 설정과 실제 도구를 우선한다.
- **기획 원본:** BMAD GDD는 승인된 Notion 기획을 참조하는 작업용 사본이다. `_bmad-output/planning-drafts/`는 미승인 기획 초안, `_bmad-output/implementation-artifacts/`는 기술 작업 산출물이다. 자동 게시나 기존 기획 대체는 하지 않는다. 상세는 [BMAD 프로젝트 경계](../../_bmad/PROJECT_INTEGRATION.md).
- **개인 설정:** 신시의 사용자 설정·개인 이름·작업 출력은 이식하지 않았다. `_bmad/custom/*.user.toml` 등 개인 override와 Python 캐시는 Git 제외다.

## 신시에서 가져온 것과 제외한 것

VFX 2종은 시간 변화·판정 경계·원본 보존·실제 게임 캡처·취소/지연/중첩·프리워밍 등록과 실행 구분을 보존했다. Unity MCP, `.meta`/GUID, 프리팹/SO, Mirror, 신시의 보스·색상·씬 경로는 제거했다. 실제 Unreal API나 에셋이 없는 상태에서 임의로 만들었다고 쓰지 않도록 구성했다.

기존 `guided-issue`에는 YAML 필드의 제목·dropdown·checkbox 보존, `write-handoff`에는 미확인 설정의 재확인 방법, `meshy-model-production`에는 실제 AO 베이크 타깃·UV·케이지·local/조립 AO·동일 카메라 비교를 보강했다. 프로젝트 변경을 두 도구 경로에 반영했다.

신시의 동명 8개 일반 스킬을 통째로 덮어쓰지 않았다. DIGETHER의 공통 버전이 이미 일반화되어 있고, 신시 쪽의 Unity·브랜치명·기여자/커밋 관례를 새 프로젝트에 확정할 이유가 없기 때문이다.

## 유지와 검증

외부 스킬은 [커밋·파일 해시](../../provenance/external-skills-lock.json)와 함께 갱신한다. BMAD는 [설치 근거](../../provenance/bmad-install/README.md), 신시 이식은 [변경 근거](../../provenance/shinshi-skills.json)를 따른다. `.agents/skills/`를 수정하면 같은 작업에서 `.claude/skills/`의 본문과 지원 파일도 맞춘다. 두 경로는 깊이가 같으므로 상대 링크를 포함해 동일하게 유지한다.

도입 당시 검사는 [설치 검증](reviews/2026-10-23-skills.md), 이후 정책 정리와 역할별 가이드 검토는 [통합 점검](reviews/2026-10-23-document-skill-audit.md)에 있다. upstream가 자신의 테스트 통과를 적었다는 이유로 이 PC의 엔진·Blender·게임 동작 검증을 대신하지 않는다.

Notion 읽기 스킬은 두 도구 경로에 배치했다. 실제 조회에는 Notion 연결과 대상 페이지 접근 권한이 필요하다. 현재 페이지 URL은 미등록이며 온라인 본문 조회는 수행하지 않았다.

로컬 정책 보정의 변경 파일·해시는 [추가 변경 기록](../../provenance/skill-policy-adjustments.json)에 둔다. 외부 설치 영수증은 당시 기준이며 현재 파일 해시를 대신하지 않는다.
