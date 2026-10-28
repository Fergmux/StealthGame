// Objectives Chain story oc-013 AC1 (AC-OC-13 [U]): StealthGame.Objectives.ClueBoundAndRoleBlind.
// GIVEN the fixture layout (12 courts, public ClueTag records, ObjectivesClueFixture.h) and 1,000 pinned assignments, THEN every
// bound site's clue (the one its team sees when it is live) matches 1 to ClueMatchMaxCount courts, equals the role-blind
// FObjectivesClue::ClueOf of its court's record, and its client string (BuildClueText) is a function of the record only:
// rotating the five roles among the same courts (a role perturbation) leaves every court's string unchanged. The graybox half
// of AC-OC-13 waits for the Level Socket Contract's ClueTag records (not built): recorded in the story.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ObjectivesClue.h"
#include "ObjectivesClueFixture.h"
#include "ObjectivesTestRig.h"
#include "View/ObjectiveViewHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesClueBoundAndRoleBlindTest, "StealthGame.Objectives.ClueBoundAndRoleBlind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesClueBoundAndRoleBlindTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	FCoreRig Rig(/*bActive*/ false);
	StealthObjectivesClueFixture::Records(Rig.Clues.Records);
	const int32 MaxCount = FObjectivesTuning_Server().ClueMatchMaxCount;

	int32 Violations = 0;
	int32 PerturbationChanges = 0;
	int32 Sites = 0;
	for (int32 Index = 0; Index < StealthObjectivesClueFixture::AssignmentCount; ++Index)
	{
		// Arrange: the pinned assignment, then its role rotation over the same five courts.
		TMap<uint16, FString> Strings[2];
		for (int32 Variant = 0; Variant < 2; ++Variant)
		{
			const FSiteAssignment Base = StealthObjectivesClueFixture::Assignment(Index);
			Rig.Assignment.Assignment = Variant == 0 ? Base : StealthObjectivesClueFixture::RotateRoles(Base);
			// Act
			if (!Rig.Core.Bind(Rig.Chain.Get(), Rig.Assignment, static_cast<uint64>(Index * 2 + Variant + 1)).IsValid())
			{
				AddError(FString::Printf(TEXT("assignment %d variant %d: bind failed"), Index, Variant));
				return false;
			}
			Rig.Core.HandleActiveEntry(RoundEpoch + Index, ActivePhaseStart);
			// Assert: every bound site (the clue its team's card shows when the site is live).
			for (int32 Def = 0; Def < Rig.Core.GetDefinitionCount(); ++Def)
			{
				const FChainBinding* Binding = Rig.Core.GetBinding(Def);
				const FCourtClueRecord* Record = FObjectivesClue::FindRecord(Rig.Clues.Records, Binding->Court);
				++Sites;
				const bool bOk = Record != nullptr && Binding->Clue == FObjectivesClue::ClueOf(*Record) && Binding->ClueMatches >= 1 && Binding->ClueMatches <= MaxCount
					&& FObjectivesClue::CountMatches(Rig.Clues.Records, Binding->Clue) == Binding->ClueMatches;
				Violations += bOk ? 0 : 1;
				Strings[Variant].Add(Binding->Court.Value, ObjectiveViewHelpers::BuildClueText(Binding->Clue).ToString());
			}
			// The live sites in each team's view carry the same clue as their binding.
			for (int32 Team = 0; Team < 2; ++Team)
			{
				for (const FObjectiveLiveSite& Site : Rig.Core.BuildTeamView(Team).LiveSites)
				{
					const FChainBinding* Binding = Rig.Core.GetBinding(Site.DefIndex);
					Violations += Binding != nullptr && Binding->Clue == Site.Clue && Site.Clue.AreaType != 0 ? 0 : 1;
				}
			}
		}
		for (const TPair<uint16, FString>& Court : Strings[0])
		{
			const FString* Other = Strings[1].Find(Court.Key);
			PerturbationChanges += Other != nullptr && *Other == Court.Value ? 0 : 1;
		}
	}
	AddInfo(FString::Printf(TEXT("OC13 assignments=%d sites=%d violations=%d role-perturbation changes=%d (ClueMatchMaxCount %d)"),
		StealthObjectivesClueFixture::AssignmentCount, Sites, Violations, PerturbationChanges, MaxCount));
	TestEqual(TEXT("every bound site's clue matches 1..ClueMatchMaxCount courts and is ClueOf its own record"), Violations, 0);
	TestEqual(TEXT("rotating the roles over the same courts changes no court's clue string"), PerturbationChanges, 0);
	TestEqual(TEXT("no ClueTooBroad on a layout within the bound"), Rig.Round.Count(ERoundLogType::ClueTooBroad), 0);

	// Structural: ClueOf takes one public record and nothing else, and is deterministic.
	FCourtClueRecord Record;
	Record.Court = FCourtFixtureId(101);
	Record.AreaTypeId = static_cast<uint8>(EClueAreaType::Market);
	Record.NearestFeatureKind = static_cast<uint8>(EClueFeatureKind::WaterSteps);
	TestTrue(TEXT("the same record gives the same clue"), FObjectivesClue::ClueOf(Record) == FObjectivesClue::ClueOf(Record));
	return true;
}

#endif
