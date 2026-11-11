#include "DigetherDevWebServer.h"

#include "Async/Async.h"
#include "DigetherDevDebugDraw.h"
#include "DigetherDevRegistry.h"
#include "DigetherDevState.h"
#include "Dom/JsonObject.h"
#include "HttpPath.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	// 화면 원본은 DigetherDevWebPage.inl, 문자열 상수 길이 제한으로 조각 배열
#include "DigetherDevWebPage.inl"

	constexpr const TCHAR* DevHeader = TEXT("X-Digether-Dev");

	const FString* FindHeader(const FHttpServerRequest& Request, const TCHAR* Name)
	{
		for (const TPair<FString, TArray<FString>>& Header : Request.Headers)
		{
			if (Header.Key.Equals(Name, ESearchCase::IgnoreCase) && Header.Value.Num() > 0)
			{
				return &Header.Value[0];
			}
		}
		return nullptr;
	}

	TUniquePtr<FHttpServerResponse> MakeJson(const FString& Json)
	{
		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(Json, TEXT("application/json"));
		Response->Headers.Add(TEXT("Cache-Control"), { TEXT("no-store") });
		return Response;
	}

	FString ResultJson(const DigetherDev::FResult& Result)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetBoolField(TEXT("ok"), Result.bOk);
		Object->SetStringField(TEXT("message"), Result.Message);
		FString Output;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(Object, Writer);
		return Output;
	}

	// 엔진 HTTP 처리기는 코어 틱(게임 스레드)에서 호출, 아니면 게임 스레드로 넘김
	void RunOnGameThread(TFunction<void()> Work)
	{
		if (IsInGameThread())
		{
			Work();
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, MoveTemp(Work));
		}
	}
}

bool FDigetherDevWebServer::Start(const uint32 PreferredPort)
{
	if (IsRunning())
	{
		return true;
	}
	FHttpServerModule& HttpServer = FHttpServerModule::Get();
	// 리스너 허용을 먼저 켜야 GetHttpRouter가 실제 바인드를 시도하고 실패를 nullptr로 알려 줌
	HttpServer.StartAllListeners();
	for (uint32 Attempt = 0; Attempt < PortAttempts; ++Attempt)
	{
		const uint32 Candidate = PreferredPort + Attempt;
		Router = HttpServer.GetHttpRouter(Candidate, /*bFailOnBindFailure*/ true);
		if (Router.IsValid())
		{
			Port = Candidate;
			break;
		}
	}
	if (!Router.IsValid())
	{
		UE_LOG(LogDigetherDev, Warning, TEXT("개발 웹 서버를 열지 못했습니다(포트 %u~%u 사용 중)."), PreferredPort,
			PreferredPort + PortAttempts - 1);
		return false;
	}

	PreprocessorHandle = Router->RegisterRequestPreprocessor(FHttpRequestHandler::CreateRaw(this, &FDigetherDevWebServer::Preprocess));
	// 엔진은 루트 경로("/") 라우트 등록을 허용하지 않음(FHttpPath::IsValidPath), 첫 화면은 전처리기가 응답
	Routes.Add(Router->BindRoute(FHttpPath(TEXT("/index.html")), EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateRaw(this, &FDigetherDevWebServer::HandleIndex)));
	Routes.Add(Router->BindRoute(FHttpPath(TEXT("/api/commands")), EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateRaw(this, &FDigetherDevWebServer::HandleCommands)));
	// 읽기 전용 문서, 브라우저 링크로 열도록 /api/ 밖(전용 헤더 검사 제외)
	Routes.Add(Router->BindRoute(FHttpPath(TEXT("/commands.md")), EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateRaw(this, &FDigetherDevWebServer::HandleCommandsMarkdown)));
	Routes.Add(Router->BindRoute(FHttpPath(TEXT("/api/run")), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateRaw(this, &FDigetherDevWebServer::HandleRun)));
	Routes.Add(Router->BindRoute(FHttpPath(TEXT("/api/state")), EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateRaw(this, &FDigetherDevWebServer::HandleState)));

	UE_LOG(LogDigetherDev, Display, TEXT("개발 웹 화면: %s (이 PC의 브라우저에서 여세요)"), *GetUrl());
	return true;
}

void FDigetherDevWebServer::Stop()
{
	if (!Router.IsValid())
	{
		return;
	}
	for (const FHttpRouteHandle& Route : Routes)
	{
		Router->UnbindRoute(Route);
	}
	Routes.Reset();
	Router->UnregisterRequestPreprocessor(PreprocessorHandle);
	PreprocessorHandle.Reset();
	Router.Reset();
	UE_LOG(LogDigetherDev, Display, TEXT("개발 웹 화면 경로를 닫았습니다(포트 %u)."), Port);
	Port = 0;
}

FString FDigetherDevWebServer::GetUrl() const
{
	return IsRunning() ? FString::Printf(TEXT("http://localhost:%u"), Port) : FString();
}

bool FDigetherDevWebServer::Preprocess(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	const FString& Path = Request.RelativePath.GetPath();
	if (Path == TEXT("/") && Request.Verb == EHttpServerRequestVerbs::VERB_GET)
	{
		return HandleIndex(Request, OnComplete);
	}
	// 다른 웹사이트가 브라우저를 통해 몰래 명령을 보내는 것 차단: API는 전용 헤더 필수, 교차 출처 사전 요청 미허용
	if (!Path.StartsWith(TEXT("/api/")))
	{
		return false;
	}
	bool bReject = Request.Verb == EHttpServerRequestVerbs::VERB_OPTIONS || FindHeader(Request, DevHeader) == nullptr;
	if (const FString* Origin = FindHeader(Request, TEXT("Origin")))
	{
		const FString LocalHost = FString::Printf(TEXT("http://localhost:%u"), Port);
		const FString LoopBack = FString::Printf(TEXT("http://127.0.0.1:%u"), Port);
		bReject |= !Origin->Equals(LocalHost, ESearchCase::IgnoreCase) && !Origin->Equals(LoopBack, ESearchCase::IgnoreCase);
	}
	if (!bReject)
	{
		return false;
	}
	OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::Forbidden, TEXT("forbidden"),
		TEXT("이 PC의 개발 웹 화면에서만 요청할 수 있습니다.")));
	return true;
}

bool FDigetherDevWebServer::HandleIndex(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	const FString& Path = Request.RelativePath.GetPath();
	if (Path != TEXT("/") && Path != TEXT("/index.html"))
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::NotFound));
		return true;
	}
	TArray<uint8> Body;
	for (const char* Part : DigetherDevWebPageParts)
	{
		Body.Append(reinterpret_cast<const uint8*>(Part), FCStringAnsi::Strlen(Part));
	}
	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(MoveTemp(Body), TEXT("text/html;charset=utf-8"));
	Response->Headers.Add(TEXT("Cache-Control"), { TEXT("no-store") });
	OnComplete(MoveTemp(Response));
	return true;
}

bool FDigetherDevWebServer::HandleCommands(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	RunOnGameThread([OnComplete]()
	{
		OnComplete(MakeJson(DigetherDev::DescribeCommandsJson()));
	});
	return true;
}

bool FDigetherDevWebServer::HandleCommandsMarkdown(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	RunOnGameThread([OnComplete]()
	{
		OnComplete(FHttpServerResponse::Create(DigetherDev::DescribeCommandsMarkdown(), TEXT("text/markdown")));
	});
	return true;
}

bool FDigetherDevWebServer::HandleRun(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	constexpr int32 MaxBodyBytes = 4096;
	if (Request.Body.Num() == 0 || Request.Body.Num() > MaxBodyBytes)
	{
		OnComplete(MakeJson(ResultJson(DigetherDev::FResult::Fail(TEXT("요청 본문이 비었거나 너무 큽니다.")))));
		return true;
	}
	const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()), Request.Body.Num());
	const FString BodyText(Converted.Length(), Converted.Get());
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyText);
	FString Name;
	TArray<FString> Args;
	if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid() || !Object->TryGetStringField(TEXT("name"), Name))
	{
		OnComplete(MakeJson(ResultJson(DigetherDev::FResult::Fail(TEXT("요청 형식이 잘못됐습니다.")))));
		return true;
	}
	const TArray<TSharedPtr<FJsonValue>>* ArgValues = nullptr;
	if (Object->TryGetArrayField(TEXT("args"), ArgValues))
	{
		for (const TSharedPtr<FJsonValue>& Value : *ArgValues)
		{
			const FString Text = Value.IsValid() ? Value->AsString().TrimStartAndEnd() : FString();
			if (!Text.IsEmpty())
			{
				Args.Add(Text);
			}
		}
	}
	RunOnGameThread([OnComplete, Name, Args]()
	{
		const DigetherDev::FContext Context = DigetherDev::MakeContext(DigetherDev::FindPrimaryGameWorld(), DigetherDev::ESource::Web);
		OnComplete(MakeJson(ResultJson(DigetherDev::Execute(FName(*Name), Args, Context))));
	});
	return true;
}

bool FDigetherDevWebServer::HandleState(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	const FString Url = GetUrl();
	RunOnGameThread([OnComplete, Url]()
	{
		const DigetherDev::FStateSummary State = DigetherDev::CaptureState(DigetherDev::FindPrimaryGameWorld());
		OnComplete(MakeJson(DigetherDev::StateToJson(State, DigetherDev::WasCheatUsed(), Url,
			DigetherDev::Debug::GetPanelStates())));
	});
	return true;
}
