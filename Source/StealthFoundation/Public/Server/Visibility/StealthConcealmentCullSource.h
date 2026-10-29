#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

class APawn;

/**
 * IConcealmentCullSource: the Information Visibility Policy's consumer-owned cull input (ADR-0003 section 5; ADR-0017
 * section 3; IVP stories 024/025, Concealment story con-025). Declared here in Foundation so the Policy never names a
 * feature type; Concealment (a feature) is its only caller and calls it only inside a queue slot (or the staging pass for
 * a killed-concealed victim's death write):
 *   SetCullState(Pawn, true, CommitServerTime)   at the commit: CullPending, the deadline on the global pawn-update grid
 *   SetCullState(Pawn, false, Now)               at exit execution, a break, or the death write: Visible in the same step
 *
 * Usage (Concealment core, Commit item):
 *   Cull->SetCullState(Pawn, true, Clock->GetServerTimeSeconds());
 */
class IConcealmentCullSource
{
public:
	virtual ~IConcealmentCullSource() = default;

	/** Pushes Pawn's concealment into the cull record (bConcealed true: CullPending from CommitServerTime; false: Visible now). */
	virtual void SetCullState(APawn* Pawn, bool bConcealed, double CommitServerTime) = 0;
};
