#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

// The tuning schema row vocabulary (ADR-0004 sections 2 and 8; story tdc-038).
//
// Every tuning key is declared once, on its UPROPERTY inside a _Public or _Server sub-struct
// (Server/StealthTuningSubstructs.h, View/StealthTuningPublicSubstructs.h). Because UPROPERTY
// metadata exists only with editor-only data (WITH_METADATA = WITH_EDITORONLY_DATA), the editor
// generator (StealthGameEditor, console command Stealth.Tuning.SchemaGen, story tdc-041) writes the
// checked-in table Private/Tuning/StealthTuningSchema.generated.inl of constexpr rows, and the
// runtime reads only that table, never metadata.

struct FStealthServerTuning;

/**
 * F-2 type tags of a schema row (GDD F-2 typeTag; ADR-0004 amendment 2026-10-25b adds SoftRef = 4).
 * The values are hashed (F-2) and fingerprinted (tdc-043): never renumber them.
 */
namespace StealthTuningTypeTag
{
	inline constexpr uint8 Bool = 0;
	inline constexpr uint8 Int = 1;
	inline constexpr uint8 Float = 2;
	inline constexpr uint8 Enum = 3;
	/** TSoftObjectPtr asset key (`*Asset`, _Server only). No row of this type exists yet: the generator rejects it until the Crowd schema-row story adds the AllowedClass/bAllowNull columns. */
	inline constexpr uint8 SoftRef = 4;
}

/**
 * Latch class of a key (GDD CR-11, ADR-0004 section 8). The order is part of the schema
 * fingerprint: never reorder.
 *   Round   frozen at Preparing, read freely for the round
 *   Action  copied into the action record when the action starts, never re-read for that action
 *   Use     read at the point of use
 *   Session validated at session open and carried unchanged into every round
 */
enum class ETuningLatch : uint8
{
	Round,
	Action,
	Use,
	Session,
};

/**
 * One generated schema row (ADR-0004 Key Interfaces). Values of every type travel as double:
 * bool 0/1, int32 and enum widened exactly, float widened exactly (a float key's Default and
 * ranges are float values widened, so a round trip through Set/Get is lossless).
 *
 * Get/Set are generated non-capturing functions through pointers-to-member (never offsetof), so
 * renaming a field is a compile error in the generated table, not silent memory corruption. A
 * fixture row in a test may leave them null and be used only through value-level functions.
 */
struct FTuningSchemaRow
{
	/** Canonical key, ASCII, globally unique (CR-15.2); the UPROPERTY field name. */
	const ANSICHAR* Key = nullptr;
	/** StealthTuningTypeTag value. */
	uint8 TypeTag = StealthTuningTypeTag::Float;
	/** Reads the key from a server face. */
	double (*Get)(const FStealthServerTuning&) = nullptr;
	/** Writes the key into a server face (bool: non-zero is true; int/enum: rounded). */
	void (*Set)(FStealthServerTuning&, double) = nullptr;
	/** Schema default (the C++ initialiser). */
	double Default = 0.0;
	/** Hard range (F-1 clamps to it). */
	double HardMin = 0.0;
	double HardMax = 0.0;
	/** Safe range (warning only, CR-15.5). */
	double SafeMin = 0.0;
	double SafeMax = 0.0;
	/** Audience: true when the key sits in a _Public sub-struct (on the client face and in ClientHash). */
	bool bPublic = false;
	/** Latch class (CR-11). */
	ETuningLatch Latch = ETuningLatch::Round;
	/** Owner system id (the `Owner` UPROPERTY meta). */
	const ANSICHAR* Owner = nullptr;
	/** Unit or kind suffix (the `Unit` UPROPERTY meta, CR-15.2). */
	const ANSICHAR* Unit = nullptr;
	/** True when the key declares a sentinel (the `Sentinel` UPROPERTY meta, GDD F-1 / CR-3). */
	bool bHasSentinel = false;
	/** A value outside the continuous range that is valid by itself (for example EndedAutoResetSec 0). */
	double Sentinel = 0.0;
};

/**
 * The generated schema table, rows in byte-wise (ordinal, case-sensitive) key order: the F-2 hash
 * order and the snapshot wire order. Empty in a build with STEALTH_TUNING_SCHEMA_BOOTSTRAP=1
 * (story tdc-042), which can never pass SchemaComplete.
 */
STEALTHFOUNDATION_API TConstArrayView<FTuningSchemaRow> GetTuningSchema();

/** The schema fingerprint the generator wrote beside the table (story tdc-043); 0 in a bootstrap build. */
STEALTHFOUNDATION_API uint64 GetTuningSchemaFingerprint();

/** True when this build compiled the empty bootstrap table (STEALTH_TUNING_SCHEMA_BOOTSTRAP=1). */
STEALTHFOUNDATION_API bool IsTuningSchemaBootstrapBuild();

/**
 * Schema fingerprint (ADR-0004 section 2; story tdc-043): FNV-1a-64 over, for each row in
 * byte-wise key order, u8 len(Key) | ascii(Key) | u8 TypeTag | u8 bPublic | u8 Latch | u16le
 * enum value count (0 until enum keys exist). Covers names, types, audiences, latches and enum
 * values; defaults, ranges, owner and unit are deliberately outside it (a default-only change does
 * not need a SchemaVersion bump; confirmation is open for the story reviewer). Input order is
 * irrelevant (the rows are sorted).
 */
STEALTHFOUNDATION_API uint64 ComputeTuningSchemaFingerprint(TConstArrayView<FTuningSchemaRow> Rows);
