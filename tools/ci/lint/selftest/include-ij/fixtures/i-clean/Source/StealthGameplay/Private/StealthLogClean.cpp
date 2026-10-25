#include "CoreMinimal.h"
void Clean()
{
	const FString DealDigest = Compute();
	UE_LOGF(LogTemp, Log, "key %d", KeyBinding);
	UE_LOGF(LogTemp, Log, "count %d", BindingCount);
	UE_LOGF(LogTemp, Log, "text mentions Binding and DealDigest only in a string");
	UE_LOG(LogTemp, Log, TEXT("DealDigest in TEXT"));
	// UE_LOG(LogTemp, Log, TEXT("x"), FEventLogEntry);
	/* checkf(false, TEXT("x"), ServerSubSeed); */
	UE_LOG(LogTemp, Log, TEXT("x %d"), MyFEventLog);
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("KeyBinding");
	check(DealDigest.IsEmpty());
}
