#include "Server/Random/StealthRoundSeedDrawer.h"

int32 FStealthRoundSeedDrawer::DrawRoundSeed(uint32 RoundIndex)
{
	// The round index is folded into the attempt's high bits so two rounds never share an attempt value even if the
	// source's own round index is not advanced by the caller; the low bits are the match-wide attempt counter.
	for (int32 Guard = 0; Guard < 1024; ++Guard)
	{
		const uint32 Attempt = (RoundIndex << 20) ^ NextAttempt++;
		const int32 Candidate = static_cast<int32>(Source.GetServerSubSeed(ESeedDomain::Layout, Attempt) & 0x7FFFFFFFull);
		if (Candidate != 0 && !Drawn.Contains(Candidate))
		{
			Drawn.Add(Candidate);
			return Candidate;
		}
	}
	// Unreachable with a held secret (2^31 values); a source without a secret answers 0 every time.
	return 0;
}

void FStealthRoundSeedDrawer::Reset()
{
	Drawn.Reset();
	NextAttempt = 0;
}
