// Story psr-062 (production/epics/participant-state-roster/story-062-team-view-component-and-push-step.md):
// URosterTeamViewComponent and the Roster push step write C-TeamView change-only, once per frame.
//
//   StealthGame.Roster.TeamViewPushStep      AC-PSR-62a: a 2v2 store with one component per controller; several
//                                            Roster events for one team in one frame write each owner of that team
//                                            once, carry own-team data only, and an unchanged frame writes nothing.
//   StealthGame.Roster.TeamViewReconnectFlag AC-PSR-62b: bReconnecting is set on a Disconnected projection in phase
//                                            Active and cleared only by the event (rejoin -> Rejoined, grace expiry
//                                            -> GraceExpired, kick -> Cleared, round end -> Cleared); no timer clears
//                                            it; outside phase Active it stays false.
//
// 62a runs through URosterStore (the facade owns the push step); 62b drives FRosterPushStep over the core fixture so
// the round-end event (EndRoundAll is story psr-023) can be delivered directly. Components are created without an
// owning controller: the contract under test is the write count and content, not the replication channel.

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Roster/StealthRosterPushStep.h"
#include "Tests/Roster/StealthRosterTestFixture.h"
#include "Server/Roster/StealthRosterStore.h"
#include "Server/Testing/StealthQueueTestRig.h"
#include "View/StealthRosterTeamView.h"

namespace RosterPushStepTestData
{
	constexpr int32 TeamSize = 2;
	constexpr int32 Players = 4;
	/** A long wait that a timer would have used to clear the flag (none may). */
	constexpr double LongWaitSeconds = 600.0;

	inline FIdentityKey Key(int32 Index) { return FIdentityKey(0x9057E00000000000ull, 300ull + static_cast<uint64>(Index)); }

	class FFakeRoundPhase : public IRosterRoundPhase
	{
	public:
		bool bActive = false;
		virtual bool IsRoundPhaseActive() const override { return bActive; }
	};

	/** The teammate entry for Handle in View, or nullptr. */
	const FTeamViewTeammateEntry* FindMate(const FTeamView& View, uint32 Handle)
	{
		return View.Teammates.FindByPredicate([Handle](const FTeamViewTeammateEntry& E) { return E.Handle == Handle; });
	}
}

// -------------------------------------------------------------------------------------------------------------------
// AC-PSR-62a
// -------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterTeamViewPushStepTest, "StealthGame.Roster.TeamViewPushStep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterTeamViewPushStepTest::RunTest(const FString& Parameters)
{
	using namespace RosterPushStepTestData;

	// Arrange: a 2v2 store (players 0,1 on team 0; 2,3 on team 1), everyone Active, phase Active, one view each.
	FStealthRecordingFaultSink Sink;
	FStealthFakeEntropySource Entropy(RoundTestFixtures::SecretS1);
	FFakeStealthClock Clock(10.0);
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	GameInstance->AddToRoot();
	URosterStore* Store = NewObject<URosterStore>(GameInstance);
	Store->AddToRoot();
	FRosterStoreInitOverrides Overrides;
	Overrides.bAuthority = true;
	Overrides.FaultSink = &Sink;
	Overrides.Entropy = &Entropy;
	Overrides.Clock = &Clock;
	Store->InitializeStore(Overrides);
	Store->OpenSession(TeamSize);
	TArray<FParticipantId> Ids;
	TArray<FRosterTeamAssignment> Batch;
	for (int32 Index = 0; Index < Players; ++Index)
	{
		Ids.Add(Store->RegisterParticipant(Key(Index), FString::Printf(TEXT("P%d"), Index)).Id);
		Batch.Add({ Ids[Index], Index < TeamSize ? 0 : 1 });
	}
	Store->AssignTeams(Batch);
	Store->SetTeamsLocked(true);
	Store->ResetAll(1);
	FFakeRoundPhase Phase;
	Phase.bActive = true;
	URosterTeamViewComponent* PhaseOwner = NewObject<URosterTeamViewComponent>(GetTransientPackage());
	Store->RegisterRoundPhase(&Phase, PhaseOwner);

	TArray<URosterTeamViewComponent*> Views;
	for (int32 Index = 0; Index < Players; ++Index)
	{
		URosterTeamViewComponent* View = NewObject<URosterTeamViewComponent>(GetTransientPackage());
		View->AddToRoot();
		Views.Add(View);
		Store->RegisterTeamView(Ids[Index], View);
	}
	TestEqual(TEXT("first frame writes every registered view once"), Store->FlushTeamViews(), Players);
	for (int32 Index = 0; Index < Players; ++Index)
	{
		TestEqual(FString::Printf(TEXT("view %d written once on the first frame"), Index), Views[Index]->GetServerWriteCount(), 1);
	}

	// Own-team data only.
	const uint32 Handles[Players] = { Store->FindRecord(Ids[0])->PublicHandle, Store->FindRecord(Ids[1])->PublicHandle,
		Store->FindRecord(Ids[2])->PublicHandle, Store->FindRecord(Ids[3])->PublicHandle };
	for (int32 Index = 0; Index < Players; ++Index)
	{
		const FTeamView& View = Views[Index]->GetView();
		const int32 Mate = Index ^ 1;
		TestEqual(FString::Printf(TEXT("view %d carries the owner's handle"), Index), View.Owner.Handle, Handles[Index]);
		TestEqual(FString::Printf(TEXT("view %d has exactly one teammate (2v2)"), Index), View.Teammates.Num(), 1);
		TestNotNull(FString::Printf(TEXT("view %d's teammate is its own-team mate"), Index), FindMate(View, Handles[Mate]));
		for (int32 Other = 0; Other < Players; ++Other)
		{
			if ((Other < TeamSize) != (Index < TeamSize))
			{
				TestNull(FString::Printf(TEXT("view %d carries no opponent %d"), Index, Other), FindMate(View, Handles[Other]));
			}
		}
	}

	// Act: several Roster events for team 0 in one frame (both members disconnect, one rejoins).
	Store->NotifyConnectionChanged(Ids[0], ERosterConnection::Disconnected);
	Store->NotifyConnectionChanged(Ids[1], ERosterConnection::Disconnected);
	Store->NotifyConnectionChanged(Ids[1], ERosterConnection::Connected);
	const int32 Written = Store->FlushTeamViews();

	// Assert: each team-0 owner written once (coalesced), team 1 not at all.
	TestEqual(TEXT("one frame of team-0 events writes the two team-0 views"), Written, 2);
	TestEqual(TEXT("owner 0 written once more"), Views[0]->GetServerWriteCount(), 2);
	TestEqual(TEXT("owner 1 written once more"), Views[1]->GetServerWriteCount(), 2);
	TestEqual(TEXT("owner 2 (team 1) not written"), Views[2]->GetServerWriteCount(), 1);
	TestEqual(TEXT("owner 3 (team 1) not written"), Views[3]->GetServerWriteCount(), 1);
	const FTeamViewTeammateEntry* MateOf1 = FindMate(Views[1]->GetView(), Handles[0]);
	TestTrue(TEXT("owner 1 sees teammate 0 reconnecting"), MateOf1 != nullptr && MateOf1->bReconnecting);

	// An unchanged frame writes nothing.
	TestEqual(TEXT("an unchanged frame writes nothing"), Store->FlushTeamViews(), 0);
	for (int32 Index = 0; Index < Players; ++Index)
	{
		TestEqual(FString::Printf(TEXT("view %d unchanged count"), Index), Views[Index]->GetServerWriteCount(), Index < TeamSize ? 2 : 1);
	}

	for (URosterTeamViewComponent* View : Views)
	{
		View->RemoveFromRoot();
	}
	Store->ShutdownStore();
	Store->RemoveFromRoot();
	GameInstance->RemoveFromRoot();
	return true;
}

// -------------------------------------------------------------------------------------------------------------------
// AC-PSR-62b
// -------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterTeamViewReconnectFlagTest, "StealthGame.Roster.TeamViewReconnectFlag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterTeamViewReconnectFlagTest::RunTest(const FString& Parameters)
{
	using namespace RosterPushStepTestData;

	// Arrange: a 3v3 core fixture (0,1,2 | 3,4,5), everyone Active, a push step with one view per participant.
	FRosterTestFixture F;
	const TArray<FParticipantId> Ids = F.BeginAndRegister(3, 6);
	F.AssignSplit(Ids, 3);
	F.Core.SetTeamsLocked(true);
	F.Core.ResetAll(1);
	FRosterPushStep Push(F.Core, F.RoundPhase, FRosterPushStep::FPawnResolver());
	F.Core.AddListener(&Push);
	TArray<URosterTeamViewComponent*> Views;
	for (const FParticipantId& Id : Ids)
	{
		URosterTeamViewComponent* View = NewObject<URosterTeamViewComponent>(GetTransientPackage());
		View->AddToRoot();
		Views.Add(View);
		Push.RegisterView(Id, View);
	}
	Push.Flush();
	auto Handle = [&F, &Ids](int32 Index) { return F.Core.FindRecord(Ids[Index])->PublicHandle; };
	auto MateFlag = [&Views, &Handle](int32 Viewer, int32 Mate) -> const FTeamViewTeammateEntry*
	{
		return FindMate(Views[Viewer]->GetView(), Handle(Mate));
	};

	// Outside phase Active the flag stays false.
	F.RoundPhase.bActive = false;
	F.Core.NotifyConnectionChanged(Ids[0], ERosterConnection::Disconnected, 1000);
	Push.Flush();
	TestFalse(TEXT("outside Active: a disconnect does not set bReconnecting"), MateFlag(1, 0)->bReconnecting);
	F.Core.NotifyConnectionChanged(Ids[0], ERosterConnection::Connected, 1100);
	Push.Flush();
	TestTrue(TEXT("outside Active: no cause is recorded"), MateFlag(1, 0)->ReconnectEndCause == EReconnectEndCause::None);

	F.RoundPhase.bActive = true;

	// (1) Rejoin -> Rejoined; no timer clears the flag before it.
	F.Core.NotifyConnectionChanged(Ids[0], ERosterConnection::Disconnected, 2000);
	Push.Flush();
	TestTrue(TEXT("rejoin case: set on the disconnect"), MateFlag(1, 0)->bReconnecting);
	F.Clock.AdvanceSeconds(LongWaitSeconds);
	Push.Flush();
	TestTrue(TEXT("no timer clears the flag"), MateFlag(1, 0)->bReconnecting && MateFlag(2, 0)->bReconnecting);
	F.Core.NotifyConnectionChanged(Ids[0], ERosterConnection::Connected, 3000);
	Push.Flush();
	TestFalse(TEXT("rejoin clears the flag"), MateFlag(1, 0)->bReconnecting);
	TestTrue(TEXT("rejoin cause is Rejoined"), MateFlag(1, 0)->ReconnectEndCause == EReconnectEndCause::Rejoined);

	// (2) Grace expiry -> GraceExpired (the Disconnect transition with the captured generation).
	F.Core.NotifyConnectionChanged(Ids[1], ERosterConnection::Disconnected, 4000);
	Push.Flush();
	TestTrue(TEXT("grace case: set on the disconnect"), MateFlag(0, 1)->bReconnecting);
	FRosterTransitionOptions Grace;
	Grace.ExpectedConnectionGeneration = F.Core.FindRecord(Ids[1])->Connection.ConnectionGeneration;
	TestTrue(TEXT("grace expiry transition applies"), F.Core.RequestTransition(Ids[1], EParticipantState::Informant, ERosterReason::Disconnect, Grace) == ERosterResult::Applied);
	Push.Flush();
	TestFalse(TEXT("grace expiry clears the flag"), MateFlag(0, 1)->bReconnecting);
	TestTrue(TEXT("grace expiry cause is GraceExpired"), MateFlag(0, 1)->ReconnectEndCause == EReconnectEndCause::GraceExpired);

	// (3) Kick -> Cleared.
	F.Core.NotifyConnectionChanged(Ids[3], ERosterConnection::Disconnected, 5000);
	Push.Flush();
	TestTrue(TEXT("kick case: set on the disconnect"), MateFlag(4, 3)->bReconnecting);
	F.Core.NotifyConnectionChanged(Ids[3], ERosterConnection::Dormant, 5100, /*bKicked*/ true);
	Push.Flush();
	TestFalse(TEXT("a kick clears the flag"), MateFlag(4, 3)->bReconnecting);
	TestTrue(TEXT("kick cause is Cleared"), MateFlag(4, 3)->ReconnectEndCause == EReconnectEndCause::Cleared);

	// (4) Round end -> Cleared.
	F.Core.NotifyConnectionChanged(Ids[4], ERosterConnection::Disconnected, 6000);
	Push.Flush();
	TestTrue(TEXT("round-end case: set on the disconnect"), MateFlag(5, 4)->bReconnecting);
	FRosterEvent RoundEnded;
	RoundEnded.Type = ERosterEventType::RosterRoundEnded;
	Push.OnRosterEvent(RoundEnded);
	Push.Flush();
	TestFalse(TEXT("round end clears the flag"), MateFlag(5, 4)->bReconnecting);
	TestTrue(TEXT("round-end cause is Cleared"), MateFlag(5, 4)->ReconnectEndCause == EReconnectEndCause::Cleared);

	F.Core.RemoveListener(&Push);
	for (URosterTeamViewComponent* View : Views)
	{
		View->RemoveFromRoot();
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS
