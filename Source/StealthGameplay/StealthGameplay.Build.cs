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

		// ADR-0010 Shared Pawn (story SCL-001): UStealthMotionWarpingComponent derives from a MotionWarping type in a
		// public header, so MotionWarping is public; GameplayTasks and AIModule (AI possession in
		// tests, the crowd controller later) stay private.
		PublicDependencyModuleNames.Add("MotionWarping");
		PrivateDependencyModuleNames.AddRange(new string[] { "GameplayTasks", "AIModule" });

		// ADR-0011 section 9 (story iif-094): the client command builder binds Enhanced Input; its public header only
		// forward-declares the Enhanced Input types, so the dependency stays private. CommonUI: the builder subscribes to
		// the action router's input-mode change (ADR-0011 section 9, ADR-0021 E7; spike V5).
		PrivateDependencyModuleNames.AddRange(new string[] { "EnhancedInput", "InputCore", "CommonUI", "CommonInput", "UMG", "Slate", "SlateCore", "DeveloperSettings" });
	}
}
