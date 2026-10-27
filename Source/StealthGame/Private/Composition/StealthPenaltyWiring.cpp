#include "StealthPenaltyWiring.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Server/Core/Round/StealthCrowdControl.h"
#include "Server/Core/StealthPawnControlQuery.h"
#include "Server/Core/StealthPawnFactory.h"
#include "Server/CrowdDirector.h"
#include "Server/Random/StealthSecretStream.h"
#include "Server/Random/StealthServerSeeds.h"
#include "Server/Roster/StealthRosterStore.h"
#include "Server/Session/StealthSessionService.h"
#include "GameFramework/GameStateBase.h"
#include "Server/StealthCompositionRoot.h"
#include "Server/StealthPenaltyHarnessSeams.h"
#include "Server/StealthPenaltyService.h"
#include "Server/StealthTuningInvariants.h"
#include "StealthCharacter.h"
#include "StealthGame.h"
#include "StealthGameMode.h"
#include "View/Core/StealthPawn.h"
#include "View/Random/StealthRandomTag.h"
#include "View/Random/StealthRangedDraw.h"

namespace StealthPenaltyWiringLocal
{
	/** The stand-in pick's stream tag (ADR-0006 section 5: one tag per draw site). */
	constexpr uint64 RespawnPickStreamTag = StealthRandom::Tag("RSPNPICK");
	/**
	 * The key of the HARNESS-ONLY crowd-less team-start respawn (StealthPenaltyHarnessSeams; never in production): no real
	 * pawn has it (a pawn key is serial << 32 | object index, and no object has index 0xFFFFFFFF).
	 */
	constexpr uint64 TeamStartFallbackKey = 0xFA11BAC5FFFFFFFFull;

	/** True only in a test-seam build whose harness switched the crowd-less team-start respawn on (story nkp-014). */
	bool IsHarnessCrowdlessRespawnOn()
	{
#if WITH_STEALTH_TEST_SEAMS
		return StealthPenaltyHarnessSeams::IsCrowdlessTeamStartRespawn();
#else
		return false;
#endif
	}

#if WITH_STEALTH_TEST_SEAMS
	/** One probe event stamped with the frame and the server time (story nkp-014). */
	StealthPenaltyHarnessSeams::FRespawnProbeEvent ProbeEvent(StealthPenaltyHarnessSeams::ERespawnProbeStep Step, const UWorld* World, APawn* Pawn)
	{
		StealthPenaltyHarnessSeams::FRespawnProbeEvent Event;
		Event.Step = Step;
		Event.Pawn = Pawn;
		Event.Location = Pawn != nullptr ? Pawn->GetActorLocation() : FVector::ZeroVector;
		Event.Frame = GFrameCounter;
		const AGameStateBase* State = World != nullptr ? World->GetGameState() : nullptr;
		Event.ServerTime = State != nullptr ? State->GetServerWorldTimeSeconds() : 0.0;
		return Event;
	}
#endif
}

// ---- FStealthPenaltyPawnGateway (story nkp-005) ---------------------------------------------------------------------

FStealthPenaltyPawnGateway::FStealthPenaltyPawnGateway(AStealthGameMode* InGameMode)
	: GameMode(InGameMode)
{
}

void FStealthPenaltyPawnGateway::ReleaseCard(APawn* Pawn)
{
	// STAND-IN (Character Pool & Appearance not built): no card ledger exists and the pawn keeps its look.
}

void FStealthPenaltyPawnGateway::RequestHandOverToAi(APawn* Pawn, uint32 ExpectedRoundEpoch, double PossessNotBeforeServerTime)
{
	if (Pawn == nullptr)
	{
		return;
	}
	AController* Former = Pawn->GetController();
	if (UStealthPawnFactory* Factory = UStealthPawnFactory::Get(Pawn->GetWorld()))
	{
		Factory->RequestHandOver(Pawn, EStealthPawnControlKind::Ai, PossessNotBeforeServerTime);
	}
	if (AStealthGameMode* Mode = GameMode.Get())
	{
		Mode->HandleProductionExile(Cast<AStealthCharacter>(Pawn), Former);
	}
}

// ---- FStealthInformantBodiesProvider (story nkp-009) ----------------------------------------------------------------

void FStealthInformantBodiesProvider::SetService(UPenaltyService* InService)
{
	Service = InService;
}

void FStealthInformantBodiesProvider::OnInformantAwaitingBody(FParticipantId Participant, ERespawnCause Cause, double LostServerTime)
{
	if (UPenaltyService* Penalty = Service.Get())
	{
		Penalty->OnInformantAwaitingBody(Participant, Cause, LostServerTime);
	}
}

void FStealthInformantBodiesProvider::OnInformantReadmitted(FParticipantId Participant)
{
	if (UPenaltyService* Penalty = Service.Get())
	{
		Penalty->OnInformantReadmitted(Participant);
	}
}

void FStealthInformantBodiesProvider::OnInformantPawnLost(FParticipantId Participant)
{
	if (UPenaltyService* Penalty = Service.Get())
	{
		Penalty->OnInformantPawnLost(Participant);
	}
}

// ---- FStealthRosterOwnerViewFeed (story nkp-011) --------------------------------------------------------------------

FStealthRosterOwnerViewFeed::FStealthRosterOwnerViewFeed(UStealthCompositionRoot* InRoot)
	: Root(InRoot)
{
}

void FStealthRosterOwnerViewFeed::SetOwnerTally(FParticipantId Participant, uint8 WrongTargetTally, FClientCommandSeq LastResultSeq, uint32 ExpectedEpoch)
{
	// STAND-IN: roster story psr-038's SetOwnerTally is not built (the tally reaches its owner through FStrikeResult).
}

void FStealthRosterOwnerViewFeed::SetOwnerRespawnDue(FParticipantId Participant, double DueServerTime, uint32 ExpectedEpoch)
{
	const UStealthCompositionRoot* Owner = Root.Get();
	URosterStore* Store = Owner != nullptr ? Owner->GetRosterStore() : nullptr;
	if (Store != nullptr && Store->IsStoreInitialized())
	{
		Store->SetOwnerRespawnDue(Participant, DueServerTime, ExpectedEpoch);
	}
}

// ---- FStealthPenaltyRespawnGateway (story nkp-012) ------------------------------------------------------------------

FStealthPenaltyRespawnGateway::FStealthPenaltyRespawnGateway(UWorld* InWorld, UStealthCompositionRoot* InRoot)
	: World(InWorld)
	, Root(InRoot)
{
}

APawn* FStealthPenaltyRespawnGateway::Resolve(FPenaltyPawnKey Key) const
{
	const TWeakObjectPtr<APawn>* Found = Known.Find(Key);
	return Found != nullptr ? Found->Get() : nullptr;
}

bool FStealthPenaltyRespawnGateway::IsTakeoverEligible(APawn* Pawn) const
{
	AStealthPawn* Shared = Cast<AStealthPawn>(Pawn);
	const UStealthCompositionRoot* Owner = Root.Get();
	URosterStore* Store = Owner != nullptr ? Owner->GetRosterStore() : nullptr;
	const UCrowdDirector* Crowd = UCrowdDirector::Get(World.Get());
	if (Shared == nullptr || !IsValid(Shared) || Shared->IsActorBeingDestroyed() || Shared->HasDeathWrite() || Store == nullptr || !Store->IsStoreInitialized()
		|| Crowd == nullptr)
	{
		return false;
	}
	const AController* Controller = Shared->GetController();
	if (Controller == nullptr || Controller->IsA<APlayerController>() || Store->IsPawnBound(Shared))
	{
		return false; // a human-driven pawn, a pawn mid hand-over, or a participant's body is never taken over
	}
	return Crowd->GetTakeoverEligibility(Shared) == ETakeoverEligibility::Eligible;
}

bool FStealthPenaltyRespawnGateway::FindTeamAnchor(ERosterTeam Team, FVector& OutAnchor) const
{
	UWorld* Resolved = World.Get();
	if (Resolved == nullptr || Team == ERosterTeam::None)
	{
		return false;
	}
	const FName WantedTag(*FString::Printf(TEXT("Team%d"), static_cast<int32>(Team)));
	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;
	for (TActorIterator<APlayerStart> It(Resolved); It; ++It)
	{
		if (It->PlayerStartTag == WantedTag)
		{
			Sum += It->GetActorLocation();
			++Count;
		}
	}
	if (Count == 0)
	{
		return false;
	}
	OutAnchor = Sum / static_cast<double>(Count);
	return true;
}

bool FStealthPenaltyRespawnGateway::IsUnseenByOpponents(ERosterTeam Team, const APawn* Candidate) const
{
	UWorld* Resolved = World.Get();
	const UStealthCompositionRoot* Owner = Root.Get();
	URosterStore* Store = Owner != nullptr ? Owner->GetRosterStore() : nullptr;
	if (Resolved == nullptr || Store == nullptr || !Store->IsStoreInitialized())
	{
		return false;
	}
	TArray<FParticipantId> Ids;
	Store->GetParticipantIds(Ids);
	const FVector Target = Candidate->GetActorLocation();
	for (const FParticipantId& Id : Ids)
	{
		const FParticipantRecord* Record = Store->FindRecord(Id);
		if (Record == nullptr || Record->Team == Team || Record->Team == ERosterTeam::None || !Record->HasLivePawn())
		{
			continue;
		}
		const APawn* Viewer = Store->GetPawn(Id);
		if (Viewer == nullptr)
		{
			continue;
		}
		// The ViewBlock trace stand-in: anything blocking Visibility between the viewer's eyes and the pawn hides it.
		FCollisionQueryParams Params(SCENE_QUERY_STAT(PenaltyRespawnUnseen), false);
		Params.AddIgnoredActor(Viewer);
		Params.AddIgnoredActor(Candidate);
		FHitResult Hit;
		if (!Resolved->LineTraceSingleByChannel(Hit, Viewer->GetPawnViewLocation(), Target, ECC_Visibility, Params))
		{
			return false; // a clear line: seen
		}
	}
	return true;
}

FPenaltyRespawnPick FStealthPenaltyRespawnGateway::PickRespawnPawn(ERosterTeam Team, uint32 RequestSeq, TConstArrayView<FPenaltyPawnKey> Exclude)
{
	const FPenaltyRespawnPick Out = Pick(Team, RequestSeq, Exclude);
#if WITH_STEALTH_TEST_SEAMS
	if (StealthPenaltyHarnessSeams::IsRespawnProbeOn())
	{
		StealthPenaltyHarnessSeams::FRespawnProbeEvent Event = StealthPenaltyWiringLocal::ProbeEvent(StealthPenaltyHarnessSeams::ERespawnProbeStep::Pick, World.Get(), Resolve(Out.Pawn));
		Event.RequestSeq = RequestSeq;
		Event.Team = Team;
		Event.PickStatus = Out.Status;
		Event.WideningLevel = Out.WideningLevel;
		StealthPenaltyHarnessSeams::RecordRespawnProbeEvent(Event);
	}
#endif
	return Out;
}

FPenaltyRespawnPick FStealthPenaltyRespawnGateway::Pick(ERosterTeam Team, uint32 RequestSeq, TConstArrayView<FPenaltyPawnKey> Exclude)
{
	FPenaltyRespawnPick Out;
	if (LastPickFrame == GFrameCounter)
	{
		Out.Status = EPenaltyPickStatus::Deferred; // ADR-0014's one pick per server frame
		return Out;
	}
	LastPickFrame = GFrameCounter;
	UWorld* Resolved = World.Get();
	FVector Anchor;
	if (Resolved == nullptr || !FindTeamAnchor(Team, Anchor))
	{
		return Out; // None: no region for this team (yet)
	}
	FVector EnemyAnchor;
	const ERosterTeam Enemy = Team == ERosterTeam::Team0 ? ERosterTeam::Team1 : ERosterTeam::Team0;
	const bool bHasEnemyAnchor = FindTeamAnchor(Enemy, EnemyAnchor);
	const double Radius = StealthTuningPendingOwnerKeys::InformantRespawnPathCm;

	// Every eligible, unseen, non-excluded crowd pawn, with its distance to the anchor; sorted by stable spawn ordinal.
	struct FCandidate
	{
		APawn* Pawn = nullptr;
		uint32 Ordinal = 0;
		double DistanceToAnchor = 0.0;
		double DistanceToEnemyAnchor = 0.0;
	};
	TArray<FCandidate> All;
	bool bAnyCrowdAgent = false;
	const UCrowdDirector* Crowd = UCrowdDirector::Get(Resolved);
	const UStealthCompositionRoot* Owner = Root.Get();
	URosterStore* Store = Owner != nullptr ? Owner->GetRosterStore() : nullptr;
	for (TActorIterator<AStealthPawn> It(Resolved); It; ++It)
	{
		AStealthPawn* Pawn = *It;
		// Any live, unbound pawn the Crowd Director drives (its eligibility is not Bound) means the city has a crowd.
		bAnyCrowdAgent |= Crowd != nullptr && Store != nullptr && Store->IsStoreInitialized() && IsValid(Pawn) && !Pawn->HasDeathWrite()
			&& !Store->IsPawnBound(Pawn) && Crowd->GetTakeoverEligibility(Pawn) != ETakeoverEligibility::Bound;
		const FPenaltyPawnKey Key = UPenaltyService::MakePawnKey(Pawn);
		if (Exclude.Contains(Key) || !IsTakeoverEligible(Pawn) || !IsUnseenByOpponents(Team, Pawn))
		{
			continue;
		}
		FCandidate& Candidate = All.AddDefaulted_GetRef();
		Candidate.Pawn = Pawn;
		Candidate.Ordinal = StealthPawnControl::GetSpawnOrdinal(Pawn);
		Candidate.DistanceToAnchor = FVector::Dist2D(Pawn->GetActorLocation(), Anchor);
		Candidate.DistanceToEnemyAnchor = bHasEnemyAnchor ? FVector::Dist2D(Pawn->GetActorLocation(), EnemyAnchor) : TNumericLimits<double>::Max();
	}
	All.Sort([](const FCandidate& A, const FCandidate& B) { return A.Ordinal < B.Ordinal; });

	// The widening stand-in: level 0 (radius), level 1 (2 x radius), level 2 (anywhere not within the radius of the enemy anchor).
	TArray<APawn*, TInlineAllocator<64>> Pool;
	for (uint8 Level = 0; Level <= 2 && Pool.Num() == 0; ++Level)
	{
		for (const FCandidate& Candidate : All)
		{
			const bool bIn = Level == 0 ? Candidate.DistanceToAnchor <= Radius
				: (Level == 1 ? Candidate.DistanceToAnchor <= 2.0 * Radius : Candidate.DistanceToEnemyAnchor > Radius);
			if (bIn)
			{
				Pool.Add(Candidate.Pawn);
			}
		}
		Out.WideningLevel = Level;
	}
	if (Pool.Num() == 0)
	{
		Out.WideningLevel = 2;
		if (!bAnyCrowdAgent && StealthPenaltyWiringLocal::IsHarnessCrowdlessRespawnOn())
		{
			// HARNESS-ONLY (story nkp-014; StealthPenaltyHarnessSeams, compiled out of Shipping and off unless a PIE harness
			// round switches it on): a world with no crowd at all has nothing to take over, so the harness round's
			// TakeOverCrowdPawn spawns the participant's pawn at its team start. Production never reaches this branch.
			Out.Status = EPenaltyPickStatus::Picked;
			Out.Pawn = FPenaltyPawnKey(StealthPenaltyWiringLocal::TeamStartFallbackKey);
		}
		return Out; // None: keep waiting (no spawn, the unseen test never relaxed; PEN-R10)
	}

	// The only random draw: uniform over the pool from the request's RespawnPick stream (it continues across retries).
	const USessionService* Session = USessionService::Get(Resolved);
	const uint64 SubSeed = Session != nullptr ? Session->GetSubSeedSource().GetServerSubSeed(ESeedDomain::RespawnPick, RequestSeq) : 0;
	FSecretStream Stream(SubSeed, StealthPenaltyWiringLocal::RespawnPickStreamTag);
	uint32& Drawn = DrawsByRequest.FindOrAdd(RequestSeq);
	for (uint32 Skip = 0; Skip < Drawn; ++Skip)
	{
		Stream.NextU64();
	}
	const uint32 Index = StealthRandom::UniformBelow(Stream, static_cast<uint32>(Pool.Num()));
	++Drawn;
	APawn* Picked = Pool[static_cast<int32>(Index)];
	Out.Status = EPenaltyPickStatus::Picked;
	Out.Pawn = UPenaltyService::MakePawnKey(Picked);
	Known.Add(Out.Pawn, Picked);
	return Out;
}

bool FStealthPenaltyRespawnGateway::CheckTakeOverEligibility(FPenaltyPawnKey Pawn) const
{
	const bool bEligible = Pawn.Value == StealthPenaltyWiringLocal::TeamStartFallbackKey || IsTakeoverEligible(Resolve(Pawn));
#if WITH_STEALTH_TEST_SEAMS
	if (StealthPenaltyHarnessSeams::IsRespawnProbeOn())
	{
		StealthPenaltyHarnessSeams::FRespawnProbeEvent Event = StealthPenaltyWiringLocal::ProbeEvent(StealthPenaltyHarnessSeams::ERespawnProbeStep::PreCheck, World.Get(), Resolve(Pawn));
		Event.bOk = bEligible;
		StealthPenaltyHarnessSeams::RecordRespawnProbeEvent(Event);
	}
#endif
	return bEligible;
}

bool FStealthPenaltyRespawnGateway::TryReleaseNpc(FPenaltyPawnKey Pawn, uint32 ExpectedRoundEpoch)
{
	const bool bReleased = Release(Pawn, ExpectedRoundEpoch);
#if WITH_STEALTH_TEST_SEAMS
	if (StealthPenaltyHarnessSeams::IsRespawnProbeOn())
	{
		StealthPenaltyHarnessSeams::FRespawnProbeEvent Event = StealthPenaltyWiringLocal::ProbeEvent(StealthPenaltyHarnessSeams::ERespawnProbeStep::Release, World.Get(), Resolve(Pawn));
		Event.bOk = bReleased;
		StealthPenaltyHarnessSeams::RecordRespawnProbeEvent(Event);
	}
#endif
	return bReleased;
}

bool FStealthPenaltyRespawnGateway::Release(FPenaltyPawnKey Pawn, uint32 ExpectedRoundEpoch)
{
#if WITH_STEALTH_TEST_SEAMS
	if (StealthPenaltyHarnessSeams::ConsumeInjectedReleaseFailure())
	{
		return false; // story nkp-014's injected failure: the crowd is not touched
	}
#endif
	if (Pawn.Value == StealthPenaltyWiringLocal::TeamStartFallbackKey)
	{
		return true; // the fallback has no crowd pawn to release
	}
	AStealthPawn* Shared = Cast<AStealthPawn>(Resolve(Pawn));
	UCrowdDirector* Crowd = UCrowdDirector::Get(World.Get());
	const UStealthCompositionRoot* Owner = Root.Get();
	const URosterStore* Store = Owner != nullptr ? Owner->GetRosterStore() : nullptr;
	if (Shared == nullptr || Crowd == nullptr || Store == nullptr || !Store->IsStoreInitialized())
	{
		return false;
	}
	return Crowd->TryReleaseNpc(Shared, FEpochToken(Store->GetEpochToken().MatchId, ExpectedRoundEpoch));
}

EPenaltyTakeoverResult FStealthPenaltyRespawnGateway::TakeOverCrowdPawn(FPenaltyPawnKey Pawn, FParticipantId Participant, uint32 ExpectedRoundEpoch)
{
#if WITH_STEALTH_TEST_SEAMS
	// Story nkp-014's probe: the stand-in LSC sight query re-run at the commit tick, before the takeover changes anything.
	const bool bProbe = StealthPenaltyHarnessSeams::IsRespawnProbeOn();
	StealthPenaltyHarnessSeams::FRespawnProbeEvent Event = StealthPenaltyWiringLocal::ProbeEvent(StealthPenaltyHarnessSeams::ERespawnProbeStep::Takeover, World.Get(), Resolve(Pawn));
	if (bProbe)
	{
		const UStealthCompositionRoot* ProbeRoot = Root.Get();
		URosterStore* ProbeStore = ProbeRoot != nullptr ? ProbeRoot->GetRosterStore() : nullptr;
		const FParticipantRecord* ProbeRecord = ProbeStore != nullptr && ProbeStore->IsStoreInitialized() ? ProbeStore->FindRecord(Participant) : nullptr;
		const APawn* Candidate = Resolve(Pawn);
		Event.Participant = Participant;
		Event.bUnseenAtCommit = ProbeRecord != nullptr && Candidate != nullptr && IsUnseenByOpponents(ProbeRecord->Team, Candidate);
	}
	const EPenaltyTakeoverResult Result = TakeOver(Pawn, Participant, ExpectedRoundEpoch);
	if (bProbe)
	{
		Event.TakeoverResult = Result;
		StealthPenaltyHarnessSeams::RecordRespawnProbeEvent(Event);
	}
	return Result;
#else
	return TakeOver(Pawn, Participant, ExpectedRoundEpoch);
#endif
}

EPenaltyTakeoverResult FStealthPenaltyRespawnGateway::TakeOver(FPenaltyPawnKey Pawn, FParticipantId Participant, uint32 ExpectedRoundEpoch)
{
	if (Pawn.Value == StealthPenaltyWiringLocal::TeamStartFallbackKey)
	{
		// HARNESS-ONLY (the pick names this key only while StealthPenaltyHarnessSeams' crowd-less respawn is on): the
		// participant's pawn at its team start through the factory (BindPawn inside). Never reached in production.
		UStealthCompositionRoot* FallbackRoot = Root.Get();
		AStealthGameMode* Mode = FallbackRoot != nullptr ? FallbackRoot->GetGameMode() : nullptr;
		APlayerController* FallbackPlayer = FallbackRoot != nullptr ? FallbackRoot->GetController(Participant) : nullptr;
		APawn* Spawned = Mode != nullptr && FallbackPlayer != nullptr && StealthPenaltyWiringLocal::IsHarnessCrowdlessRespawnOn()
			? Mode->SpawnHarnessCrowdlessRespawnPawn(FallbackPlayer) : nullptr;
		if (Spawned == nullptr)
		{
			return EPenaltyTakeoverResult::Rejected_Stale;
		}
#if WITH_STEALTH_TEST_SEAMS
		StealthPenaltyHarnessSeams::FRespawnProbeEvent Spawn = StealthPenaltyWiringLocal::ProbeEvent(StealthPenaltyHarnessSeams::ERespawnProbeStep::HarnessSpawn, World.Get(), Spawned);
		Spawn.Participant = Participant;
		StealthPenaltyHarnessSeams::RecordRespawnProbeEvent(Spawn);
#endif
		UE_LOGF(LogStealth, Log, "[Penalty] informant respawn (HARNESS-ONLY crowd-less round): participant %u spawned %ls at its team start", Participant.Value,
			*Spawned->GetName());
		return EPenaltyTakeoverResult::Applied;
	}
	APawn* Target = Resolve(Pawn);
	AStealthPawn* Shared = Cast<AStealthPawn>(Target);
	UStealthCompositionRoot* Owner = Root.Get();
	URosterStore* Store = Owner != nullptr ? Owner->GetRosterStore() : nullptr;
	UStealthPawnFactory* Factory = UStealthPawnFactory::Get(World.Get());
	UCrowdDirector* Crowd = UCrowdDirector::Get(World.Get());
	// The SCL-028 predicate (TryReleaseNpc has already taken the pawn out of the crowd, so Crowd answers Bound now).
	if (Shared == nullptr || !IsValid(Shared) || Shared->IsActorBeingDestroyed() || Shared->HasDeathWrite() || Store == nullptr
		|| !Store->IsStoreInitialized() || Factory == nullptr || Store->IsPawnBound(Shared))
	{
		return EPenaltyTakeoverResult::NotEligible;
	}
	APlayerController* Player = Owner->GetController(Participant);
	if (Player == nullptr)
	{
		return EPenaltyTakeoverResult::Rejected_Stale; // no controller for the participant: the request cannot be met
	}
	FRosterEpochToken Expected = Store->GetEpochToken();
	Expected.RoundEpoch = ExpectedRoundEpoch;
	if (Store->BindPawn(Participant, Shared, Expected) != ERosterResult::Applied)
	{
		return EPenaltyTakeoverResult::Rejected_Stale; // Rejected_StaleEpoch / WrongState
	}
	if (Crowd != nullptr)
	{
		Crowd->ReleaseControl(Shared);
	}
	Factory->RequestHandOverToPlayer(Shared, Player);
	// Composition-root diagnostics only (no Penalty value): which crowd pawn the stand-in takeover bound, and where.
	UE_LOGF(LogStealth, Log, "[Penalty] informant takeover (stand-in): participant %u binds %ls at %ls", Participant.Value, *Shared->GetName(),
		*Shared->GetActorLocation().ToCompactString());
	return EPenaltyTakeoverResult::Applied;
}

void FStealthPenaltyRespawnGateway::AdoptReleasedPawn(FPenaltyPawnKey Pawn)
{
	if (Pawn.Value == StealthPenaltyWiringLocal::TeamStartFallbackKey)
	{
		return; // nothing was released
	}
	AStealthPawn* Shared = Cast<AStealthPawn>(Resolve(Pawn));
	if (UCrowdDirector* Crowd = UCrowdDirector::Get(World.Get()); Crowd != nullptr && Shared != nullptr)
	{
		Crowd->AdoptReleasedPawn(Shared, FCrowdAdoptParams());
	}
}

TOptional<uint16> FStealthPenaltyRespawnGateway::TakeNpcCard(FParticipantId Participant, FPenaltyPawnKey Pawn)
{
	// STAND-IN (Character Pool not built): no ledger; the pawn keeps its look and its card is untouched.
#if WITH_STEALTH_TEST_SEAMS
	if (StealthPenaltyHarnessSeams::IsRespawnProbeOn())
	{
		StealthPenaltyHarnessSeams::FRespawnProbeEvent Event = StealthPenaltyWiringLocal::ProbeEvent(StealthPenaltyHarnessSeams::ERespawnProbeStep::Card, World.Get(), Resolve(Pawn));
		Event.Participant = Participant;
		StealthPenaltyHarnessSeams::RecordRespawnProbeEvent(Event);
	}
#endif
	return TOptional<uint16>(0);
}

void FStealthPenaltyRespawnGateway::RecordAppearance(FParticipantId Participant, uint16 AppearanceId)
{
	// STAND-IN no-op: the Roster's AppearanceHistory writer is unnamed (psr-022).
}

// ---- StealthPenaltyHarnessSeams (story nkp-014; test-seam builds only) -------------------------------------------------

#if WITH_STEALTH_TEST_SEAMS
namespace StealthPenaltyHarnessSeamsLocal
{
	bool GCrowdlessTeamStartRespawn = false;
	bool GProbeOn = false;
	TArray<StealthPenaltyHarnessSeams::FRespawnProbeEvent> GEvents;
	int32 GHarnessSpawns = 0;
	int32 GInjectedReleaseFailures = 0;
}

void StealthPenaltyHarnessSeams::SetCrowdlessTeamStartRespawn(bool bOn) { StealthPenaltyHarnessSeamsLocal::GCrowdlessTeamStartRespawn = bOn; }
bool StealthPenaltyHarnessSeams::IsCrowdlessTeamStartRespawn() { return StealthPenaltyHarnessSeamsLocal::GCrowdlessTeamStartRespawn; }

void StealthPenaltyHarnessSeams::SetRespawnProbe(bool bOn)
{
	StealthPenaltyHarnessSeamsLocal::GProbeOn = bOn;
	StealthPenaltyHarnessSeamsLocal::GEvents.Reset();
	StealthPenaltyHarnessSeamsLocal::GInjectedReleaseFailures = 0;
}

bool StealthPenaltyHarnessSeams::IsRespawnProbeOn() { return StealthPenaltyHarnessSeamsLocal::GProbeOn; }

const TArray<StealthPenaltyHarnessSeams::FRespawnProbeEvent>& StealthPenaltyHarnessSeams::GetRespawnProbeEvents() { return StealthPenaltyHarnessSeamsLocal::GEvents; }

void StealthPenaltyHarnessSeams::RecordRespawnProbeEvent(const FRespawnProbeEvent& Event)
{
	if (Event.Step == ERespawnProbeStep::HarnessSpawn)
	{
		++StealthPenaltyHarnessSeamsLocal::GHarnessSpawns;
	}
	if (StealthPenaltyHarnessSeamsLocal::GProbeOn)
	{
		StealthPenaltyHarnessSeamsLocal::GEvents.Add(Event);
	}
}

int32 StealthPenaltyHarnessSeams::GetHarnessSpawnCount() { return StealthPenaltyHarnessSeamsLocal::GHarnessSpawns; }

void StealthPenaltyHarnessSeams::InjectReleaseFailures(int32 Count) { StealthPenaltyHarnessSeamsLocal::GInjectedReleaseFailures = FMath::Max(0, Count); }

bool StealthPenaltyHarnessSeams::ConsumeInjectedReleaseFailure()
{
	if (StealthPenaltyHarnessSeamsLocal::GInjectedReleaseFailures <= 0)
	{
		return false;
	}
	--StealthPenaltyHarnessSeamsLocal::GInjectedReleaseFailures;
	return true;
}
#endif
