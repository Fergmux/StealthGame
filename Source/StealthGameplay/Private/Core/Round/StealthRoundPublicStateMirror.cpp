#include "Server/Core/Round/StealthRoundPublicStateMirror.h"

#include "Server/Core/Round/StealthRoundPhaseCore.h"

void FRoundPublicStateMirror::RegisterSink(IRoundPublicStateSink* InSink, UObject* Owner, const FRoundPhaseCore& Core)
{
	Sink = InSink;
	SinkOwner = Owner;
	bHasWritten = false;
	Publish(Core);
}

bool FRoundPublicStateMirror::Publish(const FRoundPhaseCore& Core)
{
	if (Sink == nullptr || !SinkOwner.IsValid())
	{
		return false;
	}
	const FRoundPublicState State = Core.MakePublicState();
	if (bHasWritten && State == LastWritten)
	{
		return false;
	}
	Sink->WriteRoundPublicState(State);
	LastWritten = State;
	bHasWritten = true;
	return true;
}
