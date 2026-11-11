#pragma once

#include "CoreMinimal.h"
#include "DigetherDevRegistry.h"

class FDigetherDevWebServer;

// 도메인별 명령 등록 함수, 모듈 시작 때 한 번 호출
namespace DigetherDev
{
	FDigetherDevWebServer& GetWebServer();
	bool StartWebServer();

	void RegisterCoreCommands();
	void RegisterLegacyCommands();
	void RegisterMonsterCommands();
	void RegisterReservedCommands();
	// 진행·탐사·돈·엘리베이터(DigetherDevCommandsRun.cpp)
	void RegisterRunCommands();
	// 캐릭터·멀티(DigetherDevCommandsPlayer.cpp)
	void RegisterPlayerCommands();
	// 화면 디버그 표시 패널(DigetherDevCommandsDisplay.cpp)
	void RegisterDisplayPanels();

	inline FString ArgOr(const TArray<FString>& Args, const int32 Index, const FString& Default = FString())
	{
		return Args.IsValidIndex(Index) ? Args[Index] : Default;
	}

	inline FString JoinFrom(const TArray<FString>& Args, const int32 Start)
	{
		FString Joined;
		for (int32 Index = Start; Index < Args.Num(); ++Index)
		{
			Joined += (Joined.IsEmpty() ? TEXT("") : TEXT(" ")) + Args[Index];
		}
		return Joined;
	}

	inline FParam MakeParam(const TCHAR* Name, const EParamType Type, const bool bOptional = false, const TCHAR* Hint = TEXT(""))
	{
		FParam Param;
		Param.Name = Name;
		Param.Type = Type;
		Param.bOptional = bOptional;
		Param.Hint = Hint;
		return Param;
	}

	inline FParam MakeEnumParam(const TCHAR* Name, TArray<FString> Options, const bool bOptional = false)
	{
		FParam Param = MakeParam(Name, EParamType::Enum, bOptional);
		Param.Options = MoveTemp(Options);
		if (!bOptional && Param.Options.Num() > 0)
		{
			Param.Default = Param.Options[0];
		}
		return Param;
	}

	inline FParam MakeRangeParam(const TCHAR* Name, const EParamType Type, const double Min, const double Max,
		const bool bOptional = false, const TCHAR* Hint = TEXT(""))
	{
		FParam Param = MakeParam(Name, Type, bOptional, Hint);
		Param.Min = Min;
		Param.Max = Max;
		return Param;
	}
}
