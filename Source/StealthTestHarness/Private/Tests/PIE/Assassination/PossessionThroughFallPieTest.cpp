// Assassination story asn-019, AC1 / AC-ASN-24 PossessionThroughFall and the possession half of AC2 (ADR-0018
// BodySequenceParity; the queue half is StealthGame.Assassination.BodySequenceParity) (ADR-0008 T2; listen server + 2
// clients on L_Prototype, NpcCount 0): StealthGame.Assassination.PIE.PossessionThroughFall.<method>. Three victims of
// one kind each, through the production path: an NPC (a factory pawn driven by its AI controller) struck by the solo
// player S; a player, P2, reversed by S's brace (ReversedAgainst); and the same participant again as an informant after
// its respawn, struck by S. Sampled on the server every frame from the kill:
//   - the victim's controller keeps the dying pawn until DeathServerTime + DeathFallSec (no ownership change and no
//     respawn takeover before), and lets it go at that time (+ one server frame);
//   - the player's PlayerController is never in the Spectating state, from the kill through the release; the informant
//     respawns only after the release (a new pawn, never the body);
//   - release and removal times (the encounter core's records) match across the three kinds within one server frame, and
//     every body is removed at DeathServerTime + 9.2 s.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "Server/Core/StealthPawnFactory.h"
#include "Server/EncounterService.h"
#include "Server/Queue/StealthResolutionQueueCore.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/AssassinationPieSupport.h"

namespace PossessionThroughFallPieData
{
	enum class EVictim : uint8 { Npc, Player, Informant, Count };
	const TCHAR* Name(int32 Victim) { return Victim == 0 ? TEXT("NPC") : (Victim == 1 ? TEXT("player") : TEXT("informant")); }

	/** What the per-frame sampler saw of one victim. */
	struct FWatch
	{
		TWeakObjectPtr<APawn> Body;
		/** The body as an opaque key (the core's records compare pointers; never dereferenced once the body is gone). */
		const APawn* BodyKey = nullptr;
		TWeakObjectPtr<AController> Controller;
		bool bPlayer = false;
		double DeathServerTime = -1.0;
		/** Latest sample with the controller still holding the body, first sample without. */
		double LastHeld = -1.0;
		double FirstReleased = -1.0;
		/** A sample before the release time where the controller had changed (an early ownership change). */
		bool bEarlyChange = false;
		bool bSpectating = false;
		/** The controller possessed another pawn before the release (an early takeover). */
		bool bEarlyTakeover = false;
		bool bRespawnedAfterRelease = false;
		double MaxFrameSec = 0.0;
		double LastSample = -1.0;
	};

	const FEncounterBody* BodyRecord(const APawn* Body)
	{
		const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(StealthAssassinationPie::ServerWorld());
		const UEncounterService* Service = Root != nullptr ? Root->GetEncounterService() : nullptr;
		return Service != nullptr && Service->GetCore() != nullptr ? Service->GetCore()->FindBody(Body) : nullptr;
	}
}

NETWORK_TEST_CLASS(PossessionThroughFall, "StealthGame.Assassination.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	TWeakObjectPtr<AStealthPlayerController> Solo;
	TWeakObjectPtr<AStealthPlayerController> Pair[2];
	TWeakObjectPtr<AStealthPawn> SoloPawn;
	PossessionThroughFallPieData::FWatch Watches[3];
	/** Copies of the core's body records, taken while the bodies are still known. */
	FEncounterBody Records[3];
	uint32 Seq = 0x30000000u;
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

	/** Starts watching Body (its controller now) for victim Index. */
	void Watch(int32 Index, APawn* Body)
	{
		PossessionThroughFallPieData::FWatch& W = Watches[Index];
		W.Body = Body;
		W.BodyKey = Body;
		W.Controller = Body != nullptr ? Body->GetController() : nullptr;
		W.bPlayer = Cast<APlayerController>(W.Controller.Get()) != nullptr;
	}

	/** One server-frame sample of every watched victim. */
	void Sample()
	{
		const double Now = StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld());
		const double Fall = FEncounterParams::Defaults().BodyReleaseDelaySec();
		for (int32 Index = 0; Index < 3; ++Index)
		{
			PossessionThroughFallPieData::FWatch& W = Watches[Index];
			if (W.BodyKey == nullptr)
			{
				continue;
			}
			if (const FEncounterBody* Record = PossessionThroughFallPieData::BodyRecord(W.BodyKey))
			{
				Records[Index] = *Record;
				W.DeathServerTime = Record->DeathServerTime;
			}
			if (W.LastSample >= 0.0)
			{
				W.MaxFrameSec = FMath::Max(W.MaxFrameSec, Now - W.LastSample);
			}
			W.LastSample = Now;
			AController* Controller = W.Controller.Get();
			APawn* Body = W.Body.Get();
			const bool bHeld = Controller != nullptr && Body != nullptr && Controller->GetPawn() == Body && Body->GetController() == Controller;
			const bool bBeforeRelease = W.DeathServerTime < 0.0 || Now < W.DeathServerTime + Fall - FResolutionQueueCore::DueEpsilonSec;
			if (bHeld)
			{
				W.LastHeld = Now;
			}
			else if (W.FirstReleased < 0.0 && W.DeathServerTime >= 0.0)
			{
				W.FirstReleased = Now;
			}
			if (!bHeld && bBeforeRelease)
			{
				W.bEarlyChange = true;
				W.bEarlyTakeover |= Controller != nullptr && Controller->GetPawn() != nullptr && Controller->GetPawn() != Body;
			}
			if (W.bPlayer && Controller != nullptr)
			{
				W.bSpectating |= Controller->GetStateName() == NAME_Spectating && W.FirstReleased < 0.0;
				W.bRespawnedAfterRelease |= W.FirstReleased >= 0.0 && Controller->GetPawn() != nullptr && Controller->GetPawn() != Body;
			}
		}
	}

	void Hold(const TCHAR* Label, double Seconds, TFunction<bool()> Also = nullptr)
	{
		Network
			.Then(*FString::Printf(TEXT("%s: mark"), Label), [this]() { Mark = StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()); })
			.Until(Label, [this, Seconds, Also]()
			{
				Sample();
				return StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()) >= Mark + Seconds && (!Also || Also());
			});
	}

	TEST_METHOD(KilledBodiesKeepTheirControllerUntilReleaseAlike)
	{
		using namespace StealthAssassinationPie;
		using namespace PossessionThroughFallPieData;
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

		// 1 The NPC: a factory pawn possessed by its AI controller, struck by S.
		Network.Then(TEXT("1 spawn an AI-driven NPC and strike it"), [this]()
		{
			UStealthPawnFactory* Factory = UStealthPawnFactory::Get(ServerWorld());
			const FVector Forward = SoloPawn->GetActorForwardVector().GetSafeNormal2D();
			FStealthSpawnRequest Request;
			Request.Transform = FTransform((-Forward).Rotation(), SoloPawn->GetActorLocation() + Forward * 140.0);
			Request.ControlKind = EStealthPawnControlKind::Ai;
			const FStealthSpawnResult Result = Factory != nullptr ? Factory->SpawnSharedPawn(Request) : FStealthSpawnResult();
			TestRunner->AddInfo(FString::Printf(TEXT("ASNPIE NPC %s controller %s"), Result.Pawn ? *Result.Pawn->GetName() : TEXT("none"),
				Result.Pawn && Result.Pawn->GetController() ? *Result.Pawn->GetController()->GetName() : TEXT("none")));
			Watch(0, Result.Pawn);
			if (Result.Pawn != nullptr)
			{
				StrikeAt(SoloPawn.Get(), Result.Pawn, Seq);
			}
		});
		Hold(TEXT("1 the NPC falls and is released"), 2.4);

		// 2 The player: S braces, P2 strikes S and is reversed.
		Network.Then(TEXT("2a S braces, P2 in front"), [this]()
		{
			AsOwner(SoloPawn.Get(), Seq, [](UPawnCommandComponent& Command) { Command.PrepareBegin(); });
			PlaceFacing(PawnOf(Pair[1]), SoloPawn.Get(), 150.0);
		});
		Hold(TEXT("2a settle"), 0.4);
		Network.Then(TEXT("2b P2 strikes S"), [this]()
		{
			Watch(1, PawnOf(Pair[1]));
			StrikeAt(PawnOf(Pair[1]), SoloPawn.Get(), Seq);
		});
		Hold(TEXT("2 the reversed attacker falls, is released and respawns"), 3.0, [this]() { return PawnOf(Pair[1]) != nullptr && PawnOf(Pair[1]) != Watches[1].Body.Get(); });

		// 3 The informant: P2's new pawn, struck by S.
		Network.Then(TEXT("3 S strikes the informant"), [this]()
		{
			PlaceFacing(PawnOf(Pair[1]), SoloPawn.Get(), 150.0);
			Watch(2, PawnOf(Pair[1]));
			StrikeAt(SoloPawn.Get(), PawnOf(Pair[1]), Seq);
		});
		Hold(TEXT("3 the informant falls, is released, respawns, and every body is removed"), 10.0, [this]()
		{
			return Records[2].IsRemoved() && Records[1].IsRemoved() && Records[0].IsRemoved() && Watches[2].bRespawnedAfterRelease;
		});

		Network.Then(TEXT("assert"), [this]()
		{
			const FEncounterParams Params = FEncounterParams::Defaults();
			for (int32 Index = 0; Index < 3; ++Index)
			{
				const FWatch& W = Watches[Index];
				const FEncounterBody& R = Records[Index];
				TestRunner->AddInfo(FString::Printf(TEXT("ASNPIE %s: death %.3f lastHeld %.3f firstReleased %.3f released %.3f removed %.3f maxFrame %.3f early %d spectating %d respawned %d"),
					Name(Index), W.DeathServerTime, W.LastHeld, W.FirstReleased, R.ReleasedAtServerTime, R.RemovedAtServerTime, W.MaxFrameSec, W.bEarlyChange ? 1 : 0,
					W.bSpectating ? 1 : 0, W.bRespawnedAfterRelease ? 1 : 0));
				const double Tolerance = W.MaxFrameSec + 0.002;
				ASSERT_THAT(IsTrue(W.DeathServerTime > 0.0 && R.Pawn != nullptr, *FString::Printf(TEXT("%s: the kill made a body"), Name(Index))));
				ASSERT_THAT(IsFalse(W.bEarlyChange, *FString::Printf(TEXT("%s: the controller kept the dying pawn until DeathServerTime + DeathFallSec"), Name(Index))));
				ASSERT_THAT(IsFalse(W.bEarlyTakeover, *FString::Printf(TEXT("%s: no takeover before the release"), Name(Index))));
				ASSERT_THAT(IsTrue(W.FirstReleased >= W.DeathServerTime + Params.BodyReleaseDelaySec() - 0.002 && W.FirstReleased <= W.DeathServerTime + Params.BodyReleaseDelaySec() + Tolerance,
					*FString::Printf(TEXT("%s: released at DeathServerTime + DeathFallSec (+ one frame)"), Name(Index))));
				ASSERT_THAT(IsTrue(R.IsRemoved() && R.RemovedAtServerTime >= W.DeathServerTime + Params.BodyRemovalDelaySec() - 0.002
					&& R.RemovedAtServerTime <= W.DeathServerTime + Params.BodyRemovalDelaySec() + Tolerance, *FString::Printf(TEXT("%s: removed at DeathServerTime + 9.2 s"), Name(Index))));
				ASSERT_THAT(IsFalse(W.Body.IsValid(), *FString::Printf(TEXT("%s: the body is gone after its removal"), Name(Index))));
				if (W.bPlayer)
				{
					ASSERT_THAT(IsFalse(W.bSpectating, *FString::Printf(TEXT("%s: the PlayerController never entered Spectating"), Name(Index))));
					ASSERT_THAT(IsTrue(W.bRespawnedAfterRelease, *FString::Printf(TEXT("%s: the informant respawned after the release (a new pawn)"), Name(Index))));
				}
			}
			ASSERT_THAT(IsTrue(Watches[1].bPlayer && Watches[2].bPlayer && !Watches[0].bPlayer, TEXT("victims: an AI-driven NPC and two player-controlled bodies")));
			const double Frame = FMath::Max3(Watches[0].MaxFrameSec, Watches[1].MaxFrameSec, Watches[2].MaxFrameSec) + 0.002;
			for (int32 Index = 1; Index < 3; ++Index)
			{
				const double Release = Records[Index].ReleasedAtServerTime - Records[Index].DeathServerTime;
				const double Removal = Records[Index].RemovedAtServerTime - Records[Index].DeathServerTime;
				ASSERT_THAT(IsTrue(FMath::Abs(Release - (Records[0].ReleasedAtServerTime - Records[0].DeathServerTime)) <= Frame
					&& FMath::Abs(Removal - (Records[0].RemovedAtServerTime - Records[0].DeathServerTime)) <= Frame,
					*FString::Printf(TEXT("%s and NPC: identical release and removal times (+- one server frame)"), Name(Index))));
			}
		});
	}
};

#endif
