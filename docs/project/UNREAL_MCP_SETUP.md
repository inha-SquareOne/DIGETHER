# Unreal MCP 연결

2026-10-27 설정. 대상은 `DIGETHER/DIGETHER.uproject`, UE 5.8.2다.

## 클라이언트

저장소 루트에서 Claude Code와 Codex를 실행한다. Claude는 루트 `.mcp.json`, Codex는 `.codex/config.toml`의 `unreal-mcp`를 사용한다. 주소는 `http://127.0.0.1:8000/mcp`이며 이 PC의 실행 중인 에디터에 연결한다. 다른 에디터의 포트 점유 여부를 확인한다.

- Claude Code: `claude mcp get unreal-mcp`로 설정을 확인한다. Pending approval이면 `claude`를 실행해 해당 서버를 승인하고 `/mcp`에서 연결을 확인한다. 사용자 승인 절차는 자동 우회하지 않는다.
- Codex: `codex mcp get unreal-mcp`로 설정을 확인한다. 신뢰된 저장소에서 프로젝트 설정을 사용한다. 기존 세션에 도구가 표시되지 않으면 MCP 재연결 또는 새 세션/앱 재시작 후 확인한다.
- 두 클라이언트가 동시에 같은 에셋을 수정하지 않는다. 설정 인식과 실제 도구 호출 성공을 구분한다.

## 에디터

`ModelContextProtocol`과 `AllToolsets`를 활성화했다. 현재 PC의 사용자 설정에 `bAutoStartServer=True`를 저장하고 재조회했다. 경로는 `DIGETHER/Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`이며 Git에서 제외된다. 다른 PC에서는 Editor Preferences의 Model Context Protocol에서 Auto Start Server를 켠다. 재시작 후 자동 시작은 이번에 검증하지 않았다.

수동 시작은 에디터 콘솔의 `ModelContextProtocol.StartServer`다. `list_toolsets` → `describe_toolset` → `call_tool` 순서로 도구를 발견하고 호출한다. 프로젝트 스킬은 `ToolsetRegistry.AgentSkillToolset.ListSkills`로 확인한다.

## 확인 결과와 한계

- 실행 중인 DIGETHER 에디터에서 HTTP initialize, tools/list, list_toolsets, ListSkills, 설정 조회 성공.
- `/Game/Splash/EdSplash`, `/Game/Splash/Splash` 로드 성공, 두 에셋 모두 dirty=false. 의존성은 `/Script/InterchangeEngine`.
- Codex CLI가 프로젝트 URL·타임아웃 설정을 인식했다. 현재 대화의 네이티브 도구 목록 재로딩은 미확인이다.
- Claude CLI는 프로젝트 설정을 인식했으나 Pending approval을 반환했다. Claude 내부 도구 호출은 사용자 승인 후 확인해야 한다.
- 기존 로그에 GameFeatureData Asset Manager 등록 관련 로드 오류가 있다. 전체 에디터 로드 무오류로 판정하지 않는다. MCP 읽기 호출은 성공했다.
- C++ 재빌드·PIE·다인·패키징·DX11 시각 품질 비교는 미실행이다.

Android File Server가 에디터 설정 저장 시 SecurityToken을 DefaultEngine.ini에 다시 기록할 수 있다. 커밋 전에 비밀값을 확인하고 제외한다. 이번 값은 Git 제외 로컬 Engine.ini에 보존했다.

근거: 설치 엔진의 ModelContextProtocolSettings.h·ModelContextProtocolClientConfig.cpp, [프로젝트 MCP 스킬](../../.agents/skills/unreal-mcp/SKILL.md), [OpenAI MCP 설정 문서](https://developers.openai.com/codex/mcp).
