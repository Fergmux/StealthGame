using System.IO;
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

		// ADR-0016 section 1 (Crowd epic, story cnb-002): the Crowd feature folder Features/Crowd/ with its own Public/Server
		// and Private folders (ADR-0007 module table: Features/ live inside StealthGameplay). Its public headers are on
		// the module's public include path ("Server/CrowdDirector.h"); the include lint resolves them by header name.
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Features", "Crowd", "Public"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Features", "Crowd", "Private"));

		// ADR-0007 / ADR-0018 (Assassination epic, stories asn-002 and asn-003): the Assassination feature folder
		// Features/Assassination/, laid out like Crowd ("Server/StrikeReachCore.h", "Server/DefenceStateComponent.h").
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Features", "Assassination", "Public"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Features", "Assassination", "Private"));

		// ADR-0007 / ADR-0019 (NPC-Kill Penalty & Informants epic, story nkp-001): the Penalty feature folder
		// Features/Penalty/, laid out like Assassination ("Server/StealthPenaltyService.h").
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Features", "Penalty", "Public"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Features", "Penalty", "Private"));

		// ADR-0007 / ADR-0020 (Objectives Chain epic, stories oc-002..oc-009): the Objectives feature folder Features/Objectives/,
		// laid out like Penalty ("View/ObjectiveChainData.h" client-safe, "Server/ObjectivesService.h" guarded).
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Features", "Objectives", "Public"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Features", "Objectives", "Private"));

		// ADR-0016 sections 1, 3, 4 (stories cnb-004, cnb-007, cnb-012): the crowd's StateTree (the StateTree and
		// GameplayStateTree plugins are enabled in the .uproject), its native gameplay-tag events and the async path
		// queries. The StateTree types sit in public Crowd headers (the schema, the component, the task), so the two
		// StateTree modules and GameplayTags are public; NavigationSystem stays private.
		PublicDependencyModuleNames.AddRange(new string[] { "StateTreeModule", "GameplayStateTreeModule", "GameplayTags" });
		PrivateDependencyModuleNames.Add("NavigationSystem");

		// ADR-0020 section 6 (Objectives story oc-011): UObjectiveTeamViewComponent marks its push-model property dirty
		// (MARK_PROPERTY_DIRTY_FROM_NAME links against NetCore's UEPushModelPrivate).
		PrivateDependencyModuleNames.Add("NetCore");
	}
}
