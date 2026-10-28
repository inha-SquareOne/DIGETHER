// 'DIGETHER 개발 도구' 허브가 실제 엔진 상태를 읽고 엔진 UI를 여는 구현. 읽기 전용이며 설정·패키지를 바꾸지 않는다.
#include "DigetherDeveloperToolsModel.h"
#include "Engine/AssetManagerSettings.h"
#include "Framework/Docking/TabManager.h"
#include "ISettingsCategory.h"
#include "ISettingsContainer.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "Interfaces/IPluginManager.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "Settings/EditorLoadingSavingSettings.h"

#define LOCTEXT_NAMESPACE "DigetherDeveloperToolsLive"

namespace DigetherDevToolsLive
{
	const TCHAR* const SettingsModuleName = TEXT("Settings");
	const TCHAR* const LevelEditorModuleName = TEXT("LevelEditor");

	EDigetherPluginState ReadPluginState(FName PluginName)
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName.ToString());
		if (!Plugin.IsValid())
		{
			return EDigetherPluginState::NotFound;
		}
		return Plugin->IsEnabled() ? EDigetherPluginState::Enabled : EDigetherPluginState::Disabled;
	}

	TSharedPtr<FTabManager> GetLevelEditorTabManager()
	{
		const FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(LevelEditorModuleName);
		return LevelEditorModule ? LevelEditorModule->GetLevelEditorTabManager() : nullptr;
	}

	bool HasSettingsSection(FName Container, FName Category, FName Section)
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(SettingsModuleName);
		if (!SettingsModule)
		{
			return false;
		}
		const TSharedPtr<ISettingsContainer> ContainerPtr = SettingsModule->GetContainer(Container);
		if (!ContainerPtr.IsValid())
		{
			return false;
		}
		const TSharedPtr<ISettingsCategory> CategoryPtr = ContainerPtr->GetCategory(Category);
		return CategoryPtr.IsValid() && CategoryPtr->GetSection(Section).IsValid();
	}
}

FDigetherDevToolsEnvironment MakeDigetherLiveEnvironment()
{
	FDigetherDevToolsEnvironment Environment;
	Environment.GetPluginState = [](FName PluginName)
	{
		return DigetherDevToolsLive::ReadPluginState(PluginName);
	};
	Environment.HasGlobalTab = [](FName TabId)
	{
		return FGlobalTabmanager::Get()->HasTabSpawner(TabId);
	};
	Environment.HasLevelEditorTab = [](FName TabId)
	{
		const TSharedPtr<FTabManager> TabManager = DigetherDevToolsLive::GetLevelEditorTabManager();
		return TabManager.IsValid() && TabManager->HasTabSpawner(TabId);
	};
	Environment.HasSettingsSection = [](FName Container, FName Category, FName Section)
	{
		return DigetherDevToolsLive::HasSettingsSection(Container, Category, Section);
	};
	return Environment;
}

FDigetherAutoSaveValues ReadDigetherAutoSaveValues()
{
	FDigetherAutoSaveValues Values;
	const UEditorLoadingSavingSettings* Settings = GetDefault<UEditorLoadingSavingSettings>();
	if (Settings)
	{
		Values.bAvailable = true;
		Values.bEnabled = Settings->bAutoSaveEnable != 0;
		Values.bSaveMaps = Settings->bAutoSaveMaps != 0;
		Values.bSaveContent = Settings->bAutoSaveContent != 0;
		Values.IntervalMinutes = Settings->AutoSaveTimeMinutes;
	}
	return Values;
}

FDigetherAssetManagerValues ReadDigetherAssetManagerValues()
{
	FDigetherAssetManagerValues Values;
	const UAssetManagerSettings* Settings = GetDefault<UAssetManagerSettings>();
	if (Settings)
	{
		static const FName GameFeatureDataType(TEXT("GameFeatureData"));
		Values.bAvailable = true;
		Values.PrimaryAssetTypeCount = Settings->PrimaryAssetTypesToScan.Num();
		for (const FPrimaryAssetTypeInfo& TypeInfo : Settings->PrimaryAssetTypesToScan)
		{
			if (TypeInfo.PrimaryAssetType == GameFeatureDataType)
			{
				Values.bHasGameFeatureDataRule = true;
				break;
			}
		}
	}
	return Values;
}

bool TryOpenDigetherShortcut(const FDigetherShortcut& Shortcut, FText& OutFailureReason)
{
	const FDigetherShortcutAvailability Availability = EvaluateDigetherShortcut(Shortcut, MakeDigetherLiveEnvironment());
	if (!Availability.bAvailable)
	{
		OutFailureReason = Availability.FailureReason;
		return false;
	}

	switch (Shortcut.Target)
	{
	case EDigetherShortcutTarget::GlobalTab:
		if (!FGlobalTabmanager::Get()->TryInvokeTab(Shortcut.TabId).IsValid())
		{
			OutFailureReason = FText::Format(LOCTEXT("GlobalTabOpenFailed", "탭 '{0}'을 열지 못했습니다."), FText::FromName(Shortcut.TabId));
			return false;
		}
		return true;

	case EDigetherShortcutTarget::LevelEditorTab:
	{
		const TSharedPtr<FTabManager> TabManager = DigetherDevToolsLive::GetLevelEditorTabManager();
		if (!TabManager.IsValid() || !TabManager->TryInvokeTab(Shortcut.TabId).IsValid())
		{
			OutFailureReason = FText::Format(LOCTEXT("LevelTabOpenFailed", "레벨 에디터 탭 '{0}'을 열지 못했습니다."), FText::FromName(Shortcut.TabId));
			return false;
		}
		return true;
	}

	case EDigetherShortcutTarget::SettingsSection:
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(DigetherDevToolsLive::SettingsModuleName);
		if (!SettingsModule)
		{
			OutFailureReason = LOCTEXT("SettingsModuleMissing", "Settings 모듈이 로드되지 않았습니다.");
			return false;
		}
		SettingsModule->ShowViewer(Shortcut.SettingsContainer, Shortcut.SettingsCategory, Shortcut.SettingsSection);
		return true;
	}
	}

	OutFailureReason = LOCTEXT("UnknownShortcutTarget", "알 수 없는 바로가기 종류입니다.");
	return false;
}

#undef LOCTEXT_NAMESPACE
