#include "Server/Testing/StealthCommandSeams.h"

#if WITH_STEALTH_TEST_SEAMS

namespace StealthCommandSeamsPrivate
{
	StealthCommandSeams::FObserver& Observer()
	{
		static StealthCommandSeams::FObserver Instance;
		return Instance;
	}
}

void StealthCommandSeams::SetObserver(FObserver InObserver)
{
	check(IsInGameThread());
	StealthCommandSeamsPrivate::Observer() = MoveTemp(InObserver);
}

bool StealthCommandSeams::HasObserver()
{
	return static_cast<bool>(StealthCommandSeamsPrivate::Observer());
}

void StealthCommandSeams::Notify(const FStealthCommandLogEntry& Entry)
{
	if (const FObserver& Current = StealthCommandSeamsPrivate::Observer())
	{
		Current(Entry);
	}
}

#endif // WITH_STEALTH_TEST_SEAMS
