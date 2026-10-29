// Story con-030: Concealment's metric calculators (F1, F2, CON-R12, CON-R15). See Server/ConcealmentMetrics.h.

#include "Server/ConcealmentMetrics.h"

double ConcealmentMetrics::ExpectedOccupancy(const FOccupancyInputs& In)
{
	if (In.NumBushes <= 0.0)
	{
		return 0.0;
	}
	return (In.NpcCount * In.NpcRatePerMin * In.NpcDwellMin + In.PlayerCount * In.PlayerRatePerMin * In.PlayerDwellMin) / In.NumBushes;
}

double ConcealmentMetrics::MultiOccupantBaseline(double Lambda)
{
	return 1.0 - FMath::Exp(-FMath::Max(Lambda, 0.0));
}

double ConcealmentMetrics::ReviewCap(double M0)
{
	return FMath::Max(ReviewCapFloor, M0 + ReviewCapMarginPoints);
}

double ConcealmentMetrics::RequiredNpcRate(double PlayerRatePerMin, double PlayerCount, double NpcCount, double K)
{
	const double B = PlayerCount + NpcCount > 0.0 ? PlayerCount / (PlayerCount + NpcCount) : 0.0;
	const double Denominator = K * B * NpcCount;
	return Denominator > 0.0 ? PlayerRatePerMin * PlayerCount * (1.0 - K * B) / Denominator : 0.0;
}

ConcealmentMetrics::FPlayerConcealment ConcealmentMetrics::SummarisePlayer(TConstArrayView<FConcealmentLogRow> Rows, FParticipantId Player,
	double RoundStartServerTime, double RoundSec)
{
	FPlayerConcealment Out;
	double OpenSince = -1.0;
	for (const FConcealmentLogRow& Row : Rows)
	{
		if (Row.Participant != Player)
		{
			continue;
		}
		switch (Row.Event)
		{
		case EConcealmentLogEvent::Commit:
			++Out.Entries;
			OpenSince = Row.ServerTime;
			break;
		case EConcealmentLogEvent::ExitCross:
		case EConcealmentLogEvent::Emerge:
		case EConcealmentLogEvent::Killed:
			if (OpenSince >= 0.0)
			{
				Out.TimeConcealedSec += FMath::Max(Row.ServerTime - OpenSince, 0.0);
				OpenSince = -1.0;
			}
			break;
		default:
			break;
		}
	}
	if (OpenSince >= 0.0)
	{
		Out.TimeConcealedSec += FMath::Max(RoundStartServerTime + RoundSec - OpenSince, 0.0); // still concealed at the round end
	}
	if (RoundSec > 0.0)
	{
		Out.ShareOfRound = Out.TimeConcealedSec / RoundSec;
		Out.RatePerMin = Out.Entries / (RoundSec / 60.0);
	}
	return Out;
}

ConcealmentMetrics::FMultiOccupantShare ConcealmentMetrics::MultiOccupantShare(TConstArrayView<FConcealmentLogRow> Rows, double M0)
{
	FMultiOccupantShare Out;
	for (const FConcealmentLogRow& Row : Rows)
	{
		if (Row.Event != EConcealmentLogEvent::BushStrike || Row.CandidatesAfterReach <= 0)
		{
			continue;
		}
		++Out.StrikesWithCandidates;
		if (Row.CandidatesAfterReach >= 2)
		{
			++Out.MultiCandidate;
		}
	}
	Out.Share = Out.StrikesWithCandidates > 0 ? static_cast<double>(Out.MultiCandidate) / Out.StrikesWithCandidates : 0.0;
	if (Out.StrikesWithCandidates < MinStrikesWithCandidates)
	{
		Out.Verdict = EReviewVerdict::Inconclusive;
	}
	else
	{
		Out.Verdict = Out.Share > ReviewCap(M0) ? EReviewVerdict::Review : EReviewVerdict::WithinCap;
	}
	return Out;
}

ConcealmentMetrics::FAmbushShare ConcealmentMetrics::AmbushShareFromCounts(int32 EmergeKills, int32 TotalKills)
{
	FAmbushShare Out;
	Out.EmergeKills = EmergeKills;
	Out.TotalKills = TotalKills;
	if (TotalKills <= 0)
	{
		return Out; // Inconclusive
	}
	Out.Share = static_cast<double>(EmergeKills) / TotalKills;
	Out.Verdict = Out.Share > AmbushReviewShare ? EReviewVerdict::Review : EReviewVerdict::WithinCap;
	return Out;
}

ConcealmentMetrics::FAmbushShare ConcealmentMetrics::AmbushShare(TConstArrayView<FConcealmentLogRow> Rows, int32 TotalKills)
{
	int32 EmergeKills = 0;
	for (const FConcealmentLogRow& Row : Rows)
	{
		EmergeKills += Row.Event == EConcealmentLogEvent::Emerge && Row.bKilled ? 1 : 0;
	}
	return AmbushShareFromCounts(EmergeKills, TotalKills);
}
