#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "StealthSettings.generated.h"

class AStealthCharacter;
class UMaterialInterface;
class USkeletalMesh;

/**
 * All prototype tuning values, data-driven through Config/DefaultGame.ini
 * ([/Script/StealthGame.StealthSettings]) and editable in Project Settings > Game > Stealth Game.
 * Defaults are the "Suggested first-draft defaults" of GAME_BUILD_HANDOFF.md plus the user's
 * 2026-10-24 answers (no match timer, ~2 m range, ~3 s interactions); NpcCount 50 (user decision 2026-10-25,
 * the TDC NpcCount default).
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Stealth Game"))
class STEALTHGAME_API UStealthSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Convenience accessor for the class default object. */
	static const UStealthSettings* Get() { return GetDefault<UStealthSettings>(); }

	/** Pawn class used for players AND NPCs (one class keeps them indistinguishable). */
	UPROPERTY(Config, EditAnywhere, Category = "Classes")
	TSoftClassPtr<AStealthCharacter> CharacterClass;

	/** Maximum players per team. Confirmed rule: five. */
	UPROPERTY(Config, EditAnywhere, Category = "Teams", meta = (ClampMin = "1", ClampMax = "5"))
	int32 MaxPlayersPerTeam = 5;

	/** Display names of the two teams (Mexica warrior societies). */
	UPROPERTY(Config, EditAnywhere, Category = "Teams")
	TArray<FText> TeamNames = { NSLOCTEXT("Stealth", "TeamJaguar", "Jaguar"), NSLOCTEXT("Stealth", "TeamEagle", "Eagle") };

	/** Players needed before a round auto-starts. 1 allows solo PIE testing. */
	UPROPERTY(Config, EditAnywhere, Category = "Round", meta = (ClampMin = "1"))
	int32 MinPlayersToStart = 1;

	/** Whether the round starts automatically once enough players joined. */
	UPROPERTY(Config, EditAnywhere, Category = "Round")
	bool bAutoStartRound = true;

	/** Countdown before an auto-start, seconds. */
	UPROPERTY(Config, EditAnywhere, Category = "Round", meta = (ClampMin = "0"))
	float RoundStartDelay = 5.f;

	/** Seconds the end-of-round result is shown before a new round starts. */
	UPROPERTY(Config, EditAnywhere, Category = "Round", meta = (ClampMin = "1"))
	float RoundRestartDelay = 12.f;

	/** Seconds before an eliminated player re-enters the world as an informant. */
	UPROPERTY(Config, EditAnywhere, Category = "Round", meta = (ClampMin = "0"))
	float InformantRespawnDelay = 4.f;

	/** Defensive preparation window, seconds. Confirmed range 3-5; first-draft default 4. */
	UPROPERTY(Config, EditAnywhere, Category = "Defence", meta = (ClampMin = "3", ClampMax = "5"))
	float DefenceDuration = 4.f;

	/** Defence cooldown, seconds, measured from activation (including unused attempts). */
	UPROPERTY(Config, EditAnywhere, Category = "Defence", meta = (ClampMin = "1"))
	float DefenceCooldown = 60.f;

	/** Assassination reach, cm (~2 m). */
	UPROPERTY(Config, EditAnywhere, Category = "Assassination", meta = (ClampMin = "50"))
	float AssassinationRange = 200.f;

	/** Server multiplies the range by this to tolerate ~1 RTT of positional lag. */
	UPROPERTY(Config, EditAnywhere, Category = "Assassination", meta = (ClampMin = "1"))
	float AssassinationServerTolerance = 1.5f;

	/** Half-angle of the targeting cone in front of the attacker, degrees. */
	UPROPERTY(Config, EditAnywhere, Category = "Assassination", meta = (ClampMin = "10", ClampMax = "180"))
	float AssassinationConeHalfAngle = 50.f;

	/** Radius around the strike point that a blind attack searches for a concealed enemy, cm. */
	UPROPERTY(Config, EditAnywhere, Category = "Assassination", meta = (ClampMin = "10"))
	float BlindStrikeRadius = 130.f;

	/** Seconds the attacker is held in place after striking (the strike itself is a tell). */
	UPROPERTY(Config, EditAnywhere, Category = "Assassination", meta = (ClampMin = "0"))
	float AttackerRecoveryTime = 1.f;

	/** Seconds a body stays in the world before it is removed. */
	UPROPERTY(Config, EditAnywhere, Category = "Assassination", meta = (ClampMin = "1"))
	float BodyLifetime = 10.f;

	/** NPC kills that remove a player from active play (removal ON this kill). */
	UPROPERTY(Config, EditAnywhere, Category = "NPC Penalty", meta = (ClampMin = "1"))
	int32 NpcKillLimit = 3;

	// RunSpeedThreshold and InformantMaxSpeed were removed by story SCL-003 (TDC CR-13: no config back door): IsRunning
	// is the SCL tuning key RunSpeedThresholdCmPerSec with hysteresis, and informants are clamped to the Walk gait.

	/** Seconds of held interaction needed to complete an objective. */
	UPROPERTY(Config, EditAnywhere, Category = "Objectives", meta = (ClampMin = "0.5"))
	float ObjectiveInteractTime = 3.f;

	/** Distance within which a player can interact with an objective, cm. */
	UPROPERTY(Config, EditAnywhere, Category = "Objectives", meta = (ClampMin = "50"))
	float ObjectiveInteractRange = 250.f;

	/** Crowd NPCs per round; 50 matches the TDC key NpcCount (user decision 2026-10-25). */
	UPROPERTY(Config, EditAnywhere, Category = "Crowd", meta = (ClampMin = "0", ClampMax = "200"))
	int32 NpcCount = 50;

	/** Radius around the crowd centre used for NPC spawn and wander points, cm. */
	UPROPERTY(Config, EditAnywhere, Category = "Crowd", meta = (ClampMin = "100"))
	float CrowdRadius = 3500.f;

	/** NPC idle pause range between walks, seconds. */
	UPROPERTY(Config, EditAnywhere, Category = "Crowd")
	FVector2D NpcIdleTimeRange = FVector2D(2.f, 8.f);

	/** Seconds after an NPC is killed before a replacement walks in elsewhere (keeps density). */
	UPROPERTY(Config, EditAnywhere, Category = "Crowd", meta = (ClampMin = "0"))
	float NpcReplacementDelay = 15.f;

	/** Appearance pool: tints applied identically to players and NPCs ("Held Breath" earth palette). */
	UPROPERTY(Config, EditAnywhere, Category = "Appearance")
	TArray<FLinearColor> AppearancePalette = {
		FLinearColor(0.80f, 0.74f, 0.62f), // limewash
		FLinearColor(0.55f, 0.36f, 0.24f), // adobe
		FLinearColor(0.42f, 0.40f, 0.36f), // basalt
		FLinearColor(0.70f, 0.52f, 0.30f), // ochre
		FLinearColor(0.33f, 0.45f, 0.40f), // water-green
		FLinearColor(0.60f, 0.30f, 0.22f), // terracotta
	};

	/**
	 * The fixed character-model pool shared by players and NPCs (confirmed rule). Each character gets one
	 * model from this list. Empty = every character keeps the Blueprint's mesh and is tinted from AppearancePalette.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Appearance")
	TArray<TSoftObjectPtr<USkeletalMesh>> AppearanceMeshes;

	/** Number of distinct appearances a character can be given. */
	int32 GetAppearancePoolSize() const { return AppearanceMeshes.Num() > 0 ? AppearanceMeshes.Num() : AppearancePalette.Num(); }

	/** Material applied to every character slot and tinted through its "Tint" parameter. Empty = tint the mesh's own materials. */
	UPROPERTY(Config, EditAnywhere, Category = "Appearance")
	TSoftObjectPtr<UMaterialInterface> AppearanceMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/StealthGame/Materials/M_CrowdTint.M_CrowdTint")));

	/** Material vector parameter names tried, in order, when tinting a character. */
	UPROPERTY(Config, EditAnywhere, Category = "Appearance")
	TArray<FName> AppearanceColorParameters = { TEXT("Tint"), TEXT("BaseColor"), TEXT("Color"), TEXT("Base Color") };

	/** How long a ping stays on teammates' screens, seconds. */
	UPROPERTY(Config, EditAnywhere, Category = "Communication", meta = (ClampMin = "0.5"))
	float PingLifetime = 4.f;

	/** Minimum seconds between two pings from the same player. */
	UPROPERTY(Config, EditAnywhere, Category = "Communication", meta = (ClampMin = "0"))
	float PingCooldown = 1.5f;

	/**
	 * Client-local camera toggle lockout, seconds (IIF CameraToggleLockoutSec, default 0.30, hard 0-1, safe 0.25-0.5;
	 * story iif-014). Read here until the IIF tuning keys are registered with the TDC schema.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Input", meta = (ClampMin = "0", ClampMax = "1"))
	float CameraToggleLockoutSec = 0.30f;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
