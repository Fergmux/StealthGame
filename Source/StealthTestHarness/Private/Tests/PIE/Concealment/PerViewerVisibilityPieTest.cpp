// Concealment stories con-026 (AC1 actor part, AC3), con-025 (AC3, the cull through IConcealmentCullSource), con-033 (AC2
// server half), con-035 AC2 (prototype tests E and F as regression) and the batch's integration criterion: the production
// per-viewer cull and presentation (ADR-0003 section 5 with the S10 guard, ADR-0017) replace the prototype's replicated
// bConcealed. Listen server + 2 clients on L_Prototype (teams 2v1): the hider H is a remote client, its ally A the other
// member of its team, the enemy E the solo player.
//
//   HiderCulledForEnemyKeptForAllyAndOwner: H enters a bush through the one Hide(Enter) path. After the commit and the cull
//     deadline: the server holds H Concealed and Culled; E's client holds NO actor for H (the channel was closed); A's
//     machine holds H's actor with the body presentation-hidden (the latch: the ally's C-TeamView flag); H's own client
//     keeps its body. E then blind-strikes the bush's spot (no PawnRef): H is killed (the bush spot rule over the real
//     occupancy record), its death write carries bDiedConcealed, and E's client re-creates H's actor still hidden. Then E
//     hides: H, now an informant of the other team, holds no actor for E (no x-ray).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "Engine/NetConnection.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Server/ConcealmentService.h"
#include "Server/Visibility/StealthConcealmentCull.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/AssassinationPieSupport.h"
#include "View/AllyBodyHidden.h"
#include "View/BushRuntimeSubsystem.h"
#include "View/ConcealCollision.h"
#include "View/ConcealmentComponent.h"

namespace ConcealmentPerViewerPie
{
	using namespace StealthAssassinationPie;

	/** The client-side copy of the server pawn Pawn on World (matched by PlayerState id), or null when none exists there. */
	inline APawn* CopyOn(UWorld* World, const APawn* ServerPawn)
	{
		const APlayerState* ServerState = ServerPawn != nullptr ? ServerPawn->GetPlayerState() : nullptr;
		if (World == nullptr || ServerState == nullptr)
		{
			return nullptr;
		}
		for (TActorIterator<AStealthPawn> It(World); It; ++It)
		{
			const APlayerState* State = It->GetPlayerState();
			if (IsValid(*It) && State != nullptr && State->GetPlayerId() == ServerState->GetPlayerId())
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** The client actor of a server pawn by its replicated location (an unbound or body copy: no PlayerState link). */
	inline APawn* CopyNear(UWorld* World, const FVector& At, double RadiusCm)
	{
		for (TActorIterator<AStealthPawn> It(World); It; ++It)
		{
			// Never the local viewer's own pawn (an attacker warped to the planter edge stands inside the radius).
			if (IsValid(*It) && !It->IsLocallyControlled() && FVector::Dist2D(It->GetActorLocation(), At) <= RadiusCm)
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** Teleports a player's server pawn and its owner copy together (the owner owns its rotation). */
	inline void PlacePlayer(AStealthPlayerController* ServerController, const FVector& At, const FRotator& Facing)
	{
		APawn* Server = ServerController != nullptr ? ServerController->GetPawn() : nullptr;
		if (Server == nullptr)
		{
			return;
		}
		const ACharacter* Character = Cast<ACharacter>(Server);
		const double HalfHeight = Character != nullptr ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 86.0;
		const FVector Location(At.X, At.Y, At.Z + HalfHeight + 2.0);
		if (UCharacterMovementComponent* Move = Character != nullptr ? Character->GetCharacterMovement() : nullptr)
		{
			Move->StopMovementImmediately();
		}
		Server->TeleportTo(Location, Facing, false, true);
		ServerController->SetControlRotation(Facing);
		if (AStealthPlayerController* Local = OwnerMachineController(ServerController); Local != nullptr && Local != ServerController)
		{
			if (APawn* Copy = Local->GetPawn())
			{
				Copy->TeleportTo(Location, Facing, false, true);
			}
			Local->SetControlRotation(Facing);
		}
	}
}

NETWORK_TEST_CLASS(PerViewerVisibility, "StealthGame.Concealment.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousTimeout;
	inline static int32 PreviousNpcCount = -1;

	AStealthPlayerController* Hider = nullptr;
	AStealthPlayerController* Ally = nullptr;
	AStealthPlayerController* Enemy = nullptr;
	FBushRuntime Bush;
	FBushRuntime EnemyBush;
	uint32 Seq = 0;
	double Mark = 0.0;
	bool bDiedConcealed = false;
	bool bTraceSeen = false;
	int32 TraceSamples = 0;

	BEFORE_ALL()
	{
		if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(CQTestConsoleVariables::NetworkTimeoutName))
		{
			PreviousTimeout = Timeout->GetString();
			Timeout->Set(StealthAssassinationPie::NetworkTimeoutCeilingSeconds, ECVF_SetByCode);
		}
		UStealthSettings* Settings = GetMutableDefault<UStealthSettings>();
		PreviousNpcCount = Settings->NpcCount;
		Settings->NpcCount = 0; // no NPC entrant can share the hider's bush (the blind strike's one candidate)
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

	static UWorld* WorldOf(const AStealthPlayerController* ServerController)
	{
		const AStealthPlayerController* Local = StealthAssassinationPie::OwnerMachineController(ServerController);
		return Local != nullptr ? Local->GetWorld() : nullptr;
	}

	/** The two bushes farthest apart from each other's crowd (the first and the farthest from it). */
	static bool PickBushes(FBushRuntime& OutA, FBushRuntime& OutB)
	{
		const UBushRuntimeSubsystem* Bushes = UBushRuntimeSubsystem::Get(StealthAssassinationPie::ServerWorld());
		if (Bushes == nullptr || Bushes->GetBushes().Num() < 2)
		{
			return false;
		}
		OutA = Bushes->GetBushes()[0];
		double Best = -1.0;
		for (const FBushRuntime& Other : Bushes->GetBushes())
		{
			const double D = FVector::Dist2D(Other.Anchor, OutA.Anchor);
			if (D > Best)
			{
				Best = D;
				OutB = Other;
			}
		}
		return true;
	}

	TEST_METHOD(HiderCulledForEnemyKeptForAllyAndOwner)
	{
		using namespace ConcealmentPerViewerPie;
		Network
			.Until(TEXT("three players in Active, the bushes built on every machine, Concealment bound"), []()
			{
				if (!RoundActive() || ServerPlayers().Num() != 3 || UConcealmentService::Get(ServerWorld()) == nullptr || !UConcealmentService::Get(ServerWorld())->IsBound())
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
			.Then(TEXT("pick the roles (H a remote client) and place them"), [this]()
			{
				AStealthPlayerController* Pair[2] = { nullptr, nullptr };
				ASSERT_THAT(IsTrue(SplitTeams(Enemy, Pair), TEXT("a 2v1 split")));
				Hider = !Pair[0]->IsLocalController() ? Pair[0] : Pair[1];
				Ally = Hider == Pair[0] ? Pair[1] : Pair[0];
				ASSERT_THAT(IsTrue(!Hider->IsLocalController() && !Enemy->IsLocalController(), TEXT("the hider and the enemy are remote clients")));
				ASSERT_THAT(IsTrue(PickBushes(Bush, EnemyBush), TEXT("two bushes")));
				// H at the +X entry (30 cm out of the planter edge), facing the anchor; A and E 6 m away (outside the spot zone).
				PlacePlayer(Hider, Bush.Anchor + FVector(130.0, 0.0, 0.0), FRotator(0.0, 180.0, 0.0));
				PlacePlayer(Ally, Bush.Anchor + FVector(0.0, 600.0, 0.0), FRotator(0.0, -90.0, 0.0));
				PlacePlayer(Enemy, Bush.Anchor + FVector(0.0, -600.0, 0.0), FRotator(0.0, 90.0, 0.0));
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("settle 1 s"), [this]() { return ServerNow(ServerWorld()) >= Mark + 1.0; })
			.Then(TEXT("H hides (Hide(Enter) with its own origin)"), [this]()
			{
				const uint16 BushId = Bush.Id;
				AsOwner(Cast<AStealthPawn>(Hider->GetPawn()), Seq, [BushId](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Enter(BushId)); });
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("the commit, the cull deadline and the close have passed"), [this]()
			{
				const UStealthConcealmentCull* Cull = UStealthConcealmentCull::Get(ServerWorld());
				return (Cull != nullptr && Cull->GetState(Hider->GetPawn()) == EConcealCullState::Culled && ServerNow(ServerWorld()) >= Mark + 1.5) || ServerNow(ServerWorld()) >= Mark + 5.0;
			})
			.Then(TEXT("per viewer: E holds no actor, A holds it hidden, H keeps its body"), [this]()
			{
				const UConcealmentService* Service = UConcealmentService::Get(ServerWorld());
				const APawn* H = Hider->GetPawn();
				ASSERT_THAT(IsTrue(Service->GetState(H) == EConcealState::Concealed, TEXT("server: H is Concealed")));
				ASSERT_THAT(IsTrue(UStealthConcealmentCull::Get(ServerWorld())->GetState(H) == EConcealCullState::Culled, TEXT("server: H is Culled")));
				ASSERT_THAT(IsTrue(CopyOn(WorldOf(Enemy), H) == nullptr, TEXT("the enemy's client holds no actor for the concealed hider")));
				APawn* AllyCopy = Ally->IsLocalController() ? const_cast<APawn*>(H) : CopyOn(WorldOf(Ally), H);
				ASSERT_THAT(IsTrue(AllyCopy != nullptr, TEXT("the ally's machine holds the hider's actor")));
				ASSERT_THAT(IsTrue(IsAllyBodyHidden(Cast<AStealthPawn>(AllyCopy)), TEXT("... with the body presentation-hidden (the hatched marker's input)")));
				const AStealthPawn* OwnCopy = Cast<AStealthPawn>(OwnerMachineController(Hider)->GetPawn());
				ASSERT_THAT(IsTrue(OwnCopy != nullptr && !IsAllyBodyHidden(OwnCopy), TEXT("the hider's own client keeps its body")));
			})
			.Until(TEXT("10 s concealed: sample E's client and E's server connection every frame"), [this]()
			{
				const APawn* H = Hider->GetPawn();
				bTraceSeen |= CopyOn(WorldOf(Enemy), H) != nullptr;
				if (UNetConnection* Connection = Enemy->GetNetConnection())
				{
					bTraceSeen |= Connection->FindActorChannelRef(const_cast<APawn*>(H)) != nullptr;
				}
				++TraceSamples;
				return ServerNow(ServerWorld()) >= Mark + 11.5;
			})
			.Then(TEXT("no actor and no channel for the concealed hider at any sample (AC-CON-02)"), [this]()
			{
				ASSERT_THAT(IsTrue(TraceSamples > 100, TEXT("the 10 s were sampled every frame")));
				ASSERT_THAT(IsFalse(bTraceSeen, TEXT("E's client never held H's actor and E's connection never held H's channel")));
			})
			.Then(TEXT("E blind-strikes the bush's spot (no PawnRef)"), [this]()
			{
				// E within reach: its capsule 20 cm off the planter's -Y edge, facing the anchor.
				PlacePlayer(Enemy, Bush.Anchor + FVector(0.0, -155.0, 0.0), FRotator(0.0, 90.0, 0.0));
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("E settles"), [this]() { return ServerNow(ServerWorld()) >= Mark + 0.6; })
			.Then(TEXT("strike"), [this]()
			{
				FAttackPayload Payload;
				Payload.WorldPoint = Bush.Anchor;
				AsOwner(Cast<AStealthPawn>(Enemy->GetPawn()), Seq, [&Payload](UPawnCommandComponent& Command) { Command.Attack(Payload); });
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("H's death write"), [this]()
			{
				const AStealthPawn* H = Cast<AStealthPawn>(Hider->GetPawn());
				const AStealthPawn* Body = H;
				if (Body == nullptr)
				{
					for (TActorIterator<AStealthPawn> It(ServerWorld()); It; ++It)
					{
						if (It->HasDeathWrite() && FVector::Dist2D(It->GetActorLocation(), Bush.Anchor) < 150.0)
						{
							Body = *It;
						}
					}
				}
				if (Body != nullptr && Body->HasDeathWrite())
				{
					bDiedConcealed = Body->GetDeathState().bDiedConcealed;
					return true;
				}
				return ServerNow(ServerWorld()) >= Mark + 4.0;
			})
			.Then(TEXT("the blind strike killed the concealed hider; the death write carried bDiedConcealed"), [this]()
			{
				ASSERT_THAT(IsTrue(bDiedConcealed, TEXT("H was killed by the blind bush strike and its FDeathState.bDiedConcealed is set")));
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("the re-open reaches E"), [this]() { return ServerNow(ServerWorld()) >= Mark + 1.0; })
			.Then(TEXT("E's client re-created the body still hidden"), [this]()
			{
				APawn* BodyOnEnemy = CopyNear(WorldOf(Enemy), Bush.Anchor, 150.0);
				ASSERT_THAT(IsTrue(BodyOnEnemy != nullptr, TEXT("the re-opened body exists on the enemy's client")));
				ASSERT_THAT(IsTrue(IsAllyBodyHidden(Cast<AStealthPawn>(BodyOnEnemy)), TEXT("... and is never drawn (bDiedConcealed)")));
			})
			.Then(TEXT("E hides in the other bush"), [this]()
			{
				PlacePlayer(Enemy, EnemyBush.Anchor + FVector(130.0, 0.0, 0.0), FRotator(0.0, 180.0, 0.0));
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("E settles at its bush"), [this]() { return ServerNow(ServerWorld()) >= Mark + 6.5; }) // past E's re-entry cooldown? E never hid: none
			.Then(TEXT("E enters"), [this]()
			{
				const uint16 BushId = EnemyBush.Id;
				AsOwner(Cast<AStealthPawn>(Enemy->GetPawn()), Seq, [BushId](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Enter(BushId)); });
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("E is culled"), [this]()
			{
				const UStealthConcealmentCull* Cull = UStealthConcealmentCull::Get(ServerWorld());
				return (Cull != nullptr && Cull->GetState(Enemy->GetPawn()) == EConcealCullState::Culled && ServerNow(ServerWorld()) >= Mark + 1.5) || ServerNow(ServerWorld()) >= Mark + 5.0;
			})
			.Then(TEXT("H (an informant of the other team) holds no actor for E: no x-ray"), [this]()
			{
				ASSERT_THAT(IsTrue(UConcealmentService::Get(ServerWorld())->GetState(Enemy->GetPawn()) == EConcealState::Concealed, TEXT("server: E is Concealed")));
				ASSERT_THAT(IsTrue(CopyOn(WorldOf(Hider), Enemy->GetPawn()) == nullptr, TEXT("the informant's client holds no actor for the concealed rival")));
				// The informant's ally conceals too (AC-CON-04 ally half).
				PlacePlayer(Ally, Bush.Anchor + FVector(130.0, 0.0, 0.0), FRotator(0.0, 180.0, 0.0));
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("A settles at the bush"), [this]() { return ServerNow(ServerWorld()) >= Mark + 0.8; })
			.Then(TEXT("A hides"), [this]()
			{
				const uint16 BushId = Bush.Id;
				AsOwner(Cast<AStealthPawn>(Ally->GetPawn()), Seq, [BushId](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Enter(BushId)); });
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("A is concealed"), [this]()
			{
				const UConcealmentService* Service = UConcealmentService::Get(ServerWorld());
				return (Service->GetState(Ally->GetPawn()) == EConcealState::Concealed && ServerNow(ServerWorld()) >= Mark + 1.5) || ServerNow(ServerWorld()) >= Mark + 5.0;
			})
			.Then(TEXT("the informant holds its concealed ally with the body hidden (the hatched marker's input)"), [this]()
			{
				ASSERT_THAT(IsTrue(UConcealmentService::Get(ServerWorld())->GetState(Ally->GetPawn()) == EConcealState::Concealed, TEXT("server: A is Concealed")));
				APawn* AllyOnInformant = CopyOn(WorldOf(Hider), Ally->GetPawn());
				ASSERT_THAT(IsTrue(AllyOnInformant != nullptr && IsAllyBodyHidden(Cast<AStealthPawn>(AllyOnInformant)), TEXT("the informant's client holds A, body hidden")));
			});
	}
	/** The close-log entries of Pawn all landed on its cull boundary (at or after it, within one server frame's slack). */
	bool ClosesOnBoundary(const APawn* Pawn, double Deadline, int32& OutCloses) const
	{
		const UStealthConcealmentCull* Cull = UStealthConcealmentCull::Get(StealthAssassinationPie::ServerWorld());
		OutCloses = 0;
		bool bAll = true;
		for (const TPair<TWeakObjectPtr<APawn>, double>& Entry : Cull->GetCloseLog())
		{
			if (Entry.Key.Get() == Pawn && Entry.Value >= Deadline - 0.0011 && Entry.Value <= Deadline + 1.0)
			{
				++OutCloses;
				bAll &= Entry.Value <= Deadline + CloseSlackSec;
			}
		}
		return bAll;
	}

	/** One server frame of slack on the boundary (PIE runs the server well above 20 Hz). */
	static constexpr double CloseSlackSec = 0.05;
	double HiderDeadline = -1.0;
	double AllyDeadline = -1.0;
	bool bUnculledAtExit = false;
	bool bSawExiting = false;

	// con-026 AC2 (AC-CON-03) and con-025 AC1/AC3 in the per-commit gate: two allies concealed in one bush each hold the
	// other with the body hidden; the enemy's client holds neither; each close lands on the first pawn net-update boundary
	// at or after commit + ConcealCullDelayMs; the exit un-culls in the same server step; then two rivals concealed in one
	// bush hold no actor for each other.
	TEST_METHOD(CoOccupantsAndCullBoundary)
	{
		using namespace ConcealmentPerViewerPie;
		Network
			.Until(TEXT("three players in Active, the bushes built on every machine, Concealment bound"), []()
			{
				if (!RoundActive() || ServerPlayers().Num() != 3 || UConcealmentService::Get(ServerWorld()) == nullptr || !UConcealmentService::Get(ServerWorld())->IsBound())
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
			.Then(TEXT("roles; lift the re-entry cooldown; H at the +X entry, A at the -X entry, E 6 m away"), [this]()
			{
				AStealthPlayerController* Pair[2] = { nullptr, nullptr };
				ASSERT_THAT(IsTrue(SplitTeams(Enemy, Pair), TEXT("a 2v1 split")));
				Hider = !Pair[0]->IsLocalController() ? Pair[0] : Pair[1];
				Ally = Hider == Pair[0] ? Pair[1] : Pair[0];
				ASSERT_THAT(IsTrue(PickBushes(Bush, EnemyBush), TEXT("two bushes")));
				UConcealmentService* Service = UConcealmentService::Get(ServerWorld());
				FConcealmentParams Params = Service->GetCore()->GetParams();
				Params.Server.BushReenterCooldownSec = 0.0f;
				Service->GetCore()->SetParamsForTest(Params);
				PlacePlayer(Hider, Bush.Anchor + FVector(130.0, 0.0, 0.0), FRotator(0.0, 180.0, 0.0));
				PlacePlayer(Ally, Bush.Anchor + FVector(-130.0, 0.0, 0.0), FRotator(0.0, 0.0, 0.0));
				PlacePlayer(Enemy, Bush.Anchor + FVector(0.0, -600.0, 0.0), FRotator(0.0, 90.0, 0.0));
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("settle 1 s"), [this]() { return ServerNow(ServerWorld()) >= Mark + 1.0; })
			.Then(TEXT("H and A hide in the same bush"), [this]()
			{
				const uint16 BushId = Bush.Id;
				AsOwner(Cast<AStealthPawn>(Hider->GetPawn()), Seq, [BushId](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Enter(BushId)); });
				AsOwner(Cast<AStealthPawn>(Ally->GetPawn()), Seq, [BushId](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Enter(BushId)); });
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("both committed (their deadlines recorded) and both culled"), [this]()
			{
				const UStealthConcealmentCull* Cull = UStealthConcealmentCull::Get(ServerWorld());
				const APawn* H = Hider->GetPawn();
				const APawn* A = Ally->GetPawn();
				if (Cull->GetState(H) != EConcealCullState::Visible && HiderDeadline < 0.0)
				{
					HiderDeadline = Cull->GetDeadline(H);
				}
				if (Cull->GetState(A) != EConcealCullState::Visible && AllyDeadline < 0.0)
				{
					AllyDeadline = Cull->GetDeadline(A);
				}
				return (Cull->GetState(H) == EConcealCullState::Culled && Cull->GetState(A) == EConcealCullState::Culled && ServerNow(ServerWorld()) >= Mark + 1.5)
					|| ServerNow(ServerWorld()) >= Mark + 5.0;
			})
			.Then(TEXT("allies hold each other hidden; the enemy holds neither; the closes landed on the boundary"), [this]()
			{
				const UConcealmentService* Service = UConcealmentService::Get(ServerWorld());
				APawn* H = Hider->GetPawn();
				APawn* A = Ally->GetPawn();
				ASSERT_THAT(IsTrue(Service->GetState(H) == EConcealState::Concealed && Service->GetState(A) == EConcealState::Concealed, TEXT("server: both Concealed in the one bush")));
				APawn* HOnAllyMachine = Ally->IsLocalController() ? H : CopyOn(WorldOf(Ally), H);
				APawn* AOnHiderMachine = CopyOn(WorldOf(Hider), A);
				ASSERT_THAT(IsTrue(HOnAllyMachine != nullptr && IsAllyBodyHidden(Cast<AStealthPawn>(HOnAllyMachine)), TEXT("A's machine holds H with the body hidden")));
				ASSERT_THAT(IsTrue(AOnHiderMachine != nullptr && IsAllyBodyHidden(Cast<AStealthPawn>(AOnHiderMachine)), TEXT("H's machine holds A with the body hidden")));
				ASSERT_THAT(IsTrue(CopyOn(WorldOf(Enemy), H) == nullptr && CopyOn(WorldOf(Enemy), A) == nullptr, TEXT("the enemy's client holds neither")));
				const UStealthConcealmentCull* Cull = UStealthConcealmentCull::Get(ServerWorld());
				ASSERT_THAT(IsTrue(HiderDeadline > 0.0 && AllyDeadline > 0.0, TEXT("both deadlines were recorded at the commit")));
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Cull->ComputeDeadline(HiderDeadline - 0.25), HiderDeadline, 1e-6), TEXT("H's deadline is a grid boundary")));
				int32 HCloses = 0;
				int32 ACloses = 0;
				ASSERT_THAT(IsTrue(ClosesOnBoundary(H, HiderDeadline, HCloses), TEXT("H's close landed on its boundary (within one server frame)")));
				ASSERT_THAT(IsTrue(ClosesOnBoundary(A, AllyDeadline, ACloses), TEXT("A's close landed on its boundary (within one server frame)")));
				ASSERT_THAT(IsTrue(HCloses >= 1 && ACloses >= 1, TEXT("one close each (the enemy's connection)")));
			})
			.Then(TEXT("H exits (+X side)"), [this]()
			{
				AsOwner(Cast<AStealthPawn>(Hider->GetPawn()), Seq, [](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Exit(0)); });
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("the exit executes: the cull is Visible in that step"), [this]()
			{
				const UConcealmentService* Service = UConcealmentService::Get(ServerWorld());
				const APawn* H = Hider->GetPawn();
				if (!bSawExiting && Service->GetState(H) == EConcealState::Exiting)
				{
					bSawExiting = true;
					bUnculledAtExit = UStealthConcealmentCull::Get(ServerWorld())->GetState(H) == EConcealCullState::Visible;
				}
				return (bSawExiting && Service->GetState(H) == EConcealState::Outside && ServerNow(ServerWorld()) >= Mark + 1.5) || ServerNow(ServerWorld()) >= Mark + 4.0;
			})
			.Then(TEXT("the un-cull happened in the exit's step; E holds H again"), [this]()
			{
				ASSERT_THAT(IsTrue(bSawExiting && bUnculledAtExit, TEXT("the first Exiting frame already reads Visible")));
				ASSERT_THAT(IsTrue(CopyOn(WorldOf(Enemy), Hider->GetPawn()) != nullptr, TEXT("the enemy's client re-created H after the exit")));
				// Rivals: A exits too, then H and E share the bush.
				AsOwner(Cast<AStealthPawn>(Ally->GetPawn()), Seq, [](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Exit(2)); });
				PlacePlayer(Enemy, Bush.Anchor + FVector(0.0, -130.0, 0.0), FRotator(0.0, 90.0, 0.0));
				PlacePlayer(Hider, Bush.Anchor + FVector(130.0, 0.0, 0.0), FRotator(0.0, 180.0, 0.0));
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("A is out; settle"), [this]() { return ServerNow(ServerWorld()) >= Mark + 2.0; })
			.Then(TEXT("H and E hide in the same bush"), [this]()
			{
				const uint16 BushId = Bush.Id;
				AsOwner(Cast<AStealthPawn>(Hider->GetPawn()), Seq, [BushId](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Enter(BushId)); });
				AsOwner(Cast<AStealthPawn>(Enemy->GetPawn()), Seq, [BushId](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Enter(BushId)); });
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("both culled"), [this]()
			{
				const UStealthConcealmentCull* Cull = UStealthConcealmentCull::Get(ServerWorld());
				return (Cull->GetState(Hider->GetPawn()) == EConcealCullState::Culled && Cull->GetState(Enemy->GetPawn()) == EConcealCullState::Culled && ServerNow(ServerWorld()) >= Mark + 1.5)
					|| ServerNow(ServerWorld()) >= Mark + 5.0;
			})
			.Then(TEXT("co-occupant rivals are blind to each other"), [this]()
			{
				const UConcealmentService* Service = UConcealmentService::Get(ServerWorld());
				ASSERT_THAT(IsTrue(Service->GetState(Hider->GetPawn()) == EConcealState::Concealed && Service->GetState(Enemy->GetPawn()) == EConcealState::Concealed, TEXT("server: both Concealed")));
				ASSERT_THAT(IsTrue(CopyOn(WorldOf(Hider), Enemy->GetPawn()) == nullptr, TEXT("H's client holds no actor for E")));
				ASSERT_THAT(IsTrue(CopyOn(WorldOf(Enemy), Hider->GetPawn()) == nullptr, TEXT("E's client holds no actor for H")));
			});
	}

	bool bHostHiddenFlagChanged = false;

	// con-022 AC2 (AC-CON-20): the listen host is the opponent of a remote client who conceals. The host's world is the
	// server's, so the pawn is never culled there: the presentation latch hides it (set by the server queue item through the
	// replicated action state, never an anim notify), the mesh and every attachment lose visibility, bHidden never changes,
	// the host's presentation-aim trace through the pawn hits nothing of it, and a host blind strike on the bush resolves
	// through the same server path as a remote one (the hider dies with bDiedConcealed).
	TEST_METHOD(ListenHostParity)
	{
		using namespace ConcealmentPerViewerPie;
		Network
			.Until(TEXT("three players in Active, the bushes built, Concealment bound"), []()
			{
				return RoundActive() && ServerPlayers().Num() == 3 && UConcealmentService::Get(ServerWorld()) != nullptr && UConcealmentService::Get(ServerWorld())->IsBound()
					&& UBushRuntimeSubsystem::Get(ServerWorld()) != nullptr && UBushRuntimeSubsystem::Get(ServerWorld())->IsBuilt();
			})
			.Then(TEXT("roles: E the host, H a remote client of the other team"), [this]()
			{
				AStealthPlayerController* Solo = nullptr;
				AStealthPlayerController* Pair[2] = { nullptr, nullptr };
				ASSERT_THAT(IsTrue(SplitTeams(Solo, Pair), TEXT("a 2v1 split")));
				if (Solo->IsLocalController())
				{
					Enemy = Solo;
					Hider = Pair[0];
				}
				else
				{
					Enemy = Pair[0]->IsLocalController() ? Pair[0] : Pair[1];
					Hider = Solo;
				}
				ASSERT_THAT(IsTrue(Enemy->IsLocalController() && !Hider->IsLocalController(), TEXT("E is the listen host, H a remote client")));
				ASSERT_THAT(IsTrue(PickBushes(Bush, EnemyBush), TEXT("two bushes")));
				PlacePlayer(Hider, Bush.Anchor + FVector(130.0, 0.0, 0.0), FRotator(0.0, 180.0, 0.0));
				PlacePlayer(Enemy, Bush.Anchor + FVector(0.0, -600.0, 0.0), FRotator(0.0, 90.0, 0.0));
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("settle 1 s"), [this]() { return ServerNow(ServerWorld()) >= Mark + 1.0; })
			.Then(TEXT("H hides"), [this]()
			{
				const uint16 BushId = Bush.Id;
				AsOwner(Cast<AStealthPawn>(Hider->GetPawn()), Seq, [BushId](UPawnCommandComponent& Command) { Command.Hide(FHideCommand::Enter(BushId)); });
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("H concealed and past the enter (bHidden watched every frame)"), [this]()
			{
				const APawn* H = Hider->GetPawn();
				bHostHiddenFlagChanged |= H->IsHidden();
				const UConcealmentService* Service = UConcealmentService::Get(ServerWorld());
				return (Service->GetState(H) == EConcealState::Concealed && ServerNow(ServerWorld()) >= Mark + 1.5) || ServerNow(ServerWorld()) >= Mark + 5.0;
			})
			.Then(TEXT("on the host: the latch hides the pawn, bHidden untouched, the aim trace misses it"), [this]()
			{
				AStealthPawn* H = Cast<AStealthPawn>(Hider->GetPawn());
				ASSERT_THAT(IsTrue(UConcealmentService::Get(ServerWorld())->GetState(H) == EConcealState::Concealed, TEXT("server: H is Concealed")));
				const UConcealmentComponent* Concealment = UConcealmentComponent::Find(H);
				ASSERT_THAT(IsTrue(Concealment != nullptr && Concealment->IsPresentationHidden() && IsAllyBodyHidden(H), TEXT("the host's presentation latch is set")));
				ASSERT_THAT(IsFalse(H->GetMesh()->IsVisible(), TEXT("the mesh is not visible on the host")));
				ASSERT_THAT(IsTrue(H->GetMesh()->bSuppressNotifyEventDispatch, TEXT("no anim notify (footstep, foley, notify Niagara or sound) dispatches on the host")));
				TArray<USceneComponent*> Children;
				H->GetMesh()->GetChildrenComponents(true, Children);
				for (const USceneComponent* Child : Children)
				{
					ASSERT_THAT(IsFalse(Child->IsVisible(), *FString::Printf(TEXT("attachment %s is not visible"), *Child->GetName())));
				}
				ASSERT_THAT(IsFalse(bHostHiddenFlagChanged || H->IsHidden(), TEXT("bHidden never changed")));
				FHitResult Hit;
				const FVector From = Enemy->GetPawn()->GetActorLocation();
				const FVector To = H->GetActorLocation() + (H->GetActorLocation() - From).GetSafeNormal() * 200.0;
				FCollisionQueryParams Params(TEXT("HostAim"), false, Enemy->GetPawn());
				const bool bHit = ServerWorld()->LineTraceSingleByChannel(Hit, From, To, ConcealmentCollision::PresentationAimChannel, Params);
				ASSERT_THAT(IsFalse(bHit && Hit.GetActor() == H, TEXT("the host's presentation-aim trace passes through the hidden pawn")));
				// E within reach of the bush for the blind strike.
				PlacePlayer(Enemy, Bush.Anchor + FVector(0.0, -155.0, 0.0), FRotator(0.0, 90.0, 0.0));
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("E settles"), [this]() { return ServerNow(ServerWorld()) >= Mark + 0.6; })
			.Then(TEXT("the host strikes the spot"), [this]()
			{
				FAttackPayload Payload;
				Payload.WorldPoint = Bush.Anchor;
				AsOwner(Cast<AStealthPawn>(Enemy->GetPawn()), Seq, [&Payload](UPawnCommandComponent& Command) { Command.Attack(Payload); });
				Mark = ServerNow(ServerWorld());
				bDiedConcealed = false;
			})
			.Until(TEXT("H's death write"), [this]()
			{
				for (TActorIterator<AStealthPawn> It(ServerWorld()); It; ++It)
				{
					if (It->HasDeathWrite() && FVector::Dist2D(It->GetActorLocation(), Bush.Anchor) < 150.0)
					{
						bDiedConcealed = It->GetDeathState().bDiedConcealed;
						return true;
					}
				}
				return ServerNow(ServerWorld()) >= Mark + 4.0;
			})
			.Then(TEXT("the host's blind strike killed the concealed hider through the server path"), [this]()
			{
				ASSERT_THAT(IsTrue(bDiedConcealed, TEXT("H died with bDiedConcealed")));
			});
	}
};

#endif
