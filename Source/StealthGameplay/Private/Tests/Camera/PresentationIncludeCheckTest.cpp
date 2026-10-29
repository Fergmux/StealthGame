// Camera story cs-024 AC1 (production/epics/camera-system/story-024-presentation-include-check-tell-matrix.md; GDD camera-system.md
// AC-CS-39, IVP AC-IVP-17; ADR-0007 section 3, ADR-0015 Guideline 4): StealthGame.Camera.PresentationIncludeCheck. Every
// camera source (StealthGameplay/Private/Core/Camera and the camera headers under Public/View/Core and View/Tuning) includes
// no server query header (no "Server/" include path) and references GetPlayerState, GetController, IsPlayerControlled or
// GetOwner only at an allowlisted call site on the local controller's own possessed pawn (the allowlist is empty: the cores
// read none). The negative control seeds a server include and a GetPlayerState call and the same check must fail on it.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Server/Testing/StealthNegativeControl.h"

namespace CameraIncludeCheckTestData
{
	/** "<file>:<identifier>" call sites allowed to read identity on the local controller's own pawn (none today). */
	const TCHAR* const Allowlist[] = { TEXT("") };

	const TCHAR* const Banned[] = { TEXT("GetPlayerState"), TEXT("GetController"), TEXT("IsPlayerControlled"), TEXT("GetOwner") };

	/** The camera sources: every file of Private/Core/Camera and the named camera headers. */
	TArray<FString> CameraFiles()
	{
		const FString Root = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/StealthGameplay"));
		TArray<FString> Out;
		IFileManager::Get().FindFiles(Out, *FPaths::Combine(Root, TEXT("Private/Core/Camera/*.*")), true, false);
		for (FString& Name : Out)
		{
			Name = FPaths::Combine(Root, TEXT("Private/Core/Camera"), Name);
		}
		for (const TCHAR* Header : { TEXT("Public/View/Core/StealthViewEnvelope.h"), TEXT("Public/View/Core/StealthLookResponse.h"), TEXT("Public/View/Core/StealthCameraCore.h"),
			TEXT("Public/View/Core/StealthEyeMaskCore.h"), TEXT("Public/View/Core/StealthFov.h"), TEXT("Public/View/Core/StealthCollisionChannels.h") })
		{
			Out.Add(FPaths::Combine(Root, Header));
		}
		Out.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/StealthFoundation/Public/View/Tuning/StealthCameraParams.h")));
		return Out;
	}

	/** The rule over one file's text: comment lines stripped. */
	void CheckText(const FString& Name, const FString& Text, FStealthCheckContext& Ctx)
	{
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			const FString Trimmed = Line.TrimStart();
			if (Trimmed.StartsWith(TEXT("//")) || Trimmed.StartsWith(TEXT("*")) || Trimmed.StartsWith(TEXT("/*")))
			{
				continue;
			}
			Ctx.TestFalse(FString::Printf(TEXT("%s includes no server header: %s"), *Name, *Trimmed), Trimmed.StartsWith(TEXT("#include")) && Trimmed.Contains(TEXT("\"Server/")));
			for (const TCHAR* Word : Banned)
			{
				if (Trimmed.Contains(Word, ESearchCase::CaseSensitive))
				{
					const FString Site = FString::Printf(TEXT("%s:%s"), *FPaths::GetCleanFilename(Name), Word);
					bool bAllowed = false;
					for (const TCHAR* Allowed : Allowlist)
					{
						bAllowed |= Site == Allowed;
					}
					Ctx.TestTrue(FString::Printf(TEXT("%s calls %s only on the local controller's possessed pawn (allowlisted): %s"), *Name, Word, *Trimmed), bAllowed);
				}
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraPresentationIncludeCheckTest, "StealthGame.Camera.PresentationIncludeCheck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCameraPresentationIncludeCheckTest::RunTest(const FString& Parameters)
{
	using namespace CameraIncludeCheckTestData;
	const TArray<FString> Files = CameraFiles();
	TestTrue(*FString::Printf(TEXT("the camera sources were found (%d)"), Files.Num()), Files.Num() >= 12);
	for (const FString& File : Files)
	{
		FString Text;
		if (!TestTrue(*FString::Printf(TEXT("%s is readable"), *File), FFileHelper::LoadFileToString(Text, *File)))
		{
			continue;
		}
		FStealthCheckContext Ctx;
		CheckText(File, Text, Ctx);
		for (const FString& Failure : Ctx.GetFailures())
		{
			AddError(Failure);
		}
	}
	// Negative control: a seeded server include and a seeded identity read must both fail the same check.
	FStealthNegativeControl Control(TEXT("CameraSeededServerIncludeAndIdentityRead"));
	const FStealthNegativeControlResult Result = Control.Run([](FStealthCheckContext& Ctx)
	{
		CheckText(TEXT("Seeded.cpp"), TEXT("#include \"Server/ConcealmentService.h\"\nconst APlayerState* S = Pawn->GetPlayerState();\n"), Ctx);
	});
	FStealthNegativeControl::Report(*this, Result);
	return true;
}

#endif
