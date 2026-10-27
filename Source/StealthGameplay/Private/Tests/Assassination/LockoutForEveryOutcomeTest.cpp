// Story asn-008, AC2 / AC-ASN-07 LockoutForEveryOutcome (ASN-R15, F3). After an executed kill and, separately, an executed
// whiff, Attack, Prepare (cooldown clear), Interact (a live site: the fallback stand-in dispatch executes it) and Hide
// (beside a bush: the same) issued on the last server tick before t_exec + 1.75 reply Busy; at t_exec + 1.75 each executes
// (one fresh round per command), and no other cooldown follows the whiff. Ten Attack presses at 10 Hz during the lockout
// reply Busy, the resolver schedules nothing for them, and an Attack at t_exec + 1.75 executes. A reversed attacker's
// LockoutEnd is stamped t_exec + 1.75 too, and its later commands reply NotNow (it is an informant).
// Validation order: the slot's Busy is decided at the item's slot (the IIF bucket is ingress-side and never binds
// before the 1.75 s lockout, F3).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/Testing/StealthQueueTestRig.h"
#include "Tests/Assassination/EncounterTestRig.h"

namespace LockoutForEveryOutcomeTestData
{
	using namespace StealthEncounterTest;
	constexpr double Tick = 1.0 / 60.0;
	const ECommandKind Kinds[] = { ECommandKind::Attack, ECommandKind::PrepareBegin, ECommandKind::InteractBegin, ECommandKind::HideEnter };

	/** Executes the first strike (a kill when bKill, else a whiff) and returns its execution time. */
	inline double FirstStrike(FRig& R, APawn* A, bool bKill)
	{
		const double Exec = R.Clock.GetServerTimeSeconds();
		if (bKill)
		{
			R.AttackRef(A, R.Spawn(FVector(150.0, 0.0, 0.0)));
		}
		else
		{
			R.Attack(A, FVector(150.0, 0.0, 0.0));
		}
		return Exec;
	}

	inline void SetTime(FRig& R, double Time)
	{
		R.Clock.Advance(Time - R.Clock.GetServerTimeSeconds(), 1);
		if (R.Clock.GetServerTimeSeconds() < Time)
		{
			R.Clock.AdvanceSeconds(1e-9); // never one ulp short of the boundary (well inside the +-1 tick tolerance)
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationLockoutForEveryOutcomeTest, "StealthGame.Assassination.LockoutForEveryOutcome",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationLockoutForEveryOutcomeTest::RunTest(const FString& Parameters)
{
	using namespace LockoutForEveryOutcomeTestData;
	for (const bool bKill : { true, false })
	{
		const TCHAR* Outcome = bKill ? TEXT("after a kill") : TEXT("after a whiff");
		for (const ECommandKind Kind : Kinds)
		{
			FRig R(TEXT("LockoutEveryOutcome"));
			FStealthStubResolver Sites; // Interact at a live site, Hide beside a bush: the stand-in dispatch executes them
			R.Core->SetFallbackResolvers(&Sites, nullptr);
			APawn* A = R.Spawn(FVector::ZeroVector, 0);
			const double Exec = FirstStrike(R, A, bKill);
			const double Lockout = R.Params.Public.AttackAttemptLockoutSec;
			const int32 Name = static_cast<int32>(Kind);

			SetTime(R, Exec + Lockout - Tick); // the last tick before the boundary
			R.Command(A, Kind);
			TestTrue(FString::Printf(TEXT("%s: kind %d one tick before t_exec + 1.75 replies Busy"), Outcome, Name), !R.LastReply().bExecuted && R.LastReply().Code == ECommandRejectCode::Busy);
			TestEqual(FString::Printf(TEXT("%s: kind %d never reached the site dispatch while locked"), Outcome, Name), Sites.Resolved.Num(), 0);

			SetTime(R, Exec + Lockout);
			const EResolveResult Result = Kind == ECommandKind::Attack ? R.Attack(A, FVector(150.0, 0.0, 0.0)) : R.Command(A, Kind);
			TestTrue(FString::Printf(TEXT("%s: kind %d at t_exec + 1.75 executes"), Outcome, Name), Result == EResolveResult::Executed);
		}
	}

	// Ten presses at 10 Hz during the lockout: all Busy, no queue growth; the press at +1.75 executes.
	{
		FRig R(TEXT("LockoutSpam"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		const double Exec = FirstStrike(R, A, false);
		const int32 Scheduled = R.Queue.Scheduled.Num();
		int32 Busy = 0;
		for (int32 Press = 1; Press <= 10; ++Press)
		{
			SetTime(R, Exec + 0.1 * Press);
			R.Attack(A, FVector(150.0, 0.0, 0.0));
			Busy += !R.LastReply().bExecuted && R.LastReply().Code == ECommandRejectCode::Busy ? 1 : 0;
		}
		TestEqual(TEXT("10 presses at 10 Hz: all Busy"), Busy, 10);
		TestEqual(TEXT("the spam grows nothing in the queue"), R.Queue.Scheduled.Num(), Scheduled);
		TestEqual(TEXT("and resolves no strike"), R.Core->GetResolutions().Num(), 1);
		SetTime(R, Exec + R.Params.Public.AttackAttemptLockoutSec);
		TestTrue(TEXT("the Attack at +1.75 executes (no extra cooldown after a whiff)"), R.Attack(A, FVector(150.0, 0.0, 0.0)) == EResolveResult::Executed);
	}

	// A reversed attacker: LockoutEnd = t_exec + 1.75, later commands NotNow.
	{
		FRig R(TEXT("LockoutReversed"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* Defender = R.Spawn(FVector(150.0, 0.0, 0.0), 1);
		R.Command(Defender, ECommandKind::PrepareBegin);
		R.Advance(0.5);
		const double Exec = R.Clock.GetServerTimeSeconds();
		R.AttackRef(A, Defender);
		TestTrue(TEXT("reversal"), R.Last().Outcome == EAttackOutcome::Reversal);
		TestTrue(TEXT("the reversed attacker's LockoutEnd is t_exec + 1.75"), R.State(A) != nullptr && R.State(A)->GetLockoutEnd() == Exec + static_cast<double>(R.Params.Public.AttackAttemptLockoutSec));
		SetTime(R, Exec + 2.0);
		R.Attack(A, FVector(150.0, 0.0, 0.0));
		TestTrue(TEXT("its later Attack replies NotNow"), !R.LastReply().bExecuted && R.LastReply().Code == ECommandRejectCode::NotNow);
		R.Command(A, ECommandKind::PrepareBegin);
		TestTrue(TEXT("its later Prepare replies NotNow"), !R.LastReply().bExecuted && R.LastReply().Code == ECommandRejectCode::NotNow);
	}
	return true;
}

#endif
