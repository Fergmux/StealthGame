// Story cnb-002 (production/epics/crowd-npc-behaviour/story-002-crowd-draws-and-streams.md): counter-based crowd draws
// (ADR-0016 section 9, GDD CNB-R14, AC-CNB-29, AC-CNB-34).
//   GoldenVectors          UnitFraction53 and InvNormalAS241 match committed vectors bit for bit; u is never 0 or 1.
//   StreamsKillIndependent two fixed-timestep runs of one seed, run B with one extra kill at t = 60 s: every other NPC's
//                          k-th draw of each decision type is identical (timing may differ); a sequential shared stream
//                          (the negative control) fails the same comparison.
//   StreamDeterminism      two runs are bit-identical; a takeover then release of one pawn leaves its later draws equal
//                          to the no-takeover run (counters keyed by ordinal in the store, not on the controller).
//   NoRawRandom            the forbidden-RNG lint scans the Crowd tree with zero findings, and forced Lemire retries
//                          extend Retry only (DecisionIndex advances by one per draw).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdTestUtils.h"
#include "Fixtures/CrowdGoldenVectors.h"
#include "Server/CrowdDraws.h"

namespace CrowdDrawsTestData
{
	/** A fixed fixture sub-seed (never a real secret). */
	constexpr uint64 FixtureSubSeed = 0x00C0FFEE12345678ull;
	constexpr int32 NpcCount = 20;
	constexpr double StepSec = 0.1;
	constexpr int32 Steps = 1200; // 120 s
	constexpr double KillTimeSec = 60.0;
	constexpr uint32 KilledOrdinal = 7;
	/** The ring the toy crowd walks on (metres) and the meeting distance that triggers a yield decision. */
	constexpr double RingLength = 200.0;
	constexpr double MeetDistance = 1.0;

	uint64 DoubleBits(double Value)
	{
		uint64 Bits;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	/** One logged draw: (ordinal, decision, index) -> value bits, plus the step it happened at. */
	struct FLogged
	{
		uint32 Ordinal;
		ECrowdDecision Decision;
		uint32 Index;
		uint64 ValueBits;
		int32 Step;
	};

	/**
	 * The draw source of the toy run: the real FCrowdDraws, or (negative control) one sequential stream shared by every
	 * pawn, the shape the prototype used.
	 */
	struct FSource
	{
		bool bSequential = false;
		FCrowdDraws Draws{ FixtureSubSeed };
		FSecretStream Shared{ FixtureSubSeed, CrowdBehaviourStreamTag };
		FCrowdDecisionCounters SharedCounters;

		double Unit(uint32 Ordinal, ECrowdDecision Decision, TArray<FLogged>& Log, int32 Step)
		{
			uint32 Index;
			double Value;
			if (bSequential)
			{
				Index = SharedCounters.Take(Ordinal, Decision);
				Value = FCrowdDraws::UnitFraction53(Shared.NextU64());
			}
			else
			{
				Index = Draws.GetCounters().Peek(Ordinal, Decision);
				Value = Draws.DrawUnit(Ordinal, Decision);
			}
			Log.Add({ Ordinal, Decision, Index, DoubleBits(Value), Step });
			return Value;
		}
	};

	/**
	 * A toy fixed-timestep crowd on a ring: legs of drawn length, pauses of drawn kind and length, and a yield decision
	 * drawn when two walkers meet (conditional on who is near, like the real yield and conversation decisions). Kill:
	 * the victim stops drawing at KillStep. TakeoverFrom/To: the pawn draws nothing in that window (a human drives it)
	 * and resumes afterwards with a fresh controller object; its counters are the store's.
	 */
	TArray<FLogged> RunToyCrowd(bool bSequential, int32 KillStep, int32 TakeoverOrdinal = -1, int32 TakeoverFrom = 0, int32 TakeoverTo = 0)
	{
		FSource Source;
		Source.bSequential = bSequential;
		TArray<FLogged> Log;
		struct FNpc
		{
			double Position = 0.0;
			double Direction = 1.0;
			double LegLeft = 0.0;
			double PauseLeft = 0.0;
			bool bAlive = true;
		};
		TArray<FNpc> Npcs;
		Npcs.SetNum(NpcCount);
		for (int32 Index = 0; Index < NpcCount; ++Index)
		{
			// Deterministic start spacing; half walk each way so walkers meet.
			Npcs[Index].Position = Index * (RingLength / NpcCount);
			Npcs[Index].Direction = (Index % 2 == 0) ? 1.0 : -1.0;
		}
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			for (uint32 Ordinal = 0; Ordinal < static_cast<uint32>(NpcCount); ++Ordinal)
			{
				FNpc& Npc = Npcs[Ordinal];
				if (Ordinal == KilledOrdinal && KillStep >= 0 && Step >= KillStep)
				{
					Npc.bAlive = false;
				}
				const bool bHumanDriven = static_cast<int32>(Ordinal) == TakeoverOrdinal && Step >= TakeoverFrom && Step < TakeoverTo;
				if (!Npc.bAlive || bHumanDriven)
				{
					continue;
				}
				if (Npc.PauseLeft > 0.0)
				{
					Npc.PauseLeft -= StepSec;
					continue;
				}
				if (Npc.LegLeft <= 0.0)
				{
					const bool bLong = Source.Unit(Ordinal, ECrowdDecision::PauseKind, Log, Step) < 0.45;
					const double U = Source.Unit(Ordinal, ECrowdDecision::PauseLength, Log, Step);
					Npc.PauseLeft = bLong ? 4.0 + 12.0 * U : 1.0 + 2.0 * U;
					Npc.LegLeft = 10.0 + 20.0 * Source.Unit(Ordinal, ECrowdDecision::LegLength, Log, Step);
					continue;
				}
				const double Move = 1.4 * StepSec;
				Npc.Position = FMath::Fmod(Npc.Position + Npc.Direction * Move + RingLength, RingLength);
				Npc.LegLeft -= Move;
				// Meeting: an approaching live walker within MeetDistance draws one yield decision (kind-blind).
				for (uint32 Other = 0; Other < static_cast<uint32>(NpcCount); ++Other)
				{
					const FNpc& O = Npcs[Other];
					if (Other == Ordinal || !O.bAlive || O.PauseLeft > 0.0 || O.Direction == Npc.Direction)
					{
						continue;
					}
					double Gap = FMath::Abs(O.Position - Npc.Position);
					Gap = FMath::Min(Gap, RingLength - Gap);
					if (Gap < MeetDistance)
					{
						Source.Unit(Ordinal, ECrowdDecision::Yield, Log, Step);
					}
				}
			}
		}
		return Log;
	}

	using FDrawKey = TTuple<uint32, uint16, uint32>;

	TMap<FDrawKey, const FLogged*> Index(const TArray<FLogged>& Log)
	{
		TMap<FDrawKey, const FLogged*> Map;
		for (const FLogged& Entry : Log)
		{
			Map.Add(FDrawKey(Entry.Ordinal, static_cast<uint16>(Entry.Decision), Entry.Index), &Entry);
		}
		return Map;
	}

	/** Mismatched (ordinal, decision, index) value pairs between two logs, over keys present in both, Skip excluded. */
	int32 CountValueMismatches(const TArray<FLogged>& A, const TArray<FLogged>& B, int32 SkipOrdinal, int32& OutCommon, int32& OutTimingDiffs)
	{
		const TMap<FDrawKey, const FLogged*> MapB = Index(B);
		int32 Mismatches = 0;
		OutCommon = 0;
		OutTimingDiffs = 0;
		for (const FLogged& Entry : A)
		{
			if (static_cast<int32>(Entry.Ordinal) == SkipOrdinal)
			{
				continue;
			}
			if (const FLogged* const* Other = MapB.Find(FDrawKey(Entry.Ordinal, static_cast<uint16>(Entry.Decision), Entry.Index)))
			{
				++OutCommon;
				Mismatches += (*Other)->ValueBits != Entry.ValueBits ? 1 : 0;
				OutTimingDiffs += (*Other)->Step != Entry.Step ? 1 : 0;
			}
		}
		return Mismatches;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdGoldenVectorsTest, "StealthGame.Crowd.GoldenVectors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdGoldenVectorsTest::RunTest(const FString& Parameters)
{
	using namespace CrowdDrawsTestData;
	// Arrange / act / assert per committed row.
	for (const CrowdGoldenVectors::FUnitRow& Row : CrowdGoldenVectors::UnitRows)
	{
		const double U = FCrowdDraws::UnitFraction53(Row.Bits);
		TestEqual(FString::Printf(TEXT("UnitFraction53(0x%016llx) bits"), Row.Bits), DoubleBits(U), Row.ExpectedDoubleBits);
		TestTrue(FString::Printf(TEXT("UnitFraction53(0x%016llx) is in the open interval (0, 1)"), Row.Bits), U > 0.0 && U < 1.0);
	}
	for (const CrowdGoldenVectors::FNormalRow& Row : CrowdGoldenVectors::NormalReferenceRows)
	{
		const double Z = FCrowdDraws::InvNormalAS241(Row.U);
		TestTrue(FString::Printf(TEXT("AS241(%.17g) = %.17g within %g of the reference %.17g"), Row.U, Z, Row.Tolerance, Row.Reference), FMath::Abs(Z - Row.Reference) <= Row.Tolerance);
	}
	// The AS241 output bits at the edge inputs are pinned: a changed coefficient or evaluation order changes them.
	for (const CrowdGoldenVectors::FPinnedRow& Row : CrowdGoldenVectors::PinnedNormalRows)
	{
		const double Z = FCrowdDraws::InvNormalAS241(FCrowdDraws::UnitFraction53(Row.Bits));
		if (!TestEqual(FString::Printf(TEXT("AS241(UnitFraction53(0x%016llx)) bits"), Row.Bits), DoubleBits(Z), Row.ExpectedDoubleBits))
		{
			AddInfo(FString::Printf(TEXT("GOLDEN { 0x%016llxull, 0x%016llxull }, // %.17g"), Row.Bits, DoubleBits(Z), Z));
		}
	}
	// Never 0 or 1 over a sweep of the top and bottom of the 53-bit range.
	for (uint64 Offset = 0; Offset < 4096; ++Offset)
	{
		const double Low = FCrowdDraws::UnitFraction53(Offset);
		const double High = FCrowdDraws::UnitFraction53(~0ull - Offset);
		if (!(Low > 0.0 && Low < 1.0 && High > 0.0 && High < 1.0))
		{
			AddError(FString::Printf(TEXT("u left (0, 1) at offset %llu"), Offset));
			break;
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdStreamsKillIndependentTest, "StealthGame.Crowd.StreamsKillIndependent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdStreamsKillIndependentTest::RunTest(const FString& Parameters)
{
	using namespace CrowdDrawsTestData;
	const int32 KillStep = static_cast<int32>(KillTimeSec / StepSec);

	// Arrange + act: run A (no kill) and run B (one extra kill at t = 60 s), both counter-based.
	const TArray<FLogged> RunA = RunToyCrowd(/*bSequential*/ false, /*KillStep*/ -1);
	const TArray<FLogged> RunB = RunToyCrowd(/*bSequential*/ false, KillStep);

	// Assert: every non-involved NPC's k-th draw of each decision type is identical; timing differs somewhere (so the
	// kill really changed the world, otherwise the comparison proves nothing).
	int32 Common = 0;
	int32 TimingDiffs = 0;
	const int32 Mismatches = CountValueMismatches(RunA, RunB, KilledOrdinal, Common, TimingDiffs);
	AddInfo(FString::Printf(TEXT("KILLINDEP common draws %d, value mismatches %d, draws at a different step %d, run A %d draws, run B %d draws"), Common, Mismatches, TimingDiffs, RunA.Num(), RunB.Num()));
	TestTrue(TEXT("the runs share many draws"), Common > 1000);
	TestEqual(TEXT("value mismatches among non-involved NPCs (AC-CNB-29)"), Mismatches, 0);
	TestTrue(TEXT("the extra kill changed some other NPC's decision timing or count (the fixture is sensitive)"), TimingDiffs > 0);

	// Negative control: one sequential stream shared by every pawn shifts other pawns' draws after the kill.
	const TArray<FLogged> SeqA = RunToyCrowd(/*bSequential*/ true, -1);
	const TArray<FLogged> SeqB = RunToyCrowd(/*bSequential*/ true, KillStep);
	int32 SeqCommon = 0;
	int32 SeqTiming = 0;
	const int32 SeqMismatches = CountValueMismatches(SeqA, SeqB, KilledOrdinal, SeqCommon, SeqTiming);
	AddInfo(FString::Printf(TEXT("KILLINDEP negative control (sequential shared stream): %d mismatches of %d"), SeqMismatches, SeqCommon));
	TestTrue(TEXT("the negative control (sequential shared stream) is caught by the same comparison"), SeqMismatches > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdStreamDeterminismTest, "StealthGame.Crowd.StreamDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdStreamDeterminismTest::RunTest(const FString& Parameters)
{
	using namespace CrowdDrawsTestData;
	// Two runs of one seed and timestep: bit-identical logs.
	const TArray<FLogged> First = RunToyCrowd(false, -1);
	const TArray<FLogged> Second = RunToyCrowd(false, -1);
	bool bIdentical = First.Num() == Second.Num();
	for (int32 Index = 0; bIdentical && Index < First.Num(); ++Index)
	{
		bIdentical = First[Index].ValueBits == Second[Index].ValueBits && First[Index].Ordinal == Second[Index].Ordinal
			&& First[Index].Decision == Second[Index].Decision && First[Index].Index == Second[Index].Index && First[Index].Step == Second[Index].Step;
	}
	TestTrue(TEXT("two runs of one seed are bit-identical (AC-CNB-34a)"), bIdentical);

	// Takeover then release of pawn 3 (human-driven 30-50 s, a fresh controller afterwards): its later draws continue its
	// own counters, so the k-th draw of each type equals the no-takeover run's k-th draw.
	constexpr int32 TakenOver = 3;
	const TArray<FLogged> WithTakeover = RunToyCrowd(false, -1, TakenOver, 300, 500);
	const TMap<FDrawKey, const FLogged*> Reference = Index(First);
	int32 Compared = 0;
	int32 Mismatches = 0;
	bool bDrewAfterRelease = false;
	for (const FLogged& Entry : WithTakeover)
	{
		if (Entry.Ordinal != TakenOver)
		{
			continue;
		}
		bDrewAfterRelease |= Entry.Step >= 500;
		if (const FLogged* const* Same = Reference.Find(FDrawKey(Entry.Ordinal, static_cast<uint16>(Entry.Decision), Entry.Index)))
		{
			++Compared;
			Mismatches += (*Same)->ValueBits != Entry.ValueBits ? 1 : 0;
		}
	}
	TestTrue(TEXT("the released pawn drew again after the release"), bDrewAfterRelease);
	TestTrue(TEXT("its draws were compared"), Compared > 10);
	TestEqual(TEXT("its k-th draws equal the no-takeover run's (counters keyed by ordinal)"), Mismatches, 0);

	// A draw and a takeover in the same tick: the store keeps the index whatever object asks.
	FCrowdDraws Store(FixtureSubSeed);
	const uint64 Before = Store.Draw(TakenOver, ECrowdDecision::Destination);
	const uint64 AfterHandover = Store.Draw(TakenOver, ECrowdDecision::Destination);
	TestEqual(TEXT("index 0 then 1 of Destination"), Store.GetCounters().Peek(TakenOver, ECrowdDecision::Destination), 2u);
	TestEqual(TEXT("the second draw is tuple index 1"), AfterHandover, Store.PeekTuple(TakenOver, ECrowdDecision::Destination, 1, 0));
	TestNotEqual(TEXT("index 0 and 1 differ"), Before, AfterHandover);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdNoRawRandomTest, "StealthGame.Crowd.NoRawRandom",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdNoRawRandomTest::RunTest(const FString& Parameters)
{
	using namespace CrowdDrawsTestData;
	// The build check: the forbidden-RNG rule (comments and strings excluded by its C++ lexer) over the real tree.
	const StealthCrowdTest::FScriptRun Run = StealthCrowdTest::RunBashScript(TEXT("tools/ci/lint/forbidden-rng.sh"));
	if (TestTrue(TEXT("the forbidden-RNG rule ran (bash found)"), Run.bLaunched))
	{
		const TArray<FString> CrowdScanned = StealthCrowdTest::LinesWith(Run.Output, TEXT("SCANNED Source/StealthGameplay/Features/Crowd/"));
		const TArray<FString> CrowdFindings = StealthCrowdTest::LinesWith(Run.Output, TEXT("Source/StealthGameplay/Features/Crowd/")).FilterByPredicate([](const FString& Line) { return !Line.StartsWith(TEXT("SCANNED")); });
		TestTrue(TEXT("the rule scanned the Crowd source tree"), CrowdScanned.Num() >= 2);
		TestEqual(TEXT("no FMath::Rand* / FRandomStream / std RNG in Crowd code"), CrowdFindings.Num(), 0);
		for (const FString& Finding : CrowdFindings)
		{
			AddError(Finding);
		}
	}

	// Forced retries: N just above 2^31 rejects about half of the first draws; retries must extend Retry only.
	FCrowdDraws Draws(FixtureSubSeed);
	constexpr uint32 N = 0x80000001u;
	int32 Retried = 0;
	int32 MultiRetried = 0;
	for (uint32 Call = 0; Call < 400; ++Call)
	{
		const uint32 IndexBefore = Draws.GetCounters().Peek(1, ECrowdDecision::DecoyCourt);
		const uint32 Value = Draws.DrawBelow(1, ECrowdDecision::DecoyCourt, N);
		const uint32 Retries = Draws.GetLastRetryCount();
		TestEqual(TEXT("one DrawBelow advances DecisionIndex by exactly one"), Draws.GetCounters().Peek(1, ECrowdDecision::DecoyCourt), IndexBefore + 1);
		// Recompute from the tuple with the same index and the final Retry.
		const uint64 M = static_cast<uint64>(static_cast<uint32>(Draws.PeekTuple(1, ECrowdDecision::DecoyCourt, IndexBefore, Retries))) * N;
		TestEqual(TEXT("the value is the tuple {Ordinal, Decision, Index, Retry} of the accepted retry"), Value, static_cast<uint32>(M >> 32));
		TestTrue(TEXT("the value is below N"), Value < N);
		Retried += Retries > 0 ? 1 : 0;
		MultiRetried += Retries > 1 ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("NORAWRANDOM retries: %d of 400 draws retried, %d more than once"), Retried, MultiRetried));
	TestTrue(TEXT("the fixture forced retries"), Retried > 100);
	TestTrue(TEXT("the fixture forced consecutive retries"), MultiRetried > 20);
	// Another decision type is untouched by those retries.
	TestEqual(TEXT("retries never touch another decision's counter"), Draws.GetCounters().Peek(1, ECrowdDecision::Destination), 0u);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
