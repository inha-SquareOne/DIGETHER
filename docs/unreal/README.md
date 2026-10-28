# Unreal 기술 지침

실제 프로젝트의 엔진 버전·플랫폼·모듈·플러그인과 승인된 선택을 먼저 확인한다. 여기의 일반 기준으로 게임 정책이나 도구 지원을 추측하지 않는다.

| 변경 | 기준 |
| --- | --- |
| GameplayTag·Asset Manager·입력·Subsystem 등 표준 기능 | [표준 엔진 기능](STANDARD_FEATURES.md) |
| C++·Blueprint 구조와 수명 | [개발 패턴](ENGINEERING_PATTERNS.md) |
| authority·RPC·복제·합류 | [네트워크 권한](NETWORK_AUTHORITY.md) |
| 맵·패키지·참조·소스 관리 | [에셋 안전](ASSET_SAFETY.md) |
| 설정·데이터·저장·마이그레이션 | [데이터 작업](DATA_WORKFLOW.md) |
| 엔진 통합 검사 | [테스트](TESTING.md) |
| 측정·비교·회귀 | [성능](PERFORMANCE.md) |
| 모델·재질·도입 | [아트 제작](ART_PRODUCTION.md) |

프로젝트별 선택은 [프로필](../templates/PROJECT_PROFILE_TEMPLATE.md), 작성/리뷰/잠금 범위는 [경계표](../templates/PROJECT_BOUNDARIES_TEMPLATE.md)에 기록한다. 실제 명령·API·검사 결과의 한계를 유지한다.
