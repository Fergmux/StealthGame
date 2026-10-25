using UnrealBuildTool;

// ADR-0007 section 1 / ADR-0008 sections 1 and 3: the multi-client and multi-process test harness.
// Excluded from Shipping and Test by the .uproject TargetConfigurationDenyList; never listed in a
// Target.cs ExtraModuleNames and never a dependency of a runtime module. Private dependencies are
// the runtime modules that exist so far; StealthGame and CQTest join with story tdc-019.
public class StealthTestHarness : ModuleRules
{
	public StealthTestHarness(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "StealthFoundation", "StealthGameplay", "StealthPresentation"
		});
	}
}
