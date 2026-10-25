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

		// ADR-0008 section 4: test seams compile in every configuration except Shipping (!UE_BUILD_SHIPPING).
		// Public so the harness, the editor module and tests see it (story tdc-008).
		PublicDefinitions.Add(Target.Configuration == UnrealTargetConfiguration.Shipping ? "WITH_STEALTH_TEST_SEAMS=0" : "WITH_STEALTH_TEST_SEAMS=1");
	}
}
