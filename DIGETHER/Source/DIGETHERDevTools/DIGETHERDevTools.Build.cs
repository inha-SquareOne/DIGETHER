using UnrealBuildTool;

// 개발 치트툴(콘솔·웹 명령, 화면 디버그 표시) 전용 모듈
// DIGETHER.uproject의 TargetConfigurationDenyList로 Shipping에서는 빌드하지 않는다
// Target.cs의 ExtraModuleNames에 넣지 말 것(넣으면 Shipping에도 포함), 다른 모듈이 이 모듈에 의존하지 말 것
public class DIGETHERDevTools : ModuleRules
{
	public DIGETHERDevTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"DIGETHER",
			"HTTPServer",
			"Json",
			"AIModule",
			"GameplayTags",
			"NavigationSystem"
		});
	}
}
