#include "DigetherDevRegistry.h"

#include "Dom/JsonObject.h"
#include "Engine/DebugCameraController.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Game/Framework/DigetherGameplayGameMode.h"
#include "Game/Framework/DigetherGameplayPlayerController.h"
#include "Game/Framework/DigetherGameplayPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/IConsoleManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY(LogDigetherDev);

namespace DigetherDev
{
	namespace
	{
		struct FEntry
		{
			FCommandDesc Desc;
			FHandler Handler;
			IConsoleObject* ConsoleObject = nullptr;
		};

		TMap<FName, FEntry>& Entries()
		{
			static TMap<FName, FEntry> Map;
			return Map;
		}

		// 웹 화면 탭 순서, 목록에 없는 분류는 뒤에 이름 순
		const TCHAR* CategoryOrder[] = {
			TEXT("진행"), TEXT("탐사"), TEXT("캐릭터"), TEXT("돈·아이템"), TEXT("엘리베이터"),
			TEXT("몬스터"), TEXT("멀티"), TEXT("표시"), TEXT("기존 시험"), TEXT("도구")
		};

		int32 CategoryRank(const FString& Category)
		{
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(CategoryOrder); ++Index)
			{
				if (Category == CategoryOrder[Index])
				{
					return Index;
				}
			}
			return UE_ARRAY_COUNT(CategoryOrder);
		}

		bool bCheatUsed = false;

		const TCHAR* TypeName(const EParamType Type)
		{
			switch (Type)
			{
			case EParamType::Int: return TEXT("int");
			case EParamType::Float: return TEXT("float");
			case EParamType::Bool: return TEXT("bool");
			case EParamType::Enum: return TEXT("enum");
			case EParamType::Player: return TEXT("player");
			default: return TEXT("text");
			}
		}

		bool ParseBool(const FString& Value, bool& bOut)
		{
			if (Value == TEXT("1") || Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("on"), ESearchCase::IgnoreCase))
			{
				bOut = true;
				return true;
			}
			if (Value == TEXT("0") || Value.Equals(TEXT("false"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("off"), ESearchCase::IgnoreCase))
			{
				bOut = false;
				return true;
			}
			return false;
		}

		void ShowOnScreen(const FResult& Result)
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 6.f, Result.bOk ? FColor(120, 230, 140) : FColor(255, 140, 110),
					FString::Printf(TEXT("[개발] %s"), *Result.Message));
			}
		}

		void RunFromConsole(const FName Name, const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
		{
			const FResult Result = Execute(Name, Args, MakeContext(World, ESource::Console));
			Ar.Log(Result.Message);
		}
	}

	FString FCommandDesc::MakeUsage() const
	{
		FString Usage = Name.ToString();
		for (const FParam& Param : Params)
		{
			FString Shape = Param.Type == EParamType::Enum ? FString::Join(Param.Options, TEXT("|")) : Param.Name;
			Usage += Param.bOptional ? FString::Printf(TEXT(" [%s]"), *Shape) : FString::Printf(TEXT(" <%s>"), *Shape);
		}
		return Usage;
	}

	bool FContext::IsHost() const
	{
		return World != nullptr && World->GetNetMode() != NM_Client;
	}

	ADigetherGameplayGameMode* FContext::GetGameMode() const
	{
		return World != nullptr ? World->GetAuthGameMode<ADigetherGameplayGameMode>() : nullptr;
	}

	bool Register(FCommandDesc Desc, FHandler Handler)
	{
		if (Desc.Name.IsNone() || !Desc.Name.ToString().StartsWith(TEXT("Digether")) || !Handler
			|| Desc.Category.IsEmpty() || Desc.Summary.IsEmpty())
		{
			UE_LOG(LogDigetherDev, Warning, TEXT("개발 명령 등록 거절: %s"), *Desc.Name.ToString());
			return false;
		}
		Unregister(Desc.Name);

		FEntry Entry;
		Entry.Handler = MoveTemp(Handler);
		const FName Name = Desc.Name;
		const FString Help = FString::Printf(TEXT("[%s] %s  사용법: %s"), *Desc.Category, *Desc.Summary, *Desc.MakeUsage());
		Entry.ConsoleObject = IConsoleManager::Get().RegisterConsoleCommand(*Name.ToString(), *Help,
			FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
				[Name](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
				{
					RunFromConsole(Name, Args, World, Ar);
				}),
			ECVF_Default);
		Entry.Desc = MoveTemp(Desc);
		Entries().Add(Name, MoveTemp(Entry));
		return true;
	}

	void Unregister(const FName Name)
	{
		if (FEntry* Entry = Entries().Find(Name))
		{
			if (Entry->ConsoleObject)
			{
				IConsoleManager::Get().UnregisterConsoleObject(Entry->ConsoleObject, false);
			}
			Entries().Remove(Name);
		}
	}

	void UnregisterAll()
	{
		TArray<FName> Names;
		Entries().GetKeys(Names);
		for (const FName Name : Names)
		{
			Unregister(Name);
		}
	}

	const FCommandDesc* Find(const FName Name)
	{
		const FEntry* Entry = Entries().Find(Name);
		return Entry ? &Entry->Desc : nullptr;
	}

	TArray<const FCommandDesc*> GetCommands()
	{
		TArray<const FCommandDesc*> Result;
		for (const TPair<FName, FEntry>& Pair : Entries())
		{
			Result.Add(&Pair.Value.Desc);
		}
		Result.Sort([](const FCommandDesc& A, const FCommandDesc& B)
		{
			const int32 RankA = CategoryRank(A.Category);
			const int32 RankB = CategoryRank(B.Category);
			if (RankA != RankB)
			{
				return RankA < RankB;
			}
			if (A.Category != B.Category)
			{
				return A.Category < B.Category;
			}
			return A.Name.LexicalLess(B.Name);
		});
		return Result;
	}

	FContext MakeContext(UWorld* World, const ESource Source)
	{
		FContext Context;
		Context.World = World;
		Context.Source = Source;
		if (World == nullptr)
		{
			return Context;
		}
		// 로컬 플레이어의 현재 컨트롤러가 화면 컨트롤러, 자유 카메라면 원래 컨트롤러를 따로 찾음
		const UGameInstance* GameInstance = World->GetGameInstance();
		const ULocalPlayer* LocalPlayer = GameInstance ? GameInstance->GetFirstGamePlayer() : nullptr;
		APlayerController* View = LocalPlayer ? LocalPlayer->PlayerController.Get() : nullptr;
		if (View == nullptr || View->GetWorld() != World)
		{
			View = nullptr;
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (It->Get() && It->Get()->IsLocalController())
				{
					View = It->Get();
					break;
				}
			}
		}
		Context.ViewController = View;
		APlayerController* Game = View;
		if (const ADebugCameraController* DebugCamera = Cast<ADebugCameraController>(View))
		{
			Game = DebugCamera->OriginalControllerRef.Get();
		}
		Context.GameController = Cast<ADigetherGameplayPlayerController>(Game);
		return Context;
	}

	UWorld* FindPrimaryGameWorld()
	{
		if (GEngine == nullptr)
		{
			return nullptr;
		}
		UWorld* Best = nullptr;
		int32 BestScore = -1;
		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			UWorld* World = WorldContext.World();
			if (World == nullptr || (WorldContext.WorldType != EWorldType::Game && WorldContext.WorldType != EWorldType::PIE))
			{
				continue;
			}
			const int32 Score = World->GetNetMode() == NM_Client ? 1 : 2;
			if (Score > BestScore)
			{
				Best = World;
				BestScore = Score;
			}
		}
		return Best;
	}

	FString ValidateArgs(const FCommandDesc& Desc, const TArray<FString>& Args)
	{
		int32 Required = 0;
		for (const FParam& Param : Desc.Params)
		{
			Required += Param.bOptional ? 0 : 1;
		}
		const bool bLastIsText = Desc.Params.Num() > 0 && Desc.Params.Last().Type == EParamType::Text;
		if (Args.Num() < Required || (!bLastIsText && Args.Num() > Desc.Params.Num()))
		{
			return FString::Printf(TEXT("인자 개수가 맞지 않습니다. 사용법: %s"), *Desc.MakeUsage());
		}
		for (int32 Index = 0; Index < Args.Num() && Index < Desc.Params.Num(); ++Index)
		{
			const FParam& Param = Desc.Params[Index];
			const FString& Value = Args[Index];
			switch (Param.Type)
			{
			case EParamType::Int:
			case EParamType::Float:
			{
				const bool bNumeric = Param.Type == EParamType::Int
					? (Value.IsNumeric() && !Value.Contains(TEXT(".")))
					: Value.IsNumeric();
				if (!bNumeric)
				{
					return FString::Printf(TEXT("'%s'에는 %s를 넣어야 합니다(입력: %s)."), *Param.Name,
						Param.Type == EParamType::Int ? TEXT("정수") : TEXT("숫자"), *Value);
				}
				const double Number = FCString::Atod(*Value);
				if (!FMath::IsFinite(Number) || (Param.Min.IsSet() && Number < Param.Min.GetValue())
					|| (Param.Max.IsSet() && Number > Param.Max.GetValue()))
				{
					return FString::Printf(TEXT("'%s' 범위를 벗어났습니다(입력: %s, 범위: %s~%s)."), *Param.Name, *Value,
						Param.Min.IsSet() ? *FString::SanitizeFloat(Param.Min.GetValue()) : TEXT(""),
						Param.Max.IsSet() ? *FString::SanitizeFloat(Param.Max.GetValue()) : TEXT(""));
				}
				break;
			}
			case EParamType::Bool:
			{
				bool bIgnored = false;
				if (!ParseBool(Value, bIgnored))
				{
					return FString::Printf(TEXT("'%s'에는 1 또는 0을 넣어야 합니다(입력: %s)."), *Param.Name, *Value);
				}
				break;
			}
			case EParamType::Enum:
			{
				const bool bKnown = Param.Options.ContainsByPredicate([&Value](const FString& Option)
				{
					return Option.Equals(Value, ESearchCase::IgnoreCase);
				});
				if (!bKnown)
				{
					return FString::Printf(TEXT("'%s'는 %s 중 하나여야 합니다(입력: %s)."), *Param.Name,
						*FString::Join(Param.Options, TEXT(", ")), *Value);
				}
				break;
			}
			case EParamType::Player:
				if (!Value.Equals(TEXT("self"), ESearchCase::IgnoreCase) && !Value.Equals(TEXT("all"), ESearchCase::IgnoreCase)
					&& !(Value.IsNumeric() && !Value.Contains(TEXT("."))))
				{
					return FString::Printf(TEXT("'%s'에는 self, all 또는 플레이어 번호를 넣어야 합니다(입력: %s)."), *Param.Name, *Value);
				}
				break;
			default:
				break;
			}
		}
		return FString();
	}

	FResult Execute(const FName Name, const TArray<FString>& Args, const FContext& Context)
	{
		FResult Result;
		const FEntry* Entry = Entries().Find(Name);
		if (Entry == nullptr)
		{
			Result = FResult::Fail(FString::Printf(TEXT("%s: 알 수 없는 명령입니다. DigetherHelp로 목록을 보세요."), *Name.ToString()));
		}
		else if (!Entry->Desc.UnavailableReason.IsEmpty())
		{
			Result = FResult::Fail(FString::Printf(TEXT("%s: 아직 쓸 수 없습니다. %s"), *Name.ToString(), *Entry->Desc.UnavailableReason));
		}
		else if (const FString Problem = ValidateArgs(Entry->Desc, Args); !Problem.IsEmpty())
		{
			Result = FResult::Fail(FString::Printf(TEXT("%s: %s"), *Name.ToString(), *Problem));
		}
		else if (Entry->Desc.bRequiresGameplayWorld && Context.World == nullptr)
		{
			Result = FResult::Fail(FString::Printf(TEXT("%s: 게임 중에만 쓸 수 있습니다."), *Name.ToString()));
		}
		else if (Entry->Desc.bRequiresGameplayWorld && Entry->Desc.Scope == EScope::Host && !Context.IsHost())
		{
			Result = FResult::Fail(FString::Printf(TEXT("%s: 호스트 게임에서 실행하세요(이 프로세스는 참가자입니다)."), *Name.ToString()));
		}
		else if (Entry->Desc.bRequiresGameplayWorld && Entry->Desc.Scope == EScope::Host && Context.GetGameMode() == nullptr)
		{
			Result = FResult::Fail(FString::Printf(TEXT("%s: 게임 맵(거점·갱도)에서만 쓸 수 있습니다."), *Name.ToString()));
		}
		else if (Entry->Desc.bRequiresGameplayWorld && Entry->Desc.Scope == EScope::Local && Context.GameController == nullptr)
		{
			Result = FResult::Fail(FString::Printf(TEXT("%s: 게임 맵의 로컬 플레이어가 없습니다."), *Name.ToString()));
		}
		else if (Entry->Desc.Scope == EScope::Host && !Entry->Desc.bAllowDuringGameOver
			&& Context.GetGameMode() != nullptr && Context.GetGameMode()->IsGameOverActive())
		{
			Result = FResult::Fail(FString::Printf(TEXT("%s: 게임 오버(실패 사건·크레딧) 중에는 쓸 수 없습니다."), *Name.ToString()));
		}
		else
		{
			Result = Entry->Handler(Context, Args);
			// 호스트 프로세스에서 판정을 바꾼 명령이면 범위와 무관하게 표시(각자 명령도 호스트에서는 서버 판정 변경)
			if (Result.bOk && Entry->Desc.bChangesState && Context.IsHost())
			{
				bCheatUsed = true;
			}
		}
		UE_LOG(LogDigetherDev, Display, TEXT("%s %s -> %s: %s"), *Name.ToString(), *FString::Join(Args, TEXT(" ")),
			Result.bOk ? TEXT("성공") : TEXT("거절"), *Result.Message);
		if (Context.Source != ESource::Test)
		{
			ShowOnScreen(Result);
		}
		return Result;
	}

	bool ResolvePlayers(const FContext& Context, const FString& Token, TArray<ADigetherGameplayPlayerState*>& OutPlayers,
		FString& OutError)
	{
		OutPlayers.Reset();
		const AGameStateBase* GameState = Context.World ? Context.World->GetGameState() : nullptr;
		if (!Context.IsHost() || GameState == nullptr)
		{
			OutError = TEXT("플레이어 지정은 호스트에서만 할 수 있습니다.");
			return false;
		}
		const FString Trimmed = Token.TrimStartAndEnd();
		if (Trimmed.IsEmpty() || Trimmed.Equals(TEXT("self"), ESearchCase::IgnoreCase))
		{
			ADigetherGameplayPlayerState* Self = Context.GameController
				? Context.GameController->GetPlayerState<ADigetherGameplayPlayerState>() : nullptr;
			if (Self == nullptr)
			{
				OutError = TEXT("호스트 플레이어 상태가 없습니다.");
				return false;
			}
			OutPlayers.Add(Self);
			return true;
		}
		const bool bAll = Trimmed.Equals(TEXT("all"), ESearchCase::IgnoreCase);
		const int32 WantedId = bAll ? INDEX_NONE : FCString::Atoi(*Trimmed);
		for (APlayerState* Listed : GameState->PlayerArray)
		{
			ADigetherGameplayPlayerState* Player = Cast<ADigetherGameplayPlayerState>(Listed);
			if (Player && (bAll || Player->GetPlayerId() == WantedId))
			{
				OutPlayers.Add(Player);
			}
		}
		if (OutPlayers.IsEmpty())
		{
			OutError = FString::Printf(TEXT("플레이어 %s를 찾지 못했습니다(상태 패널의 번호 사용)."), *Trimmed);
			return false;
		}
		return true;
	}

	FString DescribeCommandsJson()
	{
		TArray<TSharedPtr<FJsonValue>> Commands;
		for (const FCommandDesc* Desc : GetCommands())
		{
			TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetStringField(TEXT("name"), Desc->Name.ToString());
			Object->SetStringField(TEXT("category"), Desc->Category);
			Object->SetStringField(TEXT("requestIds"), Desc->RequestIds);
			Object->SetStringField(TEXT("summary"), Desc->Summary);
			Object->SetStringField(TEXT("usage"), Desc->MakeUsage());
			Object->SetStringField(TEXT("scope"), Desc->Scope == EScope::Host ? TEXT("host") : TEXT("local"));
			Object->SetBoolField(TEXT("destructive"), Desc->bDestructive);
			Object->SetStringField(TEXT("unavailable"), Desc->UnavailableReason);
			TArray<TSharedPtr<FJsonValue>> Params;
			for (const FParam& Param : Desc->Params)
			{
				TSharedRef<FJsonObject> ParamObject = MakeShared<FJsonObject>();
				ParamObject->SetStringField(TEXT("name"), Param.Name);
				ParamObject->SetStringField(TEXT("type"), TypeName(Param.Type));
				ParamObject->SetBoolField(TEXT("optional"), Param.bOptional);
				ParamObject->SetStringField(TEXT("default"), Param.Default);
				ParamObject->SetStringField(TEXT("hint"), Param.Hint);
				TArray<TSharedPtr<FJsonValue>> Options;
				for (const FString& Option : Param.Options)
				{
					Options.Add(MakeShared<FJsonValueString>(Option));
				}
				ParamObject->SetArrayField(TEXT("options"), Options);
				if (Param.Min.IsSet())
				{
					ParamObject->SetNumberField(TEXT("min"), Param.Min.GetValue());
				}
				if (Param.Max.IsSet())
				{
					ParamObject->SetNumberField(TEXT("max"), Param.Max.GetValue());
				}
				Params.Add(MakeShared<FJsonValueObject>(ParamObject));
			}
			Object->SetArrayField(TEXT("params"), Params);
			Commands.Add(MakeShared<FJsonValueObject>(Object));
		}
		FString Output;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(Commands, Writer);
		return Output;
	}

	FString DescribeCommandsMarkdown()
	{
		FString Text = TEXT("# DIGETHER 개발 명령\n\n");
		Text += TEXT("- 개발 빌드 전용이며 출시(Shipping) 빌드에는 없습니다.\n");
		Text += TEXT("- 웹 화면: 게임을 띄운 PC의 브라우저에서 http://localhost:30080 (에디터는 자동 실행, 패키지 빌드는 실행 인자 -DigetherDevWeb)\n");
		Text += TEXT("- 콘솔: 게임 중 ~ 키를 누르고 명령을 입력합니다. 예: `DigetherHelp`, `DigetherGrantCredits 500`\n");
		Text += TEXT("- '호스트' 명령은 호스트 게임에서 실행하면 결과가 모든 참가자에게 반영됩니다.\n\n");
		FString Category;
		for (const FCommandDesc* Desc : GetCommands())
		{
			if (Desc->Category != Category)
			{
				Category = Desc->Category;
				Text += FString::Printf(TEXT("\n## %s\n\n| 명령 | 요청 ID | 설명 | 실행 위치 |\n|---|---|---|---|\n"), *Category);
			}
			FString Summary = Desc->Summary;
			if (!Desc->UnavailableReason.IsEmpty())
			{
				Summary += FString::Printf(TEXT(" (아직 사용 불가: %s)"), *Desc->UnavailableReason);
			}
			Text += FString::Printf(TEXT("| `%s` | %s | %s | %s |\n"), *Desc->MakeUsage(), *Desc->RequestIds, *Summary,
				Desc->Scope == EScope::Host ? TEXT("호스트") : TEXT("각자"));
		}
		return Text;
	}

	bool WasCheatUsed()
	{
		return bCheatUsed;
	}
}
