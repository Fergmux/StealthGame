// Story con-029 AC3 (production/epics/concealment-bushes/story-029-rustle-parity.md; ADR-0017 section 9):
// StealthGame.Concealment.RustlePulseSlot.
//   - a BushEnter, BushExit or bush-emerge pose maps to one pulse timed from its StartServerTime: onset 0.1 s before the swap
//     (Enter: Start + BushEnterSwapSec) or the leaf crossing (Exit: + BushExitCrossSec; Emerge: + BushStrikeEmergeSec);
//     no other pose (a plain strike, Brace, None) makes one;
//   - the pulse sits at the bush face on the transition's side (footprint edge midpoint), carries the side direction, the
//     BushRustleAmplitudeCm amplitude, the one shared SFX and the eye-masked tag;
//   - UBushRuntimeSubsystem::PushRustlePulse writes one slot of the 8-slot ring round-robin: the 9th pulse overwrites the
//     oldest slot; an unknown bush writes nothing; a pulse long past its life is not replayed;
//   - the pulse inputs name no pawn kind, team, controller, LOD, camera mode or occupancy (source scan, seeded control).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ConcealmentTestRig.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "View/BushRuntimeSubsystem.h"
#include "View/BushRustle.h"

namespace RustlePulseSlotTestData
{
	FActionState Pose(EActionPose Pose, double Start, uint8 Variant = 0)
	{
		FActionState State;
		State.Pose = Pose;
		State.StartServerTime = Start;
		State.Variant = Variant;
		return State;
	}

	/** True when the code (comments stripped) names a forbidden input. */
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
		for (const TCHAR* Word : { TEXT("Team"), TEXT("Controller"), TEXT("PlayerState"), TEXT("IsPlayer"), TEXT("LOD"), TEXT("Camera"), TEXT("FirstPerson"), TEXT("Occupan") })
		{
			if (Code.Contains(Word, ESearchCase::CaseSensitive))
			{
				return Word;
			}
		}
		return FString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentRustlePulseSlotTest, "StealthGame.Concealment.RustlePulseSlot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentRustlePulseSlotTest::RunTest(const FString& Parameters)
{
	using namespace RustlePulseSlotTestData;
	const FConcealmentTuning_Public Tuning;
	constexpr double Start = 40.0;

	// Pose -> kind and onset.
	TestTrue(TEXT("BushEnter is an Enter pulse"), BushRustle::KindOfPose(Pose(EActionPose::BushEnter, Start)) == TOptional<EBushRustleKind>(EBushRustleKind::Enter));
	TestTrue(TEXT("BushExit is an Exit pulse"), BushRustle::KindOfPose(Pose(EActionPose::BushExit, Start)) == TOptional<EBushRustleKind>(EBushRustleKind::Exit));
	TestTrue(TEXT("a bush-emerge strike is an Emerge pulse"), BushRustle::KindOfPose(Pose(EActionPose::Strike, Start, StrikeVariant::BushPreface)) == TOptional<EBushRustleKind>(EBushRustleKind::Emerge));
	TestFalse(TEXT("a plain strike makes no pulse"), BushRustle::KindOfPose(Pose(EActionPose::Strike, Start)).IsSet());
	TestFalse(TEXT("pose None (merely occupied) makes no pulse"), BushRustle::KindOfPose(Pose(EActionPose::None, Start)).IsSet());
	TestEqual(TEXT("Enter onset = Start + BushEnterSwapSec - 0.1"), BushRustle::OnsetFor(EBushRustleKind::Enter, Start, Tuning), Start + Tuning.BushEnterSwapSec - 0.1, 1e-6);
	TestEqual(TEXT("Exit onset = Start + BushExitCrossSec - 0.1"), BushRustle::OnsetFor(EBushRustleKind::Exit, Start, Tuning), Start + Tuning.BushExitCrossSec - 0.1, 1e-6);
	TestEqual(TEXT("Emerge onset = Start + BushStrikeEmergeSec - 0.1"), BushRustle::OnsetFor(EBushRustleKind::Emerge, Start, Tuning), Start + Tuning.BushStrikeEmergeSec - 0.1, 1e-6);

	// The pulse geometry.
	const FBushRuntime Bush = ConcealmentGeometry::MakeBushRuntime(3, FVector(1000.0, -500.0, 20.0), ConcealmentGeometry::AllSidesMask);
	for (uint8 Side = 0; Side < 4; ++Side)
	{
		const FBushRustlePulse Pulse = BushRustle::MakePulse(Bush, Side, EBushRustleKind::Exit, Start, Tuning, BushRustle::DefaultSound());
		const FVector Expected = Bush.Anchor + ConcealmentGeometry::SideNormal(Side) * 100.0;
		TestTrue(FString::Printf(TEXT("side %d: at the face midpoint, the side's direction"), Side), Pulse.WorldPos.Equals(Expected, 0.01)
			&& Pulse.SideDir.Equals(FVector2D(ConcealmentGeometry::SideNormal(Side)), 1e-4));
		TestTrue(FString::Printf(TEXT("side %d: amplitude, the shared SFX, eye-masked"), Side), FMath::IsNearlyEqual(Pulse.AmplitudeCm, Tuning.BushRustleAmplitudeCm)
			&& Pulse.Sound == BushRustle::DefaultSound() && Pulse.bEyeMasked);
	}

	// The ring through the subsystem (a client-like standalone world): 8 slots round-robin; the 9th overwrites the oldest.
	StealthConcealmentTest::FWorldHolder World(TEXT("ConcealmentRustleRing"));
	UBushRuntimeSubsystem* Bushes = UBushRuntimeSubsystem::Get(World.World);
	FBushSiteRecord Site;
	Site.BushId = 3;
	Site.GroundCentre = FVector(1000.0, -500.0, 20.0);
	if (!TestNotNull(TEXT("the bush runtime exists"), Bushes) || !TestTrue(TEXT("built"), Bushes->BuildFromSites(TConstArrayView<FBushSiteRecord>(&Site, 1))))
	{
		return false;
	}
	for (int32 Index = 0; Index < BushRustle::SlotCount; ++Index)
	{
		const int32 Slot = Bushes->PushRustlePulse(3, static_cast<uint8>(Index % 4), Start + Index, EBushRustleKind::Enter, Start + Index);
		TestEqual(FString::Printf(TEXT("pulse %d takes slot %d"), Index, Index), Slot, Index);
	}
	const FBushRustlePulse* Oldest = Bushes->GetRustleRing().GetSlot(0);
	TestTrue(TEXT("slot 0 holds the first pulse"), Oldest != nullptr && FMath::IsNearlyEqual(Oldest->StartServerTime, Start));
	const int32 Ninth = Bushes->PushRustlePulse(3, 2, Start + 20.0, EBushRustleKind::Emerge, Start + 20.0);
	const FBushRustlePulse* Overwritten = Bushes->GetRustleRing().GetSlot(0);
	TestTrue(TEXT("the 9th pulse overwrites the oldest slot (0)"), Ninth == 0 && Overwritten != nullptr && Overwritten->Kind == EBushRustleKind::Emerge
		&& FMath::IsNearlyEqual(Overwritten->StartServerTime, Start + 20.0));
	TestTrue(TEXT("slot 1 still holds the second pulse"), Bushes->GetRustleRing().GetSlot(1) != nullptr && FMath::IsNearlyEqual(Bushes->GetRustleRing().GetSlot(1)->StartServerTime, Start + 1.0));
	TestEqual(TEXT("an unknown bush writes nothing"), Bushes->PushRustlePulse(99, 0, Start, EBushRustleKind::Enter, Start), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("a pulse past its life is not replayed (a very late actor)"), Bushes->PushRustlePulse(3, 0, Start, EBushRustleKind::Exit, Start + 30.0), static_cast<int32>(INDEX_NONE));
	const int32 LateSlot = Bushes->PushRustlePulse(3, 0, Start + 50.0, EBushRustleKind::Exit, BushRustle::OnsetFor(EBushRustleKind::Exit, Start + 50.0, Tuning) + 0.2);
	TestTrue(TEXT("a late write inside the pulse's life keeps the original onset (seeked, never restarted)"), LateSlot != INDEX_NONE
		&& FMath::IsNearlyEqual(Bushes->GetRustleRing().GetSlot(LateSlot)->OnsetServerTime, BushRustle::OnsetFor(EBushRustleKind::Exit, Start + 50.0, Tuning)));
	TestEqual(TEXT("ten pulses logged (the two refused writes are not)"), Bushes->GetRustleLog().Num(), 10);

	// No pawn kind, team, LOD, camera mode or occupancy in the pulse path.
	TestFalse(TEXT("control: the scan sees a forbidden input"), ForbiddenIn(TEXT("if (Pawn->IsPlayerControlled()) { Amplitude *= OccupancyScale; }")).IsEmpty());
	const FString Root = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/StealthGameplay/Features/Concealment"));
	FString Rustle;
	if (TestTrue(TEXT("BushRustle.cpp is readable"), FFileHelper::LoadFileToString(Rustle, *FPaths::Combine(Root, TEXT("Private/BushRustle.cpp")))))
	{
		TestEqual(TEXT("BushRustle.cpp names no kind / team / LOD / camera / occupancy input"), ForbiddenIn(Rustle), FString());
	}
	FString Subsystem;
	if (TestTrue(TEXT("BushRuntimeSubsystem.cpp is readable"), FFileHelper::LoadFileToString(Subsystem, *FPaths::Combine(Root, TEXT("Private/BushRuntimeSubsystem.cpp")))))
	{
		const int32 From = Subsystem.Find(TEXT("int32 UBushRuntimeSubsystem::PushRustlePulse("));
		if (TestTrue(TEXT("PushRustlePulse was found"), From != INDEX_NONE))
		{
			TestEqual(TEXT("PushRustlePulse and the SFX pool name no such input"), ForbiddenIn(Subsystem.Mid(From)), FString());
		}
	}
	return true;
}

#endif
