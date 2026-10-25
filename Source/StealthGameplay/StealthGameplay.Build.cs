using UnrealBuildTool;

// ADR-0007 section 1: the Gameplay runtime module (Core/ systems and Features/ that build on them).
// Its Private dependency set (EnhancedInput, AIModule, ...) is set by ADR-0010/0011/0013/0016 when
// those systems land.
public class StealthGameplay : ModuleRules
{
	public StealthGameplay(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "StealthFoundation" });
	}
}
