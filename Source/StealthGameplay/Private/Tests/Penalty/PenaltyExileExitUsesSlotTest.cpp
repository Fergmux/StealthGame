// Story nkp-006 (production/epics/npc-kill-penalty-informants/story-006-exile-releases-body-to-crowd.md), AC3 /
// AC-NKP-45 (PEN-R5; Crowd CR-4, CNB-R9.1): GIVEN a fixed crowd seed and 30 simulated minutes run twice, once with 10
// exiles injected (two of them inside one slot interval, released in the same frame) and once with none, WHEN AmbientExit
// events are counted city-wide, THEN each exile body's exit occupies a scheduled exit slot (it was started by the ambient
// schedule's StartAmbientExit, never added beside it), the exits started never exceed the slots fired, and the totals of
// the two runs are equal (the exile adds nothing to the city's exit rate). For a like-for-like population the same ten
// player bodies exist in both runs: in the control run they are released to the crowd without the exile flag (an ordinary
// release, no exit at the next slot), in the exile run through Penalty's exile (PenaltyCrowdSimRig.h).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Penalty/PenaltyCrowdSimRig.h"

namespace PenaltyExileExitUsesSlotTestData
{
	/** The AC's run: 30 simulated minutes, 10 exiles. */
	inline constexpr double RunSec = 30.0 * 60.0;
	inline constexpr int32 Exiles = 10;
	/** Injection times: one a minute from minute 2; the last two share a frame (two exiles inside one slot interval). */
	inline double InjectAt(int32 Index) { return 120.0 + 60.0 * FMath::Min(Index, Exiles - 2); }

	struct FRun
	{
		int32 Slots = 0;
		int32 ExitsStarted = 0;
		int32 ExileExitsInSlots = 0;
		int32 ExileBodies = 0;
	};

	FRun Run(bool bExiles)
	{
		using namespace StealthPenaltyCrowdSim;
		FRig Rig;
		TArray<uint32> Bodies;
		for (int32 Index = 0; Index < Exiles; ++Index)
		{
			const double Angle = 2.0 * UE_DOUBLE_PI * Index / Exiles;
			FNpc& Body = Rig.SpawnPlayerBody(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * 900.0, static_cast<float>(Index * 36));
			Bodies.Add(Body.Body.Ordinal);
		}
		int32 Next = 0;
		while (Rig.World->GetNow() < RunSec)
		{
			while (Next < Exiles && Rig.World->GetNow() >= InjectAt(Next))
			{
				FNpc* Body = Rig.World->FindNpc(Bodies[Next]);
				if (bExiles)
				{
					Rig.ExileNow(*Body);
				}
				else
				{
					// The control: the same body released to the crowd with the same lockout, without the exile's exit.
					Body->Agent.ScheduleAdoption(Rig.World->GetNow(), Rig.World->GetNow() + StealthPenaltyTest::FRig::LockoutSec(), false, false, 0.0, *Rig.World);
				}
				++Next;
			}
			Rig.World->Step();
		}
		FRun Out;
		Out.Slots = Rig.ExitSlots();
		Out.ExitsStarted = Rig.World->ExitsStarted;
		for (const uint32 Ordinal : Bodies)
		{
			Out.ExileExitsInSlots += Rig.World->ExitStartedAt.Contains(Ordinal) ? 1 : 0;
		}
		Out.ExileBodies = Rig.Bodies.Adopted.Num();
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyExileExitUsesSlotTest, "StealthGame.Penalty.ExileExitUsesSlot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyExileExitUsesSlotTest::RunTest(const FString& Parameters)
{
	using namespace PenaltyExileExitUsesSlotTestData;

	// Arrange + Act: the two runs on the same seed.
	const FRun Control = Run(false);
	const FRun WithExiles = Run(true);
	AddInfo(FString::Printf(TEXT("EXITSLOT 30 min: control slots %d exits %d; exiles slots %d exits %d, exile bodies %d, their exits in slots %d"),
		Control.Slots, Control.ExitsStarted, WithExiles.Slots, WithExiles.ExitsStarted, WithExiles.ExileBodies, WithExiles.ExileExitsInSlots));

	// Assert
	TestEqual(TEXT("ten exiles were injected through Penalty"), WithExiles.ExileBodies, Exiles);
	TestEqual(TEXT("each exile body's exit was started by a scheduled exit slot"), WithExiles.ExileExitsInSlots, Exiles);
	TestTrue(TEXT("no extra exit: exits started never exceed the slots fired (exile run)"), WithExiles.ExitsStarted <= WithExiles.Slots);
	TestTrue(TEXT("no extra exit: exits started never exceed the slots fired (control run)"), Control.ExitsStarted <= Control.Slots);
	TestEqual(TEXT("the same scheduled exit slots in both runs"), WithExiles.Slots, Control.Slots);
	TestEqual(TEXT("the total exit count of the two runs is equal"), WithExiles.ExitsStarted, Control.ExitsStarted);
	return true;
}

#endif
