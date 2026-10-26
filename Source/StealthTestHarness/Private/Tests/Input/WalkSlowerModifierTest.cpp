// Story iif-018 (production/epics/input-interaction-framework/story-018-walk-slower-two-client.md), GDD AC 76 server
// clause (R15a, Integration, P WalkSlowerModifier): GIVEN a keyboard walker holding W (turning slowly, so it walks a circle in the open street) WHEN it holds Left Ctrl, then
// Left Alt, then both, then neither, for 3 s each THEN the server-received Move magnitude is 0.8, 0.9, 0.8 and 1.0, the
// pace follows, every speed change follows the baseline acceleration or braking (no step above it) and no RPC other
// than ServerMove is sent; AND holding Run with Left Ctrl gives run speed.
// CQTest: NETWORK_TEST_CLASS(WalkSlowerModifier, "StealthGame.Input.PIE"), listen server + 2 clients (the AC's two PIE
// clients; client 1 walks, client 2 is the second connection), no net emulation. Keys are injected on client 1's
// controller (simulated key events through Enhanced Input, as spike V1).
// What the server measures: the received acceleration magnitude of client 1's pawn over the CMC's max acceleration
// (ServerMove carries the acceleration; the CMC clamps it to 1), and the steady-state 2D speed. Recorded deviations (see
// the story's Completion Notes): since SCL-010 the absolute paces 112 / 126 / 140 cm/s are asserted (WalkSpeedCmPerSec
// 140 and the pace quantiser), the run phase at RunSpeedCmPerSec 450 and with no RPC beyond the move family (the sample
// sprint shim and its UpdateInputState_Server are gone); "baseline acceleration" is the movement model's acceleration
// and braking bound (max acceleration plus max braking deceleration) on the owner's per-frame simulation; the informant
// clause uses stub state (the prototype owner state Informant plus IMC_Informant on the owner; the Roster-driven swap is
// story iif-071) and repeats the four modifier phases; "only ServerMove" allows the engine's
// ServerUpdateCamera background (ADR-0015 disables client camera updates later).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Input/RpcSpy.h"
#include "InputKeyEventArgs.h"
#include "StealthGameMode.h"
#include "StealthPlayerController.h"
#include "StealthGameState.h"
#include "StealthPlayerState.h"
#include "StealthSettings.h"
#include "View/Input/PawnCommandClient.h"
#include "View/Core/StealthPawn.h"
#include "View/Core/WalkSlowerScale.h"

namespace WalkSlowerModifierTestData
{
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	const TCHAR* const NetworkTimeoutCeilingSeconds = TEXT("300");
	/** 3 s per phase at the PIE frame rate is measured in frames, never wall time: about 3 s at 60 Hz. */
	constexpr uint64 PhaseFrames = 180;
	/** The steady-state window at the end of each phase. */
	constexpr uint64 SampleFrames = 60;
	constexpr double MagnitudeTolerance = 0.03;
	constexpr double PaceRatioTolerance = 0.05;
	/** Steady-state speed tolerance of the absolute paces (the walker turns a slow circle). */
	constexpr double AbsolutePaceToleranceCm = 3.0;

	struct FPhase
	{
		const TCHAR* Name;
		bool bCtrl;
		bool bAlt;
		bool bRun;
		double ExpectedMagnitude;
		/** The walker is an informant (prototype owner state + IMC_Informant, which keeps IMC_Common's walk-slower). */
		bool bInformant = false;
		/**
		 * The phase starts from a stop the test forced (not a modifier change) right after the sample's sprint: neither its
		 * transition nor its RPCs (the sample's own UpdateInputState_Server on the Shift release) are checked.
		 */
		bool bSkipStepCheck = false;
	};
	const FPhase Phases[] = {
		{ TEXT("neither (reference)"), false, false, false, StealthPaceSet::Full },
		{ TEXT("Left Ctrl"), true, false, false, StealthPaceSet::Slow },
		{ TEXT("Left Alt"), false, true, false, StealthPaceSet::Middle },
		{ TEXT("both"), true, true, false, StealthPaceSet::Slow },
		{ TEXT("neither"), false, false, false, StealthPaceSet::Full },
		{ TEXT("Run + Left Ctrl"), true, false, true, StealthPaceSet::Full },
		// The first informant phase also absorbs the stop after the sample's sprint and the swap (no step check there).
		{ TEXT("informant: neither (settle)"), false, false, false, StealthPaceSet::Full, true, true },
		{ TEXT("informant: Left Ctrl"), true, false, false, StealthPaceSet::Slow, true },
		{ TEXT("informant: Left Alt"), false, true, false, StealthPaceSet::Middle, true },
		{ TEXT("informant: both"), true, true, false, StealthPaceSet::Slow, true },
	};

	/** Yaw added to the walker's control rotation per frame, so it walks a circle (~3 m radius) instead of into a wall. */
	constexpr double CircleYawPerFrameDeg = 0.6;
	/** The run phase turns this much faster (90 deg/s, inside RunTurnRateDegPerSec 180): a ~2.9 m circle at 450 cm/s. */
	constexpr double RunCircleYawFactor = 2.5;
	/** The no-step clause is checked in the transition window after each modifier change. */
	constexpr uint64 TransitionFrames = 40;

	struct FSample
	{
		double TargetSum = 0.0;
		/** Client-to-server RPCs of this phase beyond the engine move/camera background. */
		FString OtherRpcs;
		double MagnitudeSum = 0.0;
		double SpeedSum = 0.0;
		int32 Count = 0;
		double MaxStepOver = 0.0;
	};
}

NETWORK_TEST_CLASS(WalkSlowerModifier, "StealthGame.Input.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	/** CQTest keeps the description pointer: the label strings must outlive the command queue. */
	TArray<FString> Labels;
	const TCHAR* L(FString Label) { return *Labels.Add_GetRef(MoveTemp(Label)); }
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	TUniquePtr<FOutgoingRpcSpy> Spy;
	TArray<WalkSlowerModifierTestData::FSample> Samples;
	uint64 PhaseStart = 0;
	double LastSpeed = -1.0;
	double LastTime = 0.0;
	int32 ClientIndexSeen = 0;
	bool bInformantSet = false;
	uint64 Mark = 0;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(WalkSlowerModifierTestData::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
		}
		// No crowd: since SCL-018 the crowd walks through Move like a player, and a crowd pawn crossing the walker's circle
		// blocked it (a flaky 'both' phase, 2026-10-26); the measured paces need an open street (spike SCL-045 pattern).
		UStealthSettings* Settings = GetMutableDefault<UStealthSettings>();
		PreviousNpcCount = Settings->NpcCount;
		Settings->NpcCount = 0;
	}

	AFTER_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			if (!PreviousNetworkTimeout.IsEmpty())
			{
				Timeout->Set(*PreviousNetworkTimeout, ECVF_SetByCode);
			}
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
		Network.StartOnFixtureMap(WalkSlowerModifierTestData::FixtureMap);
	}

	AFTER_EACH()
	{
		Spy.Reset();
	}

	static UWorld* FirstClientWorld()
	{
		const FWorldContext* Best = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetNetMode() == NM_Client && (!Best || Context.PIEInstance < Best->PIEInstance))
			{
				Best = &Context;
			}
		}
		return Best ? Best->World() : nullptr;
	}

	static APlayerController* WalkerController() { UWorld* W = FirstClientWorld(); return W ? W->GetFirstPlayerController() : nullptr; }

	static UWorld* ServerWorldOf()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetNetMode() == NM_ListenServer)
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	/** The server's copy of the walker's pawn (matched through the walker's server-side controller). */
	static ACharacter* ServerWalker(UWorld* Server)
	{
		const APlayerController* Client = WalkerController();
		const APlayerState* ClientState = Client ? Client->PlayerState.Get() : nullptr;
		for (FConstPlayerControllerIterator It = Server->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && !PC->IsLocalController() && PC->PlayerState && ClientState && PC->PlayerState->GetPlayerId() == ClientState->GetPlayerId())
			{
				return Cast<ACharacter>(PC->GetPawn());
			}
		}
		return nullptr;
	}

	static void Key(const FKey& InKey, bool bDown)
	{
		if (APlayerController* PC = WalkerController())
		{
			PC->InputKey(FInputKeyEventArgs::CreateSimulated(InKey, bDown ? IE_Pressed : IE_Released, bDown ? 1.f : 0.f));
		}
	}

	TEST_METHOD(KeyboardModifiersScaleTheServerReceivedMove)
	{
		using namespace WalkSlowerModifierTestData;
		Network
			.Until(TEXT("the walker possesses its pawn on both sides"), []()
			{
				UWorld* Server = nullptr;
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					Server = Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetNetMode() == NM_ListenServer ? Context.World() : Server;
				}
				// R6: pawns exist in Preparing but commands are NotNow until Active (the real readiness sequence).
				const AStealthGameState* ClientState = WalkerController() && WalkerController()->GetWorld() ? WalkerController()->GetWorld()->GetGameState<AStealthGameState>() : nullptr;
				return Server && WalkerController() && Cast<AStealthPawn>(WalkerController()->GetPawn()) && ServerWalker(Server)
					&& ClientState != nullptr && ClientState->GetRoundPublicState().Phase == ERoundPhase::Active;
			})
			.Then(TEXT("attach the RPC spy"), [this]()
			{
				Spy = MakeUnique<FOutgoingRpcSpy>(FirstClientWorld());
				ASSERT_THAT(IsTrue(Spy->IsAttached(), TEXT("RPC spy attached to the walker's NetDriver")));
			});

		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Phases); ++Index)
		{
			const FPhase Phase = Phases[Index];
			if (Phase.bInformant && (Index == 0 || !Phases[Index - 1].bInformant))
			{
				Network.Then(TEXT("the walker becomes an informant"), [this]()
				{
						// The informant stub state: the prototype owner state on the server (the Roster transition is not
						// driven in this PIE) and the informant context on the owner (story iif-071 wires it to the Roster).
						if (UWorld* Server = ServerWorldOf())
						{
							if (ACharacter* Walker = ServerWalker(Server))
							{
								if (AStealthPlayerState* State = Walker->GetPlayerState<AStealthPlayerState>())
								{
									State->LifeState = EStealthLifeState::Informant;
								}
							}
						}
						if (AStealthPlayerController* Owner = Cast<AStealthPlayerController>(WalkerController()))
						{
							// The swap ignores held keys until release (ADR-0011 E3): release W first; it is pressed afresh below.
							Key(EKeys::W, false);
							Key(EKeys::LeftControl, false);
							Key(EKeys::LeftAlt, false);
							Owner->GetCommandClient()->SetInformantContext(true);
						}
						bInformantSet = true;
									});
				Network.Until(TEXT("the informant context settles"), [this]() { Mark = Mark ? Mark : GFrameCounter; const bool bDone = GFrameCounter >= Mark + 15; if (bDone) { Mark = 0; } return bDone; });
			}
			Network
				.Then(L(FString::Printf(TEXT("phase %s: set the modifiers"), Phase.Name)), [this, Phase, Index]()
				{
					Key(EKeys::W, true);
					Key(EKeys::LeftControl, Phase.bCtrl);
					Key(EKeys::LeftAlt, Phase.bAlt);
					Key(EKeys::LeftShift, Phase.bRun);
					PhaseStart = GFrameCounter;
					Samples.AddDefaulted();
					LastSpeed = -1.0;
					Spy->Records.Reset();
				});
			Network.UntilServer(L(FString::Printf(TEXT("phase %s: 3 s, sampled at the end"), Phase.Name)), [this, Phase](FBasePIENetworkComponentState& Server)
				{
					ACharacter* Walker = ServerWalker(Server.World);
					if (Walker && GFrameCounter > PhaseStart + PhaseFrames - SampleFrames)
					{
						const UCharacterMovementComponent* Move = Walker->GetCharacterMovement();
						FSample& S = Samples.Last();
						S.MagnitudeSum += Move->GetCurrentAcceleration().Size() / FMath::Max(Move->GetMaxAcceleration(), 1.f);
						S.SpeedSum += Walker->GetVelocity().Size2D();
						// The pace the movement model targets (SCL F1: its max speed already applies the quantised pace).
						S.TargetSum += Move->GetMaxSpeed();
						++S.Count;
					}
					// No step above the movement component's own acceleration / braking model on any frame of the owner's
					// simulation (the server applies moves in packed batches, so per-server-tick deltas are not per-move).
					APlayerController* WalkerPC = WalkerController();
					const ACharacter* Owner = WalkerPC ? Cast<ACharacter>(WalkerPC->GetPawn()) : nullptr;
					if (WalkerPC)
					{
						// Running turns faster so the run circle stays as tight as the walk circle (radius = speed / yaw rate).
						WalkerPC->SetControlRotation(WalkerPC->GetControlRotation() + FRotator(0.0, CircleYawPerFrameDeg * (Phase.bRun ? RunCircleYawFactor : 1.0), 0.0));
					}
					if (Owner && GFrameCounter <= PhaseStart + TransitionFrames)
					{
						const UCharacterMovementComponent* Move = Owner->GetCharacterMovement();
						const double Now = Owner->GetWorld()->GetTimeSeconds();
						const double Speed = Owner->GetVelocity().Size2D();
						if (LastSpeed >= 0.0 && Now > LastTime)
						{
							const double Dt = Now - LastTime;
							const double Friction = (Move->bUseSeparateBrakingFriction ? Move->BrakingFriction : Move->GroundFriction) * FMath::Max(Move->BrakingFrictionFactor, 0.f);
							// A reversal (W to S) combines acceleration with braking friction, so the bound is their sum: still the
							// movement component's own rates, never a snap.
							const double Limit = (Move->GetMaxAcceleration() + Move->GetMaxBrakingDeceleration() + Friction * FMath::Max(Speed, LastSpeed)) * Dt + 1.0;
							Samples.Last().MaxStepOver = FMath::Max(Samples.Last().MaxStepOver, FMath::Abs(Speed - LastSpeed) - Limit);
						}
						LastSpeed = Speed;
						LastTime = Now;
					}
					if (GFrameCounter >= PhaseStart + PhaseFrames)
					{
						Samples.Last().OtherRpcs = FOutgoingRpcSpy::Describe(Spy->Excluding([](FName F) { return FOutgoingRpcSpy::IsEngineBackground(F); }));
						return true;
					}
					return false;
				});
		}

		Network.Then(TEXT("release and assert"), [this]()
		{
			Key(EKeys::W, false);
			Key(EKeys::S, false);
			Key(EKeys::LeftControl, false);
			Key(EKeys::LeftAlt, false);
			Key(EKeys::LeftShift, false);
			const FSample& Reference = Samples[0];
			const double ReferenceSpeed = Reference.Count ? Reference.SpeedSum / Reference.Count : 0.0;
			const double ReferenceTarget = Reference.Count ? Reference.TargetSum / Reference.Count : 0.0;
			ASSERT_THAT(IsTrue(ReferenceSpeed > 10.0, L(FString::Printf(TEXT("the walker moved (reference speed %.1f cm/s)"), ReferenceSpeed))));
			for (int32 Index = 0; Index < Samples.Num(); ++Index)
			{
				const FPhase& Phase = Phases[Index];
				const FSample& S = Samples[Index];
				const double Magnitude = S.Count ? S.MagnitudeSum / S.Count : 0.0;
				const double Speed = S.Count ? S.SpeedSum / S.Count : 0.0;
				const double Target = S.Count ? S.TargetSum / S.Count : 0.0;
				TestRunner->AddInfo(FString::Printf(TEXT("WALKSLOWER %s: server-received magnitude %.3f (expected %.2f), target pace %.1f cm/s (ratio %.3f), measured speed %.1f cm/s, max step over limit %.2f"),
					Phase.Name, Magnitude, Phase.ExpectedMagnitude, Target, ReferenceTarget > 0.0 ? Target / ReferenceTarget : 0.0, Speed, S.MaxStepOver));
				ASSERT_THAT(IsNear(Phase.ExpectedMagnitude, Magnitude, MagnitudeTolerance, L(FString::Printf(TEXT("%s: server-received Move magnitude"), Phase.Name))));
				if (Phase.bSkipStepCheck)
				{
					// The settle phase only absorbs the stop after the run and the informant swap.
				}
				else if (!Phase.bRun)
				{
					ASSERT_THAT(IsNear(Phase.ExpectedMagnitude, Target / ReferenceTarget, PaceRatioTolerance, L(FString::Printf(TEXT("%s: pace as a ratio of the walk speed"), Phase.Name))));
					// SCL-010: the absolute pace the server's movement model targets, WalkSpeedCmPerSec x the quantised pace
					// (112 / 126 / 140 cm/s). The measured speed is logged only: the drifting circle can graze level geometry.
					ASSERT_THAT(IsNear(140.0 * Phase.ExpectedMagnitude, Target, AbsolutePaceToleranceCm, L(FString::Printf(TEXT("%s: absolute pace %.1f cm/s"), Phase.Name, Target))));
				}
				else
				{
					ASSERT_THAT(IsNear(450.0, Target, AbsolutePaceToleranceCm, L(FString::Printf(TEXT("%s: the run tier at 450 cm/s (%.1f)"), Phase.Name, Target))));
				}
				if (!Phase.bSkipStepCheck)
				{
					ASSERT_THAT(IsTrue(S.MaxStepOver <= 0.0, L(FString::Printf(TEXT("%s: no speed step above the acceleration/braking limit"), Phase.Name))));
				}
			}
			for (int32 Index = 0; Index < Samples.Num(); ++Index)
			{
				TestRunner->AddInfo(FString::Printf(TEXT("WALKSLOWER %s: other client-to-server RPCs [%s]"), Phases[Index].Name, *Samples[Index].OtherRpcs));
				// Since SCL-010 IA_Run consumes LeftShift and the gait rides ServerMove (FLAG_Custom_0): every phase, Run
				// included, sends nothing but the move family.
				if (!Phases[Index].bSkipStepCheck)
				{
					ASSERT_THAT(IsTrue(Samples[Index].OtherRpcs.IsEmpty(), L(FString::Printf(TEXT("%s: no client-to-server RPC beyond ServerMove: [%s]"), Phases[Index].Name, *Samples[Index].OtherRpcs))));
				}
			}
		});
	}
};

#endif
