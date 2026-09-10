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
	}
}