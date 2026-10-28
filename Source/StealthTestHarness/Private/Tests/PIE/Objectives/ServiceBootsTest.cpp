// Objectives Chain story oc-005 (production/epics/objectives-chain/story-005-service-shell-and-composition-root.md), AC4:
// StealthGame.Objectives.PIE.ServiceBoots.<method> (ADR-0008 T2; listen server + 2 clients on L_Prototype, NpcCount 0). With
// the production composition root, once the round reaches Active: the server world has a bound UObjectivesService whose core
// is Active with both teams at Stage1; its three registrations are live (the action hub's Interact predicate and
// interact-target owner, and the TimerFired(StageSignal) resolver on Round's queue); the composition root's fault sink is
// empty; and no client world has an Objectives service (the service exists on the server only).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "Server/ActionHub.h"
#include "Server/ObjectivesService.h"
#include "Server/StealthCompositionRoot.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/AssassinationPieSupport.h"

NETWORK_TEST_CLASS(ServiceBoots, "StealthGame.Objectives.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

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

	TEST_METHOD(ServerOnlyServiceRegisteredWithNoFaultAtActive)
	{
		using namespace StealthAssassinationPie;
		Network
			.Until(TEXT("the round is Active"), []() { return RoundActive(); })
			.Then(TEXT("assert the service, its registrations and the fault sink"), [this]()
			{
				UWorld* Server = ServerWorld();
				ASSERT_THAT(IsNotNull(Server, TEXT("a listen-server world")));
				const UObjectivesService* Service = UObjectivesService::Get(Server);
				ASSERT_THAT(IsNotNull(Service, TEXT("the server world has an Objectives service")));
				ASSERT_THAT(IsTrue(Service->IsBound(), TEXT("the service is bound by the composition root")));
				ASSERT_THAT(IsTrue(Service->GetSelectedChain() != nullptr, TEXT("the Cacao chain was selected at Preparing")));
				ASSERT_THAT(IsTrue(Service->GetObjectiveTeamStage(0) == EObjectiveTeamStage::Stage1 && Service->GetObjectiveTeamStage(1) == EObjectiveTeamStage::Stage1,
					TEXT("both teams are at Stage1 after Active entry")));
				const UActionHub* Hub = UActionHub::Get(Server);
				ASSERT_THAT(IsNotNull(Hub, TEXT("the server's action hub")));
				ASSERT_THAT(IsTrue(Hub->HasPredicate(EActionType::Interact), TEXT("registered as the Interact predicate")));
				ASSERT_THAT(IsTrue(Hub->HasInteractTargetOwner(), TEXT("registered as the interact-target owner")));
				ASSERT_THAT(IsTrue(Service->IsStageSignalRegistered(), TEXT("registered as the TimerFired(StageSignal) resolver")));
				const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(Server);
				ASSERT_THAT(IsNotNull(Root, TEXT("the composition root")));
				TestRunner->AddInfo(FString::Printf(TEXT("OC005 ServiceBoots faults=%d"), Root->GetCommandFaultCount()));
				ASSERT_THAT(IsTrue(Root->GetCommandFaultCount() == 0, TEXT("the fault sink is empty at Active")));
				for (UWorld* World : Worlds())
				{
					if (World != Server)
					{
						ASSERT_THAT(IsTrue(UObjectivesService::Get(World) == nullptr, TEXT("no client world creates an Objectives service")));
					}
				}
			});
	}
};

#endif
