// Story iif-095 (production/epics/input-interaction-framework/story-095-command-test-seams.md): the harness self-test
// of the command-interface test seams.
//   GIVEN the StealthTestHarness module with a scripted command sequence, WHEN it runs through the controller transport,
//   THEN the server command log records each command's type, payload, ClientCommandSeq and arrival tick in order; the
//   outgoing-RPC hook records each client-to-server RPC sent and reports zero for a sequence that sends none; and the
//   named stub doubles each record the calls made on them.
// Notes pinned here: "a sequence that sends none" is a run of client-local camera toggles; the CMC keeps sending its
// ServerMove family meanwhile (an idle autonomous proxy still sends moves), so the zero is counted over every RPC that is
// NOT the engine background (the ServerMove family and ServerUpdateCamera, see FOutgoingRpcSpy::IsEngineBackground). The human/AI observation point: one PrepareBegin from the client and one from an NPC pawn's
// entry point are both recorded at the queue submit (identity of the two is story iif-026's assertion, not made here).
// Two parts: StealthGame.Input.CommandTestSeams (stub doubles, no world) and the PIE run
// StealthGame.Input.PIE.CommandTestSeams.ScriptedSequenceIsLoggedAndSpied (listen server + 1 client, fixture map).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "EngineUtils.h"
#include "Input/CommandLog.h"
#include "Input/RpcSpy.h"
#include "Input/StubDoubles.h"
#include "Server/Core/StealthPawnCommandComponent.h"
#include "Server/StealthClock.h"
#include "StealthPlayerController.h"
#include "UObject/StrongObjectPtr.h"
#include "View/Input/PawnCommandClient.h"
#include "View/Core/StealthPawn.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputCommandTestSeamsTest, "StealthGame.Input.CommandTestSeams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInputCommandTestSeamsTest::RunTest(const FString& Parameters)
{
	// Each named stub double records the calls made on it.
	FStubResolutionQueue Queue;
	FCommandPayload Payload;
	Payload.Arg = 7;
	Queue.Submit(FMailboxId::ServerLocal(), FQueueItem::Make(EQueueItemType::PawnCommand, FEpochToken(), FMailboxId::ServerLocal(), Payload));
	FHoldTimerPayload Timer;
	Queue.ScheduleAt(2.5, FQueueItem::Make(EQueueItemType::HoldTimer, FEpochToken(), FMailboxId::ServerLocal(), Timer));
	TestEqual(TEXT("FStubResolutionQueue recorded the submit"), Queue.Submitted.Num(), 1);
	TestTrue(TEXT("FStubResolutionQueue recorded the due time"), Queue.Scheduled.Num() == 1 && Queue.Scheduled[0].DueServerTime == 2.5);
	int32 Slots = 0;
	bool bInSlotDuring = false;
	Queue.Drain([&](const FQueueItem&, FQueueKey Key) { ++Slots; bInSlotDuring = Queue.bInSlot && Key.IsSet(); });
	TestTrue(TEXT("FStubResolutionQueue drains in a slot with a stamped key"), Slots == 1 && bInSlotDuring && !Queue.bInSlot && Queue.Drained.Num() == 1);

	const APawn& Pawn = *GetDefault<AStealthPawn>();
	FStubRosterLegality Legality;
	Legality.Class = ECommandLegalityClass::Informant;
	TestTrue(TEXT("FStubRosterLegality answers its class and records the call"), Legality.GetLegalityClass(Pawn) == ECommandLegalityClass::Informant && Legality.Calls == 1);

	FStubSharedPawnStateView State;
	State.bRunning = true;
	State.DisplacementCm = 30.f;
	TestTrue(TEXT("FStubSharedPawnStateView answers and records"), State.IsRunning() && !State.IsInTraversal() && State.CommandedDisplacementCm(0.0) == 30.f && State.Calls == 3);

	FStubCamera Camera;
	Camera.Origin = FVector(1.0, 2.0, 3.0);
	TestTrue(TEXT("FStubCamera answers its ray and records"), Camera.GetAimRay().Origin.Equals(Camera.Origin) && Camera.Calls == 1);

	FStubInteractTargetOwner Owner;
	TestTrue(TEXT("FStubInteractTargetOwner answers the hold time"), Owner.GetInteractHoldSec() == 3.f && Owner.HoldSecCalls == 1);
	Owner.OnInteractExecuted(&Pawn, 4, FQueueKey(1, 1));
	Owner.OnInteractEnded(&Pawn, 4, true, FQueueKey(1, 2));
	TestTrue(TEXT("FStubInteractTargetOwner records executed and ended"), Owner.Executed.Num() == 1 && Owner.Ended.Num() == 1);

	TStrongObjectPtr<UStubCommandPredicate> Predicate(NewObject<UStubCommandPredicate>());
	Predicate->Verdict = FPredicateResult::Reject(ECommandRejectCode::Cooldown);
	TestTrue(TEXT("UStubCommandPredicate answers its verdict and records"), Predicate->CanBegin(*GetDefault<AStealthPawn>(), FCommandPayload()) == FPredicateResult::Reject(ECommandRejectCode::Cooldown) && Predicate->Calls == 1);

	// The command log observes the core's submit point without changing what is submitted.
	FStubResolutionQueue Observed;
	class FClock : public IStealthClock
	{
	public:
		virtual double GetServerTimeSeconds() const override { return 5.0; }
		virtual uint64 GetServerTickIndex() const override { return 42; }
	} Clock;
	UPawnCommandComponent* Component = NewObject<UPawnCommandComponent>();
	FPawnCommandCoreDeps Deps;
	Deps.Clock = &Clock;
	Deps.Queue = &Observed;
	Component->InitializeCore(Deps);
	{
		FStealthCommandLog Log;
		Component->InteractBegin(FCourtFixtureId(3));
		Component->InteractEnd();
		const TArray<FStealthCommandLogEntry> Submits = Log.At(EStealthCommandLogPoint::QueueSubmit);
		TestTrue(TEXT("the log recorded both submits in order"), Submits.Num() == 2 && Submits[0].Kind == ECommandKind::InteractBegin && Submits[1].Kind == ECommandKind::InteractEnd);
		TestTrue(TEXT("the log's arrival tick is the injected clock's tick"), Submits.Num() == 2 && Submits[0].ArrivalTick == 42);
		TestTrue(TEXT("the log carries the payload"), Submits.Num() == 2 && Submits[0].Payload.Target.CourtSocketId == 3);
	}
	TestEqual(TEXT("observing changed nothing that was submitted"), Observed.Submitted.Num(), 2);
	return true;
}

#if defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "StealthGameMode.h"

namespace CommandTestSeamsPieData
{
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	const TCHAR* const NetworkTimeoutCeilingSeconds = TEXT("240");
	/** Frames the reliable RPCs get to arrive and be ingested. */
	constexpr uint64 SettleFrames = 30;

	/** The scripted human sequence (kinds whose prototype adapters are harmless in the fixture map). */
	const ECommandKind Script[] = { ECommandKind::InteractBegin, ECommandKind::InteractEnd, ECommandKind::PrepareBegin, ECommandKind::PrepareEnd, ECommandKind::HideEnter };
}

NETWORK_TEST_CLASS(CommandTestSeams, "StealthGame.Input.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;

	TUniquePtr<FStealthCommandLog> Log;
	TUniquePtr<FOutgoingRpcSpy> Spy;
	uint64 MarkFrame = 0;
	uint16 Epoch = 0;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(CommandTestSeamsPieData::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
		}
	}

	AFTER_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			if (!PreviousNetworkTimeout.IsEmpty())
			{
				Timeout->Set(*PreviousNetworkTimeout, ECVF_SetByCode);
			}
		}
	}

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FBasePIENetworkComponentState>()
			.AsListenServer()
			.WithClients(1)
			.WithGameMode(AStealthGameMode::StaticClass())
			.Build(Network);
		Network.StartOnFixtureMap(CommandTestSeamsPieData::FixtureMap);
	}

	AFTER_EACH()
	{
		Spy.Reset();
		Log.Reset();
	}

	TEST_METHOD(ScriptedSequenceIsLoggedAndSpied)
	{
		using namespace CommandTestSeamsPieData;
		Network
			.UntilClients(TEXT("the client possesses its pawn"), [](FBasePIENetworkComponentState& Client)
			{
				const AStealthPlayerController* PC = Client.World ? Cast<AStealthPlayerController>(Client.World->GetFirstPlayerController()) : nullptr;
				return PC && Cast<AStealthPawn>(PC->GetPawn()) != nullptr && PC->GetOwnerPossessionEpoch() >= 1;
			})
			.ThenServer(TEXT("install the server command log"), [this](FBasePIENetworkComponentState&) { Log = MakeUnique<FStealthCommandLog>(); })
			.ThenClients(TEXT("a sequence that sends none: client-local camera toggles"), [this](FBasePIENetworkComponentState& Client)
			{
				Spy = MakeUnique<FOutgoingRpcSpy>(Client.World);
				ASSERT_THAT(IsTrue(Spy->IsAttached(), TEXT("the RPC spy is attached to the client NetDriver")));
				AStealthPlayerController* PC = Cast<AStealthPlayerController>(Client.World->GetFirstPlayerController());
				PC->GetCommandClient()->OnCameraToggleAccepted.Broadcast();
				MarkFrame = GFrameCounter;
			})
			.UntilClients(TEXT("settle"), [this](FBasePIENetworkComponentState&) { return GFrameCounter >= MarkFrame + SettleFrames; })
			.ThenClients(TEXT("zero non-movement RPCs"), [this](FBasePIENetworkComponentState&)
			{
				const TArray<FOutgoingRpcSpy::FRecord> Other = Spy->Excluding([](FName F) { return FOutgoingRpcSpy::IsEngineBackground(F); });
				TestRunner->AddInfo(FString::Printf(TEXT("RPCs during the camera-toggle sequence: %d (%s)"), Spy->Records.Num(), *FOutgoingRpcSpy::Describe(Spy->Records)));
				ASSERT_THAT(AreEqual(0, Other.Num(), *FString::Printf(TEXT("the camera-toggle sequence sent no RPC beyond the engine's move/camera background (%s)"), *FOutgoingRpcSpy::Describe(Other))));
				Spy->Records.Reset();
			})
			.ThenClients(TEXT("the scripted sequence through the controller transport"), [this](FBasePIENetworkComponentState& Client)
			{
				AStealthPlayerController* PC = Cast<AStealthPlayerController>(Client.World->GetFirstPlayerController());
				Epoch = PC->GetOwnerPossessionEpoch();
				uint32 Seq = 1000;
				for (const ECommandKind Kind : Script)
				{
					FNetPawnCommand Cmd;
					Cmd.Kind = Kind;
					Cmd.Seq.PossessionEpoch = Epoch;
					Cmd.Seq.Seq = ++Seq;
					PC->SendPawnCommand(Cmd, nullptr);
				}
				MarkFrame = GFrameCounter;
			})
			.UntilServer(TEXT("ingested"), [this](FBasePIENetworkComponentState&)
			{
				return Log && Log->At(EStealthCommandLogPoint::IngressOutput).Num() >= UE_ARRAY_COUNT(Script);
			})
			.ThenClients(TEXT("the spy recorded each ServerPawnCommand"), [this](FBasePIENetworkComponentState&)
			{
				const TArray<FOutgoingRpcSpy::FRecord> Commands = Spy->Excluding([](FName F) { return F != TEXT("ServerPawnCommand"); });
				ASSERT_THAT(AreEqual(static_cast<int32>(UE_ARRAY_COUNT(Script)), Commands.Num(), TEXT("one ServerPawnCommand per scripted command")));
			})
			.ThenServer(TEXT("the log recorded type, seq and arrival tick in order"), [this](FBasePIENetworkComponentState& Server)
			{
				const TArray<FStealthCommandLogEntry> Ingress = Log->At(EStealthCommandLogPoint::IngressOutput);
				const TArray<FStealthCommandLogEntry> Submits = Log->At(EStealthCommandLogPoint::QueueSubmit);
				ASSERT_THAT(AreEqual(static_cast<int32>(UE_ARRAY_COUNT(Script)), Ingress.Num(), TEXT("ingress entries")));
				ASSERT_THAT(AreEqual(static_cast<int32>(UE_ARRAY_COUNT(Script)), Submits.Num(), TEXT("queue submit entries")));
				for (int32 Index = 0; Index < Ingress.Num(); ++Index)
				{
					ASSERT_THAT(IsTrue(Ingress[Index].Kind == Script[Index], *FString::Printf(TEXT("ingress %d kind in order"), Index)));
					ASSERT_THAT(AreEqual(1001u + static_cast<uint32>(Index), Ingress[Index].Seq.Seq, *FString::Printf(TEXT("ingress %d seq"), Index)));
					ASSERT_THAT(AreEqual(Epoch, Ingress[Index].Seq.PossessionEpoch, *FString::Printf(TEXT("ingress %d possession epoch"), Index)));
					ASSERT_THAT(IsTrue(Index == 0 || Ingress[Index].ArrivalTick >= Ingress[Index - 1].ArrivalTick, TEXT("arrival ticks are in order")));
					ASSERT_THAT(IsTrue(Ingress[Index].Sink.Index != 0, TEXT("a human command carries its connection's reply sink")));
					ASSERT_THAT(IsTrue(!Ingress[Index].Mailbox.IsServerLocal(), TEXT("a human command uses its participant mailbox")));
					ASSERT_THAT(IsTrue(Submits[Index].Kind == Script[Index], *FString::Printf(TEXT("submit %d kind in order"), Index)));
					ASSERT_THAT(AreEqual(Ingress[Index].Seq.Seq, Submits[Index].Seq.Seq, TEXT("the submit carries the ingress seq")));
				}

			})
			.UntilServer(TEXT("the crowd has spawned (after the nav rebuild)"), [](FBasePIENetworkComponentState& Server)
			{
				for (TActorIterator<AStealthPawn> It(Server.World); It; ++It)
				{
					if (It->GetController() && !It->GetController()->IsA<APlayerController>())
					{
						return true;
					}
				}
				return false;
			})
			.ThenServer(TEXT("the AI observation point"), [this](FBasePIENetworkComponentState& Server)
			{
				// The AI observation point: an NPC pawn's entry point, same content, server-local mailbox and null sink.
				Log->Reset();
				AStealthPawn* Npc = nullptr;
				for (TActorIterator<AStealthPawn> It(Server.World); It && !Npc; ++It)
				{
					Npc = It->GetController() && !It->GetController()->IsA<APlayerController>() ? *It : nullptr;
				}
				ASSERT_THAT(IsNotNull(Npc, TEXT("an AI-driven pawn exists")));
				Npc->GetPawnCommand()->PrepareBegin();
				Npc->GetPawnCommand()->PrepareEnd();
				const TArray<FStealthCommandLogEntry> Ai = Log->At(EStealthCommandLogPoint::QueueSubmit);
				ASSERT_THAT(AreEqual(2, Ai.Num(), TEXT("the AI commands were recorded at the submit point")));
				ASSERT_THAT(IsTrue(Ai[0].Kind == ECommandKind::PrepareBegin && Ai[0].Mailbox.IsServerLocal() && Ai[0].Sink.Index == 0,
					TEXT("AI PrepareBegin: server-local mailbox, null sink")));
				ASSERT_THAT(IsTrue(Log->At(EStealthCommandLogPoint::IngressOutput).IsEmpty(), TEXT("AI commands skip stage-1 ingress")));
			});
	}
};

#endif // ENABLE_PIE_NETWORK_TEST

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS
