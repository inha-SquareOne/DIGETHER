# GitHub 처음 사용하기

GitHub는 팀 파일과 변경 이력을 함께 보관하는 곳이다. **기획만 작성하거나 리뷰할 때는 웹 브라우저만으로 시작할 수 있다.** 파일을 직접 전달할 때는 버튼으로 작업하는 GitHub Desktop을 선택할 수 있다. 터미널 사용은 이 안내의 전제 조건이 아니다.

현재 저장소는 [DIGETHER](https://github.com/inha-SquareOne/DIGETHER)다. 팀 초대·권한, 실제 기본 브랜치와 리뷰 담당은 기술 담당에게 확인한다. 아래 PR 흐름은 팀의 초기 운영안이며 원격 설정이 적용됐다는 뜻은 아니다.

## 여섯 단어

| 화면의 말 | 뜻 |
|---|---|
| Repository(저장소) | 팀 파일과 변경 이력 묶음 |
| Issue(이슈) | 요청·버그·질문 한 건을 논의하는 글 |
| Branch(브랜치) | 다른 작업과 구분해 변경을 준비하는 작업 갈래 |
| Commit(커밋) | 내 컴퓨터에 변경 이력을 남기는 단계 |
| Push(전송) | 내 커밋을 GitHub에 보내는 단계 |
| Pull request, PR | 변경을 팀 기준에 합치기 전에 검토를 요청하는 글 |

파일 저장 → 커밋 → 전송 → PR 검토·병합은 서로 다른 단계다. 전송했다고 팀 기준에 반영된 것은 아니다.

## 웹에서 요청하고 결과 읽기

1. 저장소의 **Issues**에서 비슷한 요청을 찾는다. 없으면 **New issue**를 누른다. 양식이 있으면 해당 양식을 쓰고, 없으면 제목과 본문을 작성한다.
2. 목적·Notion 요구 URL/개정·기대 동작·완료 조건을 적는다. 버그는 실행 순서·실제 결과·발생 환경과 이미지를 붙인다. 비밀번호·토큰·개인 정보는 이미지에서도 가린다.
3. 제출 후 생성된 Issue 링크를 Notion의 해당 요구에 연결한다. 버튼이 없거나 접근이 거절되면 담당에게 권한 확인을 요청한다. 요청 방법은 [GitHub 공식 Issue 안내](https://docs.github.com/en/issues/tracking-your-work-with-issues/using-issues/creating-an-issue)를 참고한다.
4. 담당이 보내 준 PR 링크에서 본문·댓글·검사 결과를 읽는다. 무엇이 달라졌는지, 완료 조건을 실제 확인했는지, 남은 문제가 있는지를 보고 댓글로 전달한다. 코드 차이를 모두 읽어야만 기획·아트 검토를 할 수 있는 것은 아니다.
5. PR의 **Merged**는 변경이 합쳐졌다는 의미다. 게임 기획의 최종 수용 여부는 관련 증거를 확인해 Notion에 별도로 기록한다. **Closed**만 표시된 경우 병합 없이 닫혔을 수 있으니 본문을 확인한다.

## 파일 전달: Desktop으로 첫 작업

기술 담당에게 저장소 권한과 기준 브랜치를 확인한 뒤 진행한다. 아트 파일은 **LFS 추적·용량·경로와 단일 편집 예약이 준비된 뒤** 추가한다. GitHub Desktop 설치만으로 프로젝트의 LFS 정책이 만들어지는 것은 아니다. LFS 준비는 [공식 구성 문서](https://docs.github.com/en/repositories/working-with-files/managing-large-files/configuring-git-large-file-storage)를 기술 담당이 확인한다.

1. 처음 한 번은 GitHub Desktop에서 로그인하고 **File → Clone Repository → URL**에 저장소 주소를 입력한다. **Local Path**로 작업 폴더를 고른 뒤 **Clone**한다. 이미 작업 폴더가 있으면 중복 복제하기 전에 담당에게 확인한다. [공식 복제 안내](https://docs.github.com/en/desktop/adding-and-cloning-repositories/cloning-and-forking-repositories-from-github-desktop)
2. 파일을 바꾸기 전, **Changes**에 보존해야 할 변경이 없는지 확인한다. 변경이 있으면 커밋·브랜치 전환·Discard 전에 현재 브랜치와 파일 목록을 담당에게 알려 보존 방법을 정한다. 작업 중인 에디터는 저장·종료한 뒤, **Current Branch**에서 팀 기준 브랜치를 선택한다.
3. **Fetch origin**으로 새 변경을 확인하고 **Pull origin**이 표시되면 받아온다. 오류나 충돌이 나면 아래 복구 절차를 따른다. **Pull origin with rebase**만 보이거나 강제 작업을 요구하면 담당에게 확인한다. [공식 동기화 안내](https://docs.github.com/en/desktop/working-with-your-remote-repository-on-github-or-github-enterprise/syncing-your-branch-in-github-desktop)
4. **Current Branch → New Branch**에서 팀 기준 브랜치를 바탕으로 새 작업 브랜치를 만든다. 이름은 팀 합의에 따르고 Issue와 연결한다. 기준 브랜치에서 바로 편집하지 않는 흐름을 권장한다. [공식 브랜치 안내](https://docs.github.com/en/desktop/making-changes-in-a-branch/managing-branches-in-github-desktop)
5. 지정된 파일만 수정하고 저장한다. Desktop **Changes**에서 파일 목록·삭제 여부를 검토하고 이번 작업 파일만 체크한다. **Summary**에 변경 이유를 적고 현재 작업 브랜치로 **Commit**한다. 처음 보는 설정·생성 파일이 섞였으면 담당에게 확인한다. [공식 커밋 안내](https://docs.github.com/en/desktop/making-changes-in-a-branch/committing-and-reviewing-changes-to-your-project-in-github-desktop)
6. 새 브랜치는 **Publish branch**, 이후 변경은 **Push origin**으로 전송한다. 업로드 오류가 없고 GitHub에서 해당 브랜치·커밋이 보이는지 확인한다. 저장소 전체를 새로 공개하는 **Publish repository**와 혼동하지 않는다. 전송 중 **fork(포크) 생성 안내**가 나오면 진행하지 말고 담당에게 팀 저장소 쓰기 권한을 확인한다. 개인 포크를 만드는 흐름은 이 안내의 팀 작업 흐름과 다르다.
7. **Preview Pull Request**에서 변경과 **base**(합칠 대상)를 확인하고 **Create Pull Request**로 브라우저를 연다. 제목·목적·관련 Issue/Notion 개정·검사 결과·이미지를 작성한다. 검토자는 팀이 지정한 담당으로 요청한다. [공식 PR 생성 안내](https://docs.github.com/en/desktop/working-with-your-remote-repository-on-github-or-github-enterprise/creating-an-issue-or-pull-request-from-github-desktop)
8. 수정 요청이 오면 같은 작업 브랜치에서 수정·커밋·전송하고 답글을 남긴다. 검토와 병합은 지정 담당이 수행한다. 병합 뒤 다음 작업을 시작할 때 저장·종료·변경 보존을 확인하고 기준 브랜치로 돌아와 새 변경을 받는다.

기존 작업 브랜치에 기준 브랜치의 변경을 합쳐야 한다면 첫 작업에서는 기술 담당과 진행한다. **Pull origin은 현재 브랜치의 원격 변경을 받는 동작**이므로 기준 브랜치의 모든 변경이 작업 브랜치에 자동으로 들어온다고 생각하지 않는다.

## 오류가 나거나 실수했을 때

| 상황 | 지금 할 일 |
|---|---|
| 잘못된 파일을 포함했지만 아직 커밋 전 | 파일 체크를 해제한다. 파일 자체를 삭제할 필요는 없다. |
| 잘못된 브랜치에서 작업·커밋함 | 현재 브랜치·파일 목록을 남기고 담당에게 보존과 이동을 요청한다. |
| 충돌, LFS 오류, 권한 거절, 업로드 실패 | 추가 전송을 멈추고 오류 화면·작업 경로·브랜치·변경 목록을 전달한다. |
| 파일이 Changes에 나오지 않음 | ignore 대상인지 기술 담당에게 확인한다. 원본을 삭제하거나 파일명을 임의로 바꾸지 않는다. |
| 이미 전송한 파일이 잘못됨 | Issue/PR에 알리고 담당과 수정 커밋을 준비한다. 비밀값·개인정보가 포함됐다면 추가 전송을 멈추고 즉시 담당에게 알린다. 수정 커밋만으로 과거 이력의 노출이 제거되지는 않는다. |

**Discard changes**, 강제 전송, 전체 초기화는 복구 안내 없이 누르지 않는다. 원본 파일과 현재 변경을 보존한 상태로 도움을 요청한다. 아트 바이너리는 충돌 화면에서 임의로 한쪽을 선택하지 않는다.

```text
도움을 요청할 때:
하던 작업 / 저장소·브랜치 / 파일 경로:
마지막으로 누른 버튼 / 기대 결과 / 실제 오류:
저장·커밋·전송 중 어디까지 성공했는지:
오류 화면(비밀값 제외) / 관련 Issue·PR:
```

제품 버튼과 절차는 위 GitHub 공식 문서를 2026-10-23에 확인했다. 언어·앱 버전·권한에 따라 표시가 다르면 화면에 없는 버튼을 추측해 누르지 말고 담당에게 현재 화면을 전달한다. 이 안내 작성에서 설치·업로드·계정 변경은 실행하지 않았다.
