#include "Server/CrowdDirectorCore.h"

#include "View/StealthTuningPublicSubstructs.h"

FCrowdDirectorCore::FCrowdDirectorCore(const FCrowdTuning_Server& InTuning, uint64 BehaviourSubSeed)
	: Tuning(InTuning)
	, Draws(BehaviourSubSeed)
{
	DecisionPeriod = 1.0 / FMath::Max(static_cast<double>(Tuning.CrowdDecisionPerSec), 0.1);
}

// ---- Registry ------------------------------------------------------------------------------------------------------

FCrowdAgentRecord& FCrowdDirectorCore::RegisterAgent(uint32 Ordinal, ECrowdDriverKind Driver)
{
	FCrowdAgentRecord& Record = Agents.FindOrAdd(Ordinal);
	Record.Ordinal = Ordinal;
	Record.Driver = Driver;
	if (Phase == ECrowdDirectorPhase::Halted)
	{
		Record.Activity = ECrowdActivity::Halted;
	}
	return Record;
}

void FCrowdDirectorCore::UnregisterAgent(uint32 Ordinal)
{
	Agents.Remove(Ordinal);
}

FCrowdAgentRecord* FCrowdDirectorCore::FindAgent(uint32 Ordinal)
{
	return Agents.Find(Ordinal);
}

const FCrowdAgentRecord* FCrowdDirectorCore::FindAgent(uint32 Ordinal) const
{
	return Agents.Find(Ordinal);
}

TArray<uint32> FCrowdDirectorCore::GetOrdinals() const
{
	TArray<uint32> Ordinals;
	Agents.GetKeys(Ordinals);
	Ordinals.Sort();
	return Ordinals;
}

// ---- Phase gate ----------------------------------------------------------------------------------------------------

void FCrowdDirectorCore::AssignStratifiedPhases()
{
	// Stratified over the roster in ordinal order: pawn r of N gets (r + u) / N of the period, u from its own stream, so
	// each 1/N slice of the decision period holds exactly one pawn and no frame holds more than ceil(N / (Fps / f)) + 1.
	const TArray<uint32> Ordinals = GetOrdinals();
	const int32 Count = Ordinals.Num();
	for (int32 Rank = 0; Rank < Count; ++Rank)
	{
		FCrowdAgentRecord& Record = Agents[Ordinals[Rank]];
		const double U = Draws.DrawUnit(Record.Ordinal, ECrowdDecision::TickPhase);
		Record.GridPhase = (static_cast<double>(Rank) + U) / static_cast<double>(Count) * DecisionPeriod;
	}
}

bool FCrowdDirectorCore::StartRound(double Now, bool bNavReady, ICrowdAgentStarter& Starter)
{
	if (Phase != ECrowdDirectorPhase::Dormant)
	{
		return Phase == ECrowdDirectorPhase::Active;
	}
	if (!bNavReady)
	{
		// ADR-0016 section 7: a development failure. Everyone stays Dormant; no command is issued.
		bNavNotReady = true;
		return false;
	}
	bNavNotReady = false;
	Phase = ECrowdDirectorPhase::Active;
	GridOrigin = Now;
	AssignStratifiedPhases();
	// Every pawn starts in this one server step, each from its own drawn first activity (no unison).
	for (const uint32 Ordinal : GetOrdinals())
	{
		FCrowdAgentRecord& Record = Agents[Ordinal];
		Record.bStarted = true;
		const FCrowdFirstActivity First = DrawFirstActivity(Ordinal);
		Record.Activity = First.bPause ? ECrowdActivity::Pausing : ECrowdActivity::Routing;
		Starter.StartAgent(Ordinal, First, Now);
	}
	return true;
}

void FCrowdDirectorCore::JoinActiveRound(uint32 Ordinal, double Now, ICrowdAgentStarter& Starter)
{
	FCrowdAgentRecord* Record = Agents.Find(Ordinal);
	if (Record == nullptr || Phase != ECrowdDirectorPhase::Active || Record->bStarted)
	{
		return;
	}
	// A late joiner draws its own phase inside the period (it cannot take a stratum without moving everyone else).
	Record->GridPhase = Draws.DrawUnit(Ordinal, ECrowdDecision::TickPhase) * DecisionPeriod;
	Record->bStarted = true;
	const FCrowdFirstActivity First = DrawFirstActivity(Ordinal);
	Record->Activity = First.bPause ? ECrowdActivity::Pausing : ECrowdActivity::Routing;
	Starter.StartAgent(Ordinal, First, Now);
}

void FCrowdDirectorCore::Halt()
{
	Phase = ECrowdDirectorPhase::Halted;
	for (TPair<uint32, FCrowdAgentRecord>& Pair : Agents)
	{
		Pair.Value.Activity = ECrowdActivity::Halted;
	}
}

// ---- Decision grid -------------------------------------------------------------------------------------------------

double FCrowdDirectorCore::NextGridPoint(uint32 Ordinal, double Now, bool bInclusive) const
{
	const FCrowdAgentRecord* Record = Agents.Find(Ordinal);
	const double Origin = (GridOrigin >= 0.0 ? GridOrigin : 0.0) + (Record != nullptr ? Record->GridPhase : 0.0);
	if (Now < Origin)
	{
		return Origin;
	}
	const double K = FMath::FloorToDouble((Now - Origin) / DecisionPeriod);
	double Point = Origin + K * DecisionPeriod;
	// Guard the floating floor: the result is strictly after Now (or equal when inclusive).
	while (bInclusive ? Point < Now - 1e-9 : Point <= Now + 1e-9)
	{
		Point += DecisionPeriod;
	}
	return Point;
}

int32 FCrowdDirectorCore::DecisionBoundPerFrame(double Fps) const
{
	const double FramesPerPeriod = FMath::Max(Fps * DecisionPeriod, 1.0);
	return FMath::CeilToInt32(static_cast<double>(Agents.Num()) / FramesPerPeriod) + 2;
}

// ---- First activity ------------------------------------------------------------------------------------------------

double FCrowdDirectorCore::GetFirstPauseShare() const
{
	// F4: share of time standing = E[P] / (W + E[P]), W the mean walk segment (mean leg / walk speed at the mean pace).
	const double LongShare = FMath::Clamp(static_cast<double>(Tuning.CrowdLongPausePct) / 100.0, 0.0, 1.0);
	const double MeanShort = 0.5 * (Tuning.CrowdShortPauseMinSec + Tuning.CrowdShortPauseMaxSec);
	const double MeanLong = 0.5 * (Tuning.CrowdIdleMinSec + Tuning.CrowdIdleMaxSec);
	const double MeanPause = (1.0 - LongShare) * MeanShort + LongShare * MeanLong;
	const double FullShare = FMath::Clamp(static_cast<double>(Tuning.CrowdFullPacePct) / 100.0, 0.0, 1.0);
	const double MeanPace = FullShare + (1.0 - FullShare) * 0.85;
	const double WalkSpeed = FMath::Max(static_cast<double>(FLocomotionTuning_Public().WalkSpeedCmPerSec) * MeanPace, 1.0);
	const double MeanWalk = 0.5 * (Tuning.CrowdLegMinCm + Tuning.CrowdLegMaxCm) / WalkSpeed;
	return MeanPause / FMath::Max(MeanWalk + MeanPause, UE_DOUBLE_SMALL_NUMBER);
}

FCrowdFirstActivity FCrowdDirectorCore::DrawFirstActivity(uint32 Ordinal)
{
	FCrowdFirstActivity First;
	First.bPause = Draws.DrawChance(Ordinal, ECrowdDecision::FirstActivity, GetFirstPauseShare());
	if (First.bPause)
	{
		const bool bLong = Draws.DrawChance(Ordinal, ECrowdDecision::PauseKind, Tuning.CrowdLongPausePct / 100.0);
		First.PauseSec = bLong
			? Draws.DrawUniform(Ordinal, ECrowdDecision::PauseLength, Tuning.CrowdIdleMinSec, Tuning.CrowdIdleMaxSec)
			: Draws.DrawUniform(Ordinal, ECrowdDecision::PauseLength, Tuning.CrowdShortPauseMinSec, Tuning.CrowdShortPauseMaxSec);
	}
	else
	{
		First.LegCm = Draws.DrawUniform(Ordinal, ECrowdDecision::LegLength, Tuning.CrowdLegMinCm, Tuning.CrowdLegMaxCm);
	}
	return First;
}

// ---- Activity mirror -----------------------------------------------------------------------------------------------

void FCrowdDirectorCore::SetActivity(uint32 Ordinal, ECrowdActivity Activity)
{
	if (FCrowdAgentRecord* Record = Agents.Find(Ordinal))
	{
		Record->Activity = Phase == ECrowdDirectorPhase::Halted ? ECrowdActivity::Halted : Activity;
	}
}

ECrowdActivity FCrowdDirectorCore::GetActivity(uint32 Ordinal) const
{
	const FCrowdAgentRecord* Record = Agents.Find(Ordinal);
	return Record != nullptr ? Record->Activity : ECrowdActivity::Dormant;
}

// ---- Ledger --------------------------------------------------------------------------------------------------------

void FCrowdDirectorCore::NoteKill(double KillServerTime)
{
	KillTimes.Add(KillServerTime);
	++Vacancies;
}

void FCrowdDirectorCore::NoteAmbientExit()
{
	++Vacancies;
}

void FCrowdDirectorCore::NoteVacancyServed()
{
	Vacancies = FMath::Max(0, Vacancies - 1);
}

void FCrowdDirectorCore::NoteTakeover()
{
	++TakenOver;
}

void FCrowdDirectorCore::NoteTakeoverReversed()
{
	TakenOver = FMath::Max(0, TakenOver - 1);
}

void FCrowdDirectorCore::NoteRelease()
{
	++Released;
}
