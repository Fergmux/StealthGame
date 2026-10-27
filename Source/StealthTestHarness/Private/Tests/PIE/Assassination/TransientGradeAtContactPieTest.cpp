// Assassination story asn-021 AC1 / AC-ASN-23 TransientGradeAtContact (ADR-0008 T2; listen server + 2 clients on
// L_Prototype, NpcCount 0): StealthGame.Assassination.PIE.TransientGradeAtContact.<method>. Players: the solo player S
// and the pair P1, P2 (each on its own machine). In order:
//   1 a whiff: P1 strikes the empty spot in front of it -> no grade on any machine;
//   2 a reversal: S braces, P1 strikes S -> one grade on P1's machine (the reversed attacker is also the victim: its
//     AttackerContact and VictimDeath calls answer one contact, so the beat runs once) and on
//     S's machine (DefenderCounter, its Counter's ContactServerTime), none on P2's (the ally / bystander);
//   3 a kill of a player: S strikes P2 -> the grade on S's machine (AttackerContact) and on P2's (VictimDeath, its own
//     DeathServerTime), none on P1's (the informant ally of the victim).
// Every call lands on its machine's local contact frame: no earlier than the contact it answers and within one frame plus
// the result's travel after it; the world's time dilation is 1.0 at every call and after.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/IConsoleManager.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/PresentationPieSupport.h"

NETWORK_TEST_CLASS(TransientGradeAtContact, "StealthGame.Assassination.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	TWeakObjectPtr<AStealthPlayerController> Solo;
	TWeakObjectPtr<AStealthPlayerController> Pair[2];
	int32 MachineS = INDEX_NONE;
	int32 MachineP1 = INDEX_NONE;
	int32 MachineP2 = INDEX_NONE;
	/** Grade calls per machine before each step. */
	int32 Before[3][4] = {};
	double ReversalContact = -1.0;
	double KillContact = -1.0;
	uint32 Seq = 0x32100000u;
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

	const TArray<FTransientGradeCall>* Calls(int32 Machine) const
	{
		const TArray<UWorld*> All = StealthAssassinationPie::Worlds();
		const UEncounterViewSubsystem* View = All.IsValidIndex(Machine) ? StealthPresentationPie::ViewOf(All[Machine]) : nullptr;
		return View != nullptr ? &View->GetGradeCalls() : nullptr;
	}

	int32 CountOf(int32 Machine) const
	{
		const TArray<FTransientGradeCall>* C = Calls(Machine);
		return C != nullptr ? C->Num() : 0;
	}

	void Snapshot(int32 Step)
	{
		for (int32 Machine = 0; Machine < 3; ++Machine)
		{
			Before[Machine][Step] = CountOf(Machine);
		}
	}

	void Hold(const TCHAR* Label, double Seconds, TFunction<bool()> Also = nullptr)
	{
		Network
			.Then(*FString::Printf(TEXT("%s: mark"), Label), [this]() { Mark = StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()); })
			.Until(Label, [this, Seconds, Also]()
			{
				const bool bAlso = !Also || Also(); // sampled every frame (it may latch a short-lived pose)
				return StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()) >= Mark + Seconds && bAlso;
			});
	}

	/** The calls Machine made in step Step (between Snapshot(Step) and Snapshot(Step + 1)). */
	TArray<FTransientGradeCall> StepCalls(int32 Machine, int32 Step) const
	{
		TArray<FTransientGradeCall> Out;
		const TArray<FTransientGradeCall>* C = Calls(Machine);
		if (C != nullptr)
		{
			for (int32 Index = Before[Machine][Step]; Index < Before[Machine][Step + 1] && Index < C->Num(); ++Index)
			{
				Out.Add((*C)[Index]);
			}
		}
		return Out;
	}

	TEST_METHOD(OnlyParticipantsGradeOnTheirContactFrameAndNeverForAWhiff)
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
				MachineS = StealthPresentationPie::MachineOf(S);
				MachineP1 = StealthPresentationPie::MachineOf(P[0]);
				MachineP2 = StealthPresentationPie::MachineOf(P[1]);
				return MachineS != INDEX_NONE && MachineP1 != INDEX_NONE && MachineP2 != INDEX_NONE;
			})
			.Then(TEXT("place S clear of the bushes, P2 aside"), [this]()
			{
				MoveClearOfBushes(PawnOf(Solo));
				const FVector Right = FVector::CrossProduct(FVector::UpVector, PawnOf(Solo)->GetActorForwardVector().GetSafeNormal2D());
				PawnOf(Pair[1])->TeleportTo(PawnOf(Solo)->GetActorLocation() + Right * 1500.0, PawnOf(Pair[1])->GetActorRotation());
				PlaceFacing(PawnOf(Pair[0]), PawnOf(Solo), 1200.0);
			});
		Hold(TEXT("settle"), 0.6);

		// 1 The whiff: P1 strikes the empty spot 150 cm in front of it.
		Network.Then(TEXT("1 P1 whiffs"), [this]()
		{
			Snapshot(0);
			AStealthPawn* P1 = PawnOf(Pair[0]);
			FAttackPayload Payload;
			Payload.WorldPoint = P1->GetActorLocation() + P1->GetActorForwardVector() * 150.0;
			AsOwner(P1, Seq, [&Payload](UPawnCommandComponent& Command) { Command.Attack(Payload); });
		});
		Hold(TEXT("1 past contact and the lockout"), 2.2);

		// 2 The reversal.
		Network.Then(TEXT("2a S braces, P1 in front"), [this]()
		{
			Snapshot(1);
			AsOwner(PawnOf(Solo), Seq, [](UPawnCommandComponent& Command) { Command.PrepareBegin(); });
			PlaceFacing(PawnOf(Pair[0]), PawnOf(Solo), 150.0);
		});
		Hold(TEXT("2a settle"), 0.4);
		Network.Then(TEXT("2b P1 strikes S"), [this]() { StrikeAt(PawnOf(Pair[0]), PawnOf(Solo), Seq); });
		Hold(TEXT("2 the reversal resolves"), 2.0, [this]()
		{
			const AStealthPawn* Defender = PawnOf(Solo);
			if (Defender != nullptr && Defender->GetActionState().Pose == EActionPose::Counter)
			{
				ReversalContact = Defender->GetActionState().ContactServerTime;
			}
			return ReversalContact > 0.0;
		});

		// 3 The kill of a player.
		Network.Then(TEXT("3a P2 in front of S"), [this]()
		{
			Snapshot(2);
			PlaceFacing(PawnOf(Pair[1]), PawnOf(Solo), 150.0);
		});
		Hold(TEXT("3a settle (S's counter ends)"), 1.4);
		Network.Then(TEXT("3b S strikes P2"), [this]() { StrikeAt(PawnOf(Solo), PawnOf(Pair[1]), Seq); });
		Hold(TEXT("3 the kill resolves"), 2.0, [this]()
		{
			const AStealthPawn* Victim = PawnOf(Pair[1]);
			if (Victim != nullptr && Victim->HasDeathWrite())
			{
				KillContact = Victim->GetDeathState().DeathServerTime;
			}
			return KillContact > 0.0;
		});

		Network.Then(TEXT("assert"), [this]()
		{
			Snapshot(3);
			const TArray<UWorld*> All = Worlds();
			const double Slack = 0.25; // one frame plus the result's travel in the 3-window PIE
			auto Check = [this, Slack](int32 Machine, int32 Step, ETransientGradeCause Cause, double Due, const TCHAR* What, bool bAnyCause = false)
			{
				const TArray<FTransientGradeCall> C = StepCalls(Machine, Step);
				const FTransientGradeCall* Hit = C.FindByPredicate([Cause, bAnyCause](const FTransientGradeCall& Call) { return bAnyCause || Call.Cause == Cause; });
				TestRunner->AddInfo(FString::Printf(TEXT("ASN23 step %d machine %d %s: %d call(s); at %.3f for due %.3f (contact %.3f), dilation %.2f"), Step, Machine, What, C.Num(),
					Hit != nullptr ? Hit->ServerNow : -1.0, Hit != nullptr ? Hit->DueServerTime : -1.0, Due, Hit != nullptr ? Hit->TimeDilation : -1.0f));
				return Hit != nullptr && C.Num() == 1 && FMath::IsNearlyEqual(Hit->DueServerTime, Due, 0.002) && Hit->ServerNow + 0.002 >= Due && Hit->ServerNow <= Due + Slack
					&& FMath::IsNearlyEqual(Hit->TimeDilation, 1.0f);
			};
			for (int32 Machine = 0; Machine < 3; ++Machine)
			{
				ASSERT_THAT(IsTrue(StepCalls(Machine, 0).IsEmpty(), *FString::Printf(TEXT("whiff: no grade on machine %d"), Machine)));
			}
			ASSERT_THAT(IsTrue(Check(MachineP1, 1, ETransientGradeCause::AttackerContact, ReversalContact, TEXT("reversed attacker (also the victim: one beat)"), /*bAnyCause*/ true), TEXT("reversal: the attacker's grade at its contact")));
			ASSERT_THAT(IsTrue(Check(MachineS, 1, ETransientGradeCause::DefenderCounter, ReversalContact, TEXT("bound defender")), TEXT("reversal: the defender's grade at its counter's contact")));
			ASSERT_THAT(IsTrue(StepCalls(MachineP2, 1).IsEmpty(), TEXT("reversal: no grade on the bystander's machine")));
			ASSERT_THAT(IsTrue(Check(MachineS, 2, ETransientGradeCause::AttackerContact, KillContact, TEXT("attacker")), TEXT("kill: the attacker's grade at its contact")));
			ASSERT_THAT(IsTrue(Check(MachineP2, 2, ETransientGradeCause::VictimDeath, KillContact, TEXT("victim")), TEXT("kill: the victim's grade at its DeathServerTime")));
			ASSERT_THAT(IsTrue(StepCalls(MachineP1, 2).IsEmpty(), TEXT("kill: no grade on the victim's ally's machine")));
			for (UWorld* World : All)
			{
				ASSERT_THAT(IsTrue(World->GetWorldSettings() != nullptr && FMath::IsNearlyEqual(World->GetWorldSettings()->TimeDilation, 1.0f), TEXT("no time dilation on any machine")));
			}
		});
	}
};

#endif
