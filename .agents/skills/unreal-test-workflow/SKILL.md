---
name: "unreal-test-workflow"
description: "Unreal C++ 테스트를 작성·수정하거나 실패·불안정한 테스트를 진단한다. 테스트 범위·격리·실행 증거를 설계할 때 사용하며 모든 작은 문서 수정에 테스트 생성을 강제하지 않는다."
---

# Unreal 테스트 작성과 진단

1. 변경 동작·회귀 위험·프로젝트 검사 요구와 기존 테스트 위치/실행기를 먼저 읽는다. [TDD 기준](../../../docs/ai/TDD.md)에 따라 의미 있는 동작 검사가 필요한지 판단한다.
2. 순수 로직, 엔진/월드 의존 기능, 콘텐츠, 네트워크를 구분한다. 프로젝트에서 실제 지원하는 Automation·Functional·Low-Level 또는 별도 테스트 환경을 선택한다. GoogleTest/CTest/CMake를 Unreal 기본 도구라고 가정하거나 자동 설치하지 않는다.
3. 외부 시간·네트워크·파일 상태에 의존하는 단위 검사는 가능한 경우 제어 가능한 입력과 fake로 분리한다. 실제 월드/복제/에셋 동작은 적절한 통합 환경에서 확인한다. mock 결과를 런타임 증거로 확대하지 않는다.
4. fixture·월드·delegate·타이머·임시 파일의 생성과 정리를 검토한다. 상태 누출·고정 임시 경로·임의 sleep을 피하고 실제 완료 조건과 시간 한도를 사용한다. 난수는 재현 가능한 seed와 조건을 남긴다.
5. 결함 테스트는 수정 전 실패 이유부터 확인한다. 수정 후 관련 검사, 필요한 회귀 범위로 넓힌다. 테스트 삭제나 기대값 왜곡으로 통과시키지 않는다.
6. 불안정한 실패는 환경·순서·자원 경합·동기화 조건을 기록해 재현한다. sanitizer·coverage·fuzzing은 해당 엔진/컴파일러/플랫폼의 실제 지원과 승인 범위가 있을 때만 사용한다. 모든 플랫폼 지원이나 고정 coverage 목표를 약속하지 않는다.
7. [검증 기록](../../../docs/ai/VERIFICATION.md)에 명령·버전·타깃·맵·선택/실행/성공/실패/skip 수·로그·종료 상태를 적는다. 미발견 0건, 실행 안 됨, 빌드만 성공한 상태를 테스트 PASS로 바꾸지 않는다.

[Unreal 테스트](../../../docs/unreal/TESTING.md)와 [멀티플레이 검사](../../../docs/ai/MULTIPLAYER_TEST.md)를 함께 따른다. 실행 검증 역할은 테스트 소스를 수정하지 않으므로 테스트 코드 변경은 승인된 구현 담당이 수행한다.

2026-10-10 [Epic Automation 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-test-framework-in-unreal-engine)를 확인했다. 구체적인 실행 명령·테스트 모듈·플래그는 프로젝트와 엔진 버전에서 확인한다. 이 패키지 제작 중 Unreal 테스트를 실행하지 않았다.

## 작업 경계와 근거

[작업 경계](../../../docs/ai/WORK_BOUNDARIES.md), 프로젝트 경계표·CODEOWNERS·기존 변경/잠금을 먼저 확인한다. 경계 충돌 시 미승인 경로로 범위를 넓히지 않는다. 읽기 조사와 파일 수정·외부 실행 권한을 구분한다.

## 출처

[ECC cpp-testing](https://github.com/affaan-m/ECC/blob/4eb71d92a39cab44ad40ac9d8a6a5ccb4029d6c2/skills/cpp-testing/SKILL.md)의 절차를 Unreal 환경에 맞게 수정했다. 기준 커밋은 4eb71d92a39cab44ad40ac9d8a6a5ccb4029d6c2. Copyright (c) 2026 Affaan Mustafa. [MIT 라이선스](../../../provenance/ECC_LICENSE.txt)와 [변경·제외 기록](../../../docs/SOURCES.md)을 함께 유지한다.
