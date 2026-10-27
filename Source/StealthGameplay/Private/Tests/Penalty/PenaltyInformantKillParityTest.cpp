// Story nkp-008 (production/epics/npc-kill-penalty-informants/story-008-informant-kill-inline-retire.md), AC2 /
// AC-NKP-03, server-side half (PEN-R14; extends AC-PSR-52): two runs from the same fixture on the real round queue, strike
// resolver and Round core with Penalty wired (PenaltyEncounterRig.h): an attacker with tally 0 and k + 1 < L kills an
// enemy informant (run 1) and an NPC at the same spot (run 2). Everything the attacker's and its ally's connections are
// sent about the kill compares equal field by field: the attacker's FStrikeResult reply (Result WrongTarget, TallyAfter 1,
// bExiled false, the same ContactServerTime and seq), the attacker's FActionState writes (Identical), the victim's
// FDeathState (Identical), and the ally receives nothing in either run. Penalty's additions (the inline retire and the
// respawn call, run 1 only) are server-only and change none of these bytes.
// Deferred, recorded in the story: the PIE-3 capture (StealthGame.Penalty.PIE.InformantKillParity: every RPC payload and
// replicated property on the attacker's and allies' connections, HUD widget state and cue ids, 3v3) needs the HUD epic's
// widget state and cue selector and the multi-process capture of spike S1/S14's open conditions.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyEncounterRig.h"

namespace PenaltyInformantKillParityTestData
{
	using namespace StealthPenaltyEncounterTest;

	/** The attacker's and the ally's reply sinks (non-null, so the rig records their replies). */
	inline constexpr uint16 AttackerSink = 1;
	inline constexpr uint16 AllySink = 2;
	inline constexpr uint32 AttackSeq = 41;

	struct FCapture
	{
		TArray<FCommandReply> AttackerReplies;
		TArray<FCommandReply> AllyReplies;
		TArray<FActionState> AttackerStates;
		FDeathState VictimDeath;
		bool bDeathWritten = false;
		int32 Retires = 0;
		int32 Respawns = 0;
		int32 Faults = 0;
	};

	FCapture Run(bool bInformant)
	{
		FPenaltyEncounterRig Rig(bInformant ? TEXT("PenaltyParityInformant") : TEXT("PenaltyParityNpc"), 0, /*bProductionWrites*/ true);
		Rig.R.SetInformant(4);
		APawn* Attacker = Rig.R.Spawn(FVector::ZeroVector, 1);
		Rig.R.Spawn(FVector(0.0, 600.0, 0.0), 2); // the ally
		APawn* Victim = Rig.R.Spawn(FVector(150.0, 0.0, 0.0), bInformant ? 4u : 0u);

		const FQueueItem Base = Rig.R.AttackAt(Attacker, Victim);
		FCommandPayload Payload = *Base.Payload.TryGet<FCommandPayload>();
		Payload.Sink.Index = AttackerSink;
		Payload.ClientSeq.Seq = AttackSeq;
		Rig.R.Drain({ FQueueItem::Make(EQueueItemType::PawnCommand, Rig.R.W.Support.Epoch.Current, FMailboxId::ServerLocal(), Payload) });
		for (int32 Frame = 0; Frame < 90; ++Frame)
		{
			Rig.R.Tick(); // the death write lands at ContactServerTime - StrikeNetLeadSec
		}

		FCapture Out;
		Out.AttackerReplies = Rig.R.Replies.For(AttackerSink);
		Out.AllyReplies = Rig.R.Replies.For(AllySink);
		for (const FRecordingPawnState::FWrite& Write : Rig.R.PawnState.Writes)
		{
			if (!Write.bDeath && Write.Pawn == Attacker)
			{
				Out.AttackerStates.Add(Write.State);
			}
		}
		if (const FDeathState* Death = Rig.R.PawnState.Deaths.Find(Victim))
		{
			Out.VictimDeath = *Death;
			Out.bDeathWritten = true;
		}
		Out.Retires = Rig.Bodies.Count(TEXT("RetireBinding"));
		Out.Respawns = Rig.Respawns.Calls.Num();
		Out.Faults = Rig.PenaltyFaults.GetRecords().Num();
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyInformantKillParityTest, "StealthGame.Penalty.InformantKillParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyInformantKillParityTest::RunTest(const FString& Parameters)
{
	using namespace PenaltyInformantKillParityTestData;

	// Arrange + Act: the two runs.
	const FCapture Informant = Run(true);
	const FCapture Npc = Run(false);

	// Assert: Penalty did its server-only work in the informant run only.
	TestTrue(TEXT("informant run: the binding retired once and the respawn scheduled once (server-only)"), Informant.Retires == 1 && Informant.Respawns == 1);
	TestTrue(TEXT("NPC run: neither"), Npc.Retires == 0 && Npc.Respawns == 0);

	// The attacker's owner result.
	if (TestTrue(TEXT("one StrikeResult reply to the attacker in each run"), Informant.AttackerReplies.Num() == 1 && Npc.AttackerReplies.Num() == 1))
	{
		const FCommandReply& A = Informant.AttackerReplies[0];
		const FCommandReply& B = Npc.AttackerReplies[0];
		TestTrue(TEXT("the replies are the same kind and seq"), A.Kind == B.Kind && A.Kind == ECommandReplyKind::StrikeResult && A.Seq == B.Seq);
		TestTrue(TEXT("the FStrikeResults are equal field by field"), A.Strike == B.Strike);
		TestTrue(TEXT("WrongTarget, TallyAfter 1, not exiled"), A.Strike.Result == EStrikeResult::WrongTarget && A.Strike.TallyAfter == 1 && !A.Strike.bExiled);
	}
	// The attacker's replicated pose and the victim's death.
	bool bStatesIdentical = Informant.AttackerStates.Num() == Npc.AttackerStates.Num() && Informant.AttackerStates.Num() > 0;
	for (int32 Index = 0; bStatesIdentical && Index < Informant.AttackerStates.Num(); ++Index)
	{
		bStatesIdentical = Informant.AttackerStates[Index].Identical(&Npc.AttackerStates[Index], 0);
	}
	TestTrue(TEXT("the attacker's FActionState writes are identical"), bStatesIdentical);
	TestTrue(TEXT("the victim's FDeathState is identical"), Informant.bDeathWritten && Npc.bDeathWritten && Informant.VictimDeath.Identical(&Npc.VictimDeath, 0));
	TestTrue(TEXT("the ally receives nothing in either run"), Informant.AllyReplies.Num() == 0 && Npc.AllyReplies.Num() == 0);
	TestTrue(TEXT("no Penalty fault"), Informant.Faults == 0 && Npc.Faults == 0);
	return true;
}

#endif
