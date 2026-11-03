# ADR-003: Unreal Engine 5.8.4

- 상태: 채택, 사용자 명시 결정. [ADR-002](ADR-002-engine-version.md)를 대체
- 날짜: 2026-11-03
- 결정: DIGETHER 엔진 버전은 **5.8.4**
- 근거: 사용자가 "5.8.4로 진행"이라고 선택

## 배경

2026-11-03 코드 작업 중 Epic Games Launcher가 작업 PC의 엔진 설치를 5.8.2에서 5.8.4로 자동 업데이트했다(`Engine/Build/Build.version`: 5.8.4 / Changelist 58902052 / CompatibleChangelist 55116800, `++UE5+Release-5.8`). 런처는 이전 핫픽스를 골라 설치하는 기능을 제공하지 않아 5.8.2를 유지하려면 소스 빌드 등 별도 설치가 필요했다. 같은 5.8의 핫픽스이고 호환 Changelist가 같아 맞추는 비용이 가장 작은 쪽을 선택했다.

## 영향

- 팀 전원이 5.8.4를 쓴다. 각자 Epic Games Launcher에서 엔진 자동 업데이트를 끈다. 다른 버전으로 임의 대체하거나 자동 업그레이드하지 않는다는 원칙은 그대로다.
- 검사 실행기(`scripts/Invoke-UnrealChecks.ps1`), 프로토타입 실행기(`scripts/Launch-Prototype.ps1`), 개발 도구 패널의 엔진 버전 표시가 5.8.4를 기준으로 판정한다.
- 5.8.2에서 한 엔진 소스 조사([엔진 기능 활용 범위](../design/UNREAL_FEATURE_COVERAGE.md), [굴착 구현](../design/EXCAVATION_IMPLEMENTATION.md), [전체 시스템 설계](../SYSTEM_DESIGN.md)의 재접속 경로 등)와 과거 빌드·실행 기록은 그 시점의 사실로 보존한다. 핫픽스라 동작이 같을 가능성이 높지만, 해당 기능을 고칠 때 5.8.4 소스·실행으로 다시 확인한다.
- 이 문서는 설치 방식·SDK·플러그인·플랫폼 호환성의 검증이 아니다. 실제 빌드·테스트 결과는 작업 기록과 [검증 계획](../verification/VERIFICATION_PLAN.md)에서 구분한다.
