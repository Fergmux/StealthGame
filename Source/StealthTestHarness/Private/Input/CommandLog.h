#pragma once

// The server command log (story iif-095, ADR-0011 Validation Criteria): while alive, records every command observed at
// the output of stage-1 ingress and at the core's queue submit (Server/Testing/StealthCommandSeams.h), in order, with
// its kind, payload, ClientCommandSeq, arrival tick and opaque sink. It serves the "server command log" of GDD ACs 5,
// 30 and 45. One log at a time (the seam has a single observer). Harness-only.

#include "CoreMinimal.h"
#include "Server/Testing/StealthCommandSeams.h"

class FStealthCommandLog
{
public:
	FStealthCommandLog()
	{
		StealthCommandSeams::SetObserver([this](const FStealthCommandLogEntry& Entry) { Entries.Add(Entry); });
	}

	~FStealthCommandLog()
	{
		StealthCommandSeams::SetObserver(StealthCommandSeams::FObserver());
	}

	FStealthCommandLog(const FStealthCommandLog&) = delete;
	FStealthCommandLog& operator=(const FStealthCommandLog&) = delete;

	/** The entries observed at one point, in order. */
	TArray<FStealthCommandLogEntry> At(EStealthCommandLogPoint Point) const
	{
		return Entries.FilterByPredicate([Point](const FStealthCommandLogEntry& E) { return E.Point == Point; });
	}

	/** The kinds observed at one point, in order. */
	TArray<ECommandKind> KindsAt(EStealthCommandLogPoint Point) const
	{
		TArray<ECommandKind> Kinds;
		for (const FStealthCommandLogEntry& Entry : Entries)
		{
			if (Entry.Point == Point)
			{
				Kinds.Add(Entry.Kind);
			}
		}
		return Kinds;
	}

	void Reset() { Entries.Reset(); }

	TArray<FStealthCommandLogEntry> Entries;
};
