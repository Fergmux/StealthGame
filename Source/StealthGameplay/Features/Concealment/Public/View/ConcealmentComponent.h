#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "View/ConcealCollision.h"
#include "View/ConcealmentTypes.h"
#include "View/Core/StealthMotionGate.h"
#include "View/NetPawnCommand.h"
#include "View/StealthEncounterWire.h"
#include "View/StealthTuningPublicSubstructs.h"

#include "ConcealmentComponent.generated.h"

class AStealthPawn;

/**
 * The owner-side warp mechanism (ADR-0017 section 4, spike S-CON-1 story con-010; production story con-012). The server
 * always runs the one FRootMotionSource_MoveToForce for every pawn; this picks what the OWNER does:
 *   ServerOnly        nothing (the owner follows the server's corrections; the measurement baseline)
 *   OnStateArrival    (a1) the owner starts the identical source when its FActionState arrives, seeked to Now - Start
 *   PredictOnPress    (b) the owner starts the identical source at the press (public geometry: the bush anchor or the
 *                     exit point), the server stops generating corrections for that pawn and the owner stops applying
 *                     them for the transition plus one round trip, then one normal correction lands on the fixed end point
 * The spike's verdict (production/qa/spikes/2026-10-28_S-CON-1.md) is the default. Console: Stealth.Concealment.OwnerWarp.
 */
enum class EConcealOwnerWarp : uint8
{
	ServerOnly = 0,
	OnStateArrival = 1,
	PredictOnPress = 2,
};

/**
 * UConcealmentComponent (ADR-0017 sections 1, 4, 5, 6; stories con-003, con-008, con-019..con-022): one per AStealthPawn,
 * created as a default subobject of every pawn (same class, order and flags; StealthPawnFeatureComponents), and NEVER
 * replicated: SetIsReplicatedByDefault(false), no replicated property (the replicated-property allowlist has zero rows for
 * this class). It holds:
 *   - on the server, the mirror of the pawn's FConcealmentCore record (State, BushId, transition start), written only by
 *     the core through the service (SetServerState); the record itself is server-only and never crosses the wire;
 *   - on every machine, the pawn's motion gate (IStealthMotionGate): Move input is zeroed and the capsule ignores
 *     BushVolume and Pawn in every state but Outside (ComputeConcealCollision), per simulated move, written only on change.
 *     The server reads its true state; any other machine derives it from the replicated FActionState and, on the owner,
 *     the owner-only concealed flag (UConcealmentOwnerViewComponent), on an ally the C-TeamView teammate flag.
 */
UCLASS(NotBlueprintable, ClassGroup = Stealth)
class STEALTHGAMEPLAY_API UConcealmentComponent : public UActorComponent, public IStealthMotionGate
{
	GENERATED_BODY()

public:
	UConcealmentComponent();

	/** The default-subobject name on every AStealthPawn. */
	static const FName ComponentName;

	/** The component of Pawn, or null. */
	static UConcealmentComponent* Find(const AActor* Pawn);

	/** Server: the core's mirror write (the only writer). */
	void SetServerState(EConcealState InState, uint16 InBushId, double InTransitionStartServerTime);

	/** Server: the mirrored state (Outside on clients, which never learn it). */
	EConcealState GetServerState() const { return ServerState; }
	uint16 GetServerBushId() const { return ServerBushId; }
	double GetServerTransitionStart() const { return ServerTransitionStart; }

	/**
	 * The state this machine acts on at ServerTime: the server's mirror on the authority, else derived from the replicated
	 * FActionState plus the concealed evidence this client may hold (owner flag, ally flag) and the inside latch (a
	 * BushEnter seen and no completed exit since). Never more than "inside or not" is needed off the server.
	 */
	EConcealState GetEffectiveState(double ServerTime) const;

	/** Client evidence: the owner's own concealed flag, or an ally's C-TeamView flag (set by the presentation path). */
	void SetConcealedEvidence(bool bConcealed) { bConcealedEvidence = bConcealed; }

	/** The client tuning face this component reads (the schema defaults until the TDC store feeds features). */
	void SetTuning(const FConcealmentTuning_Public& InTuning) { Tuning = InTuning; }
	const FConcealmentTuning_Public& GetTuning() const { return Tuning; }

	/** The owner-side warp mechanism in effect (the console variable, default the S-CON-1 verdict). */
	static EConcealOwnerWarp GetOwnerWarpMode();

	/**
	 * Owner client: the Hide press (PredictOnPress): starts the identical root-motion source to the public end point (the
	 * bush anchor for Enter, the requested side's exit point for Exit) and suppresses applying corrections for the
	 * transition plus one round trip. No-op on the authority, on a non-owner, and in the other modes.
	 */
	void OnOwnerHidePressed(ECommandKind Kind, uint16 BushId, uint8 Side);

	/**
	 * Server: a hide warp of this pawn started (the service's StartWarp): with PredictOnPress and a remote owner, the server
	 * stops generating corrections for DurationSec plus one round trip (never accepting the client's position).
	 */
	void OnServerWarpStarted(double DurationSec);

	// ---- Presentation (ADR-0017 sections 5 and 6; stories con-019, con-020, con-022) ------------------------------------

	/** The body-hidden latch on this machine (client-local; never replicated). */
	bool IsPresentationHidden() const { return bPresentationHidden; }

	/**
	 * The one idempotent presentation evaluation (EvaluateBodyHidden at EstimatedNow) and its application: on change, the mesh
	 * and every attached primitive SetVisibility(false/true, propagate), the presentation aim channel response, and pose
	 * freshness while hidden. Never SetActorHiddenInGame / bHidden, never anything replicated. Runs on RepNotify of the action
	 * and death states, on a team-view change, every frame while a hide window is open, and on the listen host from the
	 * server's Commit and Cross items.
	 */
	void RefreshPresentation(double EstimatedNow);

	/** True until this client's fresh actor was evaluated from its full replicated state (PostNetInit). */
	bool IsFreshPending() const { return bFreshPending; }

	/** Presentation writes since construction (observation). */
	int32 GetPresentationWriteCount() const { return PresentationWrites; }

	/** Owner: the press was predicted and not yet confirmed or expired (tests, the spike's probe). */
	bool HasPendingOwnerPrediction() const { return OwnerPredictedUntil >= 0.0; }

	/** The collision writer (observation: write count; story con-008). */
	const FConcealCollisionWriter& GetCollisionWriter() const { return CollisionWriter; }

	// IStealthMotionGate
	virtual uint8 EvaluateMoveState(double MoveServerTime) override;
	virtual void ApplyMoveState(uint8 State) override;
	virtual bool IsMoveBlocked(uint8 State) const override { return State != 0; }

	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** Updates the inside latch from a new FActionState (every machine). */
	void HandleActionState(const FActionState& State);

	/** Starts the concealment root-motion source locally (the owner's prediction or a1's identical source). */
	void StartLocalWarp(const FVector& Target, double DurationSec, bool bEnter, double ElapsedSec);

	/** One round trip of this pawn's owner connection, seconds (PlayerState ping; 0.25 s without one). */
	double RoundTripSec() const;

	/** Applies the latch value to every primitive of the pawn (change-only). */
	void ApplyHidden(bool bHidden);

	/** The local viewer's C-TeamView facts about this pawn: its teammate flag, and whether any living entry is unresolved. */
	void ReadAllyEvidence(bool& bOutAllyConcealed, bool& bOutUnresolvedEntry) const;

	/** The pawn's capsule (the gate's collision target). */
	UPrimitiveComponent* GetCapsule() const;

	double ServerNow() const;

private:
	void OnPawnActionStateChanged(AStealthPawn* Pawn);

	EConcealState ServerState = EConcealState::Outside;
	uint16 ServerBushId = 0;
	double ServerTransitionStart = 0.0;

	/** Client: a BushEnter pose was seen and no exit or emerge has completed since. */
	bool bInsideLatch = false;
	/** Client: the end time of the last exit / emerge pose seen (the latch clears after it). */
	double LatchClearAt = -1.0;
	bool bConcealedEvidence = false;
	/** Owner: the predicted transition counts as inside until this server time (-1: none). */
	double OwnerPredictedUntil = -1.0;
	/** Owner: corrections are ignored until this server time (-1: none). */
	double OwnerIgnoreCorrectionsUntil = -1.0;
	/** Server: corrections are not generated until this server time (-1: none). */
	double ServerIgnoreCorrectionsUntil = -1.0;
	/** The latch (clients start hidden: a fresh network actor shows nothing until it is evaluated). */
	bool bPresentationHidden = false;
	/** The value the primitives were last set to. */
	bool bAppliedHidden = false;
	/** The mesh's anim tick option before the hide (restored at the show; another owner, e.g. the S10 warm-up, may hold it). */
	EVisibilityBasedAnimTickOption PreHideTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
	/** Client: not yet evaluated from the full replicated state. */
	bool bFreshPending = false;
	/** Client: when the fresh actor first could be evaluated (the unresolved-ally hold's bound). */
	double FreshHeldSince = -1.0;
	int32 PresentationWrites = 0;
	/** The FActionState Seq this machine last saw (a1 starts once per pose write). */
	uint16 LastSeenActionSeq = 0;

	FConcealmentTuning_Public Tuning;
	FConcealCollisionWriter CollisionWriter;
	FDelegateHandle ActionStateHandle;
};
