#include "StealthPenaltyCore.h"

#include "Server/Queue/StealthResolutionQueueCore.h"
#include "Server/StealthClock.h"
#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningInvariants.h"

// Stories nkp-001 (store, lifetime), nkp-002 (strike rule, release grace), nkp-003 (tally, bExiled, owner re-push),
// nkp-004 (the StrikeCounted row), nkp-005..008 (exile, informant kill), nkp-009 (F2, scheduling, cancellation), nkp-010
// (Absent, readmission, lost body) and nkp-012 (the respawn commit). GDD npc-kill-penalty-informants.md PEN-R2..R11, F1,
// F2; ADR-0019 sections 1-4, 7. Server-only values: nothing here formats, logs or traces a record, a count or a row.

namespace StealthPenaltyCoreLocal
{
	const FName FaultSource(TEXT("PenaltyCore"));

	/** The saturating uint8 add of the tally (F1: t' = min(t + 1, 255)). */
	uint8 SaturatingIncrement(uint8 Value)
	{
		return Value == MAX_uint8 ? Value : static_cast<uint8>(Value + 1);
	}
}

FPenaltyParams FPenaltyParams::FromTuning(const FStealthServerTuning& Values)
{
	FPenaltyParams Out;
	Out.NpcKillStrikeLimit = Values.Penalty_Public.NpcKillStrikeLimit;
	Out.StrikeOutReleaseGraceSec = static_cast<double>(Values.Penalty_Server.StrikeOutReleaseGraceSec);
	Out.InformantRespawnDelaySec = static_cast<double>(Values.Penalty_Public.InformantRespawnDelaySec);
	Out.DeathFallSec = static_cast<double>(Values.Assassination_Public.DeathFallSec);
	// Owned elsewhere and not registered yet (TD-TDC-I3): Camera's NoPawnFadeSec, LSC's EmergenceRetrySec.
	Out.NoPawnFadeSec = StealthTuningPendingOwnerKeys::NoPawnFadeSec;
	Out.EmergenceRetrySec = StealthTuningPendingOwnerKeys::EmergenceRetrySec;
	return Out;
}

FPenaltyCore::FPenaltyCore(const FPenaltyCoreDeps& InDeps)
	: Deps(InDeps)
	, EventLog(InDeps.Log, InDeps.Slot, InDeps.Clock, InDeps.Faults)
{
	if (Deps.Tuning != nullptr)
	{
		Latched = Deps.Tuning->ReadPenaltyParams();
	}
	RoundEpoch = Deps.Roster != nullptr ? Deps.Roster->GetRoundEpoch() : 0;
}

// ---- Round lifetime ------------------------------------------------------------------------------------------------

void FPenaltyCore::OnRoundPreparing()
{
	// TDC rule CR-11 Latching: the only read of the live knobs; nothing reads them again until the next Preparing.
	if (Deps.Tuning != nullptr)
	{
		Latched = Deps.Tuning->ReadPenaltyParams();
	}
}

void FPenaltyCore::OnRosterRoundEnded()
{
	if (!bFrozen)
	{
		// ADR-0019 section 3.3 "any -> RoundEnded": every pending item cancelled, one RespawnCancelled{RoundEnded} row each
		// (written inside the end handling, before the freeze), and T_due unset (the carrier adapter writes nothing: latched).
		TArray<FParticipantId> Ids;
		Records.GetKeys(Ids);
		for (const FParticipantId& Id : Ids)
		{
			FPenaltyRecord& Record = Records.FindChecked(Id);
			const bool bPending = Record.LifeState == EInformantLifeState::Awaiting || Record.LifeState == EInformantLifeState::Picking;
			const bool bHadDue = Record.DueServerTime > 0.0;
			CancelRequest(Record);
			if (bPending)
			{
				EventLog.Write(FPenaltyRespawnCancelledRow{ Id, ERespawnCancelReason::RoundEnded }, RoundEpoch);
			}
			if (bHadDue)
			{
				RaiseDueTime(Id, TOptional<double>());
			}
		}
	}
	bFrozen = true;
	for (TPair<FParticipantId, FPenaltyRecord>& Pair : Records)
	{
		Pair.Value.LifeState = EInformantLifeState::RoundEnded;
	}
	PendingStrikeRow.Reset();
}

void FPenaltyCore::OnRosterReset(uint32 NewRoundEpoch)
{
	// ADR-0019 section 3.3 "any -> Active(0)": a request still pending (a reset without a round end) is cancelled with one
	// RespawnCancelled{Reset} row under the old epoch; a leftover item that still fires is the queue's Rejected_StaleEpoch.
	{
		TArray<FParticipantId> Ids;
		Records.GetKeys(Ids);
		for (const FParticipantId& Id : Ids)
		{
			FPenaltyRecord& Record = Records.FindChecked(Id);
			const bool bPending = Record.LifeState == EInformantLifeState::Awaiting || Record.LifeState == EInformantLifeState::Picking;
			const bool bHadDue = Record.DueServerTime > 0.0;
			CancelRequest(Record);
			if (bPending)
			{
				EventLog.Write(FPenaltyRespawnCancelledRow{ Id, ERespawnCancelReason::Reset }, RoundEpoch);
			}
			if (bHadDue)
			{
				RaiseDueTime(Id, TOptional<double>());
			}
		}
	}
	bFrozen = false;
	RoundEpoch = NewRoundEpoch;
	DyingBodyDeathTimes.Reset();
	NextRespawnRequestSeq = 1;
	for (TPair<FParticipantId, FPenaltyRecord>& Pair : Records)
	{
		Pair.Value = FPenaltyRecord(); // Active(0), tally 0, no respawn fields, no excluded pawn
	}
	ReleasedBodies.Reset();
	PendingStrikeRow.Reset();
	SlotExile.Reset();
	// ADR-0019 section 2.2a: the reset is one of the three re-push points (the display shows 0 for the new epoch).
	for (const TPair<FParticipantId, FPenaltyRecord>& Pair : Records)
	{
		RepushOwnerTally(Pair.Key, Pair.Value);
	}
}

// ---- Connection ----------------------------------------------------------------------------------------------------

void FPenaltyCore::OnConnectionChanged(FParticipantId Participant, bool bConnected)
{
	// PEN-R6: a disconnect or a return never touches the counters. A return is Round R18's readmission (OnInformantReadmitted).
	if (bFrozen || bConnected)
	{
		return;
	}
	FPenaltyRecord* Record = Records.Find(Participant);
	if (Record == nullptr)
	{
		return;
	}
	switch (Record->LifeState)
	{
	case EInformantLifeState::Awaiting:
	case EInformantLifeState::Picking:
	{
		// Awaiting / Picking -> Absent: Cancel(TimerHandle), t_lost and t_floor kept (PEN-R10, AC-NKP-11).
		const bool bHadDue = Record->DueServerTime > 0.0;
		CancelRequest(*Record);
		Record->LifeState = EInformantLifeState::Absent;
		EventLog.Write(FPenaltyRespawnCancelledRow{ Participant, ERespawnCancelReason::Disconnect }, RoundEpoch);
		if (bHadDue)
		{
			RaiseDueTime(Participant, TOptional<double>());
		}
		break;
	}
	case EInformantLifeState::Embodied:
	{
		// Embodied -> Absent (Round R17.5 releases the pawn to the crowd in this slot): t_lost = t_floor = now, no item.
		const double Now = NowServerTime();
		CancelRequest(*Record);
		Record->LifeState = EInformantLifeState::Absent;
		Record->LostServerTime = Now;
		Record->FloorServerTime = Now;
		Record->RespawnDelaySec = Latched.InformantRespawnDelaySec;
		break;
	}
	default:
		break; // Active: the Absent rule is applied when a cause arrives (PEN-R8); Absent / RoundEnded: nothing
	}
}

void FPenaltyCore::OnParticipantReclaimed(FParticipantId Participant)
{
	const FPenaltyRecord* Record = Records.Find(Participant);
	RepushOwnerTally(Participant, Record != nullptr ? *Record : FPenaltyRecord());
}

void FPenaltyCore::OnViewRebound(FParticipantId Participant)
{
	const FPenaltyRecord* Record = Records.Find(Participant);
	RepushOwnerTally(Participant, Record != nullptr ? *Record : FPenaltyRecord());
}

void FPenaltyCore::OnInformantReadmitted(FParticipantId Participant)
{
	// PEN-R6: counters unchanged. PEN-R11 / ADR-0019 section 3.3 "Absent -> Awaiting / Picking".
	if (bFrozen || !Participant.IsValid())
	{
		return;
	}
	const double Now = NowServerTime();
	FPenaltyRecord& Record = FindOrAddRecord(Participant);
	if (Record.LifeState == EInformantLifeState::Active)
	{
		// An Informant this store never saw leave (no cause reached it): the loss is the readmission itself.
		Record.LifeState = EInformantLifeState::Absent;
		Record.LostServerTime = Now;
		Record.FloorServerTime = Now;
		Record.RespawnDelaySec = Latched.InformantRespawnDelaySec;
	}
	if (Record.LifeState != EInformantLifeState::Absent)
	{
		return; // Awaiting / Picking / Embodied: already on its way, a duplicate readmission changes nothing
	}
	// The KEPT t_lost and t_floor: reclaiming never shortens the wait and never restarts it (F2: t_now only pushes it later).
	const double Due = ComputeDueTime(Record.LostServerTime, Record.RespawnDelaySec, Record.FloorServerTime, Now);
	if (Now < Due)
	{
		ArmRequest(Participant, ERespawnCause::Readmitted, Due);
	}
	else
	{
		EventLog.Write(FPenaltyRespawnScheduledRow{ Participant, ERespawnCause::Readmitted, Record.LostServerTime, Due }, RoundEpoch);
		StartPickingNow(Participant, Due);
	}
}

// ---- Respawn lifecycle ------------------------------------------------------------------------------------------------

double FPenaltyCore::ComputeDueTime(double LostServerTime, double DelaySec, double FloorServerTime, double ServerTimeNow)
{
	return FMath::Max3(LostServerTime + DelaySec, FloorServerTime, ServerTimeNow);
}

double FPenaltyCore::ComputeFloor(TOptional<double> DeathServerTime, double DeathFallSec, double NoPawnFadeSec, double LostServerTime)
{
	return DeathServerTime.IsSet() ? DeathServerTime.GetValue() + DeathFallSec + NoPawnFadeSec : LostServerTime;
}

double FPenaltyCore::NextFrameRetryOffsetSec()
{
	return 2.0 * FResolutionQueueCore::DueEpsilonSec;
}

void FPenaltyCore::OnInformantAwaitingBody(FParticipantId Participant, ERespawnCause Cause, double LostServerTime)
{
	if (bFrozen || !Participant.IsValid())
	{
		return;
	}
	TOptional<double> Death;
	if (Cause == ERespawnCause::Assassinated || Cause == ERespawnCause::ReversedAgainst)
	{
		// The dying body's death write (ADR-0018 section 4.1, valid from the kill slot), recorded by this slot's OnPawnKilled.
		double DeathTime = 0.0;
		if (DyingBodyDeathTimes.RemoveAndCopyValue(Participant, DeathTime))
		{
			Death = DeathTime;
		}
	}
	ScheduleRespawn(Participant, Cause, LostServerTime, Death);
}

void FPenaltyCore::OnInformantPawnLost(FParticipantId Participant, EPenaltyPawnLostCause Cause)
{
	FPenaltyRecord* Record = bFrozen ? nullptr : Records.Find(Participant);
	if (Record == nullptr || Record->LifeState != EInformantLifeState::Embodied)
	{
		return; // only an embodied informant has a body to lose; a second report is a no-op
	}
	const double Now = NowServerTime();
	Record->LostServerTime = Now;
	Record->FloorServerTime = Now;
	Record->RespawnDelaySec = 0.0; // F2: D = 0 for a body lost without a kill
	Record->RespawnCause = ERespawnCause::PawnLost;
	EventLog.Write(FPenaltyPawnLostRow{ Participant, Cause }, RoundEpoch);
	if (IsDisconnected(Participant))
	{
		Record->LifeState = EInformantLifeState::Absent;
		return;
	}
	StartPickingNow(Participant, Now);
}

void FPenaltyCore::OnGraceExpired(FParticipantId Participant)
{
	if (bFrozen || !Participant.IsValid())
	{
		return;
	}
	const double Now = NowServerTime();
	FPenaltyRecord& Record = FindOrAddRecord(Participant);
	CancelRequest(Record);
	Record.LifeState = EInformantLifeState::Absent;
	Record.LostServerTime = Now;
	Record.FloorServerTime = Now;
	Record.RespawnDelaySec = Latched.InformantRespawnDelaySec;
}

void FPenaltyCore::ScheduleRespawn(FParticipantId Participant, ERespawnCause Cause, double LostServerTime, TOptional<double> DeathServerTime)
{
	if (bFrozen || !Participant.IsValid())
	{
		return;
	}
	FPenaltyRecord& Record = FindOrAddRecord(Participant);
	const bool bHadDue = Record.DueServerTime > 0.0;
	CancelRequest(Record);
	const bool bBodyFalls = Cause == ERespawnCause::Assassinated || Cause == ERespawnCause::ReversedAgainst || Cause == ERespawnCause::InformantKilled;
	Record.LostServerTime = LostServerTime;
	Record.RespawnDelaySec = Cause == ERespawnCause::PawnLost ? 0.0 : Latched.InformantRespawnDelaySec;
	Record.FloorServerTime = ComputeFloor(bBodyFalls ? DeathServerTime : TOptional<double>(), Latched.DeathFallSec, Latched.NoPawnFadeSec, LostServerTime);
	Record.RespawnCause = Cause;
	if (IsDisconnected(Participant))
	{
		// PEN-R8 / S-B3: a cause on a Disconnected participant keeps t_lost and t_floor and schedules nothing (Absent).
		Record.LifeState = EInformantLifeState::Absent;
		if (bHadDue)
		{
			RaiseDueTime(Participant, TOptional<double>());
		}
		return;
	}
	ArmRequest(Participant, Cause, ComputeDueTime(LostServerTime, Record.RespawnDelaySec, Record.FloorServerTime, NowServerTime()));
}

void FPenaltyCore::ArmRequest(FParticipantId Participant, ERespawnCause Cause, double DueServerTime)
{
	FPenaltyRecord& Record = FindOrAddRecord(Participant);
	Record.RespawnRequestSeq = NextRespawnRequestSeq++;
	Record.DueServerTime = DueServerTime;
	Record.RespawnCause = Cause;
	Record.LifeState = EInformantLifeState::Awaiting;
	Record.TimerHandle = ScheduleItem(Participant, Record, DueServerTime);
	EventLog.Write(FPenaltyRespawnScheduledRow{ Participant, Cause, Record.LostServerTime, DueServerTime }, RoundEpoch);
	RaiseDueTime(Participant, DueServerTime);
}

void FPenaltyCore::StartPickingNow(FParticipantId Participant, double DueServerTime)
{
	FPenaltyRecord& Record = FindOrAddRecord(Participant);
	CancelRequest(Record);
	Record.RespawnRequestSeq = NextRespawnRequestSeq++;
	Record.DueServerTime = DueServerTime;
	Record.LifeState = EInformantLifeState::Picking;
	RunCommit(Participant);
}

bool FPenaltyCore::ResolveRespawnItem(FParticipantId Participant, uint32 RequestSeq, uint32 ItemRoundEpoch)
{
	FPenaltyRecord* Record = Records.Find(Participant);
	if (bFrozen || ItemRoundEpoch != RoundEpoch || Record == nullptr || Record->RespawnRequestSeq != RequestSeq || RequestSeq == 0
		|| (Record->LifeState != EInformantLifeState::Awaiting && Record->LifeState != EInformantLifeState::Picking))
	{
		return false; // stale (another request, a cancelled one, another round): changed nothing
	}
	Record->TimerHandle = 0;
	if (Record->bClearExclusionsAtFire)
	{
		// ADR-0019 section 4.1 step 3: a regular retry clears the exclusion set (a transiently ineligible pawn can return).
		Record->RequestExclusions.Reset();
		Record->bClearExclusionsAtFire = false;
	}
	Record->LifeState = EInformantLifeState::Picking;
	RunCommit(Participant);
	return true;
}

void FPenaltyCore::RunCommit(FParticipantId Participant)
{
	// ADR-0019 section 4.1, one call stack. Every gateway call may dispatch Roster events that reach this store, so the
	// record is looked up again after each one (no reference is held across a call).
	const double Now = NowServerTime();
	FPenaltyRecord* Record = Records.Find(Participant);
	if (Record == nullptr || bFrozen)
	{
		return; // the round is over: OnRosterRoundEnded already cancelled and logged
	}

	// 1. Re-validate: Roster state Informant, no live pawn, connection not Disconnected (phase and epoch: the freeze and the
	//    item's epoch check).
	if (IsDisconnected(Participant))
	{
		VoidRequest(Participant, ERespawnCancelReason::Disconnect, EInformantLifeState::Absent);
		return;
	}
	const TOptional<EParticipantState> State = Deps.Roster != nullptr ? Deps.Roster->GetState(Participant) : TOptional<EParticipantState>();
	const bool bLivePawn = Deps.Roster != nullptr && Deps.Roster->HasLivePawn(Participant);
	if (!State.IsSet() || State.GetValue() != EParticipantState::Informant || bLivePawn)
	{
		VoidRequest(Participant, ERespawnCancelReason::Stale, bLivePawn ? EInformantLifeState::Embodied : EInformantLifeState::Active);
		return;
	}
	Record->LifeState = EInformantLifeState::Picking;
	if (Record->PickStartServerTime <= 0.0)
	{
		Record->PickStartServerTime = Now;
	}
	if (Deps.Respawn == nullptr)
	{
		ScheduleRetry(Participant, Now + Latched.EmergenceRetrySec, /*bRegular*/ true); // no gateway yet: keep waiting
		return;
	}
	const uint32 RequestSeq = Record->RespawnRequestSeq;
	const uint32 Epoch = RoundEpoch;
	TArray<FPenaltyPawnKey, TInlineAllocator<8>> Exclude;
	if (Record->ExcludePawn.IsSet())
	{
		Exclude.Add(Record->ExcludePawn); // the participant's own released exile body (LSC R5b.1, PEN-R5)
	}
	Exclude.Append(Record->RequestExclusions);
	const ERosterTeam Team = Deps.Roster != nullptr ? Deps.Roster->GetTeam(Participant) : ERosterTeam::None;

	// 2. Pick (the only random draw; at most one pick per slot).
	const FPenaltyRespawnPick Pick = Deps.Respawn->PickRespawnPawn(Team, RequestSeq, TConstArrayView<FPenaltyPawnKey>(Exclude.GetData(), Exclude.Num()));
	if (Pick.Status == EPenaltyPickStatus::Deferred)
	{
		ScheduleRetry(Participant, Now + NextFrameRetryOffsetSec(), /*bRegular*/ false); // the per-frame guard: next frame
		return;
	}
	if (Pick.Status != EPenaltyPickStatus::Picked || !Pick.Pawn.IsSet())
	{
		ScheduleRetry(Participant, Now + Latched.EmergenceRetrySec, /*bRegular*/ true); // None (story nkp-013 widens)
		return;
	}
	if (FPenaltyRecord* Picked = Records.Find(Participant))
	{
		Picked->LastWideningLevel = Pick.WideningLevel;
	}
	const auto ExcludeAndRetryNextFrame = [this, Participant, Now](FPenaltyPawnKey Pawn)
	{
		if (FPenaltyRecord* Current = Records.Find(Participant))
		{
			Current->RequestExclusions.AddUnique(Pawn);
		}
		ScheduleRetry(Participant, Now + NextFrameRetryOffsetSec(), /*bRegular*/ false);
	};

	// 3. Pre-check (side-effect free; the same predicate as TakeOverCrowdPawn's step 1).
	if (!Deps.Respawn->CheckTakeOverEligibility(Pick.Pawn))
	{
		ExcludeAndRetryNextFrame(Pick.Pawn);
		return;
	}
	// 4. Crowd release (atomic Eligible re-check; Penalty is its only caller).
	if (!Deps.Respawn->TryReleaseNpc(Pick.Pawn, Epoch))
	{
		ExcludeAndRetryNextFrame(Pick.Pawn);
		return;
	}
	// 5. Takeover (the factory binds on Penalty's behalf; Penalty never calls BindPawn).
	const EPenaltyTakeoverResult Takeover = Deps.Respawn->TakeOverCrowdPawn(Pick.Pawn, Participant, Epoch);
	if (Takeover != EPenaltyTakeoverResult::Applied)
	{
		// The released pawn goes back to the crowd in this same stack; no card ever moved.
		Deps.Respawn->AdoptReleasedPawn(Pick.Pawn);
		if (Takeover == EPenaltyTakeoverResult::NotEligible)
		{
			// Steps 3-5 run with nothing in between that can change the pawn: a defect.
			ReportFault(EStealthFault::InvariantViolation, PenaltyFaultDetail::TakeoverNotEligibleAfterPrecheck);
			ExcludeAndRetryNextFrame(Pick.Pawn);
			return;
		}
		VoidRequest(Participant, ERespawnCancelReason::Stale, EInformantLifeState::Active); // a stale bind: the request is void
		return;
	}
	// 6. Card, only after Applied; a failed card move leaves the takeover standing (no rollback).
	const TOptional<uint16> Card = Deps.Respawn->TakeNpcCard(Participant, Pick.Pawn);
	if (Card.IsSet())
	{
		Deps.Respawn->RecordAppearance(Participant, Card.GetValue());
	}
	else
	{
		ReportFault(EStealthFault::InvariantViolation, PenaltyFaultDetail::LedgerMismatch);
	}
	// 7. Bookkeeping.
	FPenaltyRecord* Done = Records.Find(Participant);
	if (Done == nullptr)
	{
		return;
	}
	const double Latency = FMath::Max(0.0, NowServerTime() - Done->DueServerTime);
	const uint8 Widening = Done->LastWideningLevel;
	CancelRequest(*Done);
	Done->LifeState = EInformantLifeState::Embodied;
	Done->ExcludePawn = FPenaltyPawnKey();
	EventLog.Write(FPenaltyInformantRespawnRow{ Participant, Latency, Widening }, RoundEpoch);
	RaiseDueTime(Participant, TOptional<double>());
}

void FPenaltyCore::ScheduleRetry(FParticipantId Participant, double DueServerTime, bool bRegular)
{
	FPenaltyRecord* Record = Records.Find(Participant);
	if (Record == nullptr)
	{
		return;
	}
	// The same request: the same RespawnRequestSeq (the LSC stream continues), T_due unchanged (the latency's origin).
	Record->LifeState = EInformantLifeState::Picking;
	Record->bClearExclusionsAtFire = bRegular;
	Record->TimerHandle = ScheduleItem(Participant, *Record, DueServerTime);
}

void FPenaltyCore::CancelRequest(FPenaltyRecord& Record)
{
	if (Record.TimerHandle != 0 && Deps.Timers != nullptr)
	{
		Deps.Timers->CancelRespawnItem(Record.TimerHandle);
	}
	Record.TimerHandle = 0;
	Record.DueServerTime = 0.0;
	Record.RespawnRequestSeq = 0;
	Record.PickStartServerTime = 0.0;
	Record.LastWideningLevel = 0;
	Record.RequestExclusions.Reset();
	Record.bClearExclusionsAtFire = false;
}

void FPenaltyCore::VoidRequest(FParticipantId Participant, ERespawnCancelReason Reason, EInformantLifeState NewState)
{
	FPenaltyRecord* Record = Records.Find(Participant);
	if (Record == nullptr)
	{
		return;
	}
	const bool bHadDue = Record->DueServerTime > 0.0;
	CancelRequest(*Record);
	Record->LifeState = NewState;
	EventLog.Write(FPenaltyRespawnCancelledRow{ Participant, Reason }, RoundEpoch);
	if (bHadDue)
	{
		RaiseDueTime(Participant, TOptional<double>());
	}
}

uint64 FPenaltyCore::ScheduleItem(FParticipantId Participant, const FPenaltyRecord& Record, double DueServerTime)
{
	const uint64 Handle = Deps.Timers != nullptr
		? Deps.Timers->ScheduleRespawnItem(DueServerTime, FPenaltyRespawnItem{ Participant, Record.RespawnRequestSeq, RoundEpoch, Record.RespawnCause })
		: 0;
	if (Handle == 0)
	{
		ReportFault(EStealthFault::DeadRegistration, PenaltyFaultDetail::RespawnNotScheduled);
	}
	return Handle;
}

void FPenaltyCore::RaiseDueTime(FParticipantId Participant, TOptional<double> DueServerTime)
{
	if (Deps.DueTimes != nullptr)
	{
		Deps.DueTimes->OnDueTimeChanged(Participant, DueServerTime);
	}
}

bool FPenaltyCore::IsDisconnected(FParticipantId Participant) const
{
	return Deps.Roster != nullptr && Deps.Roster->IsDisconnected(Participant);
}

double FPenaltyCore::NowServerTime() const
{
	return Deps.Clock != nullptr ? Deps.Clock->GetServerTimeSeconds() : 0.0;
}

// ---- Strikes and tally ----------------------------------------------------------------------------------------------

EPenaltyKillOutcome FPenaltyCore::OnPawnKilled(const FPenaltyKill& Kill)
{
	if (bFrozen)
	{
		return EPenaltyKillOutcome::NoStrike; // frozen at round end: nothing is counted, released or retired
	}
	const double SlotServerTime = Deps.Clock != nullptr ? Deps.Clock->GetServerTimeSeconds() : 0.0;
	if (Kill.VictimKind == EVictimKind::Player && Kill.VictimParticipant.IsValid() && Kill.VictimDeathServerTime > 0.0)
	{
		// A participant's body is dying (Assassinated / ReversedAgainst): its death write time is the F2 floor of the respawn
		// Round asks for after its elimination check, in this slot's Roster flush (story nkp-009).
		DyingBodyDeathTimes.Add(Kill.VictimParticipant, Kill.VictimDeathServerTime);
	}
	if (Kill.Cause == EKillCause::Strike && Kill.VictimKind == EVictimKind::Informant)
	{
		// PEN-R7 / ADR-0019 section 2.1 (story nkp-008): every informant kill retires its binding inline, whoever struck.
		RetireInformant(Kill, SlotServerTime);
	}
	if (!Kill.Killer.IsValid())
	{
		// A killer with no participant (a bracing NPC's reversal): nothing, and no report.
		return EPenaltyKillOutcome::NoStrike;
	}
	const TOptional<EParticipantState> KillerState = Deps.Roster != nullptr ? Deps.Roster->GetState(Kill.Killer) : TOptional<EParticipantState>();
	if (KillerState.IsSet() && KillerState.GetValue() == EParticipantState::Informant)
	{
		// ASN-R2 voids an attacker that left Active earlier in the drain and an informant cannot brace: a defect.
		ReportFault(EStealthFault::InvariantViolation, PenaltyFaultDetail::InformantKiller);
		return EPenaltyKillOutcome::InformantKillerFault;
	}
	if (!KillerState.IsSet() || KillerState.GetValue() != EParticipantState::Active || Kill.Cause != EKillCause::Strike
		|| Kill.VictimKind == EVictimKind::Player)
	{
		// A reversal never charges anyone (ASN-R11); a Player victim is the Roster's (Assassinated) event.
		return EPenaltyKillOutcome::NoStrike;
	}

	FPenaltyRecord& Record = FindOrAddRecord(Kill.Killer);
	const bool bGraced = IsGraced(Kill.Victim, SlotServerTime);
	const FQueueKey SlotKey = CurrentSlotKey();
	EPenaltyKillOutcome Outcome = EPenaltyKillOutcome::NoStrike;
	if (Kill.VictimKind == EVictimKind::Npc)
	{
		if (bGraced)
		{
			Outcome = EPenaltyKillOutcome::Graced;
		}
		else
		{
			// F1: k' = k + [Cause = Strike AND VK = Npc AND S = Active AND NOT G].
			Record.StrikeCount = StealthPenaltyCoreLocal::SaturatingIncrement(Record.StrikeCount);
			Outcome = EPenaltyKillOutcome::Strike;
			if (static_cast<int32>(Record.StrikeCount) >= Latched.NpcKillStrikeLimit)
			{
				// F1 Exile = (k' >= L) AND (S = Active), L the latched value (never the live knob).
				Outcome = EPenaltyKillOutcome::StrikeAndExile;
			}
		}
	}
	// An informant victim (VictimKind = Informant) adds no strike (its retire ran above).

	// ADR-0019 section 2.1 / 7: one StrikeCounted row per NPC or informant kill, completed with TallyAfter at the result.
	PendingStrikeRow = FPendingStrikeRow{ SlotKey, Kill.Killer, Record.StrikeCount, bGraced };
	if (Outcome == EPenaltyKillOutcome::StrikeAndExile)
	{
		// Last: the exile's calls may dispatch Roster events (Round's elimination check) that reach other listeners;
		// nothing above keeps a reference into the store across them.
		RunExile(Kill, SlotKey, SlotServerTime);
	}
	return Outcome;
}

bool FPenaltyCore::RunExile(const FPenaltyKill& Kill, FQueueKey SlotKey, double SlotServerTime)
{
	if (Deps.Bodies == nullptr)
	{
		// No gateway (the Logic tests of stories nkp-001..004): the decision only, bExiled of this Attack's result.
		SlotExile = FSlotExile{ SlotKey, Kill.Killer, Kill.KillerPawn };
		return false;
	}
	// Step 1: the Roster transition; the Roster releases the body inside it (D42), so it is Unbound from here on.
	const ERosterResult Transition = Deps.Bodies->RequestStrikeLimit(Kill.Killer, RoundEpoch);
	if (Transition != ERosterResult::Applied)
	{
		ReportFault(EStealthFault::InvariantViolation, PenaltyFaultDetail::ExileTransitionRejected);
		return false;
	}
	// Steps 2-4, in this order, in this call stack (never a timer): the card, the crowd driver, the hand-over commit.
	Deps.Bodies->ReleaseCard(Kill.KillerPawn);
	Deps.Bodies->AdoptReleasedPawn(Kill.KillerPawn, Kill.KillerLockoutEndServerTime, /*bExitAtNextScheduledSlot*/ true);
	Deps.Bodies->RequestHandOverToAi(Kill.KillerPawn, RoundEpoch, Kill.KillerLockoutEndServerTime);
	// Step 5: the released body's grace clock (PEN-R5), the participant's own-body exclusion and this slot's bExiled.
	RecordReleasedBody(Kill.KillerPawn, SlotServerTime);
	FindOrAddRecord(Kill.Killer).ExcludePawn = Kill.KillerPawn;
	SlotExile = FSlotExile{ SlotKey, Kill.Killer, Kill.KillerPawn };
	return true;
}

void FPenaltyCore::RetireInformant(const FPenaltyKill& Kill, double SlotServerTime)
{
	if (Deps.Bodies != nullptr && Deps.Bodies->RetireBinding(Kill.Victim, Kill.VictimDeathServerTime) != ERosterResult::Applied)
	{
		// The body would stay BoundLive: a second strike would read Informant instead of AlreadyResolved.
		ReportFault(EStealthFault::InvariantViolation, PenaltyFaultDetail::InformantRetireRejected);
	}
	if (Kill.VictimParticipant.IsValid())
	{
		// Not a Roster transition, so no Round call: Penalty schedules this respawn itself, inline (PEN-R8, ADR-0019 2.1).
		ScheduleRespawn(Kill.VictimParticipant, ERespawnCause::InformantKilled, SlotServerTime,
			Kill.VictimDeathServerTime > 0.0 ? TOptional<double>(Kill.VictimDeathServerTime) : TOptional<double>());
	}
}

FStrikeTally FPenaltyCore::GetTallyForResult(FParticipantId Attacker, EStrikeResult Result, FClientCommandSeq Seq, FPenaltyPawnKey AttackerPawn)
{
	FStrikeTally Out;
	const FQueueKey SlotKey = CurrentSlotKey();
	if (!Attacker.IsValid() && AttackerPawn.IsSet() && SlotExile.IsSet() && SlotExile->Pawn == AttackerPawn && SlotExile->SlotKey == SlotKey)
	{
		// The exile released the attacker's body in this slot (Unbound): the result is still its participant's.
		Attacker = SlotExile->Participant;
	}

	// bExiled: true only for the Attack whose slot exiled this attacker; consumed so no later result carries it.
	if (SlotExile.IsSet())
	{
		Out.bExiled = SlotExile->Participant == Attacker && SlotExile->SlotKey == SlotKey;
		if (Out.bExiled || SlotExile->SlotKey != SlotKey)
		{
			SlotExile.Reset();
		}
	}
	if (!Attacker.IsValid())
	{
		return Out;
	}

	FPenaltyRecord* Record = bFrozen ? Records.Find(Attacker) : &FindOrAddRecord(Attacker);
	if (Record == nullptr)
	{
		return Out;
	}
	if (!bFrozen && Result == EStrikeResult::WrongTarget)
	{
		// F1: t' = min(t + WT, 255); the result alone decides (never the victim kind).
		const uint8 Before = Record->WrongTargetTally;
		Record->WrongTargetTally = StealthPenaltyCoreLocal::SaturatingIncrement(Before);
		if (Record->WrongTargetTally != Before)
		{
			Record->LastResultSeq = Seq;
		}
	}
	Out.TallyAfter = Record->WrongTargetTally;
	if (Record->StrikeCount > Record->WrongTargetTally)
	{
		// F1 invariant k <= t ("the game may be kinder than the tally, never harsher").
		ReportFault(EStealthFault::InvariantViolation, PenaltyFaultDetail::StrikeAboveTally);
	}

	if (PendingStrikeRow.IsSet())
	{
		if (PendingStrikeRow->Killer == Attacker && PendingStrikeRow->SlotKey == SlotKey)
		{
			FPenaltyStrikeCountedRow Row;
			Row.Killer = Attacker;
			Row.StrikeAfter = PendingStrikeRow->StrikeAfter;
			Row.TallyAfter = Out.TallyAfter;
			Row.bGraced = PendingStrikeRow->bGraced;
			EventLog.Write(Row, RoundEpoch);
		}
		PendingStrikeRow.Reset();
	}
	return Out;
}

void FPenaltyCore::RecordReleasedBody(FPenaltyPawnKey Body, double ReleaseServerTime)
{
	if (!bFrozen && Body.IsSet())
	{
		ReleasedBodies.Add(Body, ReleaseServerTime);
	}
}

bool FPenaltyCore::IsGraced(FPenaltyPawnKey Victim, double SlotServerTime) const
{
	const double* Released = ReleasedBodies.Find(Victim);
	// PEN-R5: inclusive boundary, double arithmetic on server time.
	return Released != nullptr && SlotServerTime - *Released <= Latched.StrikeOutReleaseGraceSec;
}

TOptional<double> FPenaltyCore::FindReleaseTime(FPenaltyPawnKey Body) const
{
	const double* Released = ReleasedBodies.Find(Body);
	return Released != nullptr ? TOptional<double>(*Released) : TOptional<double>();
}

EInformantLifeState FPenaltyCore::GetLifeState(FParticipantId Participant) const
{
	const FPenaltyRecord* Record = Records.Find(Participant);
	return Record != nullptr ? Record->LifeState : EInformantLifeState::Active;
}

TOptional<double> FPenaltyCore::GetDueServerTime(FParticipantId Participant) const
{
	const FPenaltyRecord* Record = Records.Find(Participant);
	return Record != nullptr && Record->DueServerTime > 0.0 ? TOptional<double>(Record->DueServerTime) : TOptional<double>();
}

uint8 FPenaltyCore::GetStrikeCount(FParticipantId Participant) const
{
	const FPenaltyRecord* Record = Records.Find(Participant);
	return Record != nullptr ? Record->StrikeCount : 0;
}

uint8 FPenaltyCore::GetWrongTargetTally(FParticipantId Participant) const
{
	const FPenaltyRecord* Record = Records.Find(Participant);
	return Record != nullptr ? Record->WrongTargetTally : 0;
}

// ---- Private ----------------------------------------------------------------------------------------------------------

FPenaltyRecord& FPenaltyCore::FindOrAddRecord(FParticipantId Participant)
{
	return Records.FindOrAdd(Participant);
}

void FPenaltyCore::RepushOwnerTally(FParticipantId Participant, const FPenaltyRecord& Record)
{
	if (Deps.OwnerView != nullptr && Participant.IsValid())
	{
		Deps.OwnerView->SetOwnerTally(Participant, Record.WrongTargetTally, Record.LastResultSeq, RoundEpoch);
	}
}

FQueueKey FPenaltyCore::CurrentSlotKey() const
{
	return Deps.Slot != nullptr && Deps.Slot->IsInSlot() ? Deps.Slot->CurrentKey() : FQueueKey();
}

void FPenaltyCore::ReportFault(EStealthFault Fault, int64 Detail) const
{
	if (Deps.Faults != nullptr)
	{
		Deps.Faults->Report(Fault, StealthPenaltyCoreLocal::FaultSource, Detail);
	}
}
