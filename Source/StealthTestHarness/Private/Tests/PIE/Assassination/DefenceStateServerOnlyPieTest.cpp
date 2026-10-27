// Assassination story asn-016, AC1 / AC-ASN-36 DefenceStateServerOnly, traffic half (ADR-0008 T2; listen server + 2
// clients on L_Prototype, NpcCount 0): StealthGame.Assassination.PIE.DefenceStateServerOnly.<method>. A remote client's
// player P braces through the production path (its own origin); after the replication settles, on every machine:
//   - P's own machine: its controller's UDefenceOwnerComponent holds CooldownEndServerTime = the server's stamp + 60 s
//     exactly (the only defence datum a client holds);
//   - every other machine: no controller there holds P's value (a PlayerController never reaches another connection) and
//     every UDefenceOwnerComponent it has is none (-1);
//   - no client world has any UDefenceStateComponent (T_act and LockoutEnd never reach a client), while the server's
//     P pawn has one with the stamp; the prototype bPreparingDefence no longer replicates (the brace is the FActionState
//     Brace pose, which every machine sees).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "Server/DefenceStateComponent.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/AssassinationPieSupport.h"
#include "UObject/UObjectIterator.h"
#include "View/DefenceOwnerComponent.h"
#include "View/StealthTuningPublicSubstructs.h"

NETWORK_TEST_CLASS(DefenceStateServerOnly, "StealthGame.Assassination.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	TWeakObjectPtr<AStealthPlayerController> Braced;
	TWeakObjectPtr<AStealthPawn> BracedPawn;
	uint32 Seq = 0x20000000u;
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

	TEST_METHOD(OnlyTheBracersOwnControllerHoldsItsCooldownEnd)
	{
		using namespace StealthAssassinationPie;
		Network
			.Until(TEXT("the round is Active with three players and a remote player found"), [this]()
			{
				if (!RoundActive())
				{
					return false;
				}
				for (AStealthPlayerController* Controller : ServerPlayers())
				{
					if (!Controller->IsLocalController() && OwnerMachineController(Controller) != nullptr)
					{
						Braced = Controller;
						BracedPawn = Cast<AStealthPawn>(Controller->GetPawn());
					}
				}
				return ServerPlayers().Num() == 3 && BracedPawn.IsValid();
			})
			.Then(TEXT("P braces with its own origin"), [this]()
			{
				MoveClearOfBushes(BracedPawn.Get());
				AsOwner(BracedPawn.Get(), Seq, [](UPawnCommandComponent& Command) { Command.PrepareBegin(); });
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("replication settles (1.5 s)"), [this]() { return ServerNow(ServerWorld()) >= Mark + 1.5; })
			.Then(TEXT("assert"), [this]()
			{
				const UDefenceStateComponent* ServerState = BracedPawn.IsValid() ? UDefenceStateComponent::Find(*BracedPawn) : nullptr;
				ASSERT_THAT(IsTrue(ServerState != nullptr && ServerState->GetCore().GetLastPrepareActivation().IsSet(), TEXT("the server's pawn holds the brace stamp")));
				const double Expected = ServerState->GetCore().GetCooldownEndServerTime(FAssassinationTuning_Public().DefenceCooldownSec).GetValue();
				const AStealthPlayerController* Own = OwnerMachineController(Braced.Get());
				ASSERT_THAT(IsTrue(Own != nullptr && Own->GetDefenceOwner() != nullptr && Own->GetDefenceOwner()->GetCooldownEndServerTime() == Expected,
					*FString::Printf(TEXT("P's own controller holds CooldownEndServerTime = stamp + 60 (%.3f, got %.3f)"), Expected,
						Own != nullptr && Own->GetDefenceOwner() != nullptr ? Own->GetDefenceOwner()->GetCooldownEndServerTime() : -2.0)));
				const int32 BracedId = Braced->PlayerState != nullptr ? Braced->PlayerState->GetPlayerId() : -1;
				for (UWorld* World : Worlds())
				{
					const bool bOwn = Own != nullptr && Own->GetWorld() == World;
					const bool bServer = World->GetNetMode() != NM_Client;
					int32 Components = 0;
					int32 Holding = 0;
					for (TObjectIterator<UDefenceOwnerComponent> It; It; ++It)
					{
						if (It->GetWorld() != World || It->IsTemplate())
						{
							continue;
						}
						++Components;
						const AController* Controller = Cast<AController>(It->GetOwner());
						const bool bIsP = Controller != nullptr && Controller->PlayerState != nullptr && Controller->PlayerState->GetPlayerId() == BracedId;
						Holding += It->GetCooldownEndServerTime() >= 0.0 ? 1 : 0;
						if (!bServer && !bOwn)
						{
							ASSERT_THAT(IsFalse(bIsP, TEXT("another client never holds P's controller")));
						}
					}
					int32 DefenceStates = 0;
					for (TObjectIterator<UDefenceStateComponent> It; It; ++It)
					{
						DefenceStates += It->GetWorld() == World && !It->IsTemplate() ? 1 : 0;
					}
					TestRunner->AddInfo(FString::Printf(TEXT("ASNPIE world %s: owner components %d, holding a cooldown %d, defence states %d"), *World->GetName(), Components, Holding,
						DefenceStates));
					if (World->GetNetMode() == NM_Client)
					{
						ASSERT_THAT(IsTrue(DefenceStates == 0, TEXT("no client world has any UDefenceStateComponent")));
						ASSERT_THAT(IsTrue(Holding == (bOwn ? 1 : 0), TEXT("only P's own machine holds a cooldown end; every other owner component is none")));
					}
					else
					{
						ASSERT_THAT(IsTrue(Holding == 1, TEXT("on the server only P's controller holds a value")));
					}
				}
			});
	}
};

#endif
