#pragma once

#include "CoreMinimal.h"
#include "Server/ObjectivesSeams.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AStealthGameMode;
class AStealthObjective;
class IStealthClock;
class URosterStore;
class URoundMatchStore;
class UStealthCompositionRoot;
class UStealthMatchAdapters;
class UWorld;

/**
 * The composition root's Objectives adapters (Objectives Chain stories oc-005..oc-009; ADR-0020 Key Interfaces): one object
 * implementing every seam UObjectivesService needs, over the current server world.
 *
 *   IObjectivesRosterView        URosterStore: GetLegalityClass (unbound -> Active), GetParticipant (the server-only binding
 *                                lookup), the record's State and Team.
 *   IObjectivesSessionView       UStealthMatchAdapters::GetConnectionState (Session's state table stand-in, Round's gateway).
 *   IObjectivesPawnControlView   UStealthPawnFactory::GetControlEpoch / IsHandOverPending (ADR-0010 section 4.4).
 *   IRoundObjectiveHooks         URoundMatchStore: phase Active, the queue's latch and CurrentKey, FRoundMatchCore::
 *                                DeclareObjectiveWin, the Round Event Log writer, the command clock.
 *   IObjectivesAssignmentSource  STAND-IN for the Level Socket Contract's UStealthLayoutService::GetSiteAssignment_Server
 *   IObjectivesCourtGeometry     (ADR-0014 section 7.4; the LSC epic is not built): L_Prototype's five AStealthObjective actors
 *                                are the courts. A court's id is the 1-based index of the world's objective actors sorted by
 *                                name; the actor's (OwningTeam, Stage) is its role (stage 1 team T -> P1[T], stage 2 -> P2[T],
 *                                the stage-3 shared actor -> Final), read at OnLayoutReady after the prototype layout moved
 *                                them; the four fixture faces stand FaceDistanceCm from the actor on +X, +Y, -X, -Y. The actors
 *                                keep only their placement: no objective gameplay runs on them on the production path.
 *   FindCourtInReach             STAND-IN for the IIF's prompt target: the nearest court within UStealthSettings
 *                                ObjectiveInteractRange of the pawn (the human client sends no fixture id yet).
 *   IObjectivesViewTargets       story oc-011: a team's members are the Roster records of that team (Active and Informant
 *                                alike); a member's view is the UObjectiveTeamViewComponent of its current controller (the
 *                                composition root's controller map, Session's connection table stand-in). The prototype
 *                                HUD's replicated AStealthGameState::TeamStage is no longer written on the production path.
 *   IObjectivesRivalRelease      STAND-IN until stories oc-014 / oc-015: no release is scheduled (RivalPrelimCount stays 0).
 */
class FStealthObjectivesAdapters final : public IObjectivesRosterView, public IObjectivesSessionView, public IObjectivesPawnControlView,
	public IRoundObjectiveHooks, public IObjectivesAssignmentSource, public IObjectivesCourtGeometry, public IObjectivesViewTargets,
	public IObjectivesRivalRelease
{
public:
	/** The face stand points' distance from a prototype objective actor (the LSC fixture's 1.2 m clear floor per face). */
	static constexpr double FaceDistanceCm = 120.0;

	FStealthObjectivesAdapters(UWorld* InWorld, UStealthCompositionRoot* InRoot, const IStealthClock* InClock);

	// IObjectivesRosterView
	virtual EObjectivesLegalityClass GetLegalityClass(const APawn& Pawn) const override;
	virtual FParticipantId GetParticipant(const APawn& Pawn) const override;
	virtual EParticipantState GetState(FParticipantId Participant) const override;
	virtual int32 GetTeam(FParticipantId Participant) const override;

	// IObjectivesSessionView
	virtual EObjectivesConnectionState GetConnectionState(FParticipantId Participant) const override;

	// IObjectivesPawnControlView
	virtual uint32 GetControlEpoch(const APawn& Pawn) const override;
	virtual bool IsHandOverPending(const APawn& Pawn) const override;

	// IRoundObjectiveHooks
	virtual bool IsActive() const override;
	virtual bool IsLatched() const override;
	virtual bool DeclareObjectiveWin(uint8 Team, FQueueKey Key) override;
	virtual void AppendEvent(const FRoundLogEntry& Entry) override;
	virtual FQueueKey CurrentKey() const override;
	virtual double ServerNow() const override;

	// IObjectivesAssignmentSource / IObjectivesCourtGeometry (the L_Prototype stand-in)
	virtual bool GetSiteAssignment_Server(FSiteAssignment& OutAssignment) const override;
	virtual bool GetFixtureFaces(FCourtFixtureId Court, TArray<FVector, TInlineAllocator<4>>& OutFaces) const override;

	// IObjectivesViewTargets (story oc-011)
	virtual void GetTeamMembers(uint8 Team, TArray<FParticipantId>& OutMembers) const override;
	virtual UObjectiveTeamViewComponent* FindTeamView(FParticipantId Participant) const override;

	// IObjectivesRivalRelease (stand-in)
	virtual void ScheduleRivalRelease(uint8 CompletingTeam, uint32 PrelimOrdinal) override {}

	/** The nearest court within the interact range of Pawn, or an unset id (the IIF target stand-in). */
	FCourtFixtureId FindCourtInReach(const APawn& Pawn) const;

	/** The court actor of an id (the stand-in courts; dev tools and the bot driver), or null. */
	const AStealthObjective* FindCourtActor(FCourtFixtureId Court) const;

	/** True while Pawn's legality class is Active (the hold stand-in's Illegal cancel). */
	bool IsLegalityActive(const APawn& Pawn) const;

private:
	void GetCourts(TArray<const AStealthObjective*>& Out) const;
	URosterStore* GetRoster() const;
	URoundMatchStore* GetRound() const;

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<UStealthCompositionRoot> Root;
	const IStealthClock* Clock = nullptr;
};
