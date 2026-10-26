using UnrealBuildTool;

// ADR-0007 section 1 / ADR-0008 sections 1 and 3: the multi-client and multi-process test harness.
// Excluded from Shipping and Test by the .uproject TargetConfigurationDenyList; never listed in a
// Target.cs ExtraModuleNames and never a dependency of a runtime module. Private dependencies are
// all runtime modules, including the composition root StealthGame (AStealthGameMode for T2 tests).
public class StealthTestHarness : ModuleRules
{
	public StealthTestHarness(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "NetCore", "EnhancedInput", "InputCore", "Json", "OnlineSubsystemUtils", "CommonUI", "CommonInput", "UMG", "Slate", "SlateCore", "AIModule", "GameplayTasks", "StateTreeModule", "GameplayStateTreeModule", "GameplayTags", "StealthFoundation", "StealthGameplay", "StealthPresentation", "StealthGame"
		});

		// ADR-0008 section 1, T2: CQTest's in-process PIE network tests exist only in editor builds
		// (ENABLE_PIE_NETWORK_TEST needs WITH_EDITOR). UnrealEd loads the fixture map (story tdc-019).
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "CQTest", "UnrealEd", "LevelEditor" });
		}

		// CQTest's PIENetworkComponent.h includes Iris replication headers.
		SetupIrisSupport(Target);
	}
}
