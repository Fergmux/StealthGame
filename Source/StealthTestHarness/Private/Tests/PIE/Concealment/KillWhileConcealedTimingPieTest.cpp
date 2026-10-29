// Concealment story con-033 AC2 (production/epics/concealment-bushes/story-033-blind-strike-parity-and-kill-timing.md;
// GDD AC-CON-26; ADR-0017 sections 7 and 8 B, ADR-0018 death write): StealthGame.Concealment.PIE.KillWhileConcealedTiming.
// Listen host + 2 clients on L_Prototype, NpcCount 0, the ADR-0008 LeakNetProfile (StealthLeak1) on every driver. The
// attacker S (the solo player) blind-strikes two bushes in turn: one holding a concealed NPC (an unbound Shared Pawn), then
// one holding the concealed rival V (a pair member; its teammate T watches). For each victim:
//   - the victim leaves the occupancy record in the execution slot (the first server frame after the strike);
//   - no opponent machine holds an actor for the victim before ContactServerTime - StrikeNetLeadSec (+- one tick): the
//     re-open is the death write's;
//   - the re-opened actor carries bDiedConcealed = true on arrival;
//   - the body is never drawn on any non-owner machine (the ally included) through the fall, the linger and the removal;
//   - the NPC run and the player run produce the same replicated fields and values on the victim's copies (kind-blind):
//     {pose, bDiedConcealed, DeathServerTime - ContactServerTime}.
//   NpcAndRivalReopenOnlyAtTheDeathWriteNeverDrawn

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "HAL/IConsoleManager.h"
#include "Server/StealthTuningSubstructs.h"
#include "Server/Visibility/StealthConcealmentCull.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/OwnPresentationPieSupport.h"
#include "Tests/PIE/Assassination/PresentationPieSupport.h"
#include "Tests/PIE/Concealment/ConcealmentPieSupport.h"

namespace KillWhileConcealedPie
{
	const TCHAR* const LeakNetProfile = TEXT("StealthLeak1");
	constexpr double WatchSec = 10.5;

	struct FRun
	{
		TWeakObjectPtr<AStealthPawn> Victim;
		int32 OwnerMachine = INDEX_NONE;
		uint16 BushId = 0;
		double Contact = -1.0;
		bool bOccupancyLeftInSlot = false;
		bool bSlotChecked = false;
		double EarliestOpponentCopy = -1.0;
		/** Server time the first opponent connection held an actor channel for the victim after the strike (the send side). */
		double EarliestOpponentChannel = -1.0;
		bool bOpponentCopyAtStrike = false;
		bool bArrivedDiedConcealed = true;
		bool bAnyArrival = false;
		bool bDrawn = false;
		FString Fields;
		/** A remote client is an opponent of the victim (the solo attacker may be the listen host). */
		bool bRemoteOpponent = false;
	};
}

NETWORK_TEST_CLASS(KillWhileConcealedTiming, "StealthGame.Concealment.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousTimeout;
	inline static int32 PreviousNpcCount = -1;
	FPacketSimulationSettings PacketSettings;

	AStealthPlayerController* Attacker = nullptr;
	AStealthPlayerController* RivalV = nullptr;
	AStealthPlayerController* Teammate = nullptr;
	FBushRuntime Bushes[2];
	KillWhileConcealedPie::FRun Runs[2];
	StealthOwnPresentationPie::FFrameTracker Frames;
	bool bSeen[2][3] = {};
	int32 Current = 0;
	uint32 Seq = 0x33200000u;
	double Mark = 0.0;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousTimeout = Timeout->GetString();
			Timeout->Set(TEXT("600"), ECVF_SetByCode);
		}
		UStealthSettings* Settings = GetMutableDefault<UStealthSettings>();
		PreviousNpcCount = Settings->NpcCount;
		Settings->NpcCount = 0;
	}

	AFTER_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName); Timeout && !PreviousTimeout.IsEmpty())
		{
			Timeout->Set(*PreviousTimeout, ECVF_SetByCode);
		}
		if (PreviousNpcCount >= 0)
		{
			GetMutableDefault<UStealthSettings>()->NpcCount = PreviousNpcCount;
		}
	}

	BEFORE_EACH()
	{
		PacketSettings = FPacketSimulationSettings();
		PacketSettings.LoadEmulationProfile(KillWhileConcealedPie::LeakNetProfile);
		FNetworkComponentBuilder<FBasePIENetworkComponentState>()
			.AsListenServer()
			.WithClients(2)
			.WithGameMode(AStealthGameMode::StaticClass())
			.WithPacketSimulationSettings(&PacketSettings)
			.Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
	}

	/** One frame of observation of the current run on every machine. */
	void Observe()
	{
		using namespace StealthConcealmentPie;
		Frames.Sample();
		KillWhileConcealedPie::FRun& R = Runs[Current];
		const AStealthPawn* Victim = R.Victim.Get();
		const AStealthPawn* Striker = Cast<AStealthPawn>(Attacker->GetPawn());
		if (R.Contact < 0.0 && Striker != nullptr && Striker->GetActionState().Pose == EActionPose::Strike)
		{
			R.Contact = Striker->GetActionState().ContactServerTime;
		}
		if (R.Contact > 0.0 && !R.bSlotChecked && Victim != nullptr)
		{
			// The first server frame after the strike's slot.
			R.bSlotChecked = true;
			TArray<APawn*> Occupancy;
			UConcealmentService::Get(StealthAssassinationPie::ServerWorld())->GetCore()->GetOccupancy(R.BushId, Occupancy);
			R.bOccupancyLeftInSlot = !Occupancy.Contains(Victim);
		}
		if (Victim == nullptr)
		{
			return;
		}
		if (R.Contact > 0.0 && R.EarliestOpponentChannel < 0.0)
		{
			for (AStealthPlayerController* Player : StealthAssassinationPie::ServerPlayers())
			{
				const bool bOpponent = Player != nullptr && !Player->IsLocalController() && (R.OwnerMachine == INDEX_NONE || (Player != RivalV && Player != Teammate));
				UNetConnection* Connection = bOpponent ? Player->GetNetConnection() : nullptr;
				if (Connection != nullptr && Connection->FindActorChannelRef(const_cast<AStealthPawn*>(Victim)) != nullptr)
				{
					R.EarliestOpponentChannel = StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld());
				}
			}
		}
		const TArray<UWorld*> All = StealthAssassinationPie::Worlds();
		for (int32 Machine = 0; Machine < All.Num() && Machine < 3; ++Machine)
		{
			if (Machine == R.OwnerMachine)
			{
				continue; // the victim's own machine keeps its own body
			}
			const AStealthPawn* Copy = Cast<AStealthPawn>(CopyByGuid(All[Machine], Victim));
			R.bDrawn |= IsBodyDrawn(Copy);
			const bool bOpponent = Machine > 0 && (R.OwnerMachine == INDEX_NONE || Machine != StealthPresentationPie::MachineOf(Teammate));
			if (Copy != nullptr && Machine > 0 && R.Fields.IsEmpty() && Copy->HasDeathWrite())
			{
				// The victim's replicated fields as a non-owner remote machine holds them (kind-blindness compares these).
				R.Fields = FString::Printf(TEXT("pose %d, bDiedConcealed %d, DeathServerTime - Contact %.4f"), static_cast<int32>(Copy->GetActionState().Pose),
					Copy->GetDeathState().bDiedConcealed ? 1 : 0, Copy->GetDeathState().DeathServerTime - R.Contact);
			}
			if (Copy != nullptr && bOpponent && !bSeen[Current][Machine])
			{
				bSeen[Current][Machine] = true;
				const double Now = StealthAssassinationPie::ServerNow(All[Machine]);
				R.EarliestOpponentCopy = R.EarliestOpponentCopy < 0.0 ? Now : FMath::Min(R.EarliestOpponentCopy, Now);
				R.bAnyArrival = true;
				R.bArrivedDiedConcealed &= Copy->GetDeathState().bDiedConcealed;
			}
		}
	}

	void RunVictim(int32 Index)
	{
		using namespace StealthAssassinationPie;
		using namespace StealthConcealmentPie;
		Network
			.Then(*FString::Printf(TEXT("run %d: S stands at the bush"), Index), [this, Index]()
			{
				Current = Index;
				PlacePlayerAt(Attacker, Bushes[Index].Anchor + FVector(0.0, -155.0, 0.0), FRotator(0.0, 90.0, 0.0));
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("settle (and the opponents hold no actor for the victim)"), [this, Index]()
			{
				Frames.Sample();
				return ServerNow(ServerWorld()) >= Mark + 2.4;
			})
			.Then(*FString::Printf(TEXT("run %d: blind strike"), Index), [this, Index]()
			{
				const TArray<UWorld*> All = Worlds();
				for (AStealthPlayerController* Player : ServerPlayers())
				{
					Runs[Index].bRemoteOpponent |= !Player->IsLocalController() && (Runs[Index].OwnerMachine == INDEX_NONE || (Player != RivalV && Player != Teammate));
				}
				for (int32 Machine = 1; Machine < All.Num() && Machine < 3; ++Machine)
				{
					const bool bOpponent = Machine != Runs[Index].OwnerMachine && (Runs[Index].OwnerMachine == INDEX_NONE || Machine != StealthPresentationPie::MachineOf(Teammate));
					Runs[Index].bOpponentCopyAtStrike |= bOpponent && CopyByGuid(All[Machine], Runs[Index].Victim.Get()) != nullptr;
				}
				FAttackPayload Payload;
				Payload.WorldPoint = Bushes[Index].Anchor + FVector(0.0, -100.0, 0.0);
				AsOwner(Cast<AStealthPawn>(Attacker->GetPawn()), Seq, [&Payload](UPawnCommandComponent& Command) { Command.Attack(Payload); });
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("through the fall, linger and removal"), [this]()
			{
				Observe();
				return !Runs[Current].Victim.IsValid() || ServerNow(ServerWorld()) >= Mark + KillWhileConcealedPie::WatchSec;
			});
	}

	TEST_METHOD(NpcAndRivalReopenOnlyAtTheDeathWriteNeverDrawn)
	{
		using namespace StealthAssassinationPie;
		using namespace StealthConcealmentPie;
		Network
			.Until(TEXT("three players in Active, bushes built, Concealment bound"), []()
			{
				const UConcealmentService* Service = UConcealmentService::Get(ServerWorld());
				const UBushRuntimeSubsystem* Built = UBushRuntimeSubsystem::Get(ServerWorld());
				return RoundActive() && ServerPlayers().Num() == 3 && Service != nullptr && Service->IsBound() && Built != nullptr && Built->IsBuilt();
			})
			.Then(TEXT("roles; the NPC and the rival hide"), [this]()
			{
				AStealthPlayerController* Pair[2] = { nullptr, nullptr };
				ASSERT_THAT(IsTrue(SplitTeams(Attacker, Pair), TEXT("a 2v1 split")));
				RivalV = !Pair[0]->IsLocalController() ? Pair[0] : Pair[1];
				Teammate = RivalV == Pair[0] ? Pair[1] : Pair[0];
				const TArray<FBushRuntime>& All = UBushRuntimeSubsystem::Get(ServerWorld())->GetBushes();
				Bushes[0] = All[0];
				Bushes[1] = All[1];
				AStealthPawn* Npc = SpawnHider(Bushes[0].Anchor + FVector(140.0, 0.0, 200.0), FRotator(0.0, 180.0, 0.0));
				ASSERT_THAT(IsTrue(Npc != nullptr, TEXT("the NPC spawned")));
				StandAtEntry(Npc, Bushes[0], 0.0);
				const uint16 NpcBush = Bushes[0].Id;
				AsOwner(Npc, Seq, [NpcBush](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Enter(NpcBush)); });
				Runs[0].Victim = Npc;
				Runs[0].BushId = NpcBush;
				PlacePlayerAt(RivalV, Bushes[1].Anchor + FVector(130.0, 0.0, 0.0), FRotator(0.0, 180.0, 0.0));
				const uint16 RivalBush = Bushes[1].Id;
				AsOwner(Cast<AStealthPawn>(RivalV->GetPawn()), Seq, [RivalBush](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Enter(RivalBush)); });
				Runs[1].Victim = Cast<AStealthPawn>(RivalV->GetPawn());
				Runs[1].BushId = RivalBush;
				Runs[1].OwnerMachine = StealthPresentationPie::MachineOf(RivalV);
				PlacePlayerAt(Teammate, Bushes[1].Anchor + FVector(500.0, 300.0, 0.0), FRotator(0.0, -150.0, 0.0));
				PlacePlayerAt(Attacker, Bushes[0].Anchor + FVector(0.0, -700.0, 0.0), FRotator(0.0, 90.0, 0.0));
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("both concealed and culled"), [this]()
			{
				const UStealthConcealmentCull* Cull = UStealthConcealmentCull::Get(ServerWorld());
				int32 Ready = 0;
				for (const KillWhileConcealedPie::FRun& R : Runs)
				{
					Ready += StateOf(R.Victim.Get()) == EConcealState::Concealed && Cull != nullptr && Cull->GetState(R.Victim.Get()) == EConcealCullState::Culled ? 1 : 0;
				}
				return (Ready == 2 && ServerNow(ServerWorld()) >= Mark + 1.5) || ServerNow(ServerWorld()) >= Mark + 6.0;
			});
		RunVictim(0);
		RunVictim(1);
		Network.Then(TEXT("assert"), [this]()
		{
			const FAssassinationTuning_Server Server;
			const double Lead = Server.StrikeNetLeadSec;
			const double Tick = Frames.FrameOf(0);
			for (int32 Index = 0; Index < 2; ++Index)
			{
				const KillWhileConcealedPie::FRun& R = Runs[Index];
				const TCHAR* Who = Index == 0 ? TEXT("NPC") : TEXT("rival");
				TestRunner->AddInfo(FString::Printf(TEXT("CON26 %s: contact %.3f, death write due %.3f, first opponent channel (server) %.3f, earliest opponent actor (client estimate) %.3f, arrived bDiedConcealed %d, drawn %d, fields {%s}"),
					Who, R.Contact, R.Contact - Lead, R.EarliestOpponentChannel, R.EarliestOpponentCopy, R.bArrivedDiedConcealed ? 1 : 0, R.bDrawn ? 1 : 0, *R.Fields));
				ASSERT_THAT(IsTrue(R.Contact > 0.0 && R.bSlotChecked, *FString::Printf(TEXT("%s: the strike executed"), Who)));
				ASSERT_THAT(IsTrue(R.bOccupancyLeftInSlot, *FString::Printf(TEXT("%s: the victim left the occupancy record in the execution slot"), Who)));
				if (!R.bRemoteOpponent)
				{
					TestRunner->AddInfo(FString::Printf(TEXT("CON26 %s: no remote opponent client in this session (the solo attacker is the listen host); the opponent-connection checks are covered by the NPC run"), Who));
					ASSERT_THAT(IsFalse(R.bDrawn, *FString::Printf(TEXT("%s: never drawn on any non-owner machine through fall, linger and removal"), Who)));
					continue;
				}
				ASSERT_THAT(IsFalse(R.bOpponentCopyAtStrike, *FString::Printf(TEXT("%s: no opponent held an actor at the strike (the cull had closed it)"), Who)));
				ASSERT_THAT(IsTrue(R.bAnyArrival, *FString::Printf(TEXT("%s: the death write re-opened the actor on an opponent machine"), Who)));
				// Keyed to the server's send side (the client's clock estimate under LeakNet is no measure of when a bunch left).
				ASSERT_THAT(IsTrue(R.EarliestOpponentChannel >= R.Contact - Lead - Tick, *FString::Printf(TEXT("%s: no opponent connection holds a channel (no bunch) before ContactServerTime - StrikeNetLeadSec (+- one tick)"), Who)));
				ASSERT_THAT(IsTrue(R.bArrivedDiedConcealed, *FString::Printf(TEXT("%s: the re-opened actor carries bDiedConcealed"), Who)));
				ASSERT_THAT(IsFalse(R.bDrawn, *FString::Printf(TEXT("%s: never drawn on any non-owner machine through fall, linger and removal"), Who)));
			}
			ASSERT_THAT(IsTrue(Runs[0].Fields == Runs[1].Fields, TEXT("the NPC and the player victim carry the same replicated fields and values")));
		});
	}
};

#endif
