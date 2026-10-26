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

		// ADR-0006 section 2, story st-009: the session secret comes from BCryptGenRandom (FOsEntropySource).
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.Add("bcrypt.lib");
		}

		// ADR-0004 section 2, story tdc-042: the schema bootstrap. Only tools/ci/regenerate-tuning-schema.ps1
		// sets the environment variable STEALTH_TUNING_SCHEMA_BOOTSTRAP=1 (and deletes the UBT makefile so this
		// file is re-read); the module then compiles an empty schema table instead of
		// Private/Tuning/StealthTuningSchema.generated.inl, so the editor builds after a tuning field rename and the
		// generator can rewrite the table. SchemaComplete fails on an empty table: such a build never passes CI.
		bool bSchemaBootstrap = System.Environment.GetEnvironmentVariable("STEALTH_TUNING_SCHEMA_BOOTSTRAP") == "1";
		PrivateDefinitions.Add(bSchemaBootstrap ? "STEALTH_TUNING_SCHEMA_BOOTSTRAP=1" : "STEALTH_TUNING_SCHEMA_BOOTSTRAP=0");
	}
}
