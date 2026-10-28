// Objectives Chain story oc-014 AC2 (AC-OC-10 [U]; satisfies AC-IVP-40): StealthGame.Objectives.RivalSignalDistribution.
// GIVEN 1,000 pinned round sub-seed families and scripted rounds that vary which team (and so which completer and court)
// produces the k-th preliminary completion (k = 1..4) THEN Delta is uniform on [30, 165] s (Kolmogorov-Smirnov p > 0.01, per
// team), Delta for ordinal k is identical whichever team produced it (the draw is keyed by the ordinal only: the same family
// under the mirrored script gives the same Delta for every k), and every release time of both teams lies on the one 30 s grid
// from Active. The rounds run through FObjectivesCore's counted-completion effect (the debug path's shared effect).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "ObjectivesRivalSignal.h"
#include "ObjectivesTestRig.h"

namespace RivalSignalDistributionTestData
{
	constexpr int32 Families = 1000;

	/** SplitMix64: the pinned sub-seed of (family, ordinal). */
	uint64 SubSeed(int32 Family, uint32 Ordinal)
	{
		uint64 Z = (static_cast<uint64>(Family) * 16ull + Ordinal + 1ull) * 0x9E3779B97F4A7C15ull;
		Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
		Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
		return Z ^ (Z >> 31);
	}

	class FFamilyDraw : public IObjectivesSecretDraw
	{
	public:
		int32 Family = 0;
		virtual uint64 GetStageSignalSubSeed(uint32 Ordinal) const override { return SubSeed(Family, Ordinal); }
	};

	/** The Kolmogorov-Smirnov p-value of Samples (ms) against the uniform law on [Lo, Hi]. */
	double KsPValue(TArray<double> Samples, double Lo, double Hi, double& OutD)
	{
		Samples.Sort();
		const double N = static_cast<double>(Samples.Num());
		double D = 0.0;
		for (int32 Index = 0; Index < Samples.Num(); ++Index)
		{
			const double F = FMath::Clamp((Samples[Index] - Lo) / (Hi - Lo), 0.0, 1.0);
			D = FMath::Max(D, FMath::Max((Index + 1) / N - F, F - Index / N));
		}
		OutD = D;
		const double Root = FMath::Sqrt(N);
		const double Lambda = (Root + 0.12 + 0.11 / Root) * D;
		double P = 0.0;
		for (int32 J = 1; J <= 100; ++J)
		{
			P += 2.0 * ((J % 2) == 1 ? 1.0 : -1.0) * FMath::Exp(-2.0 * J * J * Lambda * Lambda);
		}
		return FMath::Clamp(P, 0.0, 1.0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesRivalSignalDistributionTest, "StealthGame.Objectives.RivalSignalDistribution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesRivalSignalDistributionTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	using namespace RivalSignalDistributionTestData;
	FCoreRig Rig(/*bActive*/ false);
	FFamilyDraw Draw;
	FObjectivesCoreDeps Deps;
	Deps.Roster = &Rig.Roster;
	Deps.Session = &Rig.Session;
	Deps.PawnControl = &Rig.PawnControl;
	Deps.Round = &Rig.Round;
	Deps.Courts = &Rig.Courts;
	Deps.Views = &Rig.Views;
	Deps.RivalRelease = &Rig.Rival;
	Deps.SecretDraw = &Draw;
	Rig.Core.Initialize(Deps);
	const int64 T0Ms = FObjectivesRivalSignal::ToMs(ActivePhaseStart);
	const int64 QMs = Rig.Core.GetRivalQuantumMs();

	// Two mirrored scripts: the k-th prelim completer alternates 0,1,0,1 or 1,0,1,0; completions 7 s apart from Active + 5 s.
	TArray<double> DeltaByTeam[2];
	int32 OrdinalMismatches = 0;
	int32 OffGrid = 0;
	int32 Releases = 0;
	for (int32 Family = 0; Family < Families; ++Family)
	{
		Draw.Family = Family;
		int32 DeltaOf[2][5] = {};
		for (int32 Script = 0; Script < 2; ++Script)
		{
			Rig.Core.Bind(Rig.Chain.Get(), Rig.Assignment, 0);
			Rig.Round.bLatched = false;
			Rig.Core.HandleActiveEntry(RoundEpoch + Family * 2 + Script, ActivePhaseStart);
			Rig.Rival.Requests.Reset();
			for (int32 K = 1; K <= 4; ++K)
			{
				const int32 Team = ((K - 1) + Script) % 2;
				Rig.Round.Now = ActivePhaseStart + 5.0 + 7.0 * K + Family * 0.001;
				Rig.Core.ApplyDebugStageCompletion(Team, FQueueKey(10 + K, 1));
			}
			for (const FObjectivesRivalRelease& Release : Rig.Rival.Requests)
			{
				++Releases;
				DeltaOf[Script][Release.Ordinal] = Release.DeltaMs;
				DeltaByTeam[Release.CompletingTeam].Add(static_cast<double>(Release.DeltaMs));
				OffGrid += (Release.ReleaseMs - T0Ms) % QMs == 0 && Release.ReleaseMs >= Release.CompletedMs + Release.DeltaMs ? 0 : 1;
			}
		}
		for (int32 K = 1; K <= 4; ++K)
		{
			OrdinalMismatches += DeltaOf[0][K] == DeltaOf[1][K] && DeltaOf[0][K] >= 30000 && DeltaOf[0][K] <= 165000 ? 0 : 1;
		}
	}
	double D0 = 0.0;
	double D1 = 0.0;
	const double P0 = KsPValue(DeltaByTeam[0], 30000.0, 165000.0, D0);
	const double P1 = KsPValue(DeltaByTeam[1], 30000.0, 165000.0, D1);
	AddInfo(FString::Printf(TEXT("OC14 releases=%d team0 n=%d D=%.4f p=%.4f; team1 n=%d D=%.4f p=%.4f; ordinal mismatches=%d off-grid=%d"),
		Releases, DeltaByTeam[0].Num(), D0, P0, DeltaByTeam[1].Num(), D1, P1, OrdinalMismatches, OffGrid));
	TestEqual(TEXT("every family released 4 per script"), Releases, Families * 2 * 4);
	TestTrue(TEXT("team 0's delays are uniform on [30, 165] s (KS p > 0.01)"), P0 > 0.01);
	TestTrue(TEXT("team 1's delays are uniform on [30, 165] s (KS p > 0.01)"), P1 > 0.01);
	TestEqual(TEXT("Delta for ordinal k is the same whichever team produced it"), OrdinalMismatches, 0);
	TestEqual(TEXT("every release of both teams lies on the one 30 s grid from Active, at or after t_c + Delta"), OffGrid, 0);

	// Negative control: a non-uniform sample (all in the lower half) fails the KS test.
	TArray<double> Skewed;
	for (int32 Index = 0; Index < 1000; ++Index)
	{
		Skewed.Add(30000.0 + (Index % 500) * 135.0);
	}
	double DS = 0.0;
	TestTrue(TEXT("control: a skewed sample fails KS"), KsPValue(Skewed, 30000.0, 165000.0, DS) < 0.01);
	return true;
}

#endif
