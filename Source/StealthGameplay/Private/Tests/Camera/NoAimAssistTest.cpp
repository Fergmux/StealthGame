// Camera story cs-003 AC2 (production/epics/camera-system/story-003-stick-look-response-no-aim-assist.md; GDD camera-system.md
// AC-CS-14, CS-R9; IIF R15): StealthGame.Camera.NoAimAssist. An identical injected Look sequence (stick sweeps across a
// target, a mouse flick, a hold at the edge) is replayed through the real look pipeline (F6 then the F1 envelope) twice:
// once in a world with a dummy Shared Pawn standing under the screen centre of the sweep, once in an empty world. The view
// rotation trajectories are identical to 0.01 deg on every frame. The test passes only because nothing in the look path
// reads a pawn, a target or a registry; the source scan (seeded control) is the structural half of that: the pipeline's
// sources name no pawn, actor, world, registry, target or assist.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "View/Core/StealthLookResponse.h"
#include "View/Core/StealthPawn.h"
#include "View/Tuning/StealthCameraParams.h"

namespace CameraNoAimAssistTestData
{
	/** The scripted Look input of frame Index (1/60 s frames). */
	FLookFrameInput InputAt(int32 Index)
	{
		FLookFrameInput In;
		In.DtSec = 1.0 / 60.0;
		if (Index < 60)
		{
			In.Stick = FVector2D(0.6, 0.05); // a slow sweep right across the dummy
		}
		else if (Index < 90)
		{
			In.Stick = FVector2D(-0.9, -0.2); // back across it
		}
		else if (Index < 100)
		{
			In.bStick = false;
			In.MouseDeltaDeg = FVector2D(4.0, 0.5); // a mouse flick
		}
		else
		{
			In.Stick = FVector2D(1.0, 0.0); // hold at the edge
		}
		return In;
	}

	/** Runs the pipeline over the script; World only exists (the look path is never given it). */
	TArray<FVector2D> Replay()
	{
		const FStealthCameraParams Params = FStealthCameraParams::Defaults();
		const FViewEnvelopeParams Envelope = FViewEnvelopeParams::FromCamera(Params);
		const FStealthCameraUserSettings Settings;
		FViewEnvelopeState State;
		TArray<FVector2D> Trajectory;
		for (int32 Index = 0; Index < 160; ++Index)
		{
			StealthLookResponse::Step(State, InputAt(Index), Settings, Params.StickOuterZoneFrac, Envelope);
			Trajectory.Add(FVector2D(State.ViewYawOffsetDeg, State.ViewPitchDeg));
		}
		return Trajectory;
	}

	/** The first forbidden word the code names (comments stripped), or empty. */
	FString Forbidden(const FString& Text)
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
		for (const TCHAR* Word : { TEXT("Pawn"), TEXT("Actor"), TEXT("UWorld"), TEXT("GetWorld"), TEXT("Registry"), TEXT("AimTarget"), TEXT("TargetPawn"), TEXT("Assist"), TEXT("Magnet"), TEXT("LineTrace") })
		{
			if (Code.Contains(Word, ESearchCase::CaseSensitive))
			{
				return Word;
			}
		}
		return FString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraNoAimAssistTest, "StealthGame.Camera.NoAimAssist",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCameraNoAimAssistTest::RunTest(const FString& Parameters)
{
	using namespace CameraNoAimAssistTestData;
	// Run 1: a world with a dummy pawn standing where the sweep's screen centre passes (8 m ahead, 15 deg right).
	TArray<FVector2D> WithDummy;
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CameraNoAimAssistDummy"));
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		World->BeginPlay();
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const AStealthPawn* Dummy = World->SpawnActor<AStealthPawn>(FRotator(0.0, 15.0, 0.0).Vector() * 800.0, FRotator::ZeroRotator, Spawn);
		TestNotNull(TEXT("the dummy stands under the sweep's screen centre"), Dummy);
		WithDummy = Replay();
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	}
	// Run 2: no world, no pawn at all.
	const TArray<FVector2D> Without = Replay();
	TestEqual(TEXT("both runs have the same frame count"), WithDummy.Num(), Without.Num());
	double Worst = 0.0;
	for (int32 Index = 0; Index < FMath::Min(WithDummy.Num(), Without.Num()); ++Index)
	{
		Worst = FMath::Max(Worst, FMath::Max(FMath::Abs(WithDummy[Index].X - Without[Index].X), FMath::Abs(WithDummy[Index].Y - Without[Index].Y)));
	}
	TestTrue(*FString::Printf(TEXT("view rotation trajectories identical to 0.01 deg (worst %.6f)"), Worst), Worst <= 0.01);
	TestTrue(TEXT("the sweep actually turned the view (the comparison is meaningful)"), Without.Num() > 59 && FMath::Abs(Without[59].X) > 10.0);

	// Structural half: the look path names no pawn, actor, world, registry, target or assist.
	TestFalse(TEXT("control: the scan sees a target read"), Forbidden(TEXT("const APawn* Target = Registry.Nearest();")).IsEmpty());
	const FString Root = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/StealthGameplay/Private/Core/Camera"));
	for (const TCHAR* File : { TEXT("StealthLookResponse.cpp"), TEXT("StealthViewEnvelope.cpp") })
	{
		FString Text;
		if (TestTrue(*FString::Printf(TEXT("%s is readable"), File), FFileHelper::LoadFileToString(Text, *FPaths::Combine(Root, File))))
		{
			TestEqual(*FString::Printf(TEXT("%s names no pawn / target / registry / assist"), File), Forbidden(Text), FString());
		}
	}
	return true;
}

#endif
