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
#include "Server/StealthCompositionRoot.h"
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
	 * The key of the STAND-IN team-start fallback (a world with no crowd agent at all): no real pawn has it (a pawn key is
	 * serial << 32 | object index, and no object has index 0xFFFFFFFF).
	 */
	constexpr uint64 TeamStartFallbackKey = 0xFA11BAC5FFFFFFFFull;
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
		if (!bAnyCrowdAgent)
		{
			// STAND-IN: a world with no crowd at all (the 0-NPC harness rounds) has nothing to take over: the team-start
			// fallback (TakeOverCrowdPawn spawns the participant's pawn at its team start, the prototype respawn).
			Out.Status = EPenaltyPickStatus::Picked;
			Out.Pawn = FPenaltyPawnKey(StealthPenaltyWiringLocal::TeamStartFallbackKey);
		}
		return Out; // otherwise None: keep waiting (the unseen test is never relaxed)
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
	return Pawn.Value == StealthPenaltyWiringLocal::TeamStartFallbackKey || IsTakeoverEligible(Resolve(Pawn));
}

bool FStealthPenaltyRespawnGateway::TryReleaseNpc(FPenaltyPawnKey Pawn, uint32 ExpectedRoundEpoch)
{
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
	if (Pawn.Value == StealthPenaltyWiringLocal::TeamStartFallbackKey)
	{
		// STAND-IN (no crowd in this world): the participant's pawn at its team start through the factory (BindPawn inside).
		UStealthCompositionRoot* FallbackRoot = Root.Get();
		AStealthGameMode* Mode = FallbackRoot != nullptr ? FallbackRoot->GetGameMode() : nullptr;
		APlayerController* FallbackPlayer = FallbackRoot != nullptr ? FallbackRoot->GetController(Participant) : nullptr;
		APawn* Spawned = Mode != nullptr && FallbackPlayer != nullptr ? Mode->SpawnInformantFallbackPawn(FallbackPlayer) : nullptr;
		if (Spawned == nullptr)
		{
			return EPenaltyTakeoverResult::Rejected_Stale;
		}
		UE_LOGF(LogStealth, Log, "[Penalty] informant respawn (stand-in, no crowd to take over): participant %u spawned %ls at its team start", Participant.Value,
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
	return TOptional<uint16>(0);
}

void FStealthPenaltyRespawnGateway::RecordAppearance(FParticipantId Participant, uint16 AppearanceId)
{
	// STAND-IN no-op: the Roster's AppearanceHistory writer is unnamed (psr-022).
}
