using UnrealBuildTool;

// ADR-0007 sections 1-3: the client-facing Presentation runtime module. The guard define is a
// PRIVATE definition so it reaches every translation unit of this module and none of its
// dependents; StealthServerOnlyGuard.h turns any include of a Public/Server/ header into a compile
// error here. Test StealthGame.Build.PresentationGuardDefineIsPrivate checks both halves.
public class StealthPresentation : ModuleRules
{
	public StealthPresentation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "SlateCore" });
		// NKP story 018: the Penalty HUD overlay (Slate) paints the tally line, the returning countdown and the exile card.
		PrivateDependencyModuleNames.AddRange(new string[] { "StealthFoundation", "StealthGameplay", "Slate" });

		PrivateDefinitions.Add("STEALTH_PRESENTATION_MODULE=1");
	}
}
