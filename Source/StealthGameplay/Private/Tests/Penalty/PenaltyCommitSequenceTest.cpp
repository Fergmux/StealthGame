// Story nkp-012 (production/epics/npc-kill-penalty-informants/story-012-respawn-commit-sequence.md), AC1 / AC-NKP-30
// (PEN-R9; ADR-0019 section 4.1): GIVEN fake gateways where TryReleaseNpc fails once and TakeOverCrowdPawn fails once,
// WHEN a respawn commits, THEN per attempt the order is pick, pre-check, release, takeover, card; there are three picks in
// three different frames (attempt 1 fails the release, attempt 2 the takeover, attempt 3 succeeds); the takeover failure
// sends exactly one defect to the fault sink (the pre-check exists, so NotEligible there is a defect) and the pawn goes back
// through AdoptReleasedPawn with no card move; one card move follows attempt 3's Applied; every attempt reuses the same
// RequestSeq; each failed attempt's retry is due at ServerNow + the next-frame offset. (The GDD's "two picks" is three:
// one release failure plus one takeover failure force three; reported to the GDD owner in the story.) Edge: a pick that
// answers Deferred costs a frame but not an attempt. Penalty never calls BindPawn itself: the takeover is the only binder.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyTestRig.h"

namespace PenaltyCommitSequenceTestData
{
	inline constexpr uint64 PawnA = 700;
	inline constexpr uint64 PawnB = 701;
	inline constexpr uint64 PawnC = 702;

	FPenaltyRespawnPick Picked(uint64 Pawn)
	{
		FPenaltyRespawnPick Pick;
		Pick.Status = EPenaltyPickStatus::Picked;
		Pick.Pawn = FPenaltyPawnKey(Pawn);
		return Pick;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyCommitSequenceTest, "StealthGame.Penalty.CommitSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyCommitSequenceTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;
	using namespace PenaltyCommitSequenceTestData;

	{
		// Arrange: P awaiting (an exile at 100.0); the release fails once, then the takeover fails once.
		FRig Rig(nullptr, false, /*bWithRespawnGateway*/ true);
		Rig.Roster.States.Add(P, EParticipantState::Informant);
		Rig.Roster.Teams.Add(P, ERosterTeam::Team0);
		Rig.Core->OnInformantAwaitingBody(P, ERespawnCause::StrikeLimit, Rig.Clock.GetServerTimeSeconds());
		Rig.Gateway.Picks = { Picked(PawnA), Picked(PawnB), Picked(PawnC) };
		Rig.Gateway.Releases = { false, true, true };
		Rig.Gateway.Takeovers = { EPenaltyTakeoverResult::NotEligible, EPenaltyTakeoverResult::Applied };

		// Act: the item fires, then each next-frame retry.
		TestTrue(TEXT("attempt 1 resolves"), Rig.Fire(P));
		const TArray<FRecordingRespawns::FCall> Retry1 = Rig.Respawns.PendingFor(P);
		const double FailTime1 = Rig.Clock.GetServerTimeSeconds();
		TestTrue(TEXT("attempt 2 resolves"), Rig.Fire(P));
		const TArray<FRecordingRespawns::FCall> Retry2 = Rig.Respawns.PendingFor(P);
		const double FailTime2 = Rig.Clock.GetServerTimeSeconds();
		TestTrue(TEXT("attempt 3 resolves"), Rig.Fire(P));

		// Assert: the order per attempt.
		TestEqual(TEXT("call order: pick, pre-check, release | pick, pre-check, release, takeover, hand-back | pick, pre-check, release, takeover, card"),
			Rig.Gateway.Sequence(), FString(TEXT("Pick PreCheck Release Pick PreCheck Release Takeover Adopt Pick PreCheck Release Takeover Card RecordAppearance")));
		const TArray<FFakeRespawnGateway::FCall> Picks = Rig.Gateway.Named(TEXT("Pick"));
		TestTrue(TEXT("three picks in three different frames"), Picks.Num() == 3 && Picks[0].Frame != Picks[1].Frame && Picks[1].Frame != Picks[2].Frame && Picks[0].Frame != Picks[2].Frame);
		TestTrue(TEXT("every attempt reuses the same RequestSeq"), Picks.Num() == 3 && Picks[0].RequestSeq != 0 && Picks[0].RequestSeq == Picks[1].RequestSeq && Picks[1].RequestSeq == Picks[2].RequestSeq);
		TestTrue(TEXT("attempt 1's retry: ServerNow + the next-frame offset, the same RequestSeq"), Retry1.Num() == 1
			&& FMath::IsNearlyEqual(Retry1[0].DueServerTime, FailTime1 + FPenaltyCore::NextFrameRetryOffsetSec(), 1e-9) && Retry1[0].RequestSeq == Picks[0].RequestSeq);
		TestTrue(TEXT("attempt 2's retry: ServerNow + the next-frame offset"), Retry2.Num() == 1
			&& FMath::IsNearlyEqual(Retry2[0].DueServerTime, FailTime2 + FPenaltyCore::NextFrameRetryOffsetSec(), 1e-9));
		TestTrue(TEXT("each failed pawn is excluded from the next pick"), Picks.Num() == 3 && Picks[1].Exclude.Contains(FPenaltyPawnKey(PawnA))
			&& Picks[2].Exclude.Contains(FPenaltyPawnKey(PawnA)) && Picks[2].Exclude.Contains(FPenaltyPawnKey(PawnB)));
		const TArray<FFakeRespawnGateway::FCall> Adopts = Rig.Gateway.Named(TEXT("Adopt"));
		TestTrue(TEXT("the takeover failure hands the released pawn back through AdoptReleasedPawn"), Adopts.Num() == 1 && Adopts[0].Pawn == FPenaltyPawnKey(PawnB));
		TestEqual(TEXT("exactly one defect reaches the fault sink (NotEligible after the pre-check)"),
			Rig.Faults.GetRecords().FilterByPredicate([](const FStealthFaultRecord& R) { return R.Detail == PenaltyFaultDetail::TakeoverNotEligibleAfterPrecheck; }).Num(), 1);
		TestEqual(TEXT("no other fault"), Rig.Faults.GetRecords().Num(), 1);
		const TArray<FFakeRespawnGateway::FCall> Cards = Rig.Gateway.Named(TEXT("Card"));
		TestTrue(TEXT("one card move, after attempt 3's Applied, for its pawn"), Cards.Num() == 1 && Cards[0].Pawn == FPenaltyPawnKey(PawnC) && Cards[0].Participant == P);
		TestTrue(TEXT("Embodied, T_due cleared, ExcludePawn cleared"), Rig.Core->GetLifeState(P) == EInformantLifeState::Embodied && !Rig.Core->GetDueServerTime(P).IsSet()
			&& !Rig.Core->FindRecord(P)->ExcludePawn.IsSet() && Rig.Core->FindRecord(P)->RequestExclusions.Num() == 0);
		const TArray<FPenaltyInformantRespawnRow> Rows = Rig.Rows<FPenaltyInformantRespawnRow>();
		TestTrue(TEXT("one InformantRespawn row, PickLatencySec = now - T_due"), Rows.Num() == 1 && Rows[0].Participant == P && Rows[0].PickLatencySec > 0.0);
	}

	// Edge: Deferred costs a frame but not an attempt.
	{
		FRig Rig(nullptr, false, true);
		Rig.Roster.States.Add(P, EParticipantState::Informant);
		Rig.Core->OnInformantAwaitingBody(P, ERespawnCause::StrikeLimit, Rig.Clock.GetServerTimeSeconds());
		FPenaltyRespawnPick Deferred;
		Deferred.Status = EPenaltyPickStatus::Deferred;
		Rig.Gateway.Picks = { Deferred, Picked(PawnA) };
		Rig.Fire(P);
		Rig.Fire(P);
		TestEqual(TEXT("Deferred: pick, then the next frame's full attempt"), Rig.Gateway.Sequence(), FString(TEXT("Pick Pick PreCheck Release Takeover Card RecordAppearance")));
		TestTrue(TEXT("Deferred: no pawn excluded, no hand-back"), Rig.Gateway.Count(TEXT("Adopt")) == 0 && Rig.Gateway.Named(TEXT("Pick"))[1].Exclude.Num() == 0);
		TestTrue(TEXT("Deferred: Embodied after the second frame"), Rig.Core->GetLifeState(P) == EInformantLifeState::Embodied);
	}

	// The exile's own released body is excluded from every pick of the request (LSC R5b.1, PEN-R5).
	{
		FRig Rig(nullptr, /*bWithGateways*/ true, true);
		const FPenaltyPawnKey OwnBody(555);
		// The exile through the core (the L-th ungraced NPC kill with P's body) records ExcludePawn = that body.
		for (uint64 Npc = 900; Npc < 900 + static_cast<uint64>(DefaultLimit()); ++Npc)
		{
			Rig.NpcKillWithBody(P, OwnBody, Npc);
		}
		TestTrue(TEXT("fixture: P exiled with its body excluded"), Rig.Core->FindRecord(P) != nullptr && Rig.Core->FindRecord(P)->ExcludePawn == OwnBody);
		Rig.Core->OnInformantAwaitingBody(P, ERespawnCause::StrikeLimit, Rig.Clock.GetServerTimeSeconds());
		Rig.Fire(P);
		const TArray<FFakeRespawnGateway::FCall> Picks = Rig.Gateway.Named(TEXT("Pick"));
		TestTrue(TEXT("the exile body is in Exclude"), Picks.Num() == 1 && Picks[0].Exclude.Contains(OwnBody));
		TestFalse(TEXT("ExcludePawn cleared at embodiment"), Rig.Core->FindRecord(P)->ExcludePawn.IsSet());
	}
	return true;
}

#endif
