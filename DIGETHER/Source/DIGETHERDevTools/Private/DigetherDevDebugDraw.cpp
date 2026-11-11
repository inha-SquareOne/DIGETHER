#include "DigetherDevDebugDraw.h"

#include "CanvasItem.h"
#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace DigetherDev::Debug
{
	namespace
	{
		struct FPanel
		{
			FPanelDesc Desc;
			IConsoleVariable* Variable = nullptr;
		};

		TArray<FPanel>& Panels()
		{
			static TArray<FPanel> List;
			return List;
		}

		FDelegateHandle CanvasHandle;
		FDelegateHandle TickHandle;

		bool IsOn(const FPanel& Panel)
		{
			return Panel.Variable != nullptr && Panel.Variable->GetInt() != 0;
		}

		void DrawCanvas(UCanvas* Canvas, APlayerController* Controller)
		{
			UWorld* World = Controller ? Controller->GetWorld() : nullptr;
			if (Canvas == nullptr || World == nullptr || !World->IsGameWorld())
			{
				return;
			}
			const bool bHost = World->GetNetMode() != NM_Client;
			UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
			float Y = 80.f;
			const float X = 24.f;
			for (const FPanel& Panel : Panels())
			{
				if (!IsOn(Panel) || !Panel.Desc.Text)
				{
					continue;
				}
				TArray<FString> Lines;
				if (Panel.Desc.bHostOnly && !bHost)
				{
					Lines.Add(TEXT("호스트 화면에서만 표시합니다."));
				}
				else
				{
					Panel.Desc.Text(World, Lines);
				}
				FCanvasTextItem Title(FVector2D(X, Y), FText::FromString(FString::Printf(TEXT("[%s] %s"),
					*Panel.Desc.RequestIds, *Panel.Desc.Title)), Font, FLinearColor(1.f, 0.85f, 0.3f));
				Title.EnableShadow(FLinearColor::Black);
				Canvas->DrawItem(Title);
				Y += 16.f;
				for (const FString& Line : Lines)
				{
					FCanvasTextItem Item(FVector2D(X + 8.f, Y), FText::FromString(Line), Font, FLinearColor::White);
					Item.EnableShadow(FLinearColor::Black);
					Canvas->DrawItem(Item);
					Y += 14.f;
				}
				Y += 8.f;
			}
		}

		void DrawWorld(UWorld* World, ELevelTick, float)
		{
			if (World == nullptr || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer)
			{
				return;
			}
			const bool bHost = World->GetNetMode() != NM_Client;
			for (const FPanel& Panel : Panels())
			{
				if (IsOn(Panel) && Panel.Desc.Draw3D && (bHost || !Panel.Desc.bHostOnly))
				{
					Panel.Desc.Draw3D(World);
				}
			}
		}

		FPanel* FindPanel(const FName Id)
		{
			return Panels().FindByPredicate([Id](const FPanel& Panel) { return Panel.Desc.Id == Id; });
		}
	}

	void RegisterPanel(FPanelDesc Desc)
	{
		if (FPanel* Existing = FindPanel(Desc.Id))
		{
			Existing->Desc = MoveTemp(Desc);
			return;
		}
		FPanel Panel;
		const FString VariableName = FString::Printf(TEXT("digether.Debug.%s"), *Desc.Id.ToString());
		Panel.Variable = IConsoleManager::Get().FindConsoleVariable(*VariableName);
		if (Panel.Variable == nullptr)
		{
			Panel.Variable = IConsoleManager::Get().RegisterConsoleVariable(*VariableName, 0,
				*FString::Printf(TEXT("[%s] %s 화면 표시 (0/1)"), *Desc.RequestIds, *Desc.Title), ECVF_Cheat);
		}
		Panel.Desc = MoveTemp(Desc);
		Panels().Add(MoveTemp(Panel));
	}

	void Startup()
	{
		if (!CanvasHandle.IsValid())
		{
			CanvasHandle = UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateStatic(&DrawCanvas));
		}
		if (!TickHandle.IsValid())
		{
			TickHandle = FWorldDelegates::OnWorldPostActorTick.AddStatic(&DrawWorld);
		}
	}

	void Shutdown()
	{
		if (CanvasHandle.IsValid())
		{
			UDebugDrawService::Unregister(CanvasHandle);
			CanvasHandle.Reset();
		}
		if (TickHandle.IsValid())
		{
			FWorldDelegates::OnWorldPostActorTick.Remove(TickHandle);
			TickHandle.Reset();
		}
		for (const FPanel& Panel : Panels())
		{
			if (Panel.Variable)
			{
				IConsoleManager::Get().UnregisterConsoleObject(Panel.Variable, false);
			}
		}
		Panels().Reset();
	}

	bool SetEnabled(const FName Id, const bool bEnabled)
	{
		FPanel* Panel = FindPanel(Id);
		if (Panel == nullptr || Panel->Variable == nullptr)
		{
			return false;
		}
		Panel->Variable->Set(bEnabled ? 1 : 0, ECVF_SetByConsole);
		return true;
	}

	bool IsEnabled(const FName Id)
	{
		const FPanel* Panel = FindPanel(Id);
		return Panel != nullptr && IsOn(*Panel);
	}

	TArray<TPair<FString, bool>> GetPanelStates()
	{
		TArray<TPair<FString, bool>> States;
		for (const FPanel& Panel : Panels())
		{
			States.Emplace(Panel.Desc.Id.ToString(), IsOn(Panel));
		}
		return States;
	}

	TArray<FString> GetPanelIds()
	{
		TArray<FString> Ids;
		for (const FPanel& Panel : Panels())
		{
			Ids.Add(Panel.Desc.Id.ToString());
		}
		return Ids;
	}
}
