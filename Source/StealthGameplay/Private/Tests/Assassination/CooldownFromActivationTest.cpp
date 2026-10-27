// Story asn-003 (production/epics/assassination-defence-reversal/story-003-defence-state-component-and-cooldown.md),
// AC1 / AC-ASN-14: the defence cooldown runs from ACTIVATION (the executed Prepare(Begin)), used or unused.
// GIVEN a brace tapped at t = 100.0 (window 0.10 s), DefenceCooldownSec 60 (the tuning default), WHEN Prepare(Begin) is sent
// at 159.9 THEN it replies Cooldown, and at 160.0 it executes (inclusive; one server tick either side checked); a brace held
// to its 4.0 s cap and one released at 0.5 s both stamp 100.0; 20 presses during a live window change neither the stamp,
// the window keys nor the CapEnd due time (each replies Cooldown). Reference spec: StealthGame.Rules.Defence (69.9 blocked,
// 70.0 allowed from t = 10). The predicate object is exercised on a spawned pawn with an injected fake clock.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "AssassinationTestWorld.h"
#include "Server/AssassinationPreparePredicate.h"
#include "Server/DefenceStateComponent.h"
#include "Server/Testing/StealthFakeClock.h"
#include "View/StealthTuningPublicSubstructs.h"

namespace AssassinationCooldownTestData
{
	const FAssassinationTuning_Public Tuning;

	/** Server time T at 60 Hz as a queue key (tick index, seq). */
	FQueueKey KeyAt(double T, uint64 Seq = 1)
	{
		return FQueueKey(static_cast<uint64>(FMath::RoundToInt64(T * 60.0)), Seq);
	}

	bool IsCooldown(const FPredicateResult& R) { return R == FPredicateResult::Reject(ECommandRejectCode::Cooldown); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationCooldownFromActivationTest, "StealthGame.Assassination.CooldownFromActivation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationCooldownFromActivationTest::RunTest(const FString& Parameters)
{
	using namespace AssassinationCooldownTestData;
	using StealthAssassinationTest::ServerTickSec;
	const double C = Tuning.DefenceCooldownSec;
	const double Cap = Tuning.DefenceDurationSec;
	TestEqual(TEXT("DefenceCooldownSec default is 60 (confirmed rule)"), C, 60.0);
	TestEqual(TEXT("DefenceDurationSec default is 4 (confirmed rule)"), Cap, 4.0);

	// ---- A tap at 100.0: the stamp is the activation; the cooldown runs to 160.0 inclusive ----
	{
		FDefenceStateCore State;
		TestTrue(TEXT("no stamp at round start: Prepare allowed"), UAssassinationPreparePredicate::Evaluate(&State, 0.0, C) == FPredicateResult::Allow());
		TestTrue(TEXT("tap at 100.0 executes"), State.BeginPrepare(100.0, C, KeyAt(100.0)) == EDefenceBeginResult::Executed);
		State.EndWindow(KeyAt(100.1)); // released: the debounce expiry ends the window 0.10 s later
		TestTrue(TEXT("T_act is 100.0"), State.GetLastPrepareActivation() == TOptional<double>(100.0));

		TestTrue(TEXT("Prepare at 159.9 replies Cooldown"), IsCooldown(UAssassinationPreparePredicate::Evaluate(&State, 159.9, C)));
		TestTrue(TEXT("one tick before 160.0 replies Cooldown"), IsCooldown(UAssassinationPreparePredicate::Evaluate(&State, 160.0 - ServerTickSec, C)));
		TestTrue(TEXT("Prepare at 160.0 is allowed (inclusive)"), UAssassinationPreparePredicate::Evaluate(&State, 160.0, C) == FPredicateResult::Allow());
		TestTrue(TEXT("one tick after 160.0 is allowed"), UAssassinationPreparePredicate::Evaluate(&State, 160.0 + ServerTickSec, C) == FPredicateResult::Allow());
		TestEqual(TEXT("cooldown end is 160.0"), State.GetCooldownEndServerTime(C).Get(0.0), 160.0);

		TestTrue(TEXT("a Begin executing at 159.9 is refused as Cooldown"), State.BeginPrepare(159.9, C, KeyAt(159.9)) == EDefenceBeginResult::Cooldown);
		TestTrue(TEXT("the refused Begin left the stamp at 100.0"), State.GetLastPrepareActivation() == TOptional<double>(100.0));
		TestTrue(TEXT("a Begin at 160.0 executes"), State.BeginPrepare(160.0, C, KeyAt(160.0)) == EDefenceBeginResult::Executed);
		TestTrue(TEXT("and stamps 160.0"), State.GetLastPrepareActivation() == TOptional<double>(160.0));
	}

	// ---- Held to the 4.0 s cap, and released at 0.5 s: both stamp 100.0 (an unused brace burns the full cooldown) ----
	{
		FDefenceStateCore Held;
		Held.BeginPrepare(100.0, C, KeyAt(100.0));
		TestEqual(TEXT("held brace: CapEnd due at 104.0"), Held.GetCapEndServerTime(Cap).Get(0.0), 104.0);
		Held.EndWindow(KeyAt(104.0)); // HoldTimer(CapEnd)
		FDefenceStateCore Released;
		Released.BeginPrepare(100.0, C, KeyAt(100.0));
		Released.EndWindow(KeyAt(100.6)); // released at 0.5 s + 0.10 s debounce
		TestTrue(TEXT("held to cap stamps 100.0"), Held.GetLastPrepareActivation() == TOptional<double>(100.0));
		TestTrue(TEXT("released at 0.5 s stamps 100.0"), Released.GetLastPrepareActivation() == TOptional<double>(100.0));
		TestTrue(TEXT("both block at 159.9"), IsCooldown(UAssassinationPreparePredicate::Evaluate(&Held, 159.9, C)) && IsCooldown(UAssassinationPreparePredicate::Evaluate(&Released, 159.9, C)));
		TestTrue(TEXT("a later end is ignored (first end wins)"), (Held.EndWindow(KeyAt(105.0)), Held.GetWindow().EndKey == KeyAt(104.0)));
	}

	// ---- 20 presses during a live window: no stamp, window key or CapEnd change ----
	{
		FDefenceStateCore State;
		State.BeginPrepare(100.0, C, KeyAt(100.0));
		const TOptional<double> StampBefore = State.GetLastPrepareActivation();
		const FDefenceWindow WindowBefore = State.GetWindow();
		const TOptional<double> CapBefore = State.GetCapEndServerTime(Cap);
		int32 Refused = 0;
		for (int32 Press = 1; Press <= 20; ++Press)
		{
			const double T = 100.0 + 0.15 * Press; // inside the 4 s window
			Refused += State.BeginPrepare(T, C, KeyAt(T, 2)) == EDefenceBeginResult::Cooldown ? 1 : 0;
		}
		TestEqual(TEXT("all 20 presses reply Cooldown"), Refused, 20);
		TestTrue(TEXT("the stamp is unchanged"), State.GetLastPrepareActivation() == StampBefore);
		TestTrue(TEXT("the window keys are unchanged"), State.GetWindow().BeginKey == WindowBefore.BeginKey && State.GetWindow().EndKey == WindowBefore.EndKey);
		TestTrue(TEXT("the CapEnd due time is unchanged"), State.GetCapEndServerTime(Cap) == CapBefore);
	}

	// ---- The registered predicate object on a pawn, with an injected clock ----
	{
		StealthAssassinationTest::FScopedWorld TestWorld(TEXT("AssassinationCooldownTestWorld"));
		AStealthPawn* Pawn = TestWorld.SpawnPawn(0.0);
		if (!TestNotNull(TEXT("pawn spawned"), Pawn))
		{
			return false;
		}
		FFakeStealthClock Clock(0.0, 0);
		UAssassinationPreparePredicate* Predicate = NewObject<UAssassinationPreparePredicate>();
		Predicate->Initialize(&Clock, Tuning);
		const FCommandPayload Cmd;

		TestNull(TEXT("a pawn that never braced has no defence component"), UDefenceStateComponent::Find(*Pawn));
		TestTrue(TEXT("CanBegin allows a pawn with no stamp"), Predicate->CanBegin(*Pawn, Cmd) == FPredicateResult::Allow());
		TestNull(TEXT("CanBegin is side-effect-free (creates no component)"), UDefenceStateComponent::Find(*Pawn));

		UDefenceStateComponent* Component = UDefenceStateComponent::FindOrAdd(*Pawn);
		if (!TestNotNull(TEXT("FindOrAdd creates the component on the server"), Component))
		{
			return false;
		}
		TestFalse(TEXT("the defence component is not replicated"), Component->GetIsReplicated());
		TestFalse(TEXT("the defence component never ticks"), Component->PrimaryComponentTick.bCanEverTick);
		TestTrue(TEXT("FindOrAdd returns the same component the second time"), UDefenceStateComponent::FindOrAdd(*Pawn) == Component);

		Clock.AdvanceSeconds(100.0);
		Component->GetCore().BeginPrepare(Clock.GetServerTimeSeconds(), C, KeyAt(100.0));
		Clock.AdvanceSeconds(59.9);
		TestTrue(TEXT("CanBegin at 159.9 replies Cooldown"), IsCooldown(Predicate->CanBegin(*Pawn, Cmd)));
		Clock.AdvanceSeconds(0.1);
		TestTrue(TEXT("CanBegin at 160.0 allows"), Predicate->CanBegin(*Pawn, Cmd) == FPredicateResult::Allow());

		Predicate->Initialize(nullptr, Tuning);
		TestTrue(TEXT("no clock bound: Reject(NotNow)"), Predicate->CanBegin(*Pawn, Cmd) == FPredicateResult::Reject(ECommandRejectCode::NotNow));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS
