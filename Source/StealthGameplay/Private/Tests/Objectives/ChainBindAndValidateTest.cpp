// Story oc-003 (production/epics/objectives-chain/story-003-core-chain-validation-and-court-binding.md), AC1 / AC2
// (AC-OC-01) ChainBindAndValidate, on FObjectivesCore with the fixture assignment through the stub source:
//   AC1  the MVP chain validates; per team one Prelim1 and one Prelim2 with distinct ids and text, one Final with Both; it
//        binds Prelim1(T) -> P1[T], Prelim2(T) -> P2[T], Final -> Final (once, for both teams); every site Locked; team 0 and
//        team 1 get different P1 and P2 courts; an assignment missing a P2 site fails and binds nothing.
//   AC2  one mutation each (ObjectivesPerStageCount 2, CompletionsRequiredCount 2, 17 rows, a duplicate ObjectiveId, a
//        SiteRole not matching StageKind, an empty CardTitle, a whitespace-only CardLine) returns the named enum value
//        EChainValidation::ObjectiveChainInvalid with the first failing clause, nothing is bound and the assignment is never
//        read. Two defects at once still give one result (the first clause). The 16-row bound: 17 rows fail TooManyRows; a
//        16-row chain does not fail the bound (it fails a later clause instead, because rule 1 allows exactly five MVP rows).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ObjectivesTestRig.h"

namespace ObjectivesChainBindAndValidateTestData
{
	using namespace StealthObjectivesTest;

	struct FMutation
	{
		const TCHAR* Name;
		EChainValidationClause Expected;
		TFunction<void(UObjectiveChainData&, FObjectivesTuning_Server&)> Apply;
	};

	void AddCopies(UObjectiveChainData& Chain, int32 Total)
	{
		int32 Next = 0;
		while (Chain.Definitions.Num() < Total)
		{
			FObjectiveDef Extra = Chain.Definitions[0];
			Extra.ObjectiveId = FName(*FString::Printf(TEXT("Extra.%d"), Next++));
			Chain.Definitions.Add(Extra);
		}
	}

	TArray<FMutation> Mutations()
	{
		TArray<FMutation> Out;
		Out.Add({ TEXT("ObjectivesPerStageCount 2"), EChainValidationClause::PerStageCount, [](UObjectiveChainData&, FObjectivesTuning_Server& T) { T.ObjectivesPerStageCount = 2; } });
		Out.Add({ TEXT("CompletionsRequiredCount 2"), EChainValidationClause::CompletionsRequired, [](UObjectiveChainData& C, FObjectivesTuning_Server&) { C.Definitions[1].CompletionsRequiredCount = 2; } });
		Out.Add({ TEXT("17 rows"), EChainValidationClause::TooManyRows, [](UObjectiveChainData& C, FObjectivesTuning_Server&) { AddCopies(C, 17); } });
		Out.Add({ TEXT("duplicate ObjectiveId"), EChainValidationClause::DuplicateId, [](UObjectiveChainData& C, FObjectivesTuning_Server&) { C.Definitions[2].ObjectiveId = C.Definitions[0].ObjectiveId; } });
		Out.Add({ TEXT("SiteRole not matching StageKind"), EChainValidationClause::RoleStageMismatch, [](UObjectiveChainData& C, FObjectivesTuning_Server&) { C.Definitions[1].SiteRole = EObjectiveSiteRole::P1; } });
		Out.Add({ TEXT("empty CardTitle"), EChainValidationClause::EmptyCardTitle, [](UObjectiveChainData& C, FObjectivesTuning_Server&) { C.Definitions[3].CardTitle = FText::GetEmpty(); } });
		Out.Add({ TEXT("whitespace-only CardLine"), EChainValidationClause::EmptyCardLine, [](UObjectiveChainData& C, FObjectivesTuning_Server&) { C.Definitions[4].CardLine = FText::FromString(TEXT("   ")); } });
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesChainBindAndValidateTest, "StealthGame.Objectives.ChainBindAndValidate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesChainBindAndValidateTest::RunTest(const FString& Parameters)
{
	using namespace ObjectivesChainBindAndValidateTestData;

	// AC1: the MVP chain validates and binds to the fixture assignment, every site Locked (before Active entry).
	{
		FCoreRig Rig(/*bActive*/ false);
		TestTrue(TEXT("the MVP chain validates"), FObjectivesCore::ValidateChain(Rig.Chain.Get(), FObjectivesTuning_Server()).IsValid());
		TestTrue(TEXT("the chain is bound"), Rig.Core.IsBound());
		TestEqual(TEXT("the assignment was read once"), Rig.Assignment.Reads, 1);
		const uint16 Expected[] = { CourtP1Team0, CourtP2Team0, CourtP1Team1, CourtP2Team1, CourtFinal };
		for (int32 Def = 0; Def < Rig.Core.GetDefinitionCount(); ++Def)
		{
			const FChainBinding* Binding = Rig.Core.GetBinding(Def);
			if (TestNotNull(FString::Printf(TEXT("row %d has a binding"), Def), Binding))
			{
				TestEqual(FString::Printf(TEXT("row %d binds its role's court"), Def), static_cast<int32>(Binding->Court.Value), static_cast<int32>(Expected[Def]));
			}
			for (int32 Team = 0; Team < 2; ++Team)
			{
				TestEqual(FString::Printf(TEXT("team %d row %d is Locked"), Team, Def), static_cast<int32>(Rig.Core.GetSiteState(Team, Def)), static_cast<int32>(EObjectiveSiteState::Locked));
			}
		}
		// Per team: one Prelim1 and one Prelim2 with distinct ids and text; one Final for Both, bound once.
		const UObjectiveChainData& Chain = *Rig.Chain;
		TestTrue(TEXT("team 0's prelims have distinct ids"), Chain.Definitions[DefP1Team0].ObjectiveId != Chain.Definitions[DefP2Team0].ObjectiveId);
		TestTrue(TEXT("team 1's prelims have distinct ids"), Chain.Definitions[DefP1Team1].ObjectiveId != Chain.Definitions[DefP2Team1].ObjectiveId);
		TestTrue(TEXT("team 0's and team 1's P1 card text differ"), !Chain.Definitions[DefP1Team0].CardLine.EqualTo(Chain.Definitions[DefP1Team1].CardLine));
		TestEqual(TEXT("the final is eligible for Both"), static_cast<int32>(Chain.Definitions[DefFinal].TeamEligibility), static_cast<int32>(EObjectiveEligibility::Both));
		int32 FinalBindings = 0;
		for (int32 Def = 0; Def < Rig.Core.GetDefinitionCount(); ++Def)
		{
			FinalBindings += Rig.Core.GetBinding(Def)->Court.Value == CourtFinal ? 1 : 0;
		}
		TestEqual(TEXT("the final court is bound once, for both teams"), FinalBindings, 1);
		TestTrue(TEXT("team 0 and team 1 get different P1 sites"), Rig.Core.GetBinding(DefP1Team0)->Court != Rig.Core.GetBinding(DefP1Team1)->Court);
		TestTrue(TEXT("team 0 and team 1 get different P2 sites"), Rig.Core.GetBinding(DefP2Team0)->Court != Rig.Core.GetBinding(DefP2Team1)->Court);
	}

	// AC1 edge: an assignment missing a P2 site fails validation of the bind and binds nothing (never a hole).
	{
		FCoreRig Rig(/*bActive*/ false);
		Rig.Core.Reset();
		Rig.Assignment.Assignment.P2[1] = FCourtFixtureId();
		const FChainValidationResult Result = Rig.Core.Bind(Rig.Chain.Get(), Rig.Assignment);
		TestEqual(TEXT("a missing P2 site is ObjectiveChainInvalid"), static_cast<int32>(Result.Result), static_cast<int32>(EChainValidation::ObjectiveChainInvalid));
		TestEqual(TEXT("the clause is AssignmentIncomplete"), static_cast<int32>(Result.Clause), static_cast<int32>(EChainValidationClause::AssignmentIncomplete));
		TestFalse(TEXT("nothing is bound"), Rig.Core.IsBound());
		TestEqual(TEXT("nothing is bound (no rows)"), Rig.Core.GetDefinitionCount(), 0);
	}

	// AC2: one mutation each rejects with the named enum value; nothing is bound and the assignment is never read.
	for (const FMutation& Mutation : Mutations())
	{
		FCoreRig Rig(/*bActive*/ false);
		Rig.Core.Reset();
		FObjectivesTuning_Server Tuning;
		UObjectiveChainData* Chain = MakeCacaoChain();
		Mutation.Apply(*Chain, Tuning);
		const FChainValidationResult Validated = FObjectivesCore::ValidateChain(Chain, Tuning);
		TestEqual(FString::Printf(TEXT("%s: EChainValidation::ObjectiveChainInvalid"), Mutation.Name), static_cast<int32>(Validated.Result), static_cast<int32>(EChainValidation::ObjectiveChainInvalid));
		TestEqual(FString::Printf(TEXT("%s: the first failing clause"), Mutation.Name), static_cast<int32>(Validated.Clause), static_cast<int32>(Mutation.Expected));
		Rig.Core.SetTuning(FObjectivesTuning_Public(), Tuning);
		const int32 ReadsBefore = Rig.Assignment.Reads;
		const FChainValidationResult Bound = Rig.Core.Bind(Chain, Rig.Assignment);
		TestFalse(FString::Printf(TEXT("%s: Bind fails"), Mutation.Name), Bound.IsValid());
		TestFalse(FString::Printf(TEXT("%s: nothing is bound"), Mutation.Name), Rig.Core.IsBound());
		TestEqual(FString::Printf(TEXT("%s: the assignment is not read"), Mutation.Name), Rig.Assignment.Reads, ReadsBefore);
	}

	// Two defects at once: one result, the first clause in rule order (the duplicate id on row 2 precedes the empty line of row 4).
	{
		UObjectiveChainData* Chain = MakeCacaoChain([](UObjectiveChainData& C)
		{
			C.Definitions[2].ObjectiveId = C.Definitions[0].ObjectiveId;
			C.Definitions[4].CardLine = FText::GetEmpty();
		});
		const FChainValidationResult Result = FObjectivesCore::ValidateChain(Chain, FObjectivesTuning_Server());
		TestEqual(TEXT("two defects: the first clause (DuplicateId)"), static_cast<int32>(Result.Clause), static_cast<int32>(EChainValidationClause::DuplicateId));
		TestEqual(TEXT("two defects: the row of the first clause"), Result.Row, 2);
	}

	// The 16-row bound: 16 rows do not fail the bound (a later rule fails: too many Prelim1 rows); 17 rows fail the bound.
	{
		UObjectiveChainData* Sixteen = MakeCacaoChain([](UObjectiveChainData& C) { AddCopies(C, 16); });
		const FChainValidationResult Result = FObjectivesCore::ValidateChain(Sixteen, FObjectivesTuning_Server());
		TestTrue(TEXT("16 rows: the row bound holds (not TooManyRows)"), Result.Clause != EChainValidationClause::TooManyRows);
		TestEqual(TEXT("16 rows: rule 1 (one Prelim1 per team) fails instead"), static_cast<int32>(Result.Clause), static_cast<int32>(EChainValidationClause::TeamStageRows));
	}
	TestEqual(TEXT("a null chain is NoChain"), static_cast<int32>(FObjectivesCore::ValidateChain(nullptr, FObjectivesTuning_Server()).Clause), static_cast<int32>(EChainValidationClause::NoChain));
	return true;
}

#endif
