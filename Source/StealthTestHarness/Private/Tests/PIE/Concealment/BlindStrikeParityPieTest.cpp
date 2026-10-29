// Concealment story con-033 AC1 (production/epics/concealment-bushes/story-033-blind-strike-parity-and-kill-timing.md;
// GDD AC-CON-15; ADR-0017 sections 7 and 8 B): StealthGame.Concealment.PIE.BlindStrikeParity.<method>. Listen host + 2
// clients on L_Prototype, NpcCount 0. The attacker A (a remote client of the pair) blind-strikes the spot of five bushes
// in turn, from the same stance relative to each (capsule 20 cm off the planter's -Y edge, facing the anchor; the strike
// point the footprint's -Y edge midpoint), with a fixed command sequence base:
//   0 an empty bush, 1 a bush holding only A's ally (concealed), 2 a bush whose concealed NPC is out of reach (A stands
//   2.45 m from the anchor: the point is in the zone, the occupant beyond F1), 3 a bush with a concealed NPC, 4 a bush
//   with the concealed rival, 5 an empty bush struck from case 2's stance (case 2's same-request control).
// On A's machine, from the strike to the Owner-only confirmation (the FStrikeResult reply), the trace is recorded: every
// replicated property change of A's own pawn and controller copies (movement excluded) and every reply, each with its
// receipt time relative to the strike's StartServerTime, plus A's Strike pose fields (Variant, Contact - Start).
//   TracesIdenticalUntilTheConfirmationNpcBodyNeverDrawn: each trace is identical to the trace of the same request into an
//     empty bush (cases 1, 3, 4 against case 0; case 2 against case 5; keys equal, receipt times within +- one server tick
//     and one frame of A's machine); case 3 is an NPC kill (case 4 a rival kill, cases 0-2 whiffs), and
//     the killed NPC's body is never drawn on any machine (it has no owner) through its fall, linger and removal.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "Server/EncounterService.h"
#include "Server/StealthCompositionRoot.h"
#include "Server/Visibility/StealthConcealmentCull.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/OwnPresentationPieSupport.h"
#include "Tests/PIE/Assassination/PresentationLogHook.h"
#include "Tests/PIE/Assassination/PresentationPieSupport.h"
#include "Tests/PIE/Concealment/ConcealmentPieSupport.h"

namespace BlindStrikeParityPie
{
	/** Five cases plus case 5, an empty bush struck from case 2's stance (the out-of-reach case's same-request control). */
	constexpr int32 CaseCount = 6;
	/** The reference case of each case: the same request (stance and point), so only the bush's contents differ. */
	constexpr int32 ReferenceOf[CaseCount] = { 0, 0, 5, 0, 0, 5 };
	/** A's capsule centre from the anchor along -Y (in reach: 20 cm off the planter edge with r_c 35). */
	constexpr double InReachCm = 155.0;
	/** Case 2: beyond F1 (2.45 m > 2.35 m boundary), the strike point still in the zone. */
	constexpr double OutOfReachCm = 245.0;
	constexpr double BodyWatchSec = 10.5;

	struct FTraceEntry
	{
		FString Key;
		double At = 0.0;
	};
}

NETWORK_TEST_CLASS(BlindStrikeParity, "StealthGame.Concealment.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousTimeout;
	inline static int32 PreviousNpcCount = -1;

	AStealthPlayerController* Attacker = nullptr;
	AStealthPlayerController* Ally = nullptr;
	AStealthPlayerController* Rival = nullptr;
	int32 MachineA = INDEX_NONE;
	TArray<FBushRuntime> Bushes;
	TWeakObjectPtr<AStealthPawn> NpcOutOfReach;
	TWeakObjectPtr<AStealthPawn> NpcVictim;
	StealthOwnPresentationPie::FFrameTracker Frames;
	StealthPresentationLog::FReplicatedPropertyWatcher PawnWatch;
	StealthPresentationLog::FReplicatedPropertyWatcher ControllerWatch;
	TArray<BlindStrikeParityPie::FTraceEntry> Traces[BlindStrikeParityPie::CaseCount];
	FString PoseKey[BlindStrikeParityPie::CaseCount];
	EStrikeResult Results[BlindStrikeParityPie::CaseCount] = {};
	double StrikeStart[BlindStrikeParityPie::CaseCount] = {};
	int32 RepliesBefore = 0;
	int32 Case = 0;
	bool bConfirmed = false;
	bool bNpcBodyDrawn = false;
	bool bNpcDied = false;
	bool bRivalDied = false;
	uint32 Seq = 0x33100000u;
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
		FNetworkComponentBuilder<FBasePIENetworkComponentState>()
			.AsListenServer()
			.WithClients(2)
			.WithGameMode(AStealthGameMode::StaticClass())
			.Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
	}

	AStealthPlayerController* LocalA() const { return StealthAssassinationPie::OwnerMachineController(Attacker); }

	/** Hides an unbound NPC in Bush (returned), standing it at the +X entry first. */
	AStealthPawn* HideNpc(const FBushRuntime& Bush)
	{
		AStealthPawn* Npc = StealthConcealmentPie::SpawnHider(Bush.Anchor + FVector(140.0, 0.0, 200.0), FRotator(0.0, 180.0, 0.0));
		if (Npc != nullptr)
		{
			StealthConcealmentPie::StandAtEntry(Npc, Bush, 0.0);
			const uint16 BushId = Bush.Id;
			StealthAssassinationPie::AsOwner(Npc, Seq, [BushId](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Enter(BushId)); });
		}
		return Npc;
	}

	void HidePlayer(AStealthPlayerController* Player, const FBushRuntime& Bush)
	{
		StealthConcealmentPie::PlacePlayerAt(Player, Bush.Anchor + FVector(130.0, 0.0, 0.0), FRotator(0.0, 180.0, 0.0));
		const uint16 BushId = Bush.Id;
		StealthAssassinationPie::AsOwner(Cast<AStealthPawn>(Player->GetPawn()), Seq, [BushId](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Enter(BushId)); });
	}

	void SampleNpcBody()
	{
		const AStealthPawn* Npc = NpcVictim.Get();
		if (Npc == nullptr)
		{
			return;
		}
		bNpcDied |= Npc->HasDeathWrite();
		for (UWorld* World : StealthAssassinationPie::Worlds())
		{
			bNpcBodyDrawn |= StealthConcealmentPie::IsBodyDrawn(Cast<AStealthPawn>(StealthConcealmentPie::CopyByGuid(World, Npc)));
		}
	}

	/** One frame of the current case's trace on A's machine (until the confirmation arrives). */
	void SampleTrace()
	{
		Frames.Sample();
		SampleNpcBody();
		if (bConfirmed)
		{
			return;
		}
		AStealthPlayerController* Local = LocalA();
		const AStealthPawn* Server = Cast<AStealthPawn>(Attacker->GetPawn());
		if (Local == nullptr || Server == nullptr)
		{
			return;
		}
		if (StrikeStart[Case] <= 0.0 && Server->GetActionState().Pose == EActionPose::Strike)
		{
			StrikeStart[Case] = Server->GetActionState().StartServerTime;
		}
		PawnWatch.Sample();
		ControllerWatch.Sample();
		const TArray<FCommandReply>& Replies = Local->GetReceivedReplies();
		const double Now = StealthAssassinationPie::ServerNow(Local->GetWorld());
		for (int32 Index = RepliesBefore; Index < Replies.Num(); ++Index)
		{
			if (Replies[Index].Kind == ECommandReplyKind::StrikeResult)
			{
				bConfirmed = true;
				Results[Case] = Replies[Index].Strike.Result;
				break;
			}
			Traces[Case].Add({ FString::Printf(TEXT("Reply.%d"), static_cast<int32>(Replies[Index].Kind)), Now });
		}
		RepliesBefore = Replies.Num();
		const AStealthPawn* Copy = Cast<AStealthPawn>(Local->GetPawn());
		if (Copy != nullptr && PoseKey[Case].IsEmpty() && Copy->GetActionState().Pose == EActionPose::Strike)
		{
			const FActionState& Pose = Copy->GetActionState();
			PoseKey[Case] = FString::Printf(TEXT("Variant %d, Contact - Start %.4f"), Pose.Variant, Pose.ContactServerTime - Pose.StartServerTime);
		}
	}

	void RunCase(int32 Index)
	{
		using namespace StealthAssassinationPie;
		using namespace StealthConcealmentPie;
		Network
			.Until(TEXT("A's previous strike recovered (its root-motion warp would pull a teleported pawn back)"), [this]()
			{
				Frames.Sample();
				SampleNpcBody();
				const AStealthPawn* A = Cast<AStealthPawn>(Attacker->GetPawn());
				return A == nullptr || A->GetActionState().Pose != EActionPose::Strike;
			})
			.Then(*FString::Printf(TEXT("case %d: A at the bush"), Index), [this, Index]()
			{
				Case = Index;
				const FBushRuntime& Bush = Bushes[Index];
				const double Back = Index == 2 || Index == 5 ? BlindStrikeParityPie::OutOfReachCm : BlindStrikeParityPie::InReachCm;
				PlacePlayerAt(Attacker, Bush.Anchor + FVector(0.0, -Back, 0.0), FRotator(0.0, 90.0, 0.0));
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("A settles"), [this]() { Frames.Sample(); SampleNpcBody(); return ServerNow(ServerWorld()) >= Mark + 1.2; })
			.Then(*FString::Printf(TEXT("case %d: the blind strike"), Index), [this, Index]()
			{
				bConfirmed = false;
				RepliesBefore = LocalA()->GetReceivedReplies().Num();
				PawnWatch.Begin(LocalA()->GetPawn(), MachineA);
				ControllerWatch.Begin(LocalA(), MachineA);
				FAttackPayload Payload;
				Payload.WorldPoint = Bushes[Index].Anchor + FVector(0.0, -100.0, 0.0);
				Seq = 0x33100000u + static_cast<uint32>(Index) * 16u; // the same command sequence shape every case
				AsOwner(Cast<AStealthPawn>(Attacker->GetPawn()), Seq, [&Payload](UPawnCommandComponent& Command) { Command.Attack(Payload); });
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("the trace until the confirmation"), [this]()
			{
				SampleTrace();
				return (bConfirmed && ServerNow(ServerWorld()) >= Mark + 0.3) || ServerNow(ServerWorld()) >= Mark + 2.0;
			})
			.Then(TEXT("collect"), [this, Index]()
			{
				// Diagnostics: the server's resolution of this strike and where the bush's occupants stand.
				const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(StealthAssassinationPie::ServerWorld());
				const UEncounterService* Encounters = Root != nullptr ? Root->GetEncounterService() : nullptr;
				if (Encounters != nullptr && Encounters->GetCore() != nullptr && !Encounters->GetCore()->GetResolutions().IsEmpty())
				{
					const FAttackResolution& R = Encounters->GetCore()->GetResolutions().Last();
					TArray<APawn*> Occupancy;
					UConcealmentService::Get(StealthAssassinationPie::ServerWorld())->GetCore()->GetOccupancy(Bushes[Index].Id, Occupancy);
					FString Where;
					for (const APawn* Occupant : Occupancy)
					{
						Where += FString::Printf(TEXT(" %s@%s"), *Occupant->GetName(), *(Occupant->GetActorLocation() - Bushes[Index].Anchor).ToCompactString());
					}
					const APawn* A = Attacker->GetPawn();
					TestRunner->AddInfo(FString::Printf(TEXT("CON15 case %d server: rule %d bush %d victim %s outcome %d reach %d (d %.1f); attacker at %s yaw %.1f; occupancy:%s"), Index,
						static_cast<int32>(R.Rule), R.BushId, R.Victim != nullptr ? *R.Victim->GetName() : TEXT("-"), static_cast<int32>(R.Outcome), R.Reach.bInReach ? 1 : 0, R.Reach.DistanceCm,
						A != nullptr ? *(A->GetActorLocation() - Bushes[Index].Anchor).ToCompactString() : TEXT("-"), A != nullptr ? A->GetActorRotation().Yaw : 0.0, *Where));
				}
				for (const StealthPresentationLog::FPropertyChange& Change : PawnWatch.GetChanges())
				{
					if (!StealthPresentationLog::FReplicatedPropertyWatcher::IsMovementProperty(Change.Property))
					{
						Traces[Index].Add({ TEXT("Pawn.") + Change.Property, Change.ServerNow });
					}
				}
				for (const StealthPresentationLog::FPropertyChange& Change : ControllerWatch.GetChanges())
				{
					Traces[Index].Add({ TEXT("Controller.") + Change.Property, Change.ServerNow });
				}
				Traces[Index].Sort([](const BlindStrikeParityPie::FTraceEntry& L, const BlindStrikeParityPie::FTraceEntry& R) { return L.Key < R.Key || (L.Key == R.Key && L.At < R.At); });
				bRivalDied |= Rival->GetPawn() == nullptr || Cast<AStealthPawn>(Rival->GetPawn())->HasDeathWrite();
			});
	}

	TEST_METHOD(TracesIdenticalUntilTheConfirmationNpcBodyNeverDrawn)
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
			.Then(TEXT("roles; the ally, the NPCs and the rival hide"), [this]()
			{
				AStealthPlayerController* Pair[2] = { nullptr, nullptr };
				ASSERT_THAT(IsTrue(SplitTeams(Rival, Pair), TEXT("a 2v1 split")));
				Attacker = !Pair[0]->IsLocalController() ? Pair[0] : Pair[1];
				Ally = Attacker == Pair[0] ? Pair[1] : Pair[0];
				MachineA = StealthPresentationPie::MachineOf(Attacker);
				ASSERT_THAT(IsTrue(!Attacker->IsLocalController() && MachineA != INDEX_NONE, TEXT("A is a remote client")));
				const TArray<FBushRuntime>& All = UBushRuntimeSubsystem::Get(ServerWorld())->GetBushes();
				ASSERT_THAT(IsTrue(All.Num() >= BlindStrikeParityPie::CaseCount, TEXT("six bushes")));
				for (int32 Index = 0; Index < BlindStrikeParityPie::CaseCount; ++Index)
				{
					Bushes.Add(All[Index]);
				}
				HidePlayer(Ally, Bushes[1]);
				NpcOutOfReach = HideNpc(Bushes[2]);
				NpcVictim = HideNpc(Bushes[3]);
				HidePlayer(Rival, Bushes[4]);
				PlacePlayerAt(Attacker, Bushes[0].Anchor + FVector(0.0, -600.0, 0.0), FRotator(0.0, 90.0, 0.0));
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("all four hidden and culled"), [this]()
			{
				const APawn* Hidden[4] = { Ally->GetPawn(), NpcOutOfReach.Get(), NpcVictim.Get(), Rival->GetPawn() };
				const UStealthConcealmentCull* Cull = UStealthConcealmentCull::Get(ServerWorld());
				int32 Ready = 0;
				for (const APawn* Pawn : Hidden)
				{
					Ready += StateOf(Pawn) == EConcealState::Concealed && Cull != nullptr && Cull->GetState(Pawn) == EConcealCullState::Culled ? 1 : 0;
				}
				return (Ready == 4 && ServerNow(ServerWorld()) >= Mark + 1.5) || ServerNow(ServerWorld()) >= Mark + 6.0;
			});
		// The rival's kill last: it ends the solo team's living presence, after which no further strike would be legal.
		for (const int32 Index : { 0, 1, 2, 5, 3, 4 })
		{
			RunCase(Index);
		}
		Network
			.Then(TEXT("mark"), [this]() { Mark = ServerNow(ServerWorld()); })
			.Until(TEXT("watch the killed NPC's body through its removal"), [this]()
			{
				Frames.Sample();
				SampleNpcBody();
				return !NpcVictim.IsValid() || ServerNow(ServerWorld()) >= Mark + BlindStrikeParityPie::BodyWatchSec;
			})
			.Then(TEXT("assert"), [this]()
			{
				const double Tolerance = Frames.FrameOf(0) + Frames.FrameOf(MachineA);
				for (int32 Index = 0; Index < BlindStrikeParityPie::CaseCount; ++Index)
				{
					FString Text;
					for (const BlindStrikeParityPie::FTraceEntry& Entry : Traces[Index])
					{
						Text += FString::Printf(TEXT(" %s@%+.3f"), *Entry.Key, Entry.At - StrikeStart[Index]);
					}
					TestRunner->AddInfo(FString::Printf(TEXT("CON15 case %d: result %d, pose {%s}, trace:%s"), Index, static_cast<int32>(Results[Index]), *PoseKey[Index], *Text));
					ASSERT_THAT(IsTrue(StrikeStart[Index] > 0.0 && !PoseKey[Index].IsEmpty(), *FString::Printf(TEXT("case %d: the strike executed and reached A's machine"), Index)));
				}
				for (int32 Index = 1; Index < BlindStrikeParityPie::CaseCount; ++Index)
				{
					const int32 Ref = BlindStrikeParityPie::ReferenceOf[Index];
					if (Ref == Index)
					{
						continue;
					}
					ASSERT_THAT(IsTrue(PoseKey[Index] == PoseKey[Ref], *FString::Printf(TEXT("case %d: A's Strike pose fields equal case %d's (the same request)"), Index, Ref)));
					ASSERT_THAT(IsTrue(Traces[Index].Num() == Traces[Ref].Num(), *FString::Printf(TEXT("case %d: as many trace entries as case %d (%d vs %d)"), Index, Ref, Traces[Index].Num(), Traces[Ref].Num())));
					for (int32 Entry = 0; Entry < Traces[Ref].Num(); ++Entry)
					{
						const double A = Traces[Ref][Entry].At - StrikeStart[Ref];
						const double B = Traces[Index][Entry].At - StrikeStart[Index];
						ASSERT_THAT(IsTrue(Traces[Index][Entry].Key == Traces[Ref][Entry].Key && FMath::Abs(A - B) <= Tolerance,
							*FString::Printf(TEXT("case %d entry %d: %s@%+.3f equals case %d's %s@%+.3f (+- %.3f)"), Index, Entry, *Traces[Index][Entry].Key, B, Ref, *Traces[Ref][Entry].Key, A, Tolerance)));
					}
				}
				ASSERT_THAT(IsTrue(Results[0] == EStrikeResult::NoContact && Results[1] == EStrikeResult::NoContact && Results[2] == EStrikeResult::NoContact,
					TEXT("cases 0-2 (empty, ally-only, out of reach) are whiffs")));
				ASSERT_THAT(IsTrue(Results[5] == EStrikeResult::NoContact, TEXT("case 5 (empty, the out-of-reach stance) is a whiff")));
				ASSERT_THAT(IsTrue(bNpcDied && Results[3] != EStrikeResult::NoContact, TEXT("case 3 is an NPC kill")));
				ASSERT_THAT(IsTrue(bRivalDied && Results[4] != EStrikeResult::NoContact, TEXT("case 4 kills the concealed rival")));
				ASSERT_THAT(IsFalse(bNpcBodyDrawn, TEXT("the killed NPC's body is never drawn on any machine through its removal")));
			});
	}
};

#endif
