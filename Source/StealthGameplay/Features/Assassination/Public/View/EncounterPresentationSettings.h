#pragma once

// Stories asn-018 / asn-020 / asn-021: the presentation data of the encounter (clip assets and the authored clips' own
// timings, the stand-in clock and the transient grade). Gameplay timings (contact, lockout, fall, linger, grace) are TDC
// public tuning keys (FAssassinationTuning_Public), never here.

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "EncounterPresentationSettings.generated.h"

class UAnimMontage;

/**
 * Project settings "Stealth Encounter Presentation" (Config/DefaultGame.ini, [/Script/StealthGameplay.EncounterPresentationSettings]).
 *
 * PLACEHOLDER clips (stories asn-018 / asn-020): the D54 engine half (SCL story 056 follow-up) has not delivered the
 * authored Strike_F / Strike_L60 / Strike_R60 / BushPreface / Counter / Death_Fall clips, so the three strike slots point
 * at one montage built from the D54 spike clip A_D54_Strike_T2MSwiftFast, the counter at A_D54_Brace_LibBlock1 and the
 * fall at A_D54_Fall_LibShotFallForward (imported by Stealth.Animations.ImportEncounterClips). Replace the asset paths and
 * the clip timings below when the authored clips land; no code changes.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Stealth Encounter Presentation"))
class STEALTHGAMEPLAY_API UEncounterPresentationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UEncounterPresentationSettings();

	/** The class default object. */
	static const UEncounterPresentationSettings* Get();

	/** Strike entry montages by variant (ASN-R16), root motion with a runtime "StrikeContact" skew-warp window. */
	UPROPERTY(Config, EditAnywhere, Category = "Clips")
	TSoftObjectPtr<UAnimMontage> StrikeFMontage;

	UPROPERTY(Config, EditAnywhere, Category = "Clips")
	TSoftObjectPtr<UAnimMontage> StrikeL60Montage;

	UPROPERTY(Config, EditAnywhere, Category = "Clips")
	TSoftObjectPtr<UAnimMontage> StrikeR60Montage;

	/** The defender's counter after a reversal (starts at the reversed strike's contact). */
	UPROPERTY(Config, EditAnywhere, Category = "Clips")
	TSoftObjectPtr<UAnimMontage> CounterMontage;

	/** The one Death_Fall of every victim (never auto blends out: the final pose is held through the linger). */
	UPROPERTY(Config, EditAnywhere, Category = "Clips")
	TSoftObjectPtr<UAnimMontage> DeathFallMontage;

	/** Clip time of the strike clip's contact frame (the end of its warp window; the D54 spike clip's frame 28 at 30 fps). */
	UPROPERTY(Config, EditAnywhere, Category = "Clips", meta = (ClampMin = "0.05", Units = "s"))
	float StrikeClipContactSec = 0.9f;

	/**
	 * STAND-IN for the ADR-0002 section 9 clock estimator (Round story 091): a client counts its server-time estimate as
	 * synced once this long has passed since its world began play (the server always is).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Clock", meta = (ClampMin = "0", Units = "s"))
	float UnsyncedClockGraceSec = 5.0f;

	/** A playing clip further than this from its scheduled position is sought (never restarted). */
	UPROPERTY(Config, EditAnywhere, Category = "Clips", meta = (ClampMin = "0.005", Units = "s"))
	float SeekToleranceSec = 0.05f;

	/** Transient grade (art bible 2.7): snap in, hold, ease out. */
	UPROPERTY(Config, EditAnywhere, Category = "Grade", meta = (Units = "s"))
	float GradeInSec = 0.12f;

	UPROPERTY(Config, EditAnywhere, Category = "Grade", meta = (Units = "s"))
	float GradeHoldSec = 0.20f;

	UPROPERTY(Config, EditAnywhere, Category = "Grade", meta = (Units = "s"))
	float GradeOutSec = 0.80f;

	/** Effective white temperature of the grade (6,500 K + 900 K, art bible 2.7). */
	UPROPERTY(Config, EditAnywhere, Category = "Grade")
	float GradeWhiteTemp = 7400.0f;

	/** Saturation multiplier (-60 percent). */
	UPROPERTY(Config, EditAnywhere, Category = "Grade")
	float GradeSaturation = 0.4f;

	/** Contrast multiplier (+25 percent). */
	UPROPERTY(Config, EditAnywhere, Category = "Grade")
	float GradeContrast = 1.25f;

	/** Vignette intensity (25 percent). */
	UPROPERTY(Config, EditAnywhere, Category = "Grade")
	float GradeVignette = 0.25f;

	/** Reduce screen effects (art bible 7.6 rule 8): the snap eases over this long ... */
	UPROPERTY(Config, EditAnywhere, Category = "Grade|Reduce screen effects", meta = (Units = "s"))
	float ReducedGradeInSec = 0.5f;

	/** ... the saturation drop is capped at -30 percent, and the vignette is off. */
	UPROPERTY(Config, EditAnywhere, Category = "Grade|Reduce screen effects")
	float ReducedGradeSaturation = 0.7f;
};
