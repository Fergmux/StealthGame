using UnrealBuildTool;

public class StealthGame : ModuleRules
{
	public StealthGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "NetCore",
			"AIModule", "GameplayTasks", "NavigationSystem", "DeveloperSettings"
		});

		// ADR-0007 composition root: StealthGame wires the production modules together.
		PrivateDependencyModuleNames.AddRange(new string[] { "StealthFoundation", "StealthGameplay", "StealthPresentation" });

		// Spike R5 (Concealment story con-021, non-Shipping): the render-thread frame time of the hide/show profile.
		PrivateDependencyModuleNames.Add("RenderCore");
	}
}
