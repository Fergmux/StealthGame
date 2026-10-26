#include "Server/Core/PawnCommandCore.h"

#include "Server/Queue/StealthResolutionQueueCore.h"
#include "Server/StealthClock.h"
#include "Server/StealthFaultSink.h"
#include "Server/Testing/StealthCommandSeams.h"

namespace PawnCommandCorePrivate
{
	const FName FaultSource(TEXT("PawnCommandCore"));
}

bool FPawnCommandCore::Initialize(const FPawnCommandCoreDeps& InDeps)
{
	Deps = InDeps;
	if (!IsValid())
	{
		ReportMisuse(EPawnCommandMisuse::MissingSeam);
	}
	return IsValid();
}

bool FPawnCommandCore::Submit(const FCommandOrigin& Origin, ECommandKind Kind, const FCommandPayload& Payload)
{
	if (!IsValid())
	{
		ReportMisuse(EPawnCommandMisuse::MissingSeam);
		return false;
	}
	if (static_cast<int32>(Kind) >= GCommandKindCount)
	{
		ReportMisuse(EPawnCommandMisuse::KindOutOfRange);
		return false;
	}

	FCommandPayload Stamped = Payload;
	Stamped.Kind = static_cast<uint8>(Kind);
	Stamped.Sink = Origin.Sink;

	const FEpochToken Expected = Deps.Epoch ? Deps.Epoch->GetCurrentEpoch() : FEpochToken();
	LastSubmitServerTime = ReadClock();
	Deps.Queue->Submit(Origin.Mailbox, FQueueItem::Make(EQueueItemType::PawnCommand, Expected, Origin.Mailbox, Stamped));
	++SubmittedCount;

#if WITH_STEALTH_TEST_SEAMS
	// The server command log's submit point (story iif-095); the tick is read only while a log observes.
	if (StealthCommandSeams::HasObserver())
	{
		FStealthCommandLogEntry Entry;
		Entry.Point = EStealthCommandLogPoint::QueueSubmit;
		Entry.Kind = Kind;
		Entry.Seq = Stamped.ClientSeq;
		Entry.ArrivalTick = Deps.Clock->GetServerTickIndex();
		Entry.Sink = Stamped.Sink;
		Entry.Mailbox = Origin.Mailbox;
		Entry.Payload = Stamped;
		StealthCommandSeams::Notify(Entry);
	}
#endif
	return true;
}

double FPawnCommandCore::ReadClock()
{
	const double Now = Deps.Clock->GetServerTimeSeconds();
	if (Now < LastReadServerTime)
	{
		// Undefined by the spec (QL-STORY-READY gap, story iif-002): pinned to "report and clamp", never back-date.
		ReportMisuse(EPawnCommandMisuse::ClockRegressed);
		return LastReadServerTime;
	}
	LastReadServerTime = Now;
	return Now;
}

void FPawnCommandCore::ReportMisuse(EPawnCommandMisuse Misuse) const
{
	if (Deps.FaultSink)
	{
		Deps.FaultSink->Report(EStealthFault::CommandMisuse, PawnCommandCorePrivate::FaultSource, static_cast<int64>(Misuse));
	}
}
