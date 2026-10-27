// Story asn-008 (production/epics/assassination-defence-reversal/story-008-attacker-lockout-and-attack-lifecycle.md), AC1 /
// AC-ASN-06 NoTargetDependentRejection (ASN-R1). Attacks at an NPC, an Active rival, an informant, an ally, a concealed
// rival (in a bush), an empty open spot, an empty bush and a dead body, each in a fresh round at the same tick: every one
// executes (Attack's CanBegin is Allow; the slot never rejects for the target), the reply (Executed, reply tick) is the
// same, the resolver schedules and submits nothing, each stamps LockoutEnd = t_exec + 1.75, and only
// FStrikeResult.Result differs, matching the fixture table.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/AssassinationAttackPredicate.h"
#include "Tests/Assassination/EncounterTestRig.h"

namespace NoTargetDependentRejectionTestData
{
	enum class ETarget : uint8 { Npc, Rival, Informant, Ally, ConcealedRival, EmptySpot, EmptyBush, DeadBody };

	struct FRow
	{
		ETarget Target;
		const TCHAR* Name;
		EStrikeResult Expected;
	};

	const FRow Table[] = {
		{ ETarget::Npc, TEXT("NPC"), EStrikeResult::WrongTarget },
		{ ETarget::Rival, TEXT("Active rival"), EStrikeResult::RivalEliminated },
		{ ETarget::Informant, TEXT("informant"), EStrikeResult::WrongTarget },
		{ ETarget::Ally, TEXT("ally"), EStrikeResult::NoContact },
		{ ETarget::ConcealedRival, TEXT("concealed rival"), EStrikeResult::RivalEliminated },
		{ ETarget::EmptySpot, TEXT("empty open spot"), EStrikeResult::NoContact },
		{ ETarget::EmptyBush, TEXT("empty bush"), EStrikeResult::NoContact },
		{ ETarget::DeadBody, TEXT("dead body"), EStrikeResult::NoContact },
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationNoTargetDependentRejectionTest, "StealthGame.Assassination.NoTargetDependentRejection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationNoTargetDependentRejectionTest::RunTest(const FString& Parameters)
{
	using namespace StealthEncounterTest;
	using namespace NoTargetDependentRejectionTestData;
	TestTrue(TEXT("Attack's CanBegin is Allow for every target"), UAssassinationAttackPredicate::Evaluate() == FPredicateResult::Allow());

	TOptional<FEncounterCommandReply> First;
	for (const FRow& Row : Table)
	{
		FRig R(TEXT("NoTargetRejection"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		const FVector Point(150.0, 0.0, 0.0);
		APawn* Target = nullptr;
		switch (Row.Target)
		{
		case ETarget::Npc: Target = R.Spawn(Point); break;
		case ETarget::Rival: Target = R.Spawn(Point, 1); break;
		case ETarget::Informant: Target = R.Spawn(Point, 1); R.Roster.Members[Target].State = EParticipantState::Informant; break;
		case ETarget::Ally: Target = R.Spawn(Point, 0); break;
		case ETarget::ConcealedRival: R.AddBush(1, Point, 60.0); Target = R.Spawn(Point, 1); R.Conceal(Target, 1); break;
		case ETarget::EmptySpot: break;
		case ETarget::EmptyBush: R.AddBush(1, Point, 60.0); break;
		case ETarget::DeadBody:
		{
			Target = R.Spawn(Point);
			APawn* Earlier = R.Spawn(FVector(0.0, 30.0, 0.0), 0);
			R.AttackRef(Earlier, Target); // an earlier slot of this tick killed it
			break;
		}
		}
		const int32 ScheduledBefore = R.Queue.Scheduled.Num();
		const double ExecTime = R.Clock.GetServerTimeSeconds();
		const EResolveResult Result = Row.Target == ETarget::ConcealedRival ? R.Attack(A, Point) : (Target != nullptr ? R.AttackRef(A, Target) : R.Attack(A, Point));
		TestTrue(FString::Printf(TEXT("%s: executed (never rejected)"), Row.Name), Result == EResolveResult::Executed && R.LastReply().bExecuted);
		TestEqual(FString::Printf(TEXT("%s: FStrikeResult.Result"), Row.Name), static_cast<int32>(R.Last().StrikeResult.Result), static_cast<int32>(Row.Expected));
		TestTrue(FString::Printf(TEXT("%s: LockoutEnd = t_exec + 1.75"), Row.Name), R.State(A) != nullptr
			&& R.State(A)->GetLockoutEnd() == ExecTime + static_cast<double>(R.Params.Public.AttackAttemptLockoutSec));
		TestEqual(FString::Printf(TEXT("%s: nothing scheduled or submitted by the resolver"), Row.Name), R.Queue.Scheduled.Num(), ScheduledBefore);
		if (!First.IsSet())
		{
			First = R.LastReply();
		}
		TestTrue(FString::Printf(TEXT("%s: the same reply as every other target (code and tick)"), Row.Name), R.LastReply().SameReplyAs(First.GetValue()));
	}
	return true;
}

#endif
