#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthTuningSubstructs.h"
#include "View/StealthClientTuning.h"

#include <type_traits>

#include "StealthServerTuning.generated.h"

/**
 * The server face of the tuning (ADR-0004 section 1; story tdc-040): every _Public and every
 * _Server sub-struct. Server code reads it through GetServerTuning() / FValidatedTuning (later
 * stories), never the raw UStealthTuningData asset. The generated schema rows reach each key
 * through pointers-to-member of this struct (&FStealthServerTuning::<Owner>_<Audience> then the
 * key), so the member names here are part of the generated table.
 *
 * UStealthTuningData declares the same members (same names and types); the CanonicalKeys test
 * checks the two stay in step.
 */
USTRUCT()
struct STEALTHFOUNDATION_API FStealthServerTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Round")
	FRoundTuning_Public Round_Public;

	UPROPERTY(EditAnywhere, Category = "Round")
	FRoundTuning_Server Round_Server;

	UPROPERTY(EditAnywhere, Category = "Crowd")
	FCrowdTuning_Server Crowd_Server;

	UPROPERTY(EditAnywhere, Category = "Assassination")
	FAssassinationTuning_Public Assassination_Public;

	UPROPERTY(EditAnywhere, Category = "Penalty")
	FPenaltyTuning_Public Penalty_Public;

	UPROPERTY(EditAnywhere, Category = "Objectives")
	FObjectivesTuning_Public Objectives_Public;

	UPROPERTY(EditAnywhere, Category = "Locomotion")
	FLocomotionTuning_Public Locomotion_Public;

	UPROPERTY(EditAnywhere, Category = "Locomotion")
	FLocomotionTuning_Server Locomotion_Server;
};

// The client face is a distinct type (AC-28): a ServerOnly key never compiles on it.
static_assert(!std::is_same_v<FStealthClientTuning, FStealthServerTuning>, "The client and server tuning faces must be distinct types (ADR-0004 section 1)");
