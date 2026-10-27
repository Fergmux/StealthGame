// NPC-Kill Penalty story nkp-015, AC2 / AC-NKP-29 (PEN-R12; ADR-0019 section 5; ADR-0008 T2): listen server + 2 clients on
// L_Prototype WITH a crowd, the three players split 2v1: StealthGame.Penalty.PIE.InformantContract.<method>. The solo player
// kills a pair player, which becomes an embodied informant in a taken-over crowd pawn; then each PEN-R12 row whose owner is
// built is exercised end to end, through the owner's own path (Penalty adds no rule of its own):
//   walk        the owner's W key: the body walks (speed above half of WalkSpeedCmPerSec, never above it);
//   run clamp   the owner's W + Left Shift: the speed never exceeds WalkSpeedCmPerSec (SCL R6 through the gait legality);
//               control: the Active pair player's W + Left Shift is faster than Walk;
//   rejected    Attack and Prepare submitted with the owner's own origin: the owner's machine receives Rejected(NotNow);
//   elimination CountActive of the informant's team counts only the Active teammate; the informant's state is Informant;
//   killed      the solo player kills the informant body: its own result is WrongTarget with the tally + 1 and no exile
//               (an NPC kill's feedback), and the solo player stays Active; the informant awaits a new body;
//   respawn     it takes over another crowd pawn in its team's respawn region and is still an Informant (never Active).
// Rows NOT exercised (their owners are not built; story nkp-015 is Blocked on them): Traverse, Interact and Hide NotNow
// (input-interaction-framework story 057's Informant legality class), a concealed opponent not visible and a concealed
// ally's marker visible (information-visibility-policy's viewer class OpponentInformant and Teammate Markers). The pings
// row belongs to the Pings ACs.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Penalty/PenaltyPieSupport.h"
#include "View/Core/StealthPawnMovementComponent.h"

namespace InformantContractPieData
{
	constexpr double SettleSec = 0.6;
	constexpr double StepSec = 2.4;
	/** How long each movement row drives the owner's keys. */
	constexpr double DriveSec = 2.5;
	constexpr double BodySettleSec = 1.0;
	/** Speed slack over WalkSpeedCmPerSec (network smoothing of the server's own velocity). */
	constexpr double SpeedSlackCmPerSec = 1.0;

	TArray<FCommandReply> RepliesOf(const AStealthPlayerController* Controller, ECommandReplyKind Kind)
	{
		TArray<FCommandReply> Out;
		if (Controller != nullptr)
		{
			for (const FCommandReply& Reply : Controller->GetReceivedReplies())
			{
				if (Reply.Kind == Kind)
				{
					Out.Add(Reply);
				}
			}
		}
		return Out;
	}

	/** A key press or release on a machine's own controller (the production Enhanced Input path, as a real key arrives). */
	void PressKey(AStealthPlayerController* Machine, const FKey& Pressed, bool bDown)
	{
		if (Machine != nullptr)
		{
			Machine->InputKey(FInputKeyEventArgs::CreateSimulated(Pressed, bDown ? IE_Pressed : IE_Released, bDown ? 1.f : 0.f));
		}
	}
}

NETWORK_TEST_CLASS(InformantContract, "StealthGame.Penalty.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	TWeakObjectPtr<AStealthPlayerController> Solo;
	TWeakObjectPtr<AStealthPlayerController> Pair[2];
	TWeakObjectPtr<AStealthPawn> SoloPawn;
	FParticipantId Informant;
	FParticipantId Mate;
	ERosterTeam InformantTeam = ERosterTeam::None;
	TArray<TWeakObjectPtr<APawn>> Bodies;
	uint32 Seq = 0x30000000u;
	double Mark = 0.0;
	double BodySeenAt = -1.0;
	double PeakInformant = 0.0;
	double PeakMate = 0.0;
	uint32 AttackSeq = 0;
	uint32 PrepareSeq = 0;
	int32 SoloTallyBefore = 0;
	int32 SoloStrikesBefore = 0;

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

	double Now() const
	{
		return StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld());
	}

	AStealthPawn* PawnOf(const TWeakObjectPtr<AStealthPlayerController>& Controller) const
	{
		return Controller.IsValid() ? Cast<AStealthPawn>(Controller->GetPawn()) : nullptr;
	}

	AStealthPlayerController* MachineOf(const TWeakObjectPtr<AStealthPlayerController>& Controller) const
	{
		return StealthAssassinationPie::OwnerMachineController(Controller.Get());
	}

	/** Waits until the informant is Embodied in a body it never had, then lets it settle. */
	void AwaitBody(const TCHAR* Label)
	{
		Network
			.Then(*FString::Printf(TEXT("%s: mark"), Label), [this]() { BodySeenAt = -1.0; })
			.Until(Label, [this]()
			{
				const UPenaltyService* Penalty = StealthPenaltyPie::Penalty();
				APawn* Body = StealthPenaltyPie::BodyOf(Informant);
				if (Penalty == nullptr || Penalty->GetLifeState(Informant) != EInformantLifeState::Embodied || Body == nullptr || Bodies.Contains(Body))
				{
					return false;
				}
				BodySeenAt = BodySeenAt < 0.0 ? Now() : BodySeenAt;
				return Now() >= BodySeenAt + InformantContractPieData::BodySettleSec;
			})
			.Then(*FString::Printf(TEXT("%s: record"), Label), [this]() { Bodies.AddUnique(StealthPenaltyPie::BodyOf(Informant)); });
	}

	/** Holds Keys on Machine's controller for DriveSec while sampling Body's server speed into Peak. */
	void Drive(const TCHAR* Label, TFunction<AStealthPlayerController*()> Machine, TFunction<APawn*()> Body, TArray<FKey> Keys, double& Peak)
	{
		Network
			.Then(*FString::Printf(TEXT("%s: press"), Label), [this, Machine, Keys, &Peak]()
			{
				Peak = 0.0;
				for (const FKey& K : Keys)
				{
					InformantContractPieData::PressKey(Machine(), K, true);
				}
				Mark = Now();
			})
			.Until(Label, [this, Body, &Peak]()
			{
				if (const APawn* Pawn = Body())
				{
					Peak = FMath::Max(Peak, Pawn->GetVelocity().Size2D());
				}
				return Now() >= Mark + InformantContractPieData::DriveSec;
			})
			.Then(*FString::Printf(TEXT("%s: release"), Label), [Machine, Keys]()
			{
				for (const FKey& K : Keys)
				{
					InformantContractPieData::PressKey(Machine(), K, false);
				}
			});
	}

	TEST_METHOD(EveryBuiltRowHolds)
	{
		using namespace StealthAssassinationPie;
		using namespace InformantContractPieData;
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
				return SoloPawn.IsValid() && Crowd >= StealthPenaltyPie::CrowdNpcCount / 2 && MachineOf(Solo) && MachineOf(Pair[0]) && MachineOf(Pair[1]);
			})
			.Then(TEXT("place the players; the solo player kills pair player 2"), [this]()
			{
				MoveClearOfBushes(SoloPawn.Get());
				const FVector Right = FVector::CrossProduct(FVector::UpVector, SoloPawn->GetActorForwardVector().GetSafeNormal2D());
				PawnOf(Pair[0])->TeleportTo(SoloPawn->GetActorLocation() + Right * 1200.0, PawnOf(Pair[0])->GetActorRotation());
				Informant = StealthPenaltyPie::ParticipantOf(Pair[1].Get());
				Mate = StealthPenaltyPie::ParticipantOf(Pair[0].Get());
				InformantTeam = StealthPenaltyPie::TeamOf(Informant);
				Bodies.Add(PawnOf(Pair[1]));
				PlaceFacing(PawnOf(Pair[1]), SoloPawn.Get(), 150.0);
				StrikeAt(SoloPawn.Get(), PawnOf(Pair[1]), Seq);
			});
		AwaitBody(TEXT("the killed pair player is an embodied informant"));

		// walk, run clamp (and the Active control).
		Drive(TEXT("walk: the informant's W"), [this]() { return MachineOf(Pair[1]); }, [this]() { return StealthPenaltyPie::BodyOf(Informant); }, { EKeys::W }, PeakInformant);
		Network.Then(TEXT("walk: assert"), [this]()
		{
			const AStealthPawn* Body = Cast<AStealthPawn>(StealthPenaltyPie::BodyOf(Informant));
			ASSERT_THAT(IsTrue(Body != nullptr, TEXT("walk: the informant has its body")));
			const double Walk = Body->GetStealthMovement()->GetLocomotionTuning().WalkSpeedCmPerSec;
			TestRunner->AddInfo(FString::Printf(TEXT("NKPPIE walk: informant peak %.2f cm/s (walk %.2f)"), PeakInformant, Walk));
			ASSERT_THAT(IsTrue(PeakInformant > 0.5 * Walk && PeakInformant <= Walk + InformantContractPieData::SpeedSlackCmPerSec, TEXT("walk: the informant walks at the walking baseline")));
		});
		Drive(TEXT("run clamp: the informant's W + Left Shift"), [this]() { return MachineOf(Pair[1]); }, [this]() { return StealthPenaltyPie::BodyOf(Informant); }, { EKeys::LeftShift, EKeys::W }, PeakInformant);
		Network.Then(TEXT("place the Active mate clear of the bushes"), [this]() { MoveClearOfBushes(PawnOf(Pair[0])); });
		Drive(TEXT("run control: the Active mate's W + Left Shift"), [this]() { return MachineOf(Pair[0]); }, [this]() { return PawnOf(Pair[0]); }, { EKeys::LeftShift, EKeys::W }, PeakMate);
		Network.Then(TEXT("run clamp: assert"), [this]()
		{
			const AStealthPawn* Body = Cast<AStealthPawn>(StealthPenaltyPie::BodyOf(Informant));
			ASSERT_THAT(IsTrue(Body != nullptr, TEXT("run clamp: the informant has its body")));
			const double Walk = Body->GetStealthMovement()->GetLocomotionTuning().WalkSpeedCmPerSec;
			TestRunner->AddInfo(FString::Printf(TEXT("NKPPIE run: informant peak %.2f, Active mate peak %.2f (walk %.2f)"), PeakInformant, PeakMate, Walk));
			ASSERT_THAT(IsTrue(PeakInformant <= Walk + InformantContractPieData::SpeedSlackCmPerSec, TEXT("run clamp: the informant's Run never exceeds the walking speed")));
			ASSERT_THAT(IsTrue(PeakMate > Walk + InformantContractPieData::SpeedSlackCmPerSec, TEXT("run control: an Active player's Run is faster than Walk")));
		});

		// rejected: Attack and Prepare with the informant's own origin.
		Network
			.Then(TEXT("rejected: the informant attacks an NPC, then braces"), [this]()
			{
				AStealthPawn* Body = Cast<AStealthPawn>(StealthPenaltyPie::BodyOf(Informant));
				AStealthPawn* Npc = SpawnUnboundPawn(ServerWorld(), Body, 150.0);
				StrikeAt(Body, Npc, Seq);
				AttackSeq = Seq;
				AsOwner(Body, Seq, [](UPawnCommandComponent& Command) { Command.PrepareBegin(); });
				PrepareSeq = Seq;
				Mark = Now();
			})
			.Until(TEXT("rejected: the replies arrive"), [this]() { return Now() >= Mark + StepSec; })
			.Then(TEXT("rejected: assert"), [this]()
			{
				const TArray<FCommandReply> Rejected = RepliesOf(MachineOf(Pair[1]), ECommandReplyKind::Rejected);
				const auto NotNowFor = [&Rejected](uint32 ForSeq)
				{
					return Rejected.ContainsByPredicate([ForSeq](const FCommandReply& R) { return R.Seq.Seq == ForSeq && R.Code == ECommandRejectCode::NotNow; });
				};
				ASSERT_THAT(IsTrue(NotNowFor(AttackSeq), TEXT("rejected: the informant's Attack is answered NotNow on its own machine")));
				ASSERT_THAT(IsTrue(NotNowFor(PrepareSeq), TEXT("rejected: the informant's Prepare is answered NotNow on its own machine")));
				ASSERT_THAT(IsTrue(RepliesOf(MachineOf(Pair[1]), ECommandReplyKind::StrikeResult).Num() == 0, TEXT("rejected: no strike result reached the informant (nothing executed)")));
			});

		// elimination count.
		Network.Then(TEXT("elimination: CountActive excludes the informant"), [this]()
		{
			const URosterStore* Roster = StealthPenaltyPie::Roster();
			const FParticipantRecord* Record = Roster != nullptr ? Roster->FindRecord(Informant) : nullptr;
			const FParticipantRecord* MateRecord = Roster != nullptr ? Roster->FindRecord(Mate) : nullptr;
			ASSERT_THAT(IsTrue(Record != nullptr && Record->State == EParticipantState::Informant && MateRecord != nullptr && MateRecord->State == EParticipantState::Active,
				TEXT("elimination: the informant is an Informant, its mate Active")));
			ASSERT_THAT(IsTrue(Roster->CountActive(static_cast<int32>(InformantTeam)) == 1, TEXT("elimination: CountActive of the informant's team counts only the Active mate")));
		});

		// killed: the solo player kills the informant body.
		Network
			.Then(TEXT("killed: the solo player kills the informant"), [this]()
			{
				const TArray<FCommandReply> Strikes = RepliesOf(MachineOf(Solo), ECommandReplyKind::StrikeResult);
				SoloStrikesBefore = Strikes.Num();
				SoloTallyBefore = Strikes.Num() > 0 ? Strikes.Last().Strike.TallyAfter : 0;
				APawn* Body = StealthPenaltyPie::BodyOf(Informant);
				StealthPenaltyHarnessSeams::SetRespawnProbe(true); // the next respawn's commit attempts
				PlaceFacing(Body, SoloPawn.Get(), 150.0);
				StrikeAt(SoloPawn.Get(), Body, Seq);
				Mark = Now();
			})
			.Until(TEXT("killed: the result arrives"), [this]() { return Now() >= Mark + StepSec; })
			.Then(TEXT("killed: assert"), [this]()
			{
				const TArray<FCommandReply> Strikes = RepliesOf(MachineOf(Solo), ECommandReplyKind::StrikeResult);
				ASSERT_THAT(IsTrue(Strikes.Num() == SoloStrikesBefore + 1, TEXT("killed: the solo player received one more strike result")));
				const FStrikeResult& Result = Strikes.Last().Strike;
				ASSERT_THAT(IsTrue(Result.Result == EStrikeResult::WrongTarget && Result.TallyAfter == SoloTallyBefore + 1 && !Result.bExiled,
					TEXT("killed: the killer's feedback is an NPC kill's (WrongTarget, tally + 1, no exile)")));
				const URosterStore* Roster = StealthPenaltyPie::Roster();
				const FParticipantRecord* Killer = Roster != nullptr ? Roster->FindRecord(StealthPenaltyPie::ParticipantOf(Solo.Get())) : nullptr;
				ASSERT_THAT(IsTrue(Killer != nullptr && Killer->State == EParticipantState::Active, TEXT("killed: the killer is still Active")));
				const UPenaltyService* Penalty = StealthPenaltyPie::Penalty();
				ASSERT_THAT(IsTrue(Penalty != nullptr && Penalty->GetLifeState(Informant) == EInformantLifeState::Awaiting, TEXT("killed: the informant awaits its next body")));
			});

		// respawn.
		AwaitBody(TEXT("respawn: the informant takes over another crowd pawn"));
		Network.Then(TEXT("respawn: assert"), [this]()
		{
			APawn* Body = StealthPenaltyPie::BodyOf(Informant);
			const URosterStore* Roster = StealthPenaltyPie::Roster();
			const FParticipantRecord* Record = Roster != nullptr ? Roster->FindRecord(Informant) : nullptr;
			const TArray<StealthPenaltyPie::FAttempt> All = StealthPenaltyPie::Attempts();
			const StealthPenaltyPie::FProbeEvent* Committed = nullptr;
			uint8 Level = 0;
			for (const StealthPenaltyPie::FAttempt& Attempt : All)
			{
				const StealthPenaltyPie::FProbeEvent* Takeover = Attempt.Find(StealthPenaltyPie::EProbeStep::Takeover);
				if (Takeover != nullptr && Takeover->TakeoverResult == EPenaltyTakeoverResult::Applied)
				{
					Committed = Takeover;
					Level = Attempt.Events[0].WideningLevel;
				}
			}
			ASSERT_THAT(IsTrue(Body != nullptr && Record != nullptr && Record->State == EParticipantState::Informant, TEXT("respawn: still an Informant (never Active this round)")));
			ASSERT_THAT(IsTrue(Committed != nullptr && Committed->Pawn.Get() == Body && Committed->bUnseenAtCommit, TEXT("respawn: the body is the committed takeover's pawn, unseen at the commit tick")));
			ASSERT_THAT(IsTrue(StealthPenaltyPie::IsInRespawnRegion(InformantTeam, Committed->Location, Level),
				*FString::Printf(TEXT("respawn: the new body is in the team's respawn region (level %d)"), Level)));
		});
	}

};

#endif
