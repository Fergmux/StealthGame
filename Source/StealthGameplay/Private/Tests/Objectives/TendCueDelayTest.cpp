// Objectives Chain story oc-016 AC1 (AC-OC-32 ring clause [U]; OC-6): StealthGame.Objectives.TendCueDelay.
// GIVEN the defaults (TendCueDelaySec 1.0) THEN RingIsMarigold is false (Bone) until Begin + 1.0 s and true from Begin + 1.0 s
// (+- 1 frame, at 60 and at 30 fps) ONLY at an own live site; never at a non-live court, never for an informant; a tend
// cancelled before 1.0 s resets the ring (the next hold starts Bone again). The visual check is story oc-020's screenshots.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "View/ObjectiveViewHelpers.h"
#include "View/StealthTuningPublicSubstructs.h"

namespace TendCueDelayTestData
{
	/** A Stage1 view whose one live site is Court 11. */
	FObjectiveTeamView OwnView()
	{
		FObjectiveTeamView View;
		View.RoundEpoch = 3;
		View.ChainSerial = 1;
		View.StageIndex = EObjectiveTeamStage::Stage1;
		FObjectiveLiveSite& Site = View.LiveSites.AddDefaulted_GetRef();
		Site.Court = FCourtFixtureId(11);
		Site.DefIndex = 0;
		return View;
	}

	/** The first frame (from Begin at Start) on which the ring is Marigold, stepping Fps; -1 if never within 3 s. */
	double FirstMarigold(const FObjectiveTeamView& View, FCourtFixtureId Court, double Start, double Fps, double Delay, bool bInformant)
	{
		for (int32 Frame = 0; Frame <= static_cast<int32>(3.0 * Fps); ++Frame)
		{
			const double Now = Start + Frame / Fps;
			if (ObjectiveViewHelpers::RingIsMarigold(View, Court, Start, Now, Delay, bInformant))
			{
				return Now - Start;
			}
		}
		return -1.0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesTendCueDelayTest, "StealthGame.Objectives.TendCueDelay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesTendCueDelayTest::RunTest(const FString& Parameters)
{
	using namespace TendCueDelayTestData;
	const double Delay = FObjectivesTuning_Public().TendCueDelaySec; // read once per hold at Begin
	TestEqual(TEXT("the default TendCueDelaySec is 1.0"), Delay, 1.0);
	const FObjectiveTeamView View = OwnView();
	const double Start = 1234.5;
	for (const double Fps : { 60.0, 30.0 })
	{
		const double First = FirstMarigold(View, FCourtFixtureId(11), Start, Fps, Delay, false);
		AddInfo(FString::Printf(TEXT("OC16 own live site at %.0f fps: Marigold from %.4f s after Begin"), Fps, First));
		TestTrue(*FString::Printf(TEXT("%.0f fps: Marigold at Begin + 1.0 s (+- 1 frame)"), Fps), First >= 0.0 && FMath::Abs(First - Delay) <= 1.0 / Fps + 1e-9);
		TestFalse(*FString::Printf(TEXT("%.0f fps: Bone one frame before"), Fps), ObjectiveViewHelpers::RingIsMarigold(View, FCourtFixtureId(11), Start, Start + Delay - 1.0 / Fps, Delay, false));
		TestEqual(*FString::Printf(TEXT("%.0f fps: never at a non-live court"), Fps), FirstMarigold(View, FCourtFixtureId(12), Start, Fps, Delay, false), -1.0);
		TestEqual(*FString::Printf(TEXT("%.0f fps: never for an informant"), Fps), FirstMarigold(View, FCourtFixtureId(11), Start, Fps, Delay, true), -1.0);
	}
	TestFalse(TEXT("never with an empty view"), ObjectiveViewHelpers::RingIsMarigold(FObjectiveTeamView(), FCourtFixtureId(11), Start, Start + 5.0, Delay, false));
	TestFalse(TEXT("never at an unset court"), ObjectiveViewHelpers::RingIsMarigold(View, FCourtFixtureId(), Start, Start + 5.0, Delay, false));

	// A tend cancelled at 0.6 s: no hold (start < 0) is Bone; the next Begin at 2.0 s starts from Bone again.
	TestFalse(TEXT("a cancelled hold (no start) is Bone"), ObjectiveViewHelpers::RingIsMarigold(View, FCourtFixtureId(11), -1.0, Start + 0.6, Delay, false));
	const double Second = Start + 2.0;
	TestFalse(TEXT("the next hold starts Bone"), ObjectiveViewHelpers::RingIsMarigold(View, FCourtFixtureId(11), Second, Second + 0.5, Delay, false));
	TestTrue(TEXT("and turns Marigold 1.0 s after its own Begin"), ObjectiveViewHelpers::RingIsMarigold(View, FCourtFixtureId(11), Second, Second + Delay, Delay, false));
	return true;
}

#endif
