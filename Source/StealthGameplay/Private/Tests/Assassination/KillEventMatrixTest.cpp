// Story asn-007, AC2 / AC-ASN-13 KillEventMatrix (ASN-R10, R11). Kills of an NPC, an Active rival and an informant, and a
// reversal: OnPawnKilled carries (Npc, Strike), (Player, Strike), (Informant, Strike) and (Player, Reversal) with
// Victim = the attacker and Killer = the defender for the reversal; only the rival kill and the reversal request a Roster
// transition (Assassinated / ReversedAgainst); a whiff emits nothing. Assassination keeps no strike counter: a Penalty
// stub's counter changes only through its OnPawnKilled subscriber, and UEncounterService reflects no numeric property.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/EncounterService.h"
#include "Tests/Assassination/EncounterTestRig.h"
#include "UObject/UnrealType.h"

namespace KillEventMatrixTestData
{
	using namespace StealthEncounterTest;

	struct FExpect
	{
		const TCHAR* Name;
		EVictimKind Kind;
		EKillCause Cause;
		bool bTransition;
		ERosterReason Reason;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationKillEventMatrixTest, "StealthGame.Assassination.KillEventMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationKillEventMatrixTest::RunTest(const FString& Parameters)
{
	using namespace KillEventMatrixTestData;
	const FExpect Cases[] = {
		{ TEXT("NPC kill"), EVictimKind::Npc, EKillCause::Strike, false, ERosterReason::None },
		{ TEXT("Active rival kill"), EVictimKind::Player, EKillCause::Strike, true, ERosterReason::Assassinated },
		{ TEXT("informant kill"), EVictimKind::Informant, EKillCause::Strike, false, ERosterReason::None },
		{ TEXT("reversal"), EVictimKind::Player, EKillCause::Reversal, true, ERosterReason::ReversedAgainst },
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Cases); ++Index)
	{
		const FExpect& Expect = Cases[Index];
		FRig R(TEXT("KillEventMatrix"));
		int32 PenaltyCounter = 0; // written only by its subscriber
		R.Core->OnPawnKilled.AddLambda([&PenaltyCounter](APawn*, APawn*, EVictimKind Kind, EKillCause Cause) { PenaltyCounter += Kind == EVictimKind::Npc && Cause == EKillCause::Strike ? 1 : 0; });
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* V = Index == 0 || Index == 3 ? R.Spawn(FVector(150.0, 0.0, 0.0)) : R.Spawn(FVector(150.0, 0.0, 0.0), 1);
		if (Index == 2)
		{
			R.Roster.Members[V].State = EParticipantState::Informant;
		}
		if (Index == 3)
		{
			R.Command(V, ECommandKind::PrepareBegin); // an NPC defender braced earlier in the round
			R.Advance(1.0);
		}
		R.AttackRef(A, V);
		const bool bOne = R.Kills.Num() == 1;
		TestTrue(FString::Printf(TEXT("%s: exactly one OnPawnKilled"), Expect.Name), bOne);
		if (bOne)
		{
			const FKill& Kill = R.Kills[0];
			TestTrue(FString::Printf(TEXT("%s: (kind, cause)"), Expect.Name), Kill.Kind == Expect.Kind && Kill.Cause == Expect.Cause);
			TestTrue(FString::Printf(TEXT("%s: victim and killer"), Expect.Name), Index == 3 ? (Kill.Victim == A && Kill.Killer == V) : (Kill.Victim == V && Kill.Killer == A));
		}
		TestEqual(FString::Printf(TEXT("%s: Roster transitions"), Expect.Name), R.Roster.Transitions.Num(), Expect.bTransition ? 1 : 0);
		if (Expect.bTransition && R.Roster.Transitions.Num() == 1)
		{
			TestTrue(FString::Printf(TEXT("%s: transition reason and subject"), Expect.Name), R.Roster.Transitions[0].Reason == Expect.Reason
				&& R.Roster.Transitions[0].Pawn == (Index == 3 ? A : V));
		}
		TestEqual(FString::Printf(TEXT("%s: the Penalty stub counts only Strike x Npc"), Expect.Name), PenaltyCounter, Index == 0 ? 1 : 0);
	}

	// A whiff emits nothing.
	{
		FRig R(TEXT("KillEventMatrixWhiff"));
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		R.Attack(A, FVector(150.0, 0.0, 0.0));
		TestTrue(TEXT("whiff: no OnPawnKilled and no transition"), R.Kills.Num() == 0 && R.Roster.Transitions.Num() == 0 && R.Last().Outcome == EAttackOutcome::Whiff);
	}

	// No strike counter in the module's facade: no integer or float reflected property.
	int32 Numeric = 0;
	for (TFieldIterator<FProperty> It(UEncounterService::StaticClass()); It; ++It)
	{
		Numeric += It->IsA<FNumericProperty>() ? 1 : 0;
	}
	TestEqual(TEXT("UEncounterService exposes no integer or float UPROPERTY"), Numeric, 0);
	return true;
}

#endif
