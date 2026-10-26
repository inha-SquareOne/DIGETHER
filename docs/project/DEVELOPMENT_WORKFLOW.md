# GitHub 개발·파일 관리 흐름

GitHub 사용은 확정, 브랜치·PR·LFS 세부 절차는 초기 운영 제안이다. 현재 origin은 [프로필](PROJECT_PROFILE.md)을 따른다.

## 작업 한 건의 흐름

1. Notion 요구 ID·페이지 URL·승인 개정 번호·승인자/승인일·수용 조건을 확인한다. 이관 전이면 기획 초안의 출처와 미결을 명시한다.
2. GitHub Issue에 목적, 제외 범위, 담당, 쓰기 경로, 수용 조건, 검증 수준, 관련 기획·결정을 적는다.
3. 기준 브랜치 상태를 확인하고 `feature/<issue>-<topic>`, `fix/<issue>-<topic>`, `docs/<topic>` 등 작업 브랜치를 사용한다는 안을 팀에서 채택한다.
4. 작은 단위로 변경하고 에셋 편집 예약/잠금을 확인한다. 공통 계약 변경은 소비 영역과 함께 검토한다.
5. [검증 계획](VERIFICATION_PLAN.md)의 필요한 검사를 수행한다. 미실행을 구분한다.
6. PR에 문제·동작 변화·기획 개정·실제 검사·미검증·스크린샷 필요 여부를 기록한다. 지정 리뷰어와 통합 담당 확인 후 병합한다.
7. Issue의 개발 상태와 Notion 기획 수용 결과를 링크로 연결한다. 기준 변경·진행 상태를 갱신한다.

실제 GitHub Issue/PR은 아직 생성하지 않았다. 강제 보호 규칙·필수 승인 수·CI는 담당자가 정해야 한다.

## 파일 분류와 LFS 제안

| 대상 | 관리 방향 |
|---|---|
| `.uproject`, Source, Config, 텍스트 문서 | 일반 Git |
| `.uasset`, `.umap` | Git LFS 적용, 단일 편집자·원격 잠금 운영은 별도 합의 |
| 모델·텍스처·오디오 제작 원본 | 권리 확인 후 LFS 패턴·크기 기준·보관 경로 결정 |
| Binaries, Intermediate, Saved, DerivedDataCache | 생성물 제외. 기존 `.gitignore` 유지 |
| 플러그인의 필수 바이너리·Build 필수 자원 | 기존 ignore에 가려지는지 개별 검사 후 예외 결정 |
| 엔진 설치물·개인 설정·비밀값 | 저장소에 포함하지 않음 |

2026-10-27 사용자 승인으로 `.gitattributes`에 `*.uasset`, `*.umap` LFS 패턴을 적용했다. 각 개발 PC에서 Git LFS 설치 후 복제/체크아웃한다. 원격 업로드·저장 용량·권한·잠금은 아직 검증하지 않았다. 이번 작업은 로컬 커밋이며 푸시는 수행하지 않는다.

`.gitignore`는 프로젝트·플러그인 깊이와 무관하게 Binaries, Intermediate, Saved, DerivedDataCache와 IDE 캐시·생성된 솔루션을 제외한다. Build 필수 자원, SourceArt, `.vsconfig`, 맵 BuiltData는 추적 가능하게 유지한다. 전역 바이너리 확장자 제외를 없애 ThirdParty SDK·OBJ 원본이 숨지 않게 했다. 필수 사전 컴파일 플러그인을 도입할 때는 해당 Binaries 경로에 명시적 예외를 추가한다. `git check-ignore -v <실제경로>`로 누락 여부를 검사한다.

Android File Server의 개인 SecurityToken은 공유 DefaultEngine.ini에 넣지 않고 Git에서 제외되는 Saved/Config/WindowsEditor/Engine.ini에서 관리한다.

`--lockable` 속성만으로 팀 전체의 편집 충돌 방지가 검증되는 것은 아니다. 실제 잠금·해제와 권한 실패를 시험하고 담당 예약 절차를 병행한다. 기존 기록의 LFS 이관은 히스토리를 바꿀 수 있으므로 별도 계획으로 다룬다. 에셋은 로컬 LFS 객체로 저장하며 원격 업로드·히스토리 이관은 수행하지 않았다.

근거: [GitHub의 LFS 구성 안내](https://docs.github.com/en/repositories/working-with-files/managing-large-files/configuring-git-large-file-storage), 2026-10-23 확인. Unreal 파일 분류와 협업 절차는 이 프로젝트의 제안이다.
