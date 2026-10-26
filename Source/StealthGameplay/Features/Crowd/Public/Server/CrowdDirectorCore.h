#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/CrowdDraws.h"
#include "Server/CrowdTypes.h"
#include "Server/StealthTuningSubstructs.h"

/**
 * The first activity a pawn starts the round with (CNB-R12): a pause of PauseSec, or a leg of LegCm. Drawn per pawn
 * from its own stream (ECrowdDecision::FirstActivity, PauseKind, PauseLength, LegLength), so the crowd never starts in
 * unison.
 */
struct FCrowdFirstActivity
{
	bool bPause = false;
	double PauseSec = 0.0;
	double LegCm = 0.0;
};

/** The Director's record of one driven pawn (keyed by the factory's spawn ordinal; never by appearance or kind). */
struct FCrowdAgentRecord
{
	uint32 Ordinal = 0;
	ECrowdDriverKind Driver = ECrowdDriverKind::Full;
	/** The activity mirror (written by state enter/exit tasks, ADR-0016 section 3). */
	ECrowdActivity Activity = ECrowdActivity::Dormant;
	/** The decision-grid phase, seconds in [0, 1 / CrowdDecisionPerSec), relative to GridOrigin. */
	double GridPhase = 0.0;
	/** True once the pawn was started (round start, or joined an active round). */
	bool bStarted = false;
};

/** What the Director core calls when it starts a pawn (the controller in the game, a fake in specs). */
class ICrowdAgentStarter
{
public:
	virtual ~ICrowdAgentStarter() = default;
	/** Starts the pawn's behaviour at Now with its drawn first activity; it issues its first command in this call. */
	virtual void StartAgent(uint32 Ordinal, const FCrowdFirstActivity& First, double Now) = 0;
};

/**
 * FCrowdDirectorCore: the plain C++ core of the server-only Crowd Director (ADR-0016 section 1, ADR-0007 testable
 * core; story cnb-004). No UWorld: specs construct it headless. It owns
 *   - the frozen Crowd tuning (a copy taken at construction; never written afterwards, AC-CNB-37),
 *   - the per-ordinal counter-based draws (FCrowdDraws, CNB-R14),
 *   - the agent registry and activity mirror,
 *   - the phase gate (Dormant until StartRound, NavNotReady keeps everyone Dormant, Halt latch),
 *   - the decision grid t = GridOrigin + Phase + k / CrowdDecisionPerSec (phases stratified over the roster so the
 *     decisions spread evenly across frames, AC-CNB-33b),
 *   - the population ledger NpcCount - V - B + Rel (CNB-R1), which records kill times only.
 * Later stories add the spatial hash, path and ray budgets, stall regulator and tell counters beside it.
 */
class STEALTHGAMEPLAY_API FCrowdDirectorCore
{
public:
	/** Tuning: the frozen Crowd snapshot. BehaviourSubSeed: GetServerSubSeed(CrowdBehaviour, LayoutAttempt). */
	FCrowdDirectorCore(const FCrowdTuning_Server& Tuning, uint64 BehaviourSubSeed);

	// ---- Tuning and draws ------------------------------------------------------------------------------------------

	/** The frozen Crowd tuning (const: no Crowd code writes a Crowd value after OnTuningReady). */
	const FCrowdTuning_Server& GetTuning() const { return Tuning; }

	FCrowdDraws& GetDraws() { return Draws; }
	const FCrowdDraws& GetDraws() const { return Draws; }

	// ---- Registry --------------------------------------------------------------------------------------------------

	/** Registers a driven pawn. A pawn registered while Active is started at once by the caller (JoinActiveRound). */
	FCrowdAgentRecord& RegisterAgent(uint32 Ordinal, ECrowdDriverKind Driver);
	void UnregisterAgent(uint32 Ordinal);
	FCrowdAgentRecord* FindAgent(uint32 Ordinal);
	const FCrowdAgentRecord* FindAgent(uint32 Ordinal) const;
	int32 NumAgents() const { return Agents.Num(); }
	/** Ordinals in registration-independent (ascending) order. */
	TArray<uint32> GetOrdinals() const;

	// ---- Phase gate (CNB-R12) --------------------------------------------------------------------------------------

	ECrowdDirectorPhase GetPhase() const { return Phase; }
	bool IsActive() const { return Phase == ECrowdDirectorPhase::Active; }
	bool IsHalted() const { return Phase == ECrowdDirectorPhase::Halted; }

	/**
	 * OnRoundActive (inside Round's Active-entry step): assigns every registered pawn its grid phase and first activity
	 * and starts all of them through Starter in this one call. With bNavReady false nothing starts: the core records
	 * NavNotReady and stays Dormant (a development failure). Returns true when the crowd started.
	 */
	bool StartRound(double Now, bool bNavReady, ICrowdAgentStarter& Starter);

	/** A pawn joining an Active round (replacement, adoption): its own phase and first activity, started now. */
	void JoinActiveRound(uint32 Ordinal, double Now, ICrowdAgentStarter& Starter);

	/** True when StartRound was refused because the navmesh was not ready. */
	bool IsNavNotReady() const { return bNavNotReady; }

	/** The round-end Halt latch (terminal); every activity mirror reads Halted from here on. */
	void Halt();

	// ---- Decision grid (ADR-0016 section 3) --------------------------------------------------------------------------

	/** Seconds between decisions (1 / CrowdDecisionPerSec). */
	double GetDecisionPeriod() const { return DecisionPeriod; }

	/** The first grid point of Ordinal strictly after Now (Now itself when Now is exactly on its grid, bInclusive). */
	double NextGridPoint(uint32 Ordinal, double Now, bool bInclusive = false) const;

	/** The per-frame decision bound of AC-CNB-33b: ceil(N / (Fps / CrowdDecisionPerSec)) + 2. */
	int32 DecisionBoundPerFrame(double Fps) const;

	// ---- First activity (CNB-R12) ----------------------------------------------------------------------------------

	/**
	 * The first activity distribution: a pause with probability FirstPauseShare (the stationary share of F4, about
	 * 0.26), its length from the C2 mix (short U(CrowdShortPauseMin, Max), long U(CrowdIdleMin, Max) by
	 * CrowdLongPausePct), else a leg of U(CrowdLegMinCm, CrowdLegMaxCm).
	 */
	FCrowdFirstActivity DrawFirstActivity(uint32 Ordinal);

	/** The share of pawns that start the round paused (E[P] / (W + E[P]) at the defaults, F4). */
	double GetFirstPauseShare() const;

	// ---- Activity mirror -------------------------------------------------------------------------------------------

	void SetActivity(uint32 Ordinal, ECrowdActivity Activity);
	ECrowdActivity GetActivity(uint32 Ordinal) const;

	// ---- Population ledger (CNB-R1) --------------------------------------------------------------------------------

	/** A kill at KillServerTime: one vacancy (whoever died; the time is the only input). */
	void NoteKill(double KillServerTime);
	/** An ambient exit absorbed a pawn (one in-flight vacancy until its entry). */
	void NoteAmbientExit();
	/** An entry or replacement served one vacancy. */
	void NoteVacancyServed();
	/** An informant took over an NPC pawn (B + 1). */
	void NoteTakeover();
	/** A TryReleaseNpc reversed before binding (B - 1). */
	void NoteTakeoverReversed();
	/** A participant pawn was released to the crowd (Rel + 1). */
	void NoteRelease();

	int32 GetVacancies() const { return Vacancies; }
	int32 GetTakenOver() const { return TakenOver; }
	int32 GetReleased() const { return Released; }
	/** NpcCount - V - B + Rel. */
	int32 GetLiveNpcCount() const { return Tuning.NpcCount - Vacancies - TakenOver + Released; }
	/** The kill times the ledger recorded (it records nothing else about a kill). */
	TConstArrayView<double> GetKillTimes() const { return KillTimes; }

	/** Time the round started (the grid origin), or a negative value before StartRound. */
	double GetGridOrigin() const { return GridOrigin; }

private:
	void AssignStratifiedPhases();

	const FCrowdTuning_Server Tuning;
	FCrowdDraws Draws;
	TMap<uint32, FCrowdAgentRecord> Agents;
	ECrowdDirectorPhase Phase = ECrowdDirectorPhase::Dormant;
	bool bNavNotReady = false;
	double DecisionPeriod = 0.25;
	double GridOrigin = -1.0;
	int32 Vacancies = 0;
	int32 TakenOver = 0;
	int32 Released = 0;
	TArray<double> KillTimes;
};
