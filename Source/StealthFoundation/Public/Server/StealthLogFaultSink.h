#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/StealthFaultSink.h"

#include <atomic>

/**
 * The production fault sink (ADR-0008 section 4, ADR-0007 section 4): every report becomes one warning in the
 * LogStealthFault category and bumps a counter. Thread-safe (a Roster wrong-thread report arrives from the calling
 * thread, story psr-003). The warning names the category, the source and the detail value only; callers never put
 * a secret or a participant-revealing value in Detail.
 */
class STEALTHFOUNDATION_API FStealthLogFaultSink final : public IStealthFaultSink
{
public:
	virtual void Report(EStealthFault Fault, FName Source, int64 Detail) override;

	/** Faults reported since construction. */
	int32 GetReportCount() const { return ReportCount.load(); }

private:
	std::atomic<int32> ReportCount{ 0 };
};
