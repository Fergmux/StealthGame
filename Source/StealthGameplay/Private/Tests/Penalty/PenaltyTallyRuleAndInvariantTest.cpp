// Story nkp-003 (production/epics/npc-kill-penalty-informants/story-003-tally-rule-and-strike-result-supply.md), AC1 /
// AC-NKP-02 (PEN-R3, F1): for the outcomes RivalEliminated, WrongTarget (NPC victim), WrongTarget (informant victim),
// WrongTarget (graced body), NoContact and Reversed the tally rises by exactly 1 for each WrongTarget and 0 otherwise; over
// 10,000 sequences drawn from the fixture seed kPenaltyTestSeed, StrikeCount <= WrongTargetTally after every event and an
// exile (bExiled) happens only when the tally >= L; a tally of 255 stays 255 after another WrongTarget (no uint8 wrap) and
// the 256th WrongTarget leaves LastResultSeq on the last one that changed it.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyTestRig.h"
#include "View/Random/StealthIntStream.h"

namespace PenaltyTallyRuleAndInvariantTestData
{
	enum class EOutcome : uint8
	{
		RivalEliminated,
		WrongTargetNpc,
		WrongTargetInformant,
		WrongTargetGraced,
		NoContact,
		Reversed,
		Count,
	};

	constexpr int32 SequenceCount = 10000;
	constexpr int32 MaxEventsPerSequence = 16;
	constexpr uint64 PawnOfP = 10;

	/** One executed Attack of P with the outcome; Victim is a fresh body id. */
	FStrikeTally Play(StealthPenaltyTest::FRig& Rig, EOutcome Outcome, uint64 Victim)
	{
		using namespace StealthPenaltyTest;
		switch (Outcome)
		{
		case EOutcome::RivalEliminated:
			return Rig.Attack(P, FPenaltyKill{ Pawn(Victim), P, EVictimKind::Player, EKillCause::Strike }, EStrikeResult::RivalEliminated);
		case EOutcome::WrongTargetNpc:
			return Rig.NpcKill(P, Victim);
		case EOutcome::WrongTargetInformant:
			return Rig.InformantKill(P, Victim);
		case EOutcome::WrongTargetGraced:
			Rig.Core->RecordReleasedBody(Pawn(Victim), Rig.Clock.GetServerTimeSeconds()); // killed 0.25 s after its release
			return Rig.NpcKill(P, Victim);
		case EOutcome::NoContact:
			return Rig.Attack(P, TOptional<FPenaltyKill>(), EStrikeResult::NoContact);
		case EOutcome::Reversed:
		default:
			return Rig.Attack(P, FPenaltyKill{ Pawn(PawnOfP), FParticipantId(), EVictimKind::Player, EKillCause::Reversal }, EStrikeResult::Reversed);
		}
	}

	bool IsWrongTarget(EOutcome Outcome)
	{
		return Outcome == EOutcome::WrongTargetNpc || Outcome == EOutcome::WrongTargetInformant || Outcome == EOutcome::WrongTargetGraced;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyTallyRuleAndInvariantTest, "StealthGame.Penalty.TallyRuleAndInvariant",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyTallyRuleAndInvariantTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;
	using namespace PenaltyTallyRuleAndInvariantTestData;

	// The six outcomes, one at a time.
	{
		const TCHAR* Names[] = { TEXT("RivalEliminated"), TEXT("WrongTarget (NPC)"), TEXT("WrongTarget (informant)"), TEXT("WrongTarget (graced body)"), TEXT("NoContact"), TEXT("Reversed") };
		for (int32 Index = 0; Index < static_cast<int32>(EOutcome::Count); ++Index)
		{
			FRig Rig;
			const EOutcome Outcome = static_cast<EOutcome>(Index);
			const FStrikeTally Tally = Play(Rig, Outcome, 500);
			TestEqual(FString::Printf(TEXT("%s: tally +%d"), Names[Index], IsWrongTarget(Outcome) ? 1 : 0), static_cast<int32>(Tally.TallyAfter), IsWrongTarget(Outcome) ? 1 : 0);
			TestEqual(FString::Printf(TEXT("%s: strike only for the ungraced NPC"), Names[Index]), static_cast<int32>(Rig.Core->GetStrikeCount(P)),
				Outcome == EOutcome::WrongTargetNpc ? 1 : 0);
		}
	}

	// 10,000 seeded sequences: k <= t after every event; an exile only when t >= L.
	{
		FIntStream Stream(kPenaltyTestSeed, kPenaltyTestSeed ^ 0x9E3779B97F4A7C15ull);
		int32 Events = 0;
		int32 Exiles = 0;
		int32 InvariantBreaks = 0;
		int32 EarlyExiles = 0;
		int32 Faults = 0;
		for (int32 Sequence = 0; Sequence < SequenceCount; ++Sequence)
		{
			FRig Rig;
			const int32 Limit = Rig.Core->GetLatchedStrikeLimit();
			for (int32 Event = 0; Event < MaxEventsPerSequence; ++Event)
			{
				const EOutcome Outcome = static_cast<EOutcome>(Stream.NextU32() % static_cast<uint32>(EOutcome::Count));
				const FStrikeTally Tally = Play(Rig, Outcome, 1000 + Event);
				++Events;
				InvariantBreaks += Rig.Core->GetStrikeCount(P) > Rig.Core->GetWrongTargetTally(P) ? 1 : 0;
				if (Tally.bExiled)
				{
					++Exiles;
					EarlyExiles += static_cast<int32>(Tally.TallyAfter) < Limit ? 1 : 0;
					Rig.Roster.States[P] = EParticipantState::Informant; // story nkp-005's transition: an informant cannot attack
					break;
				}
			}
			Faults += Rig.Faults.GetRecords().Num();
		}
		TestEqual(TEXT("k <= t after every event of every sequence"), InvariantBreaks, 0);
		TestEqual(TEXT("no exile before the tally reached L"), EarlyExiles, 0);
		TestTrue(FString::Printf(TEXT("the sequences exercised the exile (%d exiles in %d events)"), Exiles, Events), Exiles > 0 && Events > SequenceCount);
		TestEqual(TEXT("no fault in any sequence"), Faults, 0);
	}

	// Saturation: 255 stays 255 (no uint8 wrap); LastResultSeq stays on the last WrongTarget that changed the tally.
	{
		FRig Rig;
		for (uint32 Index = 1; Index <= MAX_uint8; ++Index)
		{
			Rig.Core->GetTallyForResult(P, EStrikeResult::WrongTarget, FRig::Seq(Index));
		}
		const FStrikeTally At255 = Rig.Core->GetTallyForResult(P, EStrikeResult::WrongTarget, FRig::Seq(MAX_uint8 + 1));
		TestEqual(TEXT("the 256th WrongTarget leaves the tally at 255"), static_cast<int32>(At255.TallyAfter), static_cast<int32>(MAX_uint8));
		const FStrikeTally After = Rig.Core->GetTallyForResult(P, EStrikeResult::WrongTarget, FRig::Seq(MAX_uint8 + 2));
		TestEqual(TEXT("still 255 after another"), static_cast<int32>(After.TallyAfter), static_cast<int32>(MAX_uint8));
		const FPenaltyRecord* Record = Rig.Core->FindRecord(P);
		TestTrue(TEXT("LastResultSeq is the 255th result's (the last that changed the tally)"), Record != nullptr && Record->LastResultSeq.Seq == MAX_uint8);
	}
	return true;
}

#endif
