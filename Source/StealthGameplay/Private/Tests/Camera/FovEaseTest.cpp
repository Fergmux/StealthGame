// Camera story cs-010 AC2 (production/epics/camera-system/story-010-field-of-view.md; GDD camera-system.md AC-CS-31, CS-R13):
// StealthGame.Camera.FovEase. A mid-round FOV change from 60 to 70 deg eases over 0.15 s (monotone, arriving at 0.15 s, never
// overshooting) and is instant under Reduce camera motion; the eye-mask cone is unchanged by any FOV change (the F5 path
// never reads or writes it); zero RPCs: the FOV path is plain C++ with no network entry point (a source scan of StealthFov
// for Server / Client / NetMulticast / RPC with a seeded control).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "View/Core/StealthFov.h"
#include "View/Tuning/StealthCameraParams.h"

namespace CameraFovEaseTestData
{
	bool NamesNetwork(const FString& Text)
	{
		for (const TCHAR* Word : { TEXT("UFUNCTION"), TEXT("Server"), TEXT("NetMulticast"), TEXT("Client,"), TEXT("Reliable"), TEXT("RPC") })
		{
			if (Text.Contains(Word, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	FString CodeOnly(const FString& Text)
	{
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines);
		FString Code;
		for (const FString& Line : Lines)
		{
			const FString Trimmed = Line.TrimStart();
			if (!Trimmed.StartsWith(TEXT("//")) && !Trimmed.StartsWith(TEXT("*")) && !Trimmed.StartsWith(TEXT("/*")))
			{
				Code += Line + TEXT("\n");
			}
		}
		return Code;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraFovEaseTest, "StealthGame.Camera.FovEase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCameraFovEaseTest::RunTest(const FString& Parameters)
{
	using namespace CameraFovEaseTestData;
	constexpr double Dt = 1.0 / 60.0;
	const FStealthCameraParams Params = FStealthCameraParams::Defaults();
	const double ConeH = Params.EyeConeHalfHDeg;
	const double ConeV = Params.EyeConeHalfVDeg;

	FStealthFovEase Ease;
	Ease.SetTarget(60.0, false);
	TestEqual(TEXT("the first value is applied at once"), Ease.GetCurrentDeg(), 60.0, 1e-9);
	Ease.SetTarget(70.0, false);
	double Previous = Ease.GetCurrentDeg();
	bool bMonotone = true;
	double ArrivedAt = -1.0;
	for (int32 Frame = 1; Frame <= 30; ++Frame)
	{
		Ease.Step(Dt);
		bMonotone &= Ease.GetCurrentDeg() >= Previous - 1e-9 && Ease.GetCurrentDeg() <= 70.0 + 1e-9;
		Previous = Ease.GetCurrentDeg();
		if (ArrivedAt < 0.0 && FMath::IsNearlyEqual(Ease.GetCurrentDeg(), 70.0, 1e-9))
		{
			ArrivedAt = Frame * Dt;
		}
		if (Frame == 4)
		{
			TestTrue(TEXT("still easing at 0.067 s (not a cut)"), Ease.GetCurrentDeg() > 60.0 && Ease.GetCurrentDeg() < 70.0);
		}
	}
	TestTrue(TEXT("the ease is monotone with no overshoot"), bMonotone);
	TestTrue(*FString::Printf(TEXT("arrives at 0.15 s (%.3f)"), ArrivedAt), ArrivedAt > 0.15 - Dt - 1e-9 && ArrivedAt <= 0.15 + Dt);

	FStealthFovEase Reduced;
	Reduced.SetTarget(60.0, true);
	Reduced.SetTarget(70.0, true);
	TestEqual(TEXT("Reduce camera motion: the change is instant"), Reduced.GetCurrentDeg(), 70.0, 1e-9);

	// The cone is unchanged (F5 reads only the FOV keys) and no FOV path is a network call.
	const FStealthFovResult Wide = StealthFov::Compute(75.0, 16.0 / 9.0, Params);
	TestTrue(TEXT("the eye-mask cone is unchanged by FOV changes"), Params.EyeConeHalfHDeg == ConeH && Params.EyeConeHalfVDeg == ConeV && Wide.HorizontalDeg > 0.0);
	TestTrue(TEXT("control: the scan sees an RPC declaration"), NamesNetwork(TEXT("UFUNCTION(Server, Reliable) void ServerSetFov(float V);")));
	const FString Root = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/StealthGameplay"));
	for (const TCHAR* File : { TEXT("Public/View/Core/StealthFov.h"), TEXT("Private/Core/Camera/StealthFov.cpp") })
	{
		FString Text;
		if (TestTrue(*FString::Printf(TEXT("%s is readable"), File), FFileHelper::LoadFileToString(Text, *FPaths::Combine(Root, File))))
		{
			TestFalse(*FString::Printf(TEXT("%s declares no network call (zero RPCs)"), File), NamesNetwork(CodeOnly(Text)));
			TestFalse(*FString::Printf(TEXT("%s never reads the eye-mask cone"), File), CodeOnly(Text).Contains(TEXT("EyeCone")));
		}
	}
	return true;
}

#endif
