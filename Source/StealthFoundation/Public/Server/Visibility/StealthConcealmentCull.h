#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Server/Visibility/StealthConcealmentCullSource.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/Function.h"

#include "StealthConcealmentCull.generated.h"

class AActor;
class APawn;
class UActorChannel;
class UNetConnection;
class UStealthConcealmentCull;

/** A pawn's cull record state (ADR-0003 section 5; IVP story 024). */
enum class EConcealCullState : uint8
{
	Visible,
	CullPending,
	Culled,
};

/** The cull tick: TG_PostUpdateWork, after the drain and the staging pass, before this frame's replication flush. */
struct FStealthConcealmentCullTick : public FTickFunction
{
	UStealthConcealmentCull* Owner = nullptr;
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& CompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override { return TEXT("StealthConcealmentCull tick"); }
};

/**
 * UStealthConcealmentCull: the Information Visibility Policy's concealment cull (ADR-0003 sections 5.1-5.6 with the S10
 * resolution S10-R2/R3; IVP stories 024/025; Concealment story con-025). The server-only implementation of
 * IConcealmentCullSource:
 *   - SetCullState(Pawn, true, t): CullPending, CullDeadline = the first point of the global pawn-update grid (multiples of
 *     1 / PawnNetUpdateHz from the round's Active entry, shared by every pawn) at or after t + ConcealCullDelayMs; every
 *     commit of one server frame lands on the same point; no client acknowledgement is waited for.
 *   - the tick moves due records to Culled and, in the same frame, closes the pawn's actor channel on every NON-ALLY
 *     connection (FindActorChannelRef, Close(Relevancy)); a player pawn stays relevant to its owner and every ally (informant
 *     allies included); an NPC pawn is culled for every connection. The relevancy hook (AStealthPawn::IsNetRelevantFor) then
 *     keeps returning false for those connections.
 *   - SetCullState(Pawn, false, t): Visible in the same server step (un-cull), so this frame's flush may re-open it; the S10
 *     re-open guard holds the re-open on a connection while that connection's close is not yet acknowledged (the closed
 *     channel still Closing in its slot), at most ReopenAckTimeoutMs (350), then releases it (the entry turns stale).
 *     A pending cull is simply cancelled.
 * Not built here (IVP stories): the S10-R4 lost-pawn repair and the R7.6 internal switch bConcealServerCull = false.
 *
 * Usage (server): Cull->ConfigureTeams(PawnTeamFn, ViewerTeamFn); Cull->SetGridAnchor(ActiveStart);
 *                 Cull->SetCullState(Pawn, true, Now);   // Concealment's commit
 *                 Cull->IsRelevantTo(Pawn, Viewer);       // AStealthPawn::IsNetRelevantFor
 */
UCLASS(NotBlueprintType, NotBlueprintable)
class STEALTHFOUNDATION_API UStealthConcealmentCull : public UWorldSubsystem, public IConcealmentCullSource
{
	GENERATED_BODY()

public:
	/** The S10-R3 acknowledgement timeout. */
	static constexpr double ReopenAckTimeoutSec = 0.35;

	/** Server game and PIE worlds only (never a client world). */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** The cull of World, or null (clients have none). */
	static UStealthConcealmentCull* Get(const UWorld* World);

	/**
	 * The relations the audience needs (the composition root binds the Roster): PawnTeam -1 for an unbound (NPC) pawn; ViewerTeam
	 * the team of a connection's viewer (its PlayerController), -1 when unknown. Ally = same team >= 0.
	 */
	void ConfigureTeams(TFunction<int32(const APawn*)> InPawnTeam, TFunction<int32(const AActor*)> InViewerTeam);

	/** The grid's origin (the round's Active entry) and its rate (PawnNetUpdateHz) and the cull delay (ConcealCullDelayMs). */
	void SetGridAnchor(double ActiveEntryServerTime) { GridAnchor = ActiveEntryServerTime; }
	void SetTiming(double InPawnNetUpdateHz, double InCullDelayMs) { PawnNetUpdateHz = InPawnNetUpdateHz; CullDelayMs = InCullDelayMs; }

	/** The first grid point at or after CommitServerTime + the cull delay (pure arithmetic; 1 ms due tolerance). */
	double ComputeDeadline(double CommitServerTime) const;

	// IConcealmentCullSource
	virtual void SetCullState(APawn* Pawn, bool bConcealed, double CommitServerTime) override;

	/** False while Pawn is Culled for this viewer (a non-ally), or its re-open to this viewer's connection is held. */
	bool IsRelevantTo(const APawn* Pawn, const AActor* RealViewer) const;

	/** The record of Pawn (Visible when none). */
	EConcealCullState GetState(const APawn* Pawn) const;
	double GetDeadline(const APawn* Pawn) const;

	/** The cull tick's body (also called directly by tests); Now is the world's server time. */
	void TickCull(double Now);

	/** Channels this cull closed since the world began (observation). */
	int32 GetClosedChannelCount() const { return ClosedChannels; }
	/** Re-opens held by the S10 guard that timed out (observation). */
	int32 GetReopenTimeoutCount() const { return ReopenTimeouts; }
	/** Server time of each close sent, per pawn (tests: the boundary the close landed on). */
	const TArray<TPair<TWeakObjectPtr<APawn>, double>>& GetCloseLog() const { return CloseLog; }

private:
	struct FRecord
	{
		EConcealCullState State = EConcealCullState::Visible;
		double CommitServerTime = 0.0;
		double CullDeadline = 0.0;
	};
	struct FPendingClose
	{
		TWeakObjectPtr<APawn> Pawn;
		TWeakObjectPtr<UNetConnection> Connection;
		const UActorChannel* ClosedChannel = nullptr;
		int32 ChIndex = INDEX_NONE;
		double SentServerTime = 0.0;
		bool bStale = false;
	};

	bool IsAlly(const APawn* Pawn, const AActor* Viewer) const;
	void CloseForNonAllies(APawn* Pawn, double Now);
	void UpdatePendingCloses(double Now);

	TMap<TWeakObjectPtr<APawn>, FRecord> Records;
	TArray<FPendingClose> PendingCloses;
	TArray<TPair<TWeakObjectPtr<APawn>, double>> CloseLog;
	TFunction<int32(const APawn*)> PawnTeam;
	TFunction<int32(const AActor*)> ViewerTeam;
	FStealthConcealmentCullTick CullTick;
	double GridAnchor = 0.0;
	double PawnNetUpdateHz = 15.0;
	double CullDelayMs = 250.0;
	int32 ClosedChannels = 0;
	int32 ReopenTimeouts = 0;
};
