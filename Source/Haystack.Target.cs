// Copyright (c) 2026 Vanan Andreas.

using UnrealBuildTool;

public class HaystackTarget : TargetRules
{
	public HaystackTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new[] { "Haystack", "HaystackGauntlet" });

		if (Target.Configuration == UnrealTargetConfiguration.Shipping)
		{
			if (bIsEngineInstalled)
			{
				System.Console.WriteLine("Haystack Shipping on an installed engine: no console, log or CSV profiler. Build against the engine source tree for those.");
			}
			else
			{
				BuildEnvironment = TargetBuildEnvironment.Unique;
				bUseLoggingInShipping = true;
				bUseConsoleInShipping = true;
				GlobalDefinitions.Add("CSV_PROFILER_ENABLE_IN_SHIPPING=1");
			}
		}
	}
}