# L_DainExcavation — E2 굴착 연결과 직접 시험

2026-11-06. 사용자 요청으로 개인 맵을 새 ADigetherDiggableSoil에 연결했다. [E2 구현](DAIN_EXCAVATION_ACTOR_E2.md)의 맵/입력 미연결 상태를 이 문서 범위에서 갱신한다. 팀 공용 도구·멀티플레이 통합은 아니다.

## 직접 실행

1. UE 5.8.4로 저장소의 `DIGETHER/DIGETHER.uproject`를 연다.
2. 콘텐츠 브라우저의 `콘텐츠 → Developers → Dain → Excavation → Maps`에서 `L_DainExcavation`을 연다. 다른 `L_ExcavationLab`과 구분한다. Developers가 숨겨져 있으면 콘텐츠 브라우저 설정에서 개발자 콘텐츠 표시를 켠다.
3. 플레이 설정: 플레이어 수1, Net Mode Standalone. 상단 재생(선택 뷰포트)을 누른 뒤 화면을 클릭한다. Simulate는 사용하지 않는다.
4. WASD 이동, 마우스로 조준, 좌클릭 또는 좌클릭 유지로 갈색 흙을 반복해서 판다. 사거리400cm, 시험 반경75cm/간격0.35초이며 정식 도구 수치가 아니다.
5. 중앙만 뚫고 몸이 걸리면 발밑과 구멍 가장자리도 조준해 넓힌 뒤 W로 통과한다. 회색 바닥·벽은 파이지 않아야 한다.
6. Esc로 플레이 종료. 다시 재생하면 흙이 초기화된다. Shift+F1은 마우스를 에디터로 돌려준다.

아웃라이너의 `Dain_DiggableSoil` 라벨은 유지했고 실제 클래스는 이제 `DigetherDiggableSoil`이다. 옛 `DigetherDainSoilWall` 클래스가 아니며, BP_DiggableSoil을 따로 배치할 필요가 없다.

## 변경

- 개인 맵 `Content/Developers/Dain/Excavation/Maps/L_DainExcavation.umap`: 기존 흙 Actor 한 개 교체. 위치(675,-450,-25), 단위 스케일, GridCells(5,18,10), CellSpacing50(초기 연결 당시 값. 현재 값은 아래 [표면 개선 후 현재 설정](#표면-개선-후-현재-설정)). 앞면X700, 뒤면X900인 약200cm 흙벽. 나머지 액터/맵 GameMode 유지.
- 개인 머티리얼 `Content/Developers/Dain/Excavation/Materials/MI_DainSoilE2.uasset` 추가. 엔진 BasicShapeMaterial을 재사용해 갈색을 지정하고 두 PMC에 적용.
- 개인 `DainLab/DigetherDainCharacter.cpp`: 첫 타격이 새 Actor이면 TryExcavate 호출. 기존 흙 Actor 분기도 보존.
- 개인 `DainLab/DigetherDainVerification.h/.cpp`: 새/기존 Actor 모두 검사하도록 작은 조회 보조 함수 추가.
- 굴착 계산 코어·새 Actor 본체·공용 캐릭터/설정/맵 변경 없음. Blueprint 에셋 변경 없음.
- `Content/Developers/Dain/` 에셋 4개는 [ADR-004](../decisions/ADR-004-git-code-svn-assets.md)에 따라 Git이 아니라 SVN으로 관리한다. develop 병합 시 Git 추적에서 제외했다.

## 검증 결과

- UE5.8.4 Editor 검증 옵션 빌드 성공, 종료0. E2와 같은 NoPCH/강제 include 옵션으로 기본 빌드 환경 문제와 분리했다.
- 맵 저장·별도 프로세스 재열기 성공. 새 Actor 클래스/격자 확인, X700 앞면 collision trace 성공.
- NullRHI 실제 게임 실행: 통과, 유효 굴착3회, 보호 바닥 굴착 거절, 순간이동0.
- DX11 RenderOffscreen 실제 게임 실행: 통과, 굴착 전X608.78에서 정지, 1회 후X765.32, 반복 후X1050.98 도달. 유효 굴착3회/보호 거절/순간이동0. 캡슐반경35/반높이90. 화면 캡처에서 갈색 표면과 통로 확인.
- 이 검사는 개인 시험 Character의 같은 조준·DigOnce·CharacterMovement를 자동으로 운전한다. 사용자의 수동 PIE, 공용 팀 캐릭터·삽/곡괭이 애니메이션·멀티플레이·패키징은 미검증.
- 시작/종료 feature/Dain, HEAD 4d37fda6f98e18962d0cd9a86b9a33260dfa6462. 기존 Source/Content SHA-256 대조에서 위 개인 코드3개와 개인 맵1개만 변경. 새 재질1개 추가. 공용 맵과 L_ExcavationLab 보존. 기존 .vsconfig/DefaultEngine.ini 수정 보존. Commit/Push/PR 없음.

로컬 증거는 `DIGETHER/Saved/DainLabSetup/`의 `e2map_build.log`, `e2map_check.log`, `e2map_play.log`, `e2map_render.log`, `e2map_changed.json`, `e2map_render_user/Saved/QA/wall-result.json`, `e2map_render_user/Saved/QA/through-wall.png`다. 변경 전 개인 맵은 `E2MapBackup_20261106_154433`에 보관했다.

초기 연결 스크립트는 저장 이후 Python IntVector 래퍼의 직접 비교에서 실패했다. 별도 읽기 검사에서 x/y/z 성분을 확인해 저장값(5,18,10)이 정상임을 확인했다. 최초 빌드와 겹쳐 실행한 commandlet은 초기화에서 진행하지 않아 해당 작업 프로세스만 종료하고 빌드 완료 후 다시 실행했다. 선택 검사 성공을 프로젝트 전체 초기화 로그 무오류로 확대하지 않는다.

다음: 사용자가 실제 조준·굴착 크기·통과감을 확인한다. 정식 팀 도구 연결은 E3 계약 확인 후 진행한다.


## 표면 개선 후 현재 설정

[표면 개선 기록](DAIN_EXCAVATION_SMOOTHING.md): 격자25cm, 셀(9,35,19), SoilBrushRadius75cm로 갱신했다. 위 초기 연결 당시 50cm 격자·3회 굴착 결과와 구분한다. 흙벽 전체 크기와 조작은 유지한다.

