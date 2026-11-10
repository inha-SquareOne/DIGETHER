# Dain 굴착 E2 — 지형 Actor 연결

2026-11-06. 개인 브랜치 `feature/Dain`, 기준 HEAD `4d37fda6f98e18962d0cd9a86b9a33260dfa6462`. [팀 통합 설계](DAIN_EXCAVATION_INTEGRATION.md)의 E2 단계다. S07 전체 완료나 출시 준비 완료를 의미하지 않는다.

## 구현

기존 개인 `ADigetherDiggableSoil`의 초기 박스 생성 코드를 [E1 계산 코어](DAIN_EXCAVATION_CORE_E1.md)에 연결했다. 밀도 격자 → Marching Tetrahedra → 엔진 ProceduralMeshComponent 표면·충돌 방식이며, 새 플러그인이나 모듈 의존성은 추가하지 않았다.

1. 서버 권한과 좌표 변환을 검사한다. 회전·이동을 지원하고 스케일 변경은 거절한다.
2. 현재 밀도의 사본에서 허용된 영역만 깎는다. 잘못된 입력은 Rejected, 변화가 없으면 NoChange다.
3. 사용하지 않는 메시 컴포넌트에서 표면과 동기식 충돌을 만든다.
4. 충돌 생성 성공을 확인한 후 표시·충돌·밀도를 교체한다. 실패하면 ApplyFailed로 이전 유효 상태를 유지한다.
5. 성공 시 Applied와 revision 증가, 변경 영역을 남긴다. 완전 제거 시 두 컴포넌트의 표시와 충돌을 끈다.

`TryExcavate(WorldPoint, Radius)`는 서버가 검증한 타격을 받기 위한 지형 함수다. 장비·거리·쿨다운·공격 분기 검증이나 RPC를 대신하지 않는다. `HasAuthority`만으로 클라이언트 복제가 구현된 것은 아니다.

## 변경 경계

- 수정: `DIGETHER/Source/DIGETHER/Game/Excavation/Dain/DigetherDiggableSoil.h`, `.cpp`
- 추가: 같은 폴더의 `DigetherSoilActorTests.cpp`
- 기존 `DainLab` 흙벽·캐릭터 및 모든 맵·Blueprint 에셋은 수정하지 않았다.
- `BP_DiggableSoil`의 부모 C++ 동작은 변경된다. 기존 `SoilMesh` 이름은 보존했지만 이전 `SoilSize` 대신 GridCells/CellSpacing을 사용한다. 새 표면의 원점은 격자 모서리이며 기본 표면 범위는 각 축 약 25~375cm다. 기존 박스 크기·피벗과 동일하다고 가정하지 않는다. 에셋 재저장·배치 이동·수동 렌더 검증은 미수행이다.
- `L_DainExcavation`의 `Dain_DiggableSoil`은 기존 DainLab 구현이다. 이번 코드가 그 액터에 자동 교체된 것은 아니다. 이후 [맵 시험](DAIN_EXCAVATION_MAP_TEST.md)에서 해당 액터를 `ADigetherDiggableSoil`로 교체했다.

## 검증 방법과 한계

자동 시험에서 임시 게임 월드를 만들고 엔진 ACharacter(캡슐 반경35cm, 반높이90cm)의 실제 CharacterMovement와 Chaos 충돌을 실행한다. 좌표를 순간이동시켜 통과한 것으로 처리하지 않는다. 화면 없는 시험이므로 테스트 입력을 직접 넣고 `bRunPhysicsWithNoController` 및 초기 낙하 모드를 설정했다. 팀 Pawn·플레이어 입력·최종 캐릭터 규격의 검증은 아니다.

- Transaction: 초기 충돌, 충돌 cooking 실패 주입/잘못된 밀도 거절 시 기존 상태 보존, 보호 영역, 전체 제거, 초기화.
- Walk: 온전한 벽에서 정지, 한 번으로 전부 사라지지 않음, 반복 굴착 후 실제 걷기와 같은 구멍으로 복귀.
- Vertical: 흙 위에 서기, 아래 방향 굴착 후 실제 낙하, 아래에서 천장 굴착 후 실제 상승 통과. 천장 시험의 점프 속도1400cm/s는 시험 전용이며 게임 수치가 아니다.
- E1 순수 계산 3개와 함께 총 6개를 실행한다.

Red에서 미구현 TryExcavate 때문에 Actor 검사 실패를 확인했다. 초기 Walk 실패는 헤드리스 컨트롤러/이동 초기화 문제여서 기능 결함의 근거로 사용하지 않았다. 이후 실제 이동을 수행하도록 테스트 환경을 수정했다. 테스트 월드 종료 경고도 EndPlay 호출로 수정했다.

빌드는 UE 5.8.4 검증용 옵션 `-NoPCH -NoSharedPCH -NoUBA -UBADisableWaitOnMem -MaxParallelActions=3 -CompilerArguments=/FIUObject/Package.h -OverrideBuildEnvironment`를 사용한다. 기존 공용 테스트의 include/PCH 환경 문제를 프로젝트 파일 수정 없이 분리한 방식이다. 기본 옵션 빌드까지 해결했다고 보고하지 않는다. 신규 cpp 검색에는 UBT `-gather`를 사용했다.

초기화 중 UnifiedErrorTest의 Condition failed 로그는 선택 테스트와 별도다. 엔진 종료 코드만 보지 않고 JSON의 succeeded/failed/notRun을 확인한다. 수동 PIE, 실제 화면, Blueprint 재로드 호환성, 패키지, 멀티플레이, AI 경로 갱신은 미검증이다.

## 성능과 다음 작업

현재는 타격마다 작은 볼륨 전체의 표면·충돌을 동기 재생성한다. 큰 격자·연속 타격·다수 흙에서는 프레임 지연과 두 메시의 메모리 비용이 위험 요소다. E1 격자 제한은 CPU 입력 제한이며 네트워크 허용 크기나 출시 성능 보증이 아니다. 비동기 처리·청크 분할은 측정 후 별도 변경으로 검토한다.

다음은 E3의 실제 서버 도구 타격 연결이다. 플레이어/장비 담당이 사용하는 타격 함수와 장착 도구·사용 가능 상태·반경/간격 조회 계약을 확인해야 한다. 별도 공용 캐릭터·인벤토리·세션 시스템을 만들지 않는다. 변경 영역은 보강 가능한 천장이나 붕괴 구간의 확정 판정이 아니다.

## 최종 결과

- UE 5.8.4 검증용 Editor 빌드 성공(종료0). 기본 빌드 환경까지 해결한 것은 아니다.
- `DIGETHER.Dain.Soil` 선택 Automation: 성공6, 경고 동반 성공0, 실패0, 미실행0. 엔진과 검사 스크립트 종료0.
- 벽: 굴착 전 X=189.90cm에서 정지, 유효 굴착8회 후 X=923.10cm까지 실제 걷기, 같은 구멍 복귀 검사 통과.
- 바닥: 유효 굴착3회 후 캐릭터 Z=-610.99cm까지 낙하. 천장: 유효 굴착3회 후 시험 점프로 최고 Z=1092.14cm 도달.
- 기존 Source/Content 1,319개 SHA-256 대조: 이번 대상 Actor 헤더/소스 두 개만 변경. 모든 기존 에셋·맵·DainLab 원본 동일.
- 기존 `.vsconfig`, `Config/DefaultEngine.ini` 변경 보존. `git diff --check`는 기존 DefaultEngine.ini 끝 빈 줄을 지적하며 해당 파일은 수정하지 않았다.
- 증거: 프로젝트 `Saved/DainLabSetup/e2_final_build.log`, `e2_final.log`, `e2_final_report/index.json`, `e2_before.json`, `e2_preservation.json`, `Run-E2.ps1` (로컬 검증 산출물).
- 브랜치/HEAD 유지, Commit·Push·PR 없음. E2의 지형 Actor·자동 이동 범위는 검증했지만 실제 팀 Pawn/도구와 수동 PIE·화면 검증은 남아 있다.
