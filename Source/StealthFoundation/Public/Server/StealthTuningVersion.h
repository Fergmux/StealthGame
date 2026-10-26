#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

/**
 * The tuning schema version (GDD F-2 SchemaVersion; ADR-0004 section 2; story tdc-043).
 *
 * TuningSchemaVersion is hand-maintained and is the value Session's join gate reads (through
 * UStealthTuningStore::GetSchemaVersion, later story) and the F-2 hash's first field. It must be
 * bumped when a key is added, removed, renamed or re-typed, when a key's audience or latch class
 * changes, or when an enum's declared values change: exactly what the generated schema fingerprint
 * covers.
 *
 * The bump rule is mechanical: StealthGame.Tuning.SchemaVersionBumped regenerates the fingerprint and
 * fails unless TuningSchemaHistory holds an entry for TuningSchemaVersion with that fingerprint, and
 * fails when two versions share one fingerprint. To bump: regenerate the table (Stealth.Tuning.SchemaGen),
 * increase TuningSchemaVersion by one and APPEND {new version, new fingerprint} below. Never edit or
 * remove an old entry (append-only; removal is not detected by the test, a known limit).
 */
inline constexpr int32 TuningSchemaVersion = 2;

/** One bumped schema version and the fingerprint it was bumped for. */
struct FTuningSchemaHistoryEntry
{
	int32 Version;
	uint64 Fingerprint;
};

/** Append-only history of {Version, Fingerprint}, one entry per bumped version, oldest first. */
inline constexpr FTuningSchemaHistoryEntry TuningSchemaHistory[] = {
	{ 1, 0x4250230bdd621e89ull }, // 2026-10-25, story tdc-043: the eight canonical keys of story tdc-040
	{ 2, 0xc0f6026e36e2ff58ull }, // 2026-10-26, story SCL-003: the 55 Shared Character & Locomotion keys
};
