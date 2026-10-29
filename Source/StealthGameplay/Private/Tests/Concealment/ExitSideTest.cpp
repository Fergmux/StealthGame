// Story con-005 (production/epics/concealment-bushes/story-005-hide-exit-execution-and-blocked-side.md; GDD CON-R8, F3,
// AC-CON-10; ADR-0017 sections 2, 3; ADR-0003 section 5.6):
//   - StealthGame.Concealment.ExitSideAndBlocked (AC1): a closed side maps to the nearest open one; an Outside pawn on the
//     first side's end point moves the exit to the second; a concealed co-occupant or an Entering pawn there never blocks;
//     with both sides blocked the requested side is used and the depenetration is capped at 30 cm. No exit is refused.
//   - StealthGame.Concealment.ExitExecution (AC2): Exiting with SetCullState(false) in the exit's own slot, FActionState
//     {BushExit, T}; Cross at T + 0.45 leaves the record and clears the flags; End at T + 0.9 is Outside and starts the
//     re-entry cooldown.
//   - StealthGame.Concealment.ExitSideMapping (AC3): look yaw to the 2-bit side at every quantisation boundary +-1 degree;
//     a closed side maps to the open side nearest the look yaw, a tie to the lower index; the AI path gives the same index.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ConcealmentTestRig.h"

using namespace StealthConcealmentTest;

namespace ConcealmentExitSideTestData
{
	/** Sides 0 (+X) and 2 (-X) open. */
	constexpr uint8 TwoSides = (1 << 0) | (1 << 2);

	/** Runs one exit and returns the side the pawn left by (the record's side after execution). */
	uint8 ExitBy(FRig& Rig, AStealthPawn* Hider, uint8 Requested)
	{
		Rig.SubmitHide(Hider, ECommandKind::HideExit, Requested);
		Rig.Tick();
		const FConcealRecord* Record = Rig.Core->FindRecord(Hider);
		return Record != nullptr ? Record->Side : 0xFF;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentExitSideAndBlockedTest, "StealthGame.Concealment.ExitSideAndBlocked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentExitSideAndBlockedTest::RunTest(const FString& Parameters)
{
	using namespace ConcealmentExitSideTestData;
	const FBushRuntime Bush = ConcealmentGeometry::MakeBushRuntime(1, FVector::ZeroVector, TwoSides);
	const FVector End0 = ConcealmentGeometry::ExitPoint(Bush, 0, 35.0) + FVector(0.0, 0.0, 86.0);
	const FVector End2 = ConcealmentGeometry::ExitPoint(Bush, 2, 35.0) + FVector(0.0, 0.0, 86.0);

	// A closed side (1) maps to the nearest open side: 0 and 2 tie at 90 degrees, the lower index wins.
	{
		FRig Rig(TEXT("ConcealmentExitClosed"), TwoSides);
		AStealthPawn* Hider = Rig.SpawnAtReach(30.0);
		Rig.EnterAndSettle(Hider);
		TestEqual(TEXT("closed side 1 -> nearest open side 0"), ExitBy(Rig, Hider, 1), static_cast<uint8>(0));
		TestEqual(TEXT("no exit is refused"), Rig.RigQueue.Replies.Num(), 0);
	}
	// An Outside pawn on side 0's end point: the second open side (2) is used.
	{
		FRig Rig(TEXT("ConcealmentExitBlockedOnce"), TwoSides);
		AStealthPawn* Hider = Rig.SpawnAtReach(30.0);
		Rig.EnterAndSettle(Hider);
		Rig.Spawn(End0);
		TestEqual(TEXT("side 0 blocked by an Outside pawn -> side 2"), ExitBy(Rig, Hider, 0), static_cast<uint8>(2));
	}
	// A concealed co-occupant or an Entering pawn at the end point never blocks.
	{
		FRig Rig(TEXT("ConcealmentExitNonBlockers"), TwoSides);
		AStealthPawn* Hider = Rig.SpawnAtReach(30.0);
		AStealthPawn* CoOccupant = Rig.SpawnAtReach(30.0, 20.0);
		Rig.EnterAndSettle(Hider);
		Rig.EnterAndSettle(CoOccupant);
		Rig.Pawns.Of(CoOccupant).Location = End0; // a concealed pawn standing on the end point (scripted)
		AStealthPawn* Entrant = Rig.Spawn(End0 + FVector(0.0, 10.0, 0.0));
		Rig.SubmitHide(Entrant, ECommandKind::HideEnter, 1);
		Rig.Tick();
		Rig.Pawns.Of(Entrant).Location = End0 + FVector(0.0, 10.0, 0.0);
		Rig.Pawns.Of(Entrant).WarpEnd = -1.0;
		TestEqual(TEXT("the entrant is Entering"), Rig.StateOf(Entrant), EConcealState::Entering);
		TestEqual(TEXT("concealed and Entering pawns do not block side 0"), ExitBy(Rig, Hider, 0), static_cast<uint8>(0));
	}
	// Both open sides blocked by Outside pawns: the requested side, and the capped depenetration at End.
	{
		FRig Rig(TEXT("ConcealmentExitBothBlocked"), TwoSides);
		AStealthPawn* Hider = Rig.SpawnAtReach(30.0);
		Rig.EnterAndSettle(Hider);
		Rig.Spawn(End0);
		Rig.Spawn(End2);
		TestEqual(TEXT("both blocked -> the requested side 2"), ExitBy(Rig, Hider, 2), static_cast<uint8>(2));
		Rig.Run(1.0);
		TestEqual(TEXT("the exit completed"), Rig.StateOf(Hider), EConcealState::Outside);
		TestEqual(TEXT("depenetration capped at BushExitDepenetrationCapCm (30 cm)"), Rig.Pawns.Of(Hider).DepenetrationCapCm, 30.0, 1e-6);
		TestEqual(TEXT("no reply differs: nothing was refused"), Rig.RigQueue.Replies.Num(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentExitExecutionTest, "StealthGame.Concealment.ExitExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentExitExecutionTest::RunTest(const FString& Parameters)
{
	FRig Rig(TEXT("ConcealmentExitExecution"));
	const FConcealmentParams& P = Rig.Params;
	AStealthPawn* Hider = Rig.SpawnAtReach(30.0);
	Rig.Relations.Bind(Hider, 3, 1);
	Rig.EnterAndSettle(Hider);
	TestTrue(TEXT("committed: flags set"), Rig.Flags.OwnerOf(3) && Rig.Flags.TeamOf(3));

	Rig.SubmitHide(Hider, ECommandKind::HideExit, 0);
	Rig.Tick();
	const FQueueKey ExitKey = Rig.Q.Queue->GetLastDrainRecords().Last().Key;
	const double T = Rig.Now();
	TestEqual(TEXT("Exiting"), Rig.StateOf(Hider), EConcealState::Exiting);
	TestEqual(TEXT("SetCullState(false) once"), Rig.Cull.CountFor(Hider, false), 1);
	TestTrue(TEXT("... in the exit's own slot"), Rig.Cull.Calls.Num() >= 2 && Rig.Cull.Calls.Last().Key == ExitKey);
	TestEqual(TEXT("FActionState BushExit"), Rig.Pawns.Of(Hider).Action.Pose, EActionPose::BushExit);
	TestEqual(TEXT("... StartServerTime = T"), Rig.Pawns.Of(Hider).Action.StartServerTime, T, 1e-9);

	// Cross at T + BushExitCrossSec.
	double CrossAt = -1.0;
	double EndAt = -1.0;
	for (int32 Frame = 0; Frame < 70; ++Frame)
	{
		const bool bInRecord = Rig.Core->FindRecord(Hider)->bInOccupancy;
		const EConcealState Before = Rig.StateOf(Hider);
		Rig.Tick();
		if (bInRecord && !Rig.Core->FindRecord(Hider)->bInOccupancy)
		{
			CrossAt = Rig.Now();
			TestFalse(TEXT("the crossing clears the owner flag"), Rig.Flags.OwnerOf(3));
			TestFalse(TEXT("... and the team flag"), Rig.Flags.TeamOf(3));
		}
		if (Before == EConcealState::Exiting && Rig.StateOf(Hider) == EConcealState::Outside)
		{
			EndAt = Rig.Now();
		}
	}
	TestEqual(TEXT("Cross at T + BushExitCrossSec (0.45)"), CrossAt - T, static_cast<double>(P.Public.BushExitCrossSec), FrameSec + 1e-6);
	TestEqual(TEXT("End at T + BushExitDurSec (0.9): Outside"), EndAt - T, static_cast<double>(P.Public.BushExitDurSec), FrameSec + 1e-6);
	TestEqual(TEXT("one ExitCross log row"), Rig.Count(EConcealmentLogEvent::ExitCross), 1);
	TestEqual(TEXT("the re-entry cooldown starts at the transition's end"), Rig.Core->FindRecord(Hider)->LastExitCompleteServerTime, EndAt, FrameSec + 1e-6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentExitSideMappingTest, "StealthGame.Concealment.ExitSideMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentExitSideMappingTest::RunTest(const FString& Parameters)
{
	using namespace ConcealmentGeometry;
	// Side k covers [k*90 - 45, k*90 + 45), lower bound inclusive: each boundary and +-1 degree.
	struct FRow
	{
		double Yaw;
		uint8 Side;
	};
	const FRow Rows[] = {
		{ -45.0, 0 }, { -46.0, 3 }, { -44.0, 0 }, { 0.0, 0 }, { 44.0, 0 }, { 45.0, 1 }, { 46.0, 1 }, { 134.0, 1 }, { 135.0, 2 },
		{ 136.0, 2 }, { 224.0, 2 }, { 225.0, 3 }, { 226.0, 3 }, { 314.0, 3 }, { 315.0, 0 }, { 316.0, 0 }, { -135.0, 3 }, { -136.0, 2 }, { 180.0, 2 },
	};
	for (const FRow& Row : Rows)
	{
		TestEqual(FString::Printf(TEXT("yaw %.0f -> side %d"), Row.Yaw, Row.Side), SideFromYaw(Row.Yaw), Row.Side);
	}
	// A closed side maps to the open side nearest the look yaw; an angular tie goes to the lower index.
	const uint8 Sides02 = (1 << 0) | (1 << 2);
	TestEqual(TEXT("open {0,2}, look 90 (tie) -> 0"), NearestOpenSide(Sides02, 90.0), static_cast<uint8>(0));
	TestEqual(TEXT("open {0,2}, look 91 -> 2"), NearestOpenSide(Sides02, 91.0), static_cast<uint8>(2));
	TestEqual(TEXT("open {0,2}, look 89 -> 0"), NearestOpenSide(Sides02, 89.0), static_cast<uint8>(0));
	TestEqual(TEXT("open {0,2}, look 270 (tie) -> 0"), NearestOpenSide(Sides02, 270.0), static_cast<uint8>(0));
	const uint8 Side3Only = 1 << 3;
	TestEqual(TEXT("open {3}, any look -> 3"), NearestOpenSide(Side3Only, 45.0), static_cast<uint8>(3));
	TestEqual(TEXT("validate: an open request is kept"), ValidateSide(Sides02, 2), static_cast<uint8>(2));
	TestEqual(TEXT("validate: closed 1 -> 0 (tie, lower index)"), ValidateSide(Sides02, 1), static_cast<uint8>(0));
	TestEqual(TEXT("validate: closed 3 -> 0 (tie, lower index)"), ValidateSide(Sides02, 3), static_cast<uint8>(0));
	// The AI path: the same function gives the identical index for the identical yaw (no caller input exists).
	for (double Yaw = -360.0; Yaw <= 360.0; Yaw += 7.5)
	{
		if (SideFromYaw(Yaw) != SideFromYaw(Yaw + 360.0))
		{
			AddError(FString::Printf(TEXT("yaw %.1f maps differently after a full turn"), Yaw));
		}
	}
	return true;
}

#endif
