#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

struct FCrowdTuning_Server;

/**
 * The ambient emergence schedule and the kill ledger (GDD CNB-R9, F3; ADR-0016 section 6; stories cnb-026, cnb-027).
 * Plain C++, server-only. It never reads who died, who is watching or what a pawn is: its inputs are times, socket
 * indices and the counter-based schedule draws (CNB-R14: each schedule event draws from its own
 * GetServerSubSeed(AmbientSchedule, counter) stream).
 */

/** A TimerFired(CrowdSchedule) item's kind. */
enum class ECrowdScheduleKind : uint8
{
	/** An exit slot: one eligible NPC near the socket takes "leave by the alley" as its next leg. */
	Exit,
	/** An entry: a pawn emerges from the socket (a cycle's partner, or a kill's fallback replacement). */
	Entry,
};

/** How the round's queue resolved a schedule item. */
enum class ECrowdScheduleResult : uint8
{
	Fired,
	/** The round latched (Ended): never rescheduled. */
	RejectedLatched,
	/** The item carried an old round epoch: never rescheduled. */
	RejectedStaleEpoch,
	/** A kill cancelled the exit (the item was voided at revalidation): never rescheduled. */
	VoidedRevalidation,
};

/** One schedule item (the payload of a TimerFired(CrowdSchedule) queue item; never an FTimerManager timer). */
struct FCrowdScheduleItem
{
	uint32 Id = 0;
	ECrowdScheduleKind Kind = ECrowdScheduleKind::Exit;
	double Due = 0.0;
	int32 Socket = INDEX_NONE;
	/** The schedule-event counter whose stream drew this item. */
	uint32 Counter = 0;
	/** The round epoch token the item was submitted with. */
	uint32 Epoch = 0;
	/** The exit cycle (exit and its entry share it); INDEX_NONE for a fallback entry. */
	int32 Cycle = INDEX_NONE;
	/** A fallback entry serves this kill vacancy (INDEX_NONE otherwise). */
	int32 Vacancy = INDEX_NONE;
	/** How many times a blocked entry was retried. */
	int32 Retries = 0;
};

/**
 * Where schedule items go: the round's resolution queue as TimerFired(CrowdSchedule) items with the round epoch token
 * (ADR-0002); the queue later calls FCrowdAmbientSchedule::Resolve. The Director's local implementation stands in until
 * the composition root wires the round queue; specs use a recording fake.
 */
class ICrowdScheduleQueue
{
public:
	virtual ~ICrowdScheduleQueue() = default;
	virtual void Submit(const FCrowdScheduleItem& Item) = 0;
	/** Revalidation voids the item (it resolves VoidedRevalidation). */
	virtual void Void(uint32 ItemId) = 0;
};

/** What the schedule asks of the world (the Level Socket Contract, the pawn factory, the crowd agents). */
class ICrowdScheduleHost
{
public:
	virtual ~ICrowdScheduleHost() = default;
	/**
	 * Gives the exit slot to one eligible unbound NPC within 40 m of the socket path, chosen uniformly with PickU in
	 * [0, 1) (a released exile body waiting for a slot takes it first). False when none is eligible (the slot passes).
	 */
	virtual bool StartAmbientExit(int32 Socket, int32 Cycle, double PickU, double Now) = 0;
	/** LSC R5's clear check for an emergence at the socket (unwitnessed, outside its cooldown). */
	virtual bool IsSocketClear(int32 Socket, double Now) = 0;
	/** LSC RequestEmergence at the socket: the factory spawns, CPA DrawReplacement picks the card. */
	virtual bool RequestEmergence(int32 Socket, double Now) = 0;
};

/** One vacancy a kill opened (CNB-R9.2): only its time is known. */
struct FCrowdKillVacancy
{
	double KillTime = 0.0;
	double ServedTime = -1.0;
	/** Served by the fallback (no scheduled entry by CrowdReplacementMaxDelaySec). */
	bool bFallback = false;
	/** The exit this kill cancelled (an item id), or 0 when none was cancelled. */
	uint32 CancelledExit = 0;
	/** The victim was an exiting NPC inside an alley: a vacancy without a cancel. */
	bool bInAlley = false;
};

/**
 * FCrowdAmbientSchedule: exit-first ambient cycles as a Poisson process (CNB-R9.1): exits at one per
 * CrowdAmbientCycleIntervalSec per socket, city-wide exponential gaps of mean CrowdAmbientCycleIntervalSec / N_E, each
 * slot given to a socket drawn uniformly; the matching entry G ~ U(CrowdCycleGapMinSec, MaxSec) after the absorption. A
 * blocked entry retries at +EmergenceRetrySec. A kill (CNB-R9.2) adds a vacancy and cancels one exit chosen uniformly
 * among the exits scheduled in the next 60 s (LSC R5.7: at most EmergenceExitCancelMaxPct of those exits); its
 * replacement is the first entry at or after t_k + CrowdReplacementMinDelaySec, or a fallback entry at the next clear
 * socket once CrowdReplacementMaxDelaySec has passed. Halt stops it (open cycles are abandoned without emergence).
 */
class STEALTHGAMEPLAY_API FCrowdAmbientSchedule
{
public:
	/** The kill-cancel window (s) and the exit horizon kept scheduled ahead (s). */
	static constexpr double CancelWindowSec = 60.0;
	static constexpr double HorizonSec = 90.0;

	/**
	 * Starts the schedule at Now (OnRoundActive) over SocketCount sockets. SubSeedOf(Counter) is
	 * GetServerSubSeed(AmbientSchedule, Counter) of the round. Nothing is scheduled with no socket.
	 */
	void Start(double Now, int32 SocketCount, const FCrowdTuning_Server& Tuning, uint32 Epoch, ICrowdScheduleQueue& Queue, TFunction<uint64(uint32)> SubSeedOf);
	/** Keeps exits scheduled HorizonSec ahead and schedules due kill fallbacks. Call once a frame. */
	void Tick(double Now);
	/** The queue resolved an item (in its slot). */
	void Resolve(uint32 ItemId, ECrowdScheduleResult Result, double Now, ICrowdScheduleHost& Host);
	/** The exiting NPC of Cycle was absorbed at Now: its entry is scheduled after the cycle gap. */
	void NoteAbsorbed(int32 Cycle, double Now);
	/** The exiting NPC of Cycle gave up (watched) or was taken out of the cycle: no vacancy, no entry. */
	void NoteCycleAbandoned(int32 Cycle);
	/**
	 * A kill at KillTime (the scheduled death-write time; any victim). bExitingInAlley: the victim was the exiting NPC of
	 * an open cycle inside its alley (a vacancy without a cancel; that cycle closes). Returns the vacancy index.
	 */
	int32 NoteKill(double KillTime, bool bExitingInAlley, int32 VictimCycle = INDEX_NONE);
	/** Halt (CNB-R12): no further item; open cycles abandoned without emergence. */
	void Halt();

	bool IsRunning() const { return bRunning; }
	TConstArrayView<FCrowdKillVacancy> GetVacancies() const { return Vacancies; }
	/** Exits fired (slots given to an NPC or passed), per socket; entries emerged, per socket. */
	TConstArrayView<int32> GetExitsPerSocket() const { return ExitsPerSocket; }
	TConstArrayView<int32> GetEntriesPerSocket() const { return EntriesPerSocket; }
	/** Every exit item's due time in order (the Poisson gaps are their differences). */
	TConstArrayView<double> GetExitTimes() const { return ExitTimes; }
	int32 GetCancelledExits() const { return CancelledExits; }
	int32 GetCancelsRefusedByBound() const { return CancelsRefusedByBound; }
	int32 GetRetries() const { return RetryCount; }
	/** Items resolved Rejected_* and therefore dropped (never rescheduled). */
	int32 GetRejectedItems() const { return RejectedItems; }
	int32 GetOpenCycles() const { return OpenCycles.Num(); }
	/** The due time of the exit item ItemId (negative when unknown). */
	double GetExitDueOf(uint32 ItemId) const;
	/** The socket each cancelled exit was scheduled at (the AC-CNB-35 uniformity tabulation). */
	TConstArrayView<int32> GetCancelledSockets() const { return CancelledSockets; }

private:
	struct FExitSlot
	{
		uint32 ItemId = 0;
		double Due = 0.0;
		int32 Socket = INDEX_NONE;
		bool bCancelled = false;
		bool bFired = false;
	};
	double Unit(uint32 Counter, uint32 Index) const;
	void ScheduleNextExit();
	void SubmitEntry(double Due, int32 Socket, uint32 Counter, int32 Cycle, int32 Vacancy, int32 Retries);
	/** The oldest kill vacancy an entry at Now may serve (t_k + d_min <= Now), or INDEX_NONE. */
	int32 ServeableVacancy(double Now) const;

	ICrowdScheduleQueue* Queue = nullptr;
	TFunction<uint64(uint32)> SubSeedOf;
	bool bRunning = false;
	int32 Sockets = 0;
	double Lambda = 0.0;
	double GapMin = 6.0, GapMax = 20.0, MinDelay = 10.0, MaxDelay = 60.0, RetrySec = 1.0, CancelMaxPct = 25.0;
	uint32 Epoch = 0;
	uint32 NextItemId = 1;
	uint32 NextCounter = 0;
	uint32 NextKillCounter = 0;
	double LastExitDue = 0.0;
	TArray<FExitSlot> Exits;
	TMap<uint32, FCrowdScheduleItem> Items;
	/** Open cycles: cycle -> (schedule counter, socket). */
	TMap<int32, TPair<uint32, int32>> OpenCycles;
	TSet<int32> FallbackScheduled;
	TArray<FCrowdKillVacancy> Vacancies;
	TArray<int32> ExitsPerSocket;
	TArray<int32> EntriesPerSocket;
	TArray<double> ExitTimes;
	TArray<int32> CancelledSockets;
	int32 CancelledExits = 0;
	int32 CancelsRefusedByBound = 0;
	int32 RetryCount = 0;
	int32 RejectedItems = 0;
	int32 NextCycle = 0;
};
