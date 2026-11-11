# GitHub 개발·파일 관리 흐름

GitHub 사용과 Gitflow 전략은 확정이다. 한국어 커밋·공동 작성자 트레일러 제외·브랜치별 반영 경로는 [커밋 규칙과 Gitflow](../COMMIT_CONVENTION.md)가 단일 기준이다. 원격 보호·리뷰·잠금 세부 설정은 별도 확인한다. 현재 origin은 [프로필](PROJECT_PROFILE.md)을 따른다.

## 작업 한 건의 흐름

1. Notion 요구 ID·페이지 URL·승인 개정 번호·승인자/승인일·수용 조건을 확인한다. 이관 전이면 기획 초안의 출처와 미결을 명시한다.
2. GitHub Issue에 목적, 제외 범위, 담당, 쓰기 경로, 수용 조건, 검증 수준, 관련 기획·결정을 적는다.
3. 기준 브랜치 상태를 확인하고 [Gitflow 규칙](../COMMIT_CONVENTION.md)에 따라 develop에서 사용자가 지정한 `feature/<사람 이름>`를 만든다. 출시 준비는 release, 출시 긴급 수정은 hotfix 흐름을 따른다.
4. 작은 단위로 변경하고 에셋 편집 예약/잠금을 확인한다. 공통 계약 변경은 소비 영역과 함께 검토한다.
5. [검증 계획](../verification/VERIFICATION_PLAN.md)의 필요한 검사를 수행한다. 미실행을 구분한다.
6. PR에 문제·동작 변화·기획 개정·실제 검사·미검증·스크린샷 필요 여부를 기록한다. 지정 리뷰어와 통합 담당 확인 후 병합한다.
7. Issue의 개발 상태와 Notion 기획 수용 결과를 링크로 연결한다. 기준 변경·진행 상태를 갱신한다.

실제 GitHub Issue/PR은 아직 생성하지 않았다. 강제 보호 규칙·필수 승인 수·CI는 담당자가 정해야 한다.

## 파일 분류: Git과 SVN

2026-11-06 사용자 선택으로 코드·로직 에셋은 Git, 아트·맵·제작 원본은 팀 PC의 SVN에서 관리한다. Git LFS는 쓰지 않는다. 결정·배경·전환 상태는 [ADR-004](../decisions/ADR-004-git-code-svn-assets.md)가 단일 기준이다.

| 대상 | 관리 |
|---|---|
| `.uproject`, Source, Config, Plugins(`Plugins/Marketplace` 제외), Build, 텍스트 문서·스크립트 | Git |
| `Plugins/Marketplace/`(유료 Fab·마켓플레이스 플러그인, 필수 `Binaries/Win64` 포함) | SVN. 새 유료 플러그인은 이 폴더에 넣고 SVN 커밋 |
| `Content/Game/Blueprints`, `Content/Game/UI`, `Content/Game/Input`, `Content/Data`, `Content/Input`, `Content/Editor`, `Content/Frontend/UI`, `Content/Frontend/Localization` | Git(바이너리, 잠금 없음 → 편집 예약) |
| 그 밖의 `Content` 전부(맵·메시·텍스처·머티리얼·애니메이션·사운드·VFX·외형 BP·템플릿) | SVN, 바이너리는 `svn:needs-lock` |
| `DIGETHER/SourceArt` 등 제작 원본 | SVN |
| Binaries, Intermediate, Saved, DerivedDataCache | 생성물 제외. `.gitignore` 유지 |
| 플러그인의 필수 바이너리·Build 필수 자원 | ignore에 가려지는지 개별 검사 후 예외 결정 |
| 엔진 설치물·개인 설정·비밀값 | 저장소에 포함하지 않음 |

Git·SVN 경계는 루트 `.gitignore`의 Content 분리 규칙과 [SVN 첫 등록 스크립트](../../scripts/svn/Initialize-SvnAssets.ps1)의 `svn:ignore`가 함께 정한다. 한쪽만 고치지 않는다. 경로 소속은 `git check-ignore -v <경로>`로 확인한다(제외되면 SVN 쪽).

`.gitignore`는 프로젝트·플러그인 깊이와 무관하게 Binaries, Intermediate, Saved, DerivedDataCache와 IDE 캐시·생성된 솔루션을 제외한다. Build 필수 자원과 `.vsconfig`는 추적한다. 필수 사전 컴파일 플러그인을 도입할 때는 해당 Binaries 경로에 명시적 예외를 추가한다.

Android File Server의 개인 SecurityToken은 공유 DefaultEngine.ini에 넣지 않고 Git에서 제외되는 Saved/Config/WindowsEditor/Engine.ini에서 관리한다.

### 커밋 규칙

- C++·Git 쪽 에셋만 바뀌면 Git에만 커밋한다.
- SVN 쪽 에셋(맵·메시 등)도 함께 바뀌면 **SVN을 먼저 커밋**하고, 그 리비전 번호를 루트 `content.rev`에 넣어 같은 Git 커밋에 포함한다. PR 설명에도 SVN 리비전을 적는다.
- SVN은 trunk 하나로 운영하며 `develop` 코드와 맞는 상태를 유지한다. 개인 실험 에셋은 `Content/Developers/<이름>`에 둔다.
- SVN 잠금은 편집 전에 걸고(TortoiseSVN Get Lock 또는 에디터 Check Out), 커밋 뒤 바로 풀린다. 잠금을 건 채 퇴근하지 않는다.

### Git과 SVN 전환 절차

2026-11-06 기준 1단계를 로컬 적용했으며, 사용자 후속 지시로 2단계 전에 Git 전환 변경을 로컬 커밋했다. 2단계 SVN 첫 등록은 2026-11-10에 실행했고, 같은 날 작업 사본을 프로젝트 폴더 하나로 바꿨다(`content.rev` 7). 팀원 전환(4단계)은 확인하지 않았다(ADR-004의 로컬 커밋 순서 예외).

1. **Git 설정(로컬 커밋):** `.gitattributes`에서 LFS 제거, `.gitignore` Content 분리 규칙 추가, SVN 쪽 파일 `git rm --cached`.
2. **SVN 첫 등록:** 최신 Content를 가진 PC에 SVN 명령줄 도구(TortoiseSVN의 command line client tools)를 설치하고 `.\scripts\svn\Initialize-SvnAssets.ps1 -RepoUrl <저장소 주소> -DryRun`으로 명령을 확인한 뒤 `-DryRun` 없이 실행한다. 끝나면 `content.rev`가 생긴다.
3. **Git 커밋:** `git add --renormalize DIGETHER/Content`로 Git 폴더 에셋을 LFS 포인터에서 실제 파일로 바꾸고, `.gitattributes`·`.gitignore`·`content.rev`와 추적 제거를 한 커밋으로 만든다. 아직 푸시하지 않는다.
4. **팀원 준비:** 각 팀원은 Unreal 에디터를 닫고 작업 중인 에셋을 백업한다. 폴더를 옮기지 말고 프로젝트 폴더를 그 자리에서 SVN 작업 사본으로 만든다: `svn checkout <주소>/trunk DIGETHER --force`(TortoiseSVN은 `DIGETHER/DIGETHER` 폴더 우클릭 → SVN Checkout, 주소 끝은 반드시 `/trunk`). `Content`·`SourceArt` 밖의 파일과 Git 폴더는 `svn:ignore`로 제외된다. `svn status`에 `M`으로 보이는 파일은 서버와 다른 로컬 변경이므로 백업과 대조해 커밋하거나 `svn revert`한다.
5. **푸시와 pull:** 전원이 4단계를 마친 뒤 3단계 커밋을 푸시한다. pull하면 Git이 추적에서 빠진 아트·맵 파일을 디스크에서 지운다. **pull 직후 `svn update DIGETHER`를 실행해 SVN에서 복원한다.**
6. **확인:** 에디터에서 맵·BP가 열리는지, `git status`·`svn status`에 의도하지 않은 변경이 없는지 확인한다.

**브랜치 전환 주의:** 전환 전 커밋(아트를 Git이 추적하던 시점)과 전환 후 커밋 사이를 checkout하면 Git이 SVN 쪽 파일을 덮어쓰거나 지운다. 전환 뒤에는 이전 브랜치를 checkout하지 않고, 했다면 돌아온 뒤 `svn revert -R`·`svn update`로 복원한다. 팀원의 다른 feature 브랜치는 전환 커밋을 merge한 뒤 사용한다.

기존 LFS 기록은 히스토리 재작성 없이 둔다. 과거 커밋을 checkout할 때를 위해 Git LFS 설치는 유지해도 된다.
