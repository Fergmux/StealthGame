#include "Server/CrowdDueTimeList.h"

uint32 FCrowdDueTimeList::Schedule(ECrowdDueKind Kind, double Due, uint32 Payload, bool bReplaceSameKind)
{
	if (bReplaceSameKind)
	{
		Cancel(Kind);
	}
	FCrowdDueEntry& Entry = Entries.AddDefaulted_GetRef();
	Entry.Due = Due;
	Entry.Kind = Kind;
	Entry.Payload = Payload;
	Entry.Generation = NextGeneration++;
	return Entry.Generation;
}

void FCrowdDueTimeList::Cancel(ECrowdDueKind Kind)
{
	Entries.RemoveAll([Kind](const FCrowdDueEntry& Entry) { return Entry.Kind == Kind; });
}

bool FCrowdDueTimeList::PopDue(double Now, FCrowdDueEntry& Out)
{
	int32 Best = INDEX_NONE;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FCrowdDueEntry& Entry = Entries[Index];
		if (Entry.Due <= Now && (Best == INDEX_NONE || Entry.Due < Entries[Best].Due || (Entry.Due == Entries[Best].Due && Entry.Generation < Entries[Best].Generation)))
		{
			Best = Index;
		}
	}
	if (Best == INDEX_NONE)
	{
		return false;
	}
	Out = Entries[Best];
	Entries.RemoveAt(Best, EAllowShrinking::No);
	return true;
}

bool FCrowdDueTimeList::IsPending(ECrowdDueKind Kind) const
{
	return Entries.ContainsByPredicate([Kind](const FCrowdDueEntry& Entry) { return Entry.Kind == Kind; });
}

double FCrowdDueTimeList::GetDue(ECrowdDueKind Kind) const
{
	const FCrowdDueEntry* Entry = Entries.FindByPredicate([Kind](const FCrowdDueEntry& E) { return E.Kind == Kind; });
	return Entry != nullptr ? Entry->Due : -1.0;
}
