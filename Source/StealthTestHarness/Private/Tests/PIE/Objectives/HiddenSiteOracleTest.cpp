// Objectives Chain story oc-020 AC1 (AC-OC-08 [PIE-2]; IVP R7.4a): StealthGame.Objectives.PIE.HiddenSiteOracle.
// Listen server + 2 clients on L_Prototype (NpcCount 0). Team 0 first banks Stage1 (DebugCompleteStage). Team 1's remote client
// is the tender: its pawn is placed at each court in turn (inside ObjectiveInteractRange, so the client's ring targets that
// court) and its own client sends a human Interact(Begin) naming it and holds to D, at five courts in a fixed shuffled order:
// the opponent's completed P1[0], the hidden final, the opponent's live P2[0], its own locked P2[1], then its own live P1[1].
//   - the owner's replies (FHoldStatus Active then Completed, NetSerialized with the sequence and server-time stamps normalised
//     out) are byte-exact across all five; Begin-to-Complete agrees within one server frame;
//   - the four non-live tends change no state (both teams' server-built views unchanged); the own live one counts;
//   - the client's ring (AStealthPlayerController::GetOwnTendRing, ObjectiveViewHelpers::RingIsMarigold) is Bone at 0.3 s after
//     Begin for all five and Marigold at 1.5 s only at the own live site (the screenshots are the story's run-and-observe:
//     production/qa/evidence/oc-tend-*.png).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "Serialization/BitWriter.h"
#include "StealthGameMode.h"
#include "Tests/PIE/Objectives/ObjectivesPieFixture.h"
#include "View/EncounterPresentation.h"

namespace HiddenSiteOraclePieData
{
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

	struct FTend
	{
		const TCHAR* Name = TEXT("");
		FCourtFixtureId Court;
		uint32 Seq = 0;
		TArray<FCommandReply> Replies;
		double Duration = -1.0;
		EStealthTendRing Early = EStealthTendRing::None;
		EStealthTendRing Late = EStealthTendRing::None;
		bool bChangedState = false;
	};

	const TCHAR* RingName(EStealthTendRing Ring)
	{
		return Ring == EStealthTendRing::Marigold ? TEXT("Marigold") : (Ring == EStealthTendRing::Bone ? TEXT("Bone") : TEXT("None"));
	}
}

NETWORK_TEST_CLASS(HiddenSiteOracle, "StealthGame.Objectives.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static StealthObjectivesPie::FScopedPieSettings Settings;

	TWeakObjectPtr<AStealthPlayerController> Tender;
	TWeakObjectPtr<AStealthPlayerController> TenderMachine;
	StealthObjectivesPie::FCourts Courts;
	TArray<HiddenSiteOraclePieData::FTend> Tends;
	int32 Current = 0;
	uint32 Seq = 0x00300000u;
	double Mark = 0.0;
	double Start = -1.0;
	FObjectiveTeamView ViewsBefore[2];

	BEFORE_ALL() { Settings.Apply(); }
	AFTER_ALL() { Settings.Restore(); }

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FBasePIENetworkComponentState>().AsListenServer().WithClients(2).WithGameMode(AStealthGameMode::StaticClass()).Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
	}

	TArray<FCommandReply> RepliesFor(uint32 InSeq) const
	{
		TArray<FCommandReply> Out;
		for (const FCommandReply& Reply : TenderMachine->GetReceivedReplies())
		{
			if (Reply.Seq.Seq == InSeq)
			{
				Out.Add(Reply);
			}
		}
		return Out;
	}

	/** Polls the current tend on the tender's client: the ring at 0.3 s and 1.5 s after the server Begin, then the end. */
	bool Poll()
	{
		using namespace HiddenSiteOraclePieData;
		using namespace StealthObjectivesPie;
		FTend& Tend = Tends[Current];
		const TArray<FCommandReply> Replies = RepliesFor(Tend.Seq);
		const FCommandReply* Active = Replies.FindByPredicate([](const FCommandReply& R) { return R.Kind == ECommandReplyKind::HoldStatus && R.Hold.Outcome == EHoldOutcome::Active; });
		const double ClientNow = UEncounterPresentationSubsystem::EstimatedServerNow(TenderMachine->GetWorld());
		if (Active != nullptr)
		{
			Start = Active->Hold.ServerStartTime;
			if (Tend.Early == EStealthTendRing::None && ClientNow >= Start + 0.3)
			{
				Tend.Early = TenderMachine->GetOwnTendRing(ClientNow);
			}
			if (Tend.Late == EStealthTendRing::None && ClientNow >= Start + 1.5)
			{
				Tend.Late = TenderMachine->GetOwnTendRing(ClientNow);
			}
		}
		const FCommandReply* End = Replies.FindByPredicate([](const FCommandReply& R) { return R.Kind == ECommandReplyKind::HoldStatus && R.Hold.Outcome != EHoldOutcome::Active; });
		if ((End != nullptr && Tend.Late != EStealthTendRing::None) || ServerNow(ServerWorld()) >= Mark + 10.0)
		{
			Tend.Replies = Replies;
			Tend.Duration = End != nullptr ? End->Hold.ServerEndTime - End->Hold.ServerStartTime : -1.0;
			Tend.bChangedState = Service()->BuildTeamView(0) != ViewsBefore[0] || Service()->BuildTeamView(1) != ViewsBefore[1];
			return true;
		}
		return false;
	}

	TEST_METHOD(FiveCourtsAnswerTheOwnerByteForByteAndOnlyTheLiveOneGoesMarigold)
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
			.Then(TEXT("team 0 banks Stage1 (its P1 becomes a completed court)"), [this]() { ASSERT_THAT(IsTrue(Service()->DebugCompleteStage(0), TEXT("accepted"))); })
			.Until(TEXT("team 0 at Stage2"), [this]() { return Service()->GetObjectiveTeamStage(0) == EObjectiveTeamStage::Stage2; })
			.Then(TEXT("the five tends"), [this]()
			{
				const TPair<const TCHAR*, FCourtFixtureId> Order[] = {
					{ TEXT("completed"), Courts.P1[0] }, { TEXT("hidden"), Courts.Final }, { TEXT("opponent"), Courts.P2[0] },
					{ TEXT("locked"), Courts.P2[1] }, { TEXT("live"), Courts.P1[1] } };
				for (const TPair<const TCHAR*, FCourtFixtureId>& Entry : Order)
				{
					HiddenSiteOraclePieData::FTend& Tend = Tends.AddDefaulted_GetRef();
					Tend.Name = Entry.Key;
					Tend.Court = Entry.Value;
				}
			});
		for (int32 Index = 0; Index < 5; ++Index)
		{
			Network
				.Then(*FString::Printf(TEXT("tend %d: place the tender at the court"), Index), [this, Index]()
				{
					Current = Index;
					PlaceAtCourt(Tender->GetPawn(), Tends[Index].Court);
					Mark = ServerNow(ServerWorld());
				})
				.Until(*FString::Printf(TEXT("tend %d: settle"), Index), [this]() { return ServerNow(ServerWorld()) >= Mark + 0.8; })
				.Then(*FString::Printf(TEXT("tend %d: Begin from the client"), Index), [this, Index]()
				{
					ViewsBefore[0] = Service()->BuildTeamView(0);
					ViewsBefore[1] = Service()->BuildTeamView(1);
					BeginTendFromClient(TenderMachine.Get(), Tends[Index].Court, Seq);
					Tends[Index].Seq = Seq;
					Mark = ServerNow(ServerWorld());
				})
				.Until(*FString::Printf(TEXT("tend %d: held to D"), Index), [this]() { return Poll(); });
		}
		Network.Then(TEXT("assert"), [this]()
		{
			using namespace HiddenSiteOraclePieData;
			const double D = Service()->GetInteractHoldSec();
			const TArray<uint8> Reference = NormalisedBytes(Tends.Last().Replies);
			for (const FTend& Tend : Tends)
			{
				TestRunner->AddInfo(FString::Printf(TEXT("OC20 oracle %s (court %u): replies %d, bytes %d, duration %.4f s, ring %s -> %s, state changed %d"), Tend.Name,
					Tend.Court.Value, Tend.Replies.Num(), NormalisedBytes(Tend.Replies).Num(), Tend.Duration, RingName(Tend.Early), RingName(Tend.Late), Tend.bChangedState ? 1 : 0));
			}
			for (int32 Index = 0; Index < Tends.Num(); ++Index)
			{
				const FTend& Tend = Tends[Index];
				const bool bLive = Index == Tends.Num() - 1;
				ASSERT_THAT(IsTrue(Tend.Replies.Num() == 2 && NormalisedBytes(Tend.Replies) == Reference, *FString::Printf(TEXT("%s: the owner's replies are byte-exact"), Tend.Name)));
				ASSERT_THAT(IsTrue(FMath::Abs(Tend.Duration - D) <= 0.034, *FString::Printf(TEXT("%s: Begin-to-Complete is D within one server frame"), Tend.Name)));
				ASSERT_THAT(IsTrue(Tend.bChangedState == bLive, *FString::Printf(TEXT("%s: state changes only at the own live site"), Tend.Name)));
				ASSERT_THAT(IsTrue(Tend.Early == EStealthTendRing::Bone, *FString::Printf(TEXT("%s: the ring is Bone before TendCueDelaySec"), Tend.Name)));
				ASSERT_THAT(IsTrue(Tend.Late == (bLive ? EStealthTendRing::Marigold : EStealthTendRing::Bone), *FString::Printf(TEXT("%s: Marigold after the cue only at the own live site"), Tend.Name)));
			}
		});
	}
};

#endif
