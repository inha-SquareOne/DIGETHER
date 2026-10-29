#include "DigetherDeveloperToolsModel.h"

#define LOCTEXT_NAMESPACE "DigetherDeveloperToolsModel"

TArray<FName> GetDigetherDevToolsPluginNames()
{
	return {
		FName(TEXT("AssetSearch")),
		FName(TEXT("ActorPalette")),
		FName(TEXT("LevelSnapshots")),
		FName(TEXT("GameplayInsights")),
		FName(TEXT("DataValidation")),
		FName(TEXT("AssetManagerEditor")),
		FName(TEXT("UVEditor")),
		FName(TEXT("EditorScriptingUtilities")),
		FName(TEXT("BlueprintHeaderView"))
	};
}

FDigetherDevToolsRow DescribeDigetherEngineVersion(uint32 Major, uint32 Minor, uint32 Patch)
{
	constexpr uint32 ExpectedMajor = 5;
	constexpr uint32 ExpectedMinor = 8;
	constexpr uint32 ExpectedPatch = 2;

	FDigetherDevToolsRow Row;
	Row.Label = LOCTEXT("EngineVersionLabel", "엔진 버전");
	Row.Value = FText::FromString(FString::Printf(TEXT("%u.%u.%u"), Major, Minor, Patch));

	if (Major == ExpectedMajor && Minor == ExpectedMinor && Patch == ExpectedPatch)
	{
		Row.Detail = LOCTEXT("EngineVersionMatch", "프로젝트 기준 5.8.2와 같습니다.");
	}
	else
	{
		Row.Severity = EDigetherDevToolsSeverity::Problem;
		Row.Detail = LOCTEXT("EngineVersionMismatch", "프로젝트 기준 5.8.2와 다릅니다. 다른 버전으로 자동 전환하지 마세요.");
	}
	return Row;
}

FDigetherDevToolsRow DescribeDigetherPlugin(FName PluginName, EDigetherPluginState State)
{
	FDigetherDevToolsRow Row;
	Row.Label = FText::FromName(PluginName);

	switch (State)
	{
	case EDigetherPluginState::Enabled:
		Row.Value = LOCTEXT("PluginEnabled", "활성");
		Row.Detail = LOCTEXT("PluginEnabledDetail", "프로젝트에서 켜져 있습니다. 기능 동작 여부는 별도로 확인해야 합니다.");
		break;
	case EDigetherPluginState::Disabled:
		Row.Value = LOCTEXT("PluginDisabled", "비활성");
		Row.Detail = LOCTEXT("PluginDisabledDetail", "엔진에서 발견됐지만 이 프로젝트에서 꺼져 있습니다.");
		Row.Severity = EDigetherDevToolsSeverity::Warning;
		break;
	case EDigetherPluginState::NotFound:
	default:
		Row.Value = LOCTEXT("PluginNotFound", "발견 안 됨");
		Row.Detail = LOCTEXT("PluginNotFoundDetail", "엔진 또는 프로젝트에서 이 플러그인을 찾지 못했습니다.");
		Row.Severity = EDigetherDevToolsSeverity::Problem;
		break;
	}
	return Row;
}

FDigetherDevToolsRow DescribeDigetherAutoSave(const FDigetherAutoSaveValues& Values)
{
	FDigetherDevToolsRow Row;
	Row.Label = LOCTEXT("AutoSaveLabel", "자동 저장 (개인 설정)");

	if (!Values.bAvailable)
	{
		Row.Value = LOCTEXT("AutoSaveUnavailable", "읽을 수 없음");
		Row.Detail = LOCTEXT("AutoSaveUnavailableDetail", "에디터 로딩/저장 설정을 읽지 못했습니다.");
		Row.Severity = EDigetherDevToolsSeverity::Problem;
		return Row;
	}

	if (!Values.bEnabled)
	{
		Row.Value = LOCTEXT("AutoSaveOff", "꺼짐");
		Row.Detail = LOCTEXT("AutoSaveOffDetail", "자동 저장이 꺼져 있어 작업 손실 보호가 없습니다. 설정 화면에서 직접 켜세요.");
		Row.Severity = EDigetherDevToolsSeverity::Warning;
		return Row;
	}

	const FText MapsText = Values.bSaveMaps ? LOCTEXT("MapsOn", "켜짐") : LOCTEXT("MapsOff", "꺼짐");
	const FText ContentText = Values.bSaveContent ? LOCTEXT("ContentOn", "켜짐") : LOCTEXT("ContentOff", "꺼짐");
	Row.Value = FText::Format(
		LOCTEXT("AutoSaveOnValue", "켜짐 · 맵 {0} · 콘텐츠 {1} · {2}분"),
		MapsText, ContentText, FText::AsNumber(Values.IntervalMinutes));

	if (!Values.bSaveMaps && !Values.bSaveContent)
	{
		Row.Detail = LOCTEXT("AutoSaveNoTargetDetail", "자동 저장은 켜져 있지만 맵과 콘텐츠 둘 다 꺼져 있어 저장 대상이 없습니다.");
		Row.Severity = EDigetherDevToolsSeverity::Warning;
	}
	else if (Values.IntervalMinutes < 1)
	{
		Row.Detail = LOCTEXT("AutoSaveBadIntervalDetail", "저장 간격이 1분 미만으로 읽혔습니다. 설정 화면에서 확인하세요.");
		Row.Severity = EDigetherDevToolsSeverity::Warning;
	}
	else
	{
		Row.Detail = LOCTEXT("AutoSaveOnDetail", "설정값만 읽은 결과입니다. 실제 자동 저장 동작은 검증하지 않았습니다.");
	}
	return Row;
}

FDigetherDevToolsRow DescribeDigetherAssetManagerRule(const FDigetherAssetManagerValues& Values)
{
	FDigetherDevToolsRow Row;
	Row.Label = LOCTEXT("AssetManagerRuleLabel", "GameFeatureData 규칙");

	if (!Values.bAvailable)
	{
		Row.Value = LOCTEXT("AssetManagerUnavailable", "읽을 수 없음");
		Row.Detail = LOCTEXT("AssetManagerUnavailableDetail", "Asset Manager 설정을 읽지 못했습니다.");
		Row.Severity = EDigetherDevToolsSeverity::Problem;
	}
	else if (!Values.bHasGameFeatureDataRule)
	{
		Row.Value = LOCTEXT("AssetManagerRuleMissing", "규칙 없음");
		Row.Detail = FText::Format(
			LOCTEXT("AssetManagerRuleMissingDetail", "PrimaryAssetTypesToScan {0}개 중 GameFeatureData 항목이 없습니다. 게임 기능 플러그인이 동작하지 않을 수 있습니다."),
			FText::AsNumber(Values.PrimaryAssetTypeCount));
		Row.Severity = EDigetherDevToolsSeverity::Problem;
	}
	else
	{
		Row.Value = LOCTEXT("AssetManagerRulePresent", "규칙 있음");
		Row.Detail = FText::Format(
			LOCTEXT("AssetManagerRulePresentDetail", "PrimaryAssetTypesToScan {0}개 중 GameFeatureData 항목이 있습니다. 항목의 존재만 확인했으며 로딩 동작은 검증하지 않았습니다."),
			FText::AsNumber(Values.PrimaryAssetTypeCount));
	}
	return Row;
}

TArray<FDigetherShortcut> GetDigetherShortcuts()
{
	TArray<FDigetherShortcut> Shortcuts;

	{
		FDigetherShortcut& S = Shortcuts.AddDefaulted_GetRef();
		S.Id = FName(TEXT("AssetSearch"));
		S.Label = LOCTEXT("ShortcutAssetSearch", "에셋 검색");
		S.Hint = LOCTEXT("ShortcutAssetSearchHint", "Asset Search의 Search 탭을 엽니다.");
		S.Target = EDigetherShortcutTarget::GlobalTab;
		S.PluginName = FName(TEXT("AssetSearch"));
		S.TabId = FName(TEXT("Search"));
	}
	{
		FDigetherShortcut& S = Shortcuts.AddDefaulted_GetRef();
		S.Id = FName(TEXT("ActorPalette"));
		S.Label = LOCTEXT("ShortcutActorPalette", "액터 팔레트");
		S.Hint = LOCTEXT("ShortcutActorPaletteHint", "Actor Palette 1 탭을 엽니다.");
		S.Target = EDigetherShortcutTarget::GlobalTab;
		S.PluginName = FName(TEXT("ActorPalette"));
		S.TabId = FName(TEXT("ActorPaletteTab1"));
	}
	{
		FDigetherShortcut& S = Shortcuts.AddDefaulted_GetRef();
		S.Id = FName(TEXT("LevelSnapshots"));
		S.Label = LOCTEXT("ShortcutLevelSnapshots", "레벨 스냅샷");
		S.Hint = LOCTEXT("ShortcutLevelSnapshotsHint", "Level Snapshots 창을 엽니다. 복원은 직접 수행합니다.");
		S.Target = EDigetherShortcutTarget::GlobalTab;
		S.PluginName = FName(TEXT("LevelSnapshots"));
		S.TabId = FName(TEXT("LevelSnapshots"));
	}
	{
		FDigetherShortcut& S = Shortcuts.AddDefaulted_GetRef();
		S.Id = FName(TEXT("RewindDebugger"));
		S.Label = LOCTEXT("ShortcutRewindDebugger", "되감기 디버거");
		S.Hint = LOCTEXT("ShortcutRewindDebuggerHint", "Rewind Debugger 탭을 엽니다. 레벨 에디터가 열려 있어야 합니다.");
		S.Target = EDigetherShortcutTarget::LevelEditorTab;
		S.PluginName = FName(TEXT("GameplayInsights"));
		S.TabId = FName(TEXT("RewindDebugger2"));
	}
	{
		FDigetherShortcut& S = Shortcuts.AddDefaulted_GetRef();
		S.Id = FName(TEXT("AutoSaveSettings"));
		S.Label = LOCTEXT("ShortcutAutoSave", "자동 저장 설정");
		S.Hint = LOCTEXT("ShortcutAutoSaveHint", "Editor Preferences의 Loading & Saving을 엽니다. 값은 설정 화면에서 직접 바꿉니다.");
		S.Target = EDigetherShortcutTarget::SettingsSection;
		S.SettingsContainer = FName(TEXT("Editor"));
		S.SettingsCategory = FName(TEXT("General"));
		S.SettingsSection = FName(TEXT("LoadingSaving"));
	}
	{
		FDigetherShortcut& S = Shortcuts.AddDefaulted_GetRef();
		S.Id = FName(TEXT("AssetManagerSettings"));
		S.Label = LOCTEXT("ShortcutAssetManager", "Asset Manager 설정");
		S.Hint = LOCTEXT("ShortcutAssetManagerHint", "Project Settings의 Asset Manager를 엽니다.");
		S.Target = EDigetherShortcutTarget::SettingsSection;
		S.SettingsContainer = FName(TEXT("Project"));
		S.SettingsCategory = FName(TEXT("Game"));
		S.SettingsSection = FName(TEXT("AssetManagerSettings"));
	}
	{
		FDigetherShortcut& S = Shortcuts.AddDefaulted_GetRef();
		S.Id = FName(TEXT("OutputLog"));
		S.Label = LOCTEXT("ShortcutOutputLog", "출력 로그");
		S.Hint = LOCTEXT("ShortcutOutputLogHint", "Output Log 탭을 엽니다.");
		S.Target = EDigetherShortcutTarget::GlobalTab;
		S.TabId = FName(TEXT("OutputLog"));
	}

	return Shortcuts;
}

FDigetherShortcutAvailability EvaluateDigetherShortcut(const FDigetherShortcut& Shortcut, const FDigetherDevToolsEnvironment& Environment)
{
	FDigetherShortcutAvailability Result;

	if (!Shortcut.PluginName.IsNone())
	{
		if (!Environment.GetPluginState)
		{
			Result.FailureReason = LOCTEXT("NoPluginProbe", "플러그인 상태를 조회할 수 없습니다.");
			return Result;
		}

		const EDigetherPluginState State = Environment.GetPluginState(Shortcut.PluginName);
		if (State == EDigetherPluginState::NotFound)
		{
			Result.FailureReason = FText::Format(LOCTEXT("PluginMissingReason", "{0} 플러그인을 찾지 못했습니다."), FText::FromName(Shortcut.PluginName));
			return Result;
		}
		if (State == EDigetherPluginState::Disabled)
		{
			Result.FailureReason = FText::Format(LOCTEXT("PluginDisabledReason", "{0} 플러그인이 이 프로젝트에서 비활성입니다."), FText::FromName(Shortcut.PluginName));
			return Result;
		}
	}

	switch (Shortcut.Target)
	{
	case EDigetherShortcutTarget::GlobalTab:
		if (!Environment.HasGlobalTab || !Environment.HasGlobalTab(Shortcut.TabId))
		{
			Result.FailureReason = FText::Format(LOCTEXT("GlobalTabMissingReason", "탭 '{0}'이 등록되지 않았습니다. 모듈이 아직 로드되지 않았을 수 있습니다."), FText::FromName(Shortcut.TabId));
			return Result;
		}
		break;
	case EDigetherShortcutTarget::LevelEditorTab:
		if (!Environment.HasLevelEditorTab || !Environment.HasLevelEditorTab(Shortcut.TabId))
		{
			Result.FailureReason = FText::Format(LOCTEXT("LevelTabMissingReason", "레벨 에디터에 탭 '{0}'이 등록되지 않았습니다. 레벨 에디터가 열려 있는지 확인하세요."), FText::FromName(Shortcut.TabId));
			return Result;
		}
		break;
	case EDigetherShortcutTarget::SettingsSection:
		if (!Environment.HasSettingsSection || !Environment.HasSettingsSection(Shortcut.SettingsContainer, Shortcut.SettingsCategory, Shortcut.SettingsSection))
		{
			Result.FailureReason = FText::Format(
				LOCTEXT("SettingsMissingReason", "설정 화면 {0}/{1}/{2}이(가) 등록되지 않았습니다."),
				FText::FromName(Shortcut.SettingsContainer), FText::FromName(Shortcut.SettingsCategory), FText::FromName(Shortcut.SettingsSection));
			return Result;
		}
		break;
	}

	Result.bAvailable = true;
	return Result;
}

TArray<FText> GetDigetherMenuGuidance()
{
	return {
		LOCTEXT("GuidanceDataValidation", "Data Validation: 레벨 에디터 메뉴 Tools > Data Validation > Validate Data... (콘텐츠 전체를 검사하므로 필요할 때 직접 실행)"),
		LOCTEXT("GuidanceReferenceViewer", "Reference Viewer: 콘텐츠 브라우저에서 에셋 우클릭 > Reference Viewer... (Alt+Shift+R, AssetManagerEditor 필요)"),
		LOCTEXT("GuidanceSizeMap", "Size Map: 콘텐츠 브라우저에서 에셋 우클릭 > Size Map... (Alt+Shift+M, AssetManagerEditor 필요)")
	};
}

#undef LOCTEXT_NAMESPACE
