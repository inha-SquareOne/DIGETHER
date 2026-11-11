// 도구 명령: 명령 목록, 화면 표시 켜고 끄기, 웹 화면 주소·시작, 상태 표시 패널
#include "Commands/DigetherDevCommands.h"

#include "DigetherDevDebugDraw.h"
#include "DigetherDevState.h"
#include "DigetherDevWebServer.h"

namespace DigetherDev
{
	void RegisterCoreCommands()
	{
		{
			FCommandDesc Desc;
			Desc.Name = TEXT("DigetherHelp");
			Desc.Category = TEXT("도구");
			Desc.Summary = TEXT("개발 명령 목록을 로그와 화면에 보여 준다");
			Desc.Params = { MakeParam(TEXT("분류"), EParamType::Text, true, TEXT("예: 진행")) };
			Desc.Scope = EScope::Local;
			Desc.bRequiresGameplayWorld = false;
			Desc.bChangesState = false;
			Desc.bAllowDuringGameOver = true;
			const FHandler Handler = [](const FContext&, const TArray<FString>& Args)
			{
				const FString Filter = JoinFrom(Args, 0);
				int32 Count = 0;
				for (const FCommandDesc* Command : GetCommands())
				{
					if (!Filter.IsEmpty() && !Command->Category.Contains(Filter))
					{
						continue;
					}
					++Count;
					UE_LOG(LogDigetherDev, Display, TEXT("[%s] %s  %s%s"), *Command->Category, *Command->MakeUsage(), *Command->Summary,
						Command->UnavailableReason.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (사용 불가: %s)"), *Command->UnavailableReason));
				}
				const FString Url = GetWebServer().GetUrl();
				return FResult::Ok(FString::Printf(TEXT("명령 %d개를 로그(~ 콘솔·Output Log)에 출력했습니다. 웹 화면: %s"), Count,
					Url.IsEmpty() ? TEXT("꺼짐(DigetherDevWeb Start)") : *Url));
			};
			Register(Desc, Handler);
			// 기존 이름 호환
			Desc.Name = TEXT("DigetherCheatList");
			Desc.Summary = TEXT("DigetherHelp와 같다(기존 이름)");
			Register(MoveTemp(Desc), Handler);
		}
		{
			FCommandDesc Desc;
			Desc.Name = TEXT("DigetherShow");
			Desc.Category = TEXT("표시");
			Desc.Summary = TEXT("화면 디버그 표시 항목을 켜거나 끈다");
			// 항목은 이후 단계에서도 추가되므로 실행 때 확인(웹 화면은 상태의 패널 목록으로 스위치 표시)
			Desc.Params = { MakeParam(TEXT("항목"), EParamType::Text, false, TEXT("예: Status")), MakeParam(TEXT("켜기"), EParamType::Bool) };
			Desc.Params[1].Default = TEXT("1");
			Desc.Scope = EScope::Local;
			Desc.bRequiresGameplayWorld = false;
			Desc.bChangesState = false;
			Desc.bAllowDuringGameOver = true;
			Register(MoveTemp(Desc), [](const FContext&, const TArray<FString>& Args)
			{
				const bool bOn = Args[1] == TEXT("1") || Args[1].Equals(TEXT("true"), ESearchCase::IgnoreCase)
					|| Args[1].Equals(TEXT("on"), ESearchCase::IgnoreCase);
				for (const FString& Id : Debug::GetPanelIds())
				{
					if (Id.Equals(Args[0], ESearchCase::IgnoreCase))
					{
						Debug::SetEnabled(FName(*Id), bOn);
						return FResult::Ok(FString::Printf(TEXT("화면 표시 %s: %s"), *Id, bOn ? TEXT("켬") : TEXT("끔")));
					}
				}
				return FResult::Fail(FString::Printf(TEXT("표시 항목 %s가 없습니다. 있는 항목: %s"), *Args[0],
					*FString::Join(Debug::GetPanelIds(), TEXT(", "))));
			});
		}
		{
			FCommandDesc Desc;
			Desc.Name = TEXT("DigetherDevWeb");
			Desc.Category = TEXT("도구");
			Desc.Summary = TEXT("개발 웹 화면 주소를 보여 주거나 서버를 켜고 끈다");
			Desc.Params = { MakeEnumParam(TEXT("동작"), { TEXT("Url"), TEXT("Start"), TEXT("Stop") }, true) };
			Desc.Scope = EScope::Local;
			Desc.bRequiresGameplayWorld = false;
			Desc.bChangesState = false;
			Desc.bAllowDuringGameOver = true;
			Register(MoveTemp(Desc), [](const FContext& Context, const TArray<FString>& Args)
			{
				const FString Action = ArgOr(Args, 0, TEXT("Url"));
				if (Action.Equals(TEXT("Stop"), ESearchCase::IgnoreCase))
				{
					if (Context.Source == ESource::Web)
					{
						return FResult::Fail(TEXT("웹 화면에서는 웹 서버를 끌 수 없습니다. 콘솔에서 실행하세요."));
					}
					GetWebServer().Stop();
					return FResult::Ok(TEXT("개발 웹 화면을 껐습니다."));
				}
				if (Action.Equals(TEXT("Start"), ESearchCase::IgnoreCase) && !StartWebServer())
				{
					return FResult::Fail(TEXT("개발 웹 서버를 열지 못했습니다(로그 확인)."));
				}
				const FString Url = GetWebServer().GetUrl();
				return Url.IsEmpty() ? FResult::Fail(TEXT("개발 웹 화면이 꺼져 있습니다. DigetherDevWeb Start"))
					: FResult::Ok(FString::Printf(TEXT("개발 웹 화면: %s (이 PC의 브라우저)"), *Url));
			});
		}

		Debug::FPanelDesc Status;
		Status.Id = TEXT("Status");
		Status.Title = TEXT("진행 상태");
		Status.RequestIds = TEXT("공통");
		Status.Text = [](UWorld* World, TArray<FString>& OutLines)
		{
			StateToLines(CaptureState(World), OutLines);
			if (WasCheatUsed())
			{
				OutLines.Add(TEXT("치트 사용됨: 바꾼 값이 자동 저장에 남습니다"));
			}
		};
		Debug::RegisterPanel(MoveTemp(Status));
	}
}
