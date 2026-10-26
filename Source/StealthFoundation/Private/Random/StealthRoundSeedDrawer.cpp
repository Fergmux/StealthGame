#include "Server/Random/StealthRoundSeedDrawer.h"

int32 FStealthRoundSeedDrawer::DrawRoundSeed(uint32 RoundIndex)
{
	// F5: Draw counts up from 0 for this round index until the seed is neither used this match nor golden. Over a
	// 99-round match a raw collision has probability ~2.3e-6, so the bound is never approached with a held secret.
	for (uint32 Draw = 0; Draw < 4096; ++Draw)
	{
		const int32 Candidate = Source.GetRoundSeedCandidate(RoundIndex, Draw);
		if (Candidate == 0)
		{
			return 0; // no secret held (or a refused call): never invent a seed
		}
		if (!Drawn.Contains(Candidate) && !GoldenSeeds.Contains(Candidate))
		{
			Drawn.Add(Candidate);
			return Candidate;
		}
	}
	return 0;
}

void FStealthRoundSeedDrawer::Reset()
{
	Drawn.Reset();
}
