# Dain 굴착 — 실행과 검증

기준: 2026-11-06. [현재 구현·팀 통합 설계](DAIN_EXCAVATION_INTEGRATION.md).

## 실행

1. UE5.8.4로 현재 Git 작업 폴더의 DIGETHER/DIGETHER.uproject를 연다. 이번 PC의 확인 경로는 D:\Jun\DIGETHER\DIGETHER\DIGETHER.uproject다.
2. 콘텐츠 → Developers → Dain → Excavation → Maps → L_DainExcavation을 연다. 숨겨져 있으면 콘텐츠 브라우저에서 개발자 콘텐츠 표시를 켠다.
3. 플레이어1명/Standalone/선택 뷰포트 재생 후 화면을 클릭한다. Simulate는 사용하지 않는다.
4. WASD 이동, 마우스 조준, 좌클릭 유지로 갈색 흙을 판다. 발밑과 가장자리를 넓혀 통과한다. 회색 벽·바닥은 파이지 않아야 한다.
5. Esc로 종료하고 다시 재생하면 초기화된다. Shift+F1로 마우스를 에디터에 돌려준다.

아웃라이너 Dain_DiggableSoil의 클래스는 DigetherDiggableSoil이다. 별도 BP_DiggableSoil을 배치할 필요가 없다. 현재 개인 에셋은 L_DainExcavation.umap/MI_DainSoilE2.uasset 두 개다. 이전 L_ExcavationLab과 연습 BP는 현재 폴더에 없다. 개인 에셋은 Git 제외/SVN 대상이며 이번 코드 정리에서 파일·SVN 상태를 변경하지 않는다.

## 현재 시험 수치

- CellSpacing25cm, GridCells(9,35,19), 표본7200개. Actor 위치(687.5,-437.5,-12.5), 스케일1.
- 흙벽 앞면X700/뒤면X900, 폭850cm/높이450cm.
- SoilBrushRadius75cm, 간격0.35초, 카메라 사거리400cm.
- 개인 Character 캡슐 반경35/반높이90cm, MaxStepHeight45cm. 기획의 최종 캐릭터 규격이 아니다.
- 밀도 기울기 노멀로 음영을 부드럽게 했으나 단순 흙 재질/어두운 내부 조명/작은 윤곽 각은 남아 있다.

## 검사 방법

- 계산/Actor Automation 선택: DIGETHER.Dain.Soil. 총6개: SoilCore의 Carve/Topology/Validation, SoilActor의 Transaction/Walk/Vertical.
- 맵 실행: UnrealEditor-Cmd에 개인 맵 경로와 -game -LabVerify. 개인 GameMode가 검사기를 생성하여 실제 Character의 Trace/DigOnce/CharacterMovement로 구멍을 왕복한다.
- 게임 실행 결과는 실행 UserDir/Saved/QA/wall-result.json의 passed를 확인한다. 종료 코드만으로 성공 판정하지 않는다. 렌더 검사는 through-wall.png도 확인한다.
- 준비 충돌 실패·잘못된 후보·보호 영역·빈 지형·초기화·노멀 연속성·와인딩·벽/바닥/천장 실제 이동을 검사한다.

이전 맵 검사의 직선 캡슐 sweep는 턱을 밟고 넘는 실제 걷기와 달라 실패했다. 현재는 캡슐/이동 설정을 바꾸지 않고 X1050 통과 후 추가 굴착 없이 X600까지 복귀하는 것으로 검증한다. 평평한 직선 통로 확보를 주장하지 않는다.

## 기존 검증과 한계

2026-11-06 이전 작업 폴더에서 검증 옵션 Editor 빌드와 선택 Automation6개 성공, 개인 맵 DX11 실행에서 유효 굴착6회·보호 거절·순간이동0·통과/복귀를 확인했다. 노멀 연속성 검사는 변경 전 실패→수정 후 성공을 확인했다. 현 정리 후 결과는 아래에 별도 기록한다.

검증 옵션은 -NoPCH -NoSharedPCH -NoUBA -UBADisableWaitOnMem -MaxParallelActions=3 -CompilerArguments=/FIUObject/Package.h -OverrideBuildEnvironment다. 기존 공용 테스트 include/PCH 문제를 공용 파일 수정 없이 분리한 것이며 기본 빌드 환경을 고쳤다는 뜻이 아니다. 초기 UnifiedErrorTest Condition failed 로그는 선택 테스트 결과와 별도다.

수동 PIE/손맛, 공용 팀 Pawn·도구, 4인·재접속, AI 경로, 목표 규모 성능, 패키지는 미검증이다. 전체 볼륨 동기 재생성 비용을 측정하기 전 전체 갱도에 고해상도 격자를 일괄 적용하지 않는다.

## 2026-11-06 정리 후 재검증

현재 작업 폴더 D:\Jun\DIGETHER, feature/Dain, HEAD 6c51ee1f18424e8a1e282be06267e2eeaeddcb38에서 검사했다. 시작 Git 상태는 깨끗했고 에디터는 없었다.

- 구형 C++4개와 호환 분기 제거, 문서5→2 통합. 개인 C++14개/에셋2개 유지.
- 첫 NoPCH 빌드는 공용 DigetherCreateRoomPanelWidget.cpp의 UGameInstance 불완전 타입 오류로 실패(종료6). 공용 코드 수정 없이 검증 옵션에 /FIEngine/GameInstance.h를 추가한 빌드는 성공(종료0). 기본 옵션 빌드를 확인한 것은 아니다.
- 선택 Automation6개 성공/경고0/실패0/미실행0, 엔진종료0.
- 현 개인 맵 NullRHI 게임 passed=true: 유효 굴착6회, 보호거절, X1050 이상 통과 후 X599.91복귀, 순간이동0. 이번에는 수동 UI/렌더 재검증은 하지 않았다.
- 현재 Content131개 SHA-256 모두 동일. 맵/재질·SVN 에셋은 변경하지 않았다. SVN CLI가 PATH에 없어 서버/작업 사본 상태는 별도 확인하지 못했다.
- 에디터 AndroidFileServer가 자동 기록한 Config 변경은 원래 HEAD와의 정확한 차이를 확인해 복구했다. Config/.vsconfig 최종 차이 없음. 작업 중 나타난 프로젝트 바로가기 미추적 파일은 이번 작업 파일이 아니므로 보존했다.
- 로컬 증거: DIGETHER/Saved/DainCleanup의 build.log, build_validation.log, report/index.json, automation.log, play.log, play_user/Saved/QA/wall-result.json, asset_preservation.json.
- 외부 백업: D:\Jun\DigetherCleanupBackups\20261106_165537 (정리 전 개인 코드/문서). Git 커밋 이력은 변경하지 않았다. Commit/Push/PR/SVN 변경 없음.

## 2026-11-06 코드 정돈 재검증

- 개인 C++14개 서식 통일. 계산 코어 내부 함수 분리/지역 변수 이름/주석 정리. 공개 클래스·프로퍼티·함수 이름, 파일 경로와 시험 수치 유지.
- 첫 빌드에서 추출한 사면체 인수의 포인터/범위 for 불일치를 발견해 길이4 배열 참조로 수정. 최종 검증 옵션 빌드 종료0.
- 선택 Automation6성공/경고0/실패0/미실행0. 현재 개인 맵 NullRHI passed=true: 굴착6회, X1050 이상 통과 후 X599.91복귀, 보호 거절, 순간이동0.
- 콘텐츠131개 실행 전후 해시 동일. 착수 시 이미 수정된 DefaultEngine.ini를 백업과 비교해 그대로 보존. 맵·재질·설정 편집 없음. 기존 구형 코드 삭제/문서 통합 변경 유지, Commit/Push/PR 없음.
- 증거: DIGETHER/Saved/DainRefactor의 build_final.log, report/index.json, automation.log, play.log, play_user/Saved/QA/wall-result.json. 원본 백업은 D:\Jun\DigetherCleanupBackups\Refactor_20261106_171318.
- 이번에는 수동 PIE/화면 비교/멀티플레이를 수행하지 않음. 기존 검증용 include 옵션과 기본 빌드 환경 한계는 유지.

## 2026-11-06 팀 컨벤션 보완 검증

- feature/Dain, HEAD 6c51ee1f18424e8a1e282be06267e2eeaeddcb38 유지. 개인 C++12개 수정, 파일 추가·삭제 없음. 기존 변경과 구형 파일 삭제 상태 보존
- 타입 접두사·격자 조회 이름·시험 변수·한국어 주석·FText 안내·Category 정리. 비소유 검증 참조는 TWeakObjectPtr, UObject 유효성 검사 보완
- LabVerify 생성·검증 Tick 본문·파일 기록·프로세스 종료에 Shipping 제외 조건 적용. Shipping 패키지 자체는 미검증
- 기존 맵 직렬화 이름과 Blueprint 함수/핀은 유지. CellSpacing/SoilBrushRadius/HitInterval 단위는 메타데이터·주석으로 명시, CoreRedirects/맵 변경 없음
- 계산 코어는 이름 정규화·주석/공백 제외 후 실행 토큰이 이전과 동일. 밀도 연산·표면 생성·수치 유지
- Editor 빌드 성공(종료0). 변경된 개인 cpp8개가 Adaptive Build에 의해 각각 컴파일됨. 전체 강제 non-unity 검사는 범위 밖 재컴파일로 중단, makefile 재생성(-gather) 후 기존 검증 옵션으로 성공
- 빌드 옵션: -NoPCH -NoSharedPCH -NoUBA -UBADisableWaitOnMem -MaxParallelActions=3 -CompilerArguments="/FIUObject/Package.h /FIEngine/GameInstance.h" -OverrideBuildEnvironment. 공용 include/PCH 문제 수정이나 기본 옵션 빌드 성공을 뜻하지 않음
- 선택 Automation 6성공/경고0/실패0/미실행0, 프로세스 종료0
- L_DainExcavation NullRHI 실행 passed=true, 유효굴착6회·보호거절·순간이동0·실제 CharacterMovement 왕복. 반환X599.95, 한 번 굴착 후X665.00
- 공용 소스·설정·콘텐츠131개 해시 보존. 시작부터 있던 DefaultEngine.ini 변경과 바로가기·기존 삭제 상태 보존. 커밋·푸시·PR·SVN 조작 없음
- 로그에 기존 UnifiedErrorTest Condition failed 및 게임 모드 Python ToolsetDefinition/PythonTestRunner 초기화 오류가 존재. 선택 굴착 테스트 통과와 전체 시작 로그 무오류는 구분
- 증거: DIGETHER/Saved/DainConvention의 build_gather.log, report/index.json, automation.log, play.log, play_user/Saved/QA/wall-result.json, core_preservation.json, preservation.json. 백업: D:\Jun\DigetherCleanupBackups\Convention_20261106
- 수동 PIE·화면·전체 Shipping 패키지·멀티플레이·성능은 이번 검증 대상 아님

문서 검사: 변경 문서2개 링크 검사 통과. 전체 문서 검사에서는 기존 TUNING_ENVIRONMENT_STATUS.md:10의 로컬 기록 누락 링크1건 검출, 범위 밖 문서 미수정

## 2026-11-07 단계별 성능·밀도 변경 범위 계측

### 변경과 사용법

굴착 알고리즘·충돌 교체 순서·맵·수치는 유지. Dain/DigetherSoilProfiling.h 추가, DigetherDiggableSoil.cpp/SoilField.cpp에 범위 타이머 추가. 기존 Actor/Field 테스트 파일에 Performance/Footprint 진단 추가. 청크는 구현하지 않음.

개발 빌드 콘솔에서 `digether.SoilProfile 2`로 요청별 ms 로그 출력, `0`으로 해제. `1`은 로그 없이 마지막 측정값만 기록하는 자동 테스트용. 기본0, Shipping에서 계측 코드 제외. 총시간은 로그 출력 전까지이며 계측 자체의 소량 비용 포함. 로그 필터: LogDigetherSoilPerformance. Result는 Rejected0 / NoChange1 / Applied2 / ApplyFailed3.

### 실측 환경

UE5.8.4 Development Editor, i5-11500/32GB, Windows, NullRHI. 실제 UWorld·PMC·Chaos, 9×35×19셀/25cm/반경75cm, 보호박스 없음. 격자를 초기화하고 중앙 정면에서 실제 충돌 Trace로 새 타격점을 선택. 워밍업5회 제외, 측정20회 × 성공타격3회 = 60개. 초기화와 진단용 표면 비교는 측정 밖. GPU/렌더 스레드 완료 시간·패키지·4인 성능 아님.

| 단계(ms) | 평균 | p95 | 최대 |
|---|---:|---:|---:|
| 밀도 복사 | 0.000375 | 0.000700 | 0.000801 |
| Carve(입력 검증 포함) | 0.037397 | 0.039700 | 0.062000 |
| 사면체 표면 생성 | 1.296733 | 1.423098 | 1.486797 |
| 노멀 계산 | 1.691568 | 1.829803 | 1.939598 |
| 기존 후보 섹션 제거 | 0.505480 | 0.664100 | 0.729702 |
| 새 메시 등록 + 동기 충돌 생성 | 29.824115 | 31.122103 | 31.703498 |
| 상태 교체·revision | 0.082848 | 0.099804 | 0.153597 |
| 전체 TryExcavate | 33.453681 | 34.743302 | 35.551798 |

새 메시 등록+충돌 생성이 전체 평균의 약89.2%. PMC 공개 CreateMeshSection 호출의 합산 시간이며 순수 Chaos cook 시간으로 표시하지 않음. Remove에도 UpdateCollision 비용 포함. 단계 합계와 Total 사이에는 표면 입력 검증·BodySetup 검사·할당 해제·타이머 관리 등의 차이가 있음. 병렬 실행하거나 인위적으로 시간 비율을 순수 cook으로 분리하지 않음.

### 반경 밖 밀도와 표면

생산 Carve/BuildSurface의 실제 출력 비교. 로컬 중앙 첫 타격(12.5,437.5,237.5), 계산 중심은 1cm 양자화. 변경316표본 중 반경75cm 밖248개, 최대거리159.630824cm, 외부 최대밀도감소37. 변경 표본의 정확 AABB는 (25,325,125)~(125,550,350)cm. 표면 형상 변화120셀, 노멀 출력 변화144셀(형상 변화와 중복). 형상 변화 셀을 둘러싼 AABB는 (0,350,150)~(100,525,325)cm이며 정점의 정확 AABB가 아님.

깊은 위치(X+75cm)에서는 변경432/반경밖332/최대거리160.256045cm, 형상152셀/노멀180셀. 다른 두 위치도 측정했고 같은 중심 재타격은 모두 변화0. 세부값은 footprint.csv. 셀 분류는 실제 생성 삼각형의 중심으로 소속 셀을 정하고 위치/노멀을 소수9자리 문자열로 비교하는 진단용 방법, 청크 구현 아님.

원인: min(기존 밀도, EncodeDistance(거리-반경))를 내부 전체 표본에 적용. 반경 밖에서도 양수 값이 기존 값보다 작으면 감소. 단순한 HP 차감이나 반경 안 표본만 지우는 방식이 아님. 양수의 변화가 보간·기울기에 쓰이므로 외부 변경을 삭제하면 경계 위치/음영/후속 타격 결과가 바뀔 수 있음. 현재 계산 유지.

### 청크 예상 효과 — 시간 실측 아님

실제 변경 표본에서 기울기·표면 의존성을 보수적으로 포함한 셀 범위(-2~+1 이웃 박스)를 청크에 투영. 원점 고정, 말단 청크는 실제 남은 셀만 포함. 각 청크 전체를 재추출한다고 가정.

| 타격 | 전체 | 8셀 청크 | 16셀 청크 |
|---|---:|---:|---:|
| 중앙 첫 타격 | 5985셀 | 4청크/2048셀(34.2%) | 2청크/4608셀(77.0%) |
| 중앙 깊은 타격 | 5985셀 | 12청크/3712셀(62.0%) | 4청크/5472셀(91.4%) |
| 모서리 첫 타격 | 5985셀 | 1청크/512셀(8.6%) | 1청크/2304셀(38.5%) |

이는 순회량 추정이며 총시간 감소율이 아님. 표면삼각형 수·cook 고정비·컴포넌트 수가 달라져 33.45ms에 위 비율을 곱해 예측할 수 없음. 8셀은 총30청크,16셀은6청크이므로 관리비도 비교 필요. 중앙 깊은 타격의 실제 형상·노멀 변화는 8셀4청크/2048셀에 모이지만 보수적 밀도 의존성은12청크로 확대. 영향 없는 내부 밀도 변경과 넓은 의존성 박스가 불필요한 재추출을 유발할 수 있다는 근거. 사후 표면 비교 결과로 미리 갱신 생략이 가능하다고 단정하지 않음.

추천: 반경 밖 변경을 유지한 채, 공유 밀도+청크별 PMC의 작은 비교 구현에서 실제 cook 시간을 측정. 현재 비용이 작은 Carve(약0.037ms) 최적화보다 전체 충돌 재생성 범위 축소 우선. 이번에는 청크 미구현.

### 빈 메시 분류와 내부 청크

현재 Apply는 빈 인덱스를 정상 공기로 취급하며 별도 분류 enum은 없음. 추가 진단에서 전체공기 밀도1은 BuildSurface 성공/0삼각형, 전체흙255는 외곽 공기층 검사 때문에 실패함을 확인. 따라서 전체볼륨 검사를 내부 청크에 그대로 재사용하면 안 됨.

향후 분류는 추출 성공 여부와 청크 셀 꼭짓점 밀도(128초과 흙)를 함께 검사: 전부공기 / 전부흙 / 경계혼합 / 유효하지 않은 입력·추출실패. 혼합인데0삼각형은 임계값 일치·퇴화 여부까지 조사 후 처리해야 하며 무조건 정상공기 또는 무조건 오류로 단정하지 않음.

전부흙 내부 청크는 표면0이 정상일 수 있음. 이때 삼각형 충돌0은 체적이 공기라는 뜻이 아님. 이웃 외곽면과 밀도 점유 판정으로 내부 스폰/복원 등을 방지해야 함. 이전 경계면이 사라져 전부흙이 된 정상 상태에서 오래된 충돌을 무조건 유지하면 유령 벽 발생. 반대로 실제 추출실패는 이전 상태 유지. 청크 상태 분류·스폰 정책은 제안이며 이번 런타임 충돌 처리 변경 없음.

### 검증·보존·한계

기존 검증 옵션 Editor 빌드 종료0, 자동 테스트8성공/경고0/실패0/미실행0(기존6+진단2). 개인맵 계측2 실행 passed=true, 성공굴착8회·보호거절·실제 CharacterMovement 왕복·순간이동0, 복귀X599.91/Y31.20. 이 자동 조준 시나리오는 프레임 시간에 따라 타격점/횟수가 달라질 수 있어 이전6회와 횟수 동일성을 주장하지 않음. 실제 맵 성공타격 로그의 Total도 약32~34ms 범위.

Build 옵션은 기존 -NoPCH -NoSharedPCH -NoUBA -UBADisableWaitOnMem -MaxParallelActions=3 -CompilerArguments="/FIUObject/Package.h /FIEngine/GameInstance.h" -OverrideBuildEnvironment 사용. 기본 빌드 환경의 기존 문제를 고친 것은 아님. 수동 PIE/패키지/다인/순수 cook 단독 시간은 미검증. 초기 UnifiedErrorTest·Python Toolset 초기화 오류는 기존과 동일하게 선택 테스트와 구분.

근거: DIGETHER/Saved/DainPerformance/build.log, report/index.json, automation.log, play.log, play_user/Saved/QA/wall-result.json, summary.json. 원시 CSV는 automation_user/Saved/DainMeasurements/{timings,footprint,cells}.csv. 로컬 증거는 Git 제외. 백업 D:\Jun\DigetherCleanupBackups\Profiling_20261107. Commit/Push/PR/SVN 변경 없음.


## 2026-11-07 청크 소규모 비교 구현·실측

결론: 현재 개인 흙벽은 **8×8×8셀 청크를 다음 통합 검증 후보로 추천**한다. 전체 방식 대비 평균68.9%, p95 53.4% 감소했다. 기본값은0(전체)으로 유지하며 기존 경로를 제거하지 않았다. 이 결과는 출시 성능 보증이나 공용 시스템 채택 결정이 아니다.

### 비교 조건

UE5.8.4 Development Editor, i5-11500/32GB, Windows, NullRHI의 실제 UWorld/PMC/Chaos. 세 모드 각각 별도 프로세스로 순차 실행. 같은 Entry 시험 월드에 개인 맵과 같은9×35×19셀·25cm·반경75cm 흙을 배치했다. 중심 및 청크 경계 Y/Z=200/200,400/400cm의 세 시나리오에서 실제 충돌 Trace로 다음 타격점을 선택했다. 시나리오별 워밍업5회 제외, 초기화20회×성공타격3회: 모드별180건. 각 대응 행의 타격 좌표(허용오차0.001cm)와 밀도 해시가 세 방식에서 모두 일치했다. 보호 영역 없는 비교 fixture이며 보호 검증은 기존 회귀 테스트와 개인 맵 시험에서 별도로 수행했다.

초기 생성 시간은 각 라운드에서 SetComparisonChunkCells로 지형을 다시 초기화하는 함수 시간이다. 엔진 시작·맵 로딩·셰이더 준비 시간이 아니며 청크 생성/이전 컴포넌트 정리 비용을 포함한다. GC는 초기화 후 측정 밖에서 실행했다.

### 처리 시간 실측

| 항목(ms) | 전체 | 8셀 | 16셀 |
|---|---:|---:|---:|
| 전체 평균 | 33.199 | 10.326 | 21.768 |
| 전체 p95 | 34.965 | 16.311 | 28.473 |
| 전체 최대 | 41.424 | 19.712 | 33.284 |
| 표면 삼각형 생성 평균 | 1.308 | 0.451 | 1.043 |
| 음영 노멀 계산 평균 | 1.671 | 1.069 | 1.342 |
| 후보 섹션 제거 평균 | 0.562 | 0.166 | 0.120 |
| 메시 등록+동기 충돌 생성 평균 | 29.521 | 8.196 | 19.018 |
| 상태 교체 평균 | 0.084 | 0.302 | 0.146 |
| 지형 초기화 평균 | 32.486 | 32.376 | 30.820 |
| 지형 초기화 p95 | 33.826 | 34.214 | 32.409 |

MeshCollision은 CreateMeshSection의 메시 등록과 동기 Chaos cook 합계다. 순수 cook만 분리 측정한 값이 아니다. Remove에도 충돌 갱신이 포함된다. 단계별 합계와 전체 사이에는 검증·dirty 판정·할당 해제 등 비용이 있다.

| 작업량·메모리 | 전체 | 8셀 | 16셀 |
|---|---:|---:|---:|
| 갱신 청크 평균 | 1.00 | 7.56 | 2.78 |
| 갱신 청크 최대 | 1.00 | 12.00 | 4.00 |
| 재생성 삼각형 평균 | 16339.11 | 5138.00 | 11176.00 |
| PMC 개수 | 2.00 | 62.00 | 14.00 |
| CPU 메시 배열 평균(MiB) | 14.51 | 9.51 | 12.21 |
| 프로세스 물리 메모리 평균(MiB) | 2743.78 | 2597.84 | 2668.63 |
| 프로세스 물리 메모리 최대(MiB) | 2918.18 | 2652.84 | 2798.98 |

청크 전체 개수는8셀30개,16셀6개. 각 청크 앞/뒤 버퍼2개와 복구용 원래 PMC2개를 포함한다. 삼각형 수는 해당 굴착에서 다시 만든 합계이며 화면 전체 삼각형 수가 아니다. CPU 배열은 양쪽 버퍼의 ProcVertex/Index GetAllocatedSize 합계이고, Chaos·UObject·GPU 메모리는 포함하지 않는다. 프로세스 물리 메모리는 엔진 전체의 UsedPhysical 관측값이므로 흙의 전용 메모리 절감량으로 해석하지 않는다. GPU를 포함한 완전한 지형 메모리 분리는 미측정이다.

### 연속 굴착·경계 실측

| 시나리오 | 전체 평균/p95(ms) | 8셀 평균/p95(ms) | 16셀 평균/p95(ms) |
|---|---:|---:|---:|
| 중앙 연속3타 | 33.387 / 34.828 | 12.884 / 18.483 | 25.694 / 29.136 |
| Y/Z=200cm 경계 연속3타 | 33.348 / 35.089 | 11.202 / 13.597 | 12.873 / 13.968 |
| Y/Z=400cm 경계 연속3타 | 32.862 / 34.965 | 6.892 / 8.303 | 26.738 / 27.472 |

8셀의 한 번 굴착 갱신 범위는4~12개,16셀은1~4개였다. 75cm 구형 범위만으로 청크를 고르지 않고 반경 밖 실제 밀도 변경 및 노멀 의존성을 포함했다. 따라서 형상이 실제 바뀌지 않는 청크까지 일부 재계산한다. 기존 굴착식을 바꾸는 최적화는 하지 않았다.

### 검증과 제약

- 기존8개 테스트 유지, 추가8개 포함 총16개 성공/실패0. 모드별 전용 benchmark도 각각1성공. 결과는 report JSON으로 확인했으며 프로세스 종료 코드만으로 성공 판정하지 않았다.
- 초기화·Carve(보호 처리 포함)·기존 BuildSurface 함수는 작업 전 백업과 공백 제외 동일하다. 범위 추출에서 전역 좌표·같은6사면체 분할·같은 기울기 노멀 계산을 사용한다.
- 전체 표면과 청크 합집합의 삼각형 위치·방향·음영 노멀을 초기 상태 및 연속 굴착 후 비교해 일치했다. 청크 경계 전후±0.01cm 충돌 Trace 위치도 일치했다.
- 충돌 HitNormal의 정확 일치를 요구한 최초 검사는4건 실패했다. 원인은 모서리를 맞힌 Trace에서 BVH가 서로 다른 인접 삼각형을 선택하는 것이었다. 반환 노멀이 실제 접촉점의 인접 삼각형 방향인지 추가 검사해 모두 통과했다. 음영 노멀 연속성과 충돌 면 노멀 동일성은 구분한다. 도구 범위를 ImpactNormal로 결정할 때 이 차이를 고려해야 한다.
- 후보 두 번째 청크 준비 후 실패를 주입한8/16 모드에서 밀도 해시·revision·활성 충돌 위치가 유지되고 재시도가 성공했다. 실제 OS 메모리 부족/모든 Chaos 오류를 재현한 검사는 아니다. 기존 whole 모드의 cook 실패 검사도 유지했다.
- 전부공기/내부전부흙/범위오류 분류를 검사했다. 혼합인데0삼각형은 보수적으로 실패 처리하며, 퇴화 사례를 모두 정상/오류로 완전 분류했다는 의미는 아니다.
- 벽 통과와 바닥·천장 방향 시험은 실제 CharacterMovement를 사용했다. 자동 vertical fixture는8³셀로 청크 하나에 들어가므로 다중 청크의 바닥·천장 접합부 이동까지 검증한 것은 아니다. 경계 형상/충돌 시험과 실제 개인 맵의 다중 청크 흙벽 왕복은 별도다.
- 새 청크 런타임 코드는 DigetherSoilChunks.cpp에 분리했다. 추가 프레임워크·공용 API·플러그인·비동기 작업은 없다. 기존 full 추출 루프는 비교 안정성을 위해 보존하여 범위 추출 루프와 일부 중복된다.
- 다음 검증은 다수 흙 Actor·장시간 반복굴착·최소사양 렌더링/메모리다. 8셀의62 PMC 관리비와 dirty 청크마다 전체 기울기 배열을 다시 계산하는 비용이 확장 시 위험이다. 이번 작은 볼륨에서16셀은 PMC 수가 적지만 충돌 재생성 범위가 넓어 더 느렸다.

### 증거와 재실행

로컬 근거는 DIGETHER/Saved/DainChunkCompare 아래 summary.json, 각 mode0/8/16-user/Saved/DainMeasurements/chunk-benchmark.csv, final_tests-report/index.json, 각 mode*-report/index.json, core-preservation.json이다. 워밍업을 제외한180행 통계, p95는 정렬 후 nearest-rank 방식이다. CSV와 로그는 Git 제외다.

benchmark 실행: UnrealEditor-Cmd.exe 프로젝트 /Engine/Maps/Entry -nullrhi -unattended -nosound -nop4 -DisablePlugins=AndroidFileServer -SoilCompare=8 -ExecCmds="Automation RunTests DIGETHER.Dain.SoilActor.ChunkBenchmark" -TestExit="Automation Test Queue Empty" -ReportExportPath=개인출력경로 -UserDir=개인출력경로. 모드0/16도 별도 프로세스로 순차 실행한다.

Editor 빌드는 기존 검증 옵션(-NoPCH -NoSharedPCH -NoUBA -UBADisableWaitOnMem -MaxParallelActions=3 -CompilerArguments="/FIUObject/Package.h /FIEngine/GameInstance.h" -OverrideBuildEnvironment)으로 성공했다. 공용 PCH/include 문제를 고치거나 기본 옵션 빌드를 검증한 것은 아니다. 기존 UnifiedErrorTest/Python 초기화 오류와 선택 굴착 테스트 통과는 구분한다. Shipping·패키지·멀티플레이·비동기·붕괴는 이번 범위 밖이다.

### 실제 화면 실행·프레임 간격

동일한 L_DainExcavation, Development -game -LabVerify, DX11/1280×720/windowed, VSync0·MaxFPS0으로0/8/16 각각 실행. 결과 JSON의 실제 chunkCells도 요청 값과 일치함을 검사했다. 세 방식 모두6회 굴착, 보호 대상 거절, 벽 통과와 왕복 성공, 순간이동0. 캡슐 반경35cm/반높이90cm의 시험 캐릭터이며 최종 캐릭터 규격 판정은 아니다.

| 항목(ms) | 전체 | 8셀 | 16셀 |
|---|---:|---:|---:|
| 굴착 프레임 간격 평균 | 40.965 | 12.233 | 27.171 |
| 굴착 프레임 p95/최대 | 41.409 | 17.590 | 27.736 |
| 비굴착 이동 프레임 평균 | 5.189 | 5.191 | 5.253 |
| 비굴착 이동 프레임 p95 | 7.593 | 7.071 | 7.677 |

굴착 프레임은 직전 Tick에서 revision이 증가한 다음 Tick까지의 CPU 실시간 간격이다. 모드별6개뿐이므로 p95=최대이며 장시간 통계가 아니다. GPU 시간/Present 지연을 분리한 값도 아니다. 자동 조준·이동은 프레임 시간에 따라 세부 타격점이 달라지므로 엄밀한 동일 요청 비교는 앞의180건 benchmark를 기준으로 한다. 화면 실행은 프레임 지연 경향과 실제 통과의 보조 근거다.

각 render0/8/16-user/Saved/QA의 wall-result.json·frames.csv·through-wall.png 및 render-summary.json을 기록했다.8/16 스크린샷에서 관통 구멍을 확인했다. CUA 창 캡처는 FrameArrived timeout이었고 우회하지 않았다. 위 화면 증거는 엔진이 정상 렌더링한 FScreenshotRequest 출력이며 수동 PIE 조작 검증과 구분한다.

### 작업 경계 보존

시작/종료 feature/Dain, HEAD 6c51ee1f18424e8a1e282be06267e2eeaeddcb38 유지. 이번 변경은 개인 C++7개·기존 개인 문서2개 및 새 DigetherSoilChunks.cpp 하나다. 기존 개인 변경과 구형 파일 삭제 상태는 그대로 유지한다. baseline1760파일 해시 비교에서 해당9개 외 변화0/누락0이었다. 기존 DefaultEngine.ini 수정·모든 기존 맵/에셋·공용 소스는 작업 시작 상태 그대로다. 커밋/푸시/PR/SVN 조작 없음.

백업: D:\Jun\DigetherCleanupBackups\ChunkCompare_20261107. 최종 빌드 build_verified.log, 최종 회귀 verified-report/index.json, 보존 검사 preservation.json. 다음 작업은8셀 모드를 여러 흙 Actor 규모에서 검증한 뒤 채택 범위를 결정하는 것이다. 기본0을 자동 변경하지 않는다.

변경 범위 diff --check는 통과했다. 저장소 전체 검사에는 시작부터 있던 DefaultEngine.ini:125의 EOF 빈 줄 경고가 남아 있으며 해당 사용자 변경은 보존했다.


## 2026-11-10 8셀 경계 안정성·여러 흙벽 검증

판단: 8×8×8셀을 잠정 개발 기준으로 유지한다. 추가 최적화와 절차 생성 구현은 하지 않았다. 이번 범위에서 관통 실패·상태 불일치·급격한 개별 굴착 비용 증가는 발견되지 않았다. 다만 같은 프레임에 여러 요청을 처리하는 동기 비용은 남아 있어 출시 프레임 예산을 충족했다고 판정하지 않는다.

### 실제 변경 범위

DigetherSoilActorTests.cpp에 ChunkBoundaryMovement와 MultiActor 두 검사를 추가했다. 기존16검사는 유지했다. 런타임 굴착/청크 코드·알고리즘·0/16 모드·공용 코드/맵/설정·기존 에셋은 미변경. 선택은 개발 실행에서 digether.SoilChunkCells 8을 명시하고 회귀용 기본값0은 유지한다. 잠정 채택을 전체 팀 설정이나 Shipping 기본 적용으로 바꾸지 않았다.

### 청크 경계 벽·바닥·천장

실제 UWorld, Chaos, CharacterMovement, 고정1/60초 Tick의 NullRHI 자동 검사다. 벽은9×17×17셀, 바닥/천장은17×17×9셀, 간격25cm, 청크8셀, 반경75cm. 모두18개 청크를 가지며 기존 단일 청크 fixture보다 경계를 확장했다. 200cm 경계 중심과±0.01cm에서3방향×3위치=9경우를 검사했다.

| 방향 | 굴착 전 확인 | 굴착 후 실제 이동 | 성공 굴착/최대 갱신 청크 |
|---|---|---|---|
| 벽 | 전진이X177.40 부근에서 막힘 | X500보다 멀리 통과한 뒤 X-54.21까지 돌아옴 | 6회 / 8개 |
| 바닥 | 경계 위 Z304.65 부근에 서 있음 | 파낸 구멍으로 Z-773.49까지 낙하 | 3회 / 8개 |
| 천장 | 실제 점프가 Z221.48에서 막힘 | 굴착 후 최고Z1092.14, 흙 위로 캡슐 전체 통과 | 3회 / 8개 |

캐릭터 캡슐35cm/반높이90cm, 이동250cm/s, 천장 검사는 JumpZVelocity1400의 시험값이다. 순간이동 없이 처리했으며 수직 검사에서 옆으로 돌아나가지 않았는지도 확인했다. 최종 캐릭터 점프력·안전한 구멍 탈출·기획 규격 검증을 뜻하지 않는다. 이번 경계9경우는 화면/수동 PIE 검사가 아니라 실제 엔진 이동 컴포넌트 자동 검사다. 지난11월7일 실제 화면 왕복 근거와 구분한다.

기존 ChunkSurface/ChunkSeamCollision/ChunkRollback도 다시 통과했다. 경계면 Trace가 다른 인접 삼각형을 골라 HitNormal이 달라질 수 있다는 기존 제약은 유지하며, 정확한 면 노멀 동일성을 새로 보장하지 않는다.

### 1·4·8개 흙벽 실측 조건

UE5.8.4 Development Editor, Windows/i5-11500/32GB, NullRHI. 각 Actor는9×35×19셀/25cm/8셀 청크이며 Y방향2000cm 간격으로 분리한다. 중앙의 같은 로컬 조준선에서 충돌 Trace로3회 연속 굴착한다. 워밍업2라운드 제외, 측정10라운드. 개별 굴착 표본은1개30건/4개120건/8개240건, 묶음 처리30건/조건이다. 별도 프로세스에서 순차 실행했고,1개 조건은 전체 회귀 실행과 분리해 bench1로 측정했다.

초기 시간은 새 Actor 생성부터 속성 설정 및 SetComparisonChunkCells(8) 완료까지의 합계다. 현재 Spawn/OnConstruction/BeginPlay의 기존 초기화와 비교모드 초기화 비용을 포함한다. 엔진 시작·셰이더·맵 로딩·GC는 제외한다. 지난11월7일의 기존 Actor 재초기화 시간과 측정 범위가 다르다.

| 실측 | 1개 | 4개 | 8개 |
|---|---:|---:|---:|
| 초기 생성 전체 평균(ms) | 37.643 | 169.230 | 301.940 |
| 초기 생성 전체 최대(ms) | 38.136 | 221.295 | 328.650 |
| 개별 굴착 평균(ms) | 11.885 | 12.760 | 11.711 |
| 개별 굴착 p95(ms) | 15.536 | 17.529 | 15.155 |
| 개별 굴착 최대(ms) | 16.515 | 21.559 | 15.384 |
| 메시 등록+동기 cook 평균(ms) | 9.345 | 10.058 | 9.214 |
| N개 연속 요청 평균(ms) | 11.917 | 51.188 | 93.922 |
| N개 연속 요청 p95(ms) | 15.568 | 71.900 | 120.349 |
| N개 연속 요청 최대(ms) | 16.537 | 71.967 | 120.601 |
| CPU 메시 배열 합계(MiB) | 11.028 | 44.111 | 88.221 |
| 엔진 프로세스 물리 메모리 평균(MiB) | 2565.921 | 2566.952 | 2625.832 |
| 엔진 프로세스 물리 메모리 최대(MiB) | 2566.445 | 2577.508 | 2626.879 |
| PMC 개수(대기 버퍼 포함) | 62 | 248 | 496 |

N개 연속 요청은 각 벽에 한 번씩 순차 호출한 구간이다. Trace·검사/소량 CSV 문자열 작성도 포함한다. 실제 렌더링 프레임 전체 시간, 병렬 처리, 네트워크4인 시험은 아니다. 같은 프레임에 이런 요청이 모이면 그 시간만큼 게임 스레드가 점유될 수 있다는 비용 근거다. p95는 nearest-rank, 초기화10건에서는p95와최대가 같다.

CPU 메시 배열은 모든 PMC의 앞/뒤 ProcVertex/Index 할당량 합계이며, UObject·Chaos 내부·GPU 메모리를 제외한다. 프로세스 물리 메모리는 엔진 전체이다. CSV의 생성 전후 프로세스 차분은 GC·할당자 재사용 때문에 음수도 관찰되어 Actor 전용 메모리로 해석하지 않는다. 정확한 전체 지형 전용 메모리와 GPU 메모리는 미측정이다.

각 타격 순서의 밀도 해시는 Actor 수·위치와 관계없이4163355329→3733512848→1645034926으로 일치했다. 8개가4개보다 개별 호출 평균이 조금 낮은 것은 이 작은 표본의 실행 변동이며, Actor가 늘수록 빨라진다고 일반화하지 않는다.

### 잠정 채택과 남은 성능 조건

- 흙벽 수 증가에 따라 PMC·CPU 배열은 선형 증가했고, 개별 굴착 시간은 이번 규모에서 비슷한 수준이었다.8셀 유지,0/16 제거 없음.
- 8개 초기 생성 평균302ms는 탐사 중 같은 프레임에 만들면 눈에 띄는 지연이다. 절차 생성 연결에서는 탐사 진입 전 생성·준비 완료 판정을 권장한다. 로딩/스폰 스케줄러를 이번에 구현하지 않았다.
- 4개 연속 요청 평균51.19ms/p95 71.90ms,8개 평균93.92ms/p95 120.35ms는 실시간 동시 요청에서 무시할 수 없는 위험이다. 목표 프레임 예산·동일 프레임 요청 수·팀 도구 사용 간격을 정한 뒤 재검증해야 한다. 출시 성능 합격으로 처리하지 않는다.
- 추가 청크 최적화·비동기·요청 큐·멀티플레이·붕괴 구현은 보류한다. 작은 생성 연결의 설계는 계속할 수 있지만, 다인 실시간 성능 완료는 별개다.
- 여러 Actor 시험은 떨어진 흙벽이며 맞댄 Actor 간 표면·충돌 접합부는 미검증이다. 회전 배치, 장시간 반복/대형맵, 최소사양, 다수 PMC의 실제 렌더링/드로콜/GPU 비용, 패키지/Shipping도 미검증이다.

### 결과와 근거

Editor 빌드 성공(기존 -NoPCH -NoSharedPCH -NoUBA -UBADisableWaitOnMem -MaxParallelActions=3 -CompilerArguments="/FIUObject/Package.h /FIEngine/GameInstance.h" -OverrideBuildEnvironment 검증 옵션). 기본 PCH/include 문제를 해결한 것은 아니다.

전체18검사:17개 성공+경고 동반 성공1개, 실패0/미실행0. Performance 실행 중 LogHttp 외부 연결 generate_204의3초 timeout 경고1건이며 굴착 assertion 실패가 아니다. 별도1/4/8 Actor 측정은 각각1성공/경고0/실패0. 기존 엔진 시작 UnifiedErrorTest 등의 로그와 선택된 검사 결과는 구분한다.

근거 폴더: DIGETHER/Saved/DainChunkStability_20261110. build.log, count1-report/index.json, count1.log(BOUNDARY9행), bench1/count4/count8-report/index.json, 각 *-user/Saved/DainMeasurements/multi-{timings,batch,memory}.csv, summary.json. 코드·원시 자료는 실제 관측이며 추정 성능 수치를 섞지 않았다.

연결 인터페이스와 최소 배치 데이터는 [통합 설계](DAIN_EXCAVATION_INTEGRATION.md)의2026-11-10 절을 따른다. 새 절차 생성 클래스/시스템을 구현하거나 API 계약을 팀 승인으로 확정하지 않았다.

## develop 맵 기록 정정 반영 — 2026-11-10

- 팀의 실행 경로 기준은 저장소의 DIGETHER/DIGETHER.uproject다. 위 PC 절대 경로는 해당 조사 PC에서만 유효하다.
- 원격 초기 기록의 GridCells(5,18,10)·CellSpacing50은 초기 연결 당시 값이다. 현재 로컬 개인 맵의 25cm·(9,35,19) 설정과 혼동하지 않는다.
- 개인 맵의 흙은 ADigetherDiggableSoil로 연결된 상태다. 구형 흙벽 분기를 보존했다는 초기 기록은 현재 로컬 정리 후 코드 설명이 아니다.
- Content/Developers/Dain 에셋 관리 위치는 [Git/SVN 결정](../decisions/ADR-004-git-code-svn-assets.md)을 따른다. 이번 develop 반영에서 개인 맵을 이동하거나 SVN에 업로드하지 않았다.
