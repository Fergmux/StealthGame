// Objectives Chain story oc-011 (production/epics/objectives-chain/story-011-team-view-component-and-replication.md), AC2
// (ADR-0020 OBJ-V2, AC-OC-09 "0 to 2 in one update") and AC3's PIE half (push model on and off):
// StealthGame.Objectives.PIE.RivalCountOneUpdate.{OneChangePerFrame_PushModelOn, OneChangePerFrame_PushModelOff}.
// Listen server + 2 clients on L_Prototype (NpcCount 0), Net.IsPushModelEnabled set before the PIE session starts (push handles
// are assigned at object creation). On the controllers of the two remote clients (one per team):
//   - the carrier: AStealthPlayerController uses the registered sub-object list and owns the replicated
//     UObjectiveTeamViewComponent as a default subobject;
//   - OBJ-V2: on team A's controller, RivalPrelimCount 0 -> 1 -> 2 written in ONE server frame (the component's writer, which
//     the service's slot writes go through) reaches the client as exactly one change with the final value 2 (and no second
//     change follows); on team B's controller 0 -> 1 and 1 -> 2 written in two frames (the second after the client saw the
//     first) arrive as two changes; every client's observed count never decreases;
//   - the dirty mark: with push model off a write WITHOUT MARK_PROPERTY_DIRTY_FROM_NAME (test seam) still reaches the client
//     (the compare path); with push model on its arrival within 1 s is recorded as an observation only (the engine compares a
//     NetSerialize property after a GC regardless of the mark, so it is not pinnable); in both states the marked write
//     arrives, so with the mark on every write (pinned by ObjectivesAllowlistRow's source check) the view updates either way.
// The writes go straight to the component (ApplyView, the service's only write path) because the rival releases that raise the
// count in production are stories oc-014/015; the carrier, not the release, is what OBJ-V2 verifies.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "Server/ObjectivesService.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/AssassinationPieSupport.h"
#include "View/ObjectiveTeamViewComponent.h"

namespace RivalCountOneUpdatePieData
{
	constexpr double SettleSec = 0.5;
	/** How long an unmarked write must stay unsent with push model on. */
	constexpr double UnmarkedWaitSec = 1.0;
}

NETWORK_TEST_CLASS(RivalCountOneUpdate, "StealthGame.Objectives.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;
	inline static FString PreviousPushModel;

	TWeakObjectPtr<AStealthPlayerController> ServerSide[2];
	TWeakObjectPtr<AStealthPlayerController> Machine[2];
	/** The RivalPrelimCount values each client observed, in order (a new entry on every change of the value). */
	TArray<int32> Seen[2];
	int32 ChangesAtMark[2] = { 0, 0 };
	double Mark = 0.0;
	bool bPushModel = true;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(StealthAssassinationPie::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
		}
		if (IConsoleVariable* Push = IConsoleManager::Get().FindConsoleVariable(TEXT("Net.IsPushModelEnabled")))
		{
			PreviousPushModel = Push->GetString();
		}
		UStealthSettings* Settings = GetMutableDefault<UStealthSettings>();
		PreviousNpcCount = Settings->NpcCount;
		Settings->NpcCount = 0;
	}

	AFTER_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName); Timeout && !PreviousNetworkTimeout.IsEmpty())
		{
			Timeout->Set(*PreviousNetworkTimeout, ECVF_SetByCode);
		}
		if (IConsoleVariable* Push = IConsoleManager::Get().FindConsoleVariable(TEXT("Net.IsPushModelEnabled")); Push && !PreviousPushModel.IsEmpty())
		{
			Push->Set(*PreviousPushModel, ECVF_SetByCode);
		}
		if (PreviousNpcCount >= 0)
		{
			GetMutableDefault<UStealthSettings>()->NpcCount = PreviousNpcCount;
		}
	}

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FBasePIENetworkComponentState>().AsListenServer().WithClients(2).WithGameMode(AStealthGameMode::StaticClass()).Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
	}

	static double Now() { return StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()); }

	UObjectiveTeamViewComponent* ServerView(int32 Team) const { return ServerSide[Team].IsValid() ? ServerSide[Team]->GetObjectiveTeamView() : nullptr; }
	const UObjectiveTeamViewComponent* ClientView(int32 Team) const { return Machine[Team].IsValid() ? Machine[Team]->GetObjectiveTeamView() : nullptr; }

	/** Records a change of each client's observed count. */
	void Sample()
	{
		for (int32 Team = 0; Team < 2; ++Team)
		{
			if (const UObjectiveTeamViewComponent* View = ClientView(Team))
			{
				const int32 Value = View->GetView().RivalPrelimCount;
				if (Seen[Team].Num() == 0 || Seen[Team].Last() != Value)
				{
					Seen[Team].Add(Value);
				}
			}
		}
	}

	FObjectiveTeamView WithRival(int32 Team, uint8 Count) const
	{
		FObjectiveTeamView View = ServerView(Team)->GetView();
		View.RivalPrelimCount = Count;
		return View;
	}

	void Wait(const TCHAR* Label, double Seconds)
	{
		Network
			.Then(*FString::Printf(TEXT("%s: mark"), Label), [this]() { Mark = Now(); })
			.Until(Label, [this, Seconds]() { Sample(); return Now() >= Mark + Seconds; });
	}

	void RunScenario(bool bPush)
	{
		using namespace StealthAssassinationPie;
		using namespace RivalCountOneUpdatePieData;
		bPushModel = bPush;
		Network
			.Until(TEXT("Active, one remote client per team, both views in place"), [this]()
			{
				if (!RoundActive())
				{
					return false;
				}
				for (AStealthPlayerController* Player : ServerPlayers())
				{
					const uint8 Team = TeamOf(Player);
					if (!Player->IsLocalController() && Team < 2)
					{
						ServerSide[Team] = Player;
						Machine[Team] = OwnerMachineController(Player);
					}
				}
				return ClientView(0) != nullptr && ClientView(1) != nullptr && ClientView(0)->GetView().StageIndex == EObjectiveTeamStage::Stage1
					&& ClientView(1)->GetView().StageIndex == EObjectiveTeamStage::Stage1;
			})
			.Then(TEXT("assert the carrier; team A: 0 -> 1 -> 2 in one server frame"), [this]()
			{
				IConsoleVariable* Push = IConsoleManager::Get().FindConsoleVariable(TEXT("Net.IsPushModelEnabled"));
				ASSERT_THAT(IsTrue(Push != nullptr && Push->GetBool() == bPushModel, TEXT("Net.IsPushModelEnabled is in the requested state")));
				for (int32 Team = 0; Team < 2; ++Team)
				{
					const AStealthPlayerController* Controller = ServerSide[Team].Get();
					ASSERT_THAT(IsTrue(Controller->IsUsingRegisteredSubObjectList(), TEXT("the controller uses the registered sub-object list")));
					const UObjectiveTeamViewComponent* View = Controller->GetObjectiveTeamView();
					ASSERT_THAT(IsTrue(View != nullptr && View->IsDefaultSubobject() && View->GetIsReplicated(), TEXT("the view component is a replicated default subobject")));
					ASSERT_THAT(IsTrue(View->GetView().RivalPrelimCount == 0, TEXT("the rival count starts at 0")));
				}
				Sample();
				ChangesAtMark[0] = ClientView(0)->GetClientChangeCount();
				UObjectiveTeamViewComponent* View = ServerView(0);
				ASSERT_THAT(IsTrue(View->ApplyView(WithRival(0, 1)) && View->ApplyView(WithRival(0, 2)), TEXT("both writes changed the value")));
			})
			.Until(TEXT("team A's client sees 2"), [this]() { Sample(); return ClientView(0)->GetView().RivalPrelimCount == 2; });
		Wait(TEXT("settle after the one-frame pair"), SettleSec);
		Network
			.Then(TEXT("assert one change; team B: 0 -> 1 (frame 1)"), [this]()
			{
				const int32 Changes = ClientView(0)->GetClientChangeCount() - ChangesAtMark[0];
				TestRunner->AddInfo(FString::Printf(TEXT("OC011 OBJ-V2 push=%d one-frame pair: %d change(s), final %d"), bPushModel ? 1 : 0, Changes, ClientView(0)->GetView().RivalPrelimCount));
				ASSERT_THAT(IsTrue(Changes == 1, TEXT("two writes in one server frame arrive as exactly one change")));
				ASSERT_THAT(IsTrue(!Seen[0].Contains(1), TEXT("the client never saw the intermediate value 1")));
				ChangesAtMark[1] = ClientView(1)->GetClientChangeCount();
				ASSERT_THAT(IsTrue(ServerView(1)->ApplyView(WithRival(1, 1)), TEXT("team B's first write")));
			})
			.Until(TEXT("team B's client sees 1"), [this]() { Sample(); return ClientView(1)->GetView().RivalPrelimCount == 1; })
			.Then(TEXT("team B: 1 -> 2 (frame 2)"), [this]() { ASSERT_THAT(IsTrue(ServerView(1)->ApplyView(WithRival(1, 2)), TEXT("team B's second write"))); })
			.Until(TEXT("team B's client sees 2"), [this]() { Sample(); return ClientView(1)->GetView().RivalPrelimCount == 2; })
			.Then(TEXT("assert two changes; never decreasing; the unmarked write"), [this]()
			{
				const int32 Changes = ClientView(1)->GetClientChangeCount() - ChangesAtMark[1];
				TestRunner->AddInfo(FString::Printf(TEXT("OC011 OBJ-V2 push=%d two frames: %d change(s)"), bPushModel ? 1 : 0, Changes));
				ASSERT_THAT(IsTrue(Changes == 2, TEXT("writes in two frames arrive as two changes")));
				for (int32 Team = 0; Team < 2; ++Team)
				{
					for (int32 Index = 1; Index < Seen[Team].Num(); ++Index)
					{
						ASSERT_THAT(IsTrue(Seen[Team][Index] >= Seen[Team][Index - 1], TEXT("the observed count never decreases")));
					}
				}
				// The dirty-mark control on team A: a changed value written without the mark.
				FObjectiveTeamView Unmarked = ServerView(0)->GetView();
				Unmarked.CompletedDefMask = 0x4000; // a value no real round uses (row 14), so the arrival is unambiguous
				ServerView(0)->ApplyViewWithoutDirtyMarkForTest(Unmarked);
				Mark = Now();
			})
			.Until(TEXT("the unmarked-write window"), [this]()
			{
				const bool bArrived = ClientView(0)->GetView().CompletedDefMask == 0x4000;
				return bArrived || Now() >= Mark + RivalCountOneUpdatePieData::UnmarkedWaitSec;
			})
			.Then(TEXT("assert the unmarked write's fate; then mark it"), [this]()
			{
				const bool bArrived = ClientView(0)->GetView().CompletedDefMask == 0x4000;
				TestRunner->AddInfo(FString::Printf(TEXT("OC011 push=%d unmarked write arrived=%d"), bPushModel ? 1 : 0, bArrived ? 1 : 0));
				// Observation only: with push model on the engine still compares a NetSerialize property after a garbage
				// collection (RepLayout IsPropertyDirty: bRecentlyCollectedGarbage && IsNetSerialize) and on a forced compare, so
				// an unmarked write MAY arrive; whether one is sent is not a contract a test can pin. The "every write marks" rule
				// is pinned by the source check of StealthGame.Objectives.ObjectivesAllowlistRow (with its seeded control).
				if (!bPushModel)
				{
					ASSERT_THAT(IsTrue(bArrived, TEXT("push model off: the compare path sends the write")));
				}
				ServerView(0)->MarkViewDirtyForTest();
			})
			.Until(TEXT("the marked write arrives"), [this]() { return ClientView(0)->GetView().CompletedDefMask == 0x4000; });
	}

	TEST_METHOD(OneChangePerFrame_PushModelOn)
	{
		// Before the PIE session's objects exist (the BEFORE_EACH start is latent): push handles are assigned at creation.
		IConsoleManager::Get().FindConsoleVariable(TEXT("Net.IsPushModelEnabled"))->Set(TEXT("1"), ECVF_SetByCode);
		RunScenario(true);
	}

	TEST_METHOD(OneChangePerFrame_PushModelOff)
	{
		IConsoleManager::Get().FindConsoleVariable(TEXT("Net.IsPushModelEnabled"))->Set(TEXT("0"), ECVF_SetByCode);
		RunScenario(false);
	}
};

#endif
