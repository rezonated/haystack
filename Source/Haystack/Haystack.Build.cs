// Copyright (c) 2026 Vanan Andreas.

using UnrealBuildTool;
public class Haystack : ModuleRules
{
	public Haystack(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"NetCore",
			"UMG",
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"AIModule", "CoreOnline", "NavigationSystem", "RenderCore", "RHI",
		});
	}
}