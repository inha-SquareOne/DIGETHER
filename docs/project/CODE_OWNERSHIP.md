# 코드 리뷰 담당과 Notion 연결

2026-11-06 [DIGETHER 작업 보드](https://www.notion.so/20161be04f6e44229b50f6b53768c41e)의 담당자 속성을 조회했다. 보드는 작업목록 사본이며 기획 원본이 아니다. 조회한 대표 페이지의 검증 상태는 `unverified`이고 별도 승인 개정·승인자·승인일은 미기재다. 담당의 GitHub 계정 연결은 같은 날 사용자가 확인했다.

| 담당 | GitHub | Notion 작업 범위 | 현재 Git 경로 연결 |
|---|---|---|---|
| 김민성 | `@AidenKim923` | DEV-013~032, 034, 037, 046~052, 059~071, 079: 온라인·음성·정산·사망/관전·엘리베이터·보강·거점·몬스터·저장·HUD·개발 지원·하우징 동기화 | `Game/Economy`, `Elevator`, `Framework`, `Frontend`, `Hub`, `Life`, `Monster`, `Online`, `Run`, `Save`, `Settings`, `UI`, `Voice`, `Dev` |
| 강태웅 | `@pajupaju0907-byte` | DEV-003, 006~011, 033, 035, 053, 058, 072~074: 앉아서 걷기·중량·인벤토리·팀킬·특수 아이템·장비 | 독립 경로가 없어 활성 규칙 보류. 통합 Character나 Economy 전체의 담당으로 확대하지 않음 |
| 용다인 | `@go5rae` | DEV-012, 036, 038~041: 굴착·갱도 생성·붕괴·매몰·구조·산소; DEV-044 함정 공동 담당 | 현재 `Game/Excavation/Dain/`·`Game/Excavation/DainLab/`(테스트 포함). 갱도 나머지·함정은 경로 분리 후 연결 |
| 김지훈 | `@zhun0922` | DEV-042~045: 던전 진입·문·함정·완전 암흑; DEV-044는 용다인과 공동 담당 | 현재 `Prototype/World/DigetherPrototypePortal.*`. 문·함정·암흑은 경로 분리 후 연결 |

실제 규칙은 [.github/CODEOWNERS](../../.github/CODEOWNERS)에 있다. 기능 담당을 현재 구현 경로에 대응한 것이며, 작성 이력을 담당 배정 근거로 쓰지 않았다. 담당이 섞인 통합 파일·공유 BP 폴더·전체 테스트 폴더에는 포괄 규칙을 두지 않았다. 새 파일이나 경로 분리 후에는 Notion 담당과 경로를 다시 대조해 갱신한다.

CODEOWNERS는 리뷰 책임을 표시하며 파일 쓰기 독점·기획 승인·브랜치 보호를 대신하지 않는다. 공유 문서·Config·Plugins·Git BP·SVN 아트/맵의 담당은 미확정이다. SVN 파일은 Git CODEOWNERS 대상이 아니므로 잠금·인계로 관리한다. 실제 GitHub 해석 오류와 자동 리뷰 동작은 원격에서 별도 확인한다.

대표 원문: [납부 DEV-025](https://www.notion.so/4097458fa947818c9a32e2fbb4061d92), [장비 DEV-053](https://www.notion.so/4097458fa94781ee9b6cdbb8c18a04f1), [굴착 DEV-012](https://www.notion.so/4097458fa94781e6a635f1f3df0cedcc), [던전 진입 DEV-042](https://www.notion.so/4097458fa94781b1bc67cc8694bc9e81). 대표 페이지 수정 시각은 각각 11-05, 10-30, 10-30, 10-30이며 수정 시각을 승인일로 간주하지 않는다.

## 원격 검사 결과

2026-11-06 GitHub CODEOWNERS 오류 API에서 go5rae·zhun0922가 Unknown owner로 보고되었다. 계정 존재와 사용자 연결은 확인했으나 저장소 쓰기 권한은 미확인이다. 해당 3개 경로 규칙은 주석으로 보존했으며 권한 확인 후 활성화한다. 담당 배정을 바꾼 것이 아니며 다른 사람으로 대체하지 않는다.

같은 날 팀원 전원에게 write 권한을 부여했다. go5rae 규칙은 develop 병합(PR #7)으로 바뀐 현재 경로 `Game/Excavation/Dain/`·`Game/Excavation/DainLab/`로 고쳐 활성화했다. 이전 `Prototype/World/DigetherExcavation*`·`DigetherExcavationTerrainTest.cpp` 규칙은 제거했다. zhun0922 규칙은 아직 주석 상태다.

