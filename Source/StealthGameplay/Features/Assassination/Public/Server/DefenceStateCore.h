#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "View/StealthQueueTypes.h"

/**
 * The server-only per-pawn defence state (GDD assassination-defence-reversal.md ASN-R1, ASN-R3, ASN-R18, F3; ADR-0018
 * section 4.1; story asn-003). Plain C++: every time is an injected server time (the double server clock, ADR-0002) and
 * every order is a queue key, so it runs headless. Never an engine timer, never a feature tick: the brace window's cap
 * is a HoldTimer(CapEnd) queue item and the cooldown is a stamp compared at use.
 *
 * Two doubles of gameplay state (ADR-0018 guardrail) plus the window record:
 *   LastPrepareActivation  T_act, written when a Prepare(Begin) EXECUTES; the 60 s default cooldown is measured from it,
 *                          so a tapped, released-early or never-used brace burns the whole cooldown (F3; confirmed user
 *                          rule: the cooldown starts on activation, not on release). None at round start and after Reset.
 *   LockoutEnd             the attacker lockout end, t_exec + AttackAttemptLockoutSec (written by story asn-008).
 *   Window                 the protected window's Begin key and the first key that ended it (a stand-in for the IIF's hold
 *                          machinery, IIF stories 042-048: the debounce expiry or the HoldTimer(CapEnd) at
 *                          T_act + DefenceDurationSec, 4 s by default). F2's key compare reads it; story asn-009 adds the
 *                          Slot(V) = Bracing clause.
 *
 * Nothing here resets, extends or refunds the cooldown on any event (re-press, possession change, Interact, camera
 * toggle): the only writers are BeginPrepare (an executed Begin), AdoptForRecovery (ASN-R18 PawnRecovery) and Reset (round
 * reset, story asn-019). Disconnecting or handing the pawn to AI changes nothing here: no End arrives, so CapEnd closes
 * the window, and the lockout runs out at its stamped end.
 *
 * Usage (the Prepare slot resolver, server):
 *   FDefenceStateCore& State = UDefenceStateComponent::FindOrAdd(Pawn)->GetCore();
 *   if (State.BeginPrepare(Now, Tuning.DefenceCooldownSec, SlotKey) == EDefenceBeginResult::Executed)
 *   {
 *       Queue.ScheduleAt(State.GetCapEndServerTime(Tuning.DefenceDurationSec), CapEndItem);   // HoldTimer(CapEnd)
 *   }
 *   ... the CapEnd or debounce item's slot:  State.EndWindow(ItsKey);
 *   ... an Attack's slot:                   const bool bBraced = State.IsWindowOpenAt(AttackKey);  (+ Slot = Bracing, asn-009)
 */

/** What a Prepare(Begin) did at its slot. */
enum class EDefenceBeginResult : uint8
{
	/** Executed: T_act stamped now and the window opened at the Begin's key. */
	Executed,
	/** The cooldown is running (F3 CanPrepare false): nothing changed; the reply is Reject(Cooldown). */
	Cooldown,
};

/** The protected window of the latest executed Prepare(Begin), by queue key. */
struct FDefenceWindow
{
	/** Key of the executed Begin; unset when no brace has executed since the last Reset. */
	FQueueKey BeginKey;
	/** Key of the first item that ended the window (debounce expiry or CapEnd); unset while it is open. */
	FQueueKey EndKey;
};

/** The defence state of one pawn, identical for every pawn kind. */
class STEALTHGAMEPLAY_API FDefenceStateCore
{
public:
	// ---- Cooldown (F3) -----------------------------------------------------------------------------------------------

	/** F3 CanPrepare(t) = (T_act = none) OR (t - T_act >= C). Inclusive boundary. */
	bool CanPrepare(double NowServerTime, double CooldownSec) const;

	/** Seconds until CanPrepare turns true (0 when it already is). */
	double CooldownRemaining(double NowServerTime, double CooldownSec) const;

	/** T_act, or unset (none). */
	TOptional<double> GetLastPrepareActivation() const { return LastPrepareActivation; }

	/** T_act + C, the owner-only cooldown end (story asn-016 publishes it), or unset when no brace ever executed. */
	TOptional<double> GetCooldownEndServerTime(double CooldownSec) const;

	/**
	 * A Prepare(Begin) executing at its slot: when CanPrepare, stamps T_act = Now and opens the window at BeginKey;
	 * otherwise changes nothing and returns Cooldown. A press while a window is live always lands here as Cooldown (the
	 * window cap is far shorter than the cooldown), so presses never move the stamp, the window or its CapEnd.
	 */
	EDefenceBeginResult BeginPrepare(double NowServerTime, double CooldownSec, FQueueKey BeginKey);

	/** T_act + DefenceDurationSec, the due time of the window's HoldTimer(CapEnd); unset when no brace executed. */
	TOptional<double> GetCapEndServerTime(double DurationSec) const;

	// ---- Protected window (F2 key compare; IIF stand-in) --------------------------------------------------------------

	/** The item keyed EndKey ended the window (debounce expiry or CapEnd). Only the first end counts; later ones are no-ops. */
	void EndWindow(FQueueKey EndKey);

	/** F2's key clauses: Key(Begin) < k_A and not Key(End) < k_A. False when no brace executed. */
	bool IsWindowOpenAt(FQueueKey AttackKey) const;

	/** The window record (read-only). */
	const FDefenceWindow& GetWindow() const { return Window; }

	// ---- Attacker lockout (written by story asn-008) ------------------------------------------------------------------

	/** An Attack by this pawn executed at ExecServerTime: LockoutEnd = ExecServerTime + LockoutSec. */
	void BeginLockout(double ExecServerTime, double LockoutSec);

	/** True while Now < LockoutEnd (F3 Slot = Striking for t - t_exec < Lambda). */
	bool IsLockedOut(double NowServerTime) const { return NowServerTime < LockoutEnd; }

	/** The lockout end (-infinity before the first Attack). */
	double GetLockoutEnd() const { return LockoutEnd; }

	// ---- Lifecycle (ASN-R18) -------------------------------------------------------------------------------------------

	/**
	 * A PawnRecovery re-pawn of an Active player: copies the lost pawn's stamp, or stamps Now when it is unknown (Lost null
	 * or never braced), so a recovery is never a free brace. The window and the lockout are not carried: the lost body's
	 * window ended with it.
	 */
	void AdoptForRecovery(const FDefenceStateCore* LostPawnState, double NowServerTime);

	/** Round Reset: clears the stamp, the window and the lockout (story asn-019 asserts it). */
	void Reset();

private:
	TOptional<double> LastPrepareActivation;
	double LockoutEnd = -TNumericLimits<double>::Max();
	FDefenceWindow Window;
};
