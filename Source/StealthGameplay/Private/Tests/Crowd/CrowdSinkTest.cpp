// Story cnb-005 (production/epics/crowd-npc-behaviour/story-005-command-sinks-epoch-and-halt-filter.md): the crowd's
// one command boundary (ADR-0016 section 2; GDD CNB-R3, R10.5; AC-CNB-11, AC-CNB-38 sink half).
//   Vocabulary     FCrowdMotionSink exposes only Move and Look; FCrowdCommandSink adds the discrete commands; neither
//                  has Attack (compile-time: static_asserts in CrowdSink.h, re-checked here).
//   EpochGate      a call whose token epoch is not the pawn's current epoch, or whose pawn is gone, is dropped and
//                  counted (StaleEpochDrop); a current token reaches the command component.
//   HaltFilter     after the latch only Move(0) and Look pass, also from a due-time entry and an async path callback
//                  running later in the same frame.
//   ForbiddenApis  the Crowd lint (tools/ci/lint/crowd-lint.sh, comments excluded) is watched failing on planted
//                  violations and passes the real Crowd tree.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdTestUtils.h"
#include "Server/CrowdSink.h"
#include "Tests/Locomotion/LocomotionTestWorld.h"

namespace CrowdSinkTestData
{
	/** A fake factory epoch: the test bumps it to model a hand-over commit. */
	class FFakeEpochs final : public ICrowdEpochSource
	{
	public:
		uint32 Epoch = 1;
		virtual uint32 GetControlEpoch(const AStealthPawn*) const override { return Epoch; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdSinkVocabularyTest, "StealthGame.Crowd.Sink.Vocabulary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdSinkVocabularyTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("FCrowdMotionSink has no Attack"), StealthCrowdSink::THasAttack<FCrowdMotionSink>::value);
	TestFalse(TEXT("FCrowdCommandSink has no Attack"), StealthCrowdSink::THasAttack<FCrowdCommandSink>::value);
	TestFalse(TEXT("FCrowdMotionSink has no discrete command (Traverse)"), StealthCrowdSink::THasTraverse<FCrowdMotionSink>::value);
	TestTrue(TEXT("FCrowdCommandSink has the discrete commands (Traverse)"), StealthCrowdSink::THasTraverse<FCrowdCommandSink>::value);
	// Negative control: the detector itself finds an Attack member when one exists.
	struct FWithAttack { void Attack() {} };
	TestTrue(TEXT("the detector finds an Attack member (negative control)"), StealthCrowdSink::THasAttack<FWithAttack>::value);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdSinkEpochGateTest, "StealthGame.Crowd.Sink.EpochGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdSinkEpochGateTest::RunTest(const FString& Parameters)
{
	using namespace CrowdSinkTestData;
	StealthLocomotionTest::FWorld TestWorld(TEXT("CrowdSinkEpochWorld"));
	AStealthPawn* Pawn = TestWorld.SpawnPawn(FVector2D::ZeroVector);
	FFakeEpochs Epochs;

	// Arrange: a sink created at epoch 1.
	FCrowdCommandSink Sink(Pawn, Epochs.Epoch, &Epochs);
	// Act + assert: a current call reaches the command component.
	TestTrue(TEXT("Move passes at the current epoch"), Sink.Move(FVector2D(0.0, 0.9), EGait::Walk));
	TestEqual(TEXT("the held Move intent is the sink's"), Pawn->GetPawnCommand()->GetMoveIntent(), FVector2D(0.0, 0.9));
	TestTrue(TEXT("IsCurrent at the token epoch"), Sink.IsCurrent());

	// The hand-over commit bumps the epoch: every later call of the old driver drops, Move, Look and discrete alike.
	Epochs.Epoch = 2;
	TestFalse(TEXT("Move drops after the epoch bump"), Sink.Move(FVector2D(0.0, 1.0), EGait::Walk));
	TestFalse(TEXT("Look drops after the epoch bump"), Sink.Look(FRotator(-10.0, 30.0, 0.0)));
	TestFalse(TEXT("InteractEnd drops after the epoch bump"), Sink.InteractEnd());
	TestEqual(TEXT("StaleEpochDrop counts the three drops"), Sink.GetCounters().StaleEpochDrop, 3);
	TestEqual(TEXT("the held intent is unchanged by the stale calls"), Pawn->GetPawnCommand()->GetMoveIntent(), FVector2D(0.0, 0.9));

	// A new driver's sink at the new epoch passes.
	FCrowdMotionSink NewDriver(Pawn, Epochs.Epoch, &Epochs);
	TestTrue(TEXT("the new driver's Move passes"), NewDriver.Move(FVector2D::ZeroVector, EGait::Walk));

	// A stale weak pawn drops.
	Pawn->Destroy();
	TestFalse(TEXT("a call to a destroyed pawn drops"), NewDriver.Look(FRotator::ZeroRotator));
	TestEqual(TEXT("and is counted as stale"), NewDriver.GetCounters().StaleEpochDrop, 1);

	// The production epoch source reads the pawn factory (a pawn the factory never possessed has epoch 0).
	AStealthPawn* Other = TestWorld.SpawnPawn(FVector2D(500.0, 0.0));
	FCrowdMotionSink FactorySink(Other, 0u);
	TestEqual(TEXT("the factory epoch of an unpossessed pawn is 0"), FCrowdFactoryEpochSource::Get().GetControlEpoch(Other), 0u);
	TestTrue(TEXT("a sink at epoch 0 is current for it"), FactorySink.IsCurrent());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdSinkHaltFilterTest, "StealthGame.Crowd.Sink.HaltFilter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdSinkHaltFilterTest::RunTest(const FString& Parameters)
{
	using namespace CrowdSinkTestData;
	StealthLocomotionTest::FWorld TestWorld(TEXT("CrowdSinkHaltWorld"));
	AStealthPawn* Pawn = TestWorld.SpawnPawn(FVector2D::ZeroVector);
	FFakeEpochs Epochs;
	FCrowdCommandSink Sink(Pawn, Epochs.Epoch, &Epochs);
	Sink.Move(FVector2D(0.0, 1.0), EGait::Walk);

	// Halt latches the sink synchronously; the Director then clears Move with Move(0), which passes.
	Sink.SetHalted(true);
	TestTrue(TEXT("Move(0) passes after Halt"), Sink.Move(FVector2D::ZeroVector, EGait::Walk));
	TestTrue(TEXT("the held intent is cleared"), Pawn->GetPawnCommand()->GetMoveIntent().IsNearlyZero());

	// Later in the same frame: a due-time entry and an async path callback of the old behaviour try to move again.
	TArray<TFunction<bool()>> SameFrameCallbacks;
	SameFrameCallbacks.Add([&Sink]() { return Sink.Move(FVector2D(0.0, 0.8), EGait::Walk); });   // due-time entry: pause end
	SameFrameCallbacks.Add([&Sink]() { return Sink.Move(FVector2D(0.7, 0.7), EGait::Walk); });   // path callback: new corners
	SameFrameCallbacks.Add([&Sink]() { return Sink.Move(FVector2D(0.0, 1.0), EGait::Run); });    // red herring due
	SameFrameCallbacks.Add([&Sink]() { return Sink.PrepareBegin(); });
	SameFrameCallbacks.Add([&Sink]() { return Sink.Hide(FHideCommand::Enter(3)); });
	SameFrameCallbacks.Add([&Sink]() { return Sink.Traverse(FTraversalHint()); });
	int32 Passed = 0;
	for (const TFunction<bool()>& Callback : SameFrameCallbacks)
	{
		Passed += Callback() ? 1 : 0;
	}
	TestEqual(TEXT("no motion or discrete command passes after Halt"), Passed, 0);
	TestEqual(TEXT("each is counted as a Halt drop"), Sink.GetCounters().HaltDrop, SameFrameCallbacks.Num());
	TestTrue(TEXT("the held intent stays cleared"), Pawn->GetPawnCommand()->GetMoveIntent().IsNearlyZero());
	// Look keeps the head idle after Halt.
	TestTrue(TEXT("Look passes after Halt"), Sink.Look(FRotator(-5.0, 10.0, 0.0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdSinkForbiddenApisTest, "StealthGame.Crowd.Sink.ForbiddenApis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdSinkForbiddenApisTest::RunTest(const FString& Parameters)
{
	const StealthCrowdTest::FScriptRun Run = StealthCrowdTest::RunBashScript(TEXT("tools/ci/lint/selftest/run-crowd-lint-selftest.sh"));
	if (!TestTrue(TEXT("the crowd lint selftest ran (bash found)"), Run.bLaunched))
	{
		return false;
	}
	TestEqual(TEXT("selftest exit code"), Run.ExitCode, 0);
	TestTrue(TEXT("the gate is watched failing on the planted violations"), Run.Output.Contains(TEXT("SELFTEST crowd-seeded ok")));
	TestTrue(TEXT("the clean control is silent"), Run.Output.Contains(TEXT("SELFTEST crowd-clean ok")));
	TestTrue(TEXT("the real Crowd tree has zero findings"), Run.Output.Contains(TEXT("SELFTEST real-tree-zero-findings ok")));
	if (HasAnyErrors())
	{
		AddInfo(Run.Output);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
