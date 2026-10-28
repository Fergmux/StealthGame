// NPC-Kill Penalty story nkp-017, AC1 / AC-NKP-38 (PEN-R16; ADR-0019 sections 2.3 step 4, 4.4; HUD-R9; ADR-0008 T2):
// StealthGame.Penalty.PIE.ExileBeatTiming.<Run>. Listen server + 2 clients on L_Prototype with a crowd, the players split
// 2v1; the pair player on a CLIENT machine (the owner) makes L ungraced NPC kills; the L-th exiles it. Its machine's
// presentation is sampled every frame from the exiling strike until the card (or, run 4, until the lost-data cap and the
// fallback window have passed):
//   Runs 1-3 (the FStrikeResult batch held +0, +300, +900 ms on the owner's machine: StealthOwnerPresentationTestSeams
//   SetReplyLatenessSec, the injected RPC lateness):
//     - the WrongTarget plate shows at its show time s (HUD F2: max(local contact frame, the result's processing time),
//       within one frame);
//     - the card enters at s + ExileBeatSec (within one frame), once, and the client-local card-entrance event
//       OnRemovalCardEntered fires on that frame (the event Camera's ViewRelease(StrikeLimit) and the exile cue consume);
//     - from the strike to the card the view stays on the released body (the camera's view target is the owner's own
//       former pawn) and, from the plate on, the owner no longer controls it (its local controller has no pawn);
//     - no frame is black before the card has entered (no full camera fade).
//   Run 4 (the exiling StrikeResult dropped on receipt: SetDropExilingStrikeResults): no plate and no card for the third kill;
//     the view stays on the released body and no frame is black; the role is presented at the lost-data cap.
// PENDING on the Camera epic (camera-system stories 017 and 018 are not built; recorded on the story, not asserted):
// ViewRelease(StrikeLimit) itself, and run 4's fallback fade ExileCardTimeoutSec after the owner's StrikeLimit state arrives.
// The measured times are logged ("NKP017 beat").

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "StealthGameMode.h"
#include "StealthPlayerState.h"
#include "StealthSettings.h"
#include "Tests/PIE/Penalty/PenaltyPresentationPieSupport.h"

namespace ExileBeatTimingPieData
{
	constexpr double SettleSec = 0.6;
	/** Run 4: how long after the owner's Roster informant state the window stays open (past the lost-data cap and the fallback). */
	constexpr double DroppedWindowSec = 4.5;
}

NETWORK_TEST_CLASS(ExileBeatTiming, "StealthGame.Penalty.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	TWeakObjectPtr<AStealthPlayerController> Owner;
	TWeakObjectPtr<AStealthPlayerController> Machine;
	TWeakObjectPtr<APawn> ServerBody;
	TWeakObjectPtr<APawn> ClientBody;
	uint32 Seq = 0x51000000u;
	double Mark = 0.0;
	int32 LogFrom = 0;
	StealthPenaltyPresentationPie::FMachineFrame Frames;
	/** Per-frame observations from the exiling strike to the card (or the end of run 4's window). */
	int32 Samples = 0;
	int32 OffBodyFrames = 0;
	/** The view target at the first frame off the body, and at the first sample (diagnostics). */
	FString FirstOffBodyTarget;
	FString FirstSampleTarget;
	int32 BlackFrames = 0;
	int32 ControlledAfterPlateFrames = 0;
	double CardEventAt = -1.0;
	int32 CardEvents = 0;
	double RosterInformantAt = -1.0;
	double PresentedAt = -1.0;
	FDelegateHandle CardHandle;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(StealthAssassinationPie::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
		}
		UStealthSettings* Settings = GetMutableDefault<UStealthSettings>();
		PreviousNpcCount = Settings->NpcCount;
		Settings->NpcCount = StealthPenaltyPie::CrowdNpcCount;
		StealthPenaltyHarnessSeams::SetCrowdlessTeamStartRespawn(false);
	}

	AFTER_ALL()
	{
		StealthOwnerPresentationTestSeams::SetReplyLatenessSec(0.0);
		StealthOwnerPresentationTestSeams::SetDropExilingStrikeResults(false);
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName); Timeout && !PreviousNetworkTimeout.IsEmpty())
		{
			Timeout->Set(*PreviousNetworkTimeout, ECVF_SetByCode);
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

	AFTER_EACH()
	{
		StealthOwnerPresentationTestSeams::SetReplyLatenessSec(0.0);
		StealthOwnerPresentationTestSeams::SetDropExilingStrikeResults(false);
		if (Machine.IsValid() && CardHandle.IsValid())
		{
			Machine->OnRemovalCardEntered.Remove(CardHandle);
		}
		CardHandle.Reset();
	}

	double Now() const { return StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()); }

	void Wait(const TCHAR* Label, double Seconds)
	{
		Network
			.Then(*FString::Printf(TEXT("%s: mark"), Label), [this]() { Mark = Now(); })
			.Until(Label, [this, Seconds]() { return Now() >= Mark + Seconds; });
	}

	AStealthPawn* OwnerPawn() const { return Owner.IsValid() ? Cast<AStealthPawn>(Owner->GetPawn()) : nullptr; }

	/** Samples one frame of the owner machine's presentation (from the exiling strike on). */
	void SampleFrame()
	{
		using namespace StealthPenaltyPresentationPie;
		const AStealthPlayerController* Local = Machine.Get();
		Frames.Sample(Local);
		const bool bCardIn = Local != nullptr && Local->GetExileCardEnteredAt() >= 0.0;
		const FViewSample View = SampleView(Local);
		++Samples;
		if (Samples == 1)
		{
			FirstSampleTarget = FString::Printf(TEXT("%s (possessed %s)"), *GetNameSafe(View.ViewTarget), *GetNameSafe(View.Possessed));
		}
		if (!bCardIn)
		{
			if (View.ViewTarget != ClientBody.Get() && FirstOffBodyTarget.IsEmpty())
			{
				FirstOffBodyTarget = FString::Printf(TEXT("%s at sample %d (possessed %s)"), *GetNameSafe(View.ViewTarget), Samples, *GetNameSafe(View.Possessed));
			}
			OffBodyFrames += View.ViewTarget != ClientBody.Get() ? 1 : 0;
			BlackFrames += IsBlack(View) ? 1 : 0;
			const bool bPlateShown = !RecordsOf(Local, EOwnPresentationEvent::ResultPlate, LogFrom).IsEmpty();
			ControlledAfterPlateFrames += bPlateShown && View.Possessed != nullptr && View.Possessed == ClientBody.Get() ? 1 : 0;
		}
		const AStealthPlayerState* State = Local != nullptr ? Local->GetPlayerState<AStealthPlayerState>() : nullptr;
		if (RosterInformantAt < 0.0 && State != nullptr && State->IsInformant())
		{
			RosterInformantAt = MachineNow(Local);
		}
		if (PresentedAt < 0.0 && Local != nullptr && Local->IsPresentedInformant())
		{
			PresentedAt = MachineNow(Local);
		}
	}

	void Run(double LatenessSec, bool bDrop)
	{
		using namespace StealthAssassinationPie;
		using namespace StealthPenaltyPresentationPie;
		const int32 L = Limit();
		Network
			.Until(TEXT("the round is Active, the crowd is up, the players split 2v1 and a pair player is a client"), [this]()
			{
				AStealthPlayerController* S = nullptr;
				AStealthPlayerController* P[2] = { nullptr, nullptr };
				if (!RoundActive() || !SplitTeams(S, P) || StealthPenaltyPie::Penalty() == nullptr)
				{
					return false;
				}
				AStealthPlayerController* Client = ClientPairPlayer(P);
				if (Client == nullptr || OwnerMachineController(Client) == nullptr)
				{
					return false;
				}
				Owner = Client;
				Machine = OwnerMachineController(Client);
				return OwnerPawn() != nullptr && Machine->GetPawn() != nullptr;
			})
			.Then(TEXT("the owner stands clear of the bushes"), [this]() { MoveClearOfBushes(OwnerPawn()); });
		Wait(TEXT("settle"), ExileBeatTimingPieData::SettleSec);
		for (int32 Kill = 0; Kill < L - 1; ++Kill)
		{
			Network.Then(*FString::Printf(TEXT("NPC kill %d of %d"), Kill + 1, L), [this]() { NpcKill(OwnerPawn(), Seq); });
			Wait(TEXT("lockout"), StrikeStepSec);
		}
		Network
			.Then(TEXT("arm the seam, record the body, then the exiling NPC kill"), [this, LatenessSec, bDrop]()
			{
				StealthOwnerPresentationTestSeams::SetReplyLatenessSec(LatenessSec);
				StealthOwnerPresentationTestSeams::SetDropExilingStrikeResults(bDrop);
				ServerBody = OwnerPawn();
				ClientBody = Machine->GetPawn();
				LogFrom = Machine->GetOwnPresentationLog().Num();
				CardHandle = Machine->OnRemovalCardEntered.AddLambda([this](double ServerNow)
				{
					++CardEvents;
					CardEventAt = ServerNow;
				});
				NpcKill(OwnerPawn(), Seq);
			})
			.Until(TEXT("sample every frame until the card enters (or run 4's window has passed)"), [this, bDrop]()
			{
				SampleFrame();
				const AStealthPlayerController* Local = Machine.Get();
				if (Local == nullptr)
				{
					return false;
				}
				if (!bDrop)
				{
					return Local->GetExileCardEnteredAt() >= 0.0;
				}
				return RosterInformantAt >= 0.0 && StealthPenaltyPresentationPie::MachineNow(Local) >= RosterInformantAt + ExileBeatTimingPieData::DroppedWindowSec;
			})
			.Then(TEXT("assert the exile beat"), [this, LatenessSec, bDrop]()
			{
				using namespace StealthPenaltyPresentationPie;
				const AStealthPlayerController* Local = Machine.Get();
				const double Beat = UOwnResultPlateSettings::Get()->ExileBeatSec;
				const double Frame = Frames.Frame();
				const TArray<FOwnPresentationRecord> Plates = RecordsOf(Local, EOwnPresentationEvent::ResultPlate, LogFrom);
				const TArray<FOwnPresentationRecord> Cards = RecordsOf(Local, EOwnPresentationEvent::ExileCard, LogFrom);
				for (const FOwnPresentationRecord& Plate : Plates)
				{
					TestRunner->AddInfo(FString::Printf(TEXT("NKP017 beat (lateness %.0f ms%s): plate '%s' at %.3f (contact %.3f, arrival %.3f%s)"), LatenessSec * 1000.0,
						bDrop ? TEXT(", dropped") : TEXT(""), *Plate.Text, Plate.ServerNow, Plate.DueServerTime, Plate.ArrivalServerTime, Plate.bLate ? TEXT(", late") : TEXT("")));
				}
				TestRunner->AddInfo(FString::Printf(TEXT("NKP017 beat: card %s at %.3f, event x%d at %.3f; %d frames sampled, %d off the body, %d black, %d controlled after the plate; roster informant %.3f, presented %.3f; frame %.4f"),
					Cards.Num() > 0 ? *Cards[0].Text : TEXT("none"), Cards.Num() > 0 ? Cards[0].ServerNow : -1.0, CardEvents, CardEventAt, Samples, OffBodyFrames, BlackFrames,
					ControlledAfterPlateFrames, RosterInformantAt, PresentedAt, Frame));
				TestRunner->AddInfo(FString::Printf(TEXT("NKP017 beat: body %s; first sample view %s; first frame off the body %s"), *GetNameSafe(ClientBody.Get()), *FirstSampleTarget,
					FirstOffBodyTarget.IsEmpty() ? TEXT("none") : *FirstOffBodyTarget));
				TestRunner->AddInfo(TEXT("NKP017 PENDING (camera-system stories 017/018 not built): ViewRelease(StrikeLimit) from the card-entrance event, and the ExileCardTimeoutSec fallback fade"));

				ASSERT_THAT(IsTrue(Samples > 10, TEXT("fixture: the owner machine was sampled every frame")));
				ASSERT_THAT(IsTrue(ClientBody.IsValid(), TEXT("fixture: the owner's client copy of its body exists")));
				ASSERT_THAT(IsTrue(OffBodyFrames == 0, TEXT("the view stays on the released body until the card-entrance event")));
				ASSERT_THAT(IsTrue(BlackFrames == 0, TEXT("no frame is black before the card has entered")));
				ASSERT_THAT(IsTrue(ControlledAfterPlateFrames == 0, TEXT("from the plate on the owner no longer controls the released body")));
				if (bDrop)
				{
					ASSERT_THAT(IsTrue(StealthOwnerPresentationTestSeams::GetDroppedExilingResults() == 1, TEXT("fixture: exactly the exiling result was dropped")));
					ASSERT_THAT(IsTrue(Plates.IsEmpty() && Cards.IsEmpty() && CardEvents == 0, TEXT("with the result lost: no plate, no card, no card-entrance event (the data never landed)")));
					ASSERT_THAT(IsTrue(RosterInformantAt >= 0.0 && PresentedAt >= 0.0, TEXT("the owner's informant state arrived and the role was presented (the lost-data cap)")));
					return;
				}
				ASSERT_THAT(IsTrue(Plates.Num() == 1 && Plates[0].Text == TEXT("Wrong target"), TEXT("exactly one plate, 'Wrong target', for the exiling kill")));
				const double S = Plates[0].ServerNow;
				const double ShowDue = FMath::Max(Plates[0].DueServerTime, Plates[0].ArrivalServerTime);
				ASSERT_THAT(IsTrue(StealthOwnPresentationPie::OnFrameOf(S, ShowDue, Frame), TEXT("the plate shows at its F2 show time s (max of the local contact frame and the result's arrival)")));
				ASSERT_THAT(IsTrue(Cards.Num() == 1, TEXT("the card enters once")));
				ASSERT_THAT(IsTrue(Cards[0].Text == TEXT("Informant / Too many wrong targets / You watch for your team now."), TEXT("the card reads the D11 wording")));
				ASSERT_THAT(IsTrue(StealthOwnPresentationPie::OnFrameOf(Cards[0].ServerNow, S + Beat, Frame), TEXT("the card enters at s + ExileBeatSec (within one frame)")));
				ASSERT_THAT(IsTrue(CardEvents == 1 && FMath::IsNearlyEqual(CardEventAt, Cards[0].ServerNow, 0.0005), TEXT("the card-entrance event fires once, on the card's frame")));
			});
	}

	TEST_METHOD(OnTimeCardEntersAtPlatePlusBeat) { Run(0.0, false); }
	TEST_METHOD(Late300MsCardEntersAtPlatePlusBeat) { Run(0.3, false); }
	TEST_METHOD(Late900MsCardEntersAtPlatePlusBeat) { Run(0.9, false); }
	TEST_METHOD(DroppedResultHoldsTheViewWithoutACard) { Run(0.0, true); }
};

#endif
