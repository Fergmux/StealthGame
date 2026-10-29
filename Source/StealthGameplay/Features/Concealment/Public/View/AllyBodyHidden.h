#pragma once

#include "CoreMinimal.h"

class AStealthPawn;

/**
 * The presentation-safe client read of the body-hidden latch (ADR-0017 section 5; story con-020 AC4): true while Pawn's
 * body is presentation-hidden on THIS machine (an ally hidden in a bush, a fresh actor not yet evaluated, a killed-concealed
 * body). Teammate Markers TM-R5 draws the hatched "concealed" marker from it. Client-local; never replicated, never a
 * server rule.
 *
 * Usage (HUD): if (IsAllyBodyHidden(Teammate)) { DrawHatchedMarker(Teammate); }
 */
STEALTHGAMEPLAY_API bool IsAllyBodyHidden(const AStealthPawn* Pawn);
