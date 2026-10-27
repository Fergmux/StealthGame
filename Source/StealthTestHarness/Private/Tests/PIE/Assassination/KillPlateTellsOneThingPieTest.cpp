// Assassination story asn-024 AC1 / AC-ASN-16 UI clause (ADR-0008 T2; listen server + 2 clients on L_Prototype, NpcCount 0):
// StealthGame.Assassination.PIE.KillPlateTellsOneThing.<method>. The players split 2v1 (S alone, P1 and P2 the pair) and
// strike through the production path with their own origins:
//   1 S kills an unbound Shared Pawn (an NPC)    -> S: "Wrong target"
//   2 S whiffs at an empty spot                  -> S: "No contact"
//   3 S braces, P2 strikes S (reversed)          -> P2: "Reversed" inside its own role-change sequence; S: "Strike reversed"
//   4 S kills P2, now an informant               -> S: "Wrong target" (byte-identical to step 1's)
//   5 S kills P1, the pair's last Active player  -> S: "Target eliminated"
// Every plate is presented on its owner's machine on the local contact frame (FStrikeResult.ContactServerTime; the
// defender's Counter ContactServerTime), never earlier and at most one frame later, through the one F3 timeline (the plate
// view's timeline origin is the show frame for every outcome). The reversed attacker's plate and its removal card enter on
// one frame, the plate first (art bible 7.6 rule 7). Evidence screenshots asn-16-*.png are the run-and-observe step's.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/OwnPresentationPieSupport.h"

namespace KillPlateTellsOneThingPieData
{
	constexpr double SettleSec = 0.6;
	/** Past AttackAttemptLockoutSec (1.75 s) and the contact. */
	constexpr double StepSec = 2.4;
	/** The reversed attacker's respawn as an informant. */
	constexpr double RespawnWaitSec = 6.0;
}

NETWORK_TEST_CLASS(KillPlateTellsOneThing, "StealthGame.Assassination.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	TWeakObjectPtr<AStealthPlayerController> Solo;
	TWeakObjectPtr<AStealthPlayerController> Pair[2];
	TWeakObjectPtr<AStealthPawn> SoloPawn;
	TWeakObjectPtr<AStealthPawn> Npc;
	int32 MachineS = INDEX_NONE;
	int32 MachineP2 = INDEX_NONE;
	int32 LogFromS = 0;
	int32 LogFromP2 = 0;
	/** The defender's Counter contact (server). */
	double CounterContact = -1.0;
	StealthOwnPresentationPie::FFrameTracker Frames;
	uint32 Seq = 0x24100000u;
	double Mark = 0.0;
	/** Server time the respawned P2 pawn was first seen (-1: not yet); it settles before step 4 strikes it. */
	double RespawnSeenAt = -1.0;

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
	}

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FBasePIENetworkComponentState>().AsListenServer().WithClients(2).WithGameMode(AStealthGameMode::StaticClass()).Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
	}

	AStealthPawn* PawnOf(const TWeakObjectPtr<AStealthPlayerController>& Controller) const
	{
		return Controller.IsValid() ? Cast<AStealthPawn>(Controller->GetPawn()) : nullptr;
	}

	/** Every frame: the frame lengths, and the defender's Counter contact while it is up. */
	bool SampleFrame()
	{
		Frames.Sample();
		if (SoloPawn.IsValid() && SoloPawn->GetActionState().Pose == EActionPose::Counter)
		{
			CounterContact = SoloPawn->GetActionState().ContactServerTime;
		}
		return true;
	}

	void Step(const TCHAR* Label, TFunction<void()> Act, double Seconds = KillPlateTellsOneThingPieData::StepSec, TFunction<bool()> Also = nullptr)
	{
		using namespace StealthAssassinationPie;
		Network
			.Then(Label, [this, Act]() { Act(); Mark = ServerNow(ServerWorld()); })
			.Until(*FString::Printf(TEXT("%s: settle"), Label), [this, Seconds, Also]()
			{
				SampleFrame();
				return ServerNow(ServerWorld()) >= Mark + Seconds && (!Also || Also());
			});
	}

	TEST_METHOD(FivePlatesOnTheirContactFrameWithOneTimeline)
	{
		using namespace StealthAssassinationPie;
		using namespace KillPlateTellsOneThingPieData;
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
				MachineP2 = StealthPresentationPie::MachineOf(P[1]);
				return SoloPawn.IsValid() && MachineS != INDEX_NONE && MachineP2 != INDEX_NONE;
			})
			.Then(TEXT("place S clear of the bushes, the pair aside"), [this]()
			{
				MoveClearOfBushes(SoloPawn.Get());
				const FVector Right = FVector::CrossProduct(FVector::UpVector, SoloPawn->GetActorForwardVector().GetSafeNormal2D());
				PawnOf(Pair[0])->TeleportTo(SoloPawn->GetActorLocation() + Right * 1200.0, PawnOf(Pair[0])->GetActorRotation());
				PawnOf(Pair[1])->TeleportTo(SoloPawn->GetActorLocation() - Right * 1200.0, PawnOf(Pair[1])->GetActorRotation());
				LogFromS = StealthOwnPresentationPie::LogSizeOf(MachineS);
				LogFromP2 = StealthOwnPresentationPie::LogSizeOf(MachineP2);
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("settle"), [this]() { SampleFrame(); return ServerNow(ServerWorld()) >= Mark + SettleSec; });

		Step(TEXT("1 S kills an NPC"), [this]()
		{
			Npc = SpawnUnboundPawn(ServerWorld(), SoloPawn.Get(), 150.0);
			StrikeAt(SoloPawn.Get(), Npc.Get(), Seq);
		});
		Step(TEXT("2 S whiffs"), [this]()
		{
			FAttackPayload Payload;
			const FVector Right = FVector::CrossProduct(FVector::UpVector, SoloPawn->GetActorForwardVector().GetSafeNormal2D());
			Payload.WorldPoint = SoloPawn->GetActorLocation() + Right * 600.0; // nobody there
			AsOwner(SoloPawn.Get(), Seq, [&Payload](UPawnCommandComponent& Command) { Command.Attack(Payload); });
		});
		Step(TEXT("3a S moves off the NPC's body, braces, P2 comes in front"), [this]()
		{
			// Away from the NPC body lying 150 cm ahead (a dead body in reach is NoContact, never a reversal).
			const FVector Right = FVector::CrossProduct(FVector::UpVector, SoloPawn->GetActorForwardVector().GetSafeNormal2D());
			SoloPawn->TeleportTo(SoloPawn->GetActorLocation() - Right * 500.0, SoloPawn->GetActorRotation());
			AsOwner(SoloPawn.Get(), Seq, [](UPawnCommandComponent& Command) { Command.PrepareBegin(); });
			PlaceStrikerFacing(PawnOf(Pair[1]), SoloPawn.Get(), 150.0);
		}, 0.4);
		Step(TEXT("3b P2 strikes S (reversed)"), [this]()
		{
			// The geometry the resolver sees at the press (diagnostics: a NoContact here is a placement, not a plate, fault).
			const AStealthPawn* Striker = PawnOf(Pair[1]);
			const FVector ToTarget = (SoloPawn->GetActorLocation() - Striker->GetActorLocation()).GetSafeNormal2D();
			const double Bearing = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Striker->GetActorForwardVector().GetSafeNormal2D(), ToTarget), -1.0, 1.0)));
			TestRunner->AddInfo(FString::Printf(TEXT("ASN16 reversal press: distance %.1f cm, bearing %.1f deg, dz %.1f cm, S pose %d"), FVector::Dist2D(SoloPawn->GetActorLocation(), Striker->GetActorLocation()),
				Bearing, SoloPawn->GetActorLocation().Z - Striker->GetActorLocation().Z, static_cast<int32>(SoloPawn->GetActionState().Pose)));
			StrikeAt(PawnOf(Pair[1]), SoloPawn.Get(), Seq);
		}, RespawnWaitSec,
			[this]()
			{
				// NKP stories nkp-009 / nkp-012: Penalty's respawn (15 s by default) settles one second before step 4 strikes it.
				const double Now = StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld());
				if (PawnOf(Pair[1]) == nullptr)
				{
					RespawnSeenAt = -1.0;
					return false;
				}
				if (RespawnSeenAt < 0.0)
				{
					RespawnSeenAt = Now;
				}
				return Now - RespawnSeenAt >= 1.0;
			});
		Step(TEXT("4 S kills P2 (an informant)"), [this]()
		{
			PlaceFacing(PawnOf(Pair[1]), SoloPawn.Get(), 150.0);
			StrikeAt(SoloPawn.Get(), PawnOf(Pair[1]), Seq);
		});
		Step(TEXT("5 S kills P1 (the rival)"), [this]()
		{
			PlaceFacing(PawnOf(Pair[0]), SoloPawn.Get(), 150.0);
			StrikeAt(SoloPawn.Get(), PawnOf(Pair[0]), Seq);
		});

		Network.Then(TEXT("assert"), [this]()
		{
			using namespace StealthOwnPresentationPie;
			const AStealthPlayerController* SoloMachine = OwnerMachineController(Solo.Get());
			const AStealthPlayerController* P2Machine = OwnerMachineController(Pair[1].Get());
			ASSERT_THAT(IsTrue(SoloMachine && P2Machine, TEXT("both owners' machine controllers are found")));

			// The contacts each plate answers, from the owners' own data: S's four FStrikeResults, S's Counter, P2's result.
			TArray<double> SoloContacts;
			double P2Contact = -1.0;
			for (const FCommandReply& Reply : SoloMachine->GetReceivedReplies())
			{
				if (Reply.Kind == ECommandReplyKind::StrikeResult)
				{
					SoloContacts.Add(Reply.Strike.ContactServerTime);
				}
			}
			for (const FCommandReply& Reply : P2Machine->GetReceivedReplies())
			{
				if (Reply.Kind == ECommandReplyKind::StrikeResult)
				{
					P2Contact = Reply.Strike.ContactServerTime;
				}
			}
			ASSERT_THAT(IsTrue(SoloContacts.Num() == 4 && P2Contact > 0.0 && CounterContact > 0.0, TEXT("four S results, P2's result and S's Counter were seen")));
			const double Expected[5] = { SoloContacts[0], SoloContacts[1], CounterContact, SoloContacts[2], SoloContacts[3] };
			const TCHAR* const Texts[5] = { TEXT("Wrong target"), TEXT("No contact"), TEXT("Strike reversed"), TEXT("Wrong target"), TEXT("Target eliminated") };

			const TArray<FOwnPresentationRecord> SoloPlates = RecordsOf(MachineS, EOwnPresentationEvent::ResultPlate, LogFromS);
			const double FrameS = Frames.FrameOf(MachineS);
			for (const FOwnPresentationRecord& R : SoloPlates)
			{
				TestRunner->AddInfo(FString::Printf(TEXT("ASN16 S plate '%s' at %.3f for contact %.3f (arrival %.3f, late %d), frame %.3f"), *R.Text, R.ServerNow, R.DueServerTime,
					R.ArrivalServerTime, R.bLate ? 1 : 0, FrameS));
			}
			ASSERT_THAT(IsTrue(SoloPlates.Num() == 5, TEXT("S's machine showed exactly five plates")));
			for (int32 Index = 0; Index < 5; ++Index)
			{
				ASSERT_THAT(IsTrue(SoloPlates[Index].Text == Texts[Index], *FString::Printf(TEXT("plate %d reads '%s'"), Index + 1, Texts[Index])));
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(SoloPlates[Index].DueServerTime, Expected[Index], 0.002), *FString::Printf(TEXT("plate %d answers its own contact"), Index + 1)));
				ASSERT_THAT(IsTrue(OnFrameOf(SoloPlates[Index].ServerNow, Expected[Index], FrameS), *FString::Printf(TEXT("plate %d on the local contact frame (never before, within one frame)"), Index + 1)));
			}
			ASSERT_THAT(IsTrue(SoloPlates[0].Text.Equals(SoloPlates[3].Text, ESearchCase::CaseSensitive), TEXT("the NPC and informant plates are byte-identical")));

			const TArray<FOwnPresentationRecord> P2Plates = RecordsOf(MachineP2, EOwnPresentationEvent::ResultPlate, LogFromP2);
			const TArray<FOwnPresentationRecord> P2Cards = RecordsOf(MachineP2, EOwnPresentationEvent::RemovalCard, LogFromP2);
			const double FrameP2 = Frames.FrameOf(MachineP2);
			for (const FOwnPresentationRecord& R : P2Plates)
			{
				TestRunner->AddInfo(FString::Printf(TEXT("ASN16 P2 plate '%s' at %.3f for contact %.3f; removal card at %.3f"), *R.Text, R.ServerNow, R.DueServerTime,
					P2Cards.Num() > 0 ? P2Cards[0].ServerNow : -1.0));
			}
			ASSERT_THAT(IsTrue(P2Plates.Num() == 1 && P2Plates[0].Text == TEXT("Reversed"), TEXT("P2's machine showed exactly its 'Reversed' plate")));
			ASSERT_THAT(IsTrue(OnFrameOf(P2Plates[0].ServerNow, P2Contact, FrameP2), TEXT("'Reversed' on P2's local contact frame")));
			ASSERT_THAT(IsTrue(P2Cards.Num() >= 1 && FMath::Abs(P2Cards[0].ServerNow - P2Plates[0].ServerNow) <= 0.001,
				TEXT("'Reversed' and the removal card enter on one frame: one sequence (art bible 7.6 rule 7)")));
			const TArray<FOwnPresentationRecord>& P2Log = P2Machine->GetOwnPresentationLog();
			const int32 PlateAt = P2Log.IndexOfByPredicate([](const FOwnPresentationRecord& R) { return R.Event == EOwnPresentationEvent::ResultPlate; });
			const int32 CardAt = P2Log.IndexOfByPredicate([](const FOwnPresentationRecord& R) { return R.Event == EOwnPresentationEvent::RemovalCard; });
			ASSERT_THAT(IsTrue(PlateAt != INDEX_NONE && CardAt != INDEX_NONE && PlateAt < CardAt, TEXT("the plate enters first and the removal card follows (and preempts)")));
		});
	}
};

#endif
