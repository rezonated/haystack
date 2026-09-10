// Copyright (c) 2026 Vanan Andreas.

using UnrealBuildTool;
public class HaystackGauntlet : ModuleRules
{
	public HaystackGauntlet(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Gauntlet",
			"RenderCore",
			"RHI",
			"Haystack",
		});
	}
}
