// DIGETHER 전용 Editor 어댑터 모듈. 게임 Runtime 모듈(DIGETHER)을 참조하지 않는다.
using UnrealBuildTool;

public class DigetherWorkbenchEditor : ModuleRules
{
	public DigetherWorkbenchEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Slate",
			"SlateCore",
			"Projects",
			"Engine",
			"UnrealEd",
			"Settings",
			"DeveloperSettings",
			"LevelEditor",
			// DevelopmentWorkbench 플러그인의 Editor 모듈. IWorkbenchModule / FWorkbenchToolDescriptor 제공.
			"WorkbenchEditor"
		});
	}
}
