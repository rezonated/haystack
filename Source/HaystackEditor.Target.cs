// Copyright (c) 2026 Vanan Andreas.

using UnrealBuildTool;

public class HaystackEditorTarget : TargetRules
{
	public HaystackEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new[] { "Haystack", "HaystackTests" });
	}
}