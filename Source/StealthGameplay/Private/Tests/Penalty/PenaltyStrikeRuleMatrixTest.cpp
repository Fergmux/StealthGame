// Story nkp-002 (production/epics/npc-kill-penalty-informants/story-002-strike-rule-and-release-grace.md), AC1 / AC-NKP-01
// (PEN-R2, F1): all 36 cells of Cause {Strike, Reversal} x VictimKind {Player, Npc, Informant} x killer {Active participant,
// Informant participant, NPC with no participant} x grace {in, out}. StrikeCount rises by 1 only for (Strike, Npc, Active,
// out of grace); the Informant-killer cells add no strike and report exactly one fault (the ensure through the injected
// sink); the NPC-killer cells add no strike, report nothing and do not crash. Impossible combinations still run; "grace
// in" is a released-body entry on the victim, whatever the victim kind.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyTestRig.h"

namespace PenaltyStrikeRuleMatrixTestData
{
	enum class EKiller : uint8
	{
		Active,
		Informant,
		Npc,
	};

	const TCHAR* KillerName(EKiller Killer)
	{
		return Killer == EKiller::Active ? TEXT("Active") : (Killer == EKiller::Informant ? TEXT("Informant") : TEXT("NPC"));
	}

	const TCHAR* KindName(EVictimKind Kind)
	{
		return Kind == EVictimKind::Player ? TEXT("Player") : (Kind == EVictimKind::Npc ? TEXT("Npc") : TEXT("Informant"));
	}

	/** The result the resolver would carry for the cell (the tally query is asked for every executed Attack). */
	EStrikeResult ResultOf(EKillCause Cause, EVictimKind Kind)
	{
		if (Cause == EKillCause::Reversal)
		{
			return EStrikeResult::Reversed;
		}
		return Kind == EVictimKind::Player ? EStrikeResult::RivalEliminated : EStrikeResult::WrongTarget;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyStrikeRuleMatrixTest, "StealthGame.Penalty.StrikeRuleMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyStrikeRuleMatrixTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;
	using namespace PenaltyStrikeRuleMatrixTestData;

	int32 Cells = 0;
	for (const EKillCause Cause : { EKillCause::Strike, EKillCause::Reversal })
	{
		for (const EVictimKind Kind : { EVictimKind::Player, EVictimKind::Npc, EVictimKind::Informant })
		{
			for (const EKiller Killer : { EKiller::Active, EKiller::Informant, EKiller::Npc })
			{
				for (const bool bGraceIn : { true, false })
				{
					++Cells;
					const FString Cell = FString::Printf(TEXT("(%s, %s, %s killer, grace %s)"), Cause == EKillCause::Strike ? TEXT("Strike") : TEXT("Reversal"),
						KindName(Kind), KillerName(Killer), bGraceIn ? TEXT("in") : TEXT("out"));

					// Arrange
					FRig Rig;
					if (Killer == EKiller::Informant)
					{
						Rig.Roster.States[P] = EParticipantState::Informant;
					}
					const FPenaltyPawnKey Victim = Pawn(700);
					if (bGraceIn)
					{
						Rig.Core->RecordReleasedBody(Victim, Rig.Clock.GetServerTimeSeconds()); // the kill lands 0.25 s later
					}
					const FParticipantId KillerId = Killer == EKiller::Npc ? FParticipantId() : P;

					// Act
					EPenaltyKillOutcome Outcome = EPenaltyKillOutcome::NoStrike;
					Rig.Attack(KillerId, FPenaltyKill{ Victim, KillerId, Kind, Cause }, ResultOf(Cause, Kind), &Outcome);

					// Assert
					const bool bStrikes = Cause == EKillCause::Strike && Kind == EVictimKind::Npc && Killer == EKiller::Active && !bGraceIn;
					TestEqual(FString::Printf(TEXT("%s: StrikeCount"), *Cell), static_cast<int32>(Rig.Core->GetStrikeCount(P)), bStrikes ? 1 : 0);
					const int32 Faults = Rig.Faults.GetRecords().Num();
					if (Killer == EKiller::Informant)
					{
						TestTrue(FString::Printf(TEXT("%s: exactly one InformantKiller fault (got %d)"), *Cell, Faults), Faults == 1
							&& Rig.Faults.GetRecords()[0].Fault == EStealthFault::InvariantViolation
							&& Rig.Faults.GetRecords()[0].Detail == PenaltyFaultDetail::InformantKiller);
						TestTrue(FString::Printf(TEXT("%s: outcome InformantKillerFault"), *Cell), Outcome == EPenaltyKillOutcome::InformantKillerFault);
					}
					else
					{
						TestEqual(FString::Printf(TEXT("%s: no fault"), *Cell), Faults, 0);
					}
				}
			}
		}
	}
	TestEqual(TEXT("all 36 cells ran"), Cells, 36);
	return true;
}

#endif
