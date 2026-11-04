# UE 5.8.4 기능 카탈로그

기준일 2026-11-04 · UE 5.8.4 (CL 58902052) · 상태: **조사 자료, 채택 결정 아님**.

기능을 개발하기 전에 엔진에 무엇이 있는지 처음부터 다시 조사하지 않도록 영역별로 정리한 자료다. 작업할 영역의 파일부터 읽고, 복제·저장·내비게이션처럼 함께 영향을 받는 영역의 근거도 확인해 후보를 고른다. 기획 요구(DG)와 엔진 기능의 대응은 [Unreal 제공 기능과 개발반 구현 범위](../../design/UNREAL_FEATURE_COVERAGE.md), 이미 정한 사용 기준은 [표준 엔진 기능](../STANDARD_FEATURES.md)을 따른다.

## 어느 파일을 읽나

| 작업 | 파일 |
|---|---|
| GameMode·GameState·PlayerState·Controller·Character·이동·카메라·1인칭·Subsystem 종류 | [게임플레이 프레임워크](GAMEPLAY_FRAMEWORK.md) |
| Enhanced Input·키 변경·입력 모드·게임패드 | [입력](INPUT.md) |
| 복제·RPC·관련성·Dormancy·Iris·레벨 이동·재접속 | [네트워크](NETWORKING.md) |
| Steam·Online Subsystem·세션·로비·근거리 음성 | [온라인·음성](ONLINE_VOICE.md) |
| 굴착 지형·런타임 메시·파괴 가능 벽 | [지형·파괴](TERRAIN_DESTRUCTION.md) |
| 충돌 채널·트레이스·물리·Physics Material | [물리·충돌](PHYSICS_COLLISION.md) |
| 몬스터 AI·Perception·BT·StateTree·내비메시 갱신 | [AI·내비게이션](AI_NAVIGATION.md) |
| 절차 생성·PCG·레벨 스트리밍·던전 조립 | [레벨·PCG](LEVELS_PCG.md) |
| DataAsset·DataTable·GameplayTags·설정·SaveGame·현지화 | [데이터·저장](DATA_SAVE.md) |
| UMG·CommonUI·MVVM·HUD·설정 화면 | [UI](UI.md) |
| 카툰 셰이딩·조명·포스트 프로세스·Niagara·스케일러빌리티 | [렌더링·아트](RENDERING_ART.md) |
| MetaSounds·감쇠·차폐·Submix·공포 사운드 | [오디오](AUDIO.md) |
| GAS·데미지·인터랙션·컴포넌트 조합·GameFeatures | [게임플레이 시스템](GAMEPLAY_SYSTEMS.md) |
| AnimBP·Montage 복제·IK·Control Rig·1인칭 팔 | [애니메이션](ANIMATION.md) |
| Automation·Functional Test·로그·치트·멀티 PIE·네트워크 디버깅 | [디버그·테스트](DEBUG_TEST.md) |
| Insights·stat·Tick·패키징·Steam 배포·PSO·DDC | [성능·빌드](PERF_BUILD.md) |

## 읽는 법

- 각 행의 근거 표시는 다음 뜻이다.
  - `소스`: 로컬 5.8.4 헤더·구현 파일·`.uplugin`·`.Build.cs` 또는 명시한 프로젝트 소스·설정을 직접 열어 해당 주장을 확인했다.
  - `문서`: Epic 공식 문서 페이지를 열어 확인했다.
  - `미확인`: 이번 조사에서 확인하지 못했다.
- 목록에 있다고 채택한 것이 아니다. 실제로 고를 때는 [대안 비교와 사용자 선택](../../ai/EXECUTION_POLICY.md#대안-비교와-사용자-선택)을 따른다.
- 엔진에 기능이 있다는 것과 이 프로젝트에서 동작을 확인했다는 것은 별개다. 검증은 [검증 계획](../../verification/VERIFICATION_PLAN.md)을 따른다.
- `.uplugin`의 Beta/Experimental 키가 없다는 사실은 안정성 보증이 아니다. `EnabledByDefault`, 프로젝트의 명시 활성화, 의존 플러그인을 통한 활성화, 실제 모듈 로드·게임 사용은 구분한다. 소스·설정에서 사용처를 찾지 못한 경우에도 Blueprint·에셋 내부 사용까지 미사용으로 단정하지 않는다.
- 검색에서 찾지 못했다고 엔진이 지원하지 않는다는 뜻은 아니다. 그런 항목은 `미확인`이나 확인 한계로 남긴다.

## 갱신 규칙

- 엔진 버전을 바꾸면 기준일과 버전을 고치고, 바뀐 기능의 근거를 다시 확인한다. 이전 버전의 확인 결과는 그 시점의 사실로 표시한다.
- 기능을 실제로 채택하거나 검증하면 해당 행의 "프로젝트 상태"를 고치고, 결정 문서나 검증 기록을 링크한다.
- `미확인` 항목을 확인하면 근거를 `소스`나 `문서`로 바꾼다.
- 개인 PC 절대 경로는 쓰지 않는다. 엔진 경로는 `Engine/...` 상대 경로로 적는다.
