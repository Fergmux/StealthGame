// Concealment story con-029 AC1 (production/epics/concealment-bushes/story-029-rustle-parity.md; GDD AC-CON-18 automated
// part, CON-R13; ADR-0017 section 9): StealthGame.Concealment.PIE.RustleParity.<method>. Listen host + 2 clients on
// L_Prototype, NpcCount 0, the re-entry cooldown lifted (a test seam) so the cycles run back to back.
//   IdenticalForNpcAndPlayerAtEveryOccupancyAndLodSilentWhileOccupied: at three bushes holding 0, 1 and 4 concealed
//     occupants, an NPC (an unbound Shared Pawn) and a player (a remote client's pawn) each enter, exit and bush-emerge
//     through the production paths. On every machine each transition writes exactly one rustle pulse, and its SFX asset,
//     onset (relative to the pose start), amplitude, side and face position (relative to the bush anchor) are identical
//     for both pawn kinds, at every occupancy and on every machine; the 1-occupant bush runs with every copy of the
//     transitioning pawn forced to its farthest LOD (the pulse reads no LOD). While a bush is merely occupied (2.5 s with
//     its occupants settled) no pulse is written on any machine. 1P / 3P: the pulse path reads no camera state (the
//     RustlePulseSlot source scan); the production camera modes are the Camera epic's.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Concealment/ConcealmentPieSupport.h"
#include "StealthPlayerController.h"
#include "View/BushRustle.h"
#include "View/Input/PawnCommandClient.h"

namespace RustleParityPie
{
	constexpr int32 Occupancies[3] = { 0, 1, 4 };
	constexpr double QuietSec = 2.5;

	/** The pulse fields that must be identical (relative to the pose start and the bush anchor). */
	struct FPulseKey
	{
		FString Sound;
		double OnsetAfterStart = 0.0;
		float Amplitude = 0.0f;
		uint8 Side = 0;
		FVector FaceOffset = FVector::ZeroVector;

		bool Matches(const FPulseKey& Other) const
		{
			return Sound == Other.Sound && FMath::IsNearlyEqual(OnsetAfterStart, Other.OnsetAfterStart, 1e-6) && FMath::IsNearlyEqual(Amplitude, Other.Amplitude)
				&& Side == Other.Side && FaceOffset.Equals(Other.FaceOffset, 0.01);
		}
		FString Describe() const
		{
			return FString::Printf(TEXT("{%s, onset +%.3f, amp %.1f, side %d, face %s}"), *Sound, OnsetAfterStart, Amplitude, Side, *FaceOffset.ToCompactString());
		}
	};
}

NETWORK_TEST_CLASS(RustleParity, "StealthGame.Concealment.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousTimeout;
	inline static int32 PreviousNpcCount = -1;

	enum class EStep : uint8 { SpawnOccupants, WaitOccupants, Quiet, Place, Enter, WaitIn, Exit, WaitOut, Emerge, WaitOutEmerge, Away, Done };

	struct FTransitionRecord
	{
		int32 Case = 0;
		bool bPlayer = false;
		EBushRustleKind Kind = EBushRustleKind::Enter;
		double Start = -1.0;
		uint16 BushId = 0;
	};

	AStealthPlayerController* Player = nullptr;
	TWeakObjectPtr<AStealthPawn> Npc;
	TArray<TWeakObjectPtr<AStealthPawn>> Occupants;
	TArray<FBushRuntime> CaseBush;
	TArray<FTransitionRecord> Records;
	TOptional<FConcealmentParams> PreviousParams;
	EStep Step = EStep::SpawnOccupants;
	int32 Case = 0;
	bool bPlayerTurn = false;
	double Since = 0.0;
	int32 QuietBefore[3] = {};
	int32 QuietViolations = 0;
	FString QuietDetail;
	uint32 Seq = 0x29000000u;
	double RunStart = 0.0;

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

	AFTER_EACH()
	{
		if (PreviousParams.IsSet())
		{
			if (UConcealmentService* Service = UConcealmentService::Get(StealthAssassinationPie::ServerWorld()); Service != nullptr && Service->GetCore() != nullptr)
			{
				Service->GetCore()->SetParamsForTest(PreviousParams.GetValue());
			}
		}
	}

	AStealthPawn* Mover() const { return bPlayerTurn ? Cast<AStealthPawn>(Player->GetPawn()) : Npc.Get(); }

	int32 PulseCount(int32 Machine, uint16 BushId) const
	{
		const UBushRuntimeSubsystem* Bushes = UBushRuntimeSubsystem::Get(StealthAssassinationPie::Worlds()[Machine]);
		return Bushes != nullptr ? Bushes->GetRustleLog().FilterByPredicate([BushId](const FBushRustlePulse& P) { return P.BushId == BushId; }).Num() : 0;
	}

	void SetFarthestLod(const AStealthPawn* ServerPawn, bool bFarthest)
	{
		for (UWorld* World : StealthAssassinationPie::Worlds())
		{
			const AStealthPawn* Copy = Cast<AStealthPawn>(StealthConcealmentPie::CopyByGuid(World, ServerPawn));
			if (USkeletalMeshComponent* Mesh = Copy != nullptr ? Copy->GetMesh() : nullptr)
			{
				Mesh->SetForcedLOD(bFarthest ? FMath::Max(Mesh->GetNumLODs(), 1) : 0); // forced LOD is 1-based; 0 = auto
			}
		}
	}

	/** Places the mover at the case bush's +X entry facing the anchor (a player's control rotation follows). */
	void PlaceMover()
	{
		const FBushRuntime& Bush = CaseBush[Case];
		if (bPlayerTurn)
		{
			// Aim yaw 0: the same aim as the unbound NPC (no controller: GetControlRotation is zero), so both emerge on side 0.
			StealthConcealmentPie::PlacePlayerAt(Player, Bush.Anchor + FVector(140.0, 0.0, 0.0), FRotator::ZeroRotator);
		}
		else
		{
			StealthConcealmentPie::StandAtEntry(Npc.Get(), Bush, 0.0);
		}
	}

	void Record(EBushRustleKind Kind)
	{
		FTransitionRecord& R = Records.AddDefaulted_GetRef();
		R.Case = Case;
		R.bPlayer = bPlayerTurn;
		R.Kind = Kind;
		R.BushId = CaseBush[Case].Id;
	}

	/** Binds the last record's Start from the mover's server pose. */
	void BindStart()
	{
		const AStealthPawn* Pawn = Mover();
		if (Records.IsEmpty() || Records.Last().Start > 0.0 || Pawn == nullptr)
		{
			return;
		}
		const TOptional<EBushRustleKind> Kind = BushRustle::KindOfPose(Pawn->GetActionState());
		if (Kind.IsSet() && Kind.GetValue() == Records.Last().Kind)
		{
			Records.Last().Start = Pawn->GetActionState().StartServerTime;
		}
	}

	/**
	 * The player's Hide goes through its owner's production press (UPawnCommandClient::PressHide: the toggled command, Exit
	 * on the side the look yaw faces, and the owner-side warp prediction the server's correction window assumes); the NPC's
	 * through its command component on the server, as the crowd submits it.
	 */
	void PressOrSubmit(TFunctionRef<void(UPawnCommandComponent&)> NpcSubmit)
	{
		if (bPlayerTurn)
		{
			if (AStealthPlayerController* Local = StealthAssassinationPie::OwnerMachineController(Player); Local != nullptr && Local->GetCommandClient() != nullptr)
			{
				Local->GetCommandClient()->PressHide();
			}
			return;
		}
		StealthAssassinationPie::AsOwner(Npc.Get(), Seq, NpcSubmit);
	}

	/** The step machine; true when every case ran. */
	bool Drive()
	{
		using namespace StealthAssassinationPie;
		using namespace StealthConcealmentPie;
		const double Now = ServerNow(ServerWorld());
		const FConcealmentTuning_Public Tuning;
		const FBushRuntime& Bush = CaseBush[FMath::Min(Case, CaseBush.Num() - 1)];
		BindStart();
		switch (Step)
		{
		case EStep::SpawnOccupants:
			for (int32 Index = 0; Index < RustleParityPie::Occupancies[Case]; ++Index)
			{
								AStealthPawn* Pawn = StealthConcealmentPie::SpawnHider(Bush.Anchor + FVector(0.0, 140.0, 200.0), FRotator::ZeroRotator);
				if (Pawn == nullptr)
				{
					continue;
				}
				// Each occupant enters from the +Y side, one by one offset along the face.
				Pawn->TeleportTo(Bush.Anchor + FVector(-60.0 + 40.0 * Index, 140.0, Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0), FRotator(0.0, -90.0, 0.0), false, true);
				const uint16 BushId = Bush.Id;
				AsOwner(Pawn, Seq, [BushId](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Enter(BushId)); });
				Occupants.Add(Pawn);
			}
			Step = EStep::WaitOccupants;
			Since = Now;
			break;
		case EStep::WaitOccupants:
		{
			int32 In = 0;
			for (const TWeakObjectPtr<AStealthPawn>& Occupant : Occupants)
			{
				In += Occupant.IsValid() && StateOf(Occupant.Get()) == EConcealState::Concealed && FVector::Dist2D(Occupant->GetActorLocation(), Bush.Anchor) < 100.0 ? 1 : 0;
			}
			if ((In >= RustleParityPie::Occupancies[Case] && Now >= Since + Tuning.BushEnterDurSec + 0.6) || Now >= Since + 5.0)
			{
				for (int32 Machine = 0; Machine < Worlds().Num() && Machine < 3; ++Machine)
				{
					QuietBefore[Machine] = PulseCount(Machine, Bush.Id);
				}
				Step = EStep::Quiet;
				Since = Now;
			}
			break;
		}
		case EStep::Quiet:
			if (Now >= Since + RustleParityPie::QuietSec)
			{
				for (int32 Machine = 0; Machine < Worlds().Num() && Machine < 3; ++Machine)
				{
					const int32 Added = PulseCount(Machine, Bush.Id) - QuietBefore[Machine];
					if (Added != 0)
					{
						++QuietViolations;
						QuietDetail += FString::Printf(TEXT(" [case %d machine %d: %d pulses]"), Case, Machine, Added);
					}
				}
				bPlayerTurn = false;
				Step = EStep::Place;
				Since = Now;
			}
			break;
		case EStep::Place:
			PlaceMover();
			SetFarthestLod(Mover(), RustleParityPie::Occupancies[Case] == 1);
			Step = EStep::Enter;
			Since = Now;
			break;
		case EStep::Enter:
			if (Now >= Since + 0.6)
			{
				const uint16 BushId = Bush.Id;
				Record(EBushRustleKind::Enter);
				PressOrSubmit([BushId](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Enter(BushId)); });
				Step = EStep::WaitIn;
				Since = Now;
			}
			break;
		case EStep::WaitIn:
			if (StateOf(Mover()) == EConcealState::Concealed && Now >= Since + Tuning.BushEnterDurSec + 0.3)
			{
				const bool bEmergeNext = Records.FilterByPredicate([this](const FTransitionRecord& R) { return R.Case == Case && R.bPlayer == bPlayerTurn && R.Kind == EBushRustleKind::Exit; }).Num() > 0;
				Step = bEmergeNext ? EStep::Emerge : EStep::Exit;
				Since = Now;
			}
			else if (Now >= Since + 4.0)
			{
				Step = EStep::Away; // refused: the record stays unbound and fails the assertion
			}
			break;
		case EStep::Exit:
			Record(EBushRustleKind::Exit);
			PressOrSubmit([](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Exit(0)); });
			Step = EStep::WaitOut;
			Since = Now;
			break;
		case EStep::WaitOut:
			if (StateOf(Mover()) == EConcealState::Outside && Now >= Since + Tuning.BushExitDurSec + 0.3)
			{
				Step = EStep::Place; // again, then the emerge
				Since = Now;
			}
			else if (Now >= Since + 4.0)
			{
				Step = EStep::Away;
			}
			break;
		case EStep::Emerge:
		{
			Record(EBushRustleKind::Emerge);
			FAttackPayload Payload;
			Payload.WorldPoint = Bush.Anchor + FVector(-500.0, 0.0, 0.0); // open ground: a whiff
			AsOwner(Mover(), Seq, [&Payload](UPawnCommandComponent& Command) { Command.Attack(Payload); });
			Step = EStep::WaitOutEmerge;
			Since = Now;
			break;
		}
		case EStep::WaitOutEmerge:
			if (StateOf(Mover()) == EConcealState::Outside && Now >= Since + 2.1)
			{
				Step = EStep::Away;
				Since = Now;
			}
			else if (Now >= Since + 5.0)
			{
				Step = EStep::Away;
			}
			break;
		case EStep::Away:
		{
			SetFarthestLod(Mover(), false);
			const FVector Aside = Bush.Anchor + FVector(0.0, -700.0 - (bPlayerTurn ? 200.0 : 0.0), 0.0);
			if (bPlayerTurn)
			{
				PlacePlayerAt(Player, Aside, FRotator::ZeroRotator);
			}
			else
			{
				Npc->TeleportTo(Aside + FVector(0.0, 0.0, 120.0), FRotator::ZeroRotator, false, true);
			}
			if (!bPlayerTurn)
			{
				bPlayerTurn = true;
				Step = EStep::Place;
			}
			else if (++Case < UE_ARRAY_COUNT(RustleParityPie::Occupancies))
			{
				Step = EStep::SpawnOccupants;
			}
			else
			{
				Step = EStep::Done;
			}
			Since = Now;
			break;
		}
		case EStep::Done:
			return Now >= Since + 1.0;
		}
		return false;
	}

	TEST_METHOD(IdenticalForNpcAndPlayerAtEveryOccupancyAndLodSilentWhileOccupied)
	{
		using namespace StealthAssassinationPie;
		using namespace StealthConcealmentPie;
		Network
			.Until(TEXT("three players in Active, bushes built everywhere, Concealment bound"), []()
			{
				const UConcealmentService* Service = UConcealmentService::Get(ServerWorld());
				if (!RoundActive() || ServerPlayers().Num() != 3 || Service == nullptr || !Service->IsBound())
				{
					return false;
				}
				for (UWorld* World : Worlds())
				{
					const UBushRuntimeSubsystem* Bushes = UBushRuntimeSubsystem::Get(World);
					if (Bushes == nullptr || !Bushes->IsBuilt())
					{
						return false;
					}
				}
				return true;
			})
			.Then(TEXT("the NPC, the player (a remote client), three bushes, watchers aside"), [this]()
			{
				PreviousParams = LiftReentryCooldown();
				ASSERT_THAT(IsTrue(PreviousParams.IsSet(), TEXT("the server core exists")));
				for (AStealthPlayerController* Candidate : ServerPlayers())
				{
					if (Player == nullptr && !Candidate->IsLocalController())
					{
						Player = Candidate;
					}
				}
				ASSERT_THAT(IsTrue(Player != nullptr, TEXT("a remote client player")));
				const TArray<FBushRuntime>& All = UBushRuntimeSubsystem::Get(ServerWorld())->GetBushes();
				ASSERT_THAT(IsTrue(All.Num() >= 3, TEXT("three bushes")));
				CaseBush = { All[0], All[1], All[2] };
				int32 Slot = 0;
				for (AStealthPlayerController* Watcher : ServerPlayers())
				{
					if (Watcher != Player)
					{
						PlacePlayerAt(Watcher, All[0].Anchor + FVector(-800.0, 400.0 * Slot++, 0.0), FRotator::ZeroRotator);
					}
				}
								Npc = StealthConcealmentPie::SpawnHider(All[0].Anchor + FVector(0.0, -700.0, 200.0), FRotator::ZeroRotator);
				ASSERT_THAT(IsTrue(Npc.IsValid(), TEXT("the NPC spawned")));
				PlacePlayerAt(Player, All[0].Anchor + FVector(0.0, -900.0, 0.0), FRotator::ZeroRotator);
				RunStart = ServerNow(ServerWorld());
			})
			.Until(TEXT("run the cases"), [this]() { return Drive() || StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()) > RunStart + 400.0; })
			.Then(TEXT("assert parity"), [this]()
			{
				using namespace RustleParityPie;
				const TArray<UWorld*> All = Worlds();
				ASSERT_THAT(IsTrue(Step == EStep::Done, TEXT("every case ran")));
				TestRunner->AddInfo(FString::Printf(TEXT("CON18 %d transitions recorded; quiet windows: %d violations%s"), Records.Num(), QuietViolations, *QuietDetail));
				ASSERT_THAT(IsTrue(Records.Num() == 3 * 2 * 4, TEXT("3 occupancies x 2 kinds x {enter, exit, enter, emerge}")));
				ASSERT_THAT(IsTrue(QuietViolations == 0, TEXT("no pulse while a bush is merely occupied")));
				TOptional<FPulseKey> Reference[3];
				for (const FTransitionRecord& R : Records)
				{
					ASSERT_THAT(IsTrue(R.Start > 0.0, *FString::Printf(TEXT("case %d %s %d: the transition executed"), R.Case, R.bPlayer ? TEXT("player") : TEXT("NPC"), static_cast<int32>(R.Kind))));
					const FBushRuntime& Bush = CaseBush[R.Case];
					for (int32 Machine = 0; Machine < All.Num() && Machine < 3; ++Machine)
					{
						const UBushRuntimeSubsystem* Bushes = UBushRuntimeSubsystem::Get(All[Machine]);
						const TArray<FBushRustlePulse> Pulses = Bushes->GetRustleLog().FilterByPredicate([&R](const FBushRustlePulse& P)
						{
							return P.BushId == R.BushId && P.Kind == R.Kind && FMath::IsNearlyEqual(P.StartServerTime, R.Start, 1e-6);
						});
						ASSERT_THAT(IsTrue(Pulses.Num() == 1, *FString::Printf(TEXT("case %d %s kind %d machine %d: exactly one pulse (%d)"), R.Case, R.bPlayer ? TEXT("player") : TEXT("NPC"), static_cast<int32>(R.Kind), Machine, Pulses.Num())));
						FPulseKey Key;
						Key.Sound = Pulses[0].Sound.ToString();
						Key.OnsetAfterStart = Pulses[0].OnsetServerTime - Pulses[0].StartServerTime;
						Key.Amplitude = Pulses[0].AmplitudeCm;
						Key.Side = Pulses[0].Side;
						Key.FaceOffset = Pulses[0].WorldPos - Bush.Anchor;
						const int32 KindIndex = static_cast<int32>(R.Kind);
						if (!Reference[KindIndex].IsSet())
						{
							Reference[KindIndex] = Key;
							TestRunner->AddInfo(FString::Printf(TEXT("CON18 reference kind %d: %s"), KindIndex, *Key.Describe()));
						}
						ASSERT_THAT(IsTrue(Key.Matches(Reference[KindIndex].GetValue()), *FString::Printf(TEXT("case %d (occupants %d) %s kind %d machine %d: %s equals the reference %s"),
							R.Case, Occupancies[R.Case], R.bPlayer ? TEXT("player") : TEXT("NPC"), KindIndex, Machine, *Key.Describe(), *Reference[KindIndex]->Describe())));
					}
				}
			});
	}
};

#endif
