// Assassination story asn-024 AC2 / AC-ASN-49 (ASN-R23, UI-ASN-3/4/5; ADR-0008 T2; listen server + 2 clients on
// L_Prototype, NpcCount 0, PktLag 75 ms on every driver): StealthGame.Assassination.PIE.OutcomePresentedAtContact.<method>.
// The players split 2v1 (S alone, P1 and P2 the pair):
//   1 the defender: S braces, P2 strikes S (reversed). On S's machine the Counter write and the FDefenceResult both arrive
//     before the contact (the result 75 ms after the slot, the Counter with the staged death write); at each arrival the
//     brace square is still shown and no plate is up; the square is removed and "Strike reversed" enters on S's local
//     ContactServerTime frame (+- 1 frame);
//   2 the struck player: S kills P1. On P1's machine the removal card (the HUD stand-in, an OnRoleChangePresented
//     subscriber) and the role-change dispatch Camera's Dying subscribes to start on P1's own DeathServerTime frame, never
//     at the arrival of its Roster (informant) state.
// NEGATIVE CONTROL (the regression of the prototype's on-receipt "Reversal!" message): with the asn-024 test seam
// PresentResultsOnReceipt on, the same reversal puts S's plate up at the FDefenceResult's arrival, before contact, and
// the contact-frame check fails; the method asserts that it does.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "Engine/NetDriver.h"
#include "HAL/IConsoleManager.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/OwnPresentationPieSupport.h"

namespace OutcomePresentedAtContactPieData
{
	/** AC-ASN-49: 75 ms per direction. */
	constexpr int32 PktLagMs = 75;
	constexpr double SettleSec = 0.8;
	/** Past the contact, the death write and the lockout. */
	constexpr double StepSec = 2.4;
	/** The receipt must lead the contact by at least this for the case to say anything (the staged Counter leads by about 75 ms). */
	constexpr double MeaningfulLeadSec = 0.02;
}

NETWORK_TEST_CLASS(OutcomePresentedAtContact, "StealthGame.Assassination.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;
	FPacketSimulationSettings PacketSettings;

	TWeakObjectPtr<AStealthPlayerController> Solo;
	TWeakObjectPtr<AStealthPlayerController> Pair[2];
	TWeakObjectPtr<AStealthPawn> SoloPawn;
	int32 MachineS = INDEX_NONE;
	int32 MachineP1 = INDEX_NONE;
	int32 LogFromS = 0;
	int32 LogFromP1 = 0;
	StealthOwnPresentationPie::FFrameTracker Frames;

	/** The defender's case, sampled on S's machine every frame. */
	double CounterContact = -1.0;
	double CounterReceipt = -1.0;
	double ResultReceipt = -1.0;
	bool bSquareAtCounterReceipt = false;
	bool bPlateAtCounterReceipt = true;
	bool bSquareAtResultReceipt = false;
	bool bPlateAtResultReceipt = true;
	int32 RepliesBefore = 0;

	/** The victim's case, sampled on P1's machine every frame. */
	double DeathServerTime = -1.0;
	double InformantArrival = -1.0;
	bool bCardAtInformantArrival = true;

	uint32 Seq = 0x24200000u;
	double Mark = 0.0;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(StealthAssassinationPie::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
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
		if (PreviousNpcCount >= 0)
		{
			GetMutableDefault<UStealthSettings>()->NpcCount = PreviousNpcCount;
		}
		StealthOwnerPresentationTestSeams::SetPresentResultsOnReceipt(false);
	}

	BEFORE_EACH()
	{
		PacketSettings = FPacketSimulationSettings();
		PacketSettings.PktLag = OutcomePresentedAtContactPieData::PktLagMs;
		FNetworkComponentBuilder<FBasePIENetworkComponentState>()
			.AsListenServer()
			.WithClients(2)
			.WithGameMode(AStealthGameMode::StaticClass())
			.WithPacketSimulationSettings(&PacketSettings)
			.Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
	}

	AStealthPawn* PawnOf(const TWeakObjectPtr<AStealthPlayerController>& Controller) const
	{
		return Controller.IsValid() ? Cast<AStealthPawn>(Controller->GetPawn()) : nullptr;
	}

	/** S's machine, every frame of the reversal: the receipts and what the HUD showed at each. */
	void SampleDefender()
	{
		using namespace StealthOwnPresentationPie;
		Frames.Sample();
		AStealthPlayerController* Local = LocalOf(MachineS);
		const TArray<UWorld*> All = StealthAssassinationPie::Worlds();
		if (Local == nullptr || !All.IsValidIndex(MachineS))
		{
			return;
		}
		const double Now = StealthAssassinationPie::ServerNow(All[MachineS]);
		const AStealthPawn* Copy = Cast<AStealthPawn>(Local->GetPawn());
		const bool bPlate = Local->GetResultPlate().bActive;
		if (CounterReceipt < 0.0 && Copy != nullptr && Copy->GetActionState().Pose == EActionPose::Counter)
		{
			CounterReceipt = Now;
			CounterContact = Copy->GetActionState().ContactServerTime;
			bSquareAtCounterReceipt = Local->IsBraceSquareShown();
			bPlateAtCounterReceipt = bPlate;
		}
		if (ResultReceipt < 0.0 && Local->GetReceivedReplies().ContainsByPredicate([](const FCommandReply& R) { return R.Kind == ECommandReplyKind::DefenceResult; }))
		{
			ResultReceipt = Now;
			bSquareAtResultReceipt = Local->IsBraceSquareShown();
			bPlateAtResultReceipt = bPlate;
		}
	}

	/** P1's machine, every frame of the kill: the Roster state's arrival and whether the card was up then. */
	void SampleVictim()
	{
		using namespace StealthOwnPresentationPie;
		Frames.Sample();
		const AStealthPawn* ServerVictim = PawnOf(Pair[0]);
		if (ServerVictim != nullptr && ServerVictim->HasDeathWrite())
		{
			DeathServerTime = ServerVictim->GetDeathState().DeathServerTime;
		}
		AStealthPlayerController* Local = LocalOf(MachineP1);
		const TArray<UWorld*> All = StealthAssassinationPie::Worlds();
		const AStealthPlayerState* State = Local != nullptr ? Local->GetPlayerState<AStealthPlayerState>() : nullptr;
		if (InformantArrival < 0.0 && State != nullptr && State->IsInformant() && All.IsValidIndex(MachineP1))
		{
			InformantArrival = StealthAssassinationPie::ServerNow(All[MachineP1]);
			bCardAtInformantArrival = !RecordsOf(MachineP1, EOwnPresentationEvent::RemovalCard, LogFromP1).IsEmpty();
		}
	}

	void RunReversal()
	{
		using namespace StealthAssassinationPie;
		Network
			.Until(TEXT("the round is Active and the players split 2v1"), [this]()
			{
				AStealthPlayerController* S = nullptr;
				AStealthPlayerController* P[2] = { nullptr, nullptr };
				if (!RoundActive() || !SplitTeams(S, P))
				{
					return false;
				}
				Solo = S;
				Pair[0] = P[0];
				Pair[1] = P[1];
				SoloPawn = Cast<AStealthPawn>(S->GetPawn());
				MachineS = StealthPresentationPie::MachineOf(S);
				MachineP1 = StealthPresentationPie::MachineOf(P[0]);
				return SoloPawn.IsValid() && MachineS != INDEX_NONE && MachineP1 != INDEX_NONE;
			})
			.Then(TEXT("place S clear of the bushes, the pair aside"), [this]()
			{
				MoveClearOfBushes(SoloPawn.Get());
				const FVector Right = FVector::CrossProduct(FVector::UpVector, SoloPawn->GetActorForwardVector().GetSafeNormal2D());
				PawnOf(Pair[0])->TeleportTo(SoloPawn->GetActorLocation() + Right * 1200.0, PawnOf(Pair[0])->GetActorRotation());
				PlaceStrikerFacing(PawnOf(Pair[1]), SoloPawn.Get(), 150.0);
				LogFromS = StealthOwnPresentationPie::LogSizeOf(MachineS);
				LogFromP1 = StealthOwnPresentationPie::LogSizeOf(MachineP1);
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("settle"), [this]() { Frames.Sample(); return ServerNow(ServerWorld()) >= Mark + OutcomePresentedAtContactPieData::SettleSec; })
			.Then(TEXT("1a S braces"), [this]()
			{
				AsOwner(SoloPawn.Get(), Seq, [](UPawnCommandComponent& Command) { Command.PrepareBegin(); });
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("1a the brace is up on S's machine"), [this]()
			{
				Frames.Sample();
				const AStealthPlayerController* Local = StealthOwnPresentationPie::LocalOf(MachineS);
				return Local != nullptr && Local->IsBraceSquareShown() && ServerNow(ServerWorld()) >= Mark + 0.4;
			})
			.Then(TEXT("1b P2 strikes S (reversed)"), [this]()
			{
				StrikeAt(PawnOf(Pair[1]), SoloPawn.Get(), Seq);
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("1b sample S's machine through the contact"), [this]()
			{
				SampleDefender();
				return ServerNow(ServerWorld()) >= Mark + OutcomePresentedAtContactPieData::StepSec;
			});
	}

	TEST_METHOD(BraceEndsAndDefencePlateAndRemovalCardStartAtContactNeverOnReceipt)
	{
		using namespace StealthAssassinationPie;
		Network.Then(TEXT("seam off"), []() { StealthOwnerPresentationTestSeams::SetPresentResultsOnReceipt(false); });
		RunReversal();
		Network
			.Then(TEXT("2a P1 in front of S (S's counter over)"), [this]()
			{
				PlaceFacing(PawnOf(Pair[0]), SoloPawn.Get(), 150.0);
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("2a settle"), [this]() { Frames.Sample(); return ServerNow(ServerWorld()) >= Mark + 0.6; })
			.Then(TEXT("2b S kills P1"), [this]()
			{
				StrikeAt(SoloPawn.Get(), PawnOf(Pair[0]), Seq);
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("2b sample P1's machine through its death"), [this]()
			{
				SampleVictim();
				return (DeathServerTime > 0.0 && ServerNow(ServerWorld()) >= DeathServerTime + 1.0) || ServerNow(ServerWorld()) > Mark + 4.0;
			});

		Network.Then(TEXT("assert"), [this]()
		{
			using namespace StealthOwnPresentationPie;
			const double FrameS = Frames.FrameOf(MachineS);
			const TArray<FOwnPresentationRecord> Plates = RecordsOf(MachineS, EOwnPresentationEvent::ResultPlate, LogFromS);
			const TArray<FOwnPresentationRecord> Squares = RecordsOf(MachineS, EOwnPresentationEvent::BraceSquare, LogFromS);
			const FOwnPresentationRecord* SquareEnd = Squares.FindByPredicate([](const FOwnPresentationRecord& R) { return R.Value == 0; });
			TestRunner->AddInfo(FString::Printf(TEXT("ASN49 defender: contact %.3f; Counter receipt %.3f (square %d, plate %d); DefenceResult receipt %.3f (square %d, plate %d); square end %.3f; plate '%s' at %.3f; frame %.3f"),
				CounterContact, CounterReceipt, bSquareAtCounterReceipt ? 1 : 0, bPlateAtCounterReceipt ? 1 : 0, ResultReceipt, bSquareAtResultReceipt ? 1 : 0, bPlateAtResultReceipt ? 1 : 0,
				SquareEnd != nullptr ? SquareEnd->ServerNow : -1.0, Plates.Num() > 0 ? *Plates[0].Text : TEXT("-"), Plates.Num() > 0 ? Plates[0].ServerNow : -1.0, FrameS));
			ASSERT_THAT(IsTrue(CounterContact > 0.0 && CounterReceipt > 0.0 && ResultReceipt > 0.0, TEXT("S's machine received the Counter and the FDefenceResult")));
			ASSERT_THAT(IsTrue(ResultReceipt < CounterContact - OutcomePresentedAtContactPieData::MeaningfulLeadSec, TEXT("the FDefenceResult arrived before the contact (the case is meaningful)")));
			ASSERT_THAT(IsTrue(bSquareAtResultReceipt && !bPlateAtResultReceipt, TEXT("at the FDefenceResult's arrival: the square still up, no plate")));
			if (CounterReceipt < CounterContact - 0.001)
			{
				ASSERT_THAT(IsTrue(bSquareAtCounterReceipt && !bPlateAtCounterReceipt, TEXT("at the Counter's arrival (before contact): the square still up, no plate")));
			}
			ASSERT_THAT(IsTrue(SquareEnd != nullptr && OnFrameOf(SquareEnd->ServerNow, CounterContact, FrameS), TEXT("the brace square is removed on the contact frame")));
			// (S's later "Target eliminated" for P1 follows in the log; the first plate is the defence plate.)
			ASSERT_THAT(IsTrue(Plates.Num() >= 1 && Plates[0].Text == TEXT("Strike reversed") && OnFrameOf(Plates[0].ServerNow, CounterContact, FrameS),
				TEXT("'Strike reversed' enters on S's local contact frame")));

			const double FrameP1 = Frames.FrameOf(MachineP1);
			const TArray<FOwnPresentationRecord> Cards = RecordsOf(MachineP1, EOwnPresentationEvent::RemovalCard, LogFromP1);
			const UEncounterViewSubsystem* View = StealthPresentationPie::ViewOf(Worlds()[MachineP1]);
			const FRoleChangeRecord* Dispatch = View != nullptr ? View->GetRoleChanges().FindByPredicate([this](const FRoleChangeRecord& R)
			{
				return FMath::IsNearlyEqual(R.DeathServerTime, DeathServerTime, 1e-6);
			}) : nullptr;
			TestRunner->AddInfo(FString::Printf(TEXT("ASN49 victim: DeathServerTime %.3f; informant state arrival %.3f (card up %d); removal card %.3f; dispatch (Camera Dying) %.3f; frame %.3f"),
				DeathServerTime, InformantArrival, bCardAtInformantArrival ? 1 : 0, Cards.Num() > 0 ? Cards[0].ServerNow : -1.0, Dispatch != nullptr ? Dispatch->ServerNow : -1.0, FrameP1));
			ASSERT_THAT(IsTrue(DeathServerTime > 0.0 && Cards.Num() == 1, TEXT("P1 was killed and its machine showed one removal card")));
			ASSERT_THAT(IsTrue(OnFrameOf(Cards[0].ServerNow, DeathServerTime, FrameP1), TEXT("the removal card starts on P1's DeathServerTime frame")));
			ASSERT_THAT(IsTrue(Dispatch != nullptr && OnFrameOf(Dispatch->ServerNow, DeathServerTime, FrameP1), TEXT("the role change Camera's Dying follows starts on the DeathServerTime frame")));
			if (InformantArrival > 0.0 && InformantArrival < DeathServerTime)
			{
				ASSERT_THAT(IsFalse(bCardAtInformantArrival, TEXT("the Roster state arrived first and the card was not up at its arrival")));
			}
		});
	}

	TEST_METHOD(ReceiptSeamPutsThePlateUpBeforeContact)
	{
		Network.Then(TEXT("seam on: results presented on receipt"), []() { StealthOwnerPresentationTestSeams::SetPresentResultsOnReceipt(true); });
		RunReversal();
		Network.Then(TEXT("assert the check fails under the seam"), [this]()
		{
			StealthOwnerPresentationTestSeams::SetPresentResultsOnReceipt(false);
			using namespace StealthOwnPresentationPie;
			const TArray<FOwnPresentationRecord> Plates = RecordsOf(MachineS, EOwnPresentationEvent::ResultPlate, LogFromS);
			TestRunner->AddInfo(FString::Printf(TEXT("ASN49 negative control: contact %.3f; plate at %.3f (receipt %.3f) -> the contact-frame check %s"), CounterContact,
				Plates.Num() > 0 ? Plates[0].ServerNow : -1.0, ResultReceipt,
				Plates.Num() > 0 && !OnFrameOf(Plates[0].ServerNow, CounterContact, Frames.FrameOf(MachineS)) ? TEXT("FAILS (as it must)") : TEXT("passes (the gate is broken)")));
			ASSERT_THAT(IsTrue(Plates.Num() == 1 && CounterContact > 0.0, TEXT("the seam showed S's plate")));
			ASSERT_THAT(IsTrue(Plates[0].ServerNow < CounterContact - OutcomePresentedAtContactPieData::MeaningfulLeadSec,
				TEXT("under the on-receipt seam the plate is up before contact: the contact-frame assertion would fail")));
		});
	}
};

#endif
