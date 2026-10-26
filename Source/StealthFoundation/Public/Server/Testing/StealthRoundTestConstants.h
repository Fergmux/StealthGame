#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

#if WITH_STEALTH_TEST_SEAMS

/**
 * Round & Match Framework test constant file (GDD "Test seams (binding on implementation)", fixture sentence;
 * story rmf-005). Fixture literals that tests share live HERE and nowhere else: the RoundLogHeaderFields names,
 * the RoundTestSeeds secrets and the fixture code strings. StealthGame.Round.FixtureConstants scans the test
 * sources and fails when one of these literals appears outside this file.
 *
 * In Public/Server/Testing (not a module's Private/Tests) so StealthGameplay's Round tests can include it too
 * (RMF Exit A ruling 2026-10-25, decision log). Compiled only under WITH_STEALTH_TEST_SEAMS.
 *
 * Never print a secret: a test asserts on secret-derived values inside the test only and reports file and line,
 * never the value (ADR-0006 section 7).
 */
namespace RoundTestFixtures
{
	// --- RoundLogHeaderFields: the R19 / ADR-0002 section 8.9 header field list, in order -------------------------

	inline const TCHAR* const HeaderFieldRoundIndex = TEXT("RoundIndex");
	inline const TCHAR* const HeaderFieldRoundEpoch = TEXT("RoundEpoch");

	/** Every header field name (R19 order). FRoundLogHeader's X-macro field list must contain each of them. */
	inline const TCHAR* const RoundLogHeaderFields[] = {
		HeaderFieldRoundIndex,
		HeaderFieldRoundEpoch,
		TEXT("Seed"),
		TEXT("LayoutId"),
		TEXT("LayoutAttempt"),
		TEXT("LayoutHash"),
		TEXT("ProviderKind"),
		TEXT("LayoutFallbackLevel"),
		TEXT("PresetId"),
		TEXT("TuningHash"),
		TEXT("SchemaVersion"),
		TEXT("FallbackLevel"),
		TEXT("HotReloaded"),
		TEXT("TuningWarnings"),
		TEXT("DealDigest"),
		TEXT("AppearanceSet"),
		TEXT("AppearanceCounts"),
		TEXT("PoolBelowMvpFloor"),
		TEXT("TeamRosters"),
		TEXT("HostParticipantId"),
		TEXT("ZoneShares"),
		TEXT("BuildVersion"),
	};

	// --- RoundTestSeeds: fixed fixture secrets (128-bit, as two little-endian words) and match nonces -------------

	struct FRoundTestSecret
	{
		uint64 Word0;
		uint64 Word1;
		uint64 MatchNonce;
	};

	/** S1: the statistical fixture secret of stories rmf-002, rmf-006, rmf-007 (bounds are fixed for it). */
	inline constexpr FRoundTestSecret SecretS1 = { 0x5A17C3E9D24B8F61ull, 0x0C3D9A7E64F1B2A5ull, 0x4D2A91C7E35B08F6ull };
	/** S2: the second fixture secret (must give different sub-seeds from S1). */
	inline constexpr FRoundTestSecret SecretS2 = { 0x7B2E4F9A1C6D3E85ull, 0xE81F05B3A9C7D264ull, 0x91B6D3F0A2E4C857ull };
	/** S3: the leak-test secret of rmf-014 (sub-seeds derived from it must appear in no output). */
	inline constexpr FRoundTestSecret SecretS3 = { 0x2F8C6E1A9D3B7054ull, 0xB4E9072C5F1A8D63ull, 0x6A0D4C2E8F17B935ull };

	/** Every RoundTestSeeds secret (the fixture-constant scan bans each word, in any case). */
	inline constexpr FRoundTestSecret RoundTestSeeds[] = { SecretS1, SecretS2, SecretS3 };

	/** Fixed seed of the rmf-007 accumulated-time fixtures (a counter stream tag, not a secret). */
	inline constexpr uint64 AccumulatedTimeFixtureSeed = 0x0000000000C0FFEEull;

	// --- Fixture code strings: Start refusals (R5), Preparing abort codes (ADR-0002 section 8.6), EndReason (R19) --

	inline const TCHAR* const FixtureCodeStrings[] = {
		TEXT("NotHost"), TEXT("WrongPhase"), TEXT("TravelInProgress"), TEXT("ParticipantNotConnected"), TEXT("TeamsInvalid"),
		TEXT("TooSmall"), TEXT("Imbalance"), TEXT("Oversize"), TEXT("PresetInvalid"), TEXT("DeckInfeasible"), TEXT("SessionRefused"),
		TEXT("BadParams"), TEXT("TuningLoadFailed"), TEXT("TuningSnapshotFault"), TEXT("DeckInvalid"), TEXT("AllocatorMisuse"),
		TEXT("LayoutAborted"), TEXT("LayoutSeedMismatch"), TEXT("TeamAbsent"), TEXT("BindFailed"), TEXT("PlacementInvariant"),
		TEXT("PlacementInfeasible"), TEXT("NpcCountTooLow"), TEXT("PreparingWatchdog"),
		TEXT("ObjectiveWin"), TEXT("Elimination"), TEXT("ForfeitElimination"), TEXT("Aborted"), TEXT("Abandoned"),
		TEXT("KickedLastActive"), TEXT("PawnLost"), TEXT("PreparingFailed"), TEXT("SessionEnded"), TEXT("DebugForced"),
	};

	/** The Preparing abort code TDC's failure warning also names (ADR-0002 section 8.6; shared with the TDC tests). */
	inline const TCHAR* const AbortCodeTuningLoadFailed = TEXT("TuningLoadFailed");

	// --- Team layouts ---------------------------------------------------------------------------------------------

	struct FRoundTeamLayout
	{
		int32 Team0;
		int32 Team1;
	};

	inline constexpr FRoundTeamLayout Layout5v5 = { 5, 5 };
	inline constexpr FRoundTeamLayout Layout4v4 = { 4, 4 };
	inline constexpr FRoundTeamLayout Layout2v2 = { 2, 2 };

	// --- Exit B team and Start fixtures (rmf-018 .. rmf-028, D53 integration batch) ---------------------------------

	/** rmf-018: a new key joins the team of 1; 2/2 ties alternate by RoundsPlayedCount. */
	inline constexpr FRoundTeamLayout Layout2v1 = { 2, 1 };
	/** rmf-019: a Dormant key on the larger (3) side is reclaimed. */
	inline constexpr FRoundTeamLayout Layout3v2 = { 3, 2 };
	/** rmf-020: a swap into a full team (5) is refused; into 4 it succeeds. */
	inline constexpr FRoundTeamLayout Layout4v5 = { 4, 5 };
	inline constexpr FRoundTeamLayout Layout5v4 = { 5, 4 };
	inline constexpr FRoundTeamLayout Layout5v3 = { 5, 3 };
	inline constexpr FRoundTeamLayout Layout1v0 = { 1, 0 };
	/** rmf-022 Oversize: a fake Roster reports 3 on one team with TeamSize 2. */
	inline constexpr FRoundTeamLayout Layout3v2Oversize = { 3, 2 };

	/** TeamSize values of the fixtures (a real match is 5; 2 for the small and Oversize cases). */
	inline constexpr int32 FixtureTeamSize = 5;
	inline constexpr int32 SmallTeamSize = 2;
	/** rmf-021: the requested preset's TeamSize (differs from FixtureTeamSize). */
	inline constexpr int32 PresetTeamSize = 4;
	/** rmf-022: MinPlayersPerTeamCount 2 makes 2v1 TooSmall. */
	inline constexpr int32 MinPlayersTwo = 2;
	/** rmf-027: the Ended dwell before a held reset executes (GDD EndedMinDwellSec default). */
	inline constexpr double EndedMinDwellSec = 3.0;

	/** rmf-026 / rmf-027: the frozen R22 host command list (AC 59 names nine, dumplog included, host-gated alike). */
	inline const TCHAR* const RoundHostCommandNames[] = {
		TEXT("stealth.round.start"), TEXT("stealth.round.reset"), TEXT("stealth.round.abort"), TEXT("stealth.round.swapteam"),
		TEXT("stealth.round.remove"), TEXT("stealth.round.preset"), TEXT("stealth.round.debugwin"), TEXT("stealth.round.debugeliminate"),
		TEXT("stealth.round.dumplog"),
	};

	// --- Queue fixtures (rmf-006 .. rmf-010) ------------------------------------------------------------------------

	/** rmf-006: ticks per statistical run and the fixed acceptance bounds for S1 (GDD AC 14). */
	inline constexpr int32 FairnessTicks = 10000;
	inline constexpr int32 TwoMailboxFirstMin = 4835;
	inline constexpr int32 TwoMailboxFirstMax = 5165;
	inline constexpr int32 ElevenMailboxFirstMin = 814;
	inline constexpr int32 ElevenMailboxFirstMax = 1004;

	/** rmf-006 / rmf-007: the participant ids of the fixture sources (server-local is the eleventh mailbox). */
	inline constexpr uint32 FixtureParticipantIds[] = { 3, 7, 11, 12, 20, 21, 30, 41, 42, 50 };

	/** rmf-007: a 3.0 s hold begun on tick 0 at 60 Hz is due on tick 180. */
	inline constexpr double HoldDurationSec = 3.0;
	inline constexpr int32 HoldFrameRateHz = 60;
	inline constexpr uint64 HoldDueTick = 180;
	inline constexpr int32 AccumulatedTimeFixtureCount = 10000;
	/** rmf-007: frame-delta jitter of the accumulated-time fixtures, in microseconds either side of 1/60 s. */
	inline constexpr int32 FrameJitterMicroseconds = 2;

	/** rmf-008: the fixture sequence of expected keys (tick, Seq) for three items over two drains. */
	inline constexpr uint64 ResolverContractFirstTick = 100;
	inline constexpr uint64 ResolverContractExpectedKeys[][2] = { { 100, 1 }, { 100, 2 }, { 101, 3 } };

	/** The epoch of fixture rounds (MatchId, RoundEpoch); rmf-010 advances RoundEpoch by one for ResetAll. */
	inline constexpr uint64 FixtureMatchId = 0x00000000AB5E0001ull;
	inline constexpr uint32 FixtureRoundEpoch = 4;

	// --- Event Log fixtures (rmf-011 .. rmf-017) --------------------------------------------------------------------

	/** rmf-012: ordinary entries, cap, and the expected truncation (GDD AC 33). */
	inline constexpr int32 TruncationOrdinaryEntries = 4999;
	inline constexpr int32 TruncationCap = 4096;
	inline constexpr int32 TruncationExpectedCount = 904;

	/** rmf-013: the scripted round's entry count, the F8 estimate range and the clamp fixture. */
	inline constexpr int32 SizeScriptedEntries = 300;
	inline constexpr int32 F8EntryBytesMin = 64;
	inline constexpr int32 F8EntryBytesMax = 96;
	inline constexpr int32 AboveClampMaxEntries = 50000;
	inline constexpr int32 ClampedMaxEntries = 16384;
	inline constexpr int32 ClampFloodEntries = 20000;

	/** rmf-014: sentinel ids and hashes that must never surface in any captured output. */
	inline constexpr uint32 SentinelParticipantIds[] = { 0x5E7100A1u, 0x5E7100B2u, 0x5E7100C3u, 0x5E7100D4u };
	inline constexpr uint64 SentinelTuningHash = 0x5E71AB12CD34EF56ull;
	inline constexpr uint64 SentinelLayoutHash = 0x5E71FE98DC76BA54ull;
	inline constexpr uint64 SentinelDealDigest = 0x5E7101234ABCDEF9ull;

	/** rmf-015: the flood rate and duration (GDD AC 61). */
	inline constexpr int32 FloodCommandsPerSecond = 100;
	inline constexpr int32 FloodSeconds = 10;

	/** rmf-016: F7 tuning inputs and the fixture durations (GDD AC 35). */
	inline constexpr int32 TargetRoundMinutes = 12;
	inline constexpr int32 StallFlagMinutes = 18;
	inline constexpr double PacingDurationSec = 936.0;
	inline constexpr double PacingExpectedRatio = 1.3;
	inline constexpr double StallBoundarySec = 1080.0;
	inline constexpr double StallJustAboveSec = 1080.001;
	inline constexpr double AbortBoundarySec = 540.0;
	inline constexpr double AbortJustAboveSec = 540.001;
	inline constexpr int32 ObjectiveWinRounds = 7;
	inline constexpr int32 EliminationRounds = 3;
	inline constexpr int32 AbortRounds = 2;
	inline constexpr double ExpectedObjectiveEndShare = 0.7;
	inline constexpr int32 Pillar4MinRounds = 20;
	inline constexpr double Pillar4MinShare = 0.70;

	/** rmf-017: the final objective's court socket and the two completers. */
	inline constexpr uint16 FinalCourtSocketId = 9;
	inline constexpr uint32 FinalCompleterA = 3;
	inline constexpr uint32 FinalCompleterB = 7;

	// --- Exit B Preparing, spawn and win fixtures (rmf-030 .. rmf-047) -------------------------------------------------

	/** The NPC count of the Preparing rig rounds (small, so a spawn list stays readable). */
	inline constexpr int32 PreparingRigNpcCount = 4;
	/** The public round seed of the Preparing rig, and a different one (LayoutSeedMismatch). */
	inline constexpr int32 PreparingRigSeed = 424242;
	inline constexpr int32 PreparingRigOtherSeed = 424243;
	/** The tuning hash TDC reports at freeze in the rig. */
	inline constexpr uint64 PreparingRigTuningHash = 0x00000000000F1CE5ull;
	/** The scripted SpawnOrder sub-seed the fake Session answers (a fixture value, never a real secret). */
	inline constexpr uint64 PreparingRigSpawnSubSeed = 0x0000000000005EEDull;
	/** One server frame of the rig clock. */
	inline constexpr double RigFrameSec = 1.0 / 60.0;

	/** rmf-035: GDD F6 fixture inputs (the F6 variable table) and the expected values. */
	inline constexpr double F6TravelLoadTimeoutSec = 60.0;
	inline constexpr double F6TravelLoadTimeoutLongSec = 120.0;
	inline constexpr double F6TravelAckGraceSec = 15.0;
	inline constexpr double F6ServerLoadBoundSec = 20.0;
	inline constexpr double F6LayoutTotalBudgetSec = 45.0;
	inline constexpr double F6TuningFreezeBoundSec = 1.0;
	inline constexpr double F6SpawnBoundSec = 1.0;
	inline constexpr double F6WorldReadyTimeoutSec = 10.0;
	inline constexpr double F6StartLeadSec = 0.25;
	inline constexpr double F6PreparingWatchdogSec = 150.0;
	inline constexpr double F6ExpectedNeededDefaultSec = 122.25;
	inline constexpr double F6ExpectedNeededLongSec = 161.25;
	inline constexpr double F6BoundaryOffsetSec = 0.01;
	/** Half the last printed digit of the 0.01 boundary. */
	inline constexpr double F6Tolerance = 0.005;

	/** rmf-036: D = 10 (5v5, no NPCs), 1,000 starts, chi-square critical value for 9 dof at alpha 0.001. */
	inline constexpr int32 SpawnOrderD = 10;
	inline constexpr int32 SpawnOrderStarts = 1000;
	inline constexpr double SpawnOrderChiSquareCritical = 27.88;

	/** rmf-038: placement fixtures and the expected F5 values (tolerance half the last printed digit). */
	inline constexpr int32 PlacementPlayers = 5;
	inline constexpr int32 PlacementNpcsPass = 5;
	inline constexpr int32 PlacementNpcsFail = 4;
	inline constexpr int32 GlobalShareNpcCount = 50;
	inline constexpr double GlobalShare5v5 = 0.167;
	inline constexpr double GlobalShare2v2 = 0.074;
	inline constexpr double GlobalShareTolerance = 0.0005;

	/** rmf-040: F4 inputs (DisconnectGraceSec 30, RejoinWindowSec 120: G_eff 30) and the post-start disconnect offset. */
	inline constexpr double GraceDisconnectSec = 30.0;
	inline constexpr double GraceRejoinWindowSec = 120.0;
	inline constexpr double GraceExpectedEffectiveSec = 30.0;
	inline constexpr double GraceDisconnectedBeforeStartSec = 5.0;
	inline constexpr double GraceDisconnectAfterStartSec = 10.0;

	/** rmf-043: the interleaving property fixture (seed of the test-local PCG32 stream, counts). */
	inline constexpr uint64 InterleavingStreamSeed = 0x00000000DEC1DE43ull;
	inline constexpr int32 InterleavingRuns = 100000;
	inline constexpr int32 InterleavingLength = 12;
	inline constexpr int32 InterleavingTeamSize = 2;

	/** rmf-047: the clock jump of the 1v0 round, in multiples of TargetRoundMinutes. */
	inline constexpr int32 EmptyTeamClockJumpMultiple = 10;
}

#endif // WITH_STEALTH_TEST_SEAMS
