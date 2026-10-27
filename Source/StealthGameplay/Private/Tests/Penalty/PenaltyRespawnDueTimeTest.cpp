// Story nkp-009 (production/epics/npc-kill-penalty-informants/story-009-respawn-due-time-scheduling-and-cancellation.md),
// AC1 / AC-NKP-10 (F2; ADR-0019 section 3.2): GIVEN the F2 cases on a fake clock with the default knobs (D = 15,
// DeathFallSec 1.2, NoPawnFadeSec 0.5, StrikeContactSec 0.40), WHEN T_due is computed, THEN a kill slot at 100.0 gives
// 115.0, an exile at 100.0 gives 115.0, an informant killed at 100.0 that disconnects while awaiting at 103.0 and reclaims
// at 120.0 gives 120.0 (the F2 example), an embodied informant disconnected at 103.0 and reclaimed at 120.0 gives 120.0,
// disconnected at 103.0 and reclaimed at 110.0 gives 118.0, and a body lost without a kill at 130.0 gives 130.0; AND with
// D = 5 and every floor input at its hard maximum, the floor (<= 4.7 s after t_lost) is never later than t_lost + D, for
// the open strike (T_v = StrikeContactSec's hard maximum 0.8) and the bush variant (T_v = 1.2). Edge: a reclaim exactly at
// T_due goes to Picking. The literal seconds are the boundary values the criterion states.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyTestRig.h"

namespace PenaltyRespawnDueTimeTestData
{
	/** The F2 fixture (GDD Formulas header defaults; the hard maxima of the floor inputs). */
	inline constexpr double StrikeContactSec = 0.40;
	inline constexpr double OpenStrikeTvMaxSec = 0.80; // StrikeContactSec hard maximum
	inline constexpr double BushStrikeTvMaxSec = 1.20; // I-ASN-3 cap: StrikeContactSec + BushStrikeEmergeSec
	inline constexpr double DeathFallMaxSec = 2.0;
	inline constexpr double NoPawnFadeMaxSec = 1.5;
	inline constexpr double MinDelaySec = 5.0;

	/** A rig starting at 50.0, so an embodiment can precede the 103.0 disconnect. */
	inline constexpr double EarlyStart = 50.0;

	/** Makes P an embodied informant (an exile at 60.0, its respawn committed at 75.0 through the fake gateway). */
	void Embody(StealthPenaltyTest::FRig& Rig, FParticipantId P)
	{
		Rig.SetTime(60.0);
		Rig.Roster.States.Add(P, EParticipantState::Informant);
		Rig.Core->OnInformantAwaitingBody(P, ERespawnCause::StrikeLimit, 60.0);
		Rig.Fire(P);
		Rig.Roster.LivePawns.Add(P);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyRespawnDueTimeTest, "StealthGame.Penalty.RespawnDueTime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyRespawnDueTimeTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;
	using namespace PenaltyRespawnDueTimeTestData;

	// The defaults the cases assume (never literals in Penalty code: the schema defaults through the tuning fake).
	{
		FRig Rig;
		const FPenaltyParams& Params = Rig.Core->GetLatchedParams();
		TestEqual(TEXT("D = InformantRespawnDelaySec default 15"), Params.InformantRespawnDelaySec, 15.0);
		TestEqual(TEXT("DeathFallSec default 1.2"), Params.DeathFallSec, static_cast<double>(1.2f));
		TestEqual(TEXT("NoPawnFadeSec default 0.5 (Camera, pending owner)"), Params.NoPawnFadeSec, 0.5);
	}

	// Case 1: a kill slot at 100.0 (Assassinated; the body's death write at 100.0 + StrikeContactSec): floor 102.1, T_due 115.0.
	{
		FRig Rig;
		Rig.SetTime(100.0);
		Rig.KillPlayer(P, Q, 100.0 + StrikeContactSec);
		Rig.Core->OnInformantAwaitingBody(P, ERespawnCause::Assassinated, 100.0);
		const FPenaltyRecord* Record = Rig.Core->FindRecord(P);
		TestTrue(TEXT("kill slot: floor = DeathServerTime + DeathFallSec + NoPawnFadeSec (about 102.1)"), Record != nullptr
			&& FMath::IsNearlyEqual(Record->FloorServerTime, 100.0 + StrikeContactSec + static_cast<double>(1.2f) + 0.5, 1e-6));
		TestEqual(TEXT("kill slot at 100.0: T_due 115.0"), Rig.Core->GetDueServerTime(P).Get(0.0), 115.0);
	}

	// Case 2: an exile at 100.0 (StrikeLimit: the body does not fall, t_floor = t_lost): T_due 115.0.
	{
		FRig Rig;
		Rig.SetTime(100.0);
		Rig.Core->OnInformantAwaitingBody(P, ERespawnCause::StrikeLimit, 100.0);
		TestEqual(TEXT("exile at 100.0: T_due 115.0"), Rig.Core->GetDueServerTime(P).Get(0.0), 115.0);
		TestEqual(TEXT("exile: t_floor = t_lost"), Rig.Core->FindRecord(P)->FloorServerTime, 100.0);
	}

	// Case 3 (the F2 example): an informant killed at 100.0, disconnected while awaiting at 103.0, reclaimed at 120.0.
	{
		FRig Rig;
		Rig.Roster.States.Add(P, EParticipantState::Informant);
		Rig.SetTime(100.0);
		Rig.KillInformant(P, Q, 901, 100.0 + StrikeContactSec);
		TestEqual(TEXT("informant killed at 100.0: T_due 115.0 while awaiting"), Rig.Core->GetDueServerTime(P).Get(0.0), 115.0);
		Rig.SetTime(103.0);
		Rig.Roster.Disconnected.Add(P);
		Rig.Core->OnConnectionChanged(P, false);
		Rig.SetTime(120.0);
		Rig.Roster.Disconnected.Remove(P);
		Rig.Core->OnInformantReadmitted(P);
		TestEqual(TEXT("disconnected at 103.0, reclaimed at 120.0: T_due 120.0"), Rig.Core->GetDueServerTime(P).Get(0.0), 120.0);
		TestTrue(TEXT("the reclaim after T_due starts Picking at once"), Rig.Core->GetLifeState(P) == EInformantLifeState::Picking);
	}

	// Cases 4 and 5: an embodied informant disconnected at 103.0 and reclaimed at 120.0 / at 110.0.
	for (const double Reclaim : { 120.0, 110.0 })
	{
		FRig Rig(nullptr, false, /*bWithRespawnGateway*/ true, EarlyStart);
		Embody(Rig, P);
		TestTrue(TEXT("fixture: P is embodied"), Rig.Core->GetLifeState(P) == EInformantLifeState::Embodied);
		Rig.SetTime(103.0);
		Rig.Roster.Disconnected.Add(P);
		Rig.Core->OnConnectionChanged(P, false);
		Rig.Roster.LivePawns.Remove(P); // Round R17.5 released the pawn to the crowd in that slot
		Rig.SetTime(Reclaim);
		Rig.Roster.Disconnected.Remove(P);
		Rig.Gateway.Picks.Add(FPenaltyRespawnPick()); // a pick at the reclaim answers None, so T_due stays observable
		++Rig.Gateway.Frame;
		Rig.Core->OnInformantReadmitted(P);
		const double Expected = Reclaim == 120.0 ? 120.0 : 118.0;
		TestEqual(FString::Printf(TEXT("embodied, disconnected at 103.0, reclaimed at %.1f: T_due %.1f"), Reclaim, Expected), Rig.Core->GetDueServerTime(P).Get(0.0), Expected);
	}

	// Case 6: a body lost without a kill at 130.0 (D = 0): T_due 130.0.
	{
		FRig Rig(nullptr, false, true, EarlyStart);
		Embody(Rig, P);
		Rig.SetTime(130.0);
		Rig.Roster.LivePawns.Remove(P);
		Rig.Gateway.Picks.Add(FPenaltyRespawnPick()); // None: the request stays Picking with its T_due
		++Rig.Gateway.Frame;
		Rig.Core->OnInformantPawnLost(P, EPenaltyPawnLostCause::KillZ);
		TestEqual(TEXT("body lost without a kill at 130.0: T_due 130.0"), Rig.Core->GetDueServerTime(P).Get(0.0), 130.0);
	}

	// The F2 formula itself, case by case (the same six values).
	TestEqual(TEXT("F2 kill"), FPenaltyCore::ComputeDueTime(100.0, 15.0, 102.1, 100.0), 115.0);
	TestEqual(TEXT("F2 exile"), FPenaltyCore::ComputeDueTime(100.0, 15.0, 100.0, 100.0), 115.0);
	TestEqual(TEXT("F2 example"), FPenaltyCore::ComputeDueTime(100.0, 15.0, 102.1, 120.0), 120.0);
	TestEqual(TEXT("F2 embodied, reclaim 120"), FPenaltyCore::ComputeDueTime(103.0, 15.0, 103.0, 120.0), 120.0);
	TestEqual(TEXT("F2 embodied, reclaim 110"), FPenaltyCore::ComputeDueTime(103.0, 15.0, 103.0, 110.0), 118.0);
	TestEqual(TEXT("F2 lost body"), FPenaltyCore::ComputeDueTime(130.0, 0.0, 130.0, 130.0), 130.0);

	// The floor at every hard maximum never decides for D >= 5 (open strike 4.3 s, bush 4.7 s).
	for (const double Tv : { OpenStrikeTvMaxSec, BushStrikeTvMaxSec })
	{
		const double Lost = 100.0;
		const double Floor = FPenaltyCore::ComputeFloor(Lost + Tv, DeathFallMaxSec, NoPawnFadeMaxSec, Lost);
		TestTrue(FString::Printf(TEXT("T_v %.1f: the floor is at most 4.7 s after t_lost"), Tv), Floor - Lost <= 4.7 + 1e-9);
		TestTrue(FString::Printf(TEXT("T_v %.1f: the floor is never later than t_lost + D (D = 5)"), Tv), Floor <= Lost + MinDelaySec);

		// The same through the scheduler with the knobs injected at those values.
		FRig Rig([](FFakeTuning& Tuning)
		{
			Tuning.Live.InformantRespawnDelaySec = MinDelaySec;
			Tuning.Live.DeathFallSec = DeathFallMaxSec;
			Tuning.Live.NoPawnFadeSec = NoPawnFadeMaxSec;
		});
		Rig.SetTime(Lost);
		Rig.KillPlayer(P, Q, Lost + Tv);
		Rig.Core->OnInformantAwaitingBody(P, ERespawnCause::Assassinated, Lost);
		TestEqual(FString::Printf(TEXT("T_v %.1f, D = 5 at the hard maxima: T_due = t_lost + D"), Tv), Rig.Core->GetDueServerTime(P).Get(0.0), Lost + MinDelaySec);
	}

	// Edge: a reclaim exactly at T_due goes to Picking (now < T_due is false).
	{
		FRig Rig;
		Rig.Roster.States.Add(P, EParticipantState::Informant);
		Rig.Roster.Disconnected.Add(P);
		Rig.SetTime(100.0);
		Rig.Core->OnInformantAwaitingBody(P, ERespawnCause::StrikeLimit, 100.0); // Absent (disconnected at the cause)
		Rig.SetTime(115.0);
		Rig.Roster.Disconnected.Remove(P);
		Rig.Core->OnInformantReadmitted(P);
		TestTrue(TEXT("a reclaim exactly at T_due starts Picking"), Rig.Core->GetLifeState(P) == EInformantLifeState::Picking);
		TestEqual(TEXT("... with T_due 115.0"), Rig.Core->GetDueServerTime(P).Get(0.0), 115.0);
	}
	return true;
}

#endif
