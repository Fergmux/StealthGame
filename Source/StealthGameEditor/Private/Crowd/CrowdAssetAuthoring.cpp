// Crowd stories cnb-010 and cnb-012: the crowd's data and StateTree assets, authored in the editor from code so they can
// be rebuilt after any schema change (ADR-0016 section 3: the ScheduledTickPolicy is cached on the compiled tree).
//   Stealth.Crowd.BuildAssets   creates or refreshes, under /Game/StealthGame/AI:
//     DA_CrowdMotionProfile_Default  UCrowdMotionProfileData with the provisional device profiles (GDD OQ12)
//     ST_CrowdFull                   the GDD States table on UCrowdStateTreeSchema
//     ST_CrowdRestricted             Routing, Pausing, Halted (the participant-bound subset, CNB-R10)
//   and saves exactly those packages (never another asset; production/never-resave-assets.md).
// Tree shape: Root selects its children in order (Halted, Reacting, then the rest: the interrupt priority); each child
// has the enter condition "agent activity is X" and the one FCrowdActivityTask; a completed child goes back to Root,
// which re-selects the child matching the agent; the five crowd events re-select from Root too.

#include "CoreMinimal.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Server/CrowdMotionProfileData.h"
#include "Server/CrowdStateTreeSchema.h"
#include "Server/CrowdStateTreeTypes.h"
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogStealthCrowdAssets, Log, All);

namespace StealthCrowdAuthoring
{
	const TCHAR* const Folder = TEXT("/Game/StealthGame/AI");

	template <typename T>
	T* LoadOrCreate(const TCHAR* Name, TArray<UPackage*>& OutPackages)
	{
		const FString PackageName = FString::Printf(TEXT("%s/%s"), Folder, Name);
		UPackage* Package = FPackageName::DoesPackageExist(PackageName) ? LoadPackage(nullptr, *PackageName, LOAD_None) : CreatePackage(*PackageName);
		check(Package);
		T* Asset = FindObject<T>(Package, Name);
		if (!Asset)
		{
			Asset = NewObject<T>(Package, Name, RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Asset);
		}
		Package->MarkPackageDirty();
		OutPackages.Add(Package);
		return Asset;
	}

	struct FStateSpec
	{
		const TCHAR* Name;
		ECrowdTreeActivity Activity;
	};

	/** Root's children in selection order: Halted > Reacting > the rest (ADR-0016 section 3 interrupt priority). */
	const FStateSpec FullStates[] = {
		{ TEXT("Halted"), ECrowdTreeActivity::Halted },
		{ TEXT("Reacting"), ECrowdTreeActivity::Reacting },
		{ TEXT("Dormant"), ECrowdTreeActivity::Dormant },
		{ TEXT("Emerging"), ECrowdTreeActivity::Emerging },
		{ TEXT("Routing"), ECrowdTreeActivity::Routing },
		{ TEXT("Pausing"), ECrowdTreeActivity::Pausing },
		{ TEXT("Conversing"), ECrowdTreeActivity::Conversing },
		{ TEXT("Tending"), ECrowdTreeActivity::Tending },
		{ TEXT("Bracing"), ECrowdTreeActivity::Bracing },
		{ TEXT("Hiding"), ECrowdTreeActivity::Hiding },
		{ TEXT("Exiting"), ECrowdTreeActivity::Exiting },
	};

	const FStateSpec RestrictedStates[] = {
		{ TEXT("Halted"), ECrowdTreeActivity::Halted },
		{ TEXT("Routing"), ECrowdTreeActivity::Routing },
		{ TEXT("Pausing"), ECrowdTreeActivity::Pausing },
	};

	/** Rebuilds Tree's editor data from States on the crowd schema and compiles it. Returns false on a compile error. */
	bool BuildTree(UStateTree& Tree, TConstArrayView<FStateSpec> States)
	{
		UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(&Tree, NAME_None, RF_Transactional);
		Tree.EditorData = EditorData;
		EditorData->Schema = NewObject<UCrowdStateTreeSchema>(EditorData, NAME_None, RF_Transactional);
		UStateTreeState& Root = EditorData->AddSubTree(FName(TEXT("Root")));
		for (const FStateSpec& Spec : States)
		{
			UStateTreeState& State = Root.AddChildState(FName(Spec.Name));
			State.AddEnterCondition<FCrowdActivityCondition>().GetInstanceData().Activity = Spec.Activity;
			State.AddTask<FCrowdActivityTask>().GetInstanceData().Activity = Spec.Activity;
			State.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Root);
		}
		// Event interrupts: every crowd event re-selects from Root (the agent has already taken the stimulus).
		const FGameplayTag Events[] = { TAG_Crowd_Stimulus_Strike, TAG_Crowd_Stimulus_Salient, TAG_Crowd_Path_Blocked, TAG_Crowd_Path_Failed, TAG_Crowd_Converse_Faced };
		for (const FGameplayTag& Event : Events)
		{
			Root.AddTransition(EStateTreeTransitionTrigger::OnEvent, Event, EStateTreeTransitionType::GotoState, &Root);
		}
		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		const bool bCompiled = Compiler.Compile(Tree);
		Log.DumpToLog(LogStealthCrowdAssets);
		return bCompiled && Tree.IsReadyToRun();
	}

	bool SavePackages(const TArray<UPackage*>& Packages)
	{
		bool bAllSaved = true;
		for (UPackage* Package : Packages)
		{
			const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
			FSavePackageArgs Args;
			Args.TopLevelFlags = RF_Public | RF_Standalone;
			Args.Error = GError;
			const bool bSaved = UPackage::SavePackage(Package, nullptr, *File, Args);
			UE_LOGF(LogStealthCrowdAssets, Display, "%ls %ls", bSaved ? TEXT("saved") : TEXT("FAILED to save"), *File);
			bAllSaved &= bSaved;
		}
		return bAllSaved;
	}

	void BuildAssets()
	{
		TArray<UPackage*> Packages;
		UCrowdMotionProfileData* Profile = LoadOrCreate<UCrowdMotionProfileData>(TEXT("DA_CrowdMotionProfile_Default"), Packages);
		Profile->Profiles = UCrowdMotionProfileData::MakeProvisionalProfiles();
		UStateTree* Full = LoadOrCreate<UStateTree>(TEXT("ST_CrowdFull"), Packages);
		UStateTree* Restricted = LoadOrCreate<UStateTree>(TEXT("ST_CrowdRestricted"), Packages);
		const bool bFull = BuildTree(*Full, FullStates);
		const bool bRestricted = BuildTree(*Restricted, RestrictedStates);
		if (!bFull || !bRestricted)
		{
			UE_LOGF(LogStealthCrowdAssets, Error, "STEALTH CROWD ASSETS: FAIL (StateTree compile: full %d, restricted %d)", bFull ? 1 : 0, bRestricted ? 1 : 0);
			return;
		}
		const bool bSaved = SavePackages(Packages);
		UE_LOGF(LogStealthCrowdAssets, Display, "STEALTH CROWD ASSETS: %ls (full on crowd schema %d, restricted %d)", bSaved ? TEXT("PASS") : TEXT("FAIL"),
			UCrowdStateTreeSchema::IsCrowdTree(Full) ? 1 : 0, UCrowdStateTreeSchema::IsCrowdTree(Restricted) ? 1 : 0);
	}

	FAutoConsoleCommand BuildCommand(
		TEXT("Stealth.Crowd.BuildAssets"),
		TEXT("Crowd stories cnb-010/012: creates or refreshes DA_CrowdMotionProfile_Default, ST_CrowdFull and ST_CrowdRestricted under /Game/StealthGame/AI (crowd schema, compiled) and saves exactly those packages."),
		FConsoleCommandDelegate::CreateStatic(&BuildAssets));
}
