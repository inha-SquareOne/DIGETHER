# 개발반 설계와 Unreal 기능 경계 문서 검증

기준일: 2026-10-25. 문서 L1 검증 기록이며 게임 실행 결과가 아니다.

## 작성 범위와 근거

- 요청: 기획 기반으로 개발반 기능 설계와 Unreal 제공 범위·추가 개발 안내 작성.
- 기획: `docs/GAME_DESIGN.md`의 DG-001~019, 2026-10-23 이관 전 초안. 온라인 Notion 승인 개정은 조회하지 않았다.
- 신규 문서: [개발반 시스템 설계](../../design/DEVELOPMENT_SYSTEM_DESIGN.md), [Unreal 기능 경계](../../design/UNREAL_FEATURE_COVERAGE.md).
- 연결 문서: 프로젝트 README·ARCHITECTURE·PROJECT_PROFILE·STATUS와 개발자 가이드. 검토 후 DECISIONS에 미결 질문, VERIFICATION_PLAN에 검사 관찰을 보완했다.
- 실제 담당자·게임 수치·서버 방식·저장 정책은 새로 확정하지 않았다.

## 확인한 엔진 근거

조사 PC의 `Engine/Build/Build.version`: MajorVersion 5, MinorVersion 8, PatchVersion 2, Changelist 56702186. 관련 `.uplugin`과 runtime 헤더를 읽었다. Enhanced Input의 AddMappingContext/BindAction, Geometry Scripting의 ApplyMeshBoolean, DynamicMeshComponent의 UpdateCollision/bUseAsyncCooking, PhysicsHandle의 GrabComponentAtLocationWithRotation을 확인했다. VoiceChat 인터페이스의 JoinChannel/LeaveChannel/Set3DPosition도 헤더에서 확인했다.

공식 문서의 UE 5.8 표기와 기능 설명을 확인해 기능 경계 문서의 E01~E15에 연결했다(이후 교차 검증 반영으로 travel 근거 E16 추가). Online Services는 공식 문서상 Beta다. UMG 웹 문서는 도구 열람 실패로 로컬 UserWidget 헤더 확인만 사용했다. 설치 파일 존재를 프로젝트 활성화·플랫폼 호환·실행 성공으로 확대하지 않았다.

## L1 검사

- 작업 시작 Git status: 기존 변경 없음.
- 최종 10개 작성/수정 문서의 상대 파일 링크 82개 검사: 누락 0개. 새 파일을 포함한 줄 끝 공백 검사도 0개.
- DG-001~019를 구현 묶음·상세 시스템·엔진 경계 표에 대조했다. 횡단 항목으로 데이터·저장·검증을 구분했다.
- `git diff --check`: 오류 없음. 실제 코드·설정·바이너리 변경 없음.
- 기획 초안/기술 제안/버전 근거/실행 미검증을 구분하고 VT-01~13 및 미결 D 항목에 연결했다.

## 교차 검증

Claude Opus 5.5 `xhigh` / plan 읽기 전용 검토는 exit 0으로 완료했다. 높은 심각도 0, 중간 4, 낮음 5건이다. **9건 모두 문제 취지를 수용**, 기각 0건이며 부모가 다음을 수정했다.

| 항목 | 반영 |
|---|---|
| 소유자 사망·이탈·미탑승 시 아이템 전이 | 서버 내부 처리 대기와 기획 결정 gate, D-006~008 질문·VT 검사 추가. 자동 드롭/소멸을 게임 규칙으로 확정하지 않음 |
| 주기 상태 수명 | 런/일차 전환 너머 장부 보존과 영구 저장 구분. 같은 월드·서버 GameInstanceSubsystem·seamless/PlayerState·inactive 후보와 제약 추가 |
| 기본 VoIP와 음성 집행 | VOIPTalker 후보·전용 서버 모듈 제약·서버/제공자 전달 차단과 클라이언트 음량 차이 추가 |
| 중량 이동 제한 | D-007 채택 시에만 적용하도록 조건화·정보 역산 질문 추가 |
| 부활·보강 내구도 | 기획 미결 상태와 일치하도록 표현 수정 |
| 런/일차 경계 | D-008 질문과 서로 다른 사건 명시. 1런=1일은 근거가 없어 확정하지 않음 |
| 물리 납부 창구 | 구현 후보로 명시, 물리 창구 채택 시에만 거리 검사 |
| 근거·계약 | travel 공식 링크/로컬 API·서버 시각·CT-DAY 추가 |
| 복제 방식 | 채택 증거에 Legacy/Iris 및 실제 프로젝트 설정 확인 추가 |

주기 수명과 음성 후보 설명이 추가되어 같은 모델로 보완 범위를 재검증했고 exit 0으로 완료했다. 9건 중 7건 해결, 2건은 낮은 심각도의 잔여 문구(VT-08 관찰 누락, 재접속 D-004 참조 누락)를 지적받아 모두 수정했다. 선택 보완인 GameInstanceSubsystem 서버 권한·세션 초기화, 처리 대기 진입 조건, ARCHITECTURE의 CT-DAY 문구도 반영했다. 재검증 후 기각 0건이며 문구·표 보정만 수행해 추가 재호출은 하지 않았다. 수정 후 최종본의 독립 재승인으로 해석하지 않는다. 최초 검토자는 웹 문서를 재열람하지 않았고 재검증자는 엔진 소스도 재조사하지 않았으며, 웹/소스 근거는 부모의 확인과 구분한다.

## 2026-10-26 보완

설계 점검에서 찾은 11건을 반영했다. 게임 수치·규칙은 새로 확정하지 않고 질문과 기술 경계만 추가했다.

| 구분 | 반영 |
|---|---|
| 기획 질문 | D-008에 납부 완료 재화의 사망 손실 포함 여부·복구 시점·구매 장비 유지/손실/양도, D-007에 장비 중량 포함, D-006에 체력 모델·피해 누적 추가. VT-05/08 관찰 보완 |
| 기술 설계 | 시스템 설계 2절 복제 규모·대역폭(휴면·거리 컬링·갱신 빈도·소유자 관련성, 로컬 `Actor.h` 확인), 5절 피해 적용 지점, 7절 장비 인스턴스·손실 범위, 10절 ADR 후보 표. 기능 경계에 복제 규모 행과 [Unreal 테스트 기준](../../unreal/TESTING.md) 연결 |
| 정합성 | DECISIONS·STATUS 기준일, ARCHITECTURE 캐릭터 행의 중량 조건화, "E절" 모호 참조를 3절 E 링크로 교체, E11 보완 근거를 E11에 병합, STATUS 다음 작업에 설계 검토·지형 비교 추가 |

## 미검증과 다음 작업

`.uproject`가 없으므로 빌드·PIE·4인 별도 프로세스·실제 4PC·패키징·성능은 모두 미실행이다. 먼저 승인 기획 개정과 플랫폼/서버 정책을 연결하고, 지형 후보를 동일 실험 맵에서 비교한다. 게임 수치는 승인 데이터가 생기기 전 테스트 픽스처와 구분한다. 커밋·푸시·외부 게시·프로젝트 생성은 수행하지 않았다.
