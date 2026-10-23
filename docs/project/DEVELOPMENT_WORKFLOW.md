# GitHub 개발·파일 관리 흐름

GitHub 사용은 확정, 브랜치·PR·LFS 세부 절차는 초기 운영 제안이다. 원격 설정은 아직 적용하지 않았다.

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
| `.uasset`, `.umap` | Git LFS 및 단일 편집자 운영 제안 |
| 모델·텍스처·오디오 제작 원본 | 권리 확인 후 LFS 패턴·크기 기준·보관 경로 결정 |
| Binaries, Intermediate, Saved, DerivedDataCache | 생성물 제외. 기존 `.gitignore` 유지 |
| 플러그인의 필수 바이너리·Build 필수 자원 | 기존 ignore에 가려지는지 개별 검사 후 예외 결정 |
| 엔진 설치물·개인 설정·비밀값 | 저장소에 포함하지 않음 |

현재 `.gitignore`는 `Build/*`, `Plugins/**/Binaries/*`, `SourceArt` 일부를 제외한다. 전역 `*.obj`, `*.dll`, `*.lib`, `*.a`, `*.so`, `*.exe`와 `*_BuiltData.uasset`, `.vsconfig`도 제외하므로 ThirdParty SDK·필수 빌드 데이터·공유 환경 설정이 누락되는지 확인한다. 플러그인/빌드/원본 자산을 추가할 때 `git check-ignore -v <실제경로>`로 필수 파일 누락을 검사한다. `*.obj`는 Wavefront OBJ 모델도 제외할 수 있으므로 아트 원본 경로의 예외를 기술 담당이 확인한다. 개인 BMAD 설정·Python 캐시 제외는 [스킬 관리](SKILLS.md)에 설명하며, 이 문서는 바이너리 추적 설정이 완료됐다고 가정하지 않는다.

LFS 도입 시 프로젝트 `.gitattributes`를 함께 관리한다. 다음은 **미실행 제안 명령**이며 최초 바이너리 추가 전에 실제 지원·저장 용량·팀 권한을 확인한다.

```powershell
git lfs install --local
git lfs track "*.uasset" --lockable
git lfs track "*.umap" --lockable
git lfs ls-files
```

`--lockable` 속성만으로 팀 전체의 편집 충돌 방지가 검증되는 것은 아니다. 실제 잠금·해제와 권한 실패를 시험하고 담당 예약 절차를 병행한다. 기존 기록의 LFS 이관은 히스토리를 바꿀 수 있으므로 별도 계획으로 다룬다. 이번 작업에는 에셋 업로드·히스토리 이관이 없다.

근거: [GitHub의 LFS 구성 안내](https://docs.github.com/en/repositories/working-with-files/managing-large-files/configuring-git-large-file-storage), 2026-10-23 확인. Unreal 파일 분류와 협업 절차는 이 프로젝트의 제안이다.
