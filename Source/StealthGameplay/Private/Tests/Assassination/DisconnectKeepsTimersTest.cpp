// Story asn-003 (production/epics/assassination-defence-reversal/story-003-defence-state-component-and-cooldown.md),
// AC3 / AC-ASN-42: a disconnect changes neither the brace window nor the lockout.
// GIVEN a pawn braced at t = 0 (Begin key (0, 3)) whose controller disconnects at +1.0 s (no End is ever sent), its window
// is closed only by the HoldTimer(CapEnd) item due at +DefenceDurationSec (4.0 s, key (240, 5)). Asserted by queue key, as
// F2 says (no wall-clock compare inside a drain): an Attack keyed before the CapEnd item ((240, 4), "+3.9 s" side) finds
// the window open (the reversal branch), one keyed after it ((240, 6), "+4.0 s" side) finds it closed (the kill branch).
// AND an attacker that disconnects mid-lockout keeps its lockout: an Attack at t_exec + 1.74 is locked out (Busy) and one at
// t_exec + 1.75 is not. Its owner result goes to the null sink: the reply-sink table discards a reply to a released sink
// and to the null sink (FStrikeResult itself is built by story asn-015, which asserts the single null-sink delivery).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AIController.h"
#include "AssassinationTestWorld.h"
#include "GameFramework/PlayerController.h"
#include "Server/Core/ReplySink.h"
#include "Server/DefenceStateComponent.h"
#include "View/StealthTuningPublicSubstructs.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationDisconnectKeepsTimersTest, "StealthGame.Assassination.DisconnectKeepsTimers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationDisconnectKeepsTimersTest::RunTest(const FString& Parameters)
{
	const FAssassinationTuning_Public Tuning;
	StealthAssassinationTest::FScopedWorld TestWorld(TEXT("AssassinationDisconnectTestWorld"));
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// ---- The defender: braced at 0, disconnects at +1.0, CapEnd closes the window ----
	{
		AStealthPawn* Defender = TestWorld.SpawnPawn(0.0);
		APlayerController* Owner = TestWorld.World->SpawnActor<APlayerController>(Params);
		if (!TestTrue(TEXT("defender fixture spawned"), Defender != nullptr && Owner != nullptr))
		{
			return false;
		}
		Owner->Possess(Defender);
		FDefenceStateCore& State = UDefenceStateComponent::FindOrAdd(*Defender)->GetCore();
		const FQueueKey BeginKey(0, 3);
		TestTrue(TEXT("brace at t = 0 executes"), State.BeginPrepare(0.0, Tuning.DefenceCooldownSec, BeginKey) == EDefenceBeginResult::Executed);
		TestEqual(TEXT("its CapEnd is due at +4.0 s (DefenceDurationSec)"), State.GetCapEndServerTime(Tuning.DefenceDurationSec).Get(-1.0), 4.0);

		Owner->UnPossess(); // the controller disconnects at +1.0: no End command will arrive
		TestTrue(TEXT("window still open at +1.5 s (key (90, 1))"), State.IsWindowOpenAt(FQueueKey(90, 1)));

		const FQueueKey CapEndKey(240, 5);
		const FQueueKey AttackBefore(240, 4);
		const FQueueKey AttackAfter(240, 6);
		TestTrue(TEXT("an Attack keyed before CapEnd finds the window open (reversal)"), State.IsWindowOpenAt(AttackBefore));
		State.EndWindow(CapEndKey); // the CapEnd item's slot
		TestTrue(TEXT("the Attack keyed before CapEnd still reads open by key (F2 is key order, not slot order)"), State.IsWindowOpenAt(AttackBefore));
		TestFalse(TEXT("an Attack keyed after CapEnd finds it closed (kill)"), State.IsWindowOpenAt(AttackAfter));
		TestFalse(TEXT("an Attack keyed before the Begin is never braced"), State.IsWindowOpenAt(FQueueKey(0, 2)));
		TestTrue(TEXT("the disconnect changed the stamp in no way"), State.GetLastPrepareActivation() == TOptional<double>(0.0));
	}

	// ---- The attacker: disconnects mid-lockout; the lockout ends at t_exec + 1.75 ----
	{
		AStealthPawn* Attacker = TestWorld.SpawnPawn(400.0);
		APlayerController* Owner = TestWorld.World->SpawnActor<APlayerController>(Params);
		AAIController* AiHold = TestWorld.World->SpawnActor<AAIController>(Params);
		if (!TestTrue(TEXT("attacker fixture spawned"), Attacker != nullptr && Owner != nullptr && AiHold != nullptr))
		{
			return false;
		}
		Owner->Possess(Attacker);
		const double ExecTime = 50.0;
		FDefenceStateCore& State = UDefenceStateComponent::FindOrAdd(*Attacker)->GetCore();
		State.BeginLockout(ExecTime, Tuning.AttackAttemptLockoutSec);
		Owner->UnPossess();      // disconnects at +0.5
		AiHold->Possess(Attacker); // handed to AI hold
		TestEqual(TEXT("AttackAttemptLockoutSec default is 1.75"), static_cast<double>(Tuning.AttackAttemptLockoutSec), 1.75);
		TestEqual(TEXT("the lockout end is t_exec + 1.75"), State.GetLockoutEnd(), ExecTime + 1.75);
		TestTrue(TEXT("an Attack at t_exec + 1.74 is locked out (Busy)"), State.IsLockedOut(ExecTime + 1.74));
		TestFalse(TEXT("an Attack at t_exec + 1.75 executes"), State.IsLockedOut(ExecTime + 1.75));
		AiHold->UnPossess();

		// The disconnected attacker's sink: a released sink and the null sink discard replies.
		FReplySinkTable Sinks;
		TestTrue(TEXT("the null sink is null"), FReplySinkTable::IsNull(FReplySinkTable::NullSink()));
		TestFalse(TEXT("a reply to the null sink is discarded"), Sinks.Append(FReplySinkTable::NullSink(), FQueueKey(3000, 1), FCommandReply()));
		const FReplySinkId AttackerSink = Sinks.Allocate();
		Sinks.Release(AttackerSink); // the connection closed mid-lockout
		TestFalse(TEXT("a reply to the disconnected attacker's released sink is discarded"), Sinks.Append(AttackerSink, FQueueKey(3000, 2), FCommandReply()));
		TestEqual(TEXT("nothing is buffered for a closed connection"), Sinks.GetBufferedCount(), 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
