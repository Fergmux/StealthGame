// Camera story cs-012 AC2 (production/epics/camera-system/story-012-eye-mask-core.md; GDD camera-system.md AC-CS-07, CS-R5b):
// StealthGame.Camera.MaskParityAndInputs. An "NPC" dummy and a "participant-bound" dummy stand at the same spot behind a wall
// (it blocks traces crossing x = 500 below y = 100, the corner); the player walks past the corner along +Y. Their alpha
// curves are identical frame for frame: the core only knows a target as a registry index and its probes (it is given no kind,
// team or binding). The static half: the mask and rig sources read no team, binding, Roster state, PlayerState or controller class
// (a scan with a seeded control).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "EyeMaskTestSupport.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace CameraMaskParityTestData
{
	FString ForbiddenIn(const FString& Text)
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
		for (const TCHAR* Word : { TEXT("GetPlayerState"), TEXT("GetController"), TEXT("IsPlayerControlled"), TEXT("Team"), TEXT("Roster"), TEXT("Participant"), TEXT("Binding"), TEXT("CrowdController") })
		{
			if (Code.Contains(Word, ESearchCase::CaseSensitive))
			{
				return Word;
			}
		}
		return FString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraMaskParityAndInputsTest, "StealthGame.Camera.MaskParityAndInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCameraMaskParityAndInputsTest::RunTest(const FString& Parameters)
{
	using namespace StealthEyeMaskTest;
	using namespace CameraMaskParityTestData;
	FEyeMaskCore Mask(Params());
	FDelayedBackend Backend;
	// A wall along x = 500 for y < 100 (the corner at y = 100): a trace is blocked when it crosses x = 500 below y = 100.
	Backend.Blocked = [](const FVector& From, const FVector& To)
	{
		if ((From.X - 500.0) * (To.X - 500.0) >= 0.0)
		{
			return false;
		}
		const double T = (500.0 - From.X) / (To.X - From.X);
		return FMath::Lerp(From.Y, To.Y, T) < 100.0;
	};
	const int32 NpcId = Mask.Register();
	const int32 BoundId = Mask.Register();
	// The same spot behind the wall for both (identical geometry): only their registry index and their "kind" differ, and the
	// core is given no kind at all.
	const FVector Feet(900.0, 300.0, 0.0);
	FEyeMaskView View = ViewAlongX();
	View.Origin = FVector(0.0, -400.0, 160.0);
	double Now = 0.0;
	TArray<float> NpcCurve;
	TArray<float> BoundCurve;
	for (int32 F = 0; F < 240; ++F)
	{
		Now += Dt;
		View.Origin.Y = -400.0 + 800.0 * F / 240.0; // the player walks along +Y past the corner over 4 s
		Mask.Step(Now, Dt, View, [&](int32, FEyeMaskProbes& Out) { Out = CapsuleProbes(Feet); return true; }, Backend);
		++Backend.Frame;
		NpcCurve.Add(Mask.GetAlpha(NpcId));
		BoundCurve.Add(Mask.GetAlpha(BoundId));
	}
	int32 Differences = 0;
	bool bSeen = false;
	bool bHidden = false;
	for (int32 F = 0; F < NpcCurve.Num(); ++F)
	{
		Differences += NpcCurve[F] != BoundCurve[F] ? 1 : 0;
		bSeen |= NpcCurve[F] >= 1.0f;
		bHidden |= NpcCurve[F] == 0.0f;
	}
	TestEqual(TEXT("the NPC and the bound dummy have identical alpha curves frame for frame"), Differences, 0);
	TestTrue(TEXT("the walk both hides and reveals the dummy (the comparison is meaningful)"), bSeen && bHidden);

	// Static half: no team, binding, Roster, PlayerState or controller read in the mask or the rig.
	TestFalse(TEXT("control: the scan sees a team read"), ForbiddenIn(TEXT("if (Pawn->GetPlayerState()->Team == Mine) {}")).IsEmpty());
	const FString Root = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/StealthGameplay"));
	for (const TCHAR* File : { TEXT("Private/Core/Camera/StealthEyeMaskCore.cpp"), TEXT("Public/View/Core/StealthEyeMaskCore.h"), TEXT("Private/Core/Camera/StealthCameraCore.cpp"),
		TEXT("Private/Core/Camera/StealthViewEnvelope.cpp"), TEXT("Private/Core/Camera/StealthLookResponse.cpp"), TEXT("Private/Core/Camera/StealthFov.cpp") })
	{
		FString Text;
		if (TestTrue(*FString::Printf(TEXT("%s is readable"), File), FFileHelper::LoadFileToString(Text, *FPaths::Combine(Root, File))))
		{
			TestEqual(*FString::Printf(TEXT("%s reads no team / binding / Roster / controller"), File), ForbiddenIn(Text), FString());
		}
	}
	return true;
}

#endif
