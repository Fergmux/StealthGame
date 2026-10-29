// Concealment story con-002 (production/epics/concealment-bushes/story-002-bush-runtime-from-manifest.md), AC1 (GDD
// AC-CON-22, LSC AC-52 stand-in) and AC3 on every machine: in a 2-client PIE each machine built UBushRuntimeSubsystem from
// the (stand-in) Manifest's BushSite records, and every bush's {BushId, HideVolume 2.0 x 2.0 x 2.25 m, EntryAxis set,
// Anchor} hashes identically on the server and both clients; each machine holds exactly one local, non-replicated
// ABushVolumeSet and no replicated concealment actor. Negative control: a 1 cm anchor perturbation of one client's copy
// changes its hash.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "StealthGameMode.h"
#include "Tests/PIE/Assassination/AssassinationPieSupport.h"
#include "View/BushRuntimeSubsystem.h"
#include "View/BushVolumeSet.h"

NETWORK_TEST_CLASS(BushSiteParity, "StealthGame.Concealment.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousTimeout;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousTimeout = Timeout->GetString();
			Timeout->Set(StealthAssassinationPie::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
		}
	}

	AFTER_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName); Timeout && !PreviousTimeout.IsEmpty())
		{
			Timeout->Set(*PreviousTimeout, ECVF_SetByCode);
		}
	}

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FBasePIENetworkComponentState>()
			.AsListenServer()
			.WithClients(2)
			.WithGameMode(AStealthGameMode::StaticClass())
			.Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
	}

	TEST_METHOD(EveryMachineBuildsTheSameBushes)
	{
		using namespace StealthAssassinationPie;
		Network
			.Until(TEXT("the round's layout is built on the server and both clients"), []()
			{
				const TArray<UWorld*> All = Worlds();
				if (All.Num() != 3 || !RoundActive())
				{
					return false;
				}
				for (UWorld* World : All)
				{
					const UBushRuntimeSubsystem* Bushes = UBushRuntimeSubsystem::Get(World);
					if (Bushes == nullptr || !Bushes->IsBuilt() || Bushes->GetBushes().Num() == 0)
					{
						return false;
					}
				}
				return true;
			})
			.Then(TEXT("hash every bush on every machine"), [this]()
			{
				const TArray<UWorld*> All = Worlds();
				const TArray<FBushRuntime>& ServerBushes = UBushRuntimeSubsystem::Get(All[0])->GetBushes();
				ASSERT_THAT(IsTrue(ServerBushes.Num() >= 1, TEXT("at least one bush")));
				for (int32 Machine = 1; Machine < All.Num(); ++Machine)
				{
					const TArray<FBushRuntime>& Copy = UBushRuntimeSubsystem::Get(All[Machine])->GetBushes();
					ASSERT_THAT(AreEqual(Copy.Num(), ServerBushes.Num(), TEXT("the same bush count on every machine")));
					for (int32 Index = 0; Index < Copy.Num(); ++Index)
					{
						ASSERT_THAT(AreEqual(ConcealmentGeometry::HashBushRuntime(Copy[Index]), ConcealmentGeometry::HashBushRuntime(ServerBushes[Index]),
							*FString::Printf(TEXT("bush %d hashes identically on machine %d"), Copy[Index].Id, Machine)));
						ASSERT_THAT(IsNear(Copy[Index].HideVolume.GetSize().Z, 225.0, 0.01, TEXT("hide volume 2.25 m")));
					}
				}
				// Negative control: a 1 cm anchor perturbation of one client's bush changes its hash.
				FBushRuntime Perturbed = UBushRuntimeSubsystem::Get(All[1])->GetBushes()[0];
				Perturbed = ConcealmentGeometry::MakeBushRuntime(Perturbed.Id, Perturbed.Anchor + FVector(1.0, 0.0, 0.0), Perturbed.EntryAxisMask);
				ASSERT_THAT(IsTrue(ConcealmentGeometry::HashBushRuntime(Perturbed) != ConcealmentGeometry::HashBushRuntime(ServerBushes[0]), TEXT("control: a 1 cm anchor change is detected")));
				// One local, non-replicated volume set per machine; no replicated concealment actor anywhere.
				for (UWorld* World : All)
				{
					int32 Sets = 0;
					for (TActorIterator<ABushVolumeSet> It(World); It; ++It)
					{
						if (IsValid(*It) && !It->IsActorBeingDestroyed())
						{
							++Sets;
							ASSERT_THAT(IsFalse(It->GetIsReplicated(), TEXT("the volume set never replicates")));
						}
					}
					ASSERT_THAT(AreEqual(Sets, 1, TEXT("exactly one ABushVolumeSet per machine")));
				}
			});
	}
};

#endif
