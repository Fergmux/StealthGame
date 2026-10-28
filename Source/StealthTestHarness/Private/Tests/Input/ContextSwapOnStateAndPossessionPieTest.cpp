// Story iif-071 (production/epics/input-interaction-framework/story-071-context-swap-on-state-and-possession.md), GDD
// AC 55 (R13, Integration, P ContextSwapOnStateAndPossession): GIVEN an Active player WHEN Roster's Owner-audience
// state changes to Informant, and separately WHEN the pawn is re-possessed THEN IMC_Informant (resp. the
// class-appropriate context) is active within one frame; AND all held inputs are flushed.
// How: listen server + 1 client on the fixture map, crowd-less, the pair staged on one clear disc before any input. (1)
// Re-possession while Active with Interact held: the server UnPossesses and re-possesses the client's pawn; on the
// owner the Interact End goes out at the ClientRestart and IMC_Active stays the context. (2) The state change: with
// Interact held, the host's development bot strikes the client (the production Roster records Active -> Informant
// through the D53 adapters); the owner's C-TeamView update swaps IMC_Active for IMC_Informant in the same frame the
// view arrives (frame delta <= 1) and the held Interact is ended by that swap. No HUD presenter is bound (MVP), so the
// raw state decides at once (StealthOwnerContext gate).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"
#include "Tests/PIE/StealthPIEOpenCircle.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "StealthGameMode.h"
#include "StealthPlayerController.h"
#include "StealthGameState.h"
#include "StealthBush.h"
#include "StealthSettings.h"
#include "View/Core/StealthPawn.h"
#include "View/Input/PawnCommandClient.h"
#include "View/StealthRosterTeamView.h"


namespace ContextSwapRoundGate
{
	/** The client's replicated Round phase is Active (R6: pawns exist in Preparing, but commands are NotNow until Active). */
	inline bool IsClientRoundActive(const APlayerController* PC)
	{
		const AStealthGameState* GS = PC && PC->GetWorld() ? PC->GetWorld()->GetGameState<AStealthGameState>() : nullptr;
		return GS != nullptr && GS->GetRoundPublicState().Phase == ERoundPhase::Active;
	}
}

namespace ContextSwapTestData
{
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	const TCHAR* const NetworkTimeoutCeilingSeconds = TEXT("300");
	constexpr uint64 SettleFrames = 12;
	constexpr uint64 StrikeCeilingFrames = 900;
	/**
	 * The strike pair's stand. 2026-10-28 full-gate flake (seed 1699684516, 50 NPCs): the host's bot struck NoContact 0.75 s
	 * into the round, so the client never became an Informant. strike_enemy's approach teleports at the bot's own Z and
	 * through whatever stands there: a host on higher ground than the client strikes NoContact (watched failing,
	 * production/qa/evidence/asn-pie-flake-fix-2026-10-28/contextswap-watch-fail.txt). The round is crowd-less and both pawns
	 * stand on one clear, flat disc this far apart before any input, so the approach spot is the host's own stand.
	 */
	constexpr double StrikeDistanceCm = 120.0;
	constexpr double StandRadiusCm = 90.0;
	constexpr double StandMarginCm = 60.0;
	constexpr uint64 StageSettleFrames = 30;
}

NETWORK_TEST_CLASS(ContextSwapOnStateAndPossession, "StealthGame.Input.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;

	inline static int32 PreviousNpcCount = -1;

	TArray<TPair<ECommandKind, uint64>> Sent;
	uint64 Mark = 0;
	uint64 RestartFrame = 0;
	uint64 ViewInformantFrame = 0;
	uint64 SwapFrame = 0;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousNetworkTimeout = Timeout->GetString();
			Timeout->Set(ContextSwapTestData::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
		}
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
			.WithClients(1)
			.WithGameMode(AStealthGameMode::StaticClass())
			.Build(Network);
		Network.StartOnFixtureMap(ContextSwapTestData::FixtureMap);
	}

	static AStealthPlayerController* Owner()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetNetMode() == NM_Client)
			{
				return Cast<AStealthPlayerController>(Context.World()->GetFirstPlayerController());
			}
		}
		return nullptr;
	}

	static UWorld* ServerWorld()
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

	static APlayerController* ServerSideOwner()
	{
		const AStealthPlayerController* Client = Owner();
		UWorld* Server = ServerWorld();
		if (!Server)
		{
			return nullptr;
		}
		for (FConstPlayerControllerIterator It = Server->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && !PC->IsLocalController() && PC->PlayerState && Client && Client->PlayerState && PC->PlayerState->GetPlayerId() == Client->PlayerState->GetPlayerId())
			{
				return PC;
			}
		}
		return nullptr;
	}

	static bool HasContext(const TCHAR* Name)
	{
		const AStealthPlayerController* PC = Owner();
		const UEnhancedInputLocalPlayerSubsystem* Subsystem = PC && PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
		const UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, *FString::Printf(TEXT("/Game/StealthGame/Input/%s.%s"), Name, Name));
		return Subsystem && Context && Subsystem->HasMappingContext(Context);
	}

	static void Key(const FKey& InKey, bool bDown)
	{
		if (AStealthPlayerController* PC = Owner())
		{
			PC->InputKey(FInputKeyEventArgs::CreateSimulated(InKey, bDown ? IE_Pressed : IE_Released, bDown ? 1.f : 0.f));
		}
	}

	/** The host's own pawn on the server. */
	static APawn* HostPawn()
	{
		UWorld* Server = ServerWorld();
		const APlayerController* Host = Server ? Server->GetFirstPlayerController() : nullptr;
		return Host && Host->IsLocalController() ? Host->GetPawn() : nullptr;
	}

	/**
	 * Stages the strike pair before any input: the client's pawn and the host's on a clear, flat disc (nothing standing on it,
	 * > 900 cm from every bush), StrikeDistanceCm apart, the host facing the client. False when no disc is clear.
	 */
	static bool StageStrikePair()
	{
		using namespace ContextSwapTestData;
		UWorld* Server = ServerWorld();
		APlayerController* ServerOwner = ServerSideOwner();
		APawn* Target = ServerOwner ? ServerOwner->GetPawn() : nullptr;
		APawn* Striker = HostPawn();
		const ACharacter* TargetCharacter = Cast<ACharacter>(Target);
		if (!Server || !TargetCharacter || !Striker)
		{
			return false;
		}
		const float HalfHeight = TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		StealthPIEOpenCircle::FParams Params;
		Params.RadiusCm = StandRadiusCm;
		Params.MarginCm = StandMarginCm;
		Params.AlsoIgnored.Add(Striker);
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
		const TOptional<FVector> Centre = StealthPIEOpenCircle::FindOpenCircle(Server, Target->GetActorLocation() - FVector(0.0, 0.0, HalfHeight), Target, Params);
		if (!Centre.IsSet())
		{
			return false;
		}
		const FVector Floor = Centre.GetValue() + FVector(0.0, 0.0, HalfHeight + 2.0);
		const FVector Axis(1.0, 0.0, 0.0);
		Target->TeleportTo(Floor - Axis * (StrikeDistanceCm * 0.5), Target->GetActorRotation(), false, true);
		Striker->TeleportTo(Floor + Axis * (StrikeDistanceCm * 0.5), (-Axis).Rotation(), false, true);
		return true;
	}

	int32 Count(ECommandKind Kind, uint64 Since) const
	{
		return Sent.FilterByPredicate([Kind, Since](const TPair<ECommandKind, uint64>& S) { return S.Key == Kind && S.Value >= Since; }).Num();
	}

	TEST_METHOD(StateChangeAndRepossessionSwapContextAndFlush)
	{
		using namespace ContextSwapTestData;
		Network
			.Until(TEXT("the client is an Active player with a pawn"), []()
			{
				const AStealthPlayerController* PC = Owner();
				return PC && Cast<AStealthPawn>(PC->GetPawn()) && ContextSwapRoundGate::IsClientRoundActive(PC) && PC->GetTeamView() && PC->GetTeamView()->GetView().Owner.State == static_cast<uint8>(EParticipantState::Active)
					&& HasContext(TEXT("IMC_Active"));
			})
			.Then(TEXT("stage the strike pair on a clear disc"), [this]()
			{
				ASSERT_THAT(IsTrue(StageStrikePair(), TEXT("a clear, flat disc away from the bushes for the strike pair")));
				Mark = GFrameCounter;
			})
			.Until(TEXT("the pair settles"), [this]() { return GFrameCounter >= Mark + ContextSwapTestData::StageSettleFrames; })
			.Then(TEXT("observe the owner"), [this]()
			{
				Owner()->GetCommandClient()->OnOwnerCommandPredicted.AddLambda([this](ECommandKind Kind, FClientCommandSeq) { Sent.Add({ Kind, GFrameCounter }); });
				Owner()->GetTeamView()->OnViewUpdated.AddLambda([this](const FTeamView& View)
				{
					if (View.Owner.State == static_cast<uint8>(EParticipantState::Informant) && ViewInformantFrame == 0)
					{
						ViewInformantFrame = GFrameCounter;
					}
				});
				Key(EKeys::E, true);
				Mark = GFrameCounter;
			})
			.Until(TEXT("Interact begun"), [this]() { return GFrameCounter >= Mark + SettleFrames; })
			// (1) Re-possession while Active.
			.Then(TEXT("the server re-possesses the client's pawn"), [this]()
			{
				ASSERT_THAT(AreEqual(1, Count(ECommandKind::InteractBegin, Mark), TEXT("one InteractBegin while held")));
				APlayerController* Server = ServerSideOwner();
				ASSERT_THAT(IsNotNull(Server, TEXT("the server-side controller")));
				APawn* Pawn = Server->GetPawn();
				Server->UnPossess();
				Server->Possess(Pawn);
				Mark = GFrameCounter;
			})
			.Until(TEXT("the owner restarted"), [this]()
			{
				// The trace records the owner's ClientRestart frames.
				for (const FStealthPossessionTraceEvent& E : AStealthPlayerController::GetPossessionTrace())
				{
					if (E.bClientWorld && E.Kind == FStealthPossessionTraceEvent::EKind::OwnerRestart && E.Frame >= Mark && E.Pawn != TEXT("None"))
					{
						RestartFrame = E.Frame;
						return true;
					}
				}
				return GFrameCounter > Mark + StrikeCeilingFrames;
			})
			.Then(TEXT("(1) the held Interact was flushed; IMC_Active stays"), [this]()
			{
				const TArray<TPair<ECommandKind, uint64>> Ends = Sent.FilterByPredicate([this](const TPair<ECommandKind, uint64>& S) { return S.Key == ECommandKind::InteractEnd && S.Value >= Mark; });
				TestRunner->AddInfo(FString::Printf(TEXT("CTXSWAP (1) restart frame %llu, ends %d at %llu, active %d informant %d"), RestartFrame, Ends.Num(), Ends.Num() ? Ends[0].Value : 0,
					HasContext(TEXT("IMC_Active")) ? 1 : 0, HasContext(TEXT("IMC_Informant")) ? 1 : 0));
				ASSERT_THAT(IsTrue(RestartFrame != 0, TEXT("the owner ran ClientRestart")));
				ASSERT_THAT(IsTrue(Ends.Num() >= 1 && Ends[0].Value <= RestartFrame + 1, TEXT("the held Interact was ended within a frame of the re-possession")));
				ASSERT_THAT(IsTrue(HasContext(TEXT("IMC_Active")) && !HasContext(TEXT("IMC_Informant")), TEXT("an Active player keeps IMC_Active")));
				Key(EKeys::E, false);
				Mark = GFrameCounter;
			})
			.Until(TEXT("release settles"), [this]() { return GFrameCounter >= Mark + SettleFrames; })
			// (2) Active -> Informant with Interact held.
			.Then(TEXT("hold Interact; the host's bot strikes the client"), [this]()
			{
				Key(EKeys::E, true);
				Mark = GFrameCounter;
			})
			.Until(TEXT("Interact begun again"), [this]() { return GFrameCounter >= Mark + SettleFrames; })
			.Then(TEXT("strike"), [this]()
			{
				ASSERT_THAT(AreEqual(1, Count(ECommandKind::InteractBegin, Mark), TEXT("a fresh InteractBegin")));
				AStealthGameMode* Mode = ServerWorld()->GetAuthGameMode<AStealthGameMode>();
				ASSERT_THAT(IsNotNull(Mode, TEXT("the server game mode")));
				Mode->DebugBotAction(0, TEXT("strike_enemy"));
				Mark = GFrameCounter;
			})
			.Until(TEXT("the owner's view says Informant and the context swapped"), [this]()
			{
				if (SwapFrame == 0 && Owner() && Owner()->GetCommandClient()->IsInformantContext())
				{
					SwapFrame = GFrameCounter;
				}
				return (ViewInformantFrame != 0 && SwapFrame != 0) || GFrameCounter > Mark + StrikeCeilingFrames;
			})
			.Then(TEXT("(2) IMC_Informant within a frame; the held Interact flushed"), [this]()
			{
				const TArray<TPair<ECommandKind, uint64>> Ends = Sent.FilterByPredicate([this](const TPair<ECommandKind, uint64>& S) { return S.Key == ECommandKind::InteractEnd && S.Value >= Mark; });
				TestRunner->AddInfo(FString::Printf(TEXT("CTXSWAP (2) view informant frame %llu, swap frame %llu, ends %d at %llu, active %d informant %d"),
					ViewInformantFrame, SwapFrame, Ends.Num(), Ends.Num() ? Ends[0].Value : 0, HasContext(TEXT("IMC_Active")) ? 1 : 0, HasContext(TEXT("IMC_Informant")) ? 1 : 0));
				ASSERT_THAT(IsTrue(ViewInformantFrame != 0, TEXT("the owner's C-TeamView reported Informant")));
				ASSERT_THAT(IsTrue(SwapFrame != 0 && SwapFrame <= ViewInformantFrame + 1, TEXT("IMC_Informant within one frame of the state arriving")));
				ASSERT_THAT(IsTrue(Ends.Num() == 1 && Ends[0].Value <= ViewInformantFrame + 1, TEXT("the held Interact was ended by the swap, once")));
				Key(EKeys::E, false);
				Mark = GFrameCounter;
			})
			.Until(TEXT("contexts rebuilt"), [this]() { return GFrameCounter >= Mark + SettleFrames; })
			.Then(TEXT("the applied contexts"), [this]()
			{
				ASSERT_THAT(IsTrue(HasContext(TEXT("IMC_Informant")) && !HasContext(TEXT("IMC_Active")) && HasContext(TEXT("IMC_Common")), TEXT("IMC_Common + IMC_Informant, no IMC_Active")));
				ASSERT_THAT(AreEqual(0, Count(ECommandKind::InteractEnd, Mark), TEXT("the release after the swap sent no second End")));
			});
	}
};

#endif
