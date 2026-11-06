# Dain 굴착 표면 개선 — 2026-11-06

사용자가 자연스러운 흙 굴착을 요청하여 기존 밀도 격자 + Marching Tetrahedra + ProceduralMeshComponent를 유지하고 개선했다. 공용 코드/맵/설정, 기존 DainSoilWall 계산은 수정하지 않는다.

## 변경

- `Dain/DigetherSoilField.cpp`: 격자에서 중앙차분(경계는 단측차분)으로 공기 방향 밀도 기울기를 계산하고, 생성 정점에서 삼선형 보간해 노멀을 만든다. 동일 위치는 동일 노멀을 사용한다. 정점 위치·인덱스를 바꾸는 메시 스무딩이 아니므로 이 처리 자체는 충돌을 변형하지 않는다. 기울기0에서는 기존 면 노멀을 유지한다.
- 같은 파일과 `Dain/DigetherDiggableSoil.h`: 축별 최대 셀64로 확장하되 총 샘플32768 제한은 유지. 개인 맵 Y축35셀을 수용하기 위한 변경이며 네트워크 허용 크기를 변경한 것이 아니다.
- `Dain/DigetherSoilFieldTests.cpp`: 파낸 표면의 같은 위치에 있는 정점 노멀 연속성과 단위 길이 검사 추가.
- `DainLab/DigetherDainCharacter.h/.cpp`: 새 흙 전용 SoilBrushRadius75cm 추가. 기존 흙용 BrushRadius125cm는 유지. 시험 타격 간격0.35초는 유지. 최종 삽/곡괭이 수치가 아니다.
- 개인 `L_DainExcavation`: 격자50→25cm, 셀(5,18,10)→(9,35,19), 위치(675,-450,-25)→(687.5,-437.5,-12.5). 실제 흙벽 앞면X700/뒤면X900, 가로 폭850/높이450cm 유지. 샘플1254→7200.
- `DainLab/DigetherDainVerification.cpp`: 평평한 직선 캡슐 sweep 대신 실제 CharacterMovement의 통과 후 같은 구멍 복귀를 검사한다. 작은 굴착은 발밑 턱을 남기며 캐릭터의 기존 MaxStepHeight45cm로 넘어갈 수 있다. 고정Z140 및 실제 지면Z92.15 직선 sweep 모두 이 걷기 경로와 달랐다. 캡슐 반경35/반높이90, 기존 이동 설정, 벽 통과 X1050 기준은 유지하고 추가 굴착 없이 X600까지 복귀해야 한다. 곧고 평평한 통로가 확보됐다는 주장은 하지 않는다.

별도 플러그인, 무작위 굴착 모양, 텍스처 에셋을 추가하지 않았다. 첨부 게임의 기술/외형을 그대로 재현했다고 주장하지 않는다. 밀도 제거 브러시는 여전히 구형이며 다음 튜닝은 사용자 직접 체감에 따라 정한다.

## 검사와 근거

- Red: 변경 전 연속 노멀 검사 실패(선택6개 중5성공1실패). Green: 노멀 변경 후6성공0경고0실패0미실행. 지형 닫힘/와인딩/보호영역/실패 보존/빈 지형 및 실제 엔진 CharacterMovement 검사 포함.
- 검증용 옵션 Editor 빌드 성공. 기존 NoPCH/강제 include 옵션 사용, 기본 빌드 환경 해결을 주장하지 않는다.
- 맵 저장·재열기, 새 격자와 X700 앞면 충돌 확인 성공.
- 첫 DX11 실행에서는 유효 굴착6회 후 실제 캐릭터 X1050.60 도달했지만 고정Z140 통로 검사가 실패했다. 최종 결과는 아래 별도 기록한다.
- 시작 시 에디터 없음. 개인 맵 백업: 프로젝트 Saved/DainLabSetup/SmoothBackup_20261106_160423.
- 코어/맵 증거: Saved/DainLabSetup/e2_smooth_red_report/index.json, e2_smooth_green_report/index.json, smooth_build.log, smooth_final_build.log, smooth_map.log.

## 직접 테스트

동일 프로젝트에서 `콘텐츠 → Developers → Dain → Excavation → Maps → L_DainExcavation`을 열고 1인 Standalone으로 재생한다. WASD/마우스 조준/좌클릭 유지. 이전보다 작은 범위를 파므로 발밑과 가장자리를 넓혀 통과한다. Esc 후 재생하면 초기화된다. 회색 벽·바닥은 굴착 금지다. 같은 C++ 변경을 읽으려면 기존 에디터를 재시작한다.

조작은 개인 시험 캐릭터용이며 팀 공용 도구나 멀티플레이 연결이 아니다. 데이터가 약5.74배 늘어 전체 표면/충돌 동기 재생성 비용도 증가할 수 있다. 목표 하드웨어·4인·다수 흙의 성능은 미검증이므로 전체 갱도에 일괄 적용하지 않는다.


## 최종 결과

- 최종 검증 옵션 Editor 빌드 성공. 코어/Actor 선택 테스트6개 성공(경고/실패/미실행0).
- `smooth_return.log` DX11 실제 게임 검증 passed=true. 벽 앞X607.51 정지, 한 번 굴착 후X664.97, 반복6회 후X1050 이상 통과, 추가 굴착 없이X599.00 복귀. 보호 바닥 거절, 순간이동0. 실제 캡슐반경35/반높이90 유지.
- `smooth_return_user/Saved/QA/through-wall.png` 확인: 면별 삼각형 음영 감소. 작은 윤곽의 각과 어두운 내부 조명은 남는다. 텍스처/조명까지 완성한 것은 아니다.
- 첫 실행과 실제 지면 높이로 바꾼 두 번째 실행에서는 직선 sweep 검사가 실패했다. 실제 걷기는 턱을 넘으므로 최종 검사를 실제 캐릭터의 왕복으로 교체했다. 평평한 직선 통로 확보를 성공 조건으로 바꾸어 주장하지 않는다.
- 시작/종료 feature/Dain, HEAD 4d37fda6f98e18962d0cd9a86b9a33260dfa6462 유지. 기존 파일 SHA-256 대조에서 개인 코드6개와 L_DainExcavation만 변경. 공용 코드/맵 및 L_ExcavationLab, 기존 .vsconfig/DefaultEngine.ini 변경 보존. Commit/Push/PR 없음.
- 최종 로컬 증거: Saved/DainLabSetup/smooth_return_build.log, smooth_return.log, smooth_return_user/Saved/QA/wall-result.json, through-wall.png, smooth_changed.json.
- 수동 PIE 조작·손맛 만족 여부, 실제 팀 도구, 멀티플레이, 다수 흙 부하·패키지는 미검증. 다음은 사용자의 직접 체감 확인이다.
