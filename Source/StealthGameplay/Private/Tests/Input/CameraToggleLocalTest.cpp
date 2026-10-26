// Story iif-014 (production/epics/input-interaction-framework/story-014-camera-toggle-local.md), GDD AC 29 (R14, Logic,
// U CameraToggleLocal): GIVEN an active hold WHEN V is pressed THEN the hold continues and zero RPCs are sent; AND a
// second press within CameraToggleLockoutSec (0.30 s) is ignored.
// Pinned boundary (QL-STORY-READY rewrite): a press < CameraToggleLockoutSec after the last ACCEPTED toggle is ignored;
// at >= the lockout it is accepted; ignored presses do not extend the lockout. Cases: press at 1.0 accepted; 1.299
// ignored; 1.30 accepted; with a fresh anchor, an ignored press at 1.15 does not move the lockout (1.31 accepted); a
// lockout of 0.60 behaves the same (the value is data: UStealthSettings CameraToggleLockoutSec until the IIF keys are
// registered). The toggle never becomes a command: the builder's send stream stays empty and the outstanding hold is
// untouched (zero RPCs: the command uplink is the only RPC path of the builder).
// Not covered by the AC, stated here: a toggle while a menu is open is the camera's own decision (Camera epic).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "View/Input/PawnCommandClient.h"

namespace CameraToggleLocalTestData
{
	/** The GDD default (IIF CameraToggleLockoutSec 0.30 s) and the alternative the story asks for. */
	constexpr double DefaultLockoutSec = 0.30;
	constexpr double LongLockoutSec = 0.60;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputCameraToggleLocalTest, "StealthGame.Input.CameraToggleLocal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInputCameraToggleLocalTest::RunTest(const FString& Parameters)
{
	using namespace CameraToggleLocalTestData;
	using EHeld = FPawnCommandClientCore::EHeld;

	// Arrange: an active Interact hold.
	TArray<FNetPawnCommand> Sent;
	FPawnCommandClientCore Core([&Sent](const FNetPawnCommand& Cmd) { Sent.Add(Cmd); });
	Core.OnHeldStarted(EHeld::Interact);
	const int32 SentBefore = Sent.Num();

	// Act + assert: the lockout boundary.
	TestTrue(TEXT("a press at 1.0 is accepted"), Core.TryAcceptCameraToggle(1.0, DefaultLockoutSec));
	TestFalse(TEXT("a press at 1.299 (inside 0.30 s) is ignored"), Core.TryAcceptCameraToggle(1.299, DefaultLockoutSec));
	TestTrue(TEXT("a press at 1.30 (exactly the lockout) is accepted"), Core.TryAcceptCameraToggle(1.30, DefaultLockoutSec));

	// The hold continues and nothing was sent.
	TestTrue(TEXT("the hold continues"), Core.IsOutstanding(EHeld::Interact));
	TestEqual(TEXT("zero commands (RPCs) were sent by the toggles"), Sent.Num(), SentBefore);

	// Ignored presses do not extend the lockout.
	FPawnCommandClientCore Fresh;
	TestTrue(TEXT("fresh anchor at 1.0"), Fresh.TryAcceptCameraToggle(1.0, DefaultLockoutSec));
	TestFalse(TEXT("an ignored press at 1.15"), Fresh.TryAcceptCameraToggle(1.15, DefaultLockoutSec));
	TestTrue(TEXT("1.31 is accepted: the ignored press did not move the lockout"), Fresh.TryAcceptCameraToggle(1.31, DefaultLockoutSec));

	// A 0.60 s lockout read from data behaves the same.
	FPawnCommandClientCore Long;
	TestTrue(TEXT("0.60 lockout: 2.0 accepted"), Long.TryAcceptCameraToggle(2.0, LongLockoutSec));
	TestFalse(TEXT("0.60 lockout: 2.59 ignored"), Long.TryAcceptCameraToggle(2.59, LongLockoutSec));
	TestTrue(TEXT("0.60 lockout: 2.60 accepted"), Long.TryAcceptCameraToggle(2.60, LongLockoutSec));

	// Releasing the held key after toggles still ends the hold once.
	Core.OnHeldTerminal(EHeld::Interact);
	TestTrue(TEXT("the hold ends with one End after the toggles"), Sent.Num() == SentBefore + 1 && Sent.Last().Kind == ECommandKind::InteractEnd);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
