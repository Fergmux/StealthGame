// Story rmf-028 (production/epics/round-match-framework/story-028-phase-admission-and-illegal-transitions.md), GDD AC
// 40 / R4, States and Transitions: entering Lobby, Preparing, Active, Ended, Reset pushes Open, RejoinOnly (x3),
// Closed in table order; every illegal transition of the States section is refused (table-driven) and leaves the
// phase, the pushed admission mode and PhaseChangeCount unchanged. This table covers the phase-to-phase pairs; "two
// latches" and "a queue item executed outside Active" are asserted where those mechanisms are built (stories 043 and
// 008), and "any phase change by a client" by story 026 (a client holds no Round core).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Algo/AnyOf.h"
#include "StealthRoundExitBTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundPhaseAdmissionAndIllegalTransitionsTest, "StealthGame.Round.PhaseAdmissionAndIllegalTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundPhaseAdmissionAndIllegalTransitionsTest::RunTest(const FString& Parameters)
{
	// --- Admission push in table order ------------------------------------------------------------------------------
	{
		FRoundFakeCallLog Log;
		FRoundFakeGatewaySet Fakes(Log);
		Fakes.Session.Script(TEXT("SetAdmissionMode"), 0, 5);
		FRoundPhaseCore Core(&Fakes.Session);
		const ERoundPhase Order[] = { ERoundPhase::Lobby, ERoundPhase::Preparing, ERoundPhase::Active, ERoundPhase::Ended, ERoundPhase::Reset };
		for (const ERoundPhase Phase : Order)
		{
			TestTrue(FString::Printf(TEXT("enter %s"), LexRoundPhase(Phase)), Core.RequestPhase(Phase, 1.0));
		}
		const ERoundAdmissionMode Expected[] = { ERoundAdmissionMode::Open, ERoundAdmissionMode::RejoinOnly, ERoundAdmissionMode::RejoinOnly,
			ERoundAdmissionMode::RejoinOnly, ERoundAdmissionMode::Closed };
		if (TestEqual(TEXT("five admission pushes"), Log.Calls.Num(), 5))
		{
			for (int32 Index = 0; Index < 5; ++Index)
			{
				TestTrue(FString::Printf(TEXT("push %d is the table's mode"), Index), Log.Calls[Index].Call == FName(TEXT("SetAdmissionMode"))
					&& Log.Calls[Index].Args[0] == static_cast<int64>(Expected[Index]));
			}
		}
		TestEqual(TEXT("PhaseChangeCount counts every write"), Core.GetPhaseChangeCount(), 5u);
		TestTrue(TEXT("no unexpected call"), Log.Unexpected.IsEmpty());
	}

	// --- Table-driven guard ------------------------------------------------------------------------------------------
	struct FPair
	{
		ERoundPhase From;
		ERoundPhase To;
	};
	const FPair Legal[] = {
		{ ERoundPhase::Unknown, ERoundPhase::Lobby }, { ERoundPhase::Lobby, ERoundPhase::Preparing }, { ERoundPhase::Preparing, ERoundPhase::Active },
		{ ERoundPhase::Preparing, ERoundPhase::Reset }, { ERoundPhase::Active, ERoundPhase::Ended }, { ERoundPhase::Active, ERoundPhase::Reset },
		{ ERoundPhase::Ended, ERoundPhase::Reset }, { ERoundPhase::Reset, ERoundPhase::Lobby },
	};
	// The GDD's "Illegal and asserted in tests" phase pairs.
	const FPair Illegal[] = {
		{ ERoundPhase::Active, ERoundPhase::Lobby }, { ERoundPhase::Active, ERoundPhase::Preparing }, { ERoundPhase::Ended, ERoundPhase::Active },
		{ ERoundPhase::Ended, ERoundPhase::Preparing }, { ERoundPhase::Lobby, ERoundPhase::Active },
	};
	for (const FPair& Pair : Legal)
	{
		TestTrue(FString::Printf(TEXT("legal %s -> %s"), LexRoundPhase(Pair.From), LexRoundPhase(Pair.To)), FRoundPhaseCore::IsLegalTransition(Pair.From, Pair.To));
	}
	// Every pair outside the legal table is refused (the illegal list is a subset; self-transitions included).
	const ERoundPhase All[] = { ERoundPhase::Unknown, ERoundPhase::Lobby, ERoundPhase::Preparing, ERoundPhase::Active, ERoundPhase::Ended, ERoundPhase::Reset };
	for (const ERoundPhase From : All)
	{
		for (const ERoundPhase To : All)
		{
			const bool bListed = Algo::AnyOf(Legal, [From, To](const FPair& P) { return P.From == From && P.To == To; });
			TestEqual(FString::Printf(TEXT("%s -> %s legality matches the table"), LexRoundPhase(From), LexRoundPhase(To)), FRoundPhaseCore::IsLegalTransition(From, To), bListed);
		}
	}

	// A refused transition changes nothing: drive a real core to each From, attempt each illegal To.
	auto DriveCore = [](FRoundPhaseCore& Core, ERoundPhase Target)
	{
		const ERoundPhase Path[] = { ERoundPhase::Lobby, ERoundPhase::Preparing, ERoundPhase::Active, ERoundPhase::Ended };
		for (const ERoundPhase Step : Path)
		{
			if (Core.GetPhase() == Target)
			{
				return;
			}
			Core.RequestPhase(Step, 1.0);
		}
	};
	for (const FPair& Pair : Illegal)
	{
		FRoundFakeCallLog Log;
		FRoundFakeGatewaySet Fakes(Log);
		Fakes.Session.Script(TEXT("SetAdmissionMode"), 0, 8);
		FRoundPhaseCore Core(&Fakes.Session);
		DriveCore(Core, Pair.From);
		TestTrue(FString::Printf(TEXT("arranged %s"), LexRoundPhase(Pair.From)), Core.GetPhase() == Pair.From);
		const uint32 CountBefore = Core.GetPhaseChangeCount();
		const int32 PushesBefore = Log.Calls.Num();

		TestFalse(FString::Printf(TEXT("illegal %s -> %s refused"), LexRoundPhase(Pair.From), LexRoundPhase(Pair.To)), Core.RequestPhase(Pair.To, 2.0));

		TestTrue(FString::Printf(TEXT("%s -> %s: phase unchanged"), LexRoundPhase(Pair.From), LexRoundPhase(Pair.To)), Core.GetPhase() == Pair.From);
		TestEqual(FString::Printf(TEXT("%s -> %s: PhaseChangeCount unchanged"), LexRoundPhase(Pair.From), LexRoundPhase(Pair.To)), Core.GetPhaseChangeCount(), CountBefore);
		TestEqual(FString::Printf(TEXT("%s -> %s: no admission push"), LexRoundPhase(Pair.From), LexRoundPhase(Pair.To)), Log.Calls.Num(), PushesBefore);
	}

	// OnSessionEnding returns any phase to Lobby.
	{
		FRoundFakeCallLog Log;
		FRoundFakeGatewaySet Fakes(Log);
		Fakes.Session.Script(TEXT("SetAdmissionMode"), 0, 8);
		FRoundPhaseCore Core(&Fakes.Session);
		DriveCore(Core, ERoundPhase::Active);
		TestTrue(TEXT("OnSessionEnding: Active -> Lobby"), Core.EnterLobbyOnSessionEnding(3.0) && Core.GetPhase() == ERoundPhase::Lobby);
	}
	return true;
}

#endif
