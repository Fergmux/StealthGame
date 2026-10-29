// Concealment story con-003 (ADR-0017 section 1): the registry of feature components every AStealthPawn creates. See
// View/Core/StealthMotionGate.h.

#include "View/Core/StealthMotionGate.h"

namespace StealthPawnFeatureComponents
{
	namespace
	{
		/** Function-local so static-initialisation order across translation units never matters. */
		TArray<FEntry>& Entries()
		{
			static TArray<FEntry> Registered;
			return Registered;
		}
	}

	void Register(FName Name, FClassGetter GetClass)
	{
		for (const FEntry& Entry : Entries())
		{
			if (Entry.Name == Name)
			{
				return; // one component per name (a hot-reloaded module re-registers)
			}
		}
		Entries().Add({ Name, GetClass });
	}

	const TArray<FEntry>& GetEntries()
	{
		return Entries();
	}
}
