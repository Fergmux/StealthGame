// Story rmf-043 (production/epics/round-match-framework/story-043-no-draw-property.md), GDD AC 17 / R15, R16, F2:
//   - 100,000 random interleavings (fixed seed, test-local PCG32 stream) over the alphabet {FinalCompletion(t),
//     DecisionElimination(t), GraceExpiry(p), Kick(p), Rejoin(p), PawnLost(p), Abort} run through the real F2 predicates
//     (StealthRoundWin) and the real single-assignment latch (FRoundTerminalLatch) with strictly increasing keys;
//   - at most one terminal outcome per round (every later TryTerminate is refused), and outcome and winner equal an
//     independent reference implementation of F2 over the sorted keys (written here, sharing no helper with production);
//   - a static check: the outcome kinds and the EndReason values have no draw / tie / both value (reviewed fixture list).
// The world model (who is Active, Connected, kicked; pawn-loss counts) is test code; GraceExpiry(p) on a Connected
// participant is that participant's drop (no transition), and on a disconnected one the expiry of its grace (Disconnect); PawnLost yields Void(PawnLost) on
// the third loss only (R17.9), Abort leaves Active without a terminal outcome (F2 Output Range).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/Core/Round/StealthRoundWin.h"
#include "Server/Testing/StealthRoundTestConstants.h"
#include "View/Random/StealthIntStream.h"
#include "View/Random/StealthRangedDraw.h"

namespace NoDrawPropertyTestData
{
	enum class ESymbol : uint8
	{
		FinalCompletion,
		DecisionElimination,
		GraceExpiry,
		Kick,
		Rejoin,
		PawnLost,
		Abort,
	};
	constexpr uint32 SymbolCount = 7;

	/** R17.9: the third loss of a pawn voids the round (an earlier loss is recovered). */
	constexpr int32 PawnLossVoidCount = 3;

	struct FItem
	{
		ESymbol Symbol = ESymbol::Abort;
		int32 Team = 0;
		int32 Participant = 0;
		FQueueKey Key;
	};

	struct FMember
	{
		int32 Team = 0;
		bool bActive = true;
		bool bConnected = true;
		bool bKicked = false;
		int32 Losses = 0;
	};

	/** The test world: TeamSize members per team, all Active and Connected at Active entry. */
	struct FWorld
	{
		TArray<FMember> Members;
		bool bActivePhase = true;

		explicit FWorld(int32 TeamSize)
		{
			for (int32 Team = 0; Team < 2; ++Team)
			{
				for (int32 Index = 0; Index < TeamSize; ++Index)
				{
					FMember& Member = Members.AddDefaulted_GetRef();
					Member.Team = Team;
				}
			}
		}

		int32 ActiveCount(int32 Team) const
		{
			int32 Count = 0;
			for (const FMember& M : Members) { Count += (M.Team == Team && M.bActive) ? 1 : 0; }
			return Count;
		}
		int32 ConnectedActiveCount(int32 Team) const
		{
			int32 Count = 0;
			for (const FMember& M : Members) { Count += (M.Team == Team && M.bActive && M.bConnected) ? 1 : 0; }
			return Count;
		}
		int32 RosterCount(int32 Team) const
		{
			int32 Count = 0;
			for (const FMember& M : Members) { Count += M.Team == Team ? 1 : 0; }
			return Count;
		}
		int32 FirstActive(int32 Team, bool bConnectedOnly) const
		{
			for (int32 Index = 0; Index < Members.Num(); ++Index)
			{
				if (Members[Index].Team == Team && Members[Index].bActive && (!bConnectedOnly || Members[Index].bConnected))
				{
					return Index;
				}
			}
			return INDEX_NONE;
		}
	};

	/** The reference outcome: a kind (0 none, 1 win, 2 void), the winner, and the server reason as an integer. */
	struct FExpected
	{
		int32 Kind = 0;
		int32 Winner = -1;
		int32 Reason = -1;
		int32 ItemIndex = -1;
	};

	/**
	 * The independent F2 reference (hand-written from the GDD text, no production call): the first item, in key order,
	 * for which ObjectiveWin, EliminationWin or a void holds.
	 */
	FExpected Reference(const TArray<FItem>& Items, int32 TeamSize)
	{
		FWorld W(TeamSize);
		for (int32 Index = 0; Index < Items.Num(); ++Index)
		{
			const FItem& I = Items[Index];
			if (!W.bActivePhase)
			{
				return FExpected(); // left Active by a host abort: no terminal outcome
			}
			auto Zeroed = [&W](int32 Team) { return W.ActiveCount(Team) == 0 && W.RosterCount(Team) >= 1; };
			switch (I.Symbol)
			{
			case ESymbol::FinalCompletion:
				// Valid(i): an Active, Connected, human-controlled completer on the team.
				if (W.FirstActive(I.Team, true) != INDEX_NONE)
				{
					return { 1, I.Team, static_cast<int32>(ERoundEndReason::ObjectiveWin), Index };
				}
				break;
			case ESymbol::DecisionElimination:
			{
				const int32 Victim = W.FirstActive(I.Team, false);
				if (Victim == INDEX_NONE)
				{
					break;
				}
				W.Members[Victim].bActive = false;
				const int32 Other = 1 - I.Team;
				if (Zeroed(I.Team) && W.RosterCount(Other) >= 1 && W.ActiveCount(Other) >= 1)
				{
					return { 1, Other, static_cast<int32>(ERoundEndReason::Elimination), Index };
				}
				break;
			}
			case ESymbol::GraceExpiry:
			case ESymbol::Kick:
			{
				FMember& M = W.Members[I.Participant];
				const bool bKick = I.Symbol == ESymbol::Kick;
				if (!bKick && M.bConnected)
				{
					M.bConnected = false; // the drop whose grace this symbol later expires: no transition yet
					break;
				}
				M.bConnected = false;
				M.bKicked = M.bKicked || bKick;
				if (!M.bActive)
				{
					break;
				}
				M.bActive = false;
				const int32 Other = 1 - M.Team;
				if (!Zeroed(M.Team) || W.RosterCount(Other) < 1)
				{
					break;
				}
				if (bKick)
				{
					return { 2, -1, static_cast<int32>(ERoundEndReason::VoidKickedLastActive), Index };
				}
				return W.ConnectedActiveCount(Other) >= 1
					? FExpected{ 1, Other, static_cast<int32>(ERoundEndReason::ForfeitElimination), Index }
					: FExpected{ 2, -1, static_cast<int32>(ERoundEndReason::VoidAbandoned), Index };
			}
			case ESymbol::Rejoin:
				if (!W.Members[I.Participant].bKicked)
				{
					W.Members[I.Participant].bConnected = true;
				}
				break;
			case ESymbol::PawnLost:
			{
				FMember& M = W.Members[I.Participant];
				if (M.bActive && ++M.Losses >= PawnLossVoidCount)
				{
					return { 2, -1, static_cast<int32>(ERoundEndReason::VoidPawnLost), Index };
				}
				break;
			}
			case ESymbol::Abort:
				W.bActivePhase = false;
				break;
			}
		}
		return FExpected();
	}

	/** The system under test: the same items through the production predicates and the real latch. */
	struct FObserved
	{
		int32 Successes = 0;
		FRoundTerminalOutcome First;
		int32 FirstIndex = -1;
		FQueueKey TerminalKey;
	};

	FObserved RunThroughLatch(const TArray<FItem>& Items, int32 TeamSize)
	{
		FWorld W(TeamSize);
		FRoundTerminalLatch Latch;
		FObserved Out;
		auto Context = [&W, &Latch]()
		{
			FRoundWinContext C;
			C.Phase = W.bActivePhase ? ERoundPhase::Active : ERoundPhase::Reset;
			C.bTerminated = Latch.IsTerminated();
			for (int32 Team = 0; Team < 2; ++Team)
			{
				C.RosterCount[Team] = W.RosterCount(Team);
				C.ConnectedActiveCount[Team] = W.ConnectedActiveCount(Team);
			}
			return C;
		};
		auto Try = [&](const TOptional<FRoundTerminalOutcome>& Outcome, int32 Index, const FQueueKey& Key)
		{
			if (Outcome.IsSet() && Latch.TryTerminate(Outcome.GetValue(), Key, W.bActivePhase ? ERoundPhase::Active : ERoundPhase::Reset))
			{
				if (++Out.Successes == 1)
				{
					Out.First = Outcome.GetValue();
					Out.FirstIndex = Index;
					Out.TerminalKey = Key;
				}
			}
		};
		auto Transition = [&](int32 MemberIndex, ERosterReason Reason, bool bKicked)
		{
			FMember& M = W.Members[MemberIndex];
			M.bActive = false;
			FRoundTransitionEvent E;
			E.Participant = FParticipantId(static_cast<uint32>(MemberIndex + 1));
			E.Team = M.Team == 0 ? ERosterTeam::Team0 : ERosterTeam::Team1;
			E.Reason = Reason;
			E.bKicked = bKicked;
			E.ActiveCountAfter[0] = W.ActiveCount(0);
			E.ActiveCountAfter[1] = W.ActiveCount(1);
			return E;
		};
		// Every item is fed, also after the latch (as if intake had not closed), to prove single assignment.
		for (int32 Index = 0; Index < Items.Num(); ++Index)
		{
			const FItem& I = Items[Index];
			switch (I.Symbol)
			{
			case ESymbol::FinalCompletion:
			{
				FRoundObjectiveCompletion Completion;
				Completion.bFinal = true;
				Completion.Team = static_cast<uint8>(I.Team);
				const bool bCompleter = W.FirstActive(I.Team, true) != INDEX_NONE;
				Completion.bCompleterActive = Completion.bCompleterConnected = Completion.bHumanControlled = Completion.bCompleterOnTeam = bCompleter;
				Completion.bSiteLive = true;
				Try(StealthRoundWin::EvaluateObjectiveCompletion(Completion, Context()), Index, I.Key);
				break;
			}
			case ESymbol::DecisionElimination:
			{
				const int32 Victim = W.FirstActive(I.Team, false);
				if (Victim != INDEX_NONE)
				{
					const FRoundTransitionEvent E = Transition(Victim, ERosterReason::Assassinated, false);
					Try(StealthRoundWin::EvaluateTransition(E, Context()), Index, I.Key);
				}
				break;
			}
			case ESymbol::GraceExpiry:
			case ESymbol::Kick:
			{
				FMember& M = W.Members[I.Participant];
				const bool bKick = I.Symbol == ESymbol::Kick;
				if (!bKick && M.bConnected)
				{
					M.bConnected = false;
					break;
				}
				M.bConnected = false;
				M.bKicked = M.bKicked || bKick;
				if (M.bActive)
				{
					const FRoundTransitionEvent E = Transition(I.Participant, ERosterReason::Disconnect, bKick);
					Try(StealthRoundWin::EvaluateTransition(E, Context()), Index, I.Key);
				}
				break;
			}
			case ESymbol::Rejoin:
				if (!W.Members[I.Participant].bKicked)
				{
					W.Members[I.Participant].bConnected = true;
				}
				break;
			case ESymbol::PawnLost:
			{
				FMember& M = W.Members[I.Participant];
				if (M.bActive && ++M.Losses >= PawnLossVoidCount)
				{
					Try(FRoundTerminalOutcome::Void(ERoundEndReason::VoidPawnLost), Index, I.Key);
				}
				break;
			}
			case ESymbol::Abort:
				W.bActivePhase = false;
				break;
			}
		}
		return Out;
	}
}

// The reviewed enumerator lists (adding a value fails to compile here until it is reviewed for draw semantics).
static_assert(GRoundTerminalKindCount == 2, "ERoundTerminalKind is {Win, Void}: no draw, tie or both value (review a new value)");
static_assert(static_cast<int32>(ERoundTerminalKind::Void) + 1 == GRoundTerminalKindCount, "ERoundTerminalKind count");
static_assert(GRoundEndReasonCount == 10 && static_cast<int32>(ERoundEndReason::DebugForced) + 1 == GRoundEndReasonCount,
	"ERoundEndReason has the ten reviewed values, none of them a draw (review a new value)");

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundNoDrawPropertyTest, "StealthGame.Round.NoDrawProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundNoDrawPropertyTest::RunTest(const FString& Parameters)
{
	using namespace NoDrawPropertyTestData;
	using namespace RoundTestFixtures;

	// Static check (runtime half): every EndReason value is a single-team win, a no-award void, or a non-terminal reason.
	const ERoundEndReason Reviewed[] = { ERoundEndReason::ObjectiveWin, ERoundEndReason::Elimination, ERoundEndReason::ForfeitElimination,
		ERoundEndReason::Aborted, ERoundEndReason::VoidAbandoned, ERoundEndReason::VoidKickedLastActive, ERoundEndReason::VoidPawnLost,
		ERoundEndReason::PreparingFailed, ERoundEndReason::SessionEnded, ERoundEndReason::DebugForced };
	TestEqual(TEXT("the reviewed EndReason list covers the enum"), static_cast<int32>(UE_ARRAY_COUNT(Reviewed)), GRoundEndReasonCount);
	TestEqual(TEXT("FRoundResult names one winning team or none (no 'both' value)"), static_cast<int32>(sizeof(FRoundResult::WinningTeam)), 1);

	FIntStream Stream(InterleavingStreamSeed, InterleavingStreamSeed ^ 0x5DEECE66Dull);
	const int32 Participants = 2 * InterleavingTeamSize;
	int32 Mismatches = 0;
	int32 DoubleTerminations = 0;
	int32 Outcomes[GRoundEndReasonCount] = {};
	int32 NoOutcome = 0;
	TArray<FItem> Items;
	Items.Reserve(InterleavingLength);
	for (int32 Run = 0; Run < InterleavingRuns; ++Run)
	{
		// A random interleaving with strictly increasing keys (random tick gaps, Seq within a tick).
		Items.Reset();
		uint64 Tick = 1000;
		uint64 Seq = 1;
		for (int32 Index = 0; Index < InterleavingLength; ++Index)
		{
			FItem& Item = Items.AddDefaulted_GetRef();
			Item.Symbol = static_cast<ESymbol>(StealthRandom::UniformBelow(Stream, SymbolCount));
			Item.Team = static_cast<int32>(StealthRandom::UniformBelow(Stream, 2));
			Item.Participant = static_cast<int32>(StealthRandom::UniformBelow(Stream, static_cast<uint32>(Participants)));
			Tick += StealthRandom::UniformBelow(Stream, 2);
			Item.Key = FQueueKey(Tick, Seq++);
		}
		const FExpected Expected = Reference(Items, InterleavingTeamSize);
		const FObserved Observed = RunThroughLatch(Items, InterleavingTeamSize);

		DoubleTerminations += Observed.Successes > 1 ? 1 : 0;
		const int32 ObservedKind = Observed.Successes == 0 ? 0 : (Observed.First.IsWin() ? 1 : 2);
		const int32 ObservedWinner = Observed.Successes > 0 && Observed.First.IsWin() ? Observed.First.WinningTeam : -1;
		const int32 ObservedReason = Observed.Successes > 0 ? static_cast<int32>(Observed.First.EndReason) : -1;
		const bool bMatch = ObservedKind == Expected.Kind && ObservedWinner == Expected.Winner && ObservedReason == Expected.Reason
			&& Observed.FirstIndex == Expected.ItemIndex && (Expected.Kind == 0 || Observed.TerminalKey == Items[Expected.ItemIndex].Key);
		if (!bMatch && Mismatches++ < 5)
		{
			AddError(FString::Printf(TEXT("run %d: reference kind %d winner %d reason %d at %d; latch kind %d winner %d reason %d at %d"), Run,
				Expected.Kind, Expected.Winner, Expected.Reason, Expected.ItemIndex, ObservedKind, ObservedWinner, ObservedReason, Observed.FirstIndex));
		}
		if (Observed.Successes > 0)
		{
			++Outcomes[static_cast<int32>(Observed.First.EndReason)];
		}
		else
		{
			++NoOutcome;
		}
	}

	TestEqual(TEXT("at most one terminal outcome per round (no second TryTerminate succeeded)"), DoubleTerminations, 0);
	TestEqual(TEXT("outcome and winner equal the independent F2 reference in every run"), Mismatches, 0);
	// The fixture exercises every outcome path (otherwise the property would be vacuous).
	TestTrue(TEXT("objective wins occur"), Outcomes[static_cast<int32>(ERoundEndReason::ObjectiveWin)] > 0);
	TestTrue(TEXT("decision eliminations occur"), Outcomes[static_cast<int32>(ERoundEndReason::Elimination)] > 0);
	TestTrue(TEXT("forfeits occur"), Outcomes[static_cast<int32>(ERoundEndReason::ForfeitElimination)] > 0);
	TestTrue(TEXT("abandoned voids occur"), Outcomes[static_cast<int32>(ERoundEndReason::VoidAbandoned)] > 0);
	TestTrue(TEXT("kick voids occur"), Outcomes[static_cast<int32>(ERoundEndReason::VoidKickedLastActive)] > 0);
	TestTrue(TEXT("pawn-loss voids occur"), Outcomes[static_cast<int32>(ERoundEndReason::VoidPawnLost)] > 0);
	TestTrue(TEXT("rounds with no outcome (aborts, 1-sided stalls) occur"), NoOutcome > 0);
	return true;
}

#endif
