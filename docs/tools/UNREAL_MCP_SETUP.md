# Unreal MCP 연결

2026-10-27 설정, 2026-10-28 검사 프로세스 분리와 오류 수정 상태 갱신. 대상은 `DIGETHER/DIGETHER.uproject`, UE 5.8.2다. 편의 플러그인·허브·로컬 검사 사용법은 [개발 도구 안내](DEVELOPER_TOOLS.md)를 따른다.

## 클라이언트

저장소 루트에서 Claude Code와 Codex를 실행한다. Claude는 루트 `.mcp.json`, Codex는 `.codex/config.toml`의 `unreal-mcp`를 사용한다. 주소는 `http://127.0.0.1:8000/mcp`이며 이 PC의 실행 중인 에디터에 연결한다. 다른 에디터의 포트 점유 여부를 확인한다.

- Claude Code: `claude mcp get unreal-mcp`로 설정을 확인한다. Pending approval이면 `claude`를 실행해 해당 서버를 승인하고 `/mcp`에서 연결을 확인한다. 사용자 승인 절차는 자동 우회하지 않는다.
- Codex: `codex mcp get unreal-mcp`로 설정을 확인한다. 신뢰된 저장소에서 프로젝트 설정을 사용한다. 기존 세션에 도구가 표시되지 않으면 MCP 재연결 또는 새 세션/앱 재시작 후 확인한다.
- 두 클라이언트가 동시에 같은 에셋을 수정하지 않는다. 설정 인식과 실제 도구 호출 성공을 구분한다.

## 에디터

`ModelContextProtocol`과 `AllToolsets`를 활성화했다. 현재 PC의 사용자 설정에 `bAutoStartServer=True`를 저장하고 재조회했다. 경로는 `DIGETHER/Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`이며 Git에서 제외된다. 다른 PC에서는 Editor Preferences의 Model Context Protocol에서 Auto Start Server를 켠다. 2026-10-28 재시작 후 자동 시작과 HTTP 초기화·도구 목록·`list_toolsets`의 실제 응답을 확인했다.

수동 시작은 에디터 콘솔의 `ModelContextProtocol.StartServer`다. `list_toolsets` → `describe_toolset` → `call_tool` 순서로 도구를 발견하고 호출한다. 프로젝트 스킬은 `ToolsetRegistry.AgentSkillToolset.ListSkills`로 확인한다.

로컬 검사 실행기의 Automation·ValidateData는 실행별 결과 폴더에 `EditorPerProjectUserSettings.ini` 복사본을 만들고 `-EditorPerProjectUserSettingsIni=`로 읽기·쓰기 경로를 지정한다. 그 검사 프로세스에 `bAutoStartServer=False` 명령줄 ini override를 전달해 MCP 포트 충돌을 막는다. 경로 격리 없이 override만 사용하면 Unreal의 설정 재저장 때 원래 항목이 빠질 수 있으므로 검사 전용 파일을 사용한다. 이 격리는 해당 ini에 대한 것이며 모든 Editor 설정 파일의 쓰기를 막는 것은 아니다. 검사용 서버 비활성화를 평소 MCP 연결 실패로 해석하지 않는다.

## 확인 결과와 한계

아래 연결 증거는 2026-10-27 실행 당시 결과다. 2026-10-28에는 신규 허브의 7개 UI 바로가기와 재시작 후 MCP 자동 시작을 별도로 확인했다. 로컬 HTTP 연결 성공은 Codex의 네이티브 도구 카탈로그 갱신·노출까지 확인했다는 뜻은 아니다.

- 실행 중인 DIGETHER 에디터에서 HTTP initialize, tools/list, list_toolsets, ListSkills, 설정 조회 성공.
- `/Game/Splash/EdSplash`, `/Game/Splash/Splash` 로드 성공, 두 에셋 모두 dirty=false. 의존성은 `/Script/InterchangeEngine`.
- Codex CLI가 프로젝트 URL·타임아웃 설정을 인식했다. 현재 대화의 네이티브 도구 목록 재로딩은 미확인이다.
- Claude CLI는 프로젝트 설정을 인식했으나 Pending approval을 반환했다. Claude 내부 도구 호출은 사용자 승인 후 확인해야 한다.
- 당시 로그에 GameFeatureData Asset Manager 등록 관련 로드 오류가 있었다. 2026-10-28 규칙 추가 후 DataValidation 재검증은 exit 0·오류/경고 0개였으며, [검사 범위와 한계](DEVELOPER_TOOLS.md#확인-결과와-남은-검증)에 기록한다. 이 결과가 모든 에디터 경로의 무오류를 보장하지는 않는다.
- 이 MCP 연결 확인에서는 C++ 재빌드·PIE·다인·패키징·DX11 시각 품질 비교를 수행하지 않았다. Workbench의 별도 과거 검사와 현재 통합 검사는 [검증 계획](../verification/VERIFICATION_PLAN.md)에서 구분한다.

Android File Server가 에디터 설정 저장 시 SecurityToken을 DefaultEngine.ini에 다시 기록할 수 있다. 커밋 전에 비밀값을 확인하고 제외한다. 이번 값은 Git 제외 로컬 Engine.ini에 보존했다.

근거: 설치 엔진의 ModelContextProtocolSettings.h·ModelContextProtocolClientConfig.cpp, [프로젝트 MCP 스킬](../../.agents/skills/unreal-mcp/SKILL.md), [OpenAI MCP 설정 문서](https://developers.openai.com/codex/mcp).
