// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DIGETHER : ModuleRules
{
	public DIGETHER(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "Json", "CoreOnline", "NetCore", "OnlineSubsystem", "OnlineSubsystemUtils", "ProceduralMeshComponent", "SlateCore", "ApplicationCore", "DeveloperSettings", "CommonUI", "CommonInput", "GameplayTags" });

		PublicIncludePaths.AddRange(new string[] {
			"DIGETHER",
			"DIGETHER/Variant_Platforming",
			"DIGETHER/Variant_Platforming/Animation",
			"DIGETHER/Variant_Combat",
			"DIGETHER/Variant_Combat/AI",
			"DIGETHER/Variant_Combat/Animation",
			"DIGETHER/Variant_Combat/Gameplay",
			"DIGETHER/Variant_Combat/Interfaces",
			"DIGETHER/Variant_Combat/UI",
			"DIGETHER/Variant_SideScrolling",
			"DIGETHER/Variant_SideScrolling/AI",
			"DIGETHER/Variant_SideScrolling/Gameplay",
			"DIGETHER/Variant_SideScrolling/Interfaces",
			"DIGETHER/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
