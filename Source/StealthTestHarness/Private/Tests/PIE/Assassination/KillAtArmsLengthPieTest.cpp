// Assassination story asn-018 AC1 / AC-ASN-01 KillAtArmsLength (ADR-0008 T2; listen server + 2 clients on L_Prototype,
// NpcCount 0): StealthGame.Assassination.PIE.KillAtArmsLength.<method>. An unbraced rival stands with its capsule axis
// 230 cm (horizontal) from the attacker's, the attacker facing it; the attacker's Attack (an aimed PawnRef strike through
// the production path, with its own origin) resolves in Round's slot:
//   - exactly one OnPawnKilled(Victim, Attacker, Player, Strike) fires, in the Attack's slot, and the victim's Roster-mirror
//     life state becomes Informant;
//   - on the server and on every client the victim's Death_Fall starts at ContactServerTime (+- one frame of that machine):
//     on the first frame of that machine whose estimated server time reached ContactServerTime. Every machine's time is
//     sampled once per engine frame (after the world ticks, as the presentation rows are stamped) from the press, so the
//     check is the machine's own frame, whatever its length (fix 2026-10-28: the earlier bound used the delta of the frame
//     the assertion ran in, 1.5 s later, and failed whenever a full-gate load hitch hit the contact frame);
//   - the attacker's own machine: the game applies no camera rotation from the press to contact (its control rotation is
//     unchanged on every frame), and the attacker's own strike clip starts on server confirmation (never predicted before
//     the slot's ContactServerTime - StrikeContactSec). The reticle / lock-on / outline half is judged on the retained
//     captures asn-01-attacker-*.png (production/qa/evidence/story-018-...).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "HAL/PlatformProcess.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Tests/PIE/StealthPIEOpenCircle.h"
#include "HAL/IConsoleManager.h"
#include "Server/EncounterService.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/PresentationPieSupport.h"

NETWORK_TEST_CLASS(KillAtArmsLength, "StealthGame.Assassination.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	TWeakObjectPtr<AStealthPlayerController> Attacker;
	TWeakObjectPtr<AStealthPlayerController> Victim;
	TWeakObjectPtr<AStealthPawn> AttackerPawn;
	TWeakObjectPtr<AStealthPawn> VictimPawn;
	TArray<TWeakObjectPtr<AStealthPawn>> VictimCopies;
	FDelegateHandle KilledHandle;
	int32 Kills = 0;
	bool bKillArgsMatch = false;
	double PressServerTime = -1.0;
	double ContactServerTime = -1.0;
	double DeathServerTime = -1.0;
	FRotator OwnerRotationAtPress;
	double MaxOwnerRotationDeltaDeg = 0.0;
	double AxisDistanceCm = 0.0;
	uint32 Seq = 0x31800000u;
	double Mark = 0.0;
	/** Per machine (StealthAssassinationPie::Worlds() order): (GFrameCounter, estimated server time) once per engine frame. */
	TArray<TArray<TPair<uint64, double>>> MachineTimes;
	/** The hitch variant: one 150 ms stall in the last frame before contact (the full gate's load, made deterministic). */
	bool bInjectHitch = false;
	bool bHitched = false;
	double HitchFrameServerTime = -1.0;

	void SampleMachineTimes()
	{
		const TArray<UWorld*> All = StealthAssassinationPie::Worlds();
		MachineTimes.SetNum(All.Num());
		for (int32 Machine = 0; Machine < All.Num(); ++Machine)
		{
			MachineTimes[Machine].Add(TPair<uint64, double>(GFrameCounter, UEncounterPresentationSubsystem::EstimatedServerNow(All[Machine])));
		}
	}

	/** The machine's sampled time of the last frame before Frame (-1 when none was sampled). */
	double TimeBeforeFrame(int32 Machine, uint64 Frame) const
	{
		double Out = -1.0;
		uint64 Best = 0;
		if (MachineTimes.IsValidIndex(Machine))
		{
			for (const TPair<uint64, double>& Sample : MachineTimes[Machine])
			{
				if (Sample.Key < Frame && Sample.Key >= Best)
				{
					Best = Sample.Key;
					Out = Sample.Value;
				}
			}
		}
		return Out;
	}

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

	AFTER_EACH()
	{
		const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(StealthAssassinationPie::ServerWorld());
		UEncounterService* Service = Root != nullptr ? Root->GetEncounterService() : nullptr;
		if (FOnPawnKilled* Killed = Service != nullptr ? Service->GetOnPawnKilled() : nullptr)
		{
			Killed->Remove(KilledHandle);
		}
	}

	TEST_METHOD(RivalAt230cmIsAssassinatedAndFallsAtContactWithNoGameCameraRotation)
	{
		RunScenario();
	}

	TEST_METHOD(RivalAt230cmFallsOnTheFirstFrameAtContactUnderAHitch)
	{
		bInjectHitch = true;
		RunScenario();
	}

	void RunScenario()
	{
		using namespace StealthAssassinationPie;
		using namespace StealthPresentationPie;
		Network
			.Until(TEXT("the round is Active and the players split 2v1"), [this]()
			{
				AStealthPlayerController* S = nullptr;
				AStealthPlayerController* P[2] = { nullptr, nullptr };
				if (!RoundActive() || !SplitTeams(S, P))
				{
					return false;
				}
				// P[0] strikes the solo rival (its team's last Active player: the round ends after, which this test allows).
				Attacker = P[0];
				Victim = S;
				AttackerPawn = Cast<AStealthPawn>(P[0]->GetPawn());
				VictimPawn = Cast<AStealthPawn>(S->GetPawn());
				return AttackerPawn.IsValid() && VictimPawn.IsValid();
			})
			.Then(TEXT("place the victim clear of the bushes and the attacker 230 cm from it, facing it"), [this]()
			{
				// The pair is placed on a clear, flat disc far from every bush (fix 2026-10-27): the spawn layout comes from OS
				// entropy, and a 230 cm spot in front of the victim that encroached on an adobe piece or the third player was
				// pushed aside by the teleport (231.9 cm in one full-suite run; the 1 cm precondition then failed).
				MoveClearOfBushes(VictimPawn.Get());
				UWorld* Server = ServerWorld();
				const float HalfHeight = VictimPawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
				StealthPIEOpenCircle::FParams Params;
				Params.RadiusCm = 115.0;
				Params.MarginCm = 60.0;
				Params.AlsoIgnored.Add(AttackerPawn.Get());
				Params.Accept = [Server](const FVector& Centre)
				{
					for (TActorIterator<AStealthBush> It(Server); It; ++It)
					{
						if (FVector::Dist2D(It->GetActorLocation(), Centre) <= 900.0)
						{
							return false;
						}
					}
					return true;
				};
				const TOptional<FVector> Centre = StealthPIEOpenCircle::FindOpenCircle(Server, VictimPawn->GetActorLocation() - FVector(0.0, 0.0, HalfHeight), VictimPawn.Get(), Params);
				ASSERT_THAT(IsTrue(Centre.IsSet(), TEXT("a clear, flat disc away from the bushes for the pair")));
				const FVector Lift(0.0, 0.0, HalfHeight + 2.0);
				// The striker keeps its own rotation (a remote owner's rotation comes back from its client, so a server-side turn
				// does not stick): the victim goes 115 cm ahead of the centre along the striker's facing and the striker 230 cm
				// behind the victim (PlaceStrikerFacing), so the target is straight ahead on the clear disc.
				const FVector Forward = AttackerPawn->GetActorForwardVector().GetSafeNormal2D();
				VictimPawn->TeleportTo(Centre.GetValue() + Forward * 115.0 + Lift, VictimPawn->GetActorRotation(), false, true);
				PlaceStrikerFacing(AttackerPawn.Get(), VictimPawn.Get(), 230.0);
				const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(ServerWorld());
				UEncounterService* Service = Root != nullptr ? Root->GetEncounterService() : nullptr;
				if (FOnPawnKilled* Killed = Service != nullptr ? Service->GetOnPawnKilled() : nullptr)
				{
					KilledHandle = Killed->AddLambda([this](APawn* Dead, APawn* Killer, EVictimKind Kind, EKillCause Cause)
					{
						++Kills;
						bKillArgsMatch = Dead == VictimPawn.Get() && Killer == AttackerPawn.Get() && Kind == EVictimKind::Player && Cause == EKillCause::Strike;
					});
				}
			})
			.Then(TEXT("settle: mark"), [this]() { Mark = ServerNow(ServerWorld()); })
			.Until(TEXT("settle"), [this]() { return ServerNow(ServerWorld()) >= Mark + 0.8; })
			.Then(TEXT("the attacker presses Attack on the rival"), [this]()
			{
				AxisDistanceCm = FVector::Dist2D(AttackerPawn->GetActorLocation(), VictimPawn->GetActorLocation());
				VictimCopies.Reset();
				for (UWorld* World : Worlds())
				{
					VictimCopies.Add(CopyOf(VictimPawn.Get(), World));
				}
				const AStealthPlayerController* Owner = OwnerMachineController(Attacker.Get());
				OwnerRotationAtPress = Owner != nullptr ? Owner->GetControlRotation() : FRotator::ZeroRotator;
				PressServerTime = ServerNow(ServerWorld());
				StrikeAt(AttackerPawn.Get(), VictimPawn.Get(), Seq);
			})
			.Until(TEXT("contact + 1.5 s, sampling the attacker's own camera every frame to contact"), [this]()
			{
				const AStealthPlayerController* Owner = OwnerMachineController(Attacker.Get());
				SampleMachineTimes();
				const double Now = ServerNow(ServerWorld());
				if (bInjectHitch && !bHitched && ContactServerTime > 0.0 && Now < ContactServerTime && Now >= ContactServerTime - 0.05)
				{
					// After this frame's world ticks (latent commands run after GEngine->Tick): the next frame is ~150 ms long
					// and is the first one at or past contact on every machine.
					bHitched = true;
					HitchFrameServerTime = Now;
					FPlatformProcess::Sleep(0.15f);
				}
				if (AttackerPawn.IsValid() && AttackerPawn->GetActionState().Pose == EActionPose::Strike)
				{
					ContactServerTime = AttackerPawn->GetActionState().ContactServerTime;
				}
				if (VictimPawn.IsValid() && VictimPawn->HasDeathWrite())
				{
					DeathServerTime = VictimPawn->GetDeathState().DeathServerTime;
				}
				if (Owner != nullptr && (ContactServerTime < 0.0 || Now <= ContactServerTime))
				{
					const FRotator Delta = (Owner->GetControlRotation() - OwnerRotationAtPress).GetNormalized();
					MaxOwnerRotationDeltaDeg = FMath::Max(MaxOwnerRotationDeltaDeg, FMath::Max3(FMath::Abs(Delta.Pitch), FMath::Abs(Delta.Yaw), FMath::Abs(Delta.Roll)));
				}
				return ContactServerTime > 0.0 && Now >= ContactServerTime + 1.5;
			})
			.Then(TEXT("assert"), [this]()
			{
				const TArray<UWorld*> All = Worlds();
				const AStealthPlayerState* VictimState = Victim.IsValid() ? Victim->GetPlayerState<AStealthPlayerState>() : nullptr;
				TestRunner->AddInfo(FString::Printf(TEXT("ASN01 axis distance %.1f cm, press %.3f, contact %.3f, death %.3f, kills %d, max owner rotation delta %.4f deg"),
					AxisDistanceCm, PressServerTime, ContactServerTime, DeathServerTime, Kills, MaxOwnerRotationDeltaDeg));
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(AxisDistanceCm, 230.0, 1.0), TEXT("the capsule axes were 230 cm apart at the press")));
				ASSERT_THAT(IsTrue(Kills == 1 && bKillArgsMatch, TEXT("exactly one OnPawnKilled(Victim, Attacker, Player, Strike)")));
				ASSERT_THAT(IsTrue(VictimState != nullptr && VictimState->IsInformant(), TEXT("the victim is an informant")));
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(DeathServerTime, ContactServerTime, 1e-6), TEXT("DeathServerTime is the strike's ContactServerTime")));
				for (int32 Machine = 0; Machine < All.Num(); ++Machine)
				{
					const AStealthPawn* Copy = VictimCopies.IsValidIndex(Machine) ? VictimCopies[Machine].Get() : nullptr;
					const FEncounterPresentationLogRow* Fall = StealthPresentationPie::FirstRow(All[Machine], Copy, EEncounterPresentationEvent::ClipStart, EEncounterClip::DeathFall);
					// "+- one frame" of this machine: the fall's frame is at or after contact and the machine's previous frame was
					// before it (the first frame that reached contact), however long that frame was.
					const double Previous = Fall != nullptr ? TimeBeforeFrame(Machine, Fall->Frame) : -1.0;
					TestRunner->AddInfo(FString::Printf(TEXT("ASN01 machine %d: Death_Fall starts at %.3f (late %d), previous frame %.3f, frame step %.1f ms"), Machine,
						Fall != nullptr ? Fall->ServerNow : -1.0, Fall != nullptr && Fall->bLate ? 1 : 0, Previous, Fall != nullptr && Previous >= 0.0 ? (Fall->ServerNow - Previous) * 1000.0 : -1.0));
					// The pre-2026-10-28 bound, reported only (the watch-fail record): one frame = this machine's delta at the assert.
					const double OldFrameSec = FMath::Max(1.0 / 60.0, static_cast<double>(All[Machine]->GetDeltaSeconds())) + 0.002;
					TestRunner->AddInfo(FString::Printf(TEXT("ASN01 machine %d: the old bound (contact + %.1f ms) %s"), Machine, OldFrameSec * 1000.0,
						Fall != nullptr && Fall->ServerNow <= ContactServerTime + OldFrameSec ? TEXT("holds") : TEXT("FAILS")));
					ASSERT_THAT(IsTrue(Fall != nullptr && Fall->ServerNow + 0.001 >= ContactServerTime && Previous >= 0.0 && Previous < ContactServerTime + 0.001,
						*FString::Printf(TEXT("machine %d: Death_Fall starts at ContactServerTime (+- one frame)"), Machine)));
				}
				if (bInjectHitch)
				{
					TestRunner->AddInfo(FString::Printf(TEXT("ASN01 hitch injected in the frame at %.3f (contact %.3f)"), HitchFrameServerTime, ContactServerTime));
					ASSERT_THAT(IsTrue(bHitched, TEXT("the hitch was injected in the last frame before contact")));
				}
				ASSERT_THAT(IsTrue(MaxOwnerRotationDeltaDeg <= 0.01, TEXT("no game-applied camera rotation on the attacker's machine from the press to contact")));
				const int32 OwnerMachine = StealthPresentationPie::MachineOf(Attacker.Get());
				const AStealthPawn* OwnerCopy = OwnerMachine != INDEX_NONE ? CopyOf(AttackerPawn.Get(), All[OwnerMachine]) : nullptr;
				const FEncounterPresentationLogRow* OwnStrike = StealthPresentationPie::FirstRow(All[OwnerMachine], OwnerCopy, EEncounterPresentationEvent::ClipStart);
				ASSERT_THAT(IsTrue(OwnStrike != nullptr && IsStrikeClip(OwnStrike->Clip) && OwnStrike->ServerNow + 0.001 >= ContactServerTime - 0.40,
					TEXT("the owner's strike clip starts on server confirmation (no predicted body pose)")));
			});
	}
};

#endif
