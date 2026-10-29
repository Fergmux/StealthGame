// Story con-030 AC2 (production/epics/concealment-bushes/story-030-concealment-log-and-metric-calculators.md; GDD F1, F2,
// CON-R12, CON-R15): StealthGame.Concealment.MetricCalculators.
//   - the F1/F2 fixture (N_B 9, n_P 5, n_N 50, r_N 0.20/min, D_N 0.345 min, r_P 0.24/min, D_P 0.345 min, k 1.5):
//     E[O] = 0.429, m0 = 0.349, cap = 44.9 %, r_N,req = 0.152/min (= 0.633 r_P), each within +-0.001 (cap +-0.1 point);
//   - a log fixture of one 720 s round: a player with three concealments of 20 s, 25 s and 12.6 s (one ended by a kill)
//     and 50 bush strikes with candidates, 22 of them with >= 2: mean time concealed 57.6 s (8.0 %), r_P 0.25/min,
//     m = 44.0 %;
//   - boundaries: 29 and 30 strikes with candidates (inconclusive below 30); ambush shares 25.0 % and 25.1 % (flag above).
// Fixtures are factory functions in this file (no file I/O); the log fixture is built through ConcealmentLog::MakeEntry,
// the production row builder, and read back through Decode.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/ConcealmentMetrics.h"

namespace ConcealmentMetricsTestData
{
	using namespace ConcealmentMetrics;

	constexpr double Tolerance = 0.001;
	constexpr double RoundStart = 100.0;
	constexpr double RoundSec = 720.0;
	const FParticipantId Player(7);
	const FParticipantId Other(8);

	/** The GDD F1/F2 worked example. */
	FOccupancyInputs F1Fixture()
	{
		FOccupancyInputs In;
		In.NumBushes = 9.0;
		In.NpcCount = 50.0;
		In.NpcRatePerMin = 0.20;
		In.NpcDwellMin = 0.345;
		In.PlayerCount = 5.0;
		In.PlayerRatePerMin = 0.24;
		In.PlayerDwellMin = 0.345;
		return In;
	}
	constexpr double FixtureK = 1.5;

	FRoundLogEntry Row(double At, EConcealmentLogEvent Event, FParticipantId Who, uint32 D2 = 0, uint32 D3 = 0, uint32 D4 = 0)
	{
		static uint64 Tick = 1;
		return ConcealmentLog::MakeEntry(FQueueKey(Tick++, 0), At, Event, Who, 3, Who.IsValid(), D2, D3, D4);
	}

	/**
	 * One 720 s round: the player conceals 20 s (exit), 25 s (emerge), 12.6 s (killed in the bush); another player and an
	 * NPC conceal too (never counted for the player); StrikesWithCandidates bush strikes with candidates of which MultiCount
	 * held >= 2, plus empty-set strikes (never counted).
	 */
	TArray<FRoundLogEntry> RoundFixture(int32 StrikesWithCandidates, int32 MultiCount)
	{
		TArray<FRoundLogEntry> Log;
		Log.Add(Row(RoundStart + 30.0, EConcealmentLogEvent::Commit, Player));
		Log.Add(Row(RoundStart + 40.0, EConcealmentLogEvent::Commit, Other));
		Log.Add(Row(RoundStart + 50.0, EConcealmentLogEvent::ExitCross, Player));
		Log.Add(Row(RoundStart + 90.0, EConcealmentLogEvent::ExitCross, Other));
		Log.Add(Row(RoundStart + 200.0, EConcealmentLogEvent::Commit, Player));
		Log.Add(Row(RoundStart + 210.0, EConcealmentLogEvent::Commit, FParticipantId()));
		Log.Add(Row(RoundStart + 225.0, EConcealmentLogEvent::Emerge, Player, 0, 0, 1));
		Log.Add(Row(RoundStart + 400.0, EConcealmentLogEvent::Commit, Player));
		Log.Add(Row(RoundStart + 412.6, EConcealmentLogEvent::Killed, Player));
		for (int32 Index = 0; Index < StrikesWithCandidates; ++Index)
		{
			const uint32 After = Index < MultiCount ? 2u + static_cast<uint32>(Index % 3) : 1u;
			Log.Add(Row(RoundStart + 500.0 + Index, EConcealmentLogEvent::BushStrike, Other, After + 1, After));
		}
		for (int32 Index = 0; Index < 7; ++Index)
		{
			Log.Add(Row(RoundStart + 600.0 + Index, EConcealmentLogEvent::BushStrike, Other, Index % 2, 0)); // empty after reach
		}
		return Log;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentMetricCalculatorsTest, "StealthGame.Concealment.MetricCalculators",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentMetricCalculatorsTest::RunTest(const FString& Parameters)
{
	using namespace ConcealmentMetricsTestData;

	// F1 / F2 at the GDD fixture.
	const FOccupancyInputs In = F1Fixture();
	const double Lambda = ExpectedOccupancy(In);
	const double M0 = MultiOccupantBaseline(Lambda);
	const double Cap = ReviewCap(M0);
	const double RReq = RequiredNpcRate(In.PlayerRatePerMin, In.PlayerCount, In.NpcCount, FixtureK);
	TestEqual(TEXT("F1 E[O] = 0.429"), Lambda, 0.429, Tolerance);
	TestEqual(TEXT("F1 NPC term 0.383"), In.NpcCount * In.NpcRatePerMin * In.NpcDwellMin / In.NumBushes, 0.383, Tolerance);
	TestEqual(TEXT("m0 = 1 - e^-lambda = 0.349"), M0, 0.349, Tolerance);
	TestEqual(TEXT("review cap max(40 %, m0 + 10 points) = 44.9 %"), Cap * 100.0, 44.9, 0.1);
	TestEqual(TEXT("F2 r_N,req = 0.152/min"), RReq, 0.152, Tolerance);
	TestEqual(TEXT("F2 r_N,req = 0.633 r_P"), RReq / In.PlayerRatePerMin, 0.633, Tolerance);
	TestEqual(TEXT("the 40 % floor holds while m0 + 10 points is below it"), ReviewCap(0.25), 0.40, 1e-9);

	// The 720 s round fixture.
	const TArray<FRoundLogEntry> Entries = RoundFixture(50, 22);
	const TArray<FConcealmentLogRow> Rows = ConcealmentLog::DecodeAll(Entries);
	TestEqual(TEXT("every fixture entry decodes"), Rows.Num(), Entries.Num());
	const FPlayerConcealment P = SummarisePlayer(Rows, Player, RoundStart, RoundSec);
	TestEqual(TEXT("three entries"), P.Entries, 3);
	TestEqual(TEXT("time concealed 57.6 s"), P.TimeConcealedSec, 57.6, Tolerance);
	TestEqual(TEXT("8.0 % of round time"), P.ShareOfRound * 100.0, 8.0, 0.1);
	TestEqual(TEXT("r_P = 3 / 12 min = 0.25/min"), P.RatePerMin, 0.25, Tolerance);
	const FMultiOccupantShare M = MultiOccupantShare(Rows, M0);
	TestEqual(TEXT("50 strikes with candidates"), M.StrikesWithCandidates, 50);
	TestEqual(TEXT("m = 44.0 %"), M.Share * 100.0, 44.0, 0.1);
	TestTrue(TEXT("44.0 % is within the 44.9 % cap"), M.Verdict == EReviewVerdict::WithinCap);
	const FAmbushShare Ambush = AmbushShare(Rows, 4);
	TestTrue(TEXT("one emerge kill of 4 kills (25 %): within"), Ambush.EmergeKills == 1 && FMath::IsNearlyEqual(Ambush.Share, 0.25) && Ambush.Verdict == EReviewVerdict::WithinCap);
	// An open concealment at the round end runs to the end.
	{
		TArray<FRoundLogEntry> Open;
		Open.Add(Row(RoundStart + RoundSec - 10.0, EConcealmentLogEvent::Commit, Player));
		TestEqual(TEXT("an open concealment counts to the round end"), SummarisePlayer(ConcealmentLog::DecodeAll(Open), Player, RoundStart, RoundSec).TimeConcealedSec, 10.0, Tolerance);
	}

	// Boundaries: 29 strikes inconclusive, 30 decide; 25.0 % within, 25.1 % flagged.
	TestTrue(TEXT("29 strikes with candidates: inconclusive"), MultiOccupantShare(ConcealmentLog::DecodeAll(RoundFixture(29, 29)), M0).Verdict == EReviewVerdict::Inconclusive);
	const FMultiOccupantShare Thirty = MultiOccupantShare(ConcealmentLog::DecodeAll(RoundFixture(30, 15)), M0);
	TestTrue(TEXT("30 strikes with candidates: conclusive (50 % > 44.9 %: review)"), Thirty.StrikesWithCandidates == 30 && Thirty.Verdict == EReviewVerdict::Review);
	TestTrue(TEXT("ambush 25.0 %: within"), AmbushShareFromCounts(250, 1000).Verdict == EReviewVerdict::WithinCap);
	TestTrue(TEXT("ambush 25.1 %: review"), AmbushShareFromCounts(251, 1000).Verdict == EReviewVerdict::Review);
	TestTrue(TEXT("ambush with no kill: inconclusive"), AmbushShareFromCounts(0, 0).Verdict == EReviewVerdict::Inconclusive);
	return true;
}

#endif
