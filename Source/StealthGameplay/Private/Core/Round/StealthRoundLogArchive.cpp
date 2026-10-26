#include "Server/Core/Round/StealthRoundLogArchive.h"

void FRoundLogSliceRecorder::Append(const FRoundLogEntry& Entry)
{
	Current.Add(Entry);
	if (Downstream != nullptr)
	{
		Downstream->Append(Entry);
	}
}

void FRoundLogSliceRecorder::ArchiveCurrent(uint32 RoundIndex, uint32 RoundEpoch, ERoundEndReason EndReason, uint8 AbortCode, bool bReachedEnded)
{
	FRoundArchivedSlice& Slice = Archive.AddDefaulted_GetRef();
	Slice.RoundIndex = RoundIndex;
	Slice.RoundEpoch = RoundEpoch;
	Slice.EndReason = EndReason;
	Slice.AbortCode = AbortCode;
	Slice.bReachedEnded = bReachedEnded;
	Slice.Entries = MoveTemp(Current);
	Current.Reset();
}

void FRoundLogSliceRecorder::ResetForNewMatch()
{
	Current.Reset();
	Archive.Reset();
}
