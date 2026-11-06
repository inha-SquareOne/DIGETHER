#include "Game/Dev/DigetherDevCommands.h"

#include "Game/Dev/DigetherDevLog.h"

namespace DigetherDev
{
	namespace
	{
		const FCommandInfo PlannedCommands[] = {
			{ FName(TEXT("DigetherAdvanceTime")), TEXT("P2 탐사 시간"), TEXT("남은 탐사 시간을 앞으로 진행한다") },
			{ FName(TEXT("DigetherSpawnItem")), TEXT("P1 경제"), TEXT("정의 ID로 물품을 생성한다") },
			{ FName(TEXT("DigetherAftershock")), TEXT("P2 탐사 시간"), TEXT("여진을 즉시 일으킨다") },
			{ FName(TEXT("DigetherRerunSettlement")), TEXT("P1 경제"), TEXT("직전 정산을 같은 스냅샷으로 다시 실행한다") },
			{ FName(TEXT("DigetherSave")), TEXT("P6 저장·재접속"), TEXT("현재 진행을 수동 저장한다") },
			{ FName(TEXT("DigetherLoad")), TEXT("P6 저장·재접속"), TEXT("저장한 진행을 불러온다") },
			{ FName(TEXT("DigetherNewGame")), TEXT("P6 저장·재접속"), TEXT("새 게임 진행을 시작한다") },
		};

		TMap<FName, FCommandHandler>& Handlers()
		{
			static TMap<FName, FCommandHandler> Map;
			return Map;
		}

		bool IsPlanned(FName Command)
		{
			for (const FCommandInfo& Info : PlannedCommands)
			{
				if (Info.Name == Command)
				{
					return true;
				}
			}
			return false;
		}

		FString MakeUnavailableMessage()
		{
			return TEXT("개발 명령은 Shipping 빌드에서 쓸 수 없다");
		}
	}

	TConstArrayView<FCommandInfo> GetPlannedCommands()
	{
		return MakeArrayView(PlannedCommands);
	}

	bool IsAvailable()
	{
#if UE_BUILD_SHIPPING
		return false;
#else
		return true;
#endif
	}

	bool Register(FName Command, FCommandHandler Handler)
	{
		if (!IsAvailable() || !IsPlanned(Command) || !Handler)
		{
			return false;
		}
		Handlers().Add(Command, MoveTemp(Handler));
		return true;
	}

	void Unregister(FName Command)
	{
		Handlers().Remove(Command);
	}

	bool IsConnected(FName Command)
	{
		return Handlers().Contains(Command);
	}

	FString MakeNotConnectedMessage(FName Command)
	{
		return FString::Printf(TEXT("%s: 아직 연결 안 됨"), *Command.ToString());
	}

	FString Execute(FName Command, const TArray<FString>& Args)
	{
		FString Response;
		if (!IsAvailable())
		{
			Response = MakeUnavailableMessage();
		}
		else if (!IsPlanned(Command))
		{
			Response = FString::Printf(TEXT("%s: 알 수 없는 명령"), *Command.ToString());
		}
		else if (const FCommandHandler* Handler = Handlers().Find(Command))
		{
			Response = (*Handler)(Args);
		}
		else
		{
			Response = MakeNotConnectedMessage(Command);
		}
		UE_LOG(LogDigetherDev, Display, TEXT("%s"), *Response);
		return Response;
	}
}
