---
name: "unreal-cpp-guidelines"
description: "Unreal C++ 작성·리뷰·리팩터링에서 타입·수명·리소스·동시성 위험을 점검한다. Epic 및 프로젝트 규칙에 맞춰 일반 C++ 원칙을 적용할 때 사용한다."
---

# Unreal C++ 점검

## 적용 순서

실제 엔진 패치·컴파일러·모듈 설정·타깃과 기존 코딩 관례를 확인한다. 언어 기능의 지원을 추측하지 않는다. 리플렉션·GC·ABI·직렬화 제약과 Epic의 해당 버전 기준을 먼저 충족하고 프로젝트 규칙과 일반 C++ 원칙을 조정한다.

## 점검

- 초기화, const 의도, 값/참조 전달, 축소 변환, 유효 범위·단위·오류 처리를 읽는다. 객체의 상태 불변식과 경계 조건이 실제 코드에 표현되어 있는지 확인한다.
- UObject 관리와 일반 C++ 리소스를 구분한다. UObject를 표준 unique_ptr/shared_ptr 소유로 바꾸지 않는다. 실제 엔진의 생성·파괴·GC 참조 경로와 리플렉션 속성을 확인한다. TObjectPtr 이름만으로 GC 유지가 보장된다고 가정하지 않는다. 약한 참조·soft 참조는 실제 수명/로딩 요구에 맞춰 고른다.
- 비 UObject 리소스에는 해당 자원과 프로젝트에 맞는 RAII를 검토한다. 콜백·타이머·delegate·비동기 작업의 취소와 객체 파괴 후 접근을 점검한다.
- 엔진 코드의 이름·접두사와 사용 중인 컨테이너·문자열·인터페이스 관례를 따른다. std::vector/std::string, underscore 명명, 표준 스마트 포인터를 일괄 기본값으로 강제하지 않는다. 외부 라이브러리 경계는 별도 제약을 확인한다.
- 예외·RTTI·언어 표준을 켜는 변경은 빌드/ABI 영향이 있으므로 기존 모듈 설정과 실제 지원을 확인한다. 일반 C++ 예제를 이유로 설정을 바꾸거나 throw 기반 오류 처리를 도입하지 않는다.
- 공유 가변 상태와 동기화, 잠금 중 외부 콜백, 스레드별 엔진 접근 제약을 확인한다. 성능 변경은 [측정 기준](../../../docs/unreal/PERFORMANCE.md)으로 검증한다.
- UHT 선언·generated 헤더·모듈 의존성 변경은 실제 빌드와 필요한 Editor/런타임 검사로 확인한다. 일반 컴파일 검사만으로 엔진 통합 성공을 선언하지 않는다.

[개발 패턴](../../../docs/unreal/ENGINEERING_PATTERNS.md)의 원칙을 기준으로 실제 위치·위험·수정·검사 결과를 보고한다.

## 기술 확인 범위

2026-10-10 Epic의 [C++ 규칙](https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine)과 [객체 포인터](https://dev.epicgames.com/documentation/en-us/unreal-engine/object-pointers-in-unreal-engine) 문서를 확인했다. 열람 페이지는 UE 5.8 기준이며 모든 이전/향후 버전에 같은 API가 있다는 뜻은 아니다. 실행 시 프로젝트 버전과 실제 선언으로 재확인한다.

## 작업 경계와 근거

[작업 경계](../../../docs/ai/WORK_BOUNDARIES.md), 프로젝트 경계표·CODEOWNERS·기존 변경/잠금을 먼저 확인한다. 경계 충돌 시 미승인 경로로 범위를 넓히지 않는다. 읽기 조사와 파일 수정·외부 실행 권한을 구분한다.

## 출처

[ECC cpp-coding-standards](https://github.com/affaan-m/ECC/blob/4eb71d92a39cab44ad40ac9d8a6a5ccb4029d6c2/skills/cpp-coding-standards/SKILL.md)의 절차를 Unreal 환경에 맞게 수정했다. 기준 커밋은 4eb71d92a39cab44ad40ac9d8a6a5ccb4029d6c2. Copyright (c) 2026 Affaan Mustafa. [MIT 라이선스](../../../provenance/ECC_LICENSE.txt)와 [변경·제외 기록](../../../docs/SOURCES.md)을 함께 유지한다.
