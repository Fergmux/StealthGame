#pragma once

#include "CoreMinimal.h"

#include "StealthGait.generated.h"

/**
 * The commanded gait of a Move (ADR-0010 section 6, ADR-0011 section 1 `Move(FVector2D Direction, EGait Gait)`).
 * Walk paces come from the Move magnitude (the shared pace set); Run is the run tier. Client-safe: the owning client
 * builds it and ServerMove carries it (FLAG_Custom_0 when the SCL speed-tier story lands).
 */
UENUM()
enum class EGait : uint8
{
	Walk,
	Run,
};
