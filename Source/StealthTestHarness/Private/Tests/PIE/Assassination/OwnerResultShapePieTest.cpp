// Assassination story asn-015, AC2 / AC-ASN-16 payload clause, delivery half (ADR-0008 T2; listen server + 2 clients on
// L_Prototype, NpcCount 0): StealthGame.Assassination.PIE.OwnerResultShape.<method>. The three players split 2v1; the
// solo player S strikes through the production path with its own origin (its mailbox and reply sink, as ingress opens it),
// and one strike is a pair player's (P2) into S's brace:
//   1 S kills an unbound Shared Pawn (an NPC)        -> S: WrongTarget, tally 1
//   2 S whiffs at an empty spot                      -> S: NoContact, tally 1
//   3 S braces, P2 strikes S: reversed               -> P2: Reversed; S: FDefenceResult {Reversed}
//   4 S kills P2, now an informant                   -> S: WrongTarget, tally 2
//   5 S kills P1, the pair's last Active player      -> S: RivalEliminated, tally 2 (the round ends)
// Every result reaches its owner's machine through ClientCommandReplies (received on that machine's own controller):
// each FStrikeResult entry has the same serialised size, the NPC and informant entries differ only in ClientCommandSeq,
// TallyAfter (+1 each) and their own contact times, the rival kill, the whiff and the reversal leave the tally unchanged,
// and P1, which strikes nothing, receives no result field at all.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "Serialization/BitWriter.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/AssassinationPieSupport.h"

namespace OwnerResultShapePieData
{
	constexpr double SettleSec = 0.6;
	/** Past AttackAttemptLockoutSec (1.75 s) and the contact. */
	constexpr double StepSec = 2.4;

	TArray<FCommandReply> Results(const AStealthPlayerController* Controller, ECommandReplyKind Kind)
	{
		TArray<FCommandReply> Out;
		if (Controller != nullptr)
		{
			for (const FCommandReply& Reply : Controller->GetReceivedReplies())
			{
				if (Reply.Kind == Kind)
				{
					Out.Add(Reply);
				}
			}
		}
		return Out;
	}

	int64 WireBits(const FCommandReply& Reply)
	{
		FBitWriter Writer(0, true);
		FCommandReply Copy = Reply;
		bool bSuccess = false;
		Copy.NetSerialize(Writer, nullptr, bSuccess);
		return Writer.GetNumBits();
	}
}

NETWORK_TEST_CLASS(OwnerResultShape, "StealthGame.Assassination.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	TWeakObjectPtr<AStealthPlayerController> Solo;
	TWeakObjectPtr<AStealthPlayerController> Pair[2];
	TWeakObjectPtr<AStealthPawn> SoloPawn;
	TWeakObjectPtr<AStealthPawn> Npc;
	uint32 Seq = 0x10000000u;
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

	void Step(const TCHAR* Label, TFunction<void()> Act)
	{
		using namespace StealthAssassinationPie;
		Network
			.Then(Label, [this, Act]() { Act(); Mark = ServerNow(ServerWorld()); })
			.Until(*FString::Printf(TEXT("%s: settle"), Label), [this]() { return ServerNow(ServerWorld()) >= Mark + OwnerResultShapePieData::StepSec; });
	}

	TEST_METHOD(EveryResultReachesOnlyItsOwnerAtOneSize)
	{
		using namespace StealthAssassinationPie;
		using namespace OwnerResultShapePieData;
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
				return SoloPawn.IsValid() && OwnerMachineController(S) && OwnerMachineController(P[0]) && OwnerMachineController(P[1]);
			})
			.Then(TEXT("place S clear of the bushes, the pair aside"), [this]()
			{
				MoveClearOfBushes(SoloPawn.Get());
				const FVector Right = FVector::CrossProduct(FVector::UpVector, SoloPawn->GetActorForwardVector().GetSafeNormal2D());
				PawnOf(Pair[0])->TeleportTo(SoloPawn->GetActorLocation() + Right * 1200.0, PawnOf(Pair[0])->GetActorRotation());
				PawnOf(Pair[1])->TeleportTo(SoloPawn->GetActorLocation() - Right * 1200.0, PawnOf(Pair[1])->GetActorRotation());
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("settle"), [this]() { return ServerNow(ServerWorld()) >= Mark + SettleSec; });

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
		Network
			.Then(TEXT("3a S braces, P2 comes in front"), [this]()
			{
				AsOwner(SoloPawn.Get(), Seq, [](UPawnCommandComponent& Command) { Command.PrepareBegin(); });
				PlaceStrikerFacing(PawnOf(Pair[1]), SoloPawn.Get(), 150.0); // P2 strikes: its own facing (story asn-024 de-flake)
				Mark = StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld());
			})
			.Until(TEXT("3a: settle (0.4 s, inside the 4 s window)"), [this]() { return StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()) >= Mark + 0.4; });
		Network.Then(TEXT("3b P2 strikes S (reversed)"), [this]() { StrikeAt(PawnOf(Pair[1]), SoloPawn.Get(), Seq); Mark = StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()); })
			.Until(TEXT("3b: settle until P2 respawned"), [this]()
			{
				const double Elapsed = StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()) - Mark;
				return Elapsed >= 6.0 && PawnOf(Pair[1]) != nullptr;
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
			const AStealthPlayerController* SoloMachine = StealthAssassinationPie::OwnerMachineController(Solo.Get());
			const AStealthPlayerController* P1Machine = StealthAssassinationPie::OwnerMachineController(Pair[0].Get());
			const AStealthPlayerController* P2Machine = StealthAssassinationPie::OwnerMachineController(Pair[1].Get());
			ASSERT_THAT(IsTrue(SoloMachine && P1Machine && P2Machine, TEXT("every player's machine controller is found")));
			const TArray<FCommandReply> SoloStrikes = Results(SoloMachine, ECommandReplyKind::StrikeResult);
			const TArray<FCommandReply> SoloDefence = Results(SoloMachine, ECommandReplyKind::DefenceResult);
			const TArray<FCommandReply> P2Strikes = Results(P2Machine, ECommandReplyKind::StrikeResult);
			for (const FCommandReply& R : SoloStrikes)
			{
				TestRunner->AddInfo(FString::Printf(TEXT("ASNPIE S result %d tally %d exiled %d contact %.3f seq %u"), static_cast<int32>(R.Strike.Result), R.Strike.TallyAfter,
					R.Strike.bExiled ? 1 : 0, R.Strike.ContactServerTime, R.Seq.Seq));
			}
			TestRunner->AddInfo(FString::Printf(TEXT("ASNPIE S defence %d; P2 strikes %d; P1 entries %d (solo is host %d)"), SoloDefence.Num(), P2Strikes.Num(),
				P1Machine->GetReceivedReplies().Num(), Solo->IsLocalController() ? 1 : 0));

			ASSERT_THAT(IsTrue(SoloStrikes.Num() == 4, TEXT("S's machine received exactly its four FStrikeResults")));
			const EStrikeResult Expected[] = { EStrikeResult::WrongTarget, EStrikeResult::NoContact, EStrikeResult::WrongTarget, EStrikeResult::RivalEliminated };
			const int32 Tally[] = { 1, 1, 2, 2 };
			for (int32 Index = 0; Index < 4; ++Index)
			{
				ASSERT_THAT(IsTrue(SoloStrikes[Index].Strike.Result == Expected[Index] && SoloStrikes[Index].Strike.TallyAfter == Tally[Index],
					*FString::Printf(TEXT("S result %d: kind %d with tally %d"), Index + 1, static_cast<int32>(Expected[Index]), Tally[Index])));
			}
			ASSERT_THAT(IsTrue(SoloDefence.Num() == 1 && SoloDefence[0].Defence.Result == EDefenceResult::Reversed, TEXT("S (the bound defender) received one FDefenceResult {Reversed}")));
			ASSERT_THAT(IsTrue(P2Strikes.Num() == 1 && P2Strikes[0].Strike.Result == EStrikeResult::Reversed && P2Strikes[0].Strike.TallyAfter == 0,
				TEXT("P2 (the reversed attacker) received exactly its Reversed result, tally unchanged (0)")));
			ASSERT_THAT(IsTrue(Results(P2Machine, ECommandReplyKind::DefenceResult).IsEmpty(), TEXT("P2 received no FDefenceResult")));
			ASSERT_THAT(IsTrue(Results(P1Machine, ECommandReplyKind::StrikeResult).IsEmpty() && Results(P1Machine, ECommandReplyKind::DefenceResult).IsEmpty(),
				TEXT("P1, which struck nothing, received no result field")));
			TSet<int64> Widths;
			for (const FCommandReply& R : SoloStrikes)
			{
				Widths.Add(WireBits(R));
			}
			Widths.Add(WireBits(P2Strikes[0]));
			ASSERT_THAT(IsTrue(Widths.Num() == 1, TEXT("every FStrikeResult entry has the same serialised size")));
			const FStrikeResult& NpcKill = SoloStrikes[0].Strike;
			const FStrikeResult& InformantKill = SoloStrikes[2].Strike;
			ASSERT_THAT(IsTrue(NpcKill.Result == InformantKill.Result && NpcKill.bExiled == InformantKill.bExiled && NpcKill.TallyAfter + 1 == InformantKill.TallyAfter
				&& SoloStrikes[0].Seq != SoloStrikes[2].Seq, TEXT("the NPC and informant kills differ only in ClientCommandSeq, TallyAfter (+1) and their own contact times")));
		});
	}
};

#endif
