#pragma once

#include "CoreMinimal.h"
#include "View/StealthTuningPublicSubstructs.h"

#include "StealthClientTuning.generated.h"

/**
 * The client face of the tuning (ADR-0004 section 1; story tdc-040): every _Public sub-struct and
 * nothing else. A distinct type from FStealthServerTuning (Server/StealthServerTuning.h), so a
 * ServerOnly key does not compile on the client (AC-28). Client and presentation code read it
 * through IStealthClientTuning::GetClientTuning() (later story), never a Server/ header.
 *
 * Member names are <Owner>_Public and must match the same-named members of FStealthServerTuning.
 */
USTRUCT()
struct STEALTHFOUNDATION_API FStealthClientTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Round")
	FRoundTuning_Public Round_Public;

	UPROPERTY(EditAnywhere, Category = "Assassination")
	FAssassinationTuning_Public Assassination_Public;

	UPROPERTY(EditAnywhere, Category = "Penalty")
	FPenaltyTuning_Public Penalty_Public;

	UPROPERTY(EditAnywhere, Category = "Objectives")
	FObjectivesTuning_Public Objectives_Public;

	UPROPERTY(EditAnywhere, Category = "Concealment")
	FConcealmentTuning_Public Concealment_Public;

	UPROPERTY(EditAnywhere, Category = "Locomotion")
	FLocomotionTuning_Public Locomotion_Public;

	UPROPERTY(EditAnywhere, Category = "Camera")
	FCameraTuning_Public Camera_Public;
};
