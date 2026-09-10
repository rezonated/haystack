// Copyright (c) 2026 Vanan Andreas.

using UnrealBuildTool;
public class HaystackTests : ModuleRules
{
	public HaystackTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"CoreOnline",
			"OnlineSubsystem",
			"Haystack",
		});
	}
}
