// Story oc-009 (production/epics/objectives-chain/story-009-final-completion-and-win-declaration.md), AC4 (GDD Edge Cases)
// FinalSameFrameOrdering, on the real queue with a strike stub (an Attack whose slot removes its victim from the Roster):
//   (a) Complete before a strike on the tender in one tick: the due HoldTimer is stamped first (lower key), the final counts and
//       latches ObjectiveWin; the strike returns Rejected_Latched (owner reply NotNow);
//   (b) a strike first (an earlier frame): the tender is removed, so the hold cancels (Illegal) at its slot: no completion, no win;
//   (c) both teams' final holds due in one frame: ordered by (DueTime, Begin key); the first latches ObjectiveWin, the second
//       returns Rejected_Latched and is logged TendCompleted(NoEffect, Latched);
//   (d) the completer killed later in the same drain (and in the next frame): the kill returns Rejected_Latched; the win stands.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesQueueRig.h"

namespace ObjectivesFinalSameFrameOrderingTestData
{
	using namespace StealthObjectivesTest;

	constexpr double D = 3.0;

	/** The strike stub: the Attack's victim (its PawnRef target) leaves Active (Assassinated). */
	void ArmStrikes(FQueueRig& Rig)
	{
		Rig.Resolver.OnStrike = [&Rig](const FCommandPayload& Cmd, FQueueKey)
		{
			const APawn* Victim = Cmd.Target.Kind == ESubjectRefKind::Pawn ? Cmd.Target.Pawn.Pawn.Get() : nullptr;
			const FParticipantId* Id = Victim != nullptr ? Rig.Roster.Bindings.Find(Victim) : nullptr;
			if (Id == nullptr)
			{
				return EResolveResult::Voided;
			}
			Rig.Roster.States[Id->Value] = EParticipantState::Informant;
			return EResolveResult::Executed;
		};
	}

	/** Both teams' finals Live. */
	void UnlockFinals(FQueueRig& Rig)
	{
		Rig.CompleteStageFast(0);
		Rig.CompleteStageFast(0);
		Rig.CompleteStageFast(1);
		Rig.CompleteStageFast(1);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesFinalSameFrameOrderingTest, "StealthGame.Objectives.FinalSameFrameOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesFinalSameFrameOrderingTest::RunTest(const FString& Parameters)
{
	using namespace ObjectivesFinalSameFrameOrderingTestData;

	// (a) Complete, then a strike on the tender in the same tick; (d) a kill later in the same drain and the next frame.
	{
		FQueueRig Rig;
		TestTrue(TEXT("(a) Active"), Rig.ToActive());
		ArmStrikes(Rig);
		UnlockFinals(Rig);
		AStealthPawn* Tender = Rig.Pawn(CourtFinal, 0, 1, 0);
		AStealthPawn* Attacker = Rig.Pawn(CourtFinal, 2, 11, 1);
		Rig.Begin(Tender, CourtFinal);
		Rig.Q.Tick(0.1);
		const FInteractHoldStandIn::FHold* Hold = Rig.Holds->FindHold(Tender);
		if (!TestNotNull(TEXT("(a) the final hold runs"), Hold))
		{
			return false;
		}
		Rig.RunToFrameBefore(Hold->DueServerTime);
		TestEqual(TEXT("(a) not yet due"), Rig.Round.Wins.Num(), 0);
		Rig.Strike(Attacker, Tender); // submitted in the frame the hold comes due
		Rig.Strike(Attacker, Tender); // (d) a second kill attempt later in the same drain
		Rig.Q.Tick(0.1);
		TestEqual(TEXT("(a) the completion counted and latched ObjectiveWin"), Rig.Round.Wins.Num(), 1);
		TestTrue(TEXT("(a) for team 0"), Rig.Round.Wins.Num() == 1 && Rig.Round.Wins[0].Key == 0);
		TestEqual(TEXT("(a, d) both strikes returned Rejected_Latched"), Rig.Resolver.StrikesLatched, 2);
		TestTrue(TEXT("(a) the tender is still Active (the strike changed nothing)"), Rig.Roster.GetState(FParticipantId(1)) == EParticipantState::Active);
		Rig.Strike(Attacker, Tender); // (d) the next frame
		Rig.Q.Tick(0.1);
		TestEqual(TEXT("(d) a kill in the next frame is Rejected_Latched too"), Rig.Resolver.StrikesLatched, 3);
		TestEqual(TEXT("(d) the win is not reversed (still one)"), Rig.Round.Wins.Num(), 1);
	}

	// (b) A strike first: the hold cancels, no completion.
	{
		FQueueRig Rig;
		TestTrue(TEXT("(b) Active"), Rig.ToActive());
		ArmStrikes(Rig);
		UnlockFinals(Rig);
		AStealthPawn* Tender = Rig.Pawn(CourtFinal, 0, 1, 0);
		AStealthPawn* Attacker = Rig.Pawn(CourtFinal, 2, 11, 1);
		int32 Cancelled = 0;
		int32 Completed = 0;
		Rig.Hub->OnActionCancelled.AddLambda([&Cancelled](AStealthPawn*, EActionType, ECancelReason Reason) { Cancelled += Reason == ECancelReason::Illegal ? 1 : 0; });
		Rig.Hub->OnActionCompleted.AddLambda([&Completed](AStealthPawn*, EActionType) { ++Completed; });
		Rig.Begin(Tender, CourtFinal);
		Rig.Q.Tick(0.1);
		Rig.Run(1.0);
		Rig.Strike(Attacker, Tender);
		Rig.Run(D);
		TestEqual(TEXT("(b) the strike resolved"), Rig.Resolver.StrikeKeys.Num(), 1);
		TestEqual(TEXT("(b) no win"), Rig.Round.Wins.Num(), 0);
		TestEqual(TEXT("(b) the hold cancelled (Illegal)"), Cancelled, 1);
		TestEqual(TEXT("(b) no completion"), Completed, 0);
		TestEqual(TEXT("(b) no FinalCompleted"), Rig.Logged(ERoundLogType::FinalCompleted), 0);
		Rig.Hub->OnActionCancelled.Clear();
		Rig.Hub->OnActionCompleted.Clear();
	}

	// (c) Both teams' final holds due in one frame.
	{
		FQueueRig Rig;
		TestTrue(TEXT("(c) Active"), Rig.ToActive());
		UnlockFinals(Rig);
		AStealthPawn* Team0 = Rig.Pawn(CourtFinal, 0, 1, 0);
		AStealthPawn* Team1 = Rig.Pawn(CourtFinal, 1, 11, 1);
		Rig.Begin(Team0, CourtFinal);
		Rig.Begin(Team1, CourtFinal);
		Rig.Run(D + 0.2);
		TestEqual(TEXT("(c) one win only"), Rig.Round.Wins.Num(), 1);
		const FInteractHoldStandIn::FHold* Leftover0 = Rig.Holds->FindHold(Team0);
		const FInteractHoldStandIn::FHold* Leftover1 = Rig.Holds->FindHold(Team1);
		TestTrue(TEXT("(c) both holds ended (no tie, no leftover)"), Leftover0 == nullptr && Leftover1 == nullptr);
		const TArray<FRoundLogEntry> Tends = Rig.LoggedOf(ERoundLogType::TendCompleted);
		if (TestEqual(TEXT("(c) both attempts are logged as TendCompleted"), Tends.Num(), 2))
		{
			const FRoundLogEntry& Winner = Tends[0];
			const FRoundLogEntry& Loser = Tends[1];
			TestTrue(TEXT("(c) the first (lower Begin key) is Counted"), Winner.Code == static_cast<uint16>(ERoundTendResult::Counted) && (Winner.Flags & RoundLogFlags::Latched) == 0);
			TestTrue(TEXT("(c) the second is NoEffect with Latched"), Loser.Code == static_cast<uint16>(ERoundTendResult::NoEffect) && (Loser.Flags & RoundLogFlags::Latched) != 0);
			TestTrue(TEXT("(c) the winner is the team of the lower Begin key"), Rig.Round.Wins.Num() == 1
				&& static_cast<uint32>(Rig.Round.Wins[0].Key) == (Winner.Participants[0] == 1 ? 0u : 1u));
		}
		TestEqual(TEXT("(c) one FinalCompleted"), Rig.Logged(ERoundLogType::FinalCompleted), 1);
		TestTrue(TEXT("(c) the queue logged Rejected_Latched for the second"), Rig.Logged(ERoundLogType::Rejected_Latched) >= 1);
	}
	return true;
}

#endif
