using UnrealBuildTool;

// ADR-0007 section 1: the Foundation runtime module (contracts, server-only services, shared types).
// Story tdc-001 test StealthGame.Build.FoundationLayout asserts the exact dependency sets below.
public class StealthFoundation : ModuleRules
{
	public StealthFoundation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "NetCore" });
		PrivateDependencyModuleNames.AddRange(new string[] { "DeveloperSettings", "Json" });
	}
}
