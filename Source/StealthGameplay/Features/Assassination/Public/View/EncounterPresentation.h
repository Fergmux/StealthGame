#pragma once

// Stories asn-018 / asn-020 / asn-021 (production/epics/assassination-defence-reversal/): the one idempotent encounter
// presentation handler of every machine (ADR-0018 section 5.1, ADR-0017's evaluate-from-now rule).

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "View/EncounterPresentationCore.h"
#include "View/EncounterViewSubsystem.h"

#include "EncounterPresentation.generated.h"

class AStealthPawn;
class UAnimMontage;

/**
 * The engine half of the presentation (montages, warp, collision, visibility). Production: FEncounterMontagePlayer over the
 * pawn's mesh and UEncounterPresentationSettings; tests inject a recorder.
 */
class IEncounterClipPlayer
{
public:
	virtual ~IEncounterClipPlayer() = default;

	/** Starts Plan.Clip at Plan.Position / Plan.PlayRate with Plan.BlendInSec (a late start is a seek plus cross-fade). False when there is nothing to play. */
	virtual bool Play(AStealthPawn& Pawn, const FEncounterPresentationPlan& Plan) = 0;

	/** The playing position of Clip on Pawn, or a negative value when it is not playing. */
	virtual double GetPosition(const AStealthPawn& Pawn, EEncounterClip Clip) const = 0;

	/** Seeks the playing Clip (never a restart) and sets its rate. */
	virtual void Seek(AStealthPawn& Pawn, EEncounterClip Clip, double Position, double PlayRate) = 0;

	/** A strike lunge's local warp (the "StrikeContact" target the pawn set, faced to the bearing, the window to contact). */
	virtual void BeginStrikeWarp(AStealthPawn& Pawn, const FEncounterPresentationPlan& Plan, const FEncounterPresentationParams& Params) = 0;

	/** Pawn collision off on this machine (the body blocks no walking pawn after its fall). */
	virtual void SetPawnCollisionOff(AStealthPawn& Pawn) = 0;

	/** Hides the body on this machine in one frame (no fade, dissolve, sink or effect). */
	virtual void HideBody(AStealthPawn& Pawn) = 0;
};

/** What the local viewer sees of a body (the VA-ASN-2 geometry test; pawns are not occluders). */
class IEncounterBodyViewQuery
{
public:
	virtual ~IEncounterBodyViewQuery() = default;

	/** False on a machine with no local viewer (the removal rule then never hides: the server removes at + 9.2 s). */
	virtual bool HasViewer(const UWorld& World) const = 0;

	/** Out: any of the body's bounds inside the viewer's frustum, and every in-frustum sample blocked by world geometry. */
	virtual void Query(const AStealthPawn& Body, bool& bOutInFrustum, bool& bOutFullyOccluded) const = 0;
};

/** The owning client's side of the presentation (role change, grade); production: UEncounterViewSubsystem. */
class IEncounterOwnerPresentation
{
public:
	virtual ~IEncounterOwnerPresentation() = default;

	/** True when Pawn is possessed by a local player on this machine. */
	virtual bool IsLocallyViewed(const AStealthPawn& Pawn) const = 0;

	/** Whether the local player's pawn was possessed as an informant (the InformantKilled reason). */
	virtual bool WasPossessedAsInformant(const AStealthPawn& Pawn) const = 0;

	virtual void DispatchRoleChange(const AStealthPawn& Pawn, ERoleChangeReason Reason, double ServerNow, double DeathServerTime) = 0;

	virtual void ApplyTransientGrade(const AStealthPawn& Pawn, ETransientGradeCause Cause, double ServerNow, double DueServerTime) = 0;
};

/** The presentation log's events (the timing hook story asn-017 / the evidence read). */
enum class EEncounterPresentationEvent : uint8
{
	ClipStart,
	ClipSeek,
	WarpBegin,
	CollisionOff,
	BodyHidden,
	RoleChange,
	Grade,
	UnsyncedFallback,
};

/** Log name of an event. */
STEALTHGAMEPLAY_API const TCHAR* LexEncounterPresentationEvent(EEncounterPresentationEvent Event);

/** One presentation log row. */
struct FEncounterPresentationLogRow
{
	EEncounterPresentationEvent Event = EEncounterPresentationEvent::ClipStart;
	/** The pawn as an opaque key (never dereferenced) and its name. */
	const AStealthPawn* PawnKey = nullptr;
	FString PawnName;
	EEncounterClip Clip = EEncounterClip::None;
	/** EstimatedServerNow() of this machine at the row. */
	double ServerNow = 0.0;
	/** Clip position for clip rows; the decision (EBodyRemovalDecision) for BodyHidden; the reason / cause for owner rows. */
	double Value = 0.0;
	/** The time the row answers: ContactServerTime / DeathServerTime / the due time. */
	double DueServerTime = -1.0;
	bool bLate = false;
};

/**
 * UEncounterPresentationSubsystem (ADR-0018 section 5.1): OnRep_ActionState, OnRep_DeathState, the server's direct write
 * (E7: the listen host has no RepNotify for its own writes) and the per-frame tick of a pawn with a scheduled clip or a body
 * in its sequence all call one state-derived Refresh(Pawn, EstimatedServerNow()):
 *  - Strike (story asn-018): the variant's clip, contact frame at ContactServerTime; late = sought and cross-faded
 *    (<= MaxCrossFadeSec), never snapped; the local "StrikeContact" warp; Counter at its ContactServerTime;
 *  - Fall (story asn-020): a reached DeathServerTime wins over any pose: the one Death_Fall (seeked when late), collision
 *    off at + DeathFallSec, then VA-ASN-2's removal (FBodyRemovalEvaluator) from + BodyLingerSec;
 *  - Own role (story asn-021): for the locally possessed pawn, OnRoleChangePresented once per FDeathState at its
 *    DeathServerTime (or on arrival when late) plus the victim's transient grade; a bound defender's grade at its Counter's
 *    ContactServerTime.
 * Refresh runs only from those calls and while a pawn's plan needs the tick (the control manifest guardrail).
 */
UCLASS()
class STEALTHGAMEPLAY_API UEncounterPresentationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UEncounterPresentationSubsystem();

	/** The subsystem of Pawn's world. */
	static UEncounterPresentationSubsystem* Get(const UWorld* World);

	/** This machine's EstimatedServerNow() (STAND-IN for Round story 091: the game state's server world time). */
	static double EstimatedServerNow(const UWorld* World);

	/** STAND-IN for IsServerClockSynced() (Round story 091): the server, or a client past UnsyncedClockGraceSec. */
	static bool IsServerClockSynced(const UWorld* World);

	/** The one idempotent handler (see the class comment). */
	void Refresh(AStealthPawn& Pawn, double Now);

	/** The public tuning the presentation reads (schema defaults until a client tuning store exists). */
	const FEncounterPresentationParams& GetParams() const { return Params; }
	const FEncounterClipTimings& GetClipTimings() const { return Timings; }

	/** The presentation log of this machine (bounded). */
	const TArray<FEncounterPresentationLogRow>& GetLog() const { return Log; }

	/** Rows of one pawn and event (observation for tests). */
	TArray<FEncounterPresentationLogRow> FindRows(const AStealthPawn* Pawn, EEncounterPresentationEvent Event) const;

	/** The last plan Refresh computed for Pawn (observation). */
	const FEncounterPresentationPlan* FindLastPlan(const AStealthPawn* Pawn) const;

	/** True while Pawn is in the per-frame schedule. */
	bool IsScheduled(const AStealthPawn* Pawn) const;

	/** Test seams: replace the engine half, the view query, the owner side, the clock and the params (null restores production). */
	void SetClipPlayer(IEncounterClipPlayer* Player);
	void SetBodyViewQuery(IEncounterBodyViewQuery* Query);
	void SetOwnerPresentation(IEncounterOwnerPresentation* Owner);
	void SetParams(const FEncounterPresentationParams& InParams) { Params = InParams; }
	void SetClockOverride(TFunction<double()> Now, TFunction<bool()> Synced);

	// UTickableWorldSubsystem
	/** Game and PIE worlds only (the editor's own world presents nothing). */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	struct FPawnState
	{
		uint16 SeenSeq = 0;
		double ActionReceiptNow = -1.0;
		bool bDeathSeen = false;
		double DeathReceiptNow = -1.0;
		EEncounterClip PlayingClip = EEncounterClip::None;
		uint32 PlayingKey = MAX_uint32;
		double PlayingRate = 0.0;
		bool bCollisionOff = false;
		bool bHidden = false;
		bool bRoleDispatched = false;
		uint16 CounterGradedSeq = 0;
		bool bLoggedUnsynced = false;
		FEncounterPresentationPlan LastPlan;
	};

	void HandleEncounterStateChanged(AStealthPawn* Pawn);
	void AddRow(EEncounterPresentationEvent Event, const AStealthPawn& Pawn, EEncounterClip Clip, double Now, double Value, double Due, bool bLate = false);
	double Now() const;
	bool ClockSynced() const;

	IEncounterClipPlayer& Player() const;
	IEncounterBodyViewQuery& ViewQuery() const;
	IEncounterOwnerPresentation& Owner() const;

	FEncounterPresentationParams Params;
	FEncounterClipTimings Timings;
	TMap<TWeakObjectPtr<AStealthPawn>, FPawnState> States;
	TSet<TWeakObjectPtr<AStealthPawn>> Scheduled;
	TArray<FEncounterPresentationLogRow> Log;
	FDelegateHandle StateChangedHandle;

	TUniquePtr<IEncounterClipPlayer> ProductionPlayer;
	TUniquePtr<IEncounterBodyViewQuery> ProductionViewQuery;
	TUniquePtr<IEncounterOwnerPresentation> ProductionOwner;
	IEncounterClipPlayer* PlayerOverride = nullptr;
	IEncounterBodyViewQuery* ViewQueryOverride = nullptr;
	IEncounterOwnerPresentation* OwnerOverride = nullptr;
	TFunction<double()> NowOverride;
	TFunction<bool()> SyncedOverride;
};
