// Assassination story asn-027 AC3 (ADR-0008 T2; listen server + 2 clients on L_Prototype, NpcCount 0):
// StealthGame.Assassination.PIE.PrototypeScenariosACF.<method>. The first-playable networked play tests A, C and F
// (production/first-playable-report.md, before-state captures production/qa/evidence/2026-10-24_*, kept) replayed on the
// production path: every strike and brace is a command through Round's queue (the player's own origin), resolved in
// Assassination's slot, never the retired prototype resolver. Scenarios as the story defines them:
//   A  an unprepared strike on an Active rival kills it: OnPawnKilled(Rival, Attacker, Player, Strike) once, the rival
//      becomes an informant and its FDeathState.DeathServerTime is the strike's ContactServerTime;
//   C  a braced defender's reversal eliminates the attacker: OnPawnKilled(Attacker, Defender, Player, Reversal); when the
//      attacker was its team's last Active player the round becomes Ended with reason Elimination and the defender's
//      team as the winner; with an Active teammate left the round stays Active (edge case);
//   F  a blind strike on a bush (a TargetSpot strike at the bush, no PawnRef, from outside it) kills the enemy concealed
//      in it, FDeathState.bDiedConcealed set; the same strike on the empty bush first kills nobody (edge case).
// Players split 2v1: Solo is alone on its team, Pair[0] / Pair[1] share the other. A (prototype report test "A" was the
// third-civilian exile, a Penalty test; the story defines A as the rival kill, decision logged 2026-10-27).

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "CQTest.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Server/EncounterService.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"
#include "Tests/PIE/Assassination/AssassinationPieSupport.h"
#include "Tests/PIE/StealthPIEOpenCircle.h"

namespace PrototypeScenariosACFData
{
	/** One OnPawnKilled call (server). */
	struct FKill
	{
		TWeakObjectPtr<APawn> Victim;
		TWeakObjectPtr<APawn> Killer;
		EVictimKind Kind = EVictimKind::Npc;
		EKillCause Cause = EKillCause::Strike;
	};

	/** Strike distance between the capsule axes of an aimed strike (inside AssassinationRangeCm 200). */
	constexpr double StrikeDistanceCm = 150.0;
	/** A blind striker stands this far outside the bush footprint (its capsule radius plus a margin). */
	constexpr double OutsideBushCm = 70.0;
	/** Seconds to watch after a contact (the death write, the Roster transition and Round's latch have landed). */
	constexpr double AfterContactSec = 3.0;
	/** The striker's lockout (AttackAttemptLockoutSec 1.75) plus a margin before its second strike. */
	constexpr double SecondStrikeWaitSec = 2.5;

	inline bool FarFromBushes(UWorld* Server, const FVector& Point)
	{
		for (TActorIterator<AStealthBush> It(Server); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), Point) <= 900.0)
			{
				return false;
			}
		}
		return true;
	}

	/** The floor point under Point on the server, ignoring Ignored (the bush's own foliage); unset when nothing is below. */
	inline TOptional<FVector> FloorUnder(UWorld* Server, const FVector& Point, const AActor* Ignored)
	{
		FHitResult Floor;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(ACFBushFloor), false, Ignored);
		if (Server->LineTraceSingleByChannel(Floor, Point + FVector(0.0, 0.0, 300.0), Point - FVector(0.0, 0.0, 1000.0), ECC_Visibility, Query))
		{
			return Floor.ImpactPoint;
		}
		return {};
	}
}

NETWORK_TEST_CLASS(PrototypeScenariosACF, "StealthGame.Assassination.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };
	inline static FString PreviousNetworkTimeout;
	inline static int32 PreviousNpcCount = -1;

	TWeakObjectPtr<AStealthPlayerController> Solo;
	TWeakObjectPtr<AStealthPlayerController> Pair[2];
	TWeakObjectPtr<AStealthPawn> Striker;
	TWeakObjectPtr<AStealthPawn> Target;
	TWeakObjectPtr<AStealthBush> Bush;
	FVector BushCentre = FVector::ZeroVector;
	/** F: the striker's distance from the bush centre (footprint half size + OutsideBushCm). */
	double BushStrikeDistanceCm = 0.0;
	TArray<PrototypeScenariosACFData::FKill> Kills;
	FDelegateHandle KilledHandle;
	double ContactServerTime = -1.0;
	double Mark = 0.0;
	uint32 Seq = 0x32700000u;
	int32 KillsAfterEmptyBush = -1;

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

	/** Waits for an Active round with a 2v1 split, then records the players and subscribes to OnPawnKilled. */
	bool SetUpPlayers()
	{
		using namespace StealthAssassinationPie;
		AStealthPlayerController* S = nullptr;
		AStealthPlayerController* P[2] = { nullptr, nullptr };
		if (!RoundActive() || !SplitTeams(S, P))
		{
			return false;
		}
		Solo = S;
		Pair[0] = P[0];
		Pair[1] = P[1];
		const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(ServerWorld());
		UEncounterService* Service = Root != nullptr ? Root->GetEncounterService() : nullptr;
		FOnPawnKilled* Killed = Service != nullptr ? Service->GetOnPawnKilled() : nullptr;
		if (Killed == nullptr)
		{
			return false;
		}
		KilledHandle = Killed->AddLambda([this](APawn* Dead, APawn* Killer, EVictimKind Kind, EKillCause Cause)
		{
			Kills.Add({ Dead, Killer, Kind, Cause });
		});
		return true;
	}

	/**
	 * Puts Target on a clear, flat disc far from every bush and the Striker StrikeDistanceCm behind it along the striker's
	 * own facing (PlaceStrikerFacing: a remote owner's rotation is its client's, so the striker is never turned).
	 */
	bool PlacePair(AStealthPawn* InStriker, AStealthPawn* InTarget)
	{
		using namespace StealthAssassinationPie;
		Striker = InStriker;
		Target = InTarget;
		MoveClearOfBushes(InTarget);
		UWorld* Server = ServerWorld();
		const float HalfHeight = InTarget->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		StealthPIEOpenCircle::FParams Params;
		Params.RadiusCm = PrototypeScenariosACFData::StrikeDistanceCm * 0.5;
		Params.MarginCm = 60.0;
		Params.AlsoIgnored.Add(InStriker);
		Params.Accept = [Server](const FVector& Centre) { return PrototypeScenariosACFData::FarFromBushes(Server, Centre); };
		const TOptional<FVector> Centre = StealthPIEOpenCircle::FindOpenCircle(Server, InTarget->GetActorLocation() - FVector(0.0, 0.0, HalfHeight), InTarget, Params);
		if (!Centre.IsSet())
		{
			return false;
		}
		const FVector Forward = InStriker->GetActorForwardVector().GetSafeNormal2D();
		InTarget->TeleportTo(Centre.GetValue() + Forward * Params.RadiusCm + FVector(0.0, 0.0, HalfHeight + 2.0), InTarget->GetActorRotation(), false, true);
		PlaceStrikerFacing(InStriker, InTarget, PrototypeScenariosACFData::StrikeDistanceCm);
		return true;
	}

	/** The latest Strike's ContactServerTime of the striker on the server (-1 before its slot). */
	void SampleContact()
	{
		if (Striker.IsValid() && Striker->GetActionState().Pose == EActionPose::Strike)
		{
			ContactServerTime = Striker->GetActionState().ContactServerTime;
		}
	}

	const AStealthGameState* ServerState() const
	{
		UWorld* Server = StealthAssassinationPie::ServerWorld();
		return Server != nullptr ? Server->GetGameState<AStealthGameState>() : nullptr;
	}

	static bool IsInformant(const AStealthPlayerController* Controller)
	{
		const AStealthPlayerState* State = Controller != nullptr ? Controller->GetPlayerState<AStealthPlayerState>() : nullptr;
		return State != nullptr && State->IsInformant();
	}

	/** The C methods' reversal: bSoloStrikes picks Solo -> Pair[0] (round end) or Pair[0] -> Solo (no end). */
	void BraceThenStrikeDeferred(bool bSoloStrikes)
	{
		// The players are known only once SetUpPlayers ran, so the steps resolve them when they execute.
		using namespace StealthAssassinationPie;
		Network
			.Then(TEXT("place the defender on a clear disc and the striker 150 cm behind it, facing it"), [this, bSoloStrikes]()
			{
				AStealthPlayerController* StrikerController = bSoloStrikes ? Solo.Get() : Pair[0].Get();
				AStealthPlayerController* DefenderController = bSoloStrikes ? Pair[0].Get() : Solo.Get();
				ASSERT_THAT(IsTrue(StrikerController != nullptr && DefenderController != nullptr, TEXT("the players are up")));
				ASSERT_THAT(IsTrue(PlacePair(Cast<AStealthPawn>(StrikerController->GetPawn()), Cast<AStealthPawn>(DefenderController->GetPawn())),
					TEXT("a clear, flat disc away from the bushes")));
			})
			.Then(TEXT("settle: mark"), [this]() { Mark = ServerNow(ServerWorld()); })
			.Until(TEXT("settle"), [this]() { return ServerNow(ServerWorld()) >= Mark + 0.8; })
			.Then(TEXT("the defender braces"), [this]()
			{
				AsOwner(Target.Get(), Seq, [](UPawnCommandComponent& Command) { Command.PrepareBegin(); });
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("the Brace pose is on the defender"), [this]()
			{
				return Target.IsValid() && Target->GetActionState().Pose == EActionPose::Brace && ServerNow(ServerWorld()) >= Mark + 0.4;
			})
			.Then(TEXT("the striker strikes the braced defender"), [this]() { StrikeAt(Striker.Get(), Target.Get(), Seq); })
			.Until(TEXT("contact + 3 s"), [this]()
			{
				SampleContact();
				return ContactServerTime > 0.0 && ServerNow(ServerWorld()) >= ContactServerTime + PrototypeScenariosACFData::AfterContactSec;
			});
	}

	TEST_METHOD(A_UnpreparedStrikeOnAnActiveRivalKillsIt)
	{
		using namespace StealthAssassinationPie;
		Network
			.Until(TEXT("the round is Active and the players split 2v1"), [this]() { return SetUpPlayers(); })
			.Then(TEXT("place the rival on a clear disc and the attacker 150 cm behind it, facing it"), [this]()
			{
				ASSERT_THAT(IsTrue(PlacePair(Cast<AStealthPawn>(Pair[0]->GetPawn()), Cast<AStealthPawn>(Solo->GetPawn())), TEXT("a clear, flat disc away from the bushes")));
			})
			.Then(TEXT("settle: mark"), [this]() { Mark = ServerNow(ServerWorld()); })
			.Until(TEXT("settle"), [this]() { return ServerNow(ServerWorld()) >= Mark + 0.8; })
			.Then(TEXT("the attacker strikes the unbraced rival"), [this]()
			{
				ASSERT_THAT(IsTrue(Target->GetActionState().Pose != EActionPose::Brace, TEXT("the rival is not braced")));
				StrikeAt(Striker.Get(), Target.Get(), Seq);
			})
			.Until(TEXT("contact + 3 s"), [this]()
			{
				SampleContact();
				return ContactServerTime > 0.0 && ServerNow(ServerWorld()) >= ContactServerTime + PrototypeScenariosACFData::AfterContactSec;
			})
			.Then(TEXT("assert"), [this]()
			{
				TestRunner->AddInfo(FString::Printf(TEXT("ACF-A contact %.3f, kills %d, death %.3f"), ContactServerTime, Kills.Num(),
					Target.IsValid() ? Target->GetDeathState().DeathServerTime : -1.0));
				ASSERT_THAT(IsTrue(Kills.Num() == 1 && Kills[0].Victim.Get() == Target.Get() && Kills[0].Killer.Get() == Striker.Get() && Kills[0].Kind == EVictimKind::Player
					&& Kills[0].Cause == EKillCause::Strike, TEXT("exactly one OnPawnKilled(Rival, Attacker, Player, Strike)")));
				ASSERT_THAT(IsTrue(IsInformant(Solo.Get()), TEXT("the rival is an informant")));
				ASSERT_THAT(IsTrue(!IsInformant(Pair[0].Get()), TEXT("the attacker is still Active")));
				ASSERT_THAT(IsTrue(Target.IsValid() && Target->HasDeathWrite() && FMath::IsNearlyEqual(Target->GetDeathState().DeathServerTime, ContactServerTime, 1e-6),
					TEXT("the rival's FDeathState.DeathServerTime is the strike's ContactServerTime")));
			});
	}

	TEST_METHOD(C_ReversalOfTheLastActiveAttackerEndsTheRoundByElimination)
	{
		using namespace StealthAssassinationPie;
		Network.Until(TEXT("the round is Active and the players split 2v1"), [this]() { return SetUpPlayers(); });
		// Solo (its team's only Active player) strikes the braced Pair[0].
		BraceThenStrikeDeferred(true);
		Network
			.Until(TEXT("the round ends (bounded by the contact + 3 s already waited, plus 10 s)"), [this]()
			{
				const AStealthGameState* State = ServerState();
				return (State != nullptr && State->GetRoundPublicState().Phase == ERoundPhase::Ended)
					|| ServerNow(ServerWorld()) >= ContactServerTime + PrototypeScenariosACFData::AfterContactSec + 10.0;
			})
			.Then(TEXT("assert"), [this]()
			{
				const AStealthGameState* State = ServerState();
				const FRoundPublicState Round = State != nullptr ? State->GetRoundPublicState() : FRoundPublicState();
				const uint8 DefenderTeam = TeamOf(Pair[0].Get());
				TestRunner->AddInfo(FString::Printf(TEXT("ACF-C contact %.3f, kills %d, phase %s, winner %d, reason %d (defender team %d)"), ContactServerTime, Kills.Num(),
					LexRoundPhase(Round.Phase), Round.LastResult.WinningTeam, Round.LastResult.PublicReason, DefenderTeam));
				ASSERT_THAT(IsTrue(Kills.Num() == 1 && Kills[0].Victim.Get() == Striker.Get() && Kills[0].Killer.Get() == Target.Get() && Kills[0].Kind == EVictimKind::Player
					&& Kills[0].Cause == EKillCause::Reversal, TEXT("exactly one OnPawnKilled(Attacker, Defender, Player, Reversal)")));
				ASSERT_THAT(IsTrue(IsInformant(Solo.Get()) && !IsInformant(Pair[0].Get()), TEXT("the attacker is eliminated, the defender is Active")));
				ASSERT_THAT(IsTrue(Round.Phase == ERoundPhase::Ended, TEXT("the round is Ended")));
				ASSERT_THAT(IsTrue(Round.LastResult.PublicReason == RoundPublicReason::Elimination, TEXT("the round ended by elimination")));
				ASSERT_THAT(IsTrue(Round.LastResult.WinningTeam == DefenderTeam, TEXT("the defender's team won")));
			});
	}

	TEST_METHOD(C_ReversalWithAnActiveTeammateLeftKeepsTheRoundActive)
	{
		using namespace StealthAssassinationPie;
		Network.Until(TEXT("the round is Active and the players split 2v1"), [this]() { return SetUpPlayers(); });
		// Pair[0] (Pair[1] stays Active) strikes the braced Solo.
		BraceThenStrikeDeferred(false);
		Network.Then(TEXT("assert"), [this]()
		{
			const AStealthGameState* State = ServerState();
			const ERoundPhase Phase = State != nullptr ? State->GetRoundPublicState().Phase : ERoundPhase::Unknown;
			TestRunner->AddInfo(FString::Printf(TEXT("ACF-C2 contact %.3f, kills %d, phase %s"), ContactServerTime, Kills.Num(), LexRoundPhase(Phase)));
			ASSERT_THAT(IsTrue(Kills.Num() == 1 && Kills[0].Victim.Get() == Striker.Get() && Kills[0].Killer.Get() == Target.Get() && Kills[0].Cause == EKillCause::Reversal,
				TEXT("exactly one OnPawnKilled(Attacker, Defender, Player, Reversal)")));
			ASSERT_THAT(IsTrue(IsInformant(Pair[0].Get()) && !IsInformant(Pair[1].Get()) && !IsInformant(Solo.Get()),
				TEXT("the attacker is eliminated; its teammate and the defender stay Active")));
			ASSERT_THAT(IsTrue(Phase == ERoundPhase::Active, TEXT("the round stays Active (the attacker's team still has an Active player)")));
		});
	}

	/** F: places the striker OutsideBushCm outside the nearest bush's footprint, facing its centre along its own facing. */
	bool PlaceStrikerOutsideBush(AStealthPawn* InStriker)
	{
		using namespace StealthAssassinationPie;
		UWorld* Server = ServerWorld();
		AStealthBush* Nearest = nullptr;
		for (TActorIterator<AStealthBush> It(Server); It; ++It)
		{
			if (Nearest == nullptr || FVector::Dist2D(It->GetActorLocation(), InStriker->GetActorLocation()) < FVector::Dist2D(Nearest->GetActorLocation(), InStriker->GetActorLocation()))
			{
				Nearest = *It;
			}
		}
		if (Nearest == nullptr)
		{
			return false;
		}
		const FBox2D Footprint = Nearest->GetFootprint2D();
		const TOptional<FVector> Floor = PrototypeScenariosACFData::FloorUnder(Server, FVector(Footprint.GetCenter(), Nearest->GetActorLocation().Z), Nearest);
		if (!Footprint.bIsValid || !Floor.IsSet())
		{
			return false;
		}
		Bush = Nearest;
		BushCentre = Floor.GetValue();
		const FVector2D HalfSize = Footprint.GetExtent();
		BushStrikeDistanceCm = FMath::Max(HalfSize.X, HalfSize.Y) + PrototypeScenariosACFData::OutsideBushCm;
		Striker = InStriker;
		ReplaceStrikerAtBush();
		return true;
	}

	/**
	 * (Re)places the striker BushStrikeDistanceCm from the bush centre along its own current facing, keeping its rotation.
	 * Before the second strike too: the first strike's root-motion lunge carries the striker toward the bush, and a striker
	 * standing at the centre has the occupant behind it (out of the bearing limit: a whiff).
	 */
	void ReplaceStrikerAtBush()
	{
		const float HalfHeight = Striker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector Forward = Striker->GetActorForwardVector().GetSafeNormal2D();
		Striker->TeleportTo(BushCentre - Forward * BushStrikeDistanceCm + FVector(0.0, 0.0, HalfHeight + 2.0), Striker->GetActorRotation());
	}

	/** F: logs and checks the striker stands outside the bush with its centre in reach. */
	void CheckStrikerOutsideBush(const TCHAR* Label)
	{
		const AStealthCharacter* StrikerCharacter = Cast<AStealthCharacter>(Striker.Get());
		const double Reach = FVector::Dist2D(Striker->GetActorLocation(), BushCentre);
		const FVector ToCentre = (BushCentre - Striker->GetActorLocation()).GetSafeNormal2D();
		const double BearingDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Striker->GetActorForwardVector().GetSafeNormal2D(), ToCentre), -1.0, 1.0)));
		TestRunner->AddInfo(FString::Printf(TEXT("ACF-F %s: striker %.1f cm from the bush centre, bearing %.1f deg, dZ %.1f cm, concealed %d"), Label, Reach, BearingDeg,
			Striker->GetActorLocation().Z - BushCentre.Z, StrikerCharacter != nullptr && StrikerCharacter->IsConcealed() ? 1 : 0));
		ASSERT_THAT(IsTrue(StrikerCharacter != nullptr && !StrikerCharacter->IsConcealed(), TEXT("the striker stands outside the bush")));
		ASSERT_THAT(IsTrue(Reach <= 235.0 && BearingDeg <= 10.0, TEXT("the bush centre is straight ahead, within the strike's reach")));
	}

	TEST_METHOD(F_BlindStrikeOnABushKillsTheConcealedEnemyAndOnAnEmptyBushNobody)
	{
		using namespace StealthAssassinationPie;
		Network
			.Until(TEXT("the round is Active and the players split 2v1"), [this]() { return SetUpPlayers(); })
			.Then(TEXT("the enemy away from the bushes; the striker outside the nearest bush, facing its centre"), [this]()
			{
				Target = Cast<AStealthPawn>(Solo->GetPawn());
				MoveClearOfBushes(Target.Get());
				ASSERT_THAT(IsTrue(PlaceStrikerOutsideBush(Cast<AStealthPawn>(Pair[0]->GetPawn())), TEXT("a bush with a floor near the striker")));
			})
			.Then(TEXT("settle: mark"), [this]() { Mark = ServerNow(ServerWorld()); })
			.Until(TEXT("settle"), [this]() { return ServerNow(ServerWorld()) >= Mark + 0.8; })
			.Then(TEXT("edge case: a blind strike at the empty bush"), [this]()
			{
				CheckStrikerOutsideBush(TEXT("empty bush"));
				FAttackPayload Blind;
				Blind.WorldPoint = BushCentre; // no PawnRef: a TargetSpot strike at the bush (ASN-R8 rule 2)
				AsOwner(Striker.Get(), Seq, [&Blind](UPawnCommandComponent& Command) { Command.Attack(Blind); });
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("the empty-bush strike has passed its contact and the lockout"), [this]()
			{
				SampleContact();
				return ContactServerTime > 0.0 && ServerNow(ServerWorld()) >= Mark + PrototypeScenariosACFData::SecondStrikeWaitSec;
			})
			.Then(TEXT("the enemy steps into the bush"), [this]()
			{
				KillsAfterEmptyBush = Kills.Num();
				ContactServerTime = -1.0;
				ReplaceStrikerAtBush(); // the first strike's lunge moved it toward the bush
				const float HalfHeight = Target->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
				Target->TeleportTo(BushCentre + FVector(0.0, 0.0, HalfHeight + 2.0), Target->GetActorRotation(), false, true);
				Mark = ServerNow(ServerWorld());
			})
			.Until(TEXT("the enemy is concealed"), [this]()
			{
				const AStealthCharacter* Hidden = Cast<AStealthCharacter>(Target.Get());
				return Hidden != nullptr && Hidden->IsConcealed() && ServerNow(ServerWorld()) >= Mark + 0.8;
			})
			.Then(TEXT("the same blind strike at the bush"), [this]()
			{
				CheckStrikerOutsideBush(TEXT("occupied bush"));
				const AStealthCharacter* Hidden = Cast<AStealthCharacter>(Target.Get());
				TestRunner->AddInfo(FString::Printf(TEXT("ACF-F enemy %.1f cm from the bush centre, dZ %.1f cm, concealed %d"), FVector::Dist2D(Target->GetActorLocation(), BushCentre),
					Target->GetActorLocation().Z - Striker->GetActorLocation().Z, Hidden != nullptr && Hidden->IsConcealed() ? 1 : 0));
				FAttackPayload Blind;
				Blind.WorldPoint = BushCentre;
				AsOwner(Striker.Get(), Seq, [&Blind](UPawnCommandComponent& Command) { Command.Attack(Blind); });
			})
			.Until(TEXT("contact + 3 s"), [this]()
			{
				SampleContact();
				return ContactServerTime > 0.0 && ServerNow(ServerWorld()) >= ContactServerTime + PrototypeScenariosACFData::AfterContactSec;
			})
			.Then(TEXT("assert"), [this]()
			{
				TestRunner->AddInfo(FString::Printf(TEXT("ACF-F kills after the empty bush %d, after the occupied bush %d, died concealed %d"), KillsAfterEmptyBush, Kills.Num(),
					Target.IsValid() && Target->GetDeathState().bDiedConcealed ? 1 : 0));
				ASSERT_THAT(IsTrue(KillsAfterEmptyBush == 0, TEXT("the blind strike at the empty bush killed nobody")));
				ASSERT_THAT(IsTrue(Kills.Num() == 1 && Kills[0].Victim.Get() == Target.Get() && Kills[0].Killer.Get() == Striker.Get() && Kills[0].Kind == EVictimKind::Player
					&& Kills[0].Cause == EKillCause::Strike, TEXT("the blind strike killed the concealed enemy: OnPawnKilled(Enemy, Striker, Player, Strike)")));
				ASSERT_THAT(IsTrue(IsInformant(Solo.Get()), TEXT("the concealed enemy is an informant")));
				ASSERT_THAT(IsTrue(Target.IsValid() && Target->HasDeathWrite() && Target->GetDeathState().bDiedConcealed, TEXT("FDeathState: died concealed")));
			});
	}
};

#endif
