// Assassination story asn-021 AC2 / ADR-0018 RoleChangeGate (ADR-0008 T2; listen server + 2 clients on L_Prototype,
// NpcCount 0): StealthGame.Assassination.PIE.RoleChangeGate.<method>. A struck player on a remote client and one on the
// listen host (the host has no RepNotify for its own writes: the direct Refresh path, E7):
//   - OnRoleChangePresented fires on the victim's own machine exactly once for its FDeathState, never before its
//     DeathServerTime (and within one frame of it), with reason Assassinated; on no other machine;
//   - the own-role message (the HUD removal card stand-in, an OnRoleChangePresented subscriber) is not on the victim's
//     screen before the dispatch: the server no longer sends it at the death write;
//   - the Roster-mirror informant state may arrive before DeathServerTime: the dispatch never follows it.
// The FDeathState-arrives-300-ms-late case and the repeated-Refresh idempotence are StealthGame.Assassination.RoleChangeDispatchOnce.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/PresentationPieSupport.h"

NETWORK_TEST_CLASS(RoleChangeGate, "StealthGame.Assassination.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	/** Victim 0 and 1 (one is the host's player), and who strikes each. */
	TWeakObjectPtr<AStealthPlayerController> Victims[2];
	TWeakObjectPtr<AStealthPlayerController> Attackers[2];
	int32 Machines[2] = { INDEX_NONE, INDEX_NONE };
	TWeakObjectPtr<AStealthPawn> Bodies[2];
	double DeathTimes[2] = { -1.0, -1.0 };
	/** Role changes per machine before the run. */
	int32 Before[3] = { 0, 0, 0 };
	/** The victim's local state was informant before its DeathServerTime (the Roster mirror arrived early). */
	bool bInformantBeforeDeath[2] = { false, false };
	bool bMessageBeforeDispatch[2] = { false, false };
	uint32 Seq = 0x32110000u;
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

	int32 RoleChanges(int32 Machine) const
	{
		const TArray<UWorld*> All = StealthAssassinationPie::Worlds();
		const UEncounterViewSubsystem* View = All.IsValidIndex(Machine) ? StealthPresentationPie::ViewOf(All[Machine]) : nullptr;
		return View != nullptr ? View->GetRoleChanges().Num() : 0;
	}

	/** Per frame while victim Index dies: its local informant state and its own-role message against its DeathServerTime. */
	void Sample(int32 Index)
	{
		const TArray<UWorld*> All = StealthAssassinationPie::Worlds();
		if (Machines[Index] == INDEX_NONE || !Bodies[Index].IsValid())
		{
			return;
		}
		UWorld* World = All[Machines[Index]];
		const AStealthPlayerController* Local = StealthAssassinationPie::OwnerMachineController(Victims[Index].Get());
		const AStealthPawn* Copy = StealthPresentationPie::CopyOf(Bodies[Index].Get(), World);
		const double Now = StealthAssassinationPie::ServerNow(World);
		if (Copy != nullptr && Copy->HasDeathWrite())
		{
			DeathTimes[Index] = Copy->GetDeathState().DeathServerTime;
		}
		const bool bBefore = DeathTimes[Index] < 0.0 || Now < DeathTimes[Index];
		const AStealthPlayerState* State = Local != nullptr ? Local->GetPlayerState<AStealthPlayerState>() : nullptr;
		bInformantBeforeDeath[Index] |= bBefore && State != nullptr && State->IsInformant();
		if (bBefore && Local != nullptr)
		{
			bMessageBeforeDispatch[Index] |= Local->GetMessages().ContainsByPredicate([](const FStealthHudMessage& M) { return M.Text.ToString().StartsWith(TEXT("You have been eliminated")); });
		}
	}

	void Kill(int32 Index, const TCHAR* Label)
	{
		Network
			.Then(*FString::Printf(TEXT("%s: place"), Label), [this, Index]()
			{
				StealthAssassinationPie::PlaceFacing(PawnOf(Attackers[Index]), PawnOf(Victims[Index]), 150.0);
				Bodies[Index] = PawnOf(Victims[Index]);
			})
			.Then(*FString::Printf(TEXT("%s: settle mark"), Label), [this]() { Mark = StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()); })
			.Until(*FString::Printf(TEXT("%s: settle"), Label), [this]() { return StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()) >= Mark + 0.5; })
			.Then(*FString::Printf(TEXT("%s: strike"), Label), [this, Index]()
			{
				Mark = StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld());
				StealthAssassinationPie::StrikeAt(PawnOf(Attackers[Index]), PawnOf(Victims[Index]), Seq);
			})
			.Until(*FString::Printf(TEXT("%s: sample to death + 1 s"), Label), [this, Index]()
			{
				Sample(Index);
				const double Now = StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld());
				return (DeathTimes[Index] > 0.0 && Now >= DeathTimes[Index] + 1.0) || Now > Mark + 4.0;
			});
	}

	TEST_METHOD(OwnRoleDispatchFiresOnceAtDeathOnRemoteClientAndListenHost)
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
				// Two victims, one of them the host's player, ordered so the round lasts to the second kill.
				if (S->IsLocalController())
				{
					Victims[0] = P[0]; Attackers[0] = S;     // a remote client struck by the host
					Victims[1] = S;    Attackers[1] = P[1];  // the host struck last (its team's last player)
				}
				else
				{
					AStealthPlayerController* Host = P[0]->IsLocalController() ? P[0] : P[1];
					AStealthPlayerController* Remote = Host == P[0] ? P[1] : P[0];
					Victims[0] = Host;   Attackers[0] = S;   // the host struck first (its mate remains)
					Victims[1] = Remote; Attackers[1] = S;   // then the remote client
				}
				Machines[0] = StealthPresentationPie::MachineOf(Victims[0].Get());
				Machines[1] = StealthPresentationPie::MachineOf(Victims[1].Get());
				for (int32 Machine = 0; Machine < 3; ++Machine)
				{
					Before[Machine] = RoleChanges(Machine);
				}
				return Machines[0] != INDEX_NONE && Machines[1] != INDEX_NONE;
			})
			.Then(TEXT("move the first victim clear of the bushes"), [this]() { MoveClearOfBushes(PawnOf(Victims[0])); })
			.Then(TEXT("settle mark"), [this]() { Mark = ServerNow(ServerWorld()); })
			.Until(TEXT("settle"), [this]() { return ServerNow(ServerWorld()) >= Mark + 0.5; });
		Kill(0, TEXT("1"));
		Network
			.Then(TEXT("lockout mark"), [this]() { Mark = ServerNow(ServerWorld()); })
			.Until(TEXT("the attacker's lockout ends"), [this]() { return ServerNow(ServerWorld()) >= Mark + 1.0; })
			.Then(TEXT("move the second victim clear of the bushes"), [this]() { MoveClearOfBushes(PawnOf(Victims[1])); });
		Kill(1, TEXT("2"));

		Network.Then(TEXT("assert"), [this]()
		{
			const TArray<UWorld*> All = Worlds();
			int32 Expected[3] = { 0, 0, 0 };
			for (int32 Index = 0; Index < 2; ++Index)
			{
				++Expected[Machines[Index]];
			}
			for (int32 Index = 0; Index < 2; ++Index)
			{
				const UEncounterViewSubsystem* View = StealthPresentationPie::ViewOf(All[Machines[Index]]);
				const FRoleChangeRecord* Record = nullptr;
				int32 ForThisDeath = 0;
				if (View != nullptr)
				{
					for (const FRoleChangeRecord& R : View->GetRoleChanges())
					{
						if (FMath::IsNearlyEqual(R.DeathServerTime, DeathTimes[Index], 1e-6))
						{
							Record = Record != nullptr ? Record : &R;
							++ForThisDeath;
						}
					}
				}
				const double Frame = FMath::Max(1.0 / 60.0, static_cast<double>(All[Machines[Index]]->GetDeltaSeconds())) + 0.003;
				TestRunner->AddInfo(FString::Printf(TEXT("RCG victim %d (%s, machine %d): death %.3f, dispatch %.3f reason %s, count %d, informant before death %d, message before dispatch %d"),
					Index, Machines[Index] == 0 ? TEXT("listen host") : TEXT("remote client"), Machines[Index], DeathTimes[Index], Record != nullptr ? Record->ServerNow : -1.0,
					Record != nullptr ? LexRoleChangeReason(Record->Reason) : TEXT("-"), ForThisDeath, bInformantBeforeDeath[Index] ? 1 : 0, bMessageBeforeDispatch[Index] ? 1 : 0));
				ASSERT_THAT(IsTrue(DeathTimes[Index] > 0.0, *FString::Printf(TEXT("victim %d was killed"), Index)));
				ASSERT_THAT(IsTrue(ForThisDeath == 1, *FString::Printf(TEXT("victim %d: exactly one dispatch for its FDeathState"), Index)));
				ASSERT_THAT(IsTrue(Record->ServerNow + 0.001 >= DeathTimes[Index] && Record->ServerNow <= DeathTimes[Index] + Frame,
					*FString::Printf(TEXT("victim %d: dispatched at its DeathServerTime, never earlier"), Index)));
				ASSERT_THAT(IsTrue(Record->Reason == ERoleChangeReason::Assassinated, *FString::Printf(TEXT("victim %d: reason Assassinated"), Index)));
				ASSERT_THAT(IsFalse(bMessageBeforeDispatch[Index], *FString::Printf(TEXT("victim %d: no own-role message before the dispatch"), Index)));
			}
			for (int32 Machine = 0; Machine < 3; ++Machine)
			{
				ASSERT_THAT(IsTrue(RoleChanges(Machine) - Before[Machine] == Expected[Machine], *FString::Printf(TEXT("machine %d: dispatches only for its own pawn"), Machine)));
			}
			ASSERT_THAT(IsTrue(Machines[0] == 0 || Machines[1] == 0, TEXT("one victim was the listen host's player")));
		});
	}
};

#endif
