// Story cnb-024 (production/epics/crowd-npc-behaviour/story-024-strike-stimulus-and-reactions.md): strike stimulus
// fan-out and outcome-blind reactions R8 (GDD CNB-R8, AC-CNB-18; ADR-0016 section 5). Headless scenes (CrowdSimWorld:
// line of sight open, the real FCrowdStrikeFanOut and the agents' reaction draws).
//   OutcomeBlind     500 scripted strikes each of kill-NPC, kill-player, whiff and reversal (a strike at an NPC, at a
//                    scripted player-like pawn, at an empty spot, at a scripted pawn that then stands still) in one fixed
//                    crowd: the reaction distribution does not differ by class (contingency chi-square p > 0.01). The
//                    stimulus carries no outcome, so this proves the crowd has nothing to branch on.
//   SharesAndLimits  4,000 strikes: stratified by distance (<= 6 m, > 6 m) every drawn R8 share within +-20 % relative of
//                    its target (panic runs and startle braces only <= 6 m; beyond, glances in their place); no reaction
//                    starts before the stimulus + CrowdReactDelayMinSec; <= CrowdStrikeResponderLimit responders a strike;
//                    no NPC responds twice within CrowdReactRefractorySec; no responder's glance targets the attacker
//                    afterwards; every performed panic run / startle brace is a T1 / T4 tell (downgrades are glances).
//   PriorityKeyed    the responder priority is the counter-based draw keyed by (ordinal, strike index): the same for a
//                    pawn whatever other draws happened, and it consumes no counter.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdTestStats.h"
#include "HAL/PlatformTime.h"
#include "Tests/Crowd/CrowdHerringRecorder.h"
#include "Tests/Crowd/CrowdSimWorld.h"

namespace CrowdStrikeReactionTestData
{
	using namespace StealthCrowdSim;
	using namespace StealthCrowdHerringTest;

	struct FReaction
	{
		uint32 Ordinal = 0;
		double Stimulus = 0.0;
		double Start = 0.0;
		ECrowdStrikeReaction Drawn = ECrowdStrikeReaction::None;
		ECrowdStrikeReaction Performed = ECrowdStrikeReaction::None;
		double DistanceCm = 0.0;
		int32 Class = 0;
	};

	struct FReactionRecorder : public FRecorder
	{
		TArray<FReaction> Reactions;
		int32 CurrentClass = 0;
		uint64 AttackerKey = 0;
		int32 AttackerGlances = 0;
		double RefractorySec = 8.0;
		TMap<uint32, double> RespondedAt;
		TMap<uint32, double> LastStimulus;
		int32 RefractoryBreaks = 0;
		virtual void OnStrikeReaction(uint32 Ordinal, double StimulusTime, double StartTime, ECrowdStrikeReaction Drawn, ECrowdStrikeReaction Done, double DistanceCm) override
		{
			Reactions.Add({ Ordinal, StimulusTime, StartTime, Drawn, Done, DistanceCm, CurrentClass });
			RespondedAt.Add(Ordinal, StimulusTime);
		}
		virtual void OnGlance(uint32 Ordinal, double Now, ECrowdGlanceKind Kind, uint64 TargetKey, double HoldSec) override
		{
			// A responder's glances for the refractory window after its reaction never target the attacker.
			const double* At = RespondedAt.Find(Ordinal);
			AttackerGlances += AttackerKey != 0 && TargetKey == AttackerKey && At != nullptr && Now - *At < RefractorySec ? 1 : 0;
		}
	};

	/** A two-way contingency chi-square p-value (rows x columns of counts). */
	inline double ContingencyP(const TArray<TArray<double>>& Table, double& OutStat)
	{
		const int32 Rows = Table.Num();
		const int32 Cols = Rows > 0 ? Table[0].Num() : 0;
		TArray<double> RowSum, ColSum;
		RowSum.SetNumZeroed(Rows);
		ColSum.SetNumZeroed(Cols);
		double Total = 0.0;
		for (int32 R = 0; R < Rows; ++R)
		{
			for (int32 C = 0; C < Cols; ++C)
			{
				RowSum[R] += Table[R][C];
				ColSum[C] += Table[R][C];
				Total += Table[R][C];
			}
		}
		OutStat = 0.0;
		int32 LiveCols = 0;
		for (int32 C = 0; C < Cols; ++C)
		{
			LiveCols += ColSum[C] > 0.0 ? 1 : 0;
			for (int32 R = 0; R < Rows; ++R)
			{
				const double Expected = RowSum[R] * ColSum[C] / FMath::Max(Total, 1.0);
				if (Expected > 0.0)
				{
					OutStat += FMath::Square(Table[R][C] - Expected) / Expected;
				}
			}
		}
		const int32 Dof = FMath::Max((Rows - 1) * (LiveCols - 1), 1);
		return StealthCrowdStats::GammaQ(0.5 * Dof, 0.5 * OutStat);
	}

	inline FConfig StrikeConfig(uint64 Seed)
	{
		FConfig Config;
		Config.HalfSizeCm = 2000.0;
		Config.SubSeed = Seed;
		// The crowd's ordinary herrings run (no clock is needed for R8; G6 as in the reference catalogue).
		return Config;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdStrikeOutcomeBlindTest, "StealthGame.Crowd.Strike.OutcomeBlind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdStrikeOutcomeBlindTest::RunTest(const FString& Parameters)
{
	using namespace CrowdStrikeReactionTestData;
	const double WallStart = FPlatformTime::Seconds();
	FWorld World(StrikeConfig(0x57A1E0001ull));
	FReactionRecorder Recorder;
	World.SetObserver(&Recorder);
	World.SpawnNpcs();
	// Two scripted player-like pawns walking through the crowd (strike targets and strikers of the player classes).
	const int32 Target = World.AddScripted(FVector(-600.0, 0.0, 0.0), 0.0f);
	const int32 Striker = World.AddScripted(FVector(600.0, 0.0, 0.0), 180.0f);
	World.GetScripted(Target).Body.MoveIntent = FVector2D(0.0, 0.6);
	World.GetScripted(Striker).Body.MoveIntent = FVector2D(0.0, 0.6);
	World.StartRound();
	World.RunUntil(10.0);
	const int32 PerClass = 500;
	for (int32 Index = 0; Index < PerClass * 4; ++Index)
	{
		const int32 Class = Index % 4;
		Recorder.CurrentClass = Class;
		const FBody& StrikerBody = World.GetScripted(Striker).Body;
		// Every class strikes beside an NPC of the crowd, so the four classes see the same spread of distances; only who
		// (if anyone) stands at the point differs, and the stimulus carries none of it.
		const FVector Base = World.GetNpc((Index * 7) % World.NumNpcs()).Body.Location;
		FVector Point = Base + FVector(80.0, 0.0, 0.0);
		switch (Class)
		{
		case 0: Point = Base; break;                                                          // kill-NPC (the NPC is the victim)
		case 1: World.GetScripted(Target).Body.Location = Point; break;                       // kill-player
		case 2: break;                                                                        // whiff (no one there)
		default: World.GetScripted(Target).Body.Location = Point; break;                      // reversal
		}
		World.EmitStrike(Point, StrikerBody.Key);
		World.RunUntil(World.GetNow() + 2.0);
		// Keep the scripted pawns inside the plane.
		for (const int32 Scripted : { Target, Striker })
		{
			FBody& Body = World.GetScripted(Scripted).Body;
			if (Body.Location.Size2D() > 1500.0)
			{
				Body.Control.Yaw = (-Body.Location).Rotation().Yaw;
			}
		}
	}
	TArray<TArray<double>> Table;
	Table.SetNum(4);
	for (TArray<double>& Row : Table)
	{
		Row.SetNumZeroed(static_cast<int32>(ECrowdStrikeReaction::Count));
	}
	for (const FReaction& Reaction : Recorder.Reactions)
	{
		Table[Reaction.Class][static_cast<int32>(Reaction.Drawn)] += 1.0;
	}
	double Stat = 0.0;
	const double P = ContingencyP(Table, Stat);
	AddInfo(FString::Printf(TEXT("OUTCOMEBLIND %d strikes x 4 classes (%.1f s wall): %d reactions, contingency chi2 %.2f p %.4f"),
		PerClass, FPlatformTime::Seconds() - WallStart, Recorder.Reactions.Num(), Stat, P));
	TestTrue(TEXT("reactions happen in every class"), Recorder.Reactions.Num() > 4 * PerClass);
	TestTrue(TEXT("the reaction distribution does not differ by strike outcome (chi-square p > 0.01)"), P > 0.01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdStrikeSharesTest, "StealthGame.Crowd.Strike.SharesAndLimits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdStrikeSharesTest::RunTest(const FString& Parameters)
{
	using namespace CrowdStrikeReactionTestData;
	const double WallStart = FPlatformTime::Seconds();
	FConfig Config = StrikeConfig(0x57A1E0002ull);
	Config.EnabledTellMask = FConfig::AllTells; // T1 / T4 budget accounting with every clock running
	FWorld World(Config);
	World.AddReferenceHerringSites();
	FReactionRecorder Recorder;
	World.SetObserver(&Recorder);
	World.SpawnNpcs();
	const int32 Attacker = World.AddScripted(FVector(0.0, 0.0, 0.0), 0.0f);
	World.GetScripted(Attacker).Body.MoveIntent = FVector2D(0.0, 1.0);
	Recorder.AttackerKey = World.GetScripted(Attacker).Body.Key;
	World.StartRound();
	World.RunUntil(10.0);
	const FCrowdTuning_Server& T = World.GetCrowdTuning();
	const int32 Strikes = 4000;
	int32 MaxResponders = 0;
	for (int32 Index = 0; Index < Strikes; ++Index)
	{
		FBody& Body = World.GetScripted(Attacker).Body;
		if (Body.Location.Size2D() > 1500.0)
		{
			Body.Control.Yaw = (-Body.Location).Rotation().Yaw + 20.0;
		}
		MaxResponders = FMath::Max(MaxResponders, World.EmitStrike(Body.Location + Body.Control.Vector() * 100.0, Body.Key));
		World.RunUntil(World.GetNow() + 1.0);
	}
	// Shares by stratum.
	const double Targets[] = { T.CrowdReactNonePct, T.CrowdReactGlancePct, T.CrowdReactPausePct, T.CrowdReactReroutePct, T.CrowdPanicRunPct, T.CrowdStartleBracePct };
	double Near[6] = {}, Far[6] = {};
	double NearN = 0, FarN = 0;
	int32 Early = 0, Refractory = 0, PanicPerformed = 0, StartlePerformed = 0, BadDowngrade = 0;
	TMap<uint32, double> Last;
	for (const FReaction& Reaction : Recorder.Reactions)
	{
		double* Bucket = Reaction.DistanceCm <= T.CrowdStartleRadiusCm ? Near : Far;
		(Reaction.DistanceCm <= T.CrowdStartleRadiusCm ? NearN : FarN) += 1.0;
		Bucket[static_cast<int32>(Reaction.Drawn)] += 1.0;
		Early += Reaction.Start < Reaction.Stimulus + T.CrowdReactDelayMinSec - 1e-9 ? 1 : 0;
		if (const double* Prev = Last.Find(Reaction.Ordinal); Prev != nullptr && Reaction.Stimulus - *Prev < T.CrowdReactRefractorySec)
		{
			++Refractory;
		}
		Last.Add(Reaction.Ordinal, Reaction.Stimulus);
		PanicPerformed += Reaction.Performed == ECrowdStrikeReaction::PanicRun ? 1 : 0;
		StartlePerformed += Reaction.Performed == ECrowdStrikeReaction::StartleBrace ? 1 : 0;
		BadDowngrade += Reaction.Performed != Reaction.Drawn && Reaction.Performed != ECrowdStrikeReaction::Glance ? 1 : 0;
	}
	FString Line;
	int32 OffShares = 0;
	for (int32 K = 0; K < 6; ++K)
	{
		const double NearShare = Near[K] / FMath::Max(NearN, 1.0);
		// Beyond 6 m the panic and startle shares fold into the glance share.
		const double FarTarget = K == 1 ? Targets[1] + Targets[4] + Targets[5] : (K >= 4 ? 0.0 : Targets[K]);
		const double FarShare = Far[K] / FMath::Max(FarN, 1.0);
		const bool bNear = FMath::Abs(NearShare * 100.0 - Targets[K]) <= 0.2 * Targets[K];
		const bool bFar = FarTarget > 0.0 ? FMath::Abs(FarShare * 100.0 - FarTarget) <= 0.2 * FarTarget : Far[K] == 0.0;
		OffShares += (bNear ? 0 : 1) + (bFar ? 0 : 1);
		Line += FString::Printf(TEXT(" %s near %.2f%% (target %.0f) far %.2f%% (target %.0f)%s;"), LexToString(static_cast<ECrowdStrikeReaction>(K)), NearShare * 100.0, Targets[K], FarShare * 100.0,
			FarTarget, bNear && bFar ? TEXT("") : TEXT(" MISS"));
	}
	const int32 RunTells = Recorder.CountPerformed(ECrowdTell::Run);
	const int32 BraceTells = Recorder.CountPerformed(ECrowdTell::Brace);
	AddInfo(FString::Printf(TEXT("STRIKESHARES %d strikes (%.1f s wall), %d reactions (near %.0f, far %.0f):%s early starts %d, max responders %d, refractory breaks %d, attacker glances after %d, panic runs %d / startles %d performed (T1 tells %d, T4 tells %d), non-glance downgrades %d"),
		Strikes, FPlatformTime::Seconds() - WallStart, Recorder.Reactions.Num(), NearN, FarN, *Line, Early, MaxResponders, Refractory, Recorder.AttackerGlances, PanicPerformed, StartlePerformed,
		RunTells, BraceTells, BadDowngrade));
	TestTrue(TEXT("enough reactions in each stratum"), NearN >= 1800 && FarN >= 2000);
	TestEqual(TEXT("every R8 share within +-20 % relative of its target in both strata (panic / startle only <= 6 m)"), OffShares, 0);
	TestEqual(TEXT("no reaction starts before the reaction-time minimum after the stimulus"), Early, 0);
	TestTrue(TEXT("at most CrowdStrikeResponderLimit responders a strike"), MaxResponders <= T.CrowdStrikeResponderLimit);
	TestEqual(TEXT("no NPC responds twice within CrowdReactRefractorySec"), Refractory, 0);
	TestEqual(TEXT("no responder's glance targets the attacker afterwards"), Recorder.AttackerGlances, 0);
	TestEqual(TEXT("a downgraded reaction is always a glance"), BadDowngrade, 0);
	TestTrue(TEXT("panic runs count as T1 tells and startle braces as T4 tells"), RunTells >= PanicPerformed && BraceTells >= StartlePerformed && PanicPerformed > 0 && StartlePerformed > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdStrikePriorityTest, "StealthGame.Crowd.Strike.PriorityKeyed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdStrikePriorityTest::RunTest(const FString& Parameters)
{
	FCrowdDraws A(0x1234ull), B(0x1234ull);
	// B takes unrelated draws, including StrikePriority draws of the same pawn, before reading the keyed priority.
	for (int32 Index = 0; Index < 17; ++Index)
	{
		B.DrawUnit(7, ECrowdDecision::StrikePriority);
		B.DrawUnit(7, ECrowdDecision::PauseLength);
	}
	int32 Different = 0;
	for (uint64 Strike = 0; Strike < 50; ++Strike)
	{
		Different += FCrowdStrikeFanOut::PriorityOf(A, 7, Strike) != FCrowdStrikeFanOut::PriorityOf(B, 7, Strike) ? 1 : 0;
	}
	const uint32 Before = A.GetCounters().Peek(7, ECrowdDecision::StrikePriority);
	FCrowdStrikeFanOut::PriorityOf(A, 7, 3);
	TestEqual(TEXT("the keyed priority does not depend on other draws"), Different, 0);
	TestEqual(TEXT("reading a priority consumes no counter"), A.GetCounters().Peek(7, ECrowdDecision::StrikePriority), Before);
	TestNotEqual(TEXT("different strikes give different priorities"), FCrowdStrikeFanOut::PriorityOf(A, 7, 1), FCrowdStrikeFanOut::PriorityOf(A, 7, 2));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
