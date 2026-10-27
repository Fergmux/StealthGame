// Story cnb-029 (production/epics/crowd-npc-behaviour/story-029-release-to-crowd.md): ReleaseToCrowd (GDD CNB-R10.4,
// R9.1 exile exit, AC-CNB-24; ADR-0016 sections 7 and 8). Headless (CrowdSimWorld, the SCL movement model at 1/60 s).
//   NoChangeAtRelease  a released restricted pawn: in the release frame and the next 10 frames |dv| per frame never exceeds
//                      the SCL walk acceleration x dt and the heading rate never exceeds the SCL turn rate; it stays
//                      restricted (no tell, no discrete command) until RestrictedUntilServerTime, joins the full catalogue
//                      at a leg boundary after it, then takes the next scheduled exit slot (AlleyExit-eligible while it
//                      waits and walks) and is absorbed.
//   ConcealedStay      a pawn released after t0 s in a bush leaves after a stay drawn from the NPC stay distribution
//                      conditioned on T > t0 (KS against the conditional log-normal, uncapped).
// The observer-client half of AC-CNB-24 (replicated velocity and ActionPose on a client) needs the multi-process
// harness (story cnb-034's nightly job); the reversal of TryReleaseNpc is StealthGame.Crowd.Informant.*.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdTestStats.h"
#include "Tests/Crowd/CrowdHerringRecorder.h"
#include "Tests/Crowd/CrowdSimWorld.h"
#include "View/Core/StealthLocomotionModel.h"

#include <cmath>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdReleaseNoChangeTest, "StealthGame.Crowd.Release.NoChangeAtRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdReleaseNoChangeTest::RunTest(const FString& Parameters)
{
	using namespace StealthCrowdSim;
	using namespace StealthCrowdHerringTest;
	FConfig Config;
	Config.HalfSizeCm = 3000.0;
	Config.EnabledTellMask = FConfig::AllTells;
	FWorld World(Config);
	for (int32 Index = 0; Index < 13; ++Index)
	{
		const double Angle = 2.0 * UE_DOUBLE_PI * Index / 13.0;
		const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.0);
		World.AddSocket(Out * 2700.0, Out * 2850.0);
	}
	FRecorder Recorder;
	World.SetObserver(&Recorder);
	World.SpawnNpcs();
	World.StartRound();
	const FLocomotionTuning_Public& Loco = World.GetLocomotionTuning();
	const double Accel = StealthLocomotion::WalkAcceleration(Loco);
	int32 Trials = 0, OverAccel = 0, OverTurn = 0, EarlyPromotions = 0, NotAtLegStart = 0, LockoutTells = 0, Absorbed = 0, AlleyWhileWaiting = 0;
	for (int32 Trial = 0; Trial < 10; ++Trial)
	{
		// A disconnected player's pawn walking under the restricted driver (taken over a few seconds ago).
		FNpc& Pawn = World.SpawnNpcAt(FVector(-1500.0 + 300.0 * Trial, -800.0 + 150.0 * Trial, 0.0), static_cast<float>(Trial * 31), ECrowdDriverKind::Restricted, Loco.WalkSpeedCmPerSec, EGait::Walk);
		const uint32 Ordinal = Pawn.Body.Ordinal;
		World.RunUntil(World.GetNow() + 4.0);
		++Trials;
		// Grace expiry: ReleaseToCrowd with the strike lockout (5 s) and the exile exit.
		const double Release = World.GetNow();
		const double Until = Release + 5.0;
		World.FindNpc(Ordinal)->Agent.ScheduleAdoption(Release, Until, true, false, 0.0, World);
		float LastSpeed = World.FindNpc(Ordinal)->Body.Motion.Speed;
		float LastYaw = World.FindNpc(Ordinal)->Body.Motion.FacingYawDeg;
		for (int32 Frame = 0; Frame <= 10; ++Frame)
		{
			World.Step();
			const FBody& Body = World.FindNpc(Ordinal)->Body;
			OverAccel += FMath::Abs(Body.Motion.Speed - LastSpeed) > Accel * Config.StepSec + 0.01 ? 1 : 0;
			const double TurnRate = Body.Motion.Speed > Loco.PivotSpeedCmPerSec ? Loco.WalkTurnRateDegPerSec : Loco.PivotTurnRateDegPerSec;
			OverTurn += FMath::Abs(FMath::FindDeltaAngleDegrees(LastYaw, Body.Motion.FacingYawDeg)) > TurnRate * Config.StepSec + 0.01 ? 1 : 0;
			LastSpeed = Body.Motion.Speed;
			LastYaw = Body.Motion.FacingYawDeg;
		}
		// Through the lockout and the promotion, to the exit.
		const double Limit = World.GetNow() + 120.0;
		bool bWaited = false;
		while (World.GetNow() < Limit && !World.FindNpc(Ordinal)->Body.bGone)
		{
			World.Step();
			const FCrowdAgent& Agent = World.FindNpc(Ordinal)->Agent;
			if (Agent.IsAwaitingExitSlot() || Agent.IsExiting())
			{
				bWaited = true;
			}
		}
		Absorbed += World.FindNpc(Ordinal)->Body.bGone ? 1 : 0;
		AlleyWhileWaiting += bWaited ? 1 : 0;
		const TPair<uint32, double>* Promotion = World.Promoted.FindByPredicate([Ordinal](const TPair<uint32, double>& P) { return P.Key == Ordinal; });
		if (Promotion == nullptr || Promotion->Value < Until - 1e-9)
		{
			++EarlyPromotions;
		}
		else
		{
			NotAtLegStart += Recorder.LegStarts.ContainsByPredicate([&](const TPair<uint32, double>& L) { return L.Key == Ordinal && FMath::Abs(L.Value - Promotion->Value) < 1e-9; }) ? 0 : 1;
		}
		for (const FTellEvent& Event : Recorder.Performed)
		{
			LockoutTells += Event.Ordinal == Ordinal && (Promotion == nullptr || Event.Time < Promotion->Value) ? 1 : 0;
		}
		for (const FCommandRecord& Record : World.GetCommandLog())
		{
			LockoutTells += Record.Ordinal == Ordinal && (Promotion == nullptr || Record.Time < Promotion->Value) ? 1 : 0;
		}
	}
	AddInfo(FString::Printf(TEXT("RELEASE %d releases: frames over the walk acceleration %d, over the turn rate %d, promotions before the lockout ended (or none) %d, not at a leg start %d, tells or discrete commands under the lockout %d, waited AlleyExit %d, absorbed through the next exit slot %d"),
		Trials, OverAccel, OverTurn, EarlyPromotions, NotAtLegStart, LockoutTells, AlleyWhileWaiting, Absorbed));
	TestEqual(TEXT("|dv| per frame within the walk acceleration in the release frame and the next 10"), OverAccel, 0);
	TestEqual(TEXT("heading rate within the SCL turn rate in the release frame and the next 10"), OverTurn, 0);
	TestEqual(TEXT("promoted only after RestrictedUntilServerTime"), EarlyPromotions, 0);
	TestEqual(TEXT("the full catalogue starts at a leg boundary"), NotAtLegStart, 0);
	TestEqual(TEXT("no tell or discrete command under the lockout"), LockoutTells, 0);
	TestEqual(TEXT("every exile body reports AlleyExit while it waits for and walks its exit"), AlleyWhileWaiting, Trials);
	TestTrue(TEXT("exile bodies take the next exit slots and are absorbed"), Absorbed >= Trials - 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdReleaseConcealedStayTest, "StealthGame.Crowd.Release.ConcealedStay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdReleaseConcealedStayTest::RunTest(const FString& Parameters)
{
	const FCrowdTuning_Server Tuning;
	const double Median = Tuning.CrowdBushDwellMedianSec;
	const double Sigma = Tuning.CrowdBushDwellSigma;
	const double Mu = FMath::Loge(Median);
	auto Cdf = [Mu, Sigma](double X) { return X <= 0.0 ? 0.0 : 0.5 * (1.0 + std::erf((FMath::Loge(X) - Mu) / (Sigma * 1.4142135623730951))); };
	int32 Fails = 0;
	FString Line;
	for (const double T0 : { 0.0, 10.0, 30.0, 90.0 })
	{
		TArray<double> Totals;
		const int32 N = 4000;
		for (int32 Index = 0; Index < N; ++Index)
		{
			const double U = (Index + 0.5) / N;
			Totals.Add(T0 + FCrowdAgent::ConditionedStaySec(U, T0, Median, Sigma));
		}
		const double F0 = Cdf(T0);
		double D = 0.0;
		const double P = StealthCrowdStats::KsPValue(Totals, [&Cdf, F0](double X) { return FMath::Clamp((Cdf(X) - F0) / (1.0 - F0), 0.0, 1.0); }, &D);
		double Max = 0.0, Min = 1.0e9;
		for (const double Total : Totals)
		{
			Max = FMath::Max(Max, Total);
			Min = FMath::Min(Min, Total);
		}
		Fails += (P > 0.01 && Min >= T0 - 1e-9) ? 0 : 1;
		Line += FString::Printf(TEXT(" t0 %.0f s: KS D %.4f p %.3f, min total %.2f, max %.1f;"), T0, D, P, Min, Max);
	}
	AddInfo(FString::Printf(TEXT("CONCEALEDSTAY%s"), *Line));
	TestEqual(TEXT("the remaining stay follows the log-normal conditioned on the time already concealed (uncapped)"), Fails, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
