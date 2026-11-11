#pragma once

#include "CoreMinimal.h"
#include "HttpRouteHandle.h"
#include "HttpResultCallback.h"

class IHttpRouter;
struct FHttpServerRequest;

// 같은 PC 브라우저용 개발 웹 서버(엔진 HTTPServer, localhost 바인드)
// 화면은 모듈에 내장, API는 등록부 명령만 실행(임의 콘솔 문자열 실행 경로 없음)
class FDigetherDevWebServer
{
public:
	static constexpr uint32 DefaultPort = 30080;
	static constexpr uint32 PortAttempts = 10;

	bool Start(uint32 PreferredPort);
	void Stop();
	bool IsRunning() const { return Router.IsValid(); }
	uint32 GetPort() const { return Port; }
	FString GetUrl() const;

private:
	bool HandleIndex(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleCommands(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleCommandsMarkdown(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleRun(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleState(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool Preprocess(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	TSharedPtr<IHttpRouter> Router;
	TArray<FHttpRouteHandle> Routes;
	FDelegateHandle PreprocessorHandle;
	uint32 Port = 0;
};
