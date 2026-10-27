#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/EncounterTypes.h"
#include "UObject/ObjectKey.h"

/**
 * The server-only per-pawn strike-role table (GDD States and Transitions; ADR-0018 section 4.1; stories asn-005, 007, 008,
 * 010). Plain C++, keyed by the pawn's object key (a destroyed pawn or a reused object slot never matches).
 *
 * Expiry is evaluated at read time from stamped end times on the queue clock, never by an engine timer or a feature
 * tick (decision logged 2026-10-27): Striking ends at the attacker's LockoutEnd, Countering at CounterEnd; after either
 * the role reads Cooling while the defence cooldown runs, else Ready. Bracing ends only when its window-ending item
 * resolves (EndBrace), so F2's key compare and this role always agree. Dead is permanent until Forget (body removal,
 * story asn-019) or Reset (round reset).
 *
 * The role becomes Dead in the kill slot, before the death write lands: that is what excludes a body whose death write
 * is still pending from every candidate set (ASN-R8, F4). GetScheduledDeathServerTime is valid from that moment, so
 * the OnPawnKilled fan-out and the Roster flush can read it (Penalty, ADR-0019).
 */
class STEALTHGAMEPLAY_API FStrikeRoleTable
{
public:
	/** The role at Now; Cooling vs Ready reads the pawn's defence cooldown (null state: never braced). */
	EStrikeRole GetRole(const APawn* Pawn, double NowServerTime, const FDefenceStateCore* Defence, double CooldownSec) const;

	/** Prepare(Begin) executed: Bracing, remembering the Prepare's reply sink (the defender's FDefenceResult target). */
	void SetBracing(const APawn* Pawn, FReplySinkId BraceSink);
	/** The window ended (debounce, CapEnd, own Attack): Bracing -> Cooling. No-op in any other role. */
	void EndBrace(const APawn* Pawn);
	/** An Attack executed: Striking until LockoutEnd. */
	void SetStriking(const APawn* Pawn, double LockoutEndServerTime);
	/** A reversal by this pawn: Countering until CounterEnd. */
	void SetCountering(const APawn* Pawn, double CounterEndServerTime);
	/** Killed or reversed in this slot; its death (and fall start) at DeathServerTime. */
	void SetDead(const APawn* Pawn, double DeathServerTime);

	bool IsDead(const APawn* Pawn) const;
	/** The death time recorded in the kill slot (DeathServerTime = ContactServerTime), unset when not Dead. */
	TOptional<double> GetScheduledDeathServerTime(const APawn* Pawn) const;
	/** The Prepare sink recorded at the last executed Begin (index 0: none or the null sink). */
	FReplySinkId GetBraceSink(const APawn* Pawn) const;
	/** The stamped end of a Striking or Countering role (0 when none). */
	double GetRoleEnd(const APawn* Pawn) const;

	/** Body removal (story asn-019): the pawn leaves the table. */
	void Forget(const APawn* Pawn) { Entries.Remove(FObjectKey(Pawn)); }
	/** Round Reset: every role returns to Ready. */
	void Reset() { Entries.Reset(); }
	int32 Num() const { return Entries.Num(); }

private:
	struct FEntry
	{
		EStrikeRole Role = EStrikeRole::Ready;
		double RoleEndServerTime = 0.0;
		double DeathServerTime = -1.0;
		FReplySinkId BraceSink;
	};

	const FEntry* Find(const APawn* Pawn) const { return Entries.Find(FObjectKey(Pawn)); }
	FEntry& FindOrAdd(const APawn* Pawn) { return Entries.FindOrAdd(FObjectKey(Pawn)); }

	TMap<FObjectKey, FEntry> Entries;
};

/**
 * F2 braced-at-the-Attack (ADR-0018, GDD F2; story asn-009): Braced(V, k_A) = [Key(Begin_V) < k_A] AND NOT
 * [Key(End_V) < k_A] AND (Slot(V) = Bracing), from key order only (no wall-clock compare inside a drain). Key(End_V) is the
 * first of DebounceExpiry and CapEnd (FDefenceStateCore::EndWindow keeps only the first). Countering, Striking and Dead
 * are never braced. O(1).
 */
class STEALTHGAMEPLAY_API FBracedAtAttackCore
{
public:
	static bool IsBraced(EStrikeRole RoleAtSlot, const FDefenceStateCore* Defence, FQueueKey AttackKey);
};
