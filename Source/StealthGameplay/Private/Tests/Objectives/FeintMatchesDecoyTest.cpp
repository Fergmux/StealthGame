// Story oc-007 (production/epics/objectives-chain/story-007-interact-predicate-and-feint-parity.md), AC2 / AC-OC-30
// FeintMatchesDecoy: GIVEN an NPC decoy tend (an unbound pawn) and a human feint (a team-0-bound pawn) at the same non-live
// court THEN both pass the same CanBegin, run the same IIF path (the hold stand-in, through Round's queue), emit identical
// action-hub events (Started then Completed, Interact; pawn ids normalised out), take the same time to complete, and produce
// the same outputs towards clients from Objectives: no team-view push and no rival release (the recording stub driver of
// everything Objectives could send). Neither changes any site state and neither logs ObjectiveStage. The only difference is
// the server-only TendCompleted(NoEffect) measurement entry of the bound feint (never replicated; ADR-0020 5.4). The byte-exact
// multi-client replication capture is story oc-020's.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesQueueRig.h"

namespace ObjectivesFeintMatchesDecoyTestData
{
	using namespace StealthObjectivesTest;

	/** One run of a tend at the non-live court: what every observer and client could receive. */
	struct FRun
	{
		FPredicateResult CanBegin;
		TArray<FString> Events;
		double CompletedAfterSec = -1.0;
		int32 ViewPushes = 0;
		int32 RivalReleases = 0;
		int32 Stages = 0;
		bool bStatesUnchanged = false;
		int32 TendEntries = 0;
	};

	FRun TendOnce(bool bHuman)
	{
		FQueueRig Rig;
		FRun Run;
		Rig.ToActive();
		AStealthPawn* Pawn = bHuman ? Rig.Pawn(CourtP2Team0, 0, 1, 0) : Rig.Pawn(CourtP2Team0, 0);
		const TArray<EObjectiveSiteState> Before = [&Rig]()
		{
			TArray<EObjectiveSiteState> Out;
			for (int32 Team = 0; Team < 2; ++Team)
			{
				for (int32 Def = 0; Def < Rig.Core().GetDefinitionCount(); ++Def)
				{
					Out.Add(Rig.Core().GetSiteState(Team, Def));
				}
			}
			return Out;
		}();
		const int32 PushesBefore = Rig.Views.Pushes.Num();
		double BeginTime = 0.0;
		Rig.Hub->OnActionStarted.AddLambda([&Run, &BeginTime, &Rig](AStealthPawn*, EActionType Type)
		{
			Run.Events.Add(FString::Printf(TEXT("Started %d"), static_cast<int32>(Type)));
			BeginTime = Rig.Q.Clock.GetServerTimeSeconds();
		});
		Rig.Hub->OnActionCompleted.AddLambda([&Run, &BeginTime, &Rig](AStealthPawn*, EActionType Type)
		{
			Run.Events.Add(FString::Printf(TEXT("Completed %d"), static_cast<int32>(Type)));
			Run.CompletedAfterSec = Rig.Q.Clock.GetServerTimeSeconds() - BeginTime;
		});
		Rig.Hub->OnActionCancelled.AddLambda([&Run](AStealthPawn*, EActionType Type, ECancelReason Reason)
		{
			Run.Events.Add(FString::Printf(TEXT("Cancelled %d %d"), static_cast<int32>(Type), static_cast<int32>(Reason)));
		});
		FCommandPayload Cmd;
		Cmd.Target = FSubjectRef::ForCourtSocket(CourtP2Team0);
		Run.CanBegin = Rig.Service->CanBegin(*Pawn, Cmd);
		Rig.Begin(Pawn, CourtP2Team0);
		Rig.Run(3.5);
		Run.ViewPushes = Rig.Views.Pushes.Num() - PushesBefore;
		Run.RivalReleases = Rig.Rival.Calls.Num();
		Run.Stages = Rig.Logged(ERoundLogType::ObjectiveStage);
		TArray<EObjectiveSiteState> After;
		for (int32 Team = 0; Team < 2; ++Team)
		{
			for (int32 Def = 0; Def < Rig.Core().GetDefinitionCount(); ++Def)
			{
				After.Add(Rig.Core().GetSiteState(Team, Def));
			}
		}
		Run.bStatesUnchanged = After == Before;
		Run.TendEntries = Rig.Logged(ERoundLogType::TendCompleted);
		Rig.Hub->OnActionStarted.Clear();
		Rig.Hub->OnActionCompleted.Clear();
		Rig.Hub->OnActionCancelled.Clear();
		return Run;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesFeintMatchesDecoyTest, "StealthGame.Objectives.FeintMatchesDecoy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesFeintMatchesDecoyTest::RunTest(const FString& Parameters)
{
	using namespace ObjectivesFeintMatchesDecoyTestData;

	const FRun Decoy = TendOnce(/*bHuman*/ false);
	const FRun Feint = TendOnce(/*bHuman*/ true);

	TestTrue(TEXT("the same CanBegin (Allow)"), Decoy.CanBegin == Feint.CanBegin && Decoy.CanBegin == FPredicateResult::Allow());
	TestEqual(TEXT("the same hub events (Started, Completed)"), Decoy.Events, Feint.Events);
	TestEqual(TEXT("the decoy's events are Started then Completed"), Decoy.Events.Num(), 2);
	TestEqual(TEXT("the same completion time (D)"), Decoy.CompletedAfterSec, Feint.CompletedAfterSec, 1e-6);
	TestTrue(TEXT("both completed after the hold"), Decoy.CompletedAfterSec >= 3.0 - 1e-6);
	TestEqual(TEXT("no team-view push from either (decoy)"), Decoy.ViewPushes, 0);
	TestEqual(TEXT("no team-view push from either (feint)"), Feint.ViewPushes, 0);
	TestEqual(TEXT("no rival release from either"), Decoy.RivalReleases + Feint.RivalReleases, 0);
	TestEqual(TEXT("no stage counted"), Decoy.Stages + Feint.Stages, 0);
	TestTrue(TEXT("the decoy changes no state"), Decoy.bStatesUnchanged);
	TestTrue(TEXT("the feint changes no state"), Feint.bStatesUnchanged);
	TestEqual(TEXT("server-only: the unbound decoy logs no TendCompleted"), Decoy.TendEntries, 0);
	TestEqual(TEXT("server-only: the bound feint logs one TendCompleted(NoEffect)"), Feint.TendEntries, 1);
	return true;
}

#endif
