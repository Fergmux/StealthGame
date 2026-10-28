// Objectives Chain story oc-019 AC2 (AC-OC-26 [PIE-2]; IIF R5, IVP R7.4a): StealthGame.Objectives.PIE.IngressAcceptsAnyCourt.
// Listen server + 2 clients on L_Prototype (NpcCount 0). Team 1's remote client is the tender; team 1 first banks Stage1
// (DebugCompleteStage), so its courts are: own live P2[1], completed P1[1], locked Final, and the opponent's live P1[0]. The
// tender's own client sends a human Interact(Begin) naming each court over its RPC (the court is public geometry: ingress
// never reads its role) and holds to D:
//   - every court command reaches the queue (the PawnCommandCore QueueSubmit observation; never dropped at ingress), and the
//     owner's replies are byte-identical across the four courts (FHoldStatus Active then Completed, serialised with
//     NetSerialize; the command sequence and the server-time stamps normalised out), Begin-to-Complete takes D within one server
//     frame at every court, and the outcome class is Completed at every court;
//   - the three non-live courts change no state (both teams' server-built views unchanged); the own live court counts;
//   - an id that is not a court fixture (999, and the malformed 0xFFFF) is dropped at ingress as an unknown id: nothing reaches
//     the queue and no reply arrives.
// (The AC-IVP-41 fixture of the IVP epic does not exist yet; this test builds its own and the IVP epic can reuse the shape.)

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "Serialization/BitWriter.h"
#include "Server/Testing/StealthCommandSeams.h"
#include "StealthGameMode.h"
#include "Tests/PIE/Objectives/ObjectivesPieFixture.h"

namespace IngressAcceptsAnyCourtPieData
{
	/** The owner's replies to one Begin, normalised: sequence and server-time stamps zeroed, then NetSerialized. */
	TArray<uint8> NormalisedBytes(TArray<FCommandReply> Replies)
	{
		FBitWriter Writer(0, true);
		for (FCommandReply& Reply : Replies)
		{
			Reply.Seq = FClientCommandSeq();
			Reply.Hold.Seq = FClientCommandSeq();
			Reply.Hold.ServerStartTime = 0.0;
			Reply.Hold.ServerEndTime = 0.0;
			bool bOk = true;
			Reply.NetSerialize(Writer, nullptr, bOk);
		}
		return TArray<uint8>(Writer.GetData(), Writer.GetNumBytes());
	}

	struct FCase
	{
		const TCHAR* Name = TEXT("");
		FCourtFixtureId Court;
		uint32 Seq = 0;
		int32 QueueSubmits = 0;
		TArray<FCommandReply> Replies;
		double Duration = -1.0;
		bool bChangedState = false;
	};
}

NETWORK_TEST_CLASS(IngressAcceptsAnyCourt, "StealthGame.Objectives.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static StealthObjectivesPie::FScopedPieSettings Settings;

	TWeakObjectPtr<AStealthPlayerController> Tender;
	TWeakObjectPtr<AStealthPlayerController> TenderMachine;
	StealthObjectivesPie::FCourts Courts;
	TArray<IngressAcceptsAnyCourtPieData::FCase> Cases;
	int32 Current = 0;
	uint32 Seq = 0x00100000u;
	double Mark = 0.0;
	FObjectiveTeamView ViewsBefore[2];
	TMap<uint32, int32> SubmitsBySeq;

	BEFORE_ALL() { Settings.Apply(); }
	AFTER_ALL() { Settings.Restore(); }

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FBasePIENetworkComponentState>().AsListenServer().WithClients(2).WithGameMode(AStealthGameMode::StaticClass()).Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
		StealthCommandSeams::SetObserver([this](const FStealthCommandLogEntry& Entry)
		{
			if (Entry.Point == EStealthCommandLogPoint::QueueSubmit && static_cast<ECommandKind>(Entry.Payload.Kind) == ECommandKind::InteractBegin)
			{
				++SubmitsBySeq.FindOrAdd(Entry.Payload.ClientSeq.Seq);
			}
		});
	}

	AFTER_EACH()
	{
		StealthCommandSeams::SetObserver(StealthCommandSeams::FObserver());
	}

	TArray<FCommandReply> RepliesFor(uint32 InSeq) const
	{
		TArray<FCommandReply> Out;
		if (const AStealthPlayerController* Local = TenderMachine.Get())
		{
			for (const FCommandReply& Reply : Local->GetReceivedReplies())
			{
				if (Reply.Seq.Seq == InSeq)
				{
					Out.Add(Reply);
				}
			}
		}
		return Out;
	}

	void SendCurrent()
	{
		using namespace StealthObjectivesPie;
		IngressAcceptsAnyCourtPieData::FCase& Case = Cases[Current];
		ViewsBefore[0] = Service()->BuildTeamView(0);
		ViewsBefore[1] = Service()->BuildTeamView(1);
		BeginTendFromClient(TenderMachine.Get(), Case.Court, Seq);
		Case.Seq = Seq;
		Mark = ServerNow(ServerWorld());
	}

	/** True when the current case is finished (Completed received, or 1.5 s without anything for an unknown id). */
	bool PollCurrent()
	{
		using namespace StealthObjectivesPie;
		IngressAcceptsAnyCourtPieData::FCase& Case = Cases[Current];
		const TArray<FCommandReply> Replies = RepliesFor(Case.Seq);
		const bool bDone = Replies.ContainsByPredicate([](const FCommandReply& R) { return R.Kind == ECommandReplyKind::HoldStatus && R.Hold.Outcome != EHoldOutcome::Active; });
		const bool bUnknown = !Case.Court.IsSet() || AStealthCourtSocket::FindCourt(ServerWorld(), Case.Court) == nullptr;
		if (bDone || (bUnknown && ServerNow(ServerWorld()) >= Mark + 1.5) || ServerNow(ServerWorld()) >= Mark + 8.0)
		{
			Case.Replies = Replies;
			const int32* Submits = SubmitsBySeq.Find(Case.Seq);
			Case.QueueSubmits = Submits != nullptr ? *Submits : 0;
			const FCommandReply* End = Replies.FindByPredicate([](const FCommandReply& R) { return R.Kind == ECommandReplyKind::HoldStatus && R.Hold.Outcome != EHoldOutcome::Active; });
			Case.Duration = End != nullptr ? End->Hold.ServerEndTime - End->Hold.ServerStartTime : -1.0;
			Case.bChangedState = Service()->BuildTeamView(0) != ViewsBefore[0] || Service()->BuildTeamView(1) != ViewsBefore[1];
			return true;
		}
		return false;
	}

	TEST_METHOD(EveryCourtReachesTheQueueAndResolvesIdenticallyUnknownIdsDrop)
	{
		using namespace StealthObjectivesPie;
		Network
			.Until(TEXT("Active, team 1's remote client found"), [this]()
			{
				if (!RoundActive() || Service() == nullptr)
				{
					return false;
				}
				for (AStealthPlayerController* Player : ServerPlayers())
				{
					if (!Player->IsLocalController() && TeamOf(Player) == 1)
					{
						Tender = Player;
						TenderMachine = OwnerMachineController(Player);
					}
				}
				Courts = BoundCourts();
				return Courts.IsSet() && TenderMachine.IsValid() && ViewOf(TenderMachine.Get()).StageIndex == EObjectiveTeamStage::Stage1;
			})
			.Then(TEXT("team 1 banks Stage1"), [this]() { ASSERT_THAT(IsTrue(Service()->DebugCompleteStage(1), TEXT("team 1's DebugCompleteStage accepted"))); })
			.Until(TEXT("team 1's client at Stage2"), [this]() { return ViewOf(TenderMachine.Get()).StageIndex == EObjectiveTeamStage::Stage2; })
			.Then(TEXT("the cases"), [this]()
			{
				using namespace IngressAcceptsAnyCourtPieData;
				const TPair<const TCHAR*, FCourtFixtureId> Order[] = {
					{ TEXT("opponent live P1[0]"), Courts.P1[0] }, { TEXT("locked final"), Courts.Final }, { TEXT("completed P1[1]"), Courts.P1[1] },
					{ TEXT("non-fixture 999"), FCourtFixtureId(999) }, { TEXT("malformed 0xFFFF"), FCourtFixtureId(0xFFFF) }, { TEXT("own live P2[1]"), Courts.P2[1] } };
				for (const TPair<const TCHAR*, FCourtFixtureId>& Entry : Order)
				{
					FCase& Case = Cases.AddDefaulted_GetRef();
					Case.Name = Entry.Key;
					Case.Court = Entry.Value;
				}
				Current = 0;
				SendCurrent();
			});
		for (int32 Index = 0; Index < 6; ++Index)
		{
			Network
				.Until(*FString::Printf(TEXT("case %d resolves"), Index), [this]() { return PollCurrent(); })
				.Then(*FString::Printf(TEXT("case %d done: next"), Index), [this]()
				{
					if (++Current < Cases.Num())
					{
						SendCurrent();
					}
				});
		}
		Network.Then(TEXT("assert"), [this]()
		{
			using namespace IngressAcceptsAnyCourtPieData;
			const double D = Service()->GetInteractHoldSec();
			const TArray<uint8> Reference = NormalisedBytes(Cases.Last().Replies); // the own live court
			for (const FCase& Case : Cases)
			{
				TestRunner->AddInfo(FString::Printf(TEXT("OC19 ingress %s (court %u): queue submits %d, replies %d, bytes %d, duration %.4f s, state changed %d"), Case.Name,
					Case.Court.Value, Case.QueueSubmits, Case.Replies.Num(), NormalisedBytes(Case.Replies).Num(), Case.Duration, Case.bChangedState ? 1 : 0));
			}
			for (int32 Index = 0; Index < Cases.Num(); ++Index)
			{
				const FCase& Case = Cases[Index];
				const bool bFixture = Index != 3 && Index != 4;
				if (bFixture)
				{
					ASSERT_THAT(IsTrue(Case.QueueSubmits == 1, *FString::Printf(TEXT("%s: the command reached the queue (not dropped at ingress)"), Case.Name)));
					ASSERT_THAT(IsTrue(NormalisedBytes(Case.Replies) == Reference && Case.Replies.Num() == 2, *FString::Printf(TEXT("%s: the owner's replies are byte-identical to the own live court's"), Case.Name)));
					ASSERT_THAT(IsTrue(FMath::Abs(Case.Duration - D) <= 0.034, *FString::Printf(TEXT("%s: Begin-to-Complete is D within one server frame"), Case.Name)));
					ASSERT_THAT(IsTrue(Case.Replies.Last().Hold.Outcome == EHoldOutcome::Completed, *FString::Printf(TEXT("%s: the timing class is Completed"), Case.Name)));
					ASSERT_THAT(IsTrue(Case.bChangedState == (Index == Cases.Num() - 1), *FString::Printf(TEXT("%s: only the own live court changes state"), Case.Name)));
				}
				else
				{
					ASSERT_THAT(IsTrue(Case.QueueSubmits == 0, *FString::Printf(TEXT("%s: dropped at ingress (never queued)"), Case.Name)));
					ASSERT_THAT(IsTrue(Case.Replies.Num() == 0, *FString::Printf(TEXT("%s: no reply"), Case.Name)));
					ASSERT_THAT(IsTrue(!Case.bChangedState, *FString::Printf(TEXT("%s: no state change"), Case.Name)));
				}
			}
		});
	}
};

#endif
