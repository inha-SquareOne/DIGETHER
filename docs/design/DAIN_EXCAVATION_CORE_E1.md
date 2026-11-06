# E1 굴착 계산 코어 구현·검증

기준일: 2026-11-06. [통합 설계](DAIN_EXCAVATION_INTEGRATION.md)의 E1 범위다. 제품 기능에 사용할 순수 계산을 구현했다. 아직 Actor에 연결하지 않았으며 S07 전체 완료가 아니다.

## 추가 파일과 역할

`DIGETHER/Source/DIGETHER/Game/Excavation/Dain/`에만 C++ 3개를 추가했다.

| 파일 | 역할 |
|---|---|
| DigetherSoilField.h | 격자·표면 데이터, 초기화/굴착/표면 생성/좌표 변환 API |
| DigetherSoilField.cpp | 계산 구현 약 150줄. 기존 DainField의 tetrahedra·UE winding 재사용, 닫힌 초기 볼륨·보호 범위·결과 구분 추가 |
| DigetherSoilFieldTests.cpp | Topology / Carve / Validation 자동 테스트 3개 |

Actor·입력·RPC·UObject·Physics 소유권을 넣지 않았다. 격자는 FGrid의 각 축 셀 수와 간격으로 정한다. 외부 한 겹은 불변 air이며 실질 표면은 샘플 영역 안쪽 반 셀에서 닫힌다. 모서리는 샘플링/삼각형 근사이며 정확한 CAD 상자가 아니다.

## 동작 계약

- Initialize: 모든 방향에서 닫힌 초기 흙을 생성한다. 입력이 잘못되면 출력 배열을 유지한다.
- Carve: 입력 전체를 먼저 확인한 뒤 밀도를 줄인다. InvalidInput / NoChange / Changed를 구분한다. 데이터가 바뀐 범위에 인접 셀 여유를 포함해 반환한다. 이 박스는 붕괴·천장 판정 결과가 아니다.
- ProtectedBoxes: 로컬 좌표의 보호 구역. 한 셀 halo 안의 샘플도 보존하여 그 구역의 보간 표면이 변하지 않도록 한다. 보호 구역보다 넓은 범위가 남을 수 있다는 의도적 보수 처리를 문서화한다.
- BuildSurface: 밀도/외부 경계 유효성 검사 후 표면을 계산한다. 완전히 빈 흙은 true + 빈 배열로 반환한다. 실패는 이전 출력 표면을 유지한다.
- WorldToLocal: 회전·이동만 허용한다. 스케일 1이 아닌 Transform은 거절한다. 크기는 격자 설정으로 지정한다.
- 128을 표면으로 하는 uint8 거리장과 2cm 단위를 유지한다. 양/음 양자화는 대칭 반올림으로 보정했다. 표면점은 1cm 단위로 양자화한다. 타격별 HP/진행량은 추가하지 않았다.
- 셀은 축별 4~32, 총 샘플 32,768 이하, 간격 2~200cm, 반경은 반 셀 이상/도메인 대각선 이하로 제한한다. 이는 이번 계산 코드의 안전 상한이며 기획 수치·성능 보장·복제 허용 크기가 아니다. 네트워크 적용 시 별도의 작은 한도/분할 검증이 필수다.

호출자는 현재 상태의 사본에 Carve를 적용하고, 메시·충돌 준비 성공 후에만 현재 상태와 revision을 확정해야 한다. 코어가 Actor의 원자 교체나 충돌 성공까지 보장하지는 않는다.

## 검증 결과

UE 5.8.4 Editor 빌드와 실제 Automation 실행으로 확인했다. 단순 포팅 소스 검사나 Python 재구현 결과가 아니다.

| 실행 | 결과 |
|---|---|
| 수정 전 Red | Topology·Carve 실패, Validation 통과. 열린 판이 닫힌 볼륨 계약을 만족하지 않음을 새 개인 구현에서 재현 |
| 첫 수정 | Carve·Validation 통과, 표면 경계 위치 검사 실패. 양/음 반올림 비대칭 확인 |
| 최종 | Topology·Carve·Validation 성공 3, 실패 0, 미실행 0, 경고 있는 선택 테스트 0 |

검사: 여섯 면의 경계 위치, 모든 기하학적 모서리가 두 삼각형에 연결됨, 바깥 법선과 UE winding, 굴착 후 닫힘, 빈 지형·전체 제거, 같은 요청의 무변화, 동일 입력 결과, 변경 범위, 보호 구역과 여유 샘플 보존, 외부 air 불변, 바닥/벽/천장 회전에서 동일 밀도, 스케일 거절, NaN/무한 반경·잘못된 배열·설정·보호 구역 거절, 실패 시 기존 데이터 보존.

실제 충돌 cook·캐릭터 이동·렌더 검사는 이번 E1에 포함되지 않는다. 성능·최대 크기 스트레스·다중 청크·네트워크 결정론/재접속/패키지 검증도 미수행이다.

## 빌드 환경의 제한

기본 PCH 빌드는 컴파일이 진행되지 않아 이번 실행만 중단했다. NoPCH로 진행하자 기존 공용 테스트 `DigetherMonsterDefinitionTest.cpp`, `DigetherPrototypeProbe.cpp`의 UPackage 포함 누락이 드러났다. 공용 소스 수정 없이 이번 빌드 명령에만 다음 옵션을 사용했다.

```text
-NoUBA -NoPCH -NoSharedPCH -UBADisableWaitOnMem -MaxParallelActions=3
-CompilerArguments=/FIUObject/Package.h -OverrideBuildEnvironment
```

따라서 최종 결과는 이 검증용 옵션의 Editor 빌드 성공이며, 기본 옵션 빌드 성공으로 보고하지 않는다. 엔진/프로젝트 설정 파일은 변경하지 않았다. 생성된 Editor DLL은 최종 새 코어를 포함한다.

Automation 프로세스는 실패한 Red에서도 종료 코드 0이었다. 그래서 성공 판정은 `index.json`의 실제 선택 테스트 결과로 했다. 실행 스크립트도 JSON 실패/누락을 실패 코드로 반환하도록 보완했다. 엔진 초기화 중 `UE::UnifiedErrorTest` 관련 메시지 뒤에 `Condition failed` 오류가 별도로 남는다. 선택한 3개 테스트의 통과와 구분하며 명령행 전체 오류 없음으로 표시하지 않는다. 해당 초기화 로그의 정확한 원인은 이번 범위에서 확정하지 않았다.

## 보존과 재실행

시작 전 기존 Source·Content 1,316개 파일 SHA-256과 종료 후 비교: 변경 0개. 공용 맵·코드, 기존 DainLab과 BP_DiggableSoil 유지. 기존 .vsconfig/DefaultEngine.ini 변경 보존. commit/push/PR 없음.

증거는 프로젝트 `Saved/DainLabSetup/`의 `e1_red_report/index.json`, `e1_final_report/index.json`, `e1_final_build.log`, `e1_final.log`, `e1_before.json`이다. 검증 실행기는 `Run-E1.ps1 -Stage <새이름>`이다. 새 빌드도 위 검증 옵션으로 먼저 수행해야 한다. Saved 파일은 로컬 증거이며 커밋된 팀 CI가 아니다.

다음은 E2: 개인 지형 Actor가 이 코어의 후보 데이터와 표면을 받아 PMC 충돌 준비 후 확정하도록 연결한다. 빈 표면에서는 기존 충돌을 해제하고 실패 시 이전 유효 상태를 보존한다. 기존 공용 캐릭터·맵은 교체하지 않는다.
