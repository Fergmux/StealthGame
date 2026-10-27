using UnrealBuildTool;

public class StealthGameEditor : ModuleRules
{
	public StealthGameEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "ToolsetRegistry" });
		PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "StealthFoundation", "StealthGameplay", "StealthPresentation", "StealthGame", "BlueprintGraph", "EnhancedInput", "InputCore", "AssetRegistry", "InterchangeCore", "InterchangeEngine", "InterchangePipelines", "AssetTools", "SkeletalMeshEditor", "StateTreeEditorModule", "StateTreeModule", "GameplayStateTreeModule", "GameplayTags", "PropertyBindingUtils" });
	}
}
