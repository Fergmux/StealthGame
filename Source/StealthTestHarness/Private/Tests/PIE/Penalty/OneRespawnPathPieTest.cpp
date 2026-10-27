// NPC-Kill Penalty story nkp-014, AC1 / AC-NKP-09 (PEN-R7, R9; ADR-0019 section 4; ADR-0008 T2): listen server + 2 clients
// on L_Prototype WITH a crowd (StealthPenaltyPie::CrowdNpcCount), the three players split 2v1:
// StealthGame.Penalty.PIE.OneRespawnPath.<Cause>. For each cause the informant's body is provided through the one commit
// path and, over the whole respawn:
//   - Penalty's path spawns no pawn: no pawn spawned since the cause is bound to any participant, and the harness-only
//     crowd-less spawn (StealthPenaltyHarnessSeams; off in these rounds) never ran (Crowd's own replacement emergence for a
//     dead body is Crowd's AC and is not counted);
//   - the body existed before T_due (a pre-existing crowd pawn, never a test-spawned NPC or an old body of the participant)
//     and lies in the team's respawn region at the level the pick used (the documented stand-in region of
//     FStealthPenaltyRespawnGateway until Level Socket Contract story 033);
//   - no opposing participant's pawn had a line of sight to it at the commit tick (the stand-in LSC sight query re-run at the
//     takeover, recorded by the respawn probe);
//   - every commit attempt made exactly one pick (one RespawnPick draw), all of one request (one RequestSeq); exactly one
//     takeover answered Applied and exactly one card move followed it, in its own attempt; a failed release redraws in a
//     later frame and moves no card (FailedReleaseRedrawsWithoutCardMove).
// Causes: Assassinated (the solo player kills a pair player), ReversedAgainst (a pair player strikes the braced solo),
// StrikeLimit (a pair player's L-th NPC kill exiles it), InformantKilled (the respawned informant is killed: it respawns
// again), ReturningInformant (an awaiting informant disconnects and returns: Round R18 readmits it; T_due is kept, never
// shortened) and BodyLostToKillZ (an embodied informant's body falls below KillZ: D = 0).
// Stand-ins (logged in the story): the connection drop and return are the Roster's NotifyConnectionChanged calls the
// composition root makes on logout and reclaim (Session's PIE rejoin reclaim, session-travel stories 026-027, is not built);
// the KillZ report is UPenaltyService::OnInformantPawnLost, the call Round R17.9 makes (round-match-framework rmf-072).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/IConsoleManager.h"
#include "Server/StealthServerTuning.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Penalty/PenaltyPieSupport.h"

namespace OneRespawnPathPieData
{
	constexpr double SettleSec = 0.6;
	/** Past AttackAttemptLockoutSec (1.75 s) and the contact. */
	constexpr double StepSec = 2.4;
	/** A respawned body settles this long before it is struck or dropped (every client has it). */
	constexpr double BodySettleSec = 1.0;
	/** ReturningInformant: the drop this long after the kill, the return this long after the drop (both before T_due). */
	constexpr double DropAfterSec = 2.0;
	constexpr double ReturnAfterSec = 3.0;
	/** KillZ: the body is put this far below the world's KillZ. */
	constexpr double BelowKillZCm = 2000.0;
}

NETWORK_TEST_CLASS(OneRespawnPath, "StealthGame.Penalty.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	TWeakObjectPtr<AStealthPlayerController> Solo;
	TWeakObjectPtr<AStealthPlayerController> Pair[2];
	TWeakObjectPtr<AStealthPawn> SoloPawn;
	/** The participant whose respawn is measured, its team, and every body it had so far. */
	FParticipantId Victim;
	ERosterTeam VictimTeam = ERosterTeam::None;
	TArray<TWeakObjectPtr<APawn>> OldBodies;
	/** Pawns the test spawned (NPC victims): never a respawn body. */
	TArray<TWeakObjectPtr<APawn>> TestSpawned;
	StealthPenaltyPie::FSpawnTracker Spawns;
	double CauseTime = 0.0;
	double Due = 0.0;
	double DueBeforeDrop = 0.0;
	int32 HarnessSpawnsAtStart = 0;
	uint32 Seq = 0x20000000u;
	double Mark = 0.0;
	double BodySeenAt = -1.0;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(StealthAssassinationPie::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
		}
		UStealthSettings* Settings = GetMutableDefault<UStealthSettings>();
		PreviousNpcCount = Settings->NpcCount;
		Settings->NpcCount = StealthPenaltyPie::CrowdNpcCount;
		// The production path: the harness-only crowd-less respawn stays off (a crowd exists anyway).
		StealthPenaltyHarnessSeams::SetCrowdlessTeamStartRespawn(false);
	}

	AFTER_ALL()
	{
		StealthPenaltyHarnessSeams::SetRespawnProbe(false);
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

	AFTER_EACH()
	{
		Spawns.Stop();
		StealthPenaltyHarnessSeams::SetRespawnProbe(false);
	}

	AStealthPawn* PawnOf(const TWeakObjectPtr<AStealthPlayerController>& Controller) const
	{
		return Controller.IsValid() ? Cast<AStealthPawn>(Controller->GetPawn()) : nullptr;
	}

	double Now() const
	{
		return StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld());
	}

	void Wait(const TCHAR* Label, double Seconds)
	{
		Network
			.Then(*FString::Printf(TEXT("%s: mark"), Label), [this]() { Mark = Now(); })
			.Until(Label, [this, Seconds]() { return Now() >= Mark + Seconds; });
	}

	/** The round is Active, the players split 2v1; the solo player stands clear of the bushes, the pair 12 m to each side. */
	void SetupRound()
	{
		using namespace StealthAssassinationPie;
		Network
			.Until(TEXT("the round is Active, the crowd is up and the players split 2v1"), [this]()
			{
				AStealthPlayerController* S = nullptr;
				AStealthPlayerController* P[2] = { nullptr, nullptr };
				if (!RoundActive() || !SplitTeams(S, P) || StealthPenaltyPie::Penalty() == nullptr || StealthPenaltyPie::Roster() == nullptr)
				{
					return false;
				}
				int32 Crowd = 0;
				for (TActorIterator<AStealthPawn> It(ServerWorld()); It; ++It)
				{
					Crowd += StealthPenaltyPie::Roster()->IsPawnBound(*It) ? 0 : 1;
				}
				Solo = S;
				Pair[0] = P[0];
				Pair[1] = P[1];
				SoloPawn = Cast<AStealthPawn>(S->GetPawn());
				return SoloPawn.IsValid() && Crowd >= StealthPenaltyPie::CrowdNpcCount / 2;
			})
			.Then(TEXT("place the players; start the spawn tracker and the probe"), [this]()
			{
				MoveClearOfBushes(SoloPawn.Get());
				const FVector Right = FVector::CrossProduct(FVector::UpVector, SoloPawn->GetActorForwardVector().GetSafeNormal2D());
				PawnOf(Pair[0])->TeleportTo(SoloPawn->GetActorLocation() + Right * 1200.0, PawnOf(Pair[0])->GetActorRotation());
				PawnOf(Pair[1])->TeleportTo(SoloPawn->GetActorLocation() - Right * 1200.0, PawnOf(Pair[1])->GetActorRotation());
				Spawns.Start(ServerWorld());
				HarnessSpawnsAtStart = StealthPenaltyHarnessSeams::GetHarnessSpawnCount();
				StealthPenaltyHarnessSeams::SetRespawnProbe(true);
				Mark = Now();
			})
			.Until(TEXT("settle"), [this]() { return Now() >= Mark + OneRespawnPathPieData::SettleSec; });
	}

	/** Names the victim (its participant, team and current body) and restarts the probe for its respawn. */
	void BeginRespawn(APlayerController* Controller)
	{
		Victim = StealthPenaltyPie::ParticipantOf(Controller);
		VictimTeam = StealthPenaltyPie::TeamOf(Victim);
		if (APawn* Body = StealthPenaltyPie::BodyOf(Victim))
		{
			OldBodies.AddUnique(Body);
		}
		StealthPenaltyHarnessSeams::SetRespawnProbe(true);
		CauseTime = Now();
	}

	/** The solo player kills the pair player Index (Assassinated). */
	void Assassinate(int32 Index)
	{
		Network.Then(TEXT("the solo player kills a pair player"), [this, Index]()
		{
			AStealthPawn* Target = PawnOf(Pair[Index]);
			BeginRespawn(Pair[Index].Get());
			StealthAssassinationPie::PlaceFacing(Target, SoloPawn.Get(), 150.0);
			StealthAssassinationPie::StrikeAt(SoloPawn.Get(), Target, Seq);
		});
	}

	/** The solo player braces and the pair player Index strikes it (ReversedAgainst for the striker). */
	void Reverse(int32 Index)
	{
		Network
			.Then(TEXT("the solo player braces, the pair player comes in front"), [this, Index]()
			{
				StealthAssassinationPie::AsOwner(SoloPawn.Get(), Seq, [](UPawnCommandComponent& Command) { Command.PrepareBegin(); });
				StealthAssassinationPie::PlaceStrikerFacing(PawnOf(Pair[Index]), SoloPawn.Get(), 150.0);
				Mark = Now();
			})
			.Until(TEXT("inside the brace window"), [this]() { return Now() >= Mark + 0.4; })
			.Then(TEXT("the pair player strikes the braced solo player"), [this, Index]()
			{
				BeginRespawn(Pair[Index].Get());
				StealthAssassinationPie::StrikeAt(PawnOf(Pair[Index]), SoloPawn.Get(), Seq);
			});
	}

	/** The pair player Index kills L unbound NPCs: the L-th exiles it (StrikeLimit). */
	void Exile(int32 Index)
	{
		const int32 Limit = FPenaltyParams::FromTuning(FStealthServerTuning()).NpcKillStrikeLimit;
		Network.Then(TEXT("the exiled-to-be player stands clear of the bushes"), [this, Index]() { StealthAssassinationPie::MoveClearOfBushes(PawnOf(Pair[Index])); });
		Wait(TEXT("settle"), OneRespawnPathPieData::SettleSec);
		for (int32 Kill = 0; Kill < Limit; ++Kill)
		{
			Network.Then(*FString::Printf(TEXT("NPC kill %d of %d"), Kill + 1, Limit), [this, Index, Kill, Limit]()
			{
				AStealthPawn* Striker = PawnOf(Pair[Index]);
				if (Kill == Limit - 1)
				{
					BeginRespawn(Pair[Index].Get());
				}
				AStealthPawn* Npc = StealthAssassinationPie::SpawnUnboundPawn(StealthAssassinationPie::ServerWorld(), Striker, 150.0);
				TestSpawned.Add(Npc);
				StealthAssassinationPie::StrikeAt(Striker, Npc, Seq);
			});
			Wait(TEXT("lockout"), OneRespawnPathPieData::StepSec);
		}
	}

	/** Waits until the victim is Awaiting its body, then records T_due. */
	void AwaitDue(const TCHAR* Label)
	{
		Network.Until(Label, [this]()
		{
			const UPenaltyService* Penalty = StealthPenaltyPie::Penalty();
			if (Penalty == nullptr || Penalty->GetLifeState(Victim) != EInformantLifeState::Awaiting || !Penalty->GetDueServerTime(Victim).IsSet())
			{
				return false;
			}
			Due = Penalty->GetDueServerTime(Victim).GetValue();
			return true;
		});
	}

	/** Waits until the victim is Embodied in a body it never had, then lets the body settle. */
	void AwaitBody(const TCHAR* Label)
	{
		Network
			.Then(*FString::Printf(TEXT("%s: mark"), Label), [this]() { BodySeenAt = -1.0; })
			.Until(Label, [this]()
			{
				const UPenaltyService* Penalty = StealthPenaltyPie::Penalty();
				APawn* Body = StealthPenaltyPie::BodyOf(Victim);
				if (Penalty == nullptr || Penalty->GetLifeState(Victim) != EInformantLifeState::Embodied || Body == nullptr || OldBodies.Contains(Body))
				{
					return false;
				}
				if (BodySeenAt < 0.0)
				{
					BodySeenAt = Now();
					// Information only: the same sight test from the first server frame after the commit (the binding check is
					// the probe's commit-tick query; opposing pawns may have moved one frame since).
					TestRunner->AddInfo(FString::Printf(TEXT("NKPPIE first frame after the takeover: seen by an opponent %d"),
						StealthPenaltyPie::IsSeenByOpponents(VictimTeam, Body) ? 1 : 0));
				}
				return Now() >= BodySeenAt + OneRespawnPathPieData::BodySettleSec;
			});
	}

	/** Asserts AC-NKP-09 on the respawn just completed (bExpectFailedRelease: the first picked attempt's release failed). */
	void AssertOneRespawn(const FString& Label, bool bExpectFailedRelease, bool bDueIsLossTime = false)
	{
		Network.Then(*FString::Printf(TEXT("%s: assert"), *Label), [this, Label, bExpectFailedRelease, bDueIsLossTime]()
		{
			using namespace StealthPenaltyPie;
			APawn* Body = BodyOf(Victim);
			const TArray<FAttempt> All = Attempts();
			for (int32 Index = 0; Index < All.Num(); ++Index)
			{
				FString Line;
				for (const FProbeEvent& Event : All[Index].Events)
				{
					Line += FString::Printf(TEXT(" %d@%llu"), static_cast<int32>(Event.Step), Event.Frame);
				}
				TestRunner->AddInfo(FString::Printf(TEXT("NKPPIE %s attempt %d:%s"), *Label, Index, *Line));
			}
			TestRunner->AddInfo(FString::Printf(TEXT("NKPPIE %s: cause %.3f T_due %.3f body %s"), *Label, CauseTime, Due, *GetNameSafe(Body)));

			ASSERT_THAT(IsTrue(Body != nullptr, *FString::Printf(TEXT("%s: the informant has a body"), *Label)));
			// No spawn by Penalty's path.
			ASSERT_THAT(IsTrue(StealthPenaltyHarnessSeams::GetHarnessSpawnCount() == HarnessSpawnsAtStart, *FString::Printf(TEXT("%s: the harness-only crowd-less spawn never ran"), *Label)));
			ASSERT_THAT(IsTrue(Spawns.CountBoundSpawnedSince(CauseTime) == 0, *FString::Printf(TEXT("%s: no pawn spawned since the cause is bound to a participant (0 spawns by Penalty's path)"), *Label)));
			// A pre-existing crowd pawn.
			ASSERT_THAT(IsTrue(Spawns.ExistedBefore(Body, Due), *FString::Printf(TEXT("%s: the body existed before T_due"), *Label)));
			ASSERT_THAT(IsTrue(!TestSpawned.Contains(Body) && Cast<AStealthPawn>(Body) != nullptr, *FString::Printf(TEXT("%s: the body is a crowd Shared Pawn (not a test NPC)"), *Label)));
			// The attempts.
			int32 Applied = 0;
			int32 Cards = 0;
			bool bOnePickEach = All.Num() > 0;
			bool bOneRequest = All.Num() > 0;
			bool bCardAfterApplied = true;
			bool bNoCardOnFailure = true;
			const FAttempt* Committed = nullptr;
			const FAttempt* FirstPicked = nullptr;
			for (const FAttempt& Attempt : All)
			{
				bOnePickEach &= Attempt.Count(EProbeStep::Pick) == 1 && Attempt.Events[0].Step == EProbeStep::Pick;
				bOneRequest &= Attempt.Events[0].RequestSeq == All[0].Events[0].RequestSeq && Attempt.Events[0].RequestSeq != 0;
				const FProbeEvent* Takeover = Attempt.Find(EProbeStep::Takeover);
				const bool bApplied = Takeover != nullptr && Takeover->TakeoverResult == EPenaltyTakeoverResult::Applied;
				Applied += bApplied ? 1 : 0;
				Cards += Attempt.Count(EProbeStep::Card);
				if (bApplied)
				{
					Committed = &Attempt;
					const int32 TakeoverAt = Attempt.Events.IndexOfByPredicate([](const FProbeEvent& E) { return E.Step == EProbeStep::Takeover; });
					const int32 CardAt = Attempt.Events.IndexOfByPredicate([](const FProbeEvent& E) { return E.Step == EProbeStep::Card; });
					bCardAfterApplied &= CardAt > TakeoverAt && Attempt.Count(EProbeStep::Card) == 1 && Attempt.Events[CardAt].Participant == Victim;
				}
				else
				{
					bNoCardOnFailure &= Attempt.Count(EProbeStep::Card) == 0;
				}
				if (FirstPicked == nullptr && Attempt.Events[0].PickStatus == EPenaltyPickStatus::Picked)
				{
					FirstPicked = &Attempt;
				}
			}
			ASSERT_THAT(IsTrue(bOnePickEach, *FString::Printf(TEXT("%s: every commit attempt made exactly one pick (one RespawnPick draw)"), *Label)));
			ASSERT_THAT(IsTrue(bOneRequest, *FString::Printf(TEXT("%s: every pick is of the same request (one RespawnRequestSeq)"), *Label)));
			ASSERT_THAT(IsTrue(Applied == 1 && Committed != nullptr, *FString::Printf(TEXT("%s: exactly one takeover answered Applied"), *Label)));
			ASSERT_THAT(IsTrue(Cards == 1 && bCardAfterApplied && bNoCardOnFailure, *FString::Printf(TEXT("%s: exactly one card move, after the Applied takeover, none in a failed attempt"), *Label)));
			const FProbeEvent& Pick = Committed->Events[0];
			const FProbeEvent* Takeover = Committed->Find(EProbeStep::Takeover);
			ASSERT_THAT(IsTrue(Pick.Pawn.Get() == Body && Takeover->Pawn.Get() == Body && Takeover->Participant == Victim,
				*FString::Printf(TEXT("%s: the body is the pawn the committed pick returned"), *Label)));
			ASSERT_THAT(IsTrue(Takeover->bUnseenAtCommit, *FString::Printf(TEXT("%s: no opposing participant's pawn had a line of sight at the commit tick"), *Label)));
			ASSERT_THAT(IsTrue(IsInRespawnRegion(VictimTeam, Takeover->Location, Pick.WideningLevel),
				*FString::Printf(TEXT("%s: the body is in the team's respawn region (widening level %d)"), *Label, Pick.WideningLevel)));
			ASSERT_THAT(IsTrue(Takeover->ServerTime >= Due - 0.05, *FString::Printf(TEXT("%s: the commit ran at or after T_due"), *Label)));
			if (bDueIsLossTime)
			{
				ASSERT_THAT(IsTrue(Due - CauseTime < 1.0, *FString::Printf(TEXT("%s: a body lost without a kill is due at once (D = 0)"), *Label)));
			}
			if (bExpectFailedRelease)
			{
				ASSERT_THAT(IsTrue(FirstPicked != nullptr && FirstPicked != Committed && FirstPicked->Find(EProbeStep::Release) != nullptr
					&& !FirstPicked->Find(EProbeStep::Release)->bOk && FirstPicked->Count(EProbeStep::Takeover) == 0 && FirstPicked->Count(EProbeStep::Card) == 0,
					*FString::Printf(TEXT("%s: the first picked attempt's release failed: no takeover, no card"), *Label)));
				ASSERT_THAT(IsTrue(Pick.Frame > FirstPicked->Events[0].Frame, *FString::Printf(TEXT("%s: the redraw ran in a later frame"), *Label)));
			}
			OldBodies.AddUnique(Body);
		});
	}

	TEST_METHOD(Assassinated)
	{
		SetupRound();
		Assassinate(1);
		AwaitDue(TEXT("the assassinated pair player awaits its body"));
		AwaitBody(TEXT("the informant takes over a crowd pawn"));
		AssertOneRespawn(TEXT("Assassinated"), false);
	}

	TEST_METHOD(ReversedAgainst)
	{
		SetupRound();
		Reverse(1);
		AwaitDue(TEXT("the reversed attacker awaits its body"));
		AwaitBody(TEXT("the informant takes over a crowd pawn"));
		AssertOneRespawn(TEXT("ReversedAgainst"), false);
	}

	TEST_METHOD(StrikeLimit)
	{
		SetupRound();
		Exile(0);
		AwaitDue(TEXT("the exiled pair player awaits its body"));
		AwaitBody(TEXT("the exiled informant takes over a crowd pawn (never its released body)"));
		AssertOneRespawn(TEXT("StrikeLimit"), false);
	}

	TEST_METHOD(InformantKilled)
	{
		SetupRound();
		Assassinate(1);
		AwaitDue(TEXT("the assassinated pair player awaits its body"));
		AwaitBody(TEXT("the first respawn"));
		Network.Then(TEXT("the solo player kills the informant"), [this]()
		{
			APawn* Body = StealthPenaltyPie::BodyOf(Victim);
			BeginRespawn(Pair[1].Get());
			StealthAssassinationPie::PlaceFacing(Body, SoloPawn.Get(), 150.0);
			StealthAssassinationPie::StrikeAt(SoloPawn.Get(), Body, Seq);
		});
		AwaitDue(TEXT("the killed informant awaits its next body"));
		AwaitBody(TEXT("the killed informant takes over another crowd pawn"));
		AssertOneRespawn(TEXT("InformantKilled"), false);
	}

	TEST_METHOD(ReturningInformant)
	{
		SetupRound();
		Assassinate(1);
		AwaitDue(TEXT("the assassinated pair player awaits its body"));
		Network
			.Then(TEXT("mark"), [this]() { Mark = Now(); DueBeforeDrop = Due; })
			.Until(TEXT("before the drop"), [this]() { return Now() >= CauseTime + OneRespawnPathPieData::DropAfterSec; })
			.Then(TEXT("the awaiting informant drops (Roster Disconnected, the logout's projection)"), [this]()
			{
				StealthPenaltyPie::Roster()->NotifyConnectionChanged(Victim, ERosterConnection::Disconnected);
			})
			.Until(TEXT("Penalty: Absent (the item cancelled, t_lost kept)"), [this]()
			{
				const UPenaltyService* Penalty = StealthPenaltyPie::Penalty();
				return Penalty != nullptr && Penalty->GetLifeState(Victim) == EInformantLifeState::Absent && !Penalty->GetDueServerTime(Victim).IsSet();
			})
			.Then(TEXT("mark the drop"), [this]() { Mark = Now(); })
			.Until(TEXT("before the return"), [this]() { return Now() >= Mark + OneRespawnPathPieData::ReturnAfterSec; })
			.Then(TEXT("the informant returns (Roster Connected, the reclaim's projection: Round R18 readmits it)"), [this]()
			{
				StealthPenaltyPie::Roster()->NotifyConnectionChanged(Victim, ERosterConnection::Connected);
			});
		AwaitDue(TEXT("Round R18 readmitted it: Awaiting again"));
		Network.Then(TEXT("T_due kept"), [this]()
		{
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Due, DueBeforeDrop, 0.001), *FString::Printf(TEXT("ReturningInformant: the readmitted T_due %.3f is the kept T_due %.3f (never shortened, never restarted)"), Due, DueBeforeDrop)));
		});
		AwaitBody(TEXT("the returning informant takes over a crowd pawn"));
		AssertOneRespawn(TEXT("ReturningInformant"), false);
	}

	TEST_METHOD(BodyLostToKillZ)
	{
		SetupRound();
		Assassinate(1);
		AwaitDue(TEXT("the assassinated pair player awaits its body"));
		AwaitBody(TEXT("the first respawn"));
		Network
			.Then(TEXT("the informant's body falls below KillZ"), [this]()
			{
				APawn* Body = StealthPenaltyPie::BodyOf(Victim);
				BeginRespawn(Pair[1].Get());
				const AWorldSettings* Settings = StealthAssassinationPie::ServerWorld()->GetWorldSettings();
				const FVector Where = Body->GetActorLocation();
				Body->SetActorLocation(FVector(Where.X, Where.Y, Settings->KillZ - OneRespawnPathPieData::BelowKillZCm), false, nullptr, ETeleportType::TeleportPhysics);
			})
			.Until(TEXT("the body is gone (the engine's FellOutOfWorld; the Roster binding cleared)"), [this]()
			{
				return StealthPenaltyPie::BodyOf(Victim) == nullptr;
			})
			.Then(TEXT("Round R17.9 reports the lost informant body (stand-in call, rmf-072)"), [this]()
			{
				Due = Now();
				if (UPenaltyService* Penalty = StealthPenaltyPie::Penalty(); Penalty != nullptr && Penalty->GetLifeState(Victim) == EInformantLifeState::Embodied)
				{
					Penalty->OnInformantPawnLost(Victim);
				}
			});
		AwaitBody(TEXT("the informant takes over a crowd pawn at once"));
		AssertOneRespawn(TEXT("BodyLostToKillZ"), false, /*bDueIsLossTime*/ true);
	}

	TEST_METHOD(FailedReleaseRedrawsWithoutCardMove)
	{
		SetupRound();
		Assassinate(1);
		Network.Then(TEXT("inject one TryReleaseNpc failure"), []() { StealthPenaltyHarnessSeams::InjectReleaseFailures(1); });
		AwaitDue(TEXT("the assassinated pair player awaits its body"));
		AwaitBody(TEXT("the informant takes over a crowd pawn after a redraw"));
		AssertOneRespawn(TEXT("FailedRelease"), true);
	}
};

#endif
