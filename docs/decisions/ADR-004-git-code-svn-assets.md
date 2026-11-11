# ADR-004: 코드·로직 에셋은 Git, 아트·맵·제작 원본은 SVN

- 상태: 채택, 사용자 선택. **전환 진행 중**: Git 쪽 설정 커밋(2026-11-06), SVN 첫 등록 완료(2026-11-10, r6), 팀원 PC 전환 미확인
- 날짜: 2026-11-06
- 결정: Git LFS 결제 없이 운영한다. 코드·설정·문서와 로직 에셋 폴더는 GitHub(Git, LFS 없음), 그 밖의 Content와 제작 원본은 팀 PC의 SVN으로 관리한다
- 근거: CI/CD 도입 논의 중 사용자가 Git+SVN(A안)을 고르고, 이어서 "BP 폴더 기준"으로 Git·SVN을 나누고 "지금 전환까지 실행"하기로 선택

## 배경

전환 전 `.gitattributes`는 `*.uasset`·`*.umap`을 Git LFS로 추적했다(LFS 파일 935개, `DIGETHER/Content` 약 144MB). 사용자는 LFS 유료 요금제를 쓰지 않기로 했다. 비교한 다른 방식은 다음과 같다.

| 방식 | 선택하지 않은 쪽의 주된 차이 |
|---|---|
| 전부 SVN | GitHub PR·Gitflow 규칙을 포기해야 함 |
| Content 전체를 SVN | C++와 BP를 두 곳에 커밋해야 하고 Git 브랜치와 BP가 어긋남 |
| Git 유지 + 자체 LFS 서버 | 현재 구조 유지가 쉽지만 LFS 서버 운영 필요 |
| Perforce 무료판 | UE 연동·잠금이 가장 좋지만 새 도구 학습과 인원 제한 |
| 로직 에셋을 접두사(`BP_` 등)로 분리 | 에셋 이동은 없지만 이름 규칙 위반 시 소속이 바뀜 |

## 나누는 기준

에셋을 옮기지 않고, 로직 에셋이 이미 모여 있는 폴더를 Git 폴더로 정했다. 규칙의 원본은 루트 `.gitignore`와 `scripts/svn/Initialize-SvnAssets.ps1`의 `svn:ignore`이며 두 곳을 함께 고친다. SVN 작업 사본은 프로젝트 폴더(`DIGETHER/DIGETHER`) 하나이고, `trunk`의 `svn:ignore *`가 `Content`·`SourceArt` 밖의 파일을 모두 제외한다. 무시된 파일도 직접 Add하면 등록되므로 Add 대상은 두 폴더 안으로 한정한다.

| Git (GitHub, LFS 없음) | SVN (팀 PC) |
|---|---|
| `Source`, `Config`, `.uproject`, `Plugins`(`Plugins/Marketplace` 제외), `Build`, 문서·스크립트 | `Content`의 나머지 전부: `Characters`, `Game/Environment`(시설 외형 BP 포함), `Game/Maps`, `Frontend/Maps`, `Frontend/Textures`, `Prototype`, `ThirdPerson`, `Variant_*`, `LevelPrototyping`, `Splash`, `Collections`, `Developers`, `__ExternalActors__`, `__ExternalObjects__` |
| `Content/Game/Blueprints`, `Content/Game/UI`, `Content/Game/Input`, `Content/Data`, `Content/Input`, `Content/Editor`, `Content/Frontend/UI`, `Content/Frontend/Localization` | `DIGETHER/SourceArt`(제작 원본) |

- 새 게임 로직 BP·WBP·DataAsset·입력 에셋은 Git 폴더에 만든다. 아트 교체 슬롯인 `Game/Environment/Hub/Blueprints`의 외형 BP는 아트팀이 고치므로 SVN에 둔다.
- 템플릿·프로토타입 폴더(`Variant_*`, `ThirdPerson`, `Prototype`, `LevelPrototyping`)의 BP는 SVN에 남는다. 게임 로직으로 쓰게 되면 Git 폴더로 옮긴다.
- 에디터에서 에셋을 Git 폴더와 SVN 폴더 사이로 옮기면 소속 저장소가 바뀐다. 옮긴 뒤 리디렉터 정리까지 양쪽에 커밋한다.
- 맵은 SVN에 두어 잠금(`svn:needs-lock`)으로 동시 편집을 막는다. Git 쪽 에셋은 잠금이 없으므로 편집 예약으로 관리한다.

## 유료 플러그인 예외 (2026-11-11 사용자 결정)

- 유료(Fab·마켓플레이스) 플러그인은 `DIGETHER/Plugins/Marketplace/<플러그인>/`에 두고 SVN으로 관리한다. Git은 루트 `.gitignore`의 `/DIGETHER/Plugins/Marketplace/` 한 줄로 제외한다. 무료·자체 플러그인은 `Plugins/` 바로 아래에 두고 Git으로 관리한다.
- SVN 쪽은 `Plugins` 폴더 자체만 등록하고 `svn:ignore = *`로 그 밖의 하위 폴더를 무시한다. `Plugins/Marketplace`에는 `svn:global-ignores`로 `Intermediate`와 `*.pdb`를 둔다. 따라서 무료 플러그인 추가 시 SVN 작업이 없고, 유료 플러그인 추가 시 `Marketplace`에 넣어 SVN 커밋만 하면 된다.
- 컴파일러 없는 PC에서도 열리도록 유료 플러그인의 `Binaries/Win64`(`.dll`, `UnrealEditor.modules`)는 SVN에 포함한다. 디버그용 `.pdb`는 제외한다.
- 첫 대상은 Blockout Tools 1.52(`Plugins/Marketplace/Blockoute60d8e1bd542V15`)다. 코드·에셋 참조가 마운트 경로 `/BlockoutToolsPlugin/`과 `IPluginManager::FindPlugin()` 기준이라 하위 폴더 이동에 의존하지 않음을 소스로 확인했다. 2026-11-11 SVN r20으로 등록했고(`.pdb`·`Intermediate` 제외), 사용자가 에디터 플러그인 활성화와 `I_School_Map` 참조 유지를 확인했다.
- `I_School_Map` 등 SVN 맵이 이 플러그인 에셋을 참조하므로 플러그인 SVN 커밋을 해당 맵 커밋보다 먼저 또는 함께 한다.

## 버전 짝 맞추기

- 저장소 루트 `content.rev`에 이 Git 커밋이 기대하는 SVN 리비전을 적는다. Jenkins는 이 리비전으로 SVN을 받는다.
- C++·로직 BP가 SVN 쪽 에셋(맵·메시 등) 변경을 필요로 하면 SVN을 먼저 커밋하고, 그 리비전을 `content.rev`에 넣어 같은 Git 커밋에 포함한다.
- SVN은 하나의 줄기(trunk)로 운영하며 `develop` 코드와 맞는 상태를 유지한다. 개인 실험 에셋은 `Content/Developers/<이름>`에 둔다.

## 전환 상태 (2026-11-06)

| 단계 | 상태 |
|---|---|
| `.gitattributes`에서 LFS 제거, `*.uasset`·`*.umap`을 `binary`로 지정 | 로컬 커밋 |
| `.gitignore`에 SVN 쪽 경로 제외 규칙 추가 | 로컬 커밋 |
| SVN 쪽 파일 877개를 Git 추적에서 제거(`git rm --cached`, 디스크 파일 유지) | 로컬 커밋(디스크 파일 유지) |
| Git 폴더의 에셋 70개를 LFS 포인터에서 실제 파일로 저장 | `git add --renormalize`로 Git 추적 에셋 실제 파일 저장 |
| SVN 첫 등록(`scripts/svn/Initialize-SvnAssets.ps1`) | 2026-11-10 실행. 저장소 `DIGETHER`의 `trunk/Content` 873개·`trunk/SourceArt` 11개 등록(r1~r6), 서버 목록에 Git 폴더 파일 없음, `svn:needs-lock` 881개(`.jpg` 2개·`.hlsl` 1개 제외), `content.rev` 6 |
| 작업 사본을 프로젝트 폴더 하나로 변경(2026-11-10 사용자 선택) | `DIGETHER/DIGETHER`를 `trunk`에 연결하고 `trunk`에 `svn:ignore *`를 걸었다(r7, `content.rev` 7). 등록된 `Content`·`SourceArt`만 관리되고 `Config`·`Source` 등은 커밋 후보에 뜨지 않음을 등록 PC에서 확인. 폴더별로 따로 받던 방식은 잘못된 위치 연결 실수가 나서 바꿨다 |
| 팀원 PC 전환 | 미확인. 저장소 주소와 계정은 관리자에게 받는다 |

**순서 주의:** Git의 추적 제거 커밋을 푸시하기 전에 SVN 첫 등록이 끝나고 팀원 전원이 SVN 작업 사본을 만들어야 한다. pull하면 Git이 추적에서 빠진 아트·맵 파일을 디스크에서 지우므로 pull 직후 `svn update`로 복원한다. 전환 전후 커밋 사이의 브랜치 전환도 같은 이유로 SVN 파일을 지우거나 덮어쓴다. 팀원 쪽 절차는 [개발 흐름](../project/DEVELOPMENT_WORKFLOW.md#git과-svn-전환-절차)을 따른다.

기존 LFS 기록은 히스토리 재작성 없이 그대로 둔다. 과거 커밋을 checkout하면 LFS 객체를 받을 수 있어야 하므로 GitHub LFS 저장소는 지우지 않는다.

## 사용자 결정 (2026-11-06)

| 항목 | 결정 |
|---|---|
| SVN 서버 | 팀 PC에 개설 완료(VisualSVN Server, HTTPS 자체 서명 인증서). 2026-11-10 등록 PC에서 접속·커밋 확인. 해당 PC는 상시 가동 |
| SVN 범위 | 위 표의 SVN 열(아트·맵·제작 원본) |
| 분리 기준 | 폴더 기준 |
| 기존 LFS 기록 | 히스토리를 재작성하지 않고 그대로 둔다 |
| CI 도구 | Jenkins, SVN과 같은 팀 PC |
| 전환 | 지금 실행(Git 쪽 로컬 적용까지 완료) |

## 미결

| 항목 | 정해야 할 것 |
|---|---|
| SVN 운영 | 팀원 계정·권한, 백업 위치·담당, 저장소 주소의 공유 문서 기재 여부 |
| Jenkins | UE 5.8.4·빌드 도구 설치, GitHub 연결 방식(외부 접속 알림 또는 주기 확인) |
| 에디터 연결 | 에디터 Revision Control에 SVN 연결 여부(개인 설정 `Saved/Config`라 저장소에 공유되지 않음) |

이 문서는 SVN 서버·첫 등록·CI 동작의 검증이 아니다. 실행 결과는 작업 기록에 따로 남긴다.

## 로컬 커밋 순서 예외 (2026-11-06)

사용자가 남아 있는 unstaged 변경도 커밋하도록 명시하여, SVN 첫 등록 전에 Git 전환 변경을 로컬 커밋했다. content.rev는 SVN 첫 등록 후 실제 리비전으로 작성한다. SVN 등록·팀원 준비 전에는 이 전환 커밋을 푸시하지 않는다. 이 로컬 커밋은 SVN 이관 완료를 뜻하지 않는다.
