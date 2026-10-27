// Story asn-009 (production/epics/assassination-defence-reversal/story-009-same-drain-ordering-and-braced-key-compare.md),
// AC1 / AC-ASN-04 BracedAtAttackKeyOrder (F2, ASN-R5), on the stub-queue harness (explicit keys, no wall-clock compare).
// The defender's Prepare(Begin) at key (1000, 4), its release at (1120, 1) scheduling DebounceExpiry, the debounce item at
// (1124, 2) and CapEnd later: an Attack at (1124, 1) is a reversal (a pending End inside the debounce still protects), at
// (1124, 3) a kill. An Attack at (1000, 3), before the Begin in the same tick, kills, and the Begin is then voided NotNow
// with LastPrepareActivation still none. Key(End) is the first of DebounceExpiry and CapEnd: a CapEnd at (1100, 1) ends it.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Assassination/EncounterTestRig.h"

namespace BracedAtAttackKeyOrderTestData
{
	const FQueueKey BeginKey(1000, 4);
	const FQueueKey ReleaseKey(1120, 1);
	const FQueueKey DebounceKey(1124, 2);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationBracedAtAttackKeyOrderTest, "StealthGame.Assassination.BracedAtAttackKeyOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationBracedAtAttackKeyOrderTest::RunTest(const FString& Parameters)
{
	using namespace StealthEncounterTest;
	using namespace BracedAtAttackKeyOrderTestData;

	// Attack at (1124, 1): before the debounce item.
	{
		FRig R(TEXT("BracedKeyOrderEarly"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* V = R.Spawn(FVector(150.0, 0.0, 0.0), 1);
		R.Command(V, ECommandKind::PrepareBegin, BeginKey);
		R.Command(V, ECommandKind::PrepareEnd, ReleaseKey);
		TestTrue(TEXT("the release scheduled DebounceExpiry"), R.Queue.Scheduled.Num() == 2);
		R.AttackRef(A, V, FQueueKey(1124, 1));
		TestTrue(TEXT("(1124, 1): reversal (the pending End still protects)"), R.Last().Outcome == EAttackOutcome::Reversal);
		TestTrue(TEXT("the debounce item after it finds the window already ended"), R.FireHold(V, EHoldTimerKind::DebounceExpiry, DebounceKey) == EResolveResult::Voided);
	}
	// Attack at (1124, 3): after the debounce item.
	{
		FRig R(TEXT("BracedKeyOrderLate"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* V = R.Spawn(FVector(150.0, 0.0, 0.0), 1);
		R.Command(V, ECommandKind::PrepareBegin, BeginKey);
		R.Command(V, ECommandKind::PrepareEnd, ReleaseKey);
		TestTrue(TEXT("the debounce item ends the window"), R.FireHold(V, EHoldTimerKind::DebounceExpiry, DebounceKey) == EResolveResult::Executed);
		TestTrue(TEXT("the role is Cooling after the window"), R.Core->GetRole(V) == EStrikeRole::Cooling);
		R.AttackRef(A, V, FQueueKey(1124, 3));
		TestTrue(TEXT("(1124, 3): kill"), R.Last().Outcome == EAttackOutcome::Kill);
	}
	// Key(End) = min(DebounceExpiry, CapEnd): a CapEnd first ends the window.
	{
		FRig R(TEXT("BracedKeyOrderCap"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* V = R.Spawn(FVector(150.0, 0.0, 0.0), 1);
		R.Command(V, ECommandKind::PrepareBegin, BeginKey);
		R.FireHold(V, EHoldTimerKind::CapEnd, FQueueKey(1100, 1));
		R.AttackRef(A, V, FQueueKey(1124, 1));
		TestTrue(TEXT("after CapEnd: kill"), R.Last().Outcome == EAttackOutcome::Kill);
	}
	// Attack at (1000, 3), before the Begin of the same tick.
	{
		FRig R(TEXT("BracedKeyOrderBeforeBegin"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* V = R.Spawn(FVector(150.0, 0.0, 0.0), 1);
		R.AttackRef(A, V, FQueueKey(1000, 3));
		TestTrue(TEXT("(1000, 3): kill"), R.Last().Outcome == EAttackOutcome::Kill);
		const EResolveResult Begin = R.Command(V, ECommandKind::PrepareBegin, BeginKey);
		TestTrue(TEXT("the later Begin is voided NotNow"), Begin == EResolveResult::Voided && R.LastReply().Code == ECommandRejectCode::NotNow);
		TestTrue(TEXT("and writes no LastPrepareActivation stamp"), R.State(V) == nullptr || !R.State(V)->GetLastPrepareActivation().IsSet());
	}
	// The pure compare: Countering, Striking and Dead are never braced, whatever the keys.
	{
		FDefenceStateCore Open;
		Open.BeginPrepare(0.0, 60.0, BeginKey);
		TestTrue(TEXT("Bracing with an open window: braced"), FBracedAtAttackCore::IsBraced(EStrikeRole::Bracing, &Open, FQueueKey(1124, 1)));
		for (const EStrikeRole Role : { EStrikeRole::Countering, EStrikeRole::Striking, EStrikeRole::Dead, EStrikeRole::Cooling, EStrikeRole::Ready })
		{
			TestFalse(FString::Printf(TEXT("role %d: never braced"), static_cast<int32>(Role)), FBracedAtAttackCore::IsBraced(Role, &Open, FQueueKey(1124, 1)));
		}
		TestFalse(TEXT("the Begin's own key is not after the Begin"), FBracedAtAttackCore::IsBraced(EStrikeRole::Bracing, &Open, BeginKey));
	}
	return true;
}

#endif
