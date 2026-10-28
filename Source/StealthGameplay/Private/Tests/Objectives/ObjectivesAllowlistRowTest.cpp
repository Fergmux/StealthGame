// Story oc-011 (production/epics/objectives-chain/story-011-team-view-component-and-replication.md), AC3, headless half
// (ADR-0020 section 6, ADR-0003 section 8): StealthGame.Objectives.ObjectivesAllowlistRow.
//   - UObjectiveTeamViewComponent registers exactly one replicated property, View: push-based, COND_None (the controller is
//     the owner-only gate), ReplicatedUsing OnRep_View.
//   - The checked-in allowlist (Source/StealthFoundation/Private/Tests/Data/ReplicationAllowlist.json) has exactly one row for
//     UObjectiveTeamViewComponent::View: audience Team, carrier C-TeamView, mapped to the IVP R3 rows ObjectiveProgress,
//     SiteKnownToTeam and RivalPrelimCount, signed off by the Policy and Objectives.
//   - The per-class reflection audit passes on the file, and FAILS (negative controls) when that row is removed, when a duplicate
//     row is seeded, and when the row's condition is wrong.
//   - Every write of View marks it dirty: in the component's source, outside the test seams, each assignment of View is followed
//     in the same function by MARK_PROPERTY_DIRTY_FROM_NAME(UObjectiveTeamViewComponent, View, this) (a seeded writer without
//     the mark is reported). The replicated effect, with Net.IsPushModelEnabled 1 and 0 and the unmarked-write control, is the
//     PIE half: StealthGame.Objectives.PIE.RivalCountOneUpdate.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "ObjectivesSourceScan.h"
#include "Server/Testing/StealthReplicationAllowlist.h"
#include "View/ObjectiveTeamViewComponent.h"

namespace ObjectivesAllowlistRowTestData
{
	const TCHAR* const ClassName = TEXT("UObjectiveTeamViewComponent");

	/** Writes of the View member in Text that are not followed by the dirty mark before the function's closing brace. */
	int32 CountUnmarkedWrites(const FString& Text)
	{
		const FString Code = StealthObjectivesSourceScan::StripCommentsAndLiterals(Text);
		int32 Unmarked = 0;
		int32 From = 0;
		while (true)
		{
			const int32 At = Code.Find(TEXT("View = "), ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
			if (At == INDEX_NONE)
			{
				return Unmarked;
			}
			From = At + 1;
			if (At > 0 && (FChar::IsAlnum(Code[At - 1]) || Code[At - 1] == TEXT('_') || Code[At - 1] == TEXT('.') || Code[At - 1] == TEXT('>')))
			{
				continue; // another identifier (NewView, Out.View, ...), not the member
			}
			const int32 End = Code.Find(TEXT("\n}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, At);
			const FString Body = Code.Mid(At, End == INDEX_NONE ? MAX_int32 : End - At);
			Unmarked += Body.Contains(TEXT("MARK_PROPERTY_DIRTY_FROM_NAME(UObjectiveTeamViewComponent, View, this)")) ? 0 : 1;
		}
	}

	/** Text with every #if WITH_STEALTH_TEST_SEAMS ... #endif block removed (the seams are the documented exception). */
	FString WithoutTestSeams(const FString& Text)
	{
		FString Out;
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, /*bCullEmpty*/ false);
		int32 Depth = 0;
		for (const FString& Line : Lines)
		{
			const FString Trimmed = Line.TrimStartAndEnd();
			if (Depth == 0 && Trimmed == TEXT("#if WITH_STEALTH_TEST_SEAMS"))
			{
				Depth = 1;
				continue;
			}
			if (Depth > 0)
			{
				Depth += Trimmed.StartsWith(TEXT("#if")) ? 1 : 0;
				Depth -= Trimmed.StartsWith(TEXT("#endif")) ? 1 : 0;
				continue;
			}
			Out += Line + TEXT("\n");
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesAllowlistRowTest, "StealthGame.Objectives.ObjectivesAllowlistRow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesAllowlistRowTest::RunTest(const FString& Parameters)
{
	using namespace ObjectivesAllowlistRowTestData;

	// The component's registration: one push-based property, COND_None.
	TArray<FLifetimeProperty> Lifetime;
	GetDefault<UObjectiveTeamViewComponent>()->GetLifetimeReplicatedProps(Lifetime);
	const FProperty* ViewProperty = UObjectiveTeamViewComponent::StaticClass()->FindPropertyByName(TEXT("View"));
	if (!TestNotNull(TEXT("the View property exists"), ViewProperty))
	{
		return false;
	}
	const FLifetimeProperty* Registered = Lifetime.FindByPredicate([ViewProperty](const FLifetimeProperty& L) { return L.RepIndex == ViewProperty->RepIndex; });
	TestTrue(TEXT("View is registered for replication"), Registered != nullptr);
	TestTrue(TEXT("View is push-based (bIsPushBased)"), Registered != nullptr && Registered->bIsPushBased);
	TestTrue(TEXT("View is COND_None (the owner-only controller is the gate)"), Registered != nullptr && Registered->Condition == COND_None);
	TestTrue(TEXT("View is ReplicatedUsing OnRep_View"), ViewProperty->HasAnyPropertyFlags(CPF_RepNotify) && ViewProperty->RepNotifyFunc == FName(TEXT("OnRep_View")));
	int32 OwnReplicated = 0;
	for (TFieldIterator<FProperty> It(UObjectiveTeamViewComponent::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		OwnReplicated += It->HasAnyPropertyFlags(CPF_Net) ? 1 : 0;
	}
	TestEqual(TEXT("the component replicates exactly one property of its own"), OwnReplicated, 1);

	// The allowlist row.
	TArray<FReplicationAllowlistRow> Rows;
	FString Error;
	if (!TestTrue(TEXT("the allowlist parses"), StealthReplicationAllowlist::Load(Rows, Error)))
	{
		AddError(Error);
		return false;
	}
	const TArray<FReplicationAllowlistRow> Ours = Rows.FilterByPredicate([](const FReplicationAllowlistRow& R) { return R.Class == ClassName && R.Property == TEXT("View"); });
	if (!TestEqual(TEXT("exactly one row for UObjectiveTeamViewComponent::View"), Ours.Num(), 1))
	{
		return false;
	}
	const FReplicationAllowlistRow& Row = Ours[0];
	TestEqual(TEXT("audience Team"), Row.Audience, FString(TEXT("Team")));
	TestEqual(TEXT("carrier C-TeamView"), Row.Carrier, FString(TEXT("C-TeamView")));
	TestEqual(TEXT("condition COND_None"), Row.Condition, FString(TEXT("COND_None")));
	for (const TCHAR* PolicyRow : { TEXT("ObjectiveProgress"), TEXT("SiteKnownToTeam"), TEXT("RivalPrelimCount") })
	{
		TestTrue(FString::Printf(TEXT("mapped to the IVP R3 row %s"), PolicyRow), Row.PolicyRows.Contains(PolicyRow));
	}
	TestTrue(TEXT("signed off by the Policy and Objectives"), Row.Owners.Contains(TEXT("information-visibility-policy")) && Row.Owners.Contains(TEXT("objectives-chain")));

	// The audit passes on the file ...
	TArray<FString> Findings;
	StealthReplicationAllowlist::AuditClass(UObjectiveTeamViewComponent::StaticClass(), Rows, Findings);
	TestEqual(TEXT("the reflection audit passes on the checked-in file"), Findings.Num(), 0);
	for (const FString& Finding : Findings)
	{
		AddError(Finding);
	}
	// ... and fails without the row, with a duplicate, and with a wrong condition (negative controls).
	{
		TArray<FReplicationAllowlistRow> Removed = Rows.FilterByPredicate([](const FReplicationAllowlistRow& R) { return R.Class != ClassName; });
		TArray<FString> Out;
		StealthReplicationAllowlist::AuditClass(UObjectiveTeamViewComponent::StaticClass(), Removed, Out);
		TestTrue(TEXT("control: the audit fails when the row is removed"), Out.Num() > 0);
	}
	{
		TArray<FReplicationAllowlistRow> Duplicated = Rows;
		Duplicated.Add(Row);
		TArray<FString> Out;
		StealthReplicationAllowlist::AuditClass(UObjectiveTeamViewComponent::StaticClass(), Duplicated, Out);
		TestTrue(TEXT("control: the audit fails on a seeded duplicate row"), Out.Num() > 0);
	}
	{
		TArray<FReplicationAllowlistRow> Wrong = Rows;
		for (FReplicationAllowlistRow& R : Wrong)
		{
			if (R.Class == ClassName)
			{
				R.Condition = TEXT("COND_SkipOwner");
			}
		}
		TArray<FString> Out;
		StealthReplicationAllowlist::AuditClass(UObjectiveTeamViewComponent::StaticClass(), Wrong, Out);
		TestTrue(TEXT("control: the audit fails on a wrong condition"), Out.Num() > 0);
	}

	// Every write marks the property dirty (source check of the only writer).
	FString Source;
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/StealthGameplay/Features/Objectives/Private/ObjectiveTeamViewComponent.cpp"));
	if (TestTrue(TEXT("the component's source is readable"), FFileHelper::LoadFileToString(Source, *Path)))
	{
		const FString Production = WithoutTestSeams(Source);
		TestTrue(TEXT("the source writes View (positive control)"), Production.Contains(TEXT("View = NewView;")));
		TestEqual(TEXT("every production write of View marks it dirty"), CountUnmarkedWrites(Production), 0);
	}
	const FString SeededWriter = TEXT("void UObjectiveTeamViewComponent::Bad(const FObjectiveTeamView& NewView)\n{\n\tView = NewView;\n}\n");
	TestEqual(TEXT("control: a seeded write without the dirty mark is reported"), CountUnmarkedWrites(SeededWriter), 1);
	return true;
}

#endif
