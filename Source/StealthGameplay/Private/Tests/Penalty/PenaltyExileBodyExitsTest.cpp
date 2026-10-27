// Story nkp-006 (production/epics/npc-kill-penalty-informants/story-006-exile-releases-body-to-crowd.md), AC2 /
// AC-NKP-24 (PEN-R5; Crowd CR-4): GIVEN an exile whose AdoptReleasedPawn carries RestrictedUntilServerTime = the strike
// lockout end and bExitAtNextScheduledSlot, WHEN the released body's lockout ends, THEN it joins the full catalogue no
// earlier than the lockout end, its next leg is an AmbientExit started by the ambient schedule's next exit slot (it reports
// the AlleyExit eligibility while it waits and walks) and it is absorbed; AND Penalty records the body as the participant's
// ExcludePawn (the backstop PickRespawnPawn takes). Headless: Penalty drives the real crowd agent and ambient schedule in
// the crowd simulation (PenaltyCrowdSimRig.h). Crowd's give-up rule while watched (CrowdExitGiveUpSec) is the agent's own
// exit path, unchanged for an exile body (StealthGame.Crowd.Ambient.EmergenceNeverWitnessed covers it).
// Deferred, recorded in the story: the PickRespawnPawn half (LSC's pick with ExcludePawn, eligibility stubbed true) is
// story nkp-012's (LSC's PickRespawnPawn is not built); "ExcludePawn clears at embodiment" is nkp-012's commit.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyCrowdSimRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyExileBodyExitsTest, "StealthGame.Penalty.ExileBodyExits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyExileBodyExitsTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyCrowdSim;

	// Arrange: the crowd running; a participant's body walking under the restricted driver.
	FRig Rig;
	FNpc& Body = Rig.SpawnPlayerBody(FVector(-600.0, 300.0, 0.0), 30.0f);
	const uint32 Ordinal = Body.Body.Ordinal;
	Rig.World->RunUntil(Rig.World->GetNow() + 4.0);

	// Act: the exile, then the simulation runs until the body is absorbed (bounded).
	const double ExileAt = Rig.World->GetNow();
	const FParticipantId Participant = Rig.ExileNow(Body);
	const double LockoutEnd = ExileAt + StealthPenaltyTest::FRig::LockoutSec();
	bool bWaitedOrExiting = false;
	bool bExitingBeforeLockout = false;
	bool bWasExiting = false;
	bool bAbandonedWhileWatched = false;
	const double Limit = Rig.World->GetNow() + 300.0;
	while (Rig.World->GetNow() < Limit && !Rig.World->FindNpc(Ordinal)->Body.bGone)
	{
		Rig.World->Step();
		const FCrowdAgent& Agent = Rig.World->FindNpc(Ordinal)->Agent;
		const bool bExitPhase = Agent.IsAwaitingExitSlot() || Agent.IsExiting();
		bWaitedOrExiting |= bExitPhase;
		bExitingBeforeLockout |= Agent.IsExiting() && Rig.World->GetNow() < LockoutEnd;
		// Crowd's give-up rule: a watched exit is abandoned (the walk ends without an absorption) like any citizen's.
		bAbandonedWhileWatched |= bWasExiting && !Agent.IsExiting() && !Rig.World->FindNpc(Ordinal)->Body.bGone;
		bWasExiting = Agent.IsExiting();
	}
	AddInfo(FString::Printf(TEXT("EXILEBODY absorbed %d, exit abandoned under the give-up rule %d, exits given up city-wide %d"),
		Rig.World->FindNpc(Ordinal)->Body.bGone ? 1 : 0, bAbandonedWhileWatched ? 1 : 0, Rig.World->GiveUpAfterSec.Num()));

	// Assert
	TestTrue(TEXT("the crowd adoption carried the lockout end"), Rig.Bodies.Adopted.Num() == 1 && Rig.Bodies.Adopted[0] == Ordinal
		&& FMath::IsNearlyEqual(Rig.Bodies.RestrictedUntil[0], LockoutEnd, 1e-9));
	const TPair<uint32, double>* Promotion = Rig.World->Promoted.FindByPredicate([Ordinal](const TPair<uint32, double>& P) { return P.Key == Ordinal; });
	TestTrue(TEXT("the full catalogue only after the lockout end"), Promotion != nullptr && Promotion->Value >= LockoutEnd - 1e-9);
	TestFalse(TEXT("no exit leg before the lockout end"), bExitingBeforeLockout);
	TestTrue(TEXT("the next leg is the AmbientExit (AlleyExit-eligible while waiting and walking)"), bWaitedOrExiting);
	TestTrue(TEXT("the exit was started by a scheduled exit slot"), Rig.World->ExitStartedAt.Contains(Ordinal));
	TestTrue(TEXT("the exit ends as any citizen's: absorbed, or abandoned while watched (CrowdExitGiveUpSec)"),
		Rig.World->FindNpc(Ordinal)->Body.bGone || bAbandonedWhileWatched);
	const FPenaltyRecord* Record = Rig.Penalty->FindRecord(Participant);
	TestTrue(TEXT("ExcludePawn = the released body"), Record != nullptr && Record->ExcludePawn == KeyOfOrdinal(Ordinal));
	TestEqual(TEXT("no Penalty fault"), Rig.Faults.GetRecords().Num(), 0);
	return true;
}

#endif
