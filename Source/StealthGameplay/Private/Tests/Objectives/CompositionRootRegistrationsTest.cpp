// Story oc-005 (production/epics/objectives-chain/story-005-service-shell-and-composition-root.md), AC3 (ADR-0020 sections 3, 5)
// CompositionRootRegistrations, with the composition root's own registration calls over a bare world's UActionHub and a real
// queue (recording fault sinks):
//   HandleLayoutReady(LayoutId, Seed) then HandleRoundActive(Epoch, ActivePhaseStart): the layout is bound, t0 is stored, both
//   teams' Prelim1 sites are Live; the service is registered weakly as ICommandPredicate(Interact), IInteractTargetOwner and
//   the TimerFired(StageSignal) resolver.
//   HandleRoundActive before HandleLayoutReady reports one fault and changes nothing; a second HandleRoundActive for the same
//   epoch is ignored.
//   Destroying the service (Unbind + garbage) and then resolving each registration reports exactly once per resolve: the
//   hub's CommandNoPredicate (Interact) and DeadRegistration (the owner); the queue's QueueNoResolver for a StageSignal item.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesQueueRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesCompositionRootRegistrationsTest, "StealthGame.Objectives.CompositionRootRegistrations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesCompositionRootRegistrationsTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;

	// Active before the layout: one fault, nothing changes.
	{
		FQueueRig Rig;
		Rig.Service->ValidateSelectedChain(FStealthServerTuning());
		const int32 FaultsBefore = Rig.Q.Faults.GetRecords().Num();
		TestFalse(TEXT("HandleRoundActive before HandleLayoutReady is refused"), Rig.Service->HandleRoundActive(Rig.Q.Epoch.Current, 5.0));
		TestEqual(TEXT("it reports one fault"), Rig.Q.Faults.GetRecords().Num(), FaultsBefore + 1);
		TestFalse(TEXT("nothing is Active"), Rig.Core().IsActive());
	}

	FQueueRig Rig;
	constexpr double ActiveStart = 12.5;
	TestTrue(TEXT("the chain is selected"), Rig.Service->ValidateSelectedChain(FStealthServerTuning()).bValid);
	TestTrue(TEXT("HandleLayoutReady binds"), Rig.Service->HandleLayoutReady(42, 0));
	TestEqual(TEXT("the layout id is kept"), static_cast<int64>(Rig.Service->GetLayoutId()), static_cast<int64>(42));
	TestTrue(TEXT("HandleRoundActive runs"), Rig.Service->HandleRoundActive(Rig.Q.Epoch.Current, ActiveStart));
	TestEqual(TEXT("t0 is stored"), Rig.Core().GetActivePhaseStart(), ActiveStart);
	TestEqual(TEXT("team 0's Prelim1 is Live"), static_cast<int32>(Rig.Core().GetSiteState(0, DefP1Team0)), static_cast<int32>(EObjectiveSiteState::Live));
	TestEqual(TEXT("team 1's Prelim1 is Live"), static_cast<int32>(Rig.Core().GetSiteState(1, DefP1Team1)), static_cast<int32>(EObjectiveSiteState::Live));
	const int32 Pushes = Rig.Views.Pushes.Num();
	TestFalse(TEXT("a second HandleRoundActive for the same epoch is ignored"), Rig.Service->HandleRoundActive(Rig.Q.Epoch.Current, ActiveStart + 1.0));
	TestEqual(TEXT("t0 unchanged by the ignored call"), Rig.Core().GetActivePhaseStart(), ActiveStart);
	TestEqual(TEXT("no view pushed by the ignored call"), Rig.Views.Pushes.Num(), Pushes);

	// The three registrations are live.
	TestTrue(TEXT("registered as the Interact predicate"), Rig.Hub->HasPredicate(EActionType::Interact));
	TestTrue(TEXT("registered as the interact-target owner"), Rig.Hub->HasInteractTargetOwner());
	TestTrue(TEXT("registered as the TimerFired(StageSignal) resolver"), Rig.Service->IsStageSignalRegistered());
	TestTrue(TEXT("the queue resolves a StageSignal item through the service (no fault)"), [&Rig]()
	{
		const int32 Before = Rig.Q.Faults.GetRecords().Num();
		Rig.Q.Queue->Submit(FMailboxId::ServerLocal(), Rig.Q.MakeTimer(ETimerKind::StageSignal, 1));
		Rig.Q.Tick();
		return Rig.Q.Faults.GetRecords().Num() == Before;
	}());

	// Destroy a registered service, then resolve each registration once: each reports exactly once. (A second service object
	// stands in for the world's, so the world's subsystem collection stays intact: it is registered exactly as the root does.)
	Rig.Service->Unbind();
	UObjectivesService* Service = NewObject<UObjectivesService>(Rig.World.World);
	FObjectivesServiceInit Init;
	Init.Roster = &Rig.Roster;
	Init.Round = &Rig.Round;
	Init.Queue = Rig.Q.Queue.Get();
	TestTrue(TEXT("the second service binds"), Service->Bind(Init) && Service->IsStageSignalRegistered());
	Rig.Hub->RegisterPredicate(EActionType::Interact, TWeakInterfacePtr<ICommandPredicate>(Service));
	Rig.Hub->RegisterInteractTargetOwner(TWeakInterfacePtr<IInteractTargetOwner>(Service));
	Service->Unbind(); // Deinitialize's step: its queue registration goes
	Service->MarkAsGarbage();
	AStealthPawn* Pawn = Rig.Pawn(CourtP1Team0, 0);
	const int32 HubBefore = Rig.HubFaults.GetRecords().Num();
	FCommandPayload Cmd;
	Cmd.Target = FSubjectRef::ForCourtSocket(CourtP1Team0);
	TestEqual(TEXT("a dead predicate answers Reject"), static_cast<int32>(Rig.Hub->ResolvePredicate(EActionType::Interact, *Pawn, Cmd).Verdict), static_cast<int32>(EPredicateVerdict::Reject));
	TestEqual(TEXT("the dead predicate reports once"), Rig.HubFaults.GetRecords().Num(), HubBefore + 1);
	TestNull(TEXT("a dead owner resolves to null"), Rig.Hub->ResolveInteractTargetOwner());
	TestEqual(TEXT("the dead owner reports once"), Rig.HubFaults.GetRecords().Num(), HubBefore + 2);
	if (Rig.HubFaults.GetRecords().Num() == HubBefore + 2)
	{
		TestTrue(TEXT("the owner's fault is DeadRegistration"), Rig.HubFaults.GetRecords().Last().Fault == EStealthFault::DeadRegistration);
	}
	const int32 QueueBefore = Rig.Q.Faults.GetRecords().Num();
	Rig.Q.Queue->Submit(FMailboxId::ServerLocal(), Rig.Q.MakeTimer(ETimerKind::StageSignal, 2));
	Rig.Q.Tick();
	TestEqual(TEXT("a StageSignal item after the service is gone reports once (QueueNoResolver)"), Rig.Q.Faults.GetRecords().Num(), QueueBefore + 1);
	return true;
}

#endif
