// Camera story cs-012 AC4 (production/epics/camera-system/story-012-eye-mask-core.md; ADR-0015 section 4.3.5; TR-camera-008):
// StealthGame.Camera.MaskTraceCap. Sixty targets near and in the cone with every probe blocked (the GDD worst case: 59 others,
// plus one) are due on every test: at most EyeMaskTracesPerFrameMax (160) traces are submitted per frame; the targets beyond
// the cap are deferred one frame and taken first on the next frame, in round-robin order of registry index (never by kind,
// team or distance: here the nearest targets are the highest indices and gain nothing); every target is tested within two
// frames of becoming due.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "EyeMaskTestSupport.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraMaskTraceCapTest, "StealthGame.Camera.MaskTraceCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCameraMaskTraceCapTest::RunTest(const FString& Parameters)
{
	using namespace StealthEyeMaskTest;
	FEyeMaskCore Mask(Params());
	FDelayedBackend Backend;
	Backend.Blocked = [](const FVector&, const FVector&) { return true; }; // every probe blocked: no early exit
	constexpr int32 Targets = 60;
	TArray<int32> Ids;
	for (int32 Index = 0; Index < Targets; ++Index)
	{
		Ids.Add(Mask.Register());
	}
	// Index i stands 15 m - i * 10 cm ahead (higher index = nearer), spread across the cone.
	auto Probes = [](int32 Id, FEyeMaskProbes& Out)
	{
		Out = CapsuleProbes(FVector(1500.0 - Id * 10.0, (Id % 10 - 5) * 40.0, 0.0));
		return true;
	};
	const FEyeMaskView View = ViewAlongX();
	double Now = 0.0;
	int32 MaxPerFrame = 0;
	TArray<int32> FirstTested;
	FirstTested.Init(-1, Targets);
	for (int32 F = 0; F < 12; ++F)
	{
		Now += Dt;
		Mask.Step(Now, Dt, View, Probes, Backend);
		MaxPerFrame = FMath::Max(MaxPerFrame, Mask.GetLastFrameTraces());
		for (int32 Index = 0; Index < Targets; ++Index)
		{
			if (FirstTested[Index] < 0 && Mask.GetTestsStarted(Ids[Index]) > 0)
			{
				FirstTested[Index] = F;
			}
		}
		++Backend.Frame;
	}
	TestTrue(*FString::Printf(TEXT("at most %d traces per frame (max %d)"), FEyeMaskCore::TracesPerFrameMax, MaxPerFrame), MaxPerFrame <= FEyeMaskCore::TracesPerFrameMax);
	TestEqual(TEXT("the first frame submits exactly the cap (32 targets x 5 probes)"), Backend.SubmittedPerFrame.Num() > 0 ? Backend.SubmittedPerFrame[0] : -1, FEyeMaskCore::TracesPerFrameMax);
	bool bOrdered = true;
	for (int32 Index = 0; Index < Targets; ++Index)
	{
		const int32 Expected = Index < FEyeMaskCore::TracesPerFrameMax / 5 ? 0 : 1;
		bOrdered &= FirstTested[Index] == Expected;
	}
	TestTrue(TEXT("registry indices 0-31 are tested on frame 0 and 32-59 (deferred, the nearest among them) on frame 1: round-robin by index, never by distance"), bOrdered);
	return true;
}

#endif
