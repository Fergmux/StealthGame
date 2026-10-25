#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

#if WITH_STEALTH_TEST_SEAMS

#include "Server/StealthServerTuning.h"

/**
 * Test-only Action-latch consumer (ADR-0004 section 8, GDD CR-11; story tdc-052). It stands in for an
 * owner system with an Action-latched duration (the defence) until that epic lands. Not a production API.
 * Compiled only under WITH_STEALTH_TEST_SEAMS.
 *
 * At action start it reads DefenceDurationSec once, copies it into its own action record and computes the
 * end as start plus the captured value. It keeps no reference to the live tuning, so it can never re-read
 * it: a later change of the live set (a PIE hot-reload, story tdc-085) does not retime the action.
 */
class FStubActionConsumer
{
public:
	/** The action record (captured at start, never refreshed). */
	struct FActionRecord
	{
		double StartSec = 0.0;
		double CapturedDurationSec = 0.0;
		double EndSec = 0.0;
	};

	/** Starts the action at StartSec, capturing DefenceDurationSec from Live (the one read of the live value). */
	void StartAction(double StartSec, const FStealthServerTuning& Live)
	{
		Record.StartSec = StartSec;
		Record.CapturedDurationSec = static_cast<double>(Live.Assassination_Public.DefenceDurationSec);
		Record.EndSec = StartSec + Record.CapturedDurationSec;
		bActive = true;
		++LiveReads;
	}

	/** True while NowSec is before the captured end (and after the start). */
	bool IsActive(double NowSec) const { return bActive && NowSec >= Record.StartSec && NowSec < Record.EndSec; }

	/** The action record. */
	const FActionRecord& GetRecord() const { return Record; }

	/** How many times the live tuning was read (one per StartAction). */
	int32 GetLiveReads() const { return LiveReads; }

private:
	FActionRecord Record;
	bool bActive = false;
	int32 LiveReads = 0;
};

#endif // WITH_STEALTH_TEST_SEAMS
