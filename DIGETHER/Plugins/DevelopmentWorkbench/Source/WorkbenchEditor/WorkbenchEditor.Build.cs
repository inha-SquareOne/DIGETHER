// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WorkbenchEditor : ModuleRules
{
	public WorkbenchEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Public 헤더가 직접 노출하는 타입(UDataTable, UEditorValidatorBase, Slate 위젯)의 모듈.
		// Json은 Public 헤더에 노출되지 않아 Private이다.
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"DataValidation"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"InputCore",
			"ApplicationCore",
			"Projects",
			"PropertyEditor",
			"ToolMenus",
			"AssetRegistry",
			"AssetTools",
			"ContentBrowser",
			"DesktopPlatform",
			"EditorSubsystem",
			"LevelEditor",
			"Kismet",
			"KismetWidgets",
			"BlueprintGraph",
			"GraphEditor",
			"Json",
			"JsonUtilities",
			"Sockets",
			"Networking"
		});
	}
}
