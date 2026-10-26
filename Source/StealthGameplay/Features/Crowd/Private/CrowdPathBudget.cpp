#include "Server/CrowdPathBudget.h"

void FCrowdPathBudget::Enqueue(const FCrowdPathRequest& Request)
{
	++Enqueued;
	for (FCrowdPathRequest& Existing : Pending)
	{
		if (Existing.Ordinal == Request.Ordinal)
		{
			Existing = Request;
			++Replaced;
			return;
		}
	}
	Pending.Add(Request);
}

int32 FCrowdPathBudget::StartThisFrame(int32 Limit, TFunctionRef<void(const FCrowdPathRequest&)> Start)
{
	// Pending holds at most one request per ordinal, so FIFO order is also round-robin over requesters.
	const int32 Count = FMath::Min(FMath::Max(Limit, 0), Pending.Num());
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FCrowdPathRequest Request = Pending[0];
		Pending.RemoveAt(0, EAllowShrinking::No);
		++Started;
		Start(Request);
	}
	return Count;
}
