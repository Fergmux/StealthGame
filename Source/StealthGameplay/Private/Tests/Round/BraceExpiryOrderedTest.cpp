// Story rmf-050 (production/epics/round-match-framework/story-050-brace-expiry-ordered.md), GDD AC 21 / R12-R14:
// a defence-window expiry (TimerFired(DefenceWindowEnd)) and a strike on that defender land in one tick. Expiry key lower:
// the defender is no longer prepared and the strike lands (victim Informant, Assassinated). Strike key lower: the
// defender is still prepared and the strike is reversed (attacker Informant, ReversedAgainst). The order is decided by
// Compare(Key(expiry), Key(strike)) alone; both keys are forced by submitting the two items from one mailbox in one tick
// (FIFO), as in story rmf-045. The outcome executes inside the strike's slot and carries the strike's key (R14 (2)).
// 3v3, so neither removal ends the round.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundEndRig.h"

namespace BraceExpiryOrderedTestData
{
	constexpr uint32 Attacker = 1; // team 0
	constexpr uint32 Defender = 4; // team 1

	struct FOutcome
	{
		uint32 Fallen = 0;
		ERosterReason Reason = ERosterReason::None;
		FQueueKey TransitionKey;
		FQueueKey StrikeKey;
		FQueueKey ExpiryKey;
	};

	FOutcome Run(bool bExpiryFirst)
	{
		FRoundEndRig R;
		R.ToActive(3, 3);
		FStealthStubResolver Expiries;
		R.Queue->RegisterTimerResolver(ETimerKind::DefenceWindowEnd, &Expiries);
		bool bPrepared = true; // the defender braced earlier (Assassination's state; the stub stands in)
		FOutcome Out;
		Expiries.OnResolve = [&](const FQueueItem& Item, FQueueKey Key)
		{
			Out.ExpiryKey = Key;
			bPrepared = false;
			return EResolveResult::Executed;
		};
		// R14 (1): the lower key acts first; the strike's outcome executes in its own slot.
		R.Strikes.OnResolve = [&](const FQueueItem& Item, FQueueKey Key)
		{
			Out.StrikeKey = Key;
			if (bPrepared)
			{
				R.Transition(Attacker, EParticipantState::Informant, ERosterReason::ReversedAgainst);
			}
			else
			{
				R.Transition(Defender, EParticipantState::Informant, ERosterReason::Assassinated);
			}
			return EResolveResult::Executed;
		};
		FTimerPayload Timer;
		Timer.Kind = ETimerKind::DefenceWindowEnd;
		Timer.Subject = FSubjectRef::ForParticipant(FParticipantId(Defender));
		const FQueueItem Expiry = FQueueItem::Make(EQueueItemType::TimerFired, R.Support.Epoch.Current, FMailboxId::ServerLocal(), Timer);
		const FQueueItem Strike = R.Support.MakeItem(EQueueItemType::PawnCommand, 1);
		if (bExpiryFirst)
		{
			R.DrainOneTick({ Expiry, Strike });
		}
		else
		{
			R.DrainOneTick({ Strike, Expiry });
		}
		const TArray<FRoundLogEntry> Transitions = R.Logged(ERoundLogType::StateTransition);
		if (Transitions.Num() == 1)
		{
			Out.Fallen = Transitions[0].Participants[0];
			Out.Reason = static_cast<ERosterReason>(Transitions[0].Code);
			Out.TransitionKey = Transitions[0].Key;
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundBraceExpiryOrderedTest, "StealthGame.Round.BraceExpiryOrdered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundBraceExpiryOrderedTest::RunTest(const FString& Parameters)
{
	using namespace BraceExpiryOrderedTestData;
	// Act + Assert: expiry key lower -> the strike lands.
	const FOutcome Lands = Run(/*bExpiryFirst*/ true);
	TestTrue(TEXT("expiry first: same tick"), Lands.ExpiryKey.ServerTickIndex == Lands.StrikeKey.ServerTickIndex);
	TestTrue(TEXT("expiry first: the expiry key is lower"), FResolutionQueueCore::Compare(Lands.ExpiryKey, Lands.StrikeKey) < 0);
	TestEqual(TEXT("expiry first: the victim falls"), Lands.Fallen, Defender);
	TestTrue(TEXT("expiry first: reason Assassinated"), Lands.Reason == ERosterReason::Assassinated);
	TestTrue(TEXT("expiry first: the outcome carries the strike's key"), Lands.TransitionKey == Lands.StrikeKey);

	// Strike key lower -> reversal.
	const FOutcome Reversed = Run(/*bExpiryFirst*/ false);
	TestTrue(TEXT("strike first: same tick"), Reversed.ExpiryKey.ServerTickIndex == Reversed.StrikeKey.ServerTickIndex);
	TestTrue(TEXT("strike first: the strike key is lower"), FResolutionQueueCore::Compare(Reversed.StrikeKey, Reversed.ExpiryKey) < 0);
	TestEqual(TEXT("strike first: the attacker falls"), Reversed.Fallen, Attacker);
	TestTrue(TEXT("strike first: reason ReversedAgainst"), Reversed.Reason == ERosterReason::ReversedAgainst);
	TestTrue(TEXT("strike first: the outcome carries the strike's key"), Reversed.TransitionKey == Reversed.StrikeKey);
	return true;
}

#endif
