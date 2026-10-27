// Story asn-010, AC2 / AC-ASN-28 CounterIsNotProtected (ASN-R13, R14). A defender that has just reversed A1 is Countering
// for 1.2 s from the slot: a second rival A2's Attack at slot + 0.6 s, and in other runs at the first and the last tick of
// the window, kills it; during Countering every discrete command of the defender (Traverse, InteractBegin/End,
// PrepareBegin/End, Attack, HideEnter/Exit) issued at 0.2, 0.6 and 1.0 s replies Busy; at 1.2 s + 1 tick its cooldown end
// is exactly the original LastPrepareActivation + 60 s (never re-stamped).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Assassination/EncounterTestRig.h"

namespace CounterIsNotProtectedTestData
{
	using namespace StealthEncounterTest;
	constexpr double Tick = 1.0 / 60.0;
	const ECommandKind Discrete[] = { ECommandKind::Traverse, ECommandKind::InteractBegin, ECommandKind::InteractEnd, ECommandKind::PrepareBegin,
		ECommandKind::PrepareEnd, ECommandKind::Attack, ECommandKind::HideEnter, ECommandKind::HideExit };

	/** A1 reversed by D; returns the reversal's slot time. */
	inline double Reverse(FRig& R, APawn* A1, APawn* D)
	{
		R.Command(D, ECommandKind::PrepareBegin);
		R.Advance(0.5);
		const double Slot = R.Clock.GetServerTimeSeconds();
		R.AttackRef(A1, D);
		return Slot;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationCounterIsNotProtectedTest, "StealthGame.Assassination.CounterIsNotProtected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationCounterIsNotProtectedTest::RunTest(const FString& Parameters)
{
	using namespace CounterIsNotProtectedTestData;
	const double Counter = FEncounterParams::Defaults().Public.CounterDurationSec;

	// A2 strikes the countering defender: at the first tick, at 0.6 s and at the last tick of the window.
	for (const double Offset : { Tick, 0.6, Counter - Tick })
	{
		FRig R(TEXT("CounterNotProtected"));
		APawn* A1 = R.Spawn(FVector::ZeroVector, 0);
		APawn* A2 = R.Spawn(FVector(0.0, 30.0, 0.0), 0);
		APawn* D = R.Spawn(FVector(150.0, 0.0, 0.0), 1);
		Reverse(R, A1, D);
		TestTrue(TEXT("A1 was reversed"), R.Last().Outcome == EAttackOutcome::Reversal);
		R.Clock.Advance(Offset, 1);
		TestTrue(FString::Printf(TEXT("+%.3f s: the defender is Countering"), Offset), R.Core->GetRole(D) == EStrikeRole::Countering);
		R.AttackRef(A2, D);
		TestTrue(FString::Printf(TEXT("+%.3f s: A2 kills the countering defender"), Offset), R.Last().Outcome == EAttackOutcome::Kill && R.Last().Victim == D && R.Core->IsDead(D));
	}

	// Every discrete command is Busy during Countering; the cooldown still counts from the original activation.
	{
		FRig R(TEXT("CounterBusy"));
		APawn* A1 = R.Spawn(FVector::ZeroVector, 0);
		APawn* D = R.Spawn(FVector(150.0, 0.0, 0.0), 1);
		R.Command(D, ECommandKind::PrepareBegin);
		const double Activation = R.State(D)->GetLastPrepareActivation().GetValue();
		R.Advance(0.5);
		const double Slot = R.Clock.GetServerTimeSeconds();
		R.AttackRef(A1, D);
		for (const double At : { 0.2, 0.6, 1.0 })
		{
			R.Clock.Advance(Slot + At - R.Clock.GetServerTimeSeconds(), 1);
			for (const ECommandKind Kind : Discrete)
			{
				const EResolveResult Result = Kind == ECommandKind::Attack ? R.AttackRef(D, A1) : R.Command(D, Kind);
				TestTrue(FString::Printf(TEXT("+%.1f s: kind %d replies Busy"), At, static_cast<int32>(Kind)), Result == EResolveResult::Voided && R.LastReply().Code == ECommandRejectCode::Busy);
			}
		}
		R.Clock.Advance(Slot + Counter + Tick - R.Clock.GetServerTimeSeconds(), 1);
		TestTrue(TEXT("after 1.2 s: no longer Countering"), R.Core->GetRole(D) == EStrikeRole::Cooling);
		TestTrue(TEXT("after 1.2 s: the cooldown end is the original activation + 60 s exactly"),
			R.State(D)->GetCooldownEndServerTime(R.Params.Public.DefenceCooldownSec).GetValue() == Activation + static_cast<double>(R.Params.Public.DefenceCooldownSec));
	}
	return true;
}

#endif
