#include "Server/Session/StealthSessionCore.h"

FSessionCore::FSessionCore()
	: Drawer(Seeds)
{
}

EStealthSessionOpenResult FSessionCore::Open(IEntropySource& Entropy, IStealthFaultSink* FaultSink)
{
	const EStealthSessionOpenResult Result = Seeds.OpenSession(Entropy, FaultSink);
	bOpen = Result == EStealthSessionOpenResult::Opened;
	return Result;
}

bool FSessionCore::End(ESessionEndReason Reason)
{
	const bool bWasOpen = bOpen;
	Seeds.Close();
	Seeds.SetRoundIndex(0);
	Drawer.Reset();
	KickedKeys.Reset();
	bOpen = false;
	++EndCount;
	LastEndReason = Reason;
	return bWasOpen;
}

bool FSessionCore::IsEmpty() const
{
	return !bOpen && !Seeds.IsOpen() && Seeds.GetRoundIndex() == 0 && Drawer.GetDrawCount() == 0 && KickedKeys.Num() == 0;
}

int32 FSessionCore::DrawRoundSeed(uint32 RoundIndex)
{
	if (!bOpen)
	{
		return 0;
	}
	Seeds.SetRoundIndex(RoundIndex);
	return Drawer.DrawRoundSeed(RoundIndex);
}
