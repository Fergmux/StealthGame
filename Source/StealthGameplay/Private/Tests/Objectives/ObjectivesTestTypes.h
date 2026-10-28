#pragma once

// Reflected control types of the StealthGame.Objectives.* static checks (story oc-002 AC1: a seeded struct with a HoldSec
// member must fail the same "no hold-time field" check FObjectiveDef passes). Never used by game code.

#include "CoreMinimal.h"

#include "ObjectivesTestTypes.generated.h"

/** Seeded violation: an objective definition shape that carries a per-row hold time. */
USTRUCT()
struct FObjectivesSeededHoldDef
{
	GENERATED_BODY()

	UPROPERTY()
	FName ObjectiveId;

	UPROPERTY()
	float HoldSec = 0.0f;
};

/** Seeded violation: card text declared as FString instead of FText (story oc-002 AC4 edge case). */
USTRUCT()
struct FObjectivesSeededStringCardDef
{
	GENERATED_BODY()

	UPROPERTY()
	FString CardLine;
};
