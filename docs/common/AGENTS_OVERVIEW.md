# Unreal 에이전트 역할

역할 정의는 작업을 위임할 때의 계약이다. 설치만으로 병렬 실행·새 권한·외부 메시지·프로젝트 정책이 승인되는 것은 아니다. [분담 절차](../ai/WORK_ORCHESTRATION.md)와 실제 실행 환경의 지침을 먼저 따른다.

| Claude 이름 / Codex 이름 | 담당 | 쓰기 범위 | 본문 |
| --- | --- | --- | --- |
| ue-architect / ue_architect | Unreal 요구사항과 실제 코드·에셋 구조를 읽고 책임·수명·의존성과 구현 단계를 설계한다 | 없음 | [역할](../ai/roles/ue-architect.md) |
| ue-implementer / ue_implementer | 승인된 Unreal 기능·수정 범위를 C++와 관련 텍스트 파일에 구현하고 필요한 검증을 수행한다 | 할당된 텍스트 파일·검사 산출물 | [역할](../ai/roles/ue-implementer.md) |
| ue-network-reviewer / ue_network_reviewer | Unreal 멀티플레이 변경의 authority·ownership·RPC·복제·합류와 재접속을 읽기 전용으로 검토한다 | 없음 | [역할](../ai/roles/ue-network-reviewer.md) |
| ue-asset-data-reviewer / ue_asset_data_reviewer | Unreal 에셋·패키지·참조·데이터·세이브 변경의 안전성과 호환성을 읽기 전용으로 검토한다 | 없음 | [역할](../ai/roles/ue-asset-data-reviewer.md) |
| ue-performance-analyst / ue_performance_analyst | 실제 Unreal 성능 측정과 코드·설정을 읽어 병목 가설과 재현 가능한 비교 계획을 만든다 | 없음 | [역할](../ai/roles/ue-performance-analyst.md) |
| ue-test-verifier / ue_test_verifier | 승인된 Unreal 빌드·테스트·멀티플레이·성능 검사를 수행하고 실제 증거와 미검증 범위를 보고한다 | 승인된 빌드·검사 산출물만 | [역할](../ai/roles/ue-test-verifier.md) |
| ue-reviewer / ue_reviewer | 최종 Unreal 코드·문서·설정 diff와 검증 증거를 독립적으로 읽어 실제 결함·범위 이탈·완료 누락을 검토한다 | 없음 | [역할](../ai/roles/ue-reviewer.md) |

## 원본과 어댑터

[역할 목록 메타데이터](../../agents/catalog.json)와 docs/ai/roles/ 본문이 원본이다. .claude/agents(Markdown/YAML)와 .codex/agents(TOML)는 같은 본문의 복사본이다. 원본을 고치면 두 복사본을 같은 작업에서 직접 맞춘다([동기화](INTEGRATION.md)). 구체적인 설치·갱신·호출과 버전 한계는 [통합 안내](INTEGRATION.md)를 따른다.

Claude·Codex 에이전트 본문은 서브에이전트의 시스템 프롬프트이고 프로젝트 루트에서 실행되므로, .claude/agents와 .codex/agents 복사본의 로컬 링크는 파일 기준 상대경로가 아니라 프로젝트 루트 기준 경로(docs/…, .agents/skills/…)로 쓴다. docs/ai/roles/의 원본은 기존처럼 파일 기준 링크를 유지한다.

Claude 읽기 역할은 Read/Grep/Glob만 허용한다. 구현과 실행 검증 역할에는 셸이 포함되어 있으므로 역할 계약 밖의 쓰기까지 기술적으로 모두 차단하는 구성은 아니다. MCP 도구는 이 allowlist에 포함하지 않는다.

Codex는 읽기 역할에 read-only, 구현·검증 역할에 workspace-write를 지정한다. 파일별 쓰기 경계는 지시문 계약이며 상위 세션 설정·실행 중 override·상속된 연결 도구까지 이 파일만으로 제한하지 못한다. 프로젝트의 실제 권한과 MCP 설정을 별도로 확인한다. 검토 역할은 외부 상태를 변경하는 연결 도구도 사용하지 않는다.

## 인계 예

구조 변경은 architect 분석 → implementer 구현 → test-verifier 실행 → reviewer 최종 확인으로 구성할 수 있다. 작은 수정에 모든 역할을 호출할 필요는 없다. 네트워크·에셋·성능 전문 역할은 해당 변경에만 사용한다. 같은 파일이나 패키지의 동시 쓰기를 배정하지 않는다. 각 요청에 대상 경로·완료 기준·검사·허용 출력 위치를 함께 준다.
