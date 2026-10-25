#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

/**
 * Schema constants (ADR-0004 section 2, Amendments 2; story tdc-038): values that invariants or
 * owner rules read but that are NOT preset fields. They are never hashed (F-2), never replicated,
 * and no preset can change them; changing one is a code change plus a GDD change, like a hard range.
 *
 * Exactly the constants below live here; each `inline constexpr` declaration must also appear in
 * StealthTuningConstants::All (the test StealthGame.Tuning.SchemaRowTypes compares the header text
 * with that list and the list with the expected six). The IVP LeakNetProfile constants and the
 * T_uncull inputs (I-CON-a..e) belong to the Information Visibility Policy and Concealment epics.
 */
namespace StealthTuningConstants
{
	/** I-4 threshold on 100 * BlindGuessSuccess (TDC Canonical Tunables, I-4; provisional pending playtest). */
	inline constexpr double BlindGuessWarnPct = 50.0;
	/** Grace margin in seconds (Round & Match Framework F4). */
	inline constexpr double GraceMarginSec = 30.0;
	/** Bush dome inner radius in cm, a fixed asset fact (Camera I-CS-4, Concealment). */
	inline constexpr double BushDomeInnerRadiusCm = 90.0;
	/** Shared Character & Locomotion capsule radius r_c in cm (Assassination I-ASN-1). */
	inline constexpr double SclCapsuleRadiusCm = 35.0;
	/** Clock-skew tolerance in seconds (Informant Pings F1, C-W22). */
	inline constexpr double PingSkewTolSec = 0.1;
	/** Due-time comparison epsilon in milliseconds (Round & Match Framework R12, ruling D22). */
	inline constexpr int32 DueEpsilonMs = 1;

	/** One schema constant: name, value (widened to double) and its GDD reference. */
	struct FEntry
	{
		const TCHAR* Name;
		double Value;
		const TCHAR* GddRef;
	};

	/** Every schema constant of this header, in declaration order. */
	inline constexpr FEntry All[] = {
		{ TEXT("BlindGuessWarnPct"), BlindGuessWarnPct, TEXT("tuning-data-config.md I-4") },
		{ TEXT("GraceMarginSec"), GraceMarginSec, TEXT("round-match-framework.md F4") },
		{ TEXT("BushDomeInnerRadiusCm"), BushDomeInnerRadiusCm, TEXT("camera-system.md I-CS-4") },
		{ TEXT("SclCapsuleRadiusCm"), SclCapsuleRadiusCm, TEXT("assassination-defence-reversal.md I-ASN-1") },
		{ TEXT("PingSkewTolSec"), PingSkewTolSec, TEXT("informant-pings-team-comms.md F1") },
		{ TEXT("DueEpsilonMs"), static_cast<double>(DueEpsilonMs), TEXT("round-match-framework.md R12") },
	};
}
