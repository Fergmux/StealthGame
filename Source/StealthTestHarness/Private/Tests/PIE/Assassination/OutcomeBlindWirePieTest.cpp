// Assassination stories asn-012 / asn-013 / asn-014, client half (ADR-0008 T2; listen server + 2 clients on L_Prototype,
// NpcCount 0): StealthGame.Assassination.PIE.OutcomeBlindWire.<method>. The host's own pawn strikes through the production
// path (UPawnCommandComponent::Attack -> Round's queue -> UEncounterService's slot), three times at an open TargetSpot
// 150 cm ahead: a kill of an unbound Shared Pawn standing there, a whiff (the pawn stands 4 m aside), and a reversal (the
// pawn standing there braced first). On the host (the direct handler, ADR-0018 E7) and on both clients (RepNotify) every
// FActionState / FDeathState presentation is recorded with this machine's server-time estimate:
//   - each run delivers the attacker's Strike to every machine with the same Variant, WarpPoint and Contact - Start;
//   - nothing reaches the target before the death write (receipt >= T_dw - 1 frame, in server time), then FDeathState
//     on the victim (kill) or on the attacker plus Counter on the defender (reversal), DeathServerTime = the Strike's
//     ContactServerTime; the whiff delivers no FDeathState and no Counter anywhere;
//   - story asn-014: no client copy of the attacker holds the server's StrikeContact warp target (the local client set is
//     switched off by a test seam, so a target there could only have come over the wire), and every live pawn of the
//     server (the sample Blueprint pawns included) has no motion-warping component that can send WarpTargets.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/IConsoleManager.h"
#include "Server/ActionHub.h"
#include "Server/Core/StealthPawnCommandComponent.h"
#include "Server/EncounterService.h"
#include "Server/EncounterTypes.h"
#include "Server/StealthCompositionRoot.h"
#include "StealthBush.h"
#include "StealthGameMode.h"
#include "StealthGameState.h"
#include "StealthSettings.h"
#include "View/Core/StealthPawn.h"

namespace OutcomeBlindWirePieData
{
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	const TCHAR* const NetworkTimeoutCeilingSeconds = TEXT("300");
	constexpr double StrikeAheadCm = 150.0;
	constexpr double WhiffAsideCm = 400.0;
	constexpr double ObserveSec = 1.2;
	/** Between runs: past AttackAttemptLockoutSec (1.75 s). */
	constexpr double LockoutWaitSec = 2.2;
	constexpr double FrameSec = 1.0 / 30.0;

	enum class ERun : uint8 { Kill, Whiff, Reversal, Count };
	const TCHAR* RunName(int32 Run) { return Run == 0 ? TEXT("kill") : (Run == 1 ? TEXT("whiff") : TEXT("reversal")); }

	/** One presentation on one machine. Machine 0 is the host. */
	struct FSeen
	{
		int32 Run = 0;
		int32 Machine = 0;
		/** 0 attacker, 1 target, 2 another pawn. */
		int32 Role = 2;
		bool bDeath = false;
		FActionState Action;
		FDeathState Death;
		double ServerTime = 0.0;
	};

	TArray<UWorld*> Worlds()
	{
		TArray<const FWorldContext*> Contexts;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr)
			{
				Contexts.Add(&Context);
			}
		}
		Contexts.Sort([](const FWorldContext& A, const FWorldContext& B)
		{
			const bool bServerA = A.World()->GetNetMode() != NM_Client;
			const bool bServerB = B.World()->GetNetMode() != NM_Client;
			return bServerA != bServerB ? bServerA : A.PIEInstance < B.PIEInstance;
		});
		TArray<UWorld*> Out;
		for (const FWorldContext* Context : Contexts)
		{
			Out.Add(Context->World());
		}
		return Out;
	}

	UWorld* ServerWorld()
	{
		const TArray<UWorld*> All = Worlds();
		return All.Num() > 0 && All[0]->GetNetMode() == NM_ListenServer ? All[0] : nullptr;
	}

	double ServerNow(const UWorld* World)
	{
		const AGameStateBase* State = World != nullptr ? World->GetGameState() : nullptr;
		return State != nullptr ? State->GetServerWorldTimeSeconds() : 0.0;
	}

	AStealthPawn* NearestPawn(UWorld* World, const FVector& At, double WithinCm)
	{
		AStealthPawn* Best = nullptr;
		double BestDistance = WithinCm;
		for (TActorIterator<AStealthPawn> It(World); It; ++It)
		{
			const double Distance = FVector::Dist2D(It->GetActorLocation(), At);
			if (IsValid(*It) && Distance <= BestDistance)
			{
				Best = *It;
				BestDistance = Distance;
			}
		}
		return Best;
	}

	bool RoundActive()
	{
		const AStealthGameState* State = ServerWorld() != nullptr ? ServerWorld()->GetGameState<AStealthGameState>() : nullptr;
		return State != nullptr && State->GetRoundPublicState().Phase == ERoundPhase::Active;
	}
}

NETWORK_TEST_CLASS(OutcomeBlindWire, "StealthGame.Assassination.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	TArray<FString> Labels;
	const TCHAR* L(FString Label) { return *Labels.Add_GetRef(MoveTemp(Label)); }

	TWeakObjectPtr<AStealthPawn> Attacker;
	TWeakObjectPtr<AStealthPawn> Target;
	TArray<TWeakObjectPtr<AStealthPawn>> AttackerCopies;
	TArray<TWeakObjectPtr<AStealthPawn>> TargetCopies;
	TArray<TPair<TWeakObjectPtr<AStealthPawn>, FDelegateHandle>> ActionHandles;
	TArray<TPair<TWeakObjectPtr<AStealthPawn>, FDelegateHandle>> DeathHandles;
	TArray<OutcomeBlindWirePieData::FSeen> Seen;
	int32 Run = 0;
	double Mark = 0.0;
	FVector TargetAt = FVector::ZeroVector;
	FVector StrikePoint = FVector::ZeroVector;
	bool bClientHasServerWarpTarget = false;
	/** UActionHub::OnActionStarted(Attack) broadcasts on the server per run (the crowd R8 stimulus; once per strike). */
	int32 HubAttackStarts[3] = { 0, 0, 0 };
	FDelegateHandle HubHandle;
	bool bServerHasWarpTarget = false;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(OutcomeBlindWirePieData::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
		}
		UStealthSettings* Settings = GetMutableDefault<UStealthSettings>();
		PreviousNpcCount = Settings->NpcCount;
		Settings->NpcCount = 0; // no crowd pawn may stand at the strike point
		StealthPawnTestSeams::bSkipRemoteStrikeWarpTarget = true;
	}

	AFTER_ALL()
	{
		StealthPawnTestSeams::bSkipRemoteStrikeWarpTarget = false;
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
		Network.StartOnFixtureMap(OutcomeBlindWirePieData::FixtureMap);
	}

	/** Binds the recorder to every pawn of every machine for the current run. */
	void Bind()
	{
		using namespace OutcomeBlindWirePieData;
		Unbind();
		const TArray<UWorld*> All = Worlds();
		for (int32 Machine = 0; Machine < All.Num(); ++Machine)
		{
			for (TActorIterator<AStealthPawn> It(All[Machine]); It; ++It)
			{
				AStealthPawn* Pawn = *It;
				const int32 Role = (Machine == 0 ? Pawn == Attacker.Get() : AttackerCopies.Contains(Pawn)) ? 0
					: ((Machine == 0 ? Pawn == Target.Get() : TargetCopies.Contains(Pawn)) ? 1 : 2);
				UWorld* World = All[Machine];
				ActionHandles.Add({ Pawn, Pawn->OnActionStateChanged.AddLambda([this, Machine, Role, World](AStealthPawn* P)
				{
					FSeen& S = Seen.AddDefaulted_GetRef();
					S.Run = Run; S.Machine = Machine; S.Role = Role; S.Action = P->GetActionState(); S.ServerTime = ServerNow(World);
				}) });
				DeathHandles.Add({ Pawn, Pawn->OnDeathStateChanged.AddLambda([this, Machine, Role, World](AStealthPawn* P)
				{
					FSeen& S = Seen.AddDefaulted_GetRef();
					S.Run = Run; S.Machine = Machine; S.Role = Role; S.bDeath = true; S.Death = P->GetDeathState(); S.ServerTime = ServerNow(World);
				}) });
			}
		}
	}

	void Unbind()
	{
		for (TPair<TWeakObjectPtr<AStealthPawn>, FDelegateHandle>& Pair : ActionHandles)
		{
			if (AStealthPawn* Pawn = Pair.Key.Get()) { Pawn->OnActionStateChanged.Remove(Pair.Value); }
		}
		for (TPair<TWeakObjectPtr<AStealthPawn>, FDelegateHandle>& Pair : DeathHandles)
		{
			if (AStealthPawn* Pawn = Pair.Key.Get()) { Pawn->OnDeathStateChanged.Remove(Pair.Value); }
		}
		ActionHandles.Reset();
		DeathHandles.Reset();
	}

	/** Places this run's target and moves the client players well away from the strike. */
	void PlaceTarget()
	{
		using namespace OutcomeBlindWirePieData;
		UWorld* Server = ServerWorld();
		AStealthPawn* A = Attacker.Get();
		const FVector Forward = A->GetActorForwardVector().GetSafeNormal2D();
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
		StrikePoint = A->GetActorLocation() + Forward * StrikeAheadCm;
		TargetAt = Run == static_cast<int32>(ERun::Whiff) ? StrikePoint + Right * WhiffAsideCm : StrikePoint;
		if (AStealthPawn* Previous = Target.Get())
		{
			Previous->Destroy(); // the last run's body would stand at the strike point
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Target = Server->SpawnActor<AStealthPawn>(AStealthPawn::StaticClass(), TargetAt, A->GetActorRotation(), Params);
		int32 Index = 0;
		for (FConstPlayerControllerIterator It = Server->GetPlayerControllerIterator(); It; ++It)
		{
			APawn* Player = It->Get() != nullptr ? It->Get()->GetPawn() : nullptr;
			if (Player != nullptr && Player != A)
			{
				Player->TeleportTo(A->GetActorLocation() - Forward * 900.0 + Right * (Index++ == 0 ? 400.0 : -400.0), Player->GetActorRotation());
			}
		}
	}

	bool FindCopies()
	{
		using namespace OutcomeBlindWirePieData;
		AttackerCopies.Reset();
		TargetCopies.Reset();
		const TArray<UWorld*> All = Worlds();
		for (int32 Machine = 1; Machine < All.Num(); ++Machine)
		{
			AStealthPawn* A = NearestPawn(All[Machine], Attacker->GetActorLocation(), 10.0);
			AStealthPawn* T = Target.IsValid() ? NearestPawn(All[Machine], Target->GetActorLocation(), 10.0) : nullptr;
			if (A == nullptr || T == nullptr || A == T)
			{
				return false;
			}
			AttackerCopies.Add(A);
			TargetCopies.Add(T);
		}
		return AttackerCopies.Num() == All.Num() - 1 && All.Num() == 3;
	}

	/** Moves the host to a spot whose strike point is far from every prototype bush (rule 2 would make the strike a bush strike). */
	void MoveAttackerClearOfBushes()
	{
		using namespace OutcomeBlindWirePieData;
		UWorld* Server = ServerWorld();
		AStealthPawn* A = Attacker.Get();
		const FVector Origin = A->GetActorLocation();
		const FVector Forward = A->GetActorForwardVector().GetSafeNormal2D();
		for (int32 Ring = 0; Ring < 8; ++Ring)
		{
			for (int32 Step = 0; Step < 8; ++Step)
			{
				const FVector Candidate = Origin + FRotator(0.0, Step * 45.0, 0.0).RotateVector(FVector(Ring * 400.0, 0.0, 0.0));
				bool bClear = true;
				for (TActorIterator<AStealthBush> It(Server); It && bClear; ++It)
				{
					bClear = FVector::Dist2D(It->GetActorLocation(), Candidate) > 600.0 && FVector::Dist2D(It->GetActorLocation(), Candidate + Forward * StrikeAheadCm) > 600.0;
				}
				FHitResult Floor;
				const bool bGround = Server->LineTraceSingleByChannel(Floor, Candidate + FVector(0.0, 0.0, 200.0), Candidate - FVector(0.0, 0.0, 800.0), ECC_Visibility);
				if (bClear && bGround)
				{
					A->TeleportTo(Candidate, A->GetActorRotation());
					TestRunner->AddInfo(FString::Printf(TEXT("ASNPIE attacker at %s (ring %d)"), *Candidate.ToCompactString(), Ring));
					return;
				}
			}
		}
		TestRunner->AddWarning(TEXT("ASNPIE no bush-free spot found near the host; striking where it stands"));
	}

	void Strike()
	{
		FAttackPayload Payload;
		Payload.WorldPoint = StrikePoint; // a TargetSpot: no PawnRef
		Attacker->GetPawnCommand()->Attack(Payload);
		Mark = OutcomeBlindWirePieData::ServerNow(OutcomeBlindWirePieData::ServerWorld());
	}

	void CheckWarpTargets()
	{
		bServerHasWarpTarget |= Attacker.IsValid() && Attacker->HasStrikeWarpTarget();
		for (const TWeakObjectPtr<AStealthPawn>& Copy : AttackerCopies)
		{
			bClientHasServerWarpTarget |= Copy.IsValid() && Copy->HasStrikeWarpTarget();
		}
	}

	/** Every live server pawn: each motion-warping component is the Shared one (COND_Never class) or not replicated. */
	int32 CountReplicatingWarping() const
	{
		int32 Bad = 0;
		for (TActorIterator<AStealthPawn> It(OutcomeBlindWirePieData::ServerWorld()); It; ++It)
		{
			Bad += It->CountWarpingThatCanReplicateTargets();
		}
		return Bad;
	}

	void QueueRun(int32 Index)
	{
		using namespace OutcomeBlindWirePieData;
		Network
			.Then(L(FString::Printf(TEXT("%s: place the target"), RunName(Index))), [this, Index]() { Run = Index; PlaceTarget(); Mark = ServerNow(ServerWorld()); })
			.Until(L(FString::Printf(TEXT("%s: every client has the target"), RunName(Index))), [this]() { return ServerNow(ServerWorld()) >= Mark + 0.6 && FindCopies(); })
			.Then(L(FString::Printf(TEXT("%s: brace (reversal) and bind the recorder"), RunName(Index))), [this, Index]()
			{
				Bind();
				if (Index == static_cast<int32>(ERun::Reversal))
				{
					Target->GetPawnCommand()->PrepareBegin();
				}
				Mark = ServerNow(ServerWorld());
			})
			.Until(L(FString::Printf(TEXT("%s: brace settles"), RunName(Index))), [this]() { return ServerNow(ServerWorld()) >= Mark + 0.3; })
			.Then(L(FString::Printf(TEXT("%s: strike"), RunName(Index))), [this]() { Strike(); })
			.Until(L(FString::Printf(TEXT("%s: observe"), RunName(Index))), [this]()
			{
				CheckWarpTargets();
				return ServerNow(ServerWorld()) >= Mark + ObserveSec;
			})
			.Then(L(FString::Printf(TEXT("%s: unbind"), RunName(Index))), [this]() { Unbind(); Mark = ServerNow(ServerWorld()); })
			.Until(L(FString::Printf(TEXT("%s: lockout ends"), RunName(Index))), [this]() { return ServerNow(ServerWorld()) >= Mark + LockoutWaitSec; });
	}

	TEST_METHOD(SlotWriteBlindDeathWriteDeferredOnEveryClient)
	{
		using namespace OutcomeBlindWirePieData;
		Network
			.Until(TEXT("the round is Active and the host and both clients have pawns"), [this]()
			{
				UWorld* Server = ServerWorld();
				if (!RoundActive() || Server == nullptr || Server->GetFirstPlayerController() == nullptr)
				{
					return false;
				}
				int32 Pawns = 0;
				for (FConstPlayerControllerIterator It = Server->GetPlayerControllerIterator(); It; ++It)
				{
					Pawns += It->Get() != nullptr && It->Get()->GetPawn() != nullptr ? 1 : 0;
				}
				Attacker = Cast<AStealthPawn>(Server->GetFirstPlayerController()->GetPawn());
				return Pawns == 3 && Attacker.IsValid();
			})
			.Then(TEXT("move the host clear of the bushes"), [this]()
			{
				MoveAttackerClearOfBushes();
				if (UActionHub* Hub = UActionHub::Get(ServerWorld()))
				{
					HubHandle = Hub->OnActionStarted.AddLambda([this](AStealthPawn* Pawn, EActionType Type)
					{
						if (Type == EActionType::Attack && Run >= 0 && Run < 3)
						{
							++HubAttackStarts[Run];
						}
					});
				}
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("1 s"), [this]() { return ServerNow(ServerWorld()) >= Mark + 1.0; });
		QueueRun(static_cast<int32>(ERun::Kill));
		QueueRun(static_cast<int32>(ERun::Whiff));
		QueueRun(static_cast<int32>(ERun::Reversal));
		Network.Then(TEXT("assert"), [this]()
		{
			const FEncounterParams Params = FEncounterParams::Defaults();
			TestRunner->AddInfo(FString::Printf(TEXT("ASNPIE recorded %d presentations over 3 runs"), Seen.Num()));
			for (const FSeen& S : Seen)
			{
				TestRunner->AddInfo(FString::Printf(TEXT("ASNPIE run %d machine %d role %d %s pose %d seq %u start %.3f contact %.3f death %.3f at %.3f"), S.Run, S.Machine, S.Role,
					S.bDeath ? TEXT("death") : TEXT("action"), static_cast<int32>(S.Action.Pose), S.Action.Seq, S.Action.StartServerTime, S.Action.ContactServerTime, S.Death.DeathServerTime, S.ServerTime));
			}
			if (const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(ServerWorld()); Root != nullptr && Root->GetEncounterService() != nullptr && Root->GetEncounterService()->GetCore() != nullptr)
			{
				for (const FAttackResolution& R : Root->GetEncounterService()->GetCore()->GetResolutions())
				{
					TestRunner->AddInfo(FString::Printf(TEXT("ASNPIE resolution: outcome %d rule %d victim %s exec %.3f contact %.3f inreach %d"), static_cast<int32>(R.Outcome), static_cast<int32>(R.Rule),
						R.Victim != nullptr ? *R.Victim->GetName() : TEXT("none"), R.ExecServerTime, R.ContactServerTime, R.Reach.bInReach ? 1 : 0));
				}
				for (const FEncounterCommandReply& R : Root->GetEncounterService()->GetCore()->GetReplies())
				{
					TestRunner->AddInfo(FString::Printf(TEXT("ASNPIE reply: kind %d executed %d code %d"), static_cast<int32>(R.Kind), R.bExecuted ? 1 : 0, static_cast<int32>(R.Code)));
				}
			}
			const FSeen* Reference = nullptr;
			for (int32 RunIndex = 0; RunIndex < 3; ++RunIndex)
			{
				const TCHAR* N = RunName(RunIndex);
				for (int32 Machine = 0; Machine < 3; ++Machine)
				{
					const FSeen* Strike = Seen.FindByPredicate([&](const FSeen& S) { return S.Run == RunIndex && S.Machine == Machine && S.Role == 0 && !S.bDeath && S.Action.Pose == EActionPose::Strike; });
					ASSERT_THAT(IsNotNull(Strike, L(FString::Printf(TEXT("%s: machine %d presents the attacker's Strike"), N, Machine))));
					if (Reference == nullptr)
					{
						Reference = Strike;
					}
					ASSERT_THAT(IsTrue(Strike->Action.Variant == Reference->Action.Variant && Strike->Action.WarpPoint == Reference->Action.WarpPoint
						&& FMath::IsNearlyEqual(Strike->Action.ContactServerTime - Strike->Action.StartServerTime, Reference->Action.ContactServerTime - Reference->Action.StartServerTime, 1e-6),
						L(FString::Printf(TEXT("%s: machine %d: same Variant, WarpPoint and contact delay as the kill on the host"), N, Machine))));
					const double DeathWriteTime = Strike->Action.ContactServerTime - Params.Server.StrikeNetLeadSec;
					const TArray<FSeen> OnTarget = Seen.FilterByPredicate([&](const FSeen& S) { return S.Run == RunIndex && S.Machine == Machine && S.Role == 1; });
					const TArray<FSeen> Outcome = Seen.FilterByPredicate([&](const FSeen& S) { return S.Run == RunIndex && S.Machine == Machine && (S.bDeath || S.Action.Pose == EActionPose::Counter); });
					for (const FSeen& S : OnTarget)
					{
						if (!S.bDeath && S.Action.Pose == EActionPose::Brace)
						{
							continue; // the reversal's brace, presented before the strike
						}
						// Measured on this machine's own clock from its Strike receipt (a client's server-time estimate lags the host's).
						ASSERT_THAT(IsTrue(S.ServerTime - Strike->ServerTime >= DeathWriteTime - Strike->Action.StartServerTime - FrameSec,
							L(FString::Printf(TEXT("%s: machine %d: nothing reaches the target before the death write (%.3f s after the Strike; T_dw is %.3f s after the slot)"),
								N, Machine, S.ServerTime - Strike->ServerTime, DeathWriteTime - Strike->Action.StartServerTime))));
					}
					if (RunIndex == static_cast<int32>(ERun::Whiff))
					{
						ASSERT_THAT(IsTrue(Outcome.IsEmpty(), L(FString::Printf(TEXT("whiff: machine %d presents no FDeathState and no Counter"), Machine))));
						continue;
					}
					const int32 FallenRole = RunIndex == static_cast<int32>(ERun::Kill) ? 1 : 0;
					const FSeen* Death = Outcome.FindByPredicate([FallenRole](const FSeen& S) { return S.bDeath && S.Role == FallenRole; });
					ASSERT_THAT(IsNotNull(Death, L(FString::Printf(TEXT("%s: machine %d presents FDeathState on the %s"), N, Machine, FallenRole == 1 ? TEXT("victim") : TEXT("attacker")))));
					TestRunner->AddInfo(FString::Printf(TEXT("ASNPIE %s machine %d: strike seen %.3f, death seen %.3f (T_dw %.3f, contact %.3f)"), N, Machine, Strike->ServerTime, Death->ServerTime, DeathWriteTime, Strike->Action.ContactServerTime));
					ASSERT_THAT(IsTrue(Death->Death.DeathServerTime == Strike->Action.ContactServerTime, L(FString::Printf(TEXT("%s: machine %d: DeathServerTime = ContactServerTime"), N, Machine))));
					ASSERT_THAT(IsTrue(Death->ServerTime - Strike->ServerTime >= DeathWriteTime - Strike->Action.StartServerTime - FrameSec,
						L(FString::Printf(TEXT("%s: machine %d: the death arrives from the death write on"), N, Machine))));
					if (RunIndex == static_cast<int32>(ERun::Reversal))
					{
						ASSERT_THAT(IsTrue(Outcome.ContainsByPredicate([](const FSeen& S) { return !S.bDeath && S.Role == 1 && S.Action.Pose == EActionPose::Counter; }),
							L(FString::Printf(TEXT("reversal: machine %d presents Counter on the defender"), Machine))));
						ASSERT_THAT(IsFalse(Outcome.ContainsByPredicate([](const FSeen& S) { return S.bDeath && S.Role == 1; }),
							L(FString::Printf(TEXT("reversal: machine %d: the defender never dies (never Disabled)"), Machine))));
					}
				}
			}
			TestRunner->AddInfo(FString::Printf(TEXT("ASNPIE warp targets: server set %d, a client copy holds one %d; replicating motion-warping components on live pawns %d"),
				bServerHasWarpTarget ? 1 : 0, bClientHasServerWarpTarget ? 1 : 0, CountReplicatingWarping()));
			TestRunner->AddInfo(FString::Printf(TEXT("ASNPIE hub OnActionStarted(Attack) per run: %d %d %d"), HubAttackStarts[0], HubAttackStarts[1], HubAttackStarts[2]));
			if (UActionHub* Hub = UActionHub::Get(ServerWorld()))
			{
				Hub->OnActionStarted.Remove(HubHandle);
			}
			ASSERT_THAT(IsTrue(HubAttackStarts[0] == 1 && HubAttackStarts[1] == 1 && HubAttackStarts[2] == 1,
				TEXT("the slot announces each strike on the hub exactly once (the crowd's R8 stimulus; no prototype double broadcast)")));
			ASSERT_THAT(IsTrue(bServerHasWarpTarget, TEXT("the server set its local StrikeContact warp target")));
			ASSERT_THAT(IsFalse(bClientHasServerWarpTarget, TEXT("no client received a strike warp target over the wire")));
			ASSERT_THAT(IsTrue(CountReplicatingWarping() == 0, TEXT("no live pawn has a motion-warping component that can send WarpTargets")));
			Unbind();
		});
	}
};

#endif
