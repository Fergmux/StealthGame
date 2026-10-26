// Story rmf-056 (production/epics/round-match-framework/story-056-round-ended-arrival-order.md), GDD AC 12d / R9 "Arrival
// order"; ADR-0002 8.5. Model level (the story's QA spec allows it while the Teammate Markers and HUD consumers do not
// exist): a strike decides the round through the real staging (the server side, the round-end rig); the server's
// round-end step writes the GameState and the Roster records in one frame. Two clients receive them with an injected
// skew: client A gets the Roster records (RoundEnded) one net frame before Phase = Ended, client B both together. Each
// client is sampled every net frame through StealthRoundArrival::ResolveRecordState, the rule the ally-marker and HUD
// viewer-state consumers apply: no ally marker or viewer state changes before that client's own Phase = Ended.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundEndRig.h"
#include "View/StealthRoundArrivalRule.h"

namespace RoundEndedArrivalOrderTestData
{
	/** One client's received copies: its phase and the ally record's state, and what its consumer shows. */
	struct FClient
	{
		int32 SkewFrames = 0;
		ERoundPhase Phase = ERoundPhase::Active;
		EParticipantState Record = EParticipantState::Active;
		EParticipantState Shown = EParticipantState::Active;
		/** The net frame the consumer's shown state first changed, and the frame Phase = Ended arrived. */
		int32 ShownChangedAt = INDEX_NONE;
		int32 EndedAt = INDEX_NONE;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundRoundEndedArrivalOrderTest, "StealthGame.Round.RoundEndedArrivalOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundRoundEndedArrivalOrderTest::RunTest(const FString& Parameters)
{
	using namespace RoundEndedArrivalOrderTestData;
	// Arrange (server): a strike-decided round (2v1: 3 is team 1's only member).
	FRoundEndRig R;
	R.ToActive(2, 1);
	R.ScriptRoundEnd();
	R.DrainOneTick({ R.Strike(1, 1, 3) });
	TestTrue(TEXT("server: latched, round-end step pending (strike staging)"), R.Rig.Core->IsRoundEndPending());
	R.RunUntil(R.Rig.Clock.GetServerTimeSeconds() + RoundTestFixtures::StrikeTvSec);
	TestTrue(TEXT("server: Ended at the death write"), R.Rig.Core->GetPhase() == ERoundPhase::Ended);
	TestEqual(TEXT("server: EndRoundAll in the same step"), R.Rig.CountSince(0, TEXT("EndRoundAll")), 1);

	// Act (clients): the server's step is net frame 0 of the delivery; the Roster carrier reaches A first.
	FClient Clients[2];
	Clients[0].SkewFrames = 1; // A: Phase = Ended one net frame after the RoundEnded record
	Clients[1].SkewFrames = 0; // B: both in the same net frame
	for (int32 Frame = 0; Frame < 4; ++Frame)
	{
		for (FClient& Client : Clients)
		{
			Client.Record = EParticipantState::RoundEnded; // the record arrives at net frame 0 for both
			if (Frame >= Client.SkewFrames && Client.EndedAt == INDEX_NONE)
			{
				Client.Phase = R.Rig.Core->GetPhaseCore().MakePublicState().Phase;
				Client.EndedAt = Frame;
			}
			const EParticipantState Next = StealthRoundArrival::ResolveRecordState(Client.Phase, Client.Record, Client.Shown);
			if (Next != Client.Shown && Client.ShownChangedAt == INDEX_NONE)
			{
				Client.ShownChangedAt = Frame;
			}
			Client.Shown = Next;
		}
	}

	// Assert.
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FClient& Client = Clients[Index];
		const TCHAR* Name = Index == 0 ? TEXT("client A (Roster first)") : TEXT("client B");
		TestEqual(FString::Printf(TEXT("%s: Phase = Ended arrived at its skew"), Name), Client.EndedAt, Client.SkewFrames);
		TestTrue(FString::Printf(TEXT("%s: the ally marker / viewer state never changed before Phase = Ended"), Name),
			Client.ShownChangedAt == INDEX_NONE || Client.ShownChangedAt >= Client.EndedAt);
		TestTrue(FString::Printf(TEXT("%s: it moves with the phase"), Name), Client.Shown == EParticipantState::RoundEnded);
	}
	// The rule itself: a RoundEnded before Ended is no change; any other state passes.
	TestTrue(TEXT("rule: RoundEnded in Active is no change"), StealthRoundArrival::ResolveRecordState(ERoundPhase::Active, EParticipantState::RoundEnded, EParticipantState::Informant) == EParticipantState::Informant);
	TestTrue(TEXT("rule: Informant in Active passes"), StealthRoundArrival::ResolveRecordState(ERoundPhase::Active, EParticipantState::Informant, EParticipantState::Active) == EParticipantState::Informant);
	TestTrue(TEXT("rule: RoundEnded in Ended passes"), StealthRoundArrival::ResolveRecordState(ERoundPhase::Ended, EParticipantState::RoundEnded, EParticipantState::Active) == EParticipantState::RoundEnded);
	return true;
}

#endif
