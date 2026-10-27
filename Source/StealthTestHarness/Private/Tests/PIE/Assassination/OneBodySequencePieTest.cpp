// Assassination story asn-020 AC1 / AC2, AC-ASN-22 OneBodySequence (ADR-0008 T2; listen server + 2 clients on
// L_Prototype, NpcCount 0): StealthGame.Assassination.PIE.OneBodySequence.<method>. Three victims of one kind each through
// the production path (as PossessionThroughFall): an AI-driven NPC struck by the solo player S, a player (P2, reversed by
// S's brace) and the same participant again as an informant. Read from every machine's presentation log:
//   - each body's Death_Fall starts at its DeathServerTime, its pawn collision goes off at + DeathFallSec (1.2 s) and, on a
//     machine with a viewer, it is hidden by VA-ASN-2 between + 7.2 s (the linger's end) and + 9.2 s (the grace cut); the
//     fall start and collision-off offsets are identical across the three kinds (+- one frame);
//   - after the fall, a walking pawn's capsule swept through each body on every machine meets no blocking hit from it;
//   - no particle, Niagara or decal component exists on any body and its mesh materials never change, from the kill to the
//     linger's end. The retained captures (asn-22-*.png) are the [Visual] half.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "Components/SkeletalMeshComponent.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "Server/Core/StealthPawnFactory.h"
#include "Server/StealthPenaltyHarnessSeams.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/PresentationPieSupport.h"

namespace OneBodySequencePieData
{
	const TCHAR* Name(int32 Victim) { return Victim == 0 ? TEXT("NPC") : (Victim == 1 ? TEXT("player") : TEXT("informant")); }

	/** One victim: its copies per machine, its death time, the materials at the kill and the audits. */
	struct FBody
	{
		TArray<TWeakObjectPtr<AStealthPawn>> Copies;
		TArray<const AStealthPawn*> Keys;
		double DeathServerTime = -1.0;
		TArray<TArray<UMaterialInterface*>> MaterialsAtKill;
		bool bEffectComponent = false;
		bool bMaterialChanged = false;
		bool bBlocked = false;
		bool bSwept = false;
	};

	TArray<UMaterialInterface*> MaterialsOf(const AStealthPawn* Pawn)
	{
		TArray<UMaterialInterface*> Out;
		if (const USkeletalMeshComponent* Mesh = Pawn != nullptr ? Pawn->GetMesh() : nullptr)
		{
			for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
			{
				Out.Add(Mesh->GetMaterial(Slot));
			}
		}
		return Out;
	}

	bool HasEffectComponent(const AStealthPawn* Pawn)
	{
		TArray<UActorComponent*> Components;
		Pawn->GetComponents(Components);
		return Components.ContainsByPredicate([](const UActorComponent* C)
		{
			const FString Class = C->GetClass()->GetName();
			return Class.Contains(TEXT("Particle")) || Class.Contains(TEXT("Niagara")) || Class.Contains(TEXT("Decal"));
		});
	}
}

NETWORK_TEST_CLASS(OneBodySequence, "StealthGame.Assassination.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	TWeakObjectPtr<AStealthPlayerController> Solo;
	TWeakObjectPtr<AStealthPlayerController> Pair[2];
	TWeakObjectPtr<AStealthPawn> SoloPawn;
	OneBodySequencePieData::FBody Bodies[3];
	TWeakObjectPtr<AStealthPawn> PendingNpc;
	uint32 Seq = 0x32000000u;
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
		// NKP story nkp-014: a 0-NPC round has no crowd pawn to take over; the harness-only team-start respawn stands in.
		StealthPenaltyHarnessSeams::SetCrowdlessTeamStartRespawn(true);
	}

	AFTER_ALL()
	{
		StealthPenaltyHarnessSeams::SetCrowdlessTeamStartRespawn(false);
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

	/** Records Victim's copies on every machine (before the strike, while their positions agree). */
	void Watch(int32 Index, AStealthPawn* Victim)
	{
		OneBodySequencePieData::FBody& B = Bodies[Index];
		for (UWorld* World : StealthAssassinationPie::Worlds())
		{
			AStealthPawn* Copy = StealthPresentationPie::CopyOf(Victim, World);
			B.Copies.Add(Copy);
			B.Keys.Add(Copy);
			B.MaterialsAtKill.Add(OneBodySequencePieData::MaterialsOf(Copy));
		}
	}

	/** One frame's audit of every watched body on every machine. */
	void Audit()
	{
		using namespace OneBodySequencePieData;
		const TArray<UWorld*> All = StealthAssassinationPie::Worlds();
		for (FBody& B : Bodies)
		{
			for (int32 Machine = 0; Machine < B.Copies.Num() && Machine < B.MaterialsAtKill.Num() && Machine < All.Num(); ++Machine)
			{
				const AStealthPawn* Body = B.Copies[Machine].Get();
				if (Body == nullptr)
				{
					continue;
				}
				if (B.DeathServerTime < 0.0 && Body->HasDeathWrite())
				{
					B.DeathServerTime = Body->GetDeathState().DeathServerTime;
				}
				const double Now = StealthAssassinationPie::ServerNow(All[Machine]);
				const bool bInSequence = B.DeathServerTime >= 0.0 && Now < B.DeathServerTime + 7.2;
				if (!bInSequence)
				{
					continue;
				}
				B.bEffectComponent |= HasEffectComponent(Body);
				B.bMaterialChanged |= MaterialsOf(Body) != B.MaterialsAtKill[Machine];
				if (Now >= B.DeathServerTime + 1.3 && Now < B.DeathServerTime + 2.0)
				{
					// A walking pawn's capsule (the published body constants) swept through the body.
					const FVector Center = Body->GetActorLocation();
					const FVector Across = FVector::CrossProduct(FVector::UpVector, Body->GetActorForwardVector()).GetSafeNormal();
					FCollisionQueryParams Params(SCENE_QUERY_STAT(OneBodySequenceSweep), false);
					TArray<FHitResult> Hits;
					All[Machine]->SweepMultiByChannel(Hits, Center - Across * 150.0, Center + Across * 150.0, FQuat::Identity, ECC_Pawn,
						FCollisionShape::MakeCapsule(AStealthPawn::SampleCapsuleRadiusCm, AStealthPawn::SampleCapsuleHalfHeightCm), Params);
					B.bSwept = true;
					B.bBlocked |= Hits.ContainsByPredicate([Body](const FHitResult& Hit) { return Hit.bBlockingHit && Hit.GetActor() == Body; });
				}
			}
		}
	}

	void Hold(const TCHAR* Label, double Seconds, TFunction<bool()> Also = nullptr)
	{
		Network
			.Then(*FString::Printf(TEXT("%s: mark"), Label), [this]() { Mark = StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()); })
			.Until(Label, [this, Seconds, Also]()
			{
				Audit();
				return StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()) >= Mark + Seconds && (!Also || Also());
			});
	}

	TEST_METHOD(NpcPlayerAndInformantFallLingerAndGoAlike)
	{
		using namespace StealthAssassinationPie;
		using namespace OneBodySequencePieData;
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
				return SoloPawn.IsValid();
			})
			.Then(TEXT("place S clear of the bushes, the pair aside"), [this]()
			{
				MoveClearOfBushes(SoloPawn.Get());
				const FVector Right = FVector::CrossProduct(FVector::UpVector, SoloPawn->GetActorForwardVector().GetSafeNormal2D());
				PawnOf(Pair[0])->TeleportTo(SoloPawn->GetActorLocation() + Right * 1500.0, PawnOf(Pair[0])->GetActorRotation());
				PawnOf(Pair[1])->TeleportTo(SoloPawn->GetActorLocation() - Right * 1500.0, PawnOf(Pair[1])->GetActorRotation());
			});
		Hold(TEXT("settle"), 0.6);

		Network.Then(TEXT("1 spawn an AI-driven NPC and strike it"), [this]()
		{
			UStealthPawnFactory* Factory = UStealthPawnFactory::Get(ServerWorld());
			const FVector Forward = SoloPawn->GetActorForwardVector().GetSafeNormal2D();
			FStealthSpawnRequest Request;
			Request.Transform = FTransform((-Forward).Rotation(), SoloPawn->GetActorLocation() + Forward * 140.0);
			Request.ControlKind = EStealthPawnControlKind::Ai;
			const FStealthSpawnResult Result = Factory != nullptr ? Factory->SpawnSharedPawn(Request) : FStealthSpawnResult();
			if (AStealthPawn* Npc = Cast<AStealthPawn>(Result.Pawn))
			{
				StrikeAt(SoloPawn.Get(), Npc, Seq);
				PendingNpc = Npc; // watched on every machine once the clients have it
			}
		});
		Hold(TEXT("1 the NPC replicates"), 0.3);
		Network.Then(TEXT("1 watch the NPC on every machine"), [this]()
		{
			Watch(0, PendingNpc.Get());
		});
		Hold(TEXT("1 the NPC falls"), 2.4);

		Network.Then(TEXT("2a S braces, P2 in front"), [this]()
		{
			AsOwner(SoloPawn.Get(), Seq, [](UPawnCommandComponent& Command) { Command.PrepareBegin(); });
			PlaceFacing(PawnOf(Pair[1]), SoloPawn.Get(), 150.0);
		});
		Hold(TEXT("2a settle"), 0.4);
		Network.Then(TEXT("2b P2 strikes S and is reversed"), [this]()
		{
			Watch(1, PawnOf(Pair[1]));
			StrikeAt(PawnOf(Pair[1]), SoloPawn.Get(), Seq);
		});
		Hold(TEXT("2 the reversed attacker falls and respawns"), 3.0, [this]() { return PawnOf(Pair[1]) != nullptr && PawnOf(Pair[1]) != Bodies[1].Copies[0].Get(); });

		Network.Then(TEXT("3 S strikes the informant"), [this]()
		{
			PlaceFacing(PawnOf(Pair[1]), SoloPawn.Get(), 150.0);
		});
		Hold(TEXT("3 settle"), 0.4);
		Network.Then(TEXT("3 strike"), [this]()
		{
			Watch(2, PawnOf(Pair[1]));
			StrikeAt(SoloPawn.Get(), PawnOf(Pair[1]), Seq);
		});
		Hold(TEXT("3 every body runs its sequence to the grace cut"), 9.6, [this]() { return Bodies[2].DeathServerTime > 0.0; });

		Network.Then(TEXT("assert"), [this]()
		{
			const TArray<UWorld*> All = Worlds();
			const FEncounterPresentationParams Params = FEncounterPresentationParams::FromPublic(FAssassinationTuning_Public());
			double FallOffset[3][3] = {};
			double CollisionOffset[3][3] = {};
			double Frame = 0.0;
			for (UWorld* World : All)
			{
				Frame = FMath::Max(Frame, static_cast<double>(World->GetDeltaSeconds()));
			}
			Frame = FMath::Max(Frame, 1.0 / 60.0) + 0.003;
			for (int32 Index = 0; Index < 3; ++Index)
			{
				const FBody& B = Bodies[Index];
				ASSERT_THAT(IsTrue(B.DeathServerTime > 0.0 && B.Keys.Num() == All.Num(), *FString::Printf(TEXT("%s: killed and watched on every machine"), Name(Index))));
				for (int32 Machine = 0; Machine < All.Num(); ++Machine)
				{
					const FEncounterPresentationLogRow* Fall = StealthPresentationPie::FirstRow(All[Machine], B.Keys[Machine], EEncounterPresentationEvent::ClipStart, EEncounterClip::DeathFall);
					const FEncounterPresentationLogRow* Off = StealthPresentationPie::FirstRow(All[Machine], B.Keys[Machine], EEncounterPresentationEvent::CollisionOff);
					const FEncounterPresentationLogRow* Hidden = StealthPresentationPie::FirstRow(All[Machine], B.Keys[Machine], EEncounterPresentationEvent::BodyHidden);
					FallOffset[Index][Machine] = Fall != nullptr ? Fall->ServerNow - B.DeathServerTime : -1.0;
					CollisionOffset[Index][Machine] = Off != nullptr ? Off->ServerNow - B.DeathServerTime : -1.0;
					const double HiddenOffset = Hidden != nullptr ? Hidden->ServerNow - B.DeathServerTime : -1.0;
					TestRunner->AddInfo(FString::Printf(TEXT("ASN22 %s machine %d: fall +%.3f, collision off +%.3f, hidden +%.3f (decision %d)"), Name(Index), Machine,
						FallOffset[Index][Machine], CollisionOffset[Index][Machine], HiddenOffset, Hidden != nullptr ? static_cast<int32>(Hidden->Value) : -1));
					ASSERT_THAT(IsTrue(Fall != nullptr && FallOffset[Index][Machine] >= -0.001 && FallOffset[Index][Machine] <= Frame,
						*FString::Printf(TEXT("%s machine %d: Death_Fall starts at DeathServerTime"), Name(Index), Machine)));
					ASSERT_THAT(IsTrue(Off != nullptr && CollisionOffset[Index][Machine] >= Params.DeathFallSec - 0.001 && CollisionOffset[Index][Machine] <= Params.DeathFallSec + Frame,
						*FString::Printf(TEXT("%s machine %d: collision off at + DeathFallSec"), Name(Index), Machine)));
					if (Hidden != nullptr)
					{
						ASSERT_THAT(IsTrue(HiddenOffset >= Params.DeathFallSec + Params.BodyLingerSec - 0.001
							&& HiddenOffset <= Params.DeathFallSec + Params.BodyLingerSec + Params.BodyRemovalGraceSec + Frame,
							*FString::Printf(TEXT("%s machine %d: hidden inside the removal window (after the 6.0 s linger, by the grace cut)"), Name(Index), Machine)));
					}
				}
				ASSERT_THAT(IsTrue(B.bSwept && !B.bBlocked, *FString::Printf(TEXT("%s: blocks no walking pawn after the fall"), Name(Index))));
				ASSERT_THAT(IsFalse(B.bEffectComponent, *FString::Printf(TEXT("%s: no particle, Niagara or decal component"), Name(Index))));
				ASSERT_THAT(IsFalse(B.bMaterialChanged, *FString::Printf(TEXT("%s: no material change"), Name(Index))));
			}
			for (int32 Index = 1; Index < 3; ++Index)
			{
				for (int32 Machine = 0; Machine < All.Num(); ++Machine)
				{
					ASSERT_THAT(IsTrue(FMath::Abs(FallOffset[Index][Machine] - FallOffset[0][Machine]) <= Frame
						&& FMath::Abs(CollisionOffset[Index][Machine] - CollisionOffset[0][Machine]) <= Frame,
						*FString::Printf(TEXT("%s and NPC machine %d: identical fall and collision timings (+- one frame)"), Name(Index), Machine)));
				}
			}
		});
	}
};

#endif
