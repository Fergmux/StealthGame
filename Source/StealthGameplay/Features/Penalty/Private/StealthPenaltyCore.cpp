#include "StealthPenaltyCore.h"

#include "Server/StealthClock.h"
#include "Server/StealthServerTuning.h"

// Stories nkp-001 (store, lifetime), nkp-002 (strike rule, release grace), nkp-003 (tally, bExiled, owner re-push) and
// nkp-004 (the StrikeCounted row). GDD npc-kill-penalty-informants.md PEN-R2..R6, F1; ADR-0019 sections 1, 2.1, 2.2,
// 2.2a, 7. Server-only values: nothing here formats, logs or traces a record, a count or a row.

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
	bFrozen = true;
	for (TPair<FParticipantId, FPenaltyRecord>& Pair : Records)
	{
		Pair.Value.LifeState = EInformantLifeState::RoundEnded;
	}
	PendingStrikeRow.Reset();
}

void FPenaltyCore::OnRosterReset(uint32 NewRoundEpoch)
{
	bFrozen = false;
	RoundEpoch = NewRoundEpoch;
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
	// PEN-R6: a disconnect or a return never touches the counters. The Absent path (story nkp-010) hooks here.
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
	// PEN-R6: counters unchanged. The readmission's respawn scheduling is story nkp-010.
}

// ---- Strikes and tally ----------------------------------------------------------------------------------------------

EPenaltyKillOutcome FPenaltyCore::OnPawnKilled(const FPenaltyKill& Kill)
{
	if (bFrozen)
	{
		return EPenaltyKillOutcome::NoStrike; // frozen at round end: nothing is counted, released or retired
	}
	const double SlotServerTime = Deps.Clock != nullptr ? Deps.Clock->GetServerTimeSeconds() : 0.0;
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
	if (Deps.Respawns != nullptr && Kill.VictimParticipant.IsValid())
	{
		Deps.Respawns->ScheduleRespawn(Kill.VictimParticipant, ERespawnCause::InformantKilled, SlotServerTime);
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
