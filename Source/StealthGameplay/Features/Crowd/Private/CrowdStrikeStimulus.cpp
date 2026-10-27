// Story cnb-024: strike stimulus fan-out (GDD CNB-R8; ADR-0016 section 5). See CrowdStrikeStimulus.h.

#include "Server/CrowdStrikeStimulus.h"

#include "Server/CrowdDraws.h"

double FCrowdStrikeFanOut::PriorityOf(const FCrowdDraws& Draws, uint32 Ordinal, uint64 StrikeIndex)
{
	// Keyed by (ordinal, strike index): the strike index is the tuple's decision index, read without advancing the
	// pawn's own StrikePriority counter, so a strike never shifts any other draw (CNB-R14).
	return FCrowdDraws::UnitFraction53(Draws.PeekTuple(Ordinal, ECrowdDecision::StrikePriority, static_cast<uint32>(StrikeIndex), 0));
}

void FCrowdStrikeFanOut::Begin(double StimulusTime, const FVector& Point, uint64 SourceKey, TArray<FCrowdStrikeCandidate> Candidates, FCrowdDraws& Draws)
{
	FCrowdPendingStrike& Strike = Pending.AddDefaulted_GetRef();
	Strike.StrikeIndex = NextStrikeIndex++;
	Strike.StimulusTime = StimulusTime;
	Strike.Point = Point;
	Strike.SourceKey = SourceKey;
	for (FCrowdStrikeCandidate& Candidate : Candidates)
	{
		Candidate.Priority = PriorityOf(Draws, Candidate.Ordinal, Strike.StrikeIndex);
	}
	Candidates.Sort([](const FCrowdStrikeCandidate& A, const FCrowdStrikeCandidate& B)
	{
		return A.Priority != B.Priority ? A.Priority < B.Priority : A.Ordinal < B.Ordinal;
	});
	Strike.Candidates = MoveTemp(Candidates);
}

int32 FCrowdStrikeFanOut::Process(int32 RayBudget, int32 ResponderLimit, TFunctionRef<bool(const FCrowdPendingStrike&, const FCrowdStrikeCandidate&)> Visible,
	TFunctionRef<void(const FCrowdPendingStrike&, const FCrowdStrikeCandidate&)> Respond)
{
	int32 Used = 0;
	while (Pending.Num() > 0)
	{
		FCrowdPendingStrike& Strike = Pending[0];
		while (Strike.Responders < ResponderLimit && Strike.Next < Strike.Candidates.Num() && Used < RayBudget)
		{
			const FCrowdStrikeCandidate& Candidate = Strike.Candidates[Strike.Next++];
			++Used;
			if (Visible(Strike, Candidate))
			{
				++Strike.Responders;
				Respond(Strike, Candidate);
			}
		}
		if (Strike.Responders >= ResponderLimit || Strike.Next >= Strike.Candidates.Num())
		{
			Pending.RemoveAt(0);
			continue;
		}
		break; // the ray budget is spent: the strike resumes next frame with its stimulus time
	}
	return Used;
}
