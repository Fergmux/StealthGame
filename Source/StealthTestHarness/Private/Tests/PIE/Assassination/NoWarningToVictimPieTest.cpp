// Assassination story asn-017 AC1 / AC-ASN-05 (ASN-R6, R16, R23; ADR-0008 T2; listen server + 2 clients on L_Prototype,
// NpcCount 0): StealthGame.Assassination.PIE.NoWarningToVictim.<case>. Six fresh scenario instances (one TEST_METHOD and so
// one PIE session each): (a) an unbraced victim walking (a scripted Move every frame), (b) an unbraced victim standing,
// (c) a braced defender, each struck from behind and from the front. Four players split 2v2 (listen server + 3 clients),
// so neither the kill nor the reversal ends the round (a round end at the death write is the bystanders' documented
// I-ASN-4 residual, not a participant's warning, and is NoObserverCue's to check). Each instance runs an un-struck CONTROL leg and then
// the STRUCK leg from the same placement with the same scripted input (the control is the earlier leg of the same fresh
// session: the 60 s defence cooldown is cleared between the legs by the defence state's Reset, as Round's Reset does).
// Fixed frame rates (the engine's fixed frame rate, 60 Hz, for every PIE world of the process), no packet loss and no
// emulated lag (a fixed in-process delay, zero variance). The victim is always a remote client's player (its moves carry
// CMC timestamps); its scripted Move is UPawnCommandComponent::Move on its own machine (the production Move path).
// Between the strike's execution and the victim machine's local contact frame:
//   1 the victim machine's presentation log (PresentationLogHook: HUD view-model, widget list, camera grade and role
//     clock, the victim's own montage and pose rows) has no entry the control lacks at the same relative time (+- 1 frame);
//   2 the victim's server displacement from the leg's start at the CMC timestamp of the last move consumed before the
//     death write equals the control's at the same timestamp (relative to the first scripted move) within 1 cm (the
//     displacement, so a teleport's settle at the leg start does not count);
//   3 the victim pawn's copies on the victim's machine and on the bystander's (the one other player: the game has two
//     teams, so the AC's third-team bystander is the remaining connection) receive no property change from the slot to the
//     death-write tick other than movement / CMC fields that change at the same times in the control ((b), (c): none).
// NEGATIVE CONTROL (ADR-0008 section 3): case (b) from behind with the StealthOwnerPresentationTestSeams
// SetPresentOnRosterReceipt seam on (a HUD line on the arrival of the owner's outcome state: its Roster state or its own
// FDeathState, whichever first) must make check 1 report the line.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Server/DefenceStateComponent.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/OwnPresentationPieSupport.h"
#include "Tests/PIE/Assassination/PresentationLogHook.h"
#include "View/Core/StealthGait.h"
#include "View/DefenceOwnerComponent.h"

namespace NoWarningToVictimPieData
{
	enum class ECase : uint8 { Moving, Standing, Braced };
	const TCHAR* CaseName(ECase Case) { return Case == ECase::Moving ? TEXT("moving") : (Case == ECase::Standing ? TEXT("standing") : TEXT("braced")); }

	constexpr double StrikeDistanceCm = 150.0;
	constexpr double BystanderAsideCm = 900.0;
	/** The fixed frame rate of every PIE world during the test. */
	constexpr float FixedFrameRate = 60.0f;
	/** The strike is issued this long after the leg begins (a walking victim is then at speed; walk start 0.4 s). */
	constexpr double IssueAfterSec = 0.25;
	constexpr double MovingIssueAfterSec = 0.6;
	/** (a): the attacker stands this far from the walking victim, so the victim walks into (front) or out to (behind) about
	 *  150 cm at the strike and never reaches the attacker's capsule before the death write. */
	constexpr double MovingFrontCm = 200.0;
	constexpr double MovingBehindCm = 100.0;
	double IssueAfter(ECase Case) { return Case == ECase::Moving ? MovingIssueAfterSec : IssueAfterSec; }
	/** A leg runs this long after the strike's issue (past contact at about issue + 0.4 s). */
	constexpr double LegSec = 1.4;
	constexpr double SettleSec = 0.6;
	/** The brace is up this long before the leg (DefenceDurationSec 4 s covers the leg). */
	constexpr double BraceLeadSec = 0.4;
	/** After the control leg's brace window ends (DefenceDurationSec 4 s from its start). */
	constexpr double BraceOverSec = 4.4;
	/** AC-ASN-05: the position bound. */
	constexpr double PositionToleranceCm = 1.0;
	/** The client's server-time estimate against the server's own clock (the ADR-0002 section 9 stand-in's accuracy). */
	constexpr double ClockToleranceSec = 0.05;
	/** The fourth player stands this far away. */
	constexpr double ParkedAsideCm = 1500.0;

	/** One server sample of the victim: the CMC timestamp of the last consumed move and the location. */
	struct FMoveSample
	{
		float ClientTimeStamp = 0.f;
		FVector Location = FVector::ZeroVector;
		double ServerNow = 0.0;
	};

	/** One leg's record. */
	struct FLeg
	{
		double ServerOrigin = 0.0;
		double VictimOrigin = 0.0;
		FVector StartLocation = FVector::ZeroVector;
		FString MoveDiagnostics;
		float FirstMoveTimeStamp = -1.f;
		StealthPresentationLog::FPresentationLogHook Hook;
		StealthPresentationLog::FReplicatedPropertyWatcher Watchers[2];
		TArray<FMoveSample> Moves;
		double Exec = -1.0;
		double Contact = -1.0;
		double DeathWrite = -1.0;
	};
}

NETWORK_TEST_CLASS(NoWarningToVictim, "StealthGame.Assassination.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;
	inline static bool bPreviousFixedFrameRate = false;
	inline static float PreviousFixedFrameRate = 30.f;

	TWeakObjectPtr<AStealthPlayerController> Victim;
	TWeakObjectPtr<AStealthPlayerController> Attacker;
	TWeakObjectPtr<AStealthPlayerController> Bystander;
	TWeakObjectPtr<AStealthPlayerController> Parked;
	int32 MachineV = INDEX_NONE;
	int32 MachineB = INDEX_NONE;
	FVector VictimStart = FVector::ZeroVector;
	FRotator VictimFacing = FRotator::ZeroRotator;
	FVector AttackerStart = FVector::ZeroVector;
	FRotator AttackerFacing = FRotator::ZeroRotator;
	FVector MoveDirection = FVector::ZeroVector;
	NoWarningToVictimPieData::FLeg Legs[2];
	StealthOwnPresentationPie::FFrameTracker Frames;
	bool bMoving = false;
	uint32 Seq = 0x01700000u;
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
		bPreviousFixedFrameRate = GEngine->bUseFixedFrameRate;
		PreviousFixedFrameRate = GEngine->FixedFrameRate;
		GEngine->bUseFixedFrameRate = true;
		GEngine->FixedFrameRate = NoWarningToVictimPieData::FixedFrameRate;
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
		GEngine->bUseFixedFrameRate = bPreviousFixedFrameRate;
		GEngine->FixedFrameRate = PreviousFixedFrameRate;
		StealthOwnerPresentationTestSeams::SetPresentOnRosterReceipt(false);
	}

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FBasePIENetworkComponentState>().AsListenServer().WithClients(3).WithGameMode(AStealthGameMode::StaticClass()).Build(Network);
		Network.StartOnFixtureMap(StealthAssassinationPie::FixtureMap);
	}

	AStealthPawn* PawnOf(const TWeakObjectPtr<AStealthPlayerController>& Controller) const
	{
		return Controller.IsValid() ? Cast<AStealthPawn>(Controller->GetPawn()) : nullptr;
	}

	/** The victim's pawn on its own machine. */
	AStealthPawn* VictimLocalPawn() const
	{
		const AStealthPlayerController* Local = StealthOwnPresentationPie::LocalOf(MachineV);
		return Local != nullptr ? Cast<AStealthPawn>(Local->GetPawn()) : nullptr;
	}

	/** The victim's scripted Move: W (walk ahead, no idle pivot) pressed or released on its own machine. */
	void MoveKey(bool bDown) const
	{
		if (AStealthPlayerController* Local = StealthOwnPresentationPie::LocalOf(MachineV))
		{
			Local->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W, bDown ? IE_Pressed : IE_Released, bDown ? 1.f : 0.f));
		}
	}

	static void Face(APawn* Pawn, const FVector& Location, const FRotator& Facing)
	{
		Pawn->TeleportTo(Location, Facing);
		if (APlayerController* Player = Cast<APlayerController>(Pawn->GetController()))
		{
			Player->SetControlRotation(Facing);
			Player->ClientSetRotation(Facing, true);
		}
	}

	/** Every frame of a leg: the scripted move, the recorders, the server samples and the strike's times. */
	void SampleLeg(int32 LegIndex, NoWarningToVictimPieData::ECase Case)
	{
		using namespace NoWarningToVictimPieData;
		FLeg& Leg = Legs[LegIndex];
		Frames.Sample();
		if (bMoving && Case == ECase::Moving && Leg.MoveDiagnostics.IsEmpty() && StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld()) >= Leg.ServerOrigin + IssueAfter(Case))
		{
			// What the scripted Move reached on the victim's machine (diagnostics for the evidence log).
			const AStealthPlayerController* LocalController = StealthOwnPresentationPie::LocalOf(MachineV);
			const AStealthPawn* Local = VictimLocalPawn();
			Leg.MoveDiagnostics = FString::Printf(TEXT("client: menu %d, last sent %s, intent %s, ignored %d, velocity %s"),
				LocalController && LocalController->GetCommandClient() ? (LocalController->GetCommandClient()->IsMenuMode() ? 1 : 0) : -1,
				LocalController && LocalController->GetCommandClient() ? *LocalController->GetCommandClient()->GetLastSentMove().ToString() : TEXT("-"),
				Local && Local->GetPawnCommand() ? *Local->GetPawnCommand()->GetMoveIntent().ToString() : TEXT("-"),
				LocalController ? (LocalController->IsMoveInputIgnored() ? 1 : 0) : -1, Local ? *Local->GetVelocity().ToString() : TEXT("-"));
		}
		Leg.Hook.Sample();
		Leg.Watchers[0].Sample();
		Leg.Watchers[1].Sample();
		const AStealthPawn* ServerVictim = PawnOf(Victim);
		const AStealthPawn* ServerAttacker = PawnOf(Attacker);
		const double Now = StealthAssassinationPie::ServerNow(StealthAssassinationPie::ServerWorld());
		if (ServerVictim != nullptr && ServerVictim->GetCharacterMovement() != nullptr)
		{
			const FNetworkPredictionData_Server_Character* Server = ServerVictim->GetCharacterMovement()->GetPredictionData_Server_Character();
			Leg.Moves.Add({ Server != nullptr ? Server->CurrentClientTimeStamp : 0.f, ServerVictim->GetActorLocation(), Now });
		}
		if (ServerAttacker != nullptr && ServerAttacker->GetActionState().Pose == EActionPose::Strike && ServerAttacker->GetActionState().StartServerTime >= Leg.ServerOrigin)
		{
			Leg.Exec = ServerAttacker->GetActionState().StartServerTime;
			Leg.Contact = ServerAttacker->GetActionState().ContactServerTime;
		}
		const bool bDeath = (ServerVictim != nullptr && ServerVictim->HasDeathWrite()) || (ServerAttacker != nullptr && ServerAttacker->HasDeathWrite());
		if (bDeath && Leg.DeathWrite < 0.0)
		{
			Leg.DeathWrite = Now;
		}
	}

	void ResetDefence()
	{
		if (AStealthPawn* Pawn = PawnOf(Victim))
		{
			if (UDefenceStateComponent* State = UDefenceStateComponent::Find(*Pawn))
			{
				State->GetCore().Reset();
			}
		}
		if (UDefenceOwnerComponent* Owner = Victim.IsValid() ? Victim->GetDefenceOwner() : nullptr)
		{
			Owner->ClearCooldownEnd();
		}
	}

	void RunLeg(int32 LegIndex, NoWarningToVictimPieData::ECase Case, bool bSeam)
	{
		using namespace StealthAssassinationPie;
		using namespace NoWarningToVictimPieData;
		const bool bStruck = LegIndex == 1;
		Network
			.Then(*FString::Printf(TEXT("leg %d: place"), LegIndex), [this]()
			{
				Face(PawnOf(Victim), VictimStart, VictimFacing);
				Face(PawnOf(Attacker), AttackerStart, AttackerFacing);
				Mark = ServerNow(ServerWorld());
			})
			.Until(*FString::Printf(TEXT("leg %d: settle"), LegIndex), [this]() { Frames.Sample(); return ServerNow(ServerWorld()) >= Mark + SettleSec; });
		if (Case == ECase::Braced)
		{
			Network
				.Then(*FString::Printf(TEXT("leg %d: brace"), LegIndex), [this]()
				{
					ResetDefence();
					AsOwner(PawnOf(Victim), Seq, [](UPawnCommandComponent& Command) { Command.PrepareBegin(); });
					Mark = ServerNow(ServerWorld());
				})
				.Until(*FString::Printf(TEXT("leg %d: brace up"), LegIndex), [this]()
				{
					Frames.Sample();
					const AStealthPawn* Pawn = PawnOf(Victim);
					return Pawn != nullptr && Pawn->GetActionState().Pose == EActionPose::Brace && ServerNow(ServerWorld()) >= Mark + BraceLeadSec;
				});
		}
		Network
			.Then(*FString::Printf(TEXT("leg %d: begin"), LegIndex), [this, LegIndex, bSeam, Case]()
			{
				using namespace StealthOwnPresentationPie;
				FLeg& Leg = Legs[LegIndex];
				Leg.ServerOrigin = ServerNow(ServerWorld());
				Leg.VictimOrigin = ServerNow(Worlds()[MachineV]);
				Leg.StartLocation = PawnOf(Victim)->GetActorLocation();
				TWeakObjectPtr<AStealthPawn> Own = VictimLocalPawn();
				Leg.Hook.Begin(MachineV, [Own](const AStealthPawn* Pawn) { return Pawn != nullptr && Pawn == Own.Get() ? FString(TEXT("Victim")) : FString(); });
				Leg.Watchers[0].Begin(VictimLocalPawn(), MachineV);
				// The bystander's copy, unless the bystander is the listen host (no connection there to receive anything).
				Leg.Watchers[1].Begin(MachineB > 0 ? StealthPresentationPie::CopyOf(PawnOf(Victim), Worlds()[MachineB]) : nullptr, MachineB);
				StealthOwnerPresentationTestSeams::SetPresentOnRosterReceipt(bSeam);
				bMoving = true;
				// (a): the scripted Move on the victim's own machine: the W key held through its Enhanced Input, so the move
				// takes the production path (UPawnCommandClient -> UPawnCommandComponent::Move -> ServerMove).
				AStealthPawn* Local = VictimLocalPawn();
				if (Case == ECase::Moving && Local != nullptr)
				{
					const FNetworkPredictionData_Client_Character* Client = Local->GetCharacterMovement() != nullptr ? Local->GetCharacterMovement()->GetPredictionData_Client_Character() : nullptr;
					Leg.FirstMoveTimeStamp = Client != nullptr ? Client->CurrentTimeStamp : 0.f;
					MoveKey(true);
				}
			})
			.Until(*FString::Printf(TEXT("leg %d: to the strike"), LegIndex), [this, LegIndex, Case]()
			{
				SampleLeg(LegIndex, Case);
				return ServerNow(ServerWorld()) >= Legs[LegIndex].ServerOrigin + IssueAfter(Case);
			})
			.Then(*FString::Printf(TEXT("leg %d: %s"), LegIndex, bStruck ? TEXT("strike") : TEXT("no strike (control)")), [this, bStruck]()
			{
				if (bStruck)
				{
					StrikeAt(PawnOf(Attacker), PawnOf(Victim), Seq);
				}
			})
			.Until(*FString::Printf(TEXT("leg %d: through the contact"), LegIndex), [this, LegIndex, Case, bStruck]()
			{
				SampleLeg(LegIndex, Case);
				const FLeg& Leg = Legs[LegIndex];
				const bool bDone = ServerNow(ServerWorld()) >= Leg.ServerOrigin + IssueAfter(Case) + LegSec;
				return bDone && (!bStruck || Leg.DeathWrite > 0.0 || ServerNow(ServerWorld()) >= Leg.ServerOrigin + LegSec + 2.0);
			})
			.Then(*FString::Printf(TEXT("leg %d: stop"), LegIndex), [this, LegIndex]()
			{
				bMoving = false;
				Legs[LegIndex].Hook.End();
				MoveKey(false);
				StealthOwnerPresentationTestSeams::SetPresentOnRosterReceipt(false);
				Mark = ServerNow(ServerWorld());
			})
			.Until(*FString::Printf(TEXT("leg %d: come to rest"), LegIndex), [this, Case]()
			{
				Frames.Sample();
				return ServerNow(ServerWorld()) >= Mark + (Case == ECase::Braced ? BraceOverSec : SettleSec);
			});
	}

	/** Check 1 under the case's two legs: the struck leg's entries the control lacks, before the local contact frame. */
	TArray<StealthPresentationLog::FEntry> Warnings() const
	{
		using namespace NoWarningToVictimPieData;
		const FLeg& Control = Legs[0];
		const FLeg& Struck = Legs[1];
		const double From = Struck.Exec - Struck.ServerOrigin;
		const double To = Struck.Contact - 0.001 - Struck.ServerOrigin;
		return StealthPresentationLog::Unmatched(Struck.Hook.Collect(), Struck.VictimOrigin, Control.Hook.Collect(), Control.VictimOrigin, From, To, Frames.FrameOf(MachineV));
	}

	void Run(NoWarningToVictimPieData::ECase Case, bool bFront, bool bNegativeControl = false)
	{
		using namespace StealthAssassinationPie;
		using namespace NoWarningToVictimPieData;
		Network
			.Until(TEXT("the round is Active and the players split 2v2"), [this]()
			{
				const TArray<AStealthPlayerController*> All = ServerPlayers();
				if (!RoundActive() || All.Num() != 4)
				{
					return false;
				}
				// The victim: a remote client's player (its moves carry CMC timestamps). The attacker: an opponent (its ally keeps
				// its team in the round after a reversal; the victim's ally after a kill). The bystander: a remote player who is
				// neither, preferring the victim's ally. The fourth stands aside.
				AStealthPlayerController* const* Remote = All.FindByPredicate([](const AStealthPlayerController* C) { return !C->IsLocalController(); });
				if (Remote == nullptr)
				{
					return false;
				}
				AStealthPlayerController* V = *Remote;
				AStealthPlayerController* Ally = nullptr;
				TArray<AStealthPlayerController*> Opponents;
				for (AStealthPlayerController* C : All)
				{
					if (C == V)
					{
						continue;
					}
					if (TeamOf(C) == TeamOf(V))
					{
						Ally = C;
					}
					else
					{
						Opponents.Add(C);
					}
				}
				if (Ally == nullptr || Opponents.Num() != 2)
				{
					return false;
				}
				Victim = V;
				Attacker = Opponents[0];
				// The bystander must be a remote machine (the host has no connection to receive anything on).
				const bool bAllyRemote = !Ally->IsLocalController();
				Bystander = bAllyRemote ? Ally : Opponents[1];
				Parked = bAllyRemote ? Opponents[1] : Ally;
				MachineV = StealthPresentationPie::MachineOf(Victim.Get());
				MachineB = StealthPresentationPie::MachineOf(Bystander.Get());
				return MachineV > 0 && MachineB != INDEX_NONE && VictimLocalPawn() != nullptr;
			})
			.Then(TEXT("the placement"), [this, bFront, Case]()
			{
				AStealthPawn* VictimPawn = PawnOf(Victim);
				MoveClearOfBushes(VictimPawn);
				VictimStart = VictimPawn->GetActorLocation();
				VictimFacing = FRotator(0.0, VictimPawn->GetActorRotation().Yaw, 0.0);
				const FVector Forward = VictimFacing.Vector();
				const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
				const double Distance = Case == ECase::Moving ? (bFront ? MovingFrontCm : MovingBehindCm) : StrikeDistanceCm;
				AttackerStart = VictimStart + (bFront ? Forward : -Forward) * Distance;
				AttackerFacing = (VictimStart - AttackerStart).Rotation();
				AttackerFacing.Pitch = 0.0;
				MoveDirection = Right;
				Face(PawnOf(Bystander), VictimStart - Right * BystanderAsideCm, Right.Rotation());
				Face(PawnOf(Parked), VictimStart + Right * ParkedAsideCm, (-Right).Rotation());
			});
		RunLeg(0, Case, false);
		RunLeg(1, Case, bNegativeControl);

		Network.Then(TEXT("assert"), [this, Case, bFront, bNegativeControl]()
		{
			using namespace StealthPresentationLog;
			const FLeg& Control = Legs[0];
			const FLeg& Struck = Legs[1];
			const double Frame = Frames.FrameOf(MachineV);
			const TCHAR* Name = CaseName(Case);
			TestRunner->AddInfo(FString::Printf(TEXT("ASN05 %s from %s%s: exec %+.3f contact %+.3f death write %+.3f (struck leg); frames: victim %.4f s; victim machine %d, bystander %d"),
				Name, bFront ? TEXT("the front") : TEXT("behind"), bNegativeControl ? TEXT(" [NEGATIVE CONTROL]") : TEXT(""), Struck.Exec - Struck.ServerOrigin,
				Struck.Contact - Struck.ServerOrigin, Struck.DeathWrite - Struck.ServerOrigin, Frame, MachineV, MachineB));
			TestRunner->AddInfo(FString::Printf(TEXT("ASN05 struck log: %s"), *Describe(Struck.Hook.Collect(), Struck.VictimOrigin)));
			if (const AStealthPlayerController* Local = StealthOwnPresentationPie::LocalOf(MachineV))
			{
				TArray<FString> Tail;
				for (const FOwnPresentationRecord& R : Local->GetOwnPresentationLog())
				{
					Tail.Add(FString::Printf(TEXT("%s@%.3f %s"), LexOwnPresentationEvent(R.Event), R.ServerNow - Struck.VictimOrigin, *R.Text.Left(40)));
				}
				TestRunner->AddInfo(FString::Printf(TEXT("ASN05 victim machine %d controller %s own log: %s"), MachineV, *Local->GetName(), *FString::Join(Tail, TEXT("; "))));
			}
			TestRunner->AddInfo(FString::Printf(TEXT("ASN05 control log: %s"), *Describe(Control.Hook.Collect(), Control.VictimOrigin)));
			ASSERT_THAT(IsTrue(Struck.Exec > 0.0 && Struck.Contact > Struck.Exec && Struck.DeathWrite > 0.0, TEXT("the strike executed and a death write followed")));

			// 1 No warning before the local contact frame.
			const TArray<FEntry> Lacking = Warnings();
			TestRunner->AddInfo(FString::Printf(TEXT("ASN05 entries the control lacks before contact: %s"), *Describe(Lacking, Struck.VictimOrigin)));
			if (bNegativeControl)
			{
				ASSERT_THAT(IsTrue(Lacking.ContainsByPredicate([](const FEntry& E) { return E.Key.Contains(TEXT("[seam]")); }),
					TEXT("NEGATIVE CONTROL: the on-receipt seam's HUD line is reported, so check 1 fails under the seam")));
				return;
			}
			ASSERT_THAT(IsTrue(Lacking.IsEmpty(), TEXT("1 nothing on the victim's machine before contact that the control lacks")));

			// 2 The victim's server position at the last consumed move before the death write.
			const FMoveSample* LastStruck = nullptr;
			for (const FMoveSample& Sample : Struck.Moves)
			{
				if (Sample.ServerNow < Struck.DeathWrite)
				{
					LastStruck = &Sample;
				}
			}
			ASSERT_THAT(IsTrue(LastStruck != nullptr, TEXT("2 the victim's moves were sampled before the death write")));
			const FMoveSample* Match = nullptr;
			if (Case == ECase::Moving)
			{
				const float RelStruck = LastStruck->ClientTimeStamp - Struck.FirstMoveTimeStamp;
				float Best = TNumericLimits<float>::Max();
				for (const FMoveSample& Sample : Control.Moves)
				{
					const float Delta = FMath::Abs((Sample.ClientTimeStamp - Control.FirstMoveTimeStamp) - RelStruck);
					if (Delta < Best)
					{
						Best = Delta;
						Match = &Sample;
					}
				}
				Match = Best <= 0.5f / NoWarningToVictimPieData::FixedFrameRate ? Match : nullptr;
			}
			else
			{
				const double RelStruck = LastStruck->ServerNow - Struck.ServerOrigin;
				double Best = TNumericLimits<double>::Max();
				for (const FMoveSample& Sample : Control.Moves)
				{
					const double Delta = FMath::Abs((Sample.ServerNow - Control.ServerOrigin) - RelStruck);
					if (Delta < Best)
					{
						Best = Delta;
						Match = &Sample;
					}
				}
			}
			const FVector StruckMoved = LastStruck->Location - Struck.StartLocation;
			const FVector ControlMoved = Match != nullptr ? Match->Location - Control.StartLocation : FVector::ZeroVector;
			const double Distance = Match != nullptr ? FVector::Dist(StruckMoved, ControlMoved) : -1.0;
			TestRunner->AddInfo(FString::Printf(TEXT("ASN05 scripted move: control [%s], struck [%s]"), *Control.MoveDiagnostics, *Struck.MoveDiagnostics));
			TestRunner->AddInfo(FString::Printf(TEXT("ASN05 displacement at the last consumed move before the death write: struck %s, control %s, %.3f cm apart (struck at %s)"),
				*StruckMoved.ToString(), *ControlMoved.ToString(), Distance, *LastStruck->Location.ToString()));
			ASSERT_THAT(IsTrue(Match != nullptr && Distance <= PositionToleranceCm, TEXT("2 the victim's server position matches the control's within 1 cm")));

			// 3 The send capture's receipt half: no property change on the victim's copies from the slot to the death write
			// except movement / CMC fields that change in the control's same window.
			for (int32 Watcher = 0; Watcher < 2; ++Watcher)
			{
				TSet<FString> ControlMovement;
				for (const FPropertyChange& Change : Control.Watchers[Watcher].GetChanges())
				{
					const double Rel = Change.ServerNow - Control.ServerOrigin;
					if (FReplicatedPropertyWatcher::IsMovementProperty(Change.Property) && Rel >= Struck.Exec - Struck.ServerOrigin - Frame && Rel < Struck.DeathWrite - Struck.ServerOrigin + Frame)
					{
						ControlMovement.Add(Change.Property);
					}
				}
				TArray<FString> Violations;
				for (const FPropertyChange& Change : Struck.Watchers[Watcher].GetChanges())
				{
					if (Change.ServerNow < Struck.Exec || Change.ServerNow >= Struck.DeathWrite + Frame)
					{
						continue;
					}
					// The death write's own fields, received on its tick (the server sees the write one frame late; the client's
					// estimate is within ClockToleranceSec of the server clock).
					const bool bDeathWriteOwn = Change.ServerNow + Frame + ClockToleranceSec >= Struck.DeathWrite
						&& (Change.Property.EndsWith(TEXT(".DeathState")) || Change.Property.EndsWith(TEXT(".ActionState")));
					const bool bMovementAsControl = FReplicatedPropertyWatcher::IsMovementProperty(Change.Property) && ControlMovement.Contains(Change.Property);
					if (!bDeathWriteOwn && !bMovementAsControl)
					{
						Violations.Add(FString::Printf(TEXT("%s at %+.3f"), *Change.Property, Change.ServerNow - Struck.ServerOrigin));
					}
				}
				TestRunner->AddInfo(FString::Printf(TEXT("ASN05 %s copy: %d change(s) in the window; control movement fields [%s]; violations [%s]"),
					Watcher == 0 ? TEXT("victim machine") : TEXT("bystander machine"), Struck.Watchers[Watcher].GetChanges().Num(), *FString::Join(ControlMovement.Array(), TEXT(", ")),
					*FString::Join(Violations, TEXT(", "))));
				ASSERT_THAT(IsTrue(Violations.IsEmpty(), *FString::Printf(TEXT("3 no outcome-caused property change reaches the %s before the death write"), Watcher == 0 ? TEXT("victim") : TEXT("bystander"))));
			}
		});
	}

	TEST_METHOD(UnbracedMovingFromBehind) { Run(NoWarningToVictimPieData::ECase::Moving, false); }
	TEST_METHOD(UnbracedMovingFromFront) { Run(NoWarningToVictimPieData::ECase::Moving, true); }
	TEST_METHOD(UnbracedStandingFromBehind) { Run(NoWarningToVictimPieData::ECase::Standing, false); }
	TEST_METHOD(UnbracedStandingFromFront) { Run(NoWarningToVictimPieData::ECase::Standing, true); }
	TEST_METHOD(BracedDefenderFromBehind) { Run(NoWarningToVictimPieData::ECase::Braced, false); }
	TEST_METHOD(BracedDefenderFromFront) { Run(NoWarningToVictimPieData::ECase::Braced, true); }
	TEST_METHOD(NegativeControlWidgetOnRosterReceiptIsReported) { Run(NoWarningToVictimPieData::ECase::Standing, false, /*bNegativeControl*/ true); }
};

#endif
