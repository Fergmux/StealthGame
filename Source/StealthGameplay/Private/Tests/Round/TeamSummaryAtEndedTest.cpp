// Story rmf-054 (production/epics/round-match-framework/story-054-team-summary-at-ended.md), GDD AC 12e / R9 (a):
// a 2v2 round won by team 0 at the final objective, team 0 with one member Informant and two WrongTarget results, team 1
// with 2 completed stages, both members Informant and no wrong targets: FRoundResult.TeamSummary = [{3, 1, 2}, {2, 2, 0}]
// ({StepsCompleted, EliminatedCount, WrongTargetCount}) in the first FRoundPublicState carrying Ended; every earlier write
// holds an all-zero TeamSummary; no summary field holds a time or a participant id (reflection: three uint8 counts).
// With a forfeit as the last removal (a grace-expiry conversion) the forfeiting member is counted in EliminatedCount.
//
// Fixture note: under F2 a team whose last Active member becomes an Informant is eliminated at once, so team 1's two
// informants are fixture Roster states set without transition events (the Roster's internals are another epic's); the
// counts Round reads in the latch step are what is under test. The WrongTarget rows are KillResolved entries appended
// through IRoundLogWriter (the Assassination stand-in), never a private log.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundEndRig.h"
#include "UObject/UnrealType.h"

namespace TeamSummaryAtEndedTestData
{
	void AppendKill(FRoundEndRig& R, uint32 Killer, uint32 KillerTeam, ERoundKillResult Result)
	{
		FRoundLogEntry Kill;
		Kill.Type = ERoundLogType::KillResolved;
		Kill.Code = static_cast<uint16>(Result);
		Kill.Participants[0] = Killer;
		Kill.Data[0] = KillerTeam;
		R.Rig.Core->GetRoundLogWriter()->Append(Kill);
	}

	bool Equals(const FRoundTeamSummary& S, int32 Steps, int32 Eliminated, int32 Wrong)
	{
		return S.StepsCompleted == Steps && S.EliminatedCount == Eliminated && S.WrongTargetCount == Wrong;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundTeamSummaryAtEndedTest, "StealthGame.Round.TeamSummaryAtEnded",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundTeamSummaryAtEndedTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;
	using namespace TeamSummaryAtEndedTestData;

	// Reflection: team aggregates only (no time, no participant id).
	for (TFieldIterator<FProperty> It(FRoundTeamSummary::StaticStruct()); It; ++It)
	{
		TestTrue(FString::Printf(TEXT("%s is a uint8 count"), *It->GetName()), It->IsA<FByteProperty>());
		TestFalse(FString::Printf(TEXT("%s names no time or id"), *It->GetName()), It->GetName().Contains(TEXT("Time")) || It->GetName().Contains(TEXT("Id")));
	}

	// --- The AC fixture ----------------------------------------------------------------------------------------------
	{
		FRoundEndRig R;
		R.ToActive(2, 2); // 1, 2 team 0; 3, 4 team 1
		R.ScriptRoundEnd();
		// Team 0: member 2 Informant (assassinated by 3), two WrongTarget results by team 0 killers; an NPC kill by team 1
		// that is not a wrong target. Team 1: 2 completed preliminary stages, both members Informant (fixture states).
		R.Transition(2, EParticipantState::Informant, ERosterReason::Assassinated);
		for (int32 Index = 0; Index < SummaryTeam0WrongTargets; ++Index)
		{
			AppendKill(R, 1, 0, ERoundKillResult::WrongTarget);
		}
		AppendKill(R, 3, 1, ERoundKillResult::Other);
		R.Rig.Fakes.Objectives.PreliminaryStages[1] = SummaryTeam1PreliminaryStages;
		R.Rig.Fakes.Objectives.PreliminaryStages[0] = 2;
		R.Rig.Fakes.Roster.SetState(FParticipantId(3), EParticipantState::Informant);
		R.Rig.Fakes.Roster.SetState(FParticipantId(4), EParticipantState::Informant);
		const int32 WritesBefore = R.PublicWrites.Num();

		// Act: team 0's final completion.
		R.DrainOneTick({ R.Completion(1, 1, 0) });

		// Assert.
		TestTrue(TEXT("Ended by the objective"), R.Rig.Core->GetPhase() == ERoundPhase::Ended);
		const FRoundPublicState* First = nullptr;
		bool bEarlierZero = true;
		for (const FRoundPublicState& Write : R.PublicWrites)
		{
			if (Write.Phase == ERoundPhase::Ended && First == nullptr)
			{
				First = &Write;
			}
			else if (First == nullptr)
			{
				bEarlierZero &= Write.LastResult.TeamSummary[0].IsZero() && Write.LastResult.TeamSummary[1].IsZero();
			}
		}
		TestTrue(TEXT("every earlier FRoundPublicState: all-zero TeamSummary"), bEarlierZero && WritesBefore >= 3);
		if (TestNotNull(TEXT("the first FRoundPublicState carrying Ended"), First))
		{
			TestTrue(TEXT("team 0 = {3, 1, 2}"), Equals(First->LastResult.TeamSummary[0], 3, 1, SummaryTeam0WrongTargets));
			TestTrue(TEXT("team 1 = {2, 2, 0}"), Equals(First->LastResult.TeamSummary[1], SummaryTeam1PreliminaryStages, 2, 0));
		}
		TestTrue(TEXT("no unexpected call"), R.Rig.Log.Unexpected.IsEmpty());
	}

	// --- A forfeit as the last removal ---------------------------------------------------------------------------------
	{
		FRoundEndRig R;
		R.ToActive(2, 2);
		R.ScriptRoundEnd();
		// 3 is assassinated; 4, team 1's last Active member, disconnects and converts at grace expiry (forfeit).
		R.Transition(3, EParticipantState::Informant, ERosterReason::Assassinated);
		R.Disconnect(4);
		const FRoundGraceRecord* Record = R.Rig.Core->GetGraceTable().Find(FParticipantId(4));
		const double Expiry = Record != nullptr ? Record->GraceExpiry : 0.0;
		R.Rig.Fakes.Roster.Script(TEXT("RequestTransition"), static_cast<int64>(ERosterResult::Applied));
		R.Rig.Fakes.Roster.Script(TEXT("ReleaseToCrowd"), static_cast<int64>(ERosterResult::Applied));

		// Act.
		R.RunUntil(Expiry);

		// Assert.
		TestTrue(TEXT("forfeit: Ended, ForfeitElimination"), R.Rig.Core->GetPhase() == ERoundPhase::Ended
			&& R.Rig.Core->GetLatch().GetOutcome().EndReason == ERoundEndReason::ForfeitElimination);
		const FRoundResult& Result = R.Rig.Core->GetPhaseCore().GetLastResult();
		TestEqual(TEXT("forfeit: the forfeiting member is counted in EliminatedCount"), static_cast<int32>(Result.TeamSummary[1].EliminatedCount), 2);
		TestEqual(TEXT("forfeit: no step for a non-objective win"), static_cast<int32>(Result.TeamSummary[0].StepsCompleted), 0);
		TestTrue(TEXT("forfeit: no unexpected call"), R.Rig.Log.Unexpected.IsEmpty());
	}
	return true;
}

#endif
