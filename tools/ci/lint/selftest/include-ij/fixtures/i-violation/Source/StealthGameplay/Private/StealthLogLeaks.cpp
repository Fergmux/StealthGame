#include "CoreMinimal.h"
void Leaks()
{
	UE_LOG(LogTemp, Log, TEXT("x %d"), FEventLogEntry.Count);
	UE_LOGF(LogTemp, Log, "x %d", DealDigest);
	ensureMsgf(false, TEXT("x"), ServerSubSeed);
	checkf(false, TEXT("x"), FSecretStream);
	ClientMessage(MailboxOrder);
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("DealDigest");
	CSV_SCOPED_TIMING_STAT(Stealth, FRoundEventLogTotals);
	SCOPE_CYCLE_COUNTER(Binding);
	UE_LOGF(LogTemp, Log, "x %d",
		Binding);
	UE_LOG(LogTemp, Log, TEXT("x"), Wrap(Deeper(FEventLogEntry)));
}
