#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/EncounterTypes.h"
#include "Server/StealthPenaltySeams.h"
#include "Server/StrikeTallySource.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "StealthPenaltyService.generated.h"

class APawn;
class FPenaltyCore;
class FResolutionQueueCore;
class IRoundLogWriter;
class IStealthClock;
class IStealthFaultSink;
class UEncounterService;
class URosterStore;
struct FRosterEvent;
struct FPenaltyServiceAdapters;

/** What the composition root hands the service (Clock required; the rest optional, each documented). */
struct FPenaltyServiceInit
{
	const IStealthClock* Clock = nullptr;
	/** The round queue: the slot context of Penalty's slot-local flags and Event Log rows (null: no row is written). */
	FResolutionQueueCore* Queue = nullptr;
	/** Round's Event Log writer (FRoundMatchCore::GetRoundLogWriter()). */
	IRoundLogWriter* Log = nullptr;
	/** Overrides the world's Roster store (null: URosterStore::Get(World)). */
	URosterStore* Roster = nullptr;
	/** The encounter service whose OnPawnKilled Penalty subscribes to (null: no kill reaches Penalty). */
	UEncounterService* Encounters = nullptr;
	/** Overrides the tuning stand-in (null: the schema defaults, until the TDC store feeds features the frozen set). */
	const IPenaltyTuningSource* Tuning = nullptr;
	/** The Roster owner-view feed (null until roster story psr-038 builds SetOwnerTally: nothing is re-pushed). */
	IPenaltyOwnerViewFeed* OwnerView = nullptr;
	IStealthFaultSink* Faults = nullptr;
};

/**
 * UPenaltyService: the NPC-Kill Penalty & Informants server facade (ADR-0019 section 1; ADR-0007 thin facade over
 * FPenaltyCore; stories nkp-001 to nkp-004). A world subsystem that is bound only on the server (Bind refuses a client
 * world); it owns the plain core and the production adapters and adds no rule of its own. Nothing in it is a replicated
 * property or reachable from Blueprint: StrikeCount never leaves the server, the tally reaches its owner only through
 * FStrikeResult and the owner re-push.
 *
 * Seams: implements IStrikeTallySource (ADR-0018 section 2.1 step 8); subscribes to UEncounterService's OnPawnKilled and
 * to the Roster's events (OnRosterReset, OnRosterRoundEnded, reclaim and connection changes). IInformantBodyProvider and
 * the InformantRespawn resolver arrive with stories nkp-009 / nkp-010.
 *
 * Usage (the composition root, after the encounter service is initialised on the round queue):
 *   UPenaltyService* Penalty = World->GetSubsystem<UPenaltyService>();
 *   FPenaltyServiceInit Init; Init.Clock = Clock; Init.Queue = Queue; Init.Log = MatchCore->GetRoundLogWriter();
 *   Init.Encounters = Encounters; Init.Faults = Faults;
 *   Penalty->Bind(Init);            // and Penalty->OnRoundPreparing() at each Preparing; Unbind() before the queue goes
 */
UCLASS(NotBlueprintable, NotBlueprintType)
class STEALTHGAMEPLAY_API UPenaltyService : public UWorldSubsystem, public IStrikeTallySource
{
	GENERATED_BODY()

public:
	/** Game worlds only (the editor world never has one). */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	/** Builds the adapters and the core and subscribes. False (nothing built) without a clock or on a client world. */
	bool Bind(const FPenaltyServiceInit& Init);
	/** Unsubscribes and releases the core (call before the round queue or the encounter core is released). */
	void Unbind();
	bool IsBound() const { return Core.IsValid(); }

	/** Round entered Preparing: the core latches L and the release grace (TDC rule CR-11 Latching). */
	void OnRoundPreparing();
	/** The participant's C-TeamView was rebound after a rejoin (Roster OnViewRebound, story psr-038): the owner re-push. */
	void OnViewRebound(FParticipantId Participant);

	// IStrikeTallySource
	virtual FStrikeTally GetTallyForResult(const APawn* Attacker, EStrikeResult Result) override;
	virtual FStrikeTally GetTallyForStrike(const APawn* Attacker, EStrikeResult Result, const FClientCommandSeq& Seq) override;

	/** The core (valid between Bind and Unbind; server-only reads for tests and dumps). */
	FPenaltyCore* GetCore() const { return Core.Get(); }

	/** The opaque, stable key of a pawn (its object index and serial number); unset for null. */
	static FPenaltyPawnKey MakePawnKey(const APawn* Pawn);

private:
	void HandlePawnKilled(APawn* Victim, APawn* Killer, EVictimKind Kind, EKillCause Cause);
	void HandleRosterEvent(const FRosterEvent& Event);
	FParticipantId ResolveParticipant(const APawn* Pawn) const;

	TSharedPtr<FPenaltyServiceAdapters> Adapters;
	TSharedPtr<FPenaltyCore> Core;
	TWeakObjectPtr<URosterStore> Roster;
	TWeakObjectPtr<UEncounterService> Encounters;
	/** The delegate subscribed to (compared before removal: a released encounter core has none). */
	FOnPawnKilled* BoundPawnKilled = nullptr;
	FDelegateHandle PawnKilledHandle;
	FDelegateHandle RosterEventHandle;
};
