using UnrealBuildTool;

public class StealthGameEditorTarget : TargetRules
{
	public StealthGameEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "StealthGame", "StealthGameEditor" });
	}
}
