#include "Server/StealthLogFaultSink.h"

DEFINE_LOG_CATEGORY_STATIC(LogStealthFault, Log, All);

void FStealthLogFaultSink::Report(EStealthFault Fault, FName Source, int64 Detail)
{
	ReportCount.fetch_add(1);
	UE_LOGF(LogStealthFault, Warning, "Fault %d from %ls (detail %lld)", static_cast<int32>(Fault), *Source.ToString(), Detail);
}
