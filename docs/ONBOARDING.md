# 환경 준비와 첫 실행

기준일: 2026-10-28. `DIGETHER/DIGETHER.uproject`와 `DIGETHER`/`DIGETHEREditor` 타깃이 있다. 기존 PC의 실행 증거와 신규 참여자의 환경 검증을 구분한다. 환경 값의 원본은 [프로필](project/PROJECT_PROFILE.md), 편의 도구·검사 실행 방법은 [개발 도구 안내](tools/DEVELOPER_TOOLS.md)다.

## 지금 할 수 있는 준비

1. 팀이 부여한 GitHub 저장소 읽기/쓰기 권한을 확인하고 저장소를 복제한다. 이미 복제되어 있으면 새 복제 없이 작업 트리 상태를 확인한다.
2. README → 프로젝트 문서 지도 → Notion 기획 원본/이관 전 초안 → 작업 경계를 읽는다.
3. Unreal Engine 5.8.4와 Windows C++ 컴파일 도구를 준비한다. 팀 배포 방식·SDK 기준은 프로필의 미결 상태를 확인한다. 다른 버전에서 프로젝트를 열어 자동 변환하지 않는다.
4. 에셋 관리 방식([ADR-004](decisions/ADR-004-git-code-svn-assets.md): 로직 폴더는 Git, 아트·맵·제작 원본은 SVN)을 확인하고 SVN 클라이언트(TortoiseSVN)를 설치해 `DIGETHER/Content`·`DIGETHER/SourceArt`를 checkout한다. 절차는 [개발 흐름](project/DEVELOPMENT_WORKFLOW.md#git과-svn-전환-절차)을 따른다. 2026-11-06 기준 SVN 첫 등록은 미실행이다.

아래는 소스 확보 절차이며 이번 작업에서 실행한 clone은 아니다.

```powershell
git clone https://github.com/inha-SquareOne/DIGETHER.git
cd DIGETHER
git status --short
git remote -v
```

## 첫 실행 절차

| 단계 | 필요한 확정값 | 성공 관찰 | 실패 시 확인 |
|---|---|---|---|
| 의존 복원 | 엔진 배포·SDK·플러그인 버전 | 팀 기준과 실제 버전 일치 | 임의 업데이트 대신 버전 차이 기록 |
| 에셋 복원 | Git 로직 폴더·SVN checkout·권한 | 에디터에서 맵·BP가 열림 | `git status`·`svn status`·누락 에셋 경고 |
| 빌드 | `DIGETHEREditor`/`DIGETHER`, UE 5.8.4 루트 | 검사 실행기의 해당 타깃 성공 | 첫 오류와 새 경고, UBT 완료 출력 |
| 프로젝트 열기 | `DIGETHER/DIGETHER.uproject` | Editor·필수 모듈 로드, Output Log 확인 | 엔진 버전·모듈·플러그인 |
| 개발 도구 | 도구 → 개발 워크벤치 열기 | 필요한 도구와 상태 조회 | [개발 도구 안내](tools/DEVELOPER_TOOLS.md)의 적용 상태·한계 |
| 기본 실행 | 첫 맵·플레이 진입 방식 | 시작·종료·재시작 확인 | 기본 맵·초기화·참조 |
| 다인 검사 | 토폴로지·접속 방법 | [검증 계획](verification/VERIFICATION_PLAN.md)의 해당 단계 관찰 | 서버/클라이언트 로그 각각 확보 |

저장소 루트에서 자신의 UE 5.8.4 루트를 지정하고 [검사 실행기](tools/DEVELOPER_TOOLS.md#로컬-검사-실행)의 BuildEditor → BuildGame → Automation → ValidateData 순서로 확인한다. Automation은 빌드를 수행하지 않는다. Editor 모듈을 처음 추가하거나 바꾼 뒤에는 Editor를 닫고 빌드한 다음 다시 연다. 새 PC에서 성공한 명령·환경·로그·확인일을 검증 기록에 남긴 뒤 그 환경의 재현 완료로 표시한다.

## 현재 확인 결과

- 2026-10-23: 로컬 origin·main 및 Git LFS CLI 3.7.1 존재를 확인했다. 이 날짜의 기록은 이후 실행 검증을 대신하지 않는다.
- 2026-10-27: `.uasset`·`.umap` LFS 로컬 설정·객체 확인, Workbench 빌드·자동검사와 Editor 관찰, MCP 읽기 호출 증거가 있다. 세부 범위는 [프로필](project/PROJECT_PROFILE.md)·[Workbench 안내](../DIGETHER/Plugins/DevelopmentWorkbench/README.md)·[MCP 연결](tools/UNREAL_MCP_SETUP.md)을 따른다.
- 2026-10-28: 편의 플러그인 9개 적용·mount·실제 활성 상태, Editor 기본/강제 Unity·Game 빌드, 설정 격리 후 최종 Automation 64개 성공(경고 있는 테스트 2개), DataValidation 대상 3개 valid를 확인했다. 허브 실제 UI와 원본 설정 보존도 확인했다. 후속 실행에서는 전체 Win64 Development 패키지·맵 4개 기동, 같은 PC의 템플릿 1/2/4인 이동·이탈·재접속과 Level Snapshots 디스크 복원을 확인했다. [현재 검사 범위와 재실행 방법](tools/DEVELOPER_TOOLS.md#후속-실기-검증)을 확인한다. Live Coding 재등록·Auto Save 복구·LFS 원격 업로드·잠금·팀 전체 환경·게임 전체 M2/M3는 별도 검증이다.
