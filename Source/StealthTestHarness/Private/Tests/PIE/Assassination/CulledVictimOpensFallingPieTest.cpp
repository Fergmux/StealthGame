// Assassination story asn-022 AC1 / AC-ASN-45 (ASN-R10, R21; ADR-0018 section 3.2 step 3 and section 5.1, ADR-0003 section
// 5.8; ADR-0008 T2; listen server + 2 clients on L_Prototype, NpcCount 0, every PIE world at a fixed 60 Hz):
// StealthGame.Assassination.PIE.CulledVictimOpensFalling.<method>.
// The defensive path: a victim whose channel to viewer W (a client, near the victim) is closed for a reason other than
// concealment, through the test-only per-connection deny seam (StealthRelevancyDenySeam, Release AtDeathWrite: the death
// write's re-open), struck by the striker S, in three runs: an AI-driven NPC, a player and the same participant again as an
// informant. For each run:
//   - server send capture to W (FChannelWatch, after each server tick flush): no open channel for the victim from the deny's
//     settle to the death-write tick; the channel opens in the death-write tick or the next;
//   - W never holds an instance of the victim between the two; the re-opened instance carries FDeathState from its first
//     sight (the initial block);
//   - W's presentation log first commands Death_Fall for the new instance, sought to EstimatedServerNow() - DeathServerTime
//     (+- 1 frame; 0 when the re-open precedes DeathServerTime, as the death write leads it by StrikeNetLeadSec), blended in
//     over <= 0.1 s, its montage weight ramping over at least 2 frames;
//   - the three kinds agree: the same channel-open offset (+- 1 frame), blend time and ramp (+- 1 frame).
// CONTROL: the NPC denied and then released by hand with no strike: W receives it again alive (no FDeathState, shown, no
// presentation clip). NEGATIVE CONTROL (ADR-0008 section 3): a second NPC whose deny is released in the strike's slot
// (OnPawnKilled, the "opens the record in the slot" build) must make the no-record-before-the-death-write check fail.
// Harness expectations depend on spike S10 (production/qa/spikes/2026-10-25_S10.md, 2026-10-26_S10-rerun.md): a re-open is
// a new client actor under the same NetGUID (the watch finds it by NetGUID), and the first frame of a re-created pawn is
// collapsed unless warmed up hidden (S10-R5, done by the presentation for a fresh actor carrying a death).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "Server/Core/StealthPawnFactory.h"
#include "Server/EncounterService.h"
#include "Server/StealthPenaltyHarnessSeams.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/ReopenPieSupport.h"

namespace CulledVictimOpensFallingPieData
{
	enum EKind : int32 { Npc, Player, Informant, NegativeControl, KindCount };
	const TCHAR* Name(int32 Kind)
	{
		switch (Kind)
		{
		case Npc: return TEXT("NPC");
		case Player: return TEXT("player");
		case Informant: return TEXT("informant");
		default: return TEXT("negative control");
		}
	}

	/** One run: the victim, its watches and what the server recorded. */
	struct FRun
	{
		TWeakObjectPtr<AStealthPawn> Victim;
		/** The victim as an opaque key (the body is gone by the assert: removal at + 9.2 s). */
		const AActor* VictimKey = nullptr;
		StealthReopenPie::FChannelWatch Channel;
		StealthReopenPie::FInstanceWatch Client;
		uint64 SettledFrame = 0;
		uint64 DeathWriteFrame = 0;
		double DeathServerTime = -1.0;
		bool bReleasedAtDeathWrite = false;
	};

	constexpr double StrikeDistanceCm = 140.0;
	constexpr double WitnessAsideCm = 450.0;
	/** A run lasts this long after the death write (the fall is 1.2 s). */
	constexpr double AfterDeathSec = 1.5;
}

NETWORK_TEST_CLASS(CulledVictimOpensFalling, "StealthGame.Assassination.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;
	inline static bool bPreviousFixedFrameRate = false;
	inline static float PreviousFixedFrameRate = 30.f;

	TWeakObjectPtr<AStealthPlayerController> Striker;
	TWeakObjectPtr<AStealthPlayerController> Target;
	TWeakObjectPtr<AStealthPlayerController> Witness;
	int32 WitnessMachine = INDEX_NONE;
	CulledVictimOpensFallingPieData::FRun Runs[CulledVictimOpensFallingPieData::KindCount];
	StealthReopenPie::FChannelWatch ControlChannel;
	StealthReopenPie::FInstanceWatch ControlClient;
	TWeakObjectPtr<AStealthPawn> PendingNpc;
	TWeakObjectPtr<AStealthPawn> TargetBody;
	FDelegateHandle DeathHandle;
	FDelegateHandle KilledHandle;
	uint32 Seq = 0x02200000u;
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
		bPreviousFixedFrameRate = GEngine->bUseFixedFrameRate;
		PreviousFixedFrameRate = GEngine->FixedFrameRate;
		GEngine->bUseFixedFrameRate = true;
		GEngine->FixedFrameRate = StealthReopenPie::FixedFrameRate;
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
		GEngine->bUseFixedFrameRate = bPreviousFixedFrameRate;
		GEngine->FixedFrameRate = PreviousFixedFrameRate;
	}

	BEFORE_EACH()
	{
		StealthRelevancyDenySeam::ReleaseAll();
		FNetworkComponentBuilder<FBasePIENetworkComponentState>().AsListenServer().WithClients(2).WithGameMode(AStealthGameMode::StaticClass()).Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
	}

	AFTER_EACH()
	{
		StealthRelevancyDenySeam::ReleaseAll();
		for (CulledVictimOpensFallingPieData::FRun& Run : Runs)
		{
			Run.Channel.Stop();
			Run.Client.Stop();
		}
		ControlChannel.Stop();
		ControlClient.Stop();
		const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(StealthAssassinationPie::ServerWorld());
		UEncounterService* Service = Root != nullptr ? Root->GetEncounterService() : nullptr;
		if (FOnDeathWriteApplied* Death = Service != nullptr ? Service->GetOnDeathWriteApplied() : nullptr)
		{
			Death->Remove(DeathHandle);
		}
		if (FOnPawnKilled* Killed = Service != nullptr ? Service->GetOnPawnKilled() : nullptr)
		{
			Killed->Remove(KilledHandle);
		}
	}

	AStealthPawn* PawnOf(const TWeakObjectPtr<AStealthPlayerController>& Controller) const
	{
		return Controller.IsValid() ? Cast<AStealthPawn>(Controller->GetPawn()) : nullptr;
	}

	UWorld* WitnessWorld() const
	{
		const TArray<UWorld*> All = StealthAssassinationPie::Worlds();
		return All.IsValidIndex(WitnessMachine) ? All[WitnessMachine] : nullptr;
	}

	void Hold(const TCHAR* Label, double Seconds, TFunction<bool()> Also = nullptr)
	{
		Network
			.Then(*FString::Printf(TEXT("%s: mark"), Label), [this]() { Mark = StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()); })
			.Until(Label, [this, Seconds, Also]()
			{
				return StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()) >= Mark + Seconds && (!Also || Also());
			});
	}

	/** Puts the witness WitnessAsideCm beside the victim, so it is near the victim before the death write. */
	void PlaceWitnessNear(const AStealthPawn* Victim)
	{
		AStealthPawn* W = PawnOf(Witness);
		if (W != nullptr && Victim != nullptr)
		{
			const FVector Right = FVector::CrossProduct(FVector::UpVector, Victim->GetActorForwardVector().GetSafeNormal2D());
			W->TeleportTo(Victim->GetActorLocation() + Right * CulledVictimOpensFallingPieData::WitnessAsideCm, W->GetActorRotation());
		}
	}

	/** Watches and denies Victim for the witness (released by the death write, or by the slot for the negative control). */
	void Arm(int32 Kind, AStealthPawn* Victim, StealthRelevancyDenySeam::ERelease Release)
	{
		CulledVictimOpensFallingPieData::FRun& Run = Runs[Kind];
		Run.Victim = Victim;
		Run.VictimKey = Victim;
		Run.Channel.Start(Victim, Witness.Get());
		Run.Client.Start(WitnessWorld(), StealthReopenPie::GuidOf(Victim));
		PlaceWitnessNear(Victim);
		StealthRelevancyDenySeam::Deny(Victim, Witness.Get(), Release);
	}

	/** Strikes Kind's victim from StrikeDistanceCm (the striker placed behind it along its own facing). */
	void Strike(int32 Kind)
	{
		CulledVictimOpensFallingPieData::FRun& Run = Runs[Kind];
		AStealthPawn* S = PawnOf(Striker);
		AStealthPawn* Victim = Run.Victim.Get();
		if (S == nullptr || Victim == nullptr)
		{
			return;
		}
		Run.SettledFrame = GFrameCounter;
		StealthAssassinationPie::PlaceStrikerFacing(S, Victim, CulledVictimOpensFallingPieData::StrikeDistanceCm);
		StealthAssassinationPie::StrikeAt(S, Victim, Seq);
	}

	bool DeathWritten(int32 Kind) const
	{
		return Runs[Kind].DeathWriteFrame != 0 || !Runs[Kind].Victim.IsValid(); // no victim: the spawn asserted, do not wait
	}

	void RunKind(int32 Kind)
	{
		using namespace CulledVictimOpensFallingPieData;
		Hold(*FString::Printf(TEXT("%s: the deny settles (close acknowledged)"), Name(Kind)), StealthReopenPie::DenySettleSec);
		Network.Then(*FString::Printf(TEXT("%s: strike"), Name(Kind)), [this, Kind]() { Strike(Kind); });
		Hold(*FString::Printf(TEXT("%s: the death write and the fall"), Name(Kind)), 0.0, [this, Kind]() { return DeathWritten(Kind); });
		Hold(*FString::Printf(TEXT("%s: after the death write"), Name(Kind)), AfterDeathSec);
	}

	AStealthPawn* SpawnNpcAhead()
	{
		return StealthReopenPie::SpawnNpcAheadAside(PawnOf(Striker), CulledVictimOpensFallingPieData::StrikeDistanceCm);
	}

	TEST_METHOD(NpcPlayerAndInformantReopenAtTheDeathWriteAlreadyFalling)
	{
		using namespace StealthAssassinationPie;
		using namespace CulledVictimOpensFallingPieData;
		Network
			.Until(TEXT("the round is Active and the players split 2v1"), [this]()
			{
				AStealthPlayerController* S = nullptr;
				AStealthPlayerController* P[2] = { nullptr, nullptr };
				if (!RoundActive() || !SplitTeams(S, P))
				{
					return false;
				}
				// The witness must be a client: the host is the striker or the target.
				const int32 HostPair = P[0]->IsLocalController() ? 0 : (P[1]->IsLocalController() ? 1 : INDEX_NONE);
				Striker = S;
				Target = HostPair == INDEX_NONE ? P[0] : P[HostPair];
				Witness = HostPair == INDEX_NONE ? P[1] : P[1 - HostPair];
				WitnessMachine = StealthPresentationPie::MachineOf(Witness.Get());
				return PawnOf(Striker) != nullptr && WitnessMachine != INDEX_NONE && !Witness->IsLocalController();
			})
			.Then(TEXT("place S clear of the bushes, the target aside; hook the death write"), [this]()
			{
				MoveClearOfBushes(PawnOf(Striker));
				const FVector Right = FVector::CrossProduct(FVector::UpVector, PawnOf(Striker)->GetActorForwardVector().GetSafeNormal2D());
				PawnOf(Target)->TeleportTo(PawnOf(Striker)->GetActorLocation() - Right * 1500.0, PawnOf(Target)->GetActorRotation());
				const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(ServerWorld());
				UEncounterService* Service = Root != nullptr ? Root->GetEncounterService() : nullptr;
				if (FOnDeathWriteApplied* Death = Service != nullptr ? Service->GetOnDeathWriteApplied() : nullptr)
				{
					DeathHandle = Death->AddLambda([this](APawn* Victim, double DeathServerTime, bool)
					{
						for (FRun& Run : Runs)
						{
							if (Run.Victim.Get() == Victim && Run.DeathWriteFrame == 0)
							{
								Run.DeathWriteFrame = GFrameCounter;
								Run.DeathServerTime = DeathServerTime;
							}
						}
					});
				}
				if (FOnPawnKilled* Killed = Service != nullptr ? Service->GetOnPawnKilled() : nullptr)
				{
					// The negative control's "opens the record in the slot": its deny is lifted inline in the strike's slot.
					KilledHandle = Killed->AddLambda([this](APawn* Victim, APawn*, EVictimKind, EKillCause)
					{
						if (Victim != nullptr && Victim == Runs[NegativeControl].Victim.Get())
						{
							StealthRelevancyDenySeam::Release(Victim);
						}
					});
				}
			});
		Hold(TEXT("settle"), 0.6);

		// CONTROL: deny and release by hand, no strike.
		Network.Then(TEXT("control: spawn an AI-driven NPC"), [this]()
		{
			PendingNpc = SpawnNpcAhead();
			ASSERT_THAT(IsTrue(PendingNpc.IsValid(), TEXT("control: the NPC spawned")));
		});
		Hold(TEXT("control: the NPC replicates"), 0.5);
		Network.Then(TEXT("control: deny the NPC to the witness"), [this]()
		{
			ControlChannel.Start(PendingNpc.Get(), Witness.Get());
			ControlClient.Start(WitnessWorld(), StealthReopenPie::GuidOf(PendingNpc.Get()));
			PlaceWitnessNear(PendingNpc.Get());
			StealthRelevancyDenySeam::Deny(PendingNpc.Get(), Witness.Get(), StealthRelevancyDenySeam::ERelease::Manual);
		});
		Hold(TEXT("control: the deny settles"), StealthReopenPie::DenySettleSec);
		Network.Then(TEXT("control: denied, then released with no strike"), [this]()
		{
			ASSERT_THAT(IsFalse(StealthReopenPie::HasOpenChannel(PendingNpc.Get(), Witness.Get()), TEXT("control: the deny closed the witness's channel")));
			ASSERT_THAT(IsTrue(StealthReopenPie::InstanceOf(WitnessWorld(), StealthReopenPie::GuidOf(PendingNpc.Get())) == nullptr, TEXT("control: the witness holds no instance while denied")));
			StealthRelevancyDenySeam::Release(PendingNpc.Get());
		});
		Hold(TEXT("control: the NPC re-opens"), 0.6);
		Network.Then(TEXT("control: the witness has the NPC's normal state"), [this]()
		{
			AStealthPawn* Copy = StealthReopenPie::InstanceOf(WitnessWorld(), StealthReopenPie::GuidOf(PendingNpc.Get()));
			ASSERT_THAT(IsTrue(StealthReopenPie::HasOpenChannel(PendingNpc.Get(), Witness.Get()), TEXT("control: the channel re-opened after the release")));
			ASSERT_THAT(IsTrue(Copy != nullptr, TEXT("control: the witness receives the NPC again")));
			ASSERT_THAT(IsFalse(Copy->HasDeathWrite(), TEXT("control: alive (no FDeathState)")));
			ASSERT_THAT(IsTrue(StealthReopenPie::IsShown(Copy), TEXT("control: shown")));
			ASSERT_THAT(IsTrue(StealthReopenPie::FirstClipRow(WitnessWorld(), Copy) == nullptr, TEXT("control: no presentation clip for it")));
			ControlChannel.Stop();
			ControlClient.Stop();
			// 1 NPC: the same NPC, denied until its death write.
			Arm(Npc, PendingNpc.Get(), StealthRelevancyDenySeam::ERelease::AtDeathWrite);
		});
		RunKind(Npc);

		// 2 player.
		Network.Then(TEXT("player: deny the target to the witness"), [this]()
		{
			TargetBody = PawnOf(Target);
			Arm(Player, TargetBody.Get(), StealthRelevancyDenySeam::ERelease::AtDeathWrite);
		});
		RunKind(Player);
		Hold(TEXT("player: the target respawns as an informant"), 0.0, [this]() { return PawnOf(Target) != nullptr && PawnOf(Target) != TargetBody.Get(); });
		Hold(TEXT("informant: the new pawn replicates"), 0.5);

		// 3 informant.
		Network.Then(TEXT("informant: deny the informant to the witness"), [this]()
		{
			Arm(Informant, PawnOf(Target), StealthRelevancyDenySeam::ERelease::AtDeathWrite);
		});
		RunKind(Informant);

		// NEGATIVE CONTROL: released in the slot.
		Network.Then(TEXT("negative control: spawn an NPC"), [this]()
		{
			PendingNpc = SpawnNpcAhead();
			ASSERT_THAT(IsTrue(PendingNpc.IsValid(), TEXT("negative control: the NPC spawned")));
		});
		Hold(TEXT("negative control: the NPC replicates"), 0.5);
		Network.Then(TEXT("negative control: deny, released in the slot"), [this]()
		{
			Arm(NegativeControl, PendingNpc.Get(), StealthRelevancyDenySeam::ERelease::Manual);
		});
		RunKind(NegativeControl);

		Network.Then(TEXT("assert"), [this]()
		{
			using namespace StealthReopenPie;
			UWorld* W = WitnessWorld();
			const FEncounterPresentationParams Params = FEncounterPresentationParams::FromPublic(FAssassinationTuning_Public());
			const UEncounterPresentationSubsystem* Presentation = StealthPresentationPie::PresentationOf(W);
			ASSERT_THAT(IsTrue(Presentation != nullptr, TEXT("the witness has a presentation subsystem")));
			const double Rate = Presentation->GetClipTimings().FallLengthSec / Params.DeathFallSec;
			const double Frame = 1.0 / FixedFrameRate;
			int64 OpenOffset[3] = {};
			double Blend[3] = {};
			int32 Ramp[3] = {};
			const TArray<StealthRelevancyDenySeam::FReleaseRecord>& Releases = StealthRelevancyDenySeam::GetReleases();
			for (int32 Kind = Npc; Kind <= Informant; ++Kind)
			{
				const FRun& Run = Runs[Kind];
				ASSERT_THAT(IsTrue(Run.DeathWriteFrame != 0 && Run.DeathServerTime > 0.0, *FString::Printf(TEXT("%s: killed (death write applied)"), Name(Kind))));
				const StealthRelevancyDenySeam::FReleaseRecord* Release = Releases.FindByPredicate([&Run](const StealthRelevancyDenySeam::FReleaseRecord& R) { return R.Pawn == Run.VictimKey && R.bAtDeathWrite; });
				ASSERT_THAT(IsTrue(Release != nullptr && Release->Frame == Run.DeathWriteFrame,
					*FString::Printf(TEXT("%s: the deny was lifted by the death write, in its step"), Name(Kind))));

				// Server send capture.
				const int32 EarlyOpen = Run.Channel.OpenBetween(Run.SettledFrame, Run.DeathWriteFrame);
				const uint64 FirstOpen = Run.Channel.FirstOpenFrom(Run.DeathWriteFrame);
				OpenOffset[Kind - Npc] = FirstOpen == MAX_uint64 ? -1 : static_cast<int64>(FirstOpen - Run.DeathWriteFrame);
				TestRunner->AddInfo(FString::Printf(TEXT("ASN45 %s: settle frame %llu, death-write frame %llu, early open frames %d, first open +%lld"), Name(Kind),
					Run.SettledFrame, Run.DeathWriteFrame, EarlyOpen, OpenOffset[Kind - Npc]));
				ASSERT_THAT(IsTrue(EarlyOpen == 0, *FString::Printf(TEXT("%s: no record for the victim to the witness before the death-write tick"), Name(Kind))));
				ASSERT_THAT(IsTrue(OpenOffset[Kind - Npc] == 0 || OpenOffset[Kind - Npc] == 1, *FString::Printf(TEXT("%s: the record opens in the death-write tick or the next"), Name(Kind))));

				// The witness's instance.
				ASSERT_THAT(IsTrue(Run.Client.PresentFramesBetween(Run.SettledFrame, Run.DeathWriteFrame) == 0, *FString::Printf(TEXT("%s: the witness holds no instance until the death write"), Name(Kind))));
				const TArray<const FInstance*> Reopened = Run.Client.CreatedBetween(Run.DeathWriteFrame, MAX_uint64);
				ASSERT_THAT(IsTrue(Reopened.Num() == 1, *FString::Printf(TEXT("%s: exactly one re-opened instance on the witness"), Name(Kind))));
				const FInstance& Instance = *Reopened[0];
				ASSERT_THAT(IsTrue(Instance.bDeathAtFirstSight, *FString::Printf(TEXT("%s: FDeathState in the initial block (set at first sight)"), Name(Kind))));

				FString WarmWhy;
				const bool bWarm = WarmedUpBeforeShown(W, Instance, WarmWhy);
				TestRunner->AddInfo(FString::Printf(TEXT("ASN45 %s warm-up: %s"), Name(Kind), *WarmWhy));
				ASSERT_THAT(IsTrue(bWarm, *FString::Printf(TEXT("%s: hidden from its creation until its first posed frame (S10-R5 warm-up)"), Name(Kind))));

				// The witness's presentation.
				const FEncounterPresentationLogRow* First = FirstClipRow(W, Instance.Key);
				ASSERT_THAT(IsTrue(First != nullptr && First->Clip == EEncounterClip::DeathFall, *FString::Printf(TEXT("%s: the first clip commanded is Death_Fall"), Name(Kind))));
				const double Seek = First->Value / Rate;
				const double Expected = FMath::Clamp(First->ServerNow - Run.DeathServerTime, 0.0, Params.DeathFallSec);
				TestRunner->AddInfo(FString::Printf(TEXT("ASN45 %s: instance first seen at %.3f (death %.3f), Death_Fall at %.3f sought to %.4f s (now - death %.4f), blend %.3f s"), Name(Kind),
					Instance.FirstServerNow, Run.DeathServerTime, First->ServerNow, Seek, First->ServerNow - Run.DeathServerTime, First->BlendInSec));
				ASSERT_THAT(IsTrue(FMath::Abs(Seek - Expected) <= Frame && First->ServerNow + 0.001 >= Run.DeathServerTime && First->ServerNow - Run.DeathServerTime <= FMath::Max(Frame + 0.003, Instance.FirstServerNow - Run.DeathServerTime + Frame + 0.003),
					*FString::Printf(TEXT("%s: Death_Fall sought to EstimatedServerNow() - DeathServerTime (+- 1 frame), never before DeathServerTime"), Name(Kind))));
				ASSERT_THAT(IsTrue(First->BlendInSec > 0.0 && First->BlendInSec <= 0.1 + 1e-6, *FString::Printf(TEXT("%s: blended in over <= 0.1 s"), Name(Kind))));
				float FirstWeight = -1.f;
				Ramp[Kind - Npc] = RampFrames(Instance, FirstWeight);
				Blend[Kind - Npc] = First->BlendInSec;
				TestRunner->AddInfo(FString::Printf(TEXT("ASN45 %s: fall weight %.3f on its first frame, 1 after %d frames"), Name(Kind), FirstWeight, Ramp[Kind - Npc]));
				ASSERT_THAT(IsTrue(FirstWeight >= 0.f && FirstWeight < 1.f && Ramp[Kind - Npc] >= 2, *FString::Printf(TEXT("%s: the blend weight ramps over at least 2 frames"), Name(Kind))));
			}
			for (int32 Index = 1; Index < 3; ++Index)
			{
				ASSERT_THAT(IsTrue(FMath::Abs(OpenOffset[Index] - OpenOffset[0]) <= 1 && FMath::IsNearlyEqual(Blend[Index], Blend[0]) && FMath::Abs(Ramp[Index] - Ramp[0]) <= 1,
					*FString::Printf(TEXT("%s and NPC: identical re-open, seek and blend (+- 1 frame)"), Name(Npc + Index))));
			}

			// NEGATIVE CONTROL: the slot release must be caught by the no-early-record check.
			const FRun& Negative = Runs[NegativeControl];
			ASSERT_THAT(IsTrue(Negative.DeathWriteFrame != 0, TEXT("negative control: killed")));
			const int32 NegativeEarly = Negative.Channel.OpenBetween(Negative.SettledFrame, Negative.DeathWriteFrame);
			TestRunner->AddInfo(FString::Printf(TEXT("ASN45 negative control: %d open frames before the death-write tick"), NegativeEarly));
			ASSERT_THAT(IsTrue(NegativeEarly > 0, TEXT("negative control: a record opened in the slot is caught by the check")));
		});
	}
};

#endif
