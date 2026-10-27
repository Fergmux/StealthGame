#pragma once

// Story asn-024 (production/epics/assassination-defence-reversal/story-024-owner-result-matcher-and-plate-timing.md): the
// client-local HUD tuning the owner result plates read (design/registry/entities.yaml HUD keys; GDD hud.md F2 / F3).

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "OwnResultPlateSettings.generated.h"

struct FOwnResultMatcherParams;
struct FOwnResultPlateTimelineParams;

/**
 * Project settings "Stealth Owner Result Plates" (Config/DefaultGame.ini, [/Script/StealthGameplay.OwnResultPlateSettings]).
 *
 * STAND-IN for the HUD epic's Client tuning keys (hud story 001, the TDC client store): the values and hard ranges are the
 * registry's (design/registry/entities.yaml), the names are the registry key names. When the HUD keys land in the tuning
 * schema, FromSettings() reads them there instead; no caller changes.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Stealth Owner Result Plates"))
class STEALTHGAMEPLAY_API UOwnResultPlateSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UOwnResultPlateSettings();

	/** The class default object. */
	static const UOwnResultPlateSettings* Get();

	/** The matcher's parameters (HudResultPlateMaxHoldBackSec, clamped to its 1.25 s hard minimum). */
	FOwnResultMatcherParams MatcherParams() const;

	/** The plate timeline (F3) of every result and defence plate. */
	FOwnResultPlateTimelineParams TimelineParams() const;

	/**
	 * F2 lost-data cap: a result whose contact time is unknown shows at its arrival + this. Hard range 1.25-2.0 s (decision
	 * D8): never below the strike lead, so the fallback never fires before a real contact.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Show time", meta = (ClampMin = "1.25", ClampMax = "2.0", Units = "s"))
	float HudResultPlateMaxHoldBackSec = 1.25f;

	/** F3 stepped entrance (four steps). Hard range 0.1-0.5 s. */
	UPROPERTY(Config, EditAnywhere, Category = "Timeline", meta = (ClampMin = "0.1", ClampMax = "0.5", Units = "s"))
	float HudResultPlateInSec = 0.25f;

	/** F3 hold. Hard range 1.0-6.0 s. */
	UPROPERTY(Config, EditAnywhere, Category = "Timeline", meta = (ClampMin = "1.0", ClampMax = "6.0", Units = "s"))
	float HudResultPlateHoldSec = 3.0f;

	/** F3 exit fade. Hard range 0.2-1.0 s. */
	UPROPERTY(Config, EditAnywhere, Category = "Timeline", meta = (ClampMin = "0.2", ClampMax = "1.0", Units = "s"))
	float HudResultPlateOutSec = 0.6f;

	/** Read-hold multiplier on the hold (accessibility). Hard range 1.0-2.0. */
	UPROPERTY(Config, EditAnywhere, Category = "Timeline", meta = (ClampMin = "1.0", ClampMax = "2.0"))
	float HudReadHoldMult = 1.0f;

	/**
	 * STAND-IN for Penalty's ExileBeatSec (npc-kill-penalty-informants.md): the exile card follows the exiling WrongTarget
	 * plate by this much. Hard range 0-2.0 s.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Exile", meta = (ClampMin = "0.0", ClampMax = "2.0", Units = "s"))
	float ExileBeatSec = 0.8f;
};
