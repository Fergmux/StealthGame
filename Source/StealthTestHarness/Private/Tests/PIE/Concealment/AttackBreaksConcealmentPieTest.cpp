// Concealment story con-027 AC2 (production/epics/concealment-bushes/story-027-uncull-before-leaf-plane.md; GDD CON-R9,
// F3(c), AC-CON-13 observed; ADR-0017 sections 3 and 5): StealthGame.Concealment.PIE.AttackBreaksConcealment.<method>.
// Listen host + 2 clients on L_Prototype, NpcCount 0 (no net emulation: AC-CON-13 carries no [Net] tag). The hider H (a
// remote client) conceals in a bush through the production Hide(Enter); a passer-by V (an unbound Shared Pawn, an NPC)
// stands 30 cm off the planter's -X edge, in front of H; the opponent E (the other team; observed on its own machine) watches.
//   StrikeFromBushShowsAttackerAtEmergeThenVictimFallsAtContact: H, culled for E, strikes V at t0 (its Strike pose with the
//     bush preface). On E's machine H's body is first drawn at t0 + BushStrikeEmergeSec (0.35 s, +- one server tick and
//     one frame of E), V's Death_Fall starts at ContactServerTime = t0 + 0.75 s (+- one frame), and H ends Outside the bush.
//   BracedVictimTakesTheReversalPath (control): V braces before t0; the same strike is reversed (H falls, V counters) and
//     H still breaks concealment (Emerging in the Attack's slot, un-culled).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "Server/Visibility/StealthConcealmentCull.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/OwnPresentationPieSupport.h"
#include "Tests/PIE/Assassination/PresentationPieSupport.h"
#include "Tests/PIE/Concealment/ConcealmentPieSupport.h"

NETWORK_TEST_CLASS(AttackBreaksConcealment, "StealthGame.Concealment.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousTimeout;
	inline static int32 PreviousNpcCount = -1;

	AStealthPlayerController* Hider = nullptr;
	AStealthPlayerController* Enemy = nullptr;
	TWeakObjectPtr<AStealthPawn> Victim;
	FBushRuntime Bush;
	int32 MachineE = INDEX_NONE;
	StealthOwnPresentationPie::FFrameTracker Frames;
	uint32 Seq = 0x27200000u;
	double Mark = 0.0;
	double T0 = -1.0;
	double Contact = -1.0;
	double AppearAt = -1.0;
	bool bCulledForEnemyBeforeStrike = false;
	bool bHiderDied = false;
	bool bVictimDied = false;
	bool bVictimCountered = false;
	EConcealState StateAfterStrike = EConcealState::Outside;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousTimeout = Timeout->GetString();
			Timeout->Set(StealthAssassinationPie::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
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

	/** Round up, roles (H a remote client; E its opponent), H concealed and culled for E, V outside the +X face. */
	void Setup(bool bVictimBraces)
	{
		using namespace StealthAssassinationPie;
		using namespace StealthConcealmentPie;
		Network
			.Until(TEXT("three players in Active, bushes built, Concealment bound"), []()
			{
				const UConcealmentService* Service = UConcealmentService::Get(ServerWorld());
				const UBushRuntimeSubsystem* Bushes = UBushRuntimeSubsystem::Get(ServerWorld());
				return RoundActive() && ServerPlayers().Num() == 3 && Service != nullptr && Service->IsBound() && Bushes != nullptr && Bushes->IsBuilt();
			})
			.Then(TEXT("roles and placement"), [this]()
			{
				AStealthPlayerController* Solo = nullptr;
				AStealthPlayerController* Pair[2] = { nullptr, nullptr };
				ASSERT_THAT(IsTrue(SplitTeams(Solo, Pair), TEXT("a 2v1 split")));
				if (!Solo->IsLocalController())
				{
					Hider = Solo;
					Enemy = !Pair[0]->IsLocalController() ? Pair[0] : Pair[1];
				}
				else
				{
					Hider = !Pair[0]->IsLocalController() ? Pair[0] : Pair[1];
					Enemy = Solo;
				}
				MachineE = StealthPresentationPie::MachineOf(Enemy);
				ASSERT_THAT(IsTrue(!Hider->IsLocalController() && MachineE != INDEX_NONE, TEXT("H is a remote client; E's machine is known")));
				Bush = UBushRuntimeSubsystem::Get(ServerWorld())->GetBushes()[0];
				PlacePlayerAt(Hider, Bush.Anchor + FVector(130.0, 0.0, 0.0), FRotator(0.0, 180.0, 0.0));
				PlacePlayerAt(Enemy, Bush.Anchor + FVector(600.0, 300.0, 0.0), FRotator(0.0, -150.0, 0.0));
				for (AStealthPlayerController* Player : ServerPlayers())
				{
					if (Player != Hider && Player != Enemy)
					{
						PlacePlayerAt(Player, Bush.Anchor + FVector(-900.0, 0.0, 0.0), FRotator::ZeroRotator);
					}
				}
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("settle"), [this]() { Frames.Sample(); return ServerNow(ServerWorld()) >= Mark + 1.0; })
			.Then(TEXT("H hides"), [this]()
			{
				const uint16 BushId = Bush.Id;
				AsOwner(Cast<AStealthPawn>(Hider->GetPawn()), Seq, [BushId](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Enter(BushId)); });
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("H Concealed, culled, and E's client holds no actor for it"), [this]()
			{
				Frames.Sample();
				const UStealthConcealmentCull* Cull = UStealthConcealmentCull::Get(ServerWorld());
				const APawn* H = Hider->GetPawn();
				const bool bClosed = Cull != nullptr && Cull->GetState(H) == EConcealCullState::Culled && CopyByGuid(Worlds()[MachineE], H) == nullptr;
				return (StateOf(H) == EConcealState::Concealed && bClosed && ServerNow(ServerWorld()) >= Mark + 1.2) || ServerNow(ServerWorld()) >= Mark + 6.0;
			})
			.Then(TEXT("V outside the -X face; H aims at it"), [this, bVictimBraces]()
			{
				const APawn* H = Hider->GetPawn();
				ASSERT_THAT(IsTrue(StateOf(H) == EConcealState::Concealed, TEXT("server: H is Concealed")));
				bCulledForEnemyBeforeStrike = MachineE == 0 ? false : CopyByGuid(Worlds()[MachineE], H) == nullptr;
				ASSERT_THAT(IsTrue(MachineE == 0 || bCulledForEnemyBeforeStrike, TEXT("E's client holds no actor for the concealed hider")));
								// V in front of H's body (H entered from +X facing the anchor: its yaw, 180, is kept inside): off the -X face.
				AStealthPawn* V = StealthConcealmentPie::SpawnHider(Bush.Anchor + FVector(-165.0, 0.0, 120.0), FRotator::ZeroRotator);
				ASSERT_THAT(IsTrue(V != nullptr, TEXT("the passer-by spawned")));
				Victim = V;
				V->TeleportTo(Bush.Anchor + FVector(-165.0, 0.0, V->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0), FRotator::ZeroRotator, false, true);
				const FRotator Aim(0.0, 180.0, 0.0);
				Hider->SetControlRotation(Aim);
				if (AStealthPlayerController* Local = OwnerMachineController(Hider))
				{
					Local->SetControlRotation(Aim);
				}
				if (bVictimBraces)
				{
					AsOwner(V, Seq, [](UPawnCommandComponent& Command) { Command.PrepareBegin(); });
				}
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("V settles (and its brace is up)"), [this]() { Frames.Sample(); return ServerNow(ServerWorld()) >= Mark + 0.8; })
			.Then(TEXT("H strikes V from inside"), [this]()
			{
				StrikeAt(Cast<AStealthPawn>(Hider->GetPawn()), Victim.Get(), Seq);
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("sample through the strike's recovery"), [this]()
			{
				Frames.Sample();
				const AStealthPawn* H = Cast<AStealthPawn>(Hider->GetPawn());
				if (H != nullptr && T0 < 0.0 && H->GetActionState().Pose == EActionPose::Strike)
				{
					T0 = H->GetActionState().StartServerTime;
					Contact = H->GetActionState().ContactServerTime;
					StateAfterStrike = StateOf(H);
				}
				if (H != nullptr && AppearAt < 0.0 && MachineE >= 0)
				{
					const AStealthPawn* Copy = Cast<AStealthPawn>(CopyByGuid(Worlds()[MachineE], H));
					if (Copy != nullptr && IsBodyDrawn(Copy))
					{
						AppearAt = ServerNow(Worlds()[MachineE]);
					}
				}
				const AStealthPawn* V = Victim.Get();
				bVictimDied |= V != nullptr && V->HasDeathWrite();
				bVictimCountered |= V != nullptr && V->GetActionState().Pose == EActionPose::Counter;
				bHiderDied |= H != nullptr && H->HasDeathWrite();
				return T0 > 0.0 && ServerNow(ServerWorld()) >= T0 + 2.4;
			});
	}

	TEST_METHOD(StrikeFromBushShowsAttackerAtEmergeThenVictimFallsAtContact)
	{
		Setup(/*bVictimBraces*/ false);
		Network.Then(TEXT("assert"), [this]()
		{
			using namespace StealthAssassinationPie;
			using namespace StealthConcealmentPie;
			const FConcealmentTuning_Public Tuning;
			const double FrameE = Frames.FrameOf(MachineE);
			const double Tick = Frames.FrameOf(0);
			const AStealthPawn* V = Victim.Get();
			const AStealthPawn* VCopy = Cast<AStealthPawn>(CopyByGuid(Worlds()[MachineE], V));
			const FEncounterPresentationLogRow* Fall = VCopy != nullptr ? StealthPresentationPie::FirstRow(Worlds()[MachineE], VCopy, EEncounterPresentationEvent::ClipStart, EEncounterClip::DeathFall) : nullptr;
			TestRunner->AddInfo(FString::Printf(TEXT("CON13 t0 %.3f contact %.3f (t0 + %.3f); E (machine %d) first draws H at %.3f (t0 + %.3f, due + %.3f); V's Death_Fall on E at %.3f; frame E %.3f tick %.3f"),
				T0, Contact, Contact - T0, MachineE, AppearAt, AppearAt - T0, Tuning.BushStrikeEmergeSec, Fall != nullptr ? Fall->ServerNow : -1.0, FrameE, Tick));
			ASSERT_THAT(IsTrue(T0 > 0.0 && StateAfterStrike == EConcealState::Emerging, TEXT("H's strike executed and H was Emerging in its slot (the break)")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Contact - T0, static_cast<double>(Tuning.BushStrikeEmergeSec) + 0.4, 1e-4), TEXT("ContactServerTime = t0 + BushStrikeEmergeSec + StrikeContactSec (0.75 s)")));
			ASSERT_THAT(IsTrue(AppearAt > 0.0, TEXT("E's machine drew H's body")));
			ASSERT_THAT(IsTrue(AppearAt >= T0 + Tuning.BushStrikeEmergeSec - 1e-3, TEXT("never before t0 + 0.35 s")));
			ASSERT_THAT(IsTrue(AppearAt <= T0 + Tuning.BushStrikeEmergeSec + Tick + FrameE, TEXT("at t0 + 0.35 s (+- one server tick and one frame of E)")));
			ASSERT_THAT(IsTrue(bVictimDied && !bHiderDied, TEXT("V was killed, H was not")));
			ASSERT_THAT(IsTrue(Fall != nullptr && StealthOwnPresentationPie::OnFrameOf(Fall->ServerNow, Contact, FrameE), TEXT("V's contact and fall start at t0 + 0.75 s on E's machine (+- one frame)")));
			const APawn* H = Hider->GetPawn();
			ASSERT_THAT(IsTrue(StateOf(H) == EConcealState::Outside, TEXT("H ends Outside")));
			ASSERT_THAT(IsTrue(ConcealmentGeometry::DistanceToFootprintCm(Bush.Footprint, H->GetActorLocation()) > 0.0, TEXT("H ends outside the bush's footprint")));
		});
	}

	TEST_METHOD(BracedVictimTakesTheReversalPath)
	{
		Setup(/*bVictimBraces*/ true);
		Network.Then(TEXT("assert the reversal"), [this]()
		{
			TestRunner->AddInfo(FString::Printf(TEXT("CON13 control: state at the strike %d, H died %d, V died %d, V countered %d"),
				static_cast<int32>(StateAfterStrike), bHiderDied ? 1 : 0, bVictimDied ? 1 : 0, bVictimCountered ? 1 : 0));
			ASSERT_THAT(IsTrue(StateAfterStrike == EConcealState::Emerging, TEXT("the strike still broke concealment (Emerging in its slot)")));
			ASSERT_THAT(IsTrue(bHiderDied && !bVictimDied && bVictimCountered, TEXT("the braced victim reversed: H falls, V counters and lives")));
		});
	}
};

#endif
