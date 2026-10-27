// Story cnb-020 (production/epics/crowd-npc-behaviour/story-020-decoy-tending-herring.md): decoy tending (T3) (GDD
// CNB-R7 decoy bullet, R16 T3, AC-CNB-14; ADR-0020). Headless accelerated scenes (CrowdSimWorld with a 14-court fixture
// set standing in for the Objectives court fixtures, which the objectives-chain epic has not built yet; the sim's
// fixture refuses a fifth interactor and exposes a NotNow hook).
//   CourtChoiceUniform  >= 2,000 tends over 50 court layouts (the decoy clock accelerated to one per 30 s per NPC; the
//                       choice rule does not depend on the rate): chi-square of the court index against uniform p > 0.01,
//                       and of the court category at choice time (live / decoy / used / currently tended; labels the crowd
//                       never sees) against the category shares p > 0.01.
//   HoldsAndRefusal     75 +-5 % of holds last exactly ObjectiveInteractSec, every early release in [Min, Max]; every
//                       Begin follows an Interact dry run; a Begin refused NotNow is followed by a different court.
//   FullLengthPosterior the T3 posterior P_full = R_full / (R_full + N c r_N) at Objectives' R_full 0.40 (F3 at T_round 9
//                       min) from the measured completion share c and the accepted rate lies in 0.65-0.75 (AC-OC-21).
//                       Objectives F3 is not built yet: the formula is the GDD F2 decoy row's (rewired when it lands).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdTestStats.h"
#include "HAL/PlatformTime.h"
#include "Tests/Crowd/CrowdHerringRecorder.h"
#include "Tests/Crowd/CrowdSimWorld.h"

namespace CrowdDecoyTendTestData
{
	using namespace StealthCrowdSim;
	using namespace StealthCrowdHerringTest;

	/** The test's court labels (the crowd never sees them): 2 live, 6 decoy, 6 used. */
	enum class ECourtLabel : uint8 { Live, Decoy, Used, Count };
	inline ECourtLabel LabelOf(int32 Court) { return Court < 2 ? ECourtLabel::Live : (Court < 8 ? ECourtLabel::Decoy : ECourtLabel::Used); }

	/** Records each choice with the category of every court at that instant (currently tended overrides the label). */
	struct FChoiceRecorder : public FRecorder
	{
		FWorld* World = nullptr;
		TArray<double> CourtCounts;
		double Observed[4] = {};
		double Expected[4] = {};
		int32 Choices = 0;
		virtual void OnDecoyCourtChosen(uint32 Ordinal, double Now, int32 Court, int32 ExcludedCourt) override
		{
			FRecorder::OnDecoyCourtChosen(Ordinal, Now, Court, ExcludedCourt);
			if (ExcludedCourt != INDEX_NONE)
			{
				return; // a re-draw after a refusal is uniform over the other courts (tested in HoldsAndRefusal)
			}
			const TArray<FCourtFixture>& Courts = World->GetCourts();
			CourtCounts.SetNumZeroed(Courts.Num());
			CourtCounts[Court] += 1.0;
			auto Category = [&Courts](int32 Index) { return Courts[Index].Interactors > 0 ? 3 : static_cast<int32>(LabelOf(Index)); };
			for (int32 Index = 0; Index < Courts.Num(); ++Index)
			{
				Expected[Category(Index)] += 1.0 / Courts.Num();
			}
			Observed[Category(Court)] += 1.0;
			++Choices;
		}
	};

	/** A court layout drawn from the layout seed (a fixed deterministic scatter over the plane). */
	inline void AddLayout(FWorld& World, uint64 LayoutSeed, double HalfCm)
	{
		uint64 State = LayoutSeed * 0x9E3779B97F4A7C15ull + 1;
		auto Next = [&State]() { State ^= State << 13; State ^= State >> 7; State ^= State << 17; return static_cast<double>(State >> 11) / 9007199254740992.0; };
		for (int32 Index = 0; Index < 14; ++Index)
		{
			World.AddCourt(FVector((Next() * 2.0 - 1.0) * HalfCm * 0.85, (Next() * 2.0 - 1.0) * HalfCm * 0.85, 0.0));
		}
	}

	inline FConfig DecoyConfig(uint64 Seed)
	{
		FConfig Config;
		Config.HalfSizeCm = 3000.0;
		Config.SubSeed = Seed;
		Config.EnabledTellMask = 1u << static_cast<uint32>(ECrowdTell::DecoyTend);
		Config.Tuning.CrowdDecoyTendIntervalSec = 30.0f; // accelerated (the choice rule does not depend on the rate)
		return Config;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdDecoyCourtChoiceTest, "StealthGame.Crowd.Decoy.CourtChoiceUniform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdDecoyCourtChoiceTest::RunTest(const FString& Parameters)
{
	using namespace CrowdDecoyTendTestData;
	const double WallStart = FPlatformTime::Seconds();
	TArray<double> CourtObserved;
	CourtObserved.SetNumZeroed(14);
	double CatObserved[4] = {}, CatExpected[4] = {};
	int32 Choices = 0, Holds = 0, Complete = 0, BadComplete = 0, BadEarly = 0, BeginsWithoutDryRun = 0;
	const int32 Layouts = 50;
	for (int32 Layout = 0; Layout < Layouts; ++Layout)
	{
		FWorld World(DecoyConfig(0xDEC0700000ull + Layout));
		AddLayout(World, 0xC0u + Layout, 3000.0);
		FChoiceRecorder Recorder;
		Recorder.World = &World;
		World.SetObserver(&Recorder);
		World.SpawnNpcs();
		World.StartRound();
		World.RunUntil(2.0 * 60.0);
		for (int32 Court = 0; Court < 14; ++Court)
		{
			CourtObserved[Court] += Recorder.CourtCounts.IsValidIndex(Court) ? Recorder.CourtCounts[Court] : 0.0;
		}
		for (int32 Cat = 0; Cat < 4; ++Cat)
		{
			CatObserved[Cat] += Recorder.Observed[Cat];
			CatExpected[Cat] += Recorder.Expected[Cat];
		}
		Choices += Recorder.Choices;
		const double Interact = World.GetActionTimings().ObjectiveInteractSec;
		const FCrowdTuning_Server& Tuning = World.GetCrowdTuning();
		for (const FDecoyHold& Hold : Recorder.DecoyHolds)
		{
			++Holds;
			Complete += Hold.bComplete ? 1 : 0;
			BadComplete += Hold.bComplete && Hold.HoldSec != Interact ? 1 : 0;
			BadEarly += !Hold.bComplete && (Hold.HoldSec < Tuning.CrowdDecoyEarlyReleaseMinSec || Hold.HoldSec > Tuning.CrowdDecoyEarlyReleaseMaxSec) ? 1 : 0;
		}
		const TArray<FCommandRecord>& Log = World.GetCommandLog();
		for (int32 Index = 0; Index < Log.Num(); ++Index)
		{
			if (Log[Index].Kind != FCommandRecord::EKind::InteractBegin)
			{
				continue;
			}
			bool bDryRun = false;
			for (int32 Back = Index - 1; Back >= 0 && Log[Back].Time >= Log[Index].Time - 1e-9; --Back)
			{
				bDryRun |= Log[Back].Kind == FCommandRecord::EKind::DryRun && Log[Back].Ordinal == Log[Index].Ordinal && Log[Back].Arg == static_cast<int32>(EActionType::Interact) && Log[Back].bAccepted;
			}
			BeginsWithoutDryRun += bDryRun ? 0 : 1;
		}
	}
	TArray<double> UniformExpected;
	UniformExpected.Init(Choices / 14.0, 14);
	double CourtStat = 0.0, CatStat = 0.0;
	const double CourtP = StealthCrowdStats::ChiSquarePValue(CourtObserved, UniformExpected, &CourtStat);
	const double CatP = StealthCrowdStats::ChiSquarePValue(MakeArrayView(CatObserved, 4), MakeArrayView(CatExpected, 4), &CatStat);
	const double CompleteShare = Holds > 0 ? static_cast<double>(Complete) / Holds : 0.0;
	AddInfo(FString::Printf(TEXT("DECOY %d choices over %d layouts (%.1f s wall): court chi2 %.2f p %.4f; category observed live %.0f decoy %.0f used %.0f tended %.0f vs expected %.1f / %.1f / %.1f / %.1f, chi2 %.2f p %.4f; holds %d, complete share %.4f, bad complete %d, bad early %d, Begins without a dry run %d"),
		Choices, Layouts, FPlatformTime::Seconds() - WallStart, CourtStat, CourtP, CatObserved[0], CatObserved[1], CatObserved[2], CatObserved[3],
		CatExpected[0], CatExpected[1], CatExpected[2], CatExpected[3], CatStat, CatP, Holds, CompleteShare, BadComplete, BadEarly, BeginsWithoutDryRun));
	TestTrue(TEXT("at least 2,000 decoy court choices"), Choices >= 2000);
	TestTrue(TEXT("court choice is uniform over all courts (chi-square p > 0.01)"), CourtP > 0.01);
	TestTrue(TEXT("live / decoy / used / currently-tended courts are chosen in proportion (chi-square p > 0.01)"), CatP > 0.01);
	TestTrue(TEXT("75 +-5 % of holds run to completion"), FMath::Abs(CompleteShare - 0.75) <= 0.05);
	TestEqual(TEXT("every completed hold lasts exactly ObjectiveInteractSec"), BadComplete, 0);
	TestEqual(TEXT("every early release lies in [CrowdDecoyEarlyReleaseMinSec, MaxSec]"), BadEarly, 0);
	TestEqual(TEXT("every Interact(Begin) follows an Interact dry run in the same tick"), BeginsWithoutDryRun, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdDecoyRefusalTest, "StealthGame.Crowd.Decoy.NotNowChoosesAnotherCourt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdDecoyRefusalTest::RunTest(const FString& Parameters)
{
	using namespace CrowdDecoyTendTestData;
	FConfig Config = DecoyConfig(0xDEC07A1ull);
	Config.NpcCount = 8;
	FWorld World(Config);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const int32 Court = World.AddCourt(FVector(600.0 * Index - 900.0, 800.0, 0.0));
		World.GetCourts()[Court].RefuseNextBegins = 6; // each court refuses its first six Begins (filled after the dry run)
	}
	FRecorder Recorder;
	World.SetObserver(&Recorder);
	World.SpawnNpcs();
	World.StartRound();
	World.RunUntil(10.0 * 60.0);
	int32 Refusals = 0, FollowedByOther = 0, FollowedBySame = 0, Unfollowed = 0;
	for (const FDecoyChoice& Refusal : Recorder.DecoyRefusals)
	{
		++Refusals;
		const FDecoyChoice* Next = Recorder.DecoyChoices.FindByPredicate([&](const FDecoyChoice& C) { return C.Ordinal == Refusal.Ordinal && C.Time >= Refusal.Time - 1e-9 && C.Excluded == Refusal.Court; });
		if (Next == nullptr)
		{
			++Unfollowed; // a refusal past the attempt limit gives up the tend (counted dropped)
			continue;
		}
		FollowedByOther += Next->Court != Refusal.Court ? 1 : 0;
		FollowedBySame += Next->Court == Refusal.Court ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("DECOYREFUSAL %d NotNow refusals: followed by a different court %d, by the same court %d, given up %d; holds %d"),
		Refusals, FollowedByOther, FollowedBySame, Unfollowed, Recorder.DecoyHolds.Num()));
	TestTrue(TEXT("Begins were refused NotNow"), Refusals >= 10);
	TestEqual(TEXT("a refused court is never the next choice"), FollowedBySame, 0);
	TestTrue(TEXT("refusals are followed by a different court choice"), FollowedByOther >= Refusals - Unfollowed && FollowedByOther > 0);
	TestTrue(TEXT("tends still happen once courts accept"), Recorder.DecoyHolds.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdDecoyPosteriorTest, "StealthGame.Crowd.Decoy.FullLengthPosterior",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdDecoyPosteriorTest::RunTest(const FString& Parameters)
{
	using namespace CrowdDecoyTendTestData;
	// The completion share c measured over an accelerated run; the accepted per-NPC rate is the default key's (its city
	// rate is gated at 600 simulated minutes by StealthGame.Crowd.TellCalibration.TellBaselineNonZero, story cnb-023).
	FWorld World(DecoyConfig(0xDEC0FF11ull));
	AddLayout(World, 0xF0u, 3000.0);
	FRecorder Recorder;
	World.SetObserver(&Recorder);
	World.SpawnNpcs();
	World.StartRound();
	World.RunUntil(8.0 * 60.0);
	int32 Complete = 0;
	for (const FDecoyHold& Hold : Recorder.DecoyHolds)
	{
		Complete += Hold.bComplete ? 1 : 0;
	}
	const double C = Recorder.DecoyHolds.Num() > 0 ? static_cast<double>(Complete) / Recorder.DecoyHolds.Num() : 0.0;
	const FCrowdTuning_Server Defaults;
	const double RN = 60.0 / Defaults.CrowdDecoyTendIntervalSec;
	const double RFull = 0.40; // Objectives R_full at T_round 9 min (F3)
	const double N = Defaults.NpcCount;
	const double PFull = RFull / (RFull + N * C * RN);
	AddInfo(FString::Printf(TEXT("DECOYPOSTERIOR c %.4f over %d holds, r_N %.5f /min, N %.0f, R_full %.2f: P_full %.4f"), C, Recorder.DecoyHolds.Num(), RN, N, RFull, PFull));
	TestTrue(TEXT("enough holds to measure c"), Recorder.DecoyHolds.Num() >= 400);
	TestTrue(TEXT("the full-length T3 posterior lies in 0.65-0.75 (AC-OC-21)"), PFull >= 0.65 && PFull <= 0.75);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
