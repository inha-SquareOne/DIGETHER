# 외부 자료와 라이선스

## ECC에서 선별

공식 저장소: [affaan-m/ECC](https://github.com/affaan-m/ECC). 기준 [커밋 4eb71d92a39cab44ad40ac9d8a6a5ccb4029d6c2](https://github.com/affaan-m/ECC/commit/4eb71d92a39cab44ad40ac9d8a6a5ccb4029d6c2).

| ECC 원본 | 채택 결과 | 조정·중복 처리 |
| --- | --- | --- |
| [cpp-coding-standards](https://github.com/affaan-m/ECC/blob/4eb71d92a39cab44ad40ac9d8a6a5ccb4029d6c2/skills/cpp-coding-standards/SKILL.md) | [unreal-cpp-guidelines](../../.agents/skills/unreal-cpp-guidelines/SKILL.md) | 타입·수명·안전 인터페이스를 채택. Epic 명명·UObject/GC·프로젝트 컨테이너·예외/빌드 정책 우선 |
| [cpp-testing](https://github.com/affaan-m/ECC/blob/4eb71d92a39cab44ad40ac9d8a6a5ccb4029d6c2/skills/cpp-testing/SKILL.md) | [unreal-test-workflow](../../.agents/skills/unreal-test-workflow/SKILL.md) | 결정적 검사·격리·실패 재현 채택. GoogleTest/CMake 자동 도입 제외, TDD 공통 규칙은 기존 문서 참조 |
| [search-first](https://github.com/affaan-m/ECC/blob/4eb71d92a39cab44ad40ac9d8a6a5ccb4029d6c2/skills/search-first/SKILL.md) | [search-first](../../.agents/skills/search-first/SKILL.md) | 저장소·엔진/공식 자료·호환 의존성 조사, 자동 설치와 연구 에이전트 강제 실행 제거 |
| [iterative-retrieval](https://github.com/affaan-m/ECC/blob/4eb71d92a39cab44ad40ac9d8a6a5ccb4029d6c2/skills/iterative-retrieval/SKILL.md) | search-first에 통합 | 후보→관련성→공백→재탐색, 고정 점수/횟수로 사실 확정하지 않음, 바이너리 한계 명시 |
| [architecture-decision-records](https://github.com/affaan-m/ECC/blob/4eb71d92a39cab44ad40ac9d8a6a5ccb4029d6c2/skills/architecture-decision-records/SKILL.md) | [동명 스킬](../../.agents/skills/architecture-decision-records/SKILL.md) | 상태·대안·결과·대체 이력 채택, 기존 기록 위치 우선, 승인된 기록의 반복 승인 제거 |
| [benchmark-optimization-loop](https://github.com/affaan-m/ECC/blob/4eb71d92a39cab44ad40ac9d8a6a5ccb4029d6c2/skills/benchmark-optimization-loop/SKILL.md) | [unreal-performance-loop](../../.agents/skills/unreal-performance-loop/SKILL.md) | 기준선·정확성·한 가설·예산·재측정, 맵/인원/빌드 조건과 역할 권한 연결, 자동 커밋 제외 |

ECC 전체 패키지·에이전트·훅·셸 스크립트·MCP 설정·인증·전역 설정은 가져오거나 실행하지 않았다. blueprint는 Unreal Blueprint 스크립팅 스킬이 아니므로 이름만으로 채택하지 않았다. 웹·DB·클라우드 전용 스킬과 중복 검증 체계는 확장 범위에서 제외했다.

원본은 MIT, Copyright (c) 2026 Affaan Mustafa. [원본 LICENSE](https://github.com/affaan-m/ECC/blob/4eb71d92a39cab44ad40ac9d8a6a5ccb4029d6c2/LICENSE)의 전문을 [ECC_LICENSE.txt](../../provenance/ECC_LICENSE.txt)에 보존했다. 수정된 스킬과 이 고지는 함께 복사된다. 이 라이선스가 원본과 무관한 전체 공용 문서의 라이선스를 새로 정하는 것은 아니다.


프로젝트 추가 스킬 출처는 [스킬 목록](../tools/SKILLS.md)을 참조한다.
