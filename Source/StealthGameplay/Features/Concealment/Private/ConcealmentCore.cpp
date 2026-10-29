// Stories con-003..con-009, con-014..con-018, con-024: the plain-C++ server core of Concealment. See
// Server/ConcealmentCore.h.

#include "Server/ConcealmentCore.h"

#include "GameFramework/Pawn.h"
#include "Server/EventLog/StealthRoundLogWriter.h"
#include "Server/StealthClock.h"
#include "Server/StealthFaultSink.h"
#include "Server/Visibility/StealthConcealmentCullSource.h"
#include "View/NetPawnCommand.h"

namespace ConcealmentCoreLocal
{
	const FName FaultSource(TEXT("Concealment"));

	/** The legal state moves (ADR-0017 section 1, AC1 of story con-003; Entering->Exiting is the guard-failure exit). */
	bool IsLegalMove(EConcealState From, EConcealState To)
	{
		switch (From)
		{
		case EConcealState::Outside: return To == EConcealState::Entering;
		case EConcealState::Entering: return To == EConcealState::Concealed || To == EConcealState::Outside || To == EConcealState::Exiting;
		case EConcealState::Concealed: return To == EConcealState::Exiting || To == EConcealState::Emerging;
		case EConcealState::Exiting: return To == EConcealState::Outside;
		case EConcealState::Emerging: return To == EConcealState::Outside;
		}
		return false;
	}
}

FConcealmentCore::FConcealmentCore(const FConcealmentCoreDeps& InDeps, const FConcealmentParams& InParams)
	: Deps(InDeps)
	, Params(InParams)
{
}

void FConcealmentCore::SetBushes(TConstArrayView<FBushRuntime> InBushes)
{
	Bushes = TArray<FBushRuntime>(InBushes.GetData(), InBushes.Num());
}

const FBushRuntime* FConcealmentCore::FindBush(uint16 BushId) const
{
	return Bushes.FindByPredicate([BushId](const FBushRuntime& Bush) { return Bush.Id == BushId; });
}

double FConcealmentCore::Now() const
{
	return Deps.Clock != nullptr ? Deps.Clock->GetServerTimeSeconds() : 0.0;
}

FConcealRecord& FConcealmentCore::RecordOf(APawn& Pawn)
{
	return Records.FindOrAdd(TWeakObjectPtr<APawn>(&Pawn));
}

const FConcealRecord* FConcealmentCore::FindRecord(const APawn* Pawn) const
{
	return Pawn != nullptr ? Records.Find(TWeakObjectPtr<APawn>(const_cast<APawn*>(Pawn))) : nullptr;
}

EConcealState FConcealmentCore::GetState(const APawn* Pawn) const
{
	const FConcealRecord* Record = FindRecord(Pawn);
	return Record != nullptr ? Record->State : EConcealState::Outside;
}

void FConcealmentCore::Report(int64 FaultValue, int64 Detail) const
{
	if (Deps.Faults != nullptr)
	{
		Deps.Faults->Report(static_cast<EStealthFault>(FaultValue), ConcealmentCoreLocal::FaultSource, Detail);
	}
}

bool FConcealmentCore::Transition(APawn& Pawn, FConcealRecord& Record, EConcealState To)
{
	if (!ConcealmentCoreLocal::IsLegalMove(Record.State, To))
	{
		Report(static_cast<int64>(EStealthFault::ConcealTransition), (static_cast<int64>(Record.State) << 8) | static_cast<int64>(To));
		return false;
	}
	// A transition (enter, exit, emerge, its end, a kill before the commit) takes a new generation; the Commit is a phase of
	// the enter transition and keeps its generation, so the enter's End item still applies (decision logged, story con-003).
	if (To != EConcealState::Concealed)
	{
		++Record.TransitionGeneration;
	}
	Record.State = To;
	Record.TransitionStartServerTime = Now();
	Deps.Pawns->OnStateChanged(Pawn, To, Record.BushId, Record.TransitionStartServerTime);
	return true;
}

void FConcealmentCore::Schedule(const FQueueItem& Cause, APawn& Pawn, const FConcealRecord& Record, EConcealPhase Phase, double Due)
{
	FTimerPayload Timer;
	Timer.Kind = ETimerKind::ConcealmentTransition;
	Timer.Subject = FSubjectRef::ForPawn(FPawnHandle{ TWeakObjectPtr<APawn>(&Pawn) });
	Timer.Generation = Record.TransitionGeneration;
	Timer.Seq = 0;
	Timer.Phase = static_cast<uint8>(Phase);
	Deps.Queue->ScheduleAt(Due, FQueueItem::Make(EQueueItemType::TimerFired, Cause.Expected, FMailboxId::ServerLocal(), Timer));
	++ScheduledItems;
}

EResolveResult FConcealmentCore::Void(const FCommandPayload& Cmd, FQueueKey Key, ECommandRejectCode Code)
{
	Deps.Queue->SetVoidCode(static_cast<uint32>(Code));
	Deps.Queue->AppendReply(Cmd.Sink, Key, FCommandReply::MakeRejected(Cmd.ClientSeq, Code));
	return EResolveResult::Voided;
}

// ---- Legality --------------------------------------------------------------------------------------------------------

FPredicateResult FConcealmentCore::CanBeginHide(const APawn& Pawn, ECommandKind Kind, uint32 Arg) const
{
	if (Deps.Pawns == nullptr || !Deps.Pawns->IsLive(Pawn))
	{
		return FPredicateResult::Reject(ECommandRejectCode::NotNow);
	}
	const EConcealState State = GetState(&Pawn);
	const ECommandLegalityClass Legality = Deps.Pawns->GetLegalityClass(Pawn);
	if (Kind == ECommandKind::HideExit)
	{
		// An exit is never refused for blocking (CON-R8); only a pawn with nothing to exit, or mid-transition, is.
		if (Legality == ECommandLegalityClass::RoundEnded || State == EConcealState::Outside)
		{
			return FPredicateResult::Reject(ECommandRejectCode::NotNow);
		}
		const FConcealRecord* Record = FindRecord(&Pawn);
		if (Record != nullptr && Record->bKilled)
		{
			return FPredicateResult::Reject(ECommandRejectCode::NotNow);
		}
		// The enter's slot is held until its End (AC-CON-25: Busy until +BushEnterDurSec), then the exit is allowed.
		return State == EConcealState::Concealed && (Record == nullptr || !Record->bTransitionOpen) ? FPredicateResult::Allow() : FPredicateResult::Reject(ECommandRejectCode::Busy);
	}
	if (Kind != ECommandKind::HideEnter)
	{
		return FPredicateResult::Reject(ECommandRejectCode::NotNow);
	}
	// CON-R3, in the IIF legality-matrix order: class and locomotion (NotNow), the slot (Busy), reach (OutOfReach), cooldown.
	if (Legality != ECommandLegalityClass::Active || Deps.Pawns->IsRunning(Pawn) || Deps.Pawns->IsTraversing(Pawn) || State == EConcealState::Concealed)
	{
		return FPredicateResult::Reject(ECommandRejectCode::NotNow);
	}
	if (State != EConcealState::Outside)
	{
		return FPredicateResult::Reject(ECommandRejectCode::Busy);
	}
	const FBushRuntime* Bush = FindBush(static_cast<uint16>(Arg));
	const FVector Location = Deps.Pawns->GetLocation(Pawn);
	if (Bush == nullptr || ConcealmentGeometry::DistanceToFootprintCm(Bush->Footprint, Location) > static_cast<double>(Params.Public.BushEntryReachCm) + 1e-3
		|| !Bush->IsSideOpen(ConcealmentGeometry::SideFromYaw(ConcealmentGeometry::YawFromAnchor(*Bush, Location))))
	{
		return FPredicateResult::Reject(ECommandRejectCode::OutOfReach);
	}
	const FConcealRecord* Record = FindRecord(&Pawn);
	if (Record != nullptr && Now() - Record->LastExitCompleteServerTime + 1e-6 < static_cast<double>(Params.Server.BushReenterCooldownSec))
	{
		return FPredicateResult::Reject(ECommandRejectCode::Cooldown);
	}
	return FPredicateResult::Allow();
}

// ---- Commands --------------------------------------------------------------------------------------------------------

EResolveResult FConcealmentCore::ResolveCommand(const FQueueItem& Item, FQueueKey Key)
{
	const FCommandPayload* Cmd = Item.Type == EQueueItemType::PawnCommand ? Item.Payload.TryGet<FCommandPayload>() : nullptr;
	APawn* Pawn = Cmd != nullptr ? Cmd->Pawn.Pawn.Get() : nullptr;
	if (Cmd == nullptr || Pawn == nullptr || Deps.Pawns == nullptr || Deps.Queue == nullptr)
	{
		return EResolveResult::Voided;
	}
	const ECommandKind Kind = static_cast<ECommandKind>(Cmd->Kind);
	uint32 Arg = Cmd->Arg;
	if (Kind == ECommandKind::HideEnter && Arg == 0)
	{
		// No BushId sent (the prompt target): the bush in entry reach of the pawn, the same public-geometry rule a client uses.
		const FVector Location = Deps.Pawns->GetLocation(*Pawn);
		double Best = TNumericLimits<double>::Max();
		for (const FBushRuntime& Bush : Bushes)
		{
			const double Distance = ConcealmentGeometry::DistanceToFootprintCm(Bush.Footprint, Location);
			if (Distance <= static_cast<double>(Params.Public.BushEntryReachCm) && Distance < Best)
			{
				Best = Distance;
				Arg = Bush.Id;
			}
		}
	}
	const FPredicateResult Verdict = CanBeginHide(*Pawn, Kind, Arg);
	if (Verdict.Verdict == EPredicateVerdict::Reject)
	{
		return Void(*Cmd, Key, Verdict.Code);
	}
	if (Kind == ECommandKind::HideEnter)
	{
		return ExecuteEnter(Item, *Pawn, static_cast<uint16>(Arg), Key);
	}
	if (Kind == ECommandKind::HideExit)
	{
		return ExecuteExitCommand(Item, *Pawn, static_cast<uint8>(Arg & 0x3), Key);
	}
	return EResolveResult::Voided;
}

EResolveResult FConcealmentCore::ExecuteEnter(const FQueueItem& Item, APawn& Pawn, uint16 BushId, FQueueKey Key)
{
	const FBushRuntime* Bush = FindBush(BushId);
	FConcealRecord& Record = RecordOf(Pawn);
	if (Bush == nullptr)
	{
		return EResolveResult::Voided;
	}
	Record.BushId = BushId;
	Record.CommitSeq = 0;
	Record.bCrossed = false;
	Record.bKilled = false;
	Record.bDeathWritten = false;
	// The entry side is the open side nearest the pawn at execution.
	Record.Side = ConcealmentGeometry::NearestOpenSide(Bush->EntryAxisMask, ConcealmentGeometry::YawFromAnchor(*Bush, Deps.Pawns->GetLocation(Pawn)));
	if (!Transition(Pawn, Record, EConcealState::Entering))
	{
		return EResolveResult::Voided;
	}
	Record.bTransitionOpen = true;
	const double Start = Record.TransitionStartServerTime;
	Deps.Pawns->WritePose(Pawn, EActionPose::BushEnter, Start);
	Deps.Pawns->StartWarp(Pawn, Bush->Anchor + FVector(0.0, 0.0, Deps.Pawns->GetCapsuleHalfHeightCm(Pawn)), Params.Public.BushEnterSwapSec, /*bEnter*/ true);
	Schedule(Item, Pawn, Record, EConcealPhase::Commit, Start + Params.Public.BushEnterSwapSec);
	Schedule(Item, Pawn, Record, EConcealPhase::End, Start + Params.Public.BushEnterDurSec);
	Deps.Queue->OnActionStarted(Pawn, EActionType::HideEnter);
	return EResolveResult::Executed;
}

EResolveResult FConcealmentCore::ExecuteExitCommand(const FQueueItem& Item, APawn& Pawn, uint8 Side, FQueueKey Key)
{
	FConcealRecord& Record = RecordOf(Pawn);
	const FBushRuntime* Bush = FindBush(Record.BushId);
	if (Bush == nullptr)
	{
		return EResolveResult::Voided;
	}
	StartExit(Item, Pawn, Record, ChooseExitSide(Pawn, *Bush, Side), Key);
	Deps.Queue->OnActionStarted(Pawn, EActionType::HideExit);
	return EResolveResult::Executed;
}

bool FConcealmentCore::IsBlockingPawn(const APawn& Pawn) const
{
	// Outside, or Exiting/Emerging past their crossing; Entering, Concealed and pre-crossing pawns never block.
	const FConcealRecord* Record = FindRecord(&Pawn);
	if (Record == nullptr || Record->State == EConcealState::Outside)
	{
		return true;
	}
	return (Record->State == EConcealState::Exiting || Record->State == EConcealState::Emerging) && Record->bCrossed;
}

uint8 FConcealmentCore::ChooseExitSide(const APawn& Pawn, const FBushRuntime& Bush, uint8 Requested) const
{
	const uint8 Valid = ConcealmentGeometry::ValidateSide(Bush.EntryAxisMask, Requested);
	const double Radius = Deps.Pawns->GetCapsuleRadiusCm(Pawn);
	// Open sides in angular order from the requested one (distance of each side centre to the valid side's centre; ties
	// keep the lower index).
	TArray<uint8, TInlineAllocator<4>> Order;
	for (uint8 Side = 0; Side < 4; ++Side)
	{
		if (Bush.IsSideOpen(Side))
		{
			Order.Add(Side);
		}
	}
	Order.StableSort([Valid](uint8 A, uint8 B)
	{
		return ConcealmentGeometry::AngularDistanceDeg(A * 90.0, Valid * 90.0) < ConcealmentGeometry::AngularDistanceDeg(B * 90.0, Valid * 90.0);
	});
	for (const uint8 Side : Order)
	{
		const FVector End = ConcealmentGeometry::ExitPoint(Bush, Side, Radius);
		TArray<APawn*> Near;
		Deps.Pawns->GetPawnsNear(End, Radius * 2.0, Near);
		bool bBlocked = false;
		for (const APawn* Other : Near)
		{
			if (Other != &Pawn && IsBlockingPawn(*Other))
			{
				bBlocked = true;
				break;
			}
		}
		if (!bBlocked)
		{
			return Side;
		}
	}
	return Valid; // every side blocked: the requested one, and the capped depenetration separates the pawns
}

void FConcealmentCore::StartExit(const FQueueItem& Cause, APawn& Pawn, FConcealRecord& Record, uint8 Side, FQueueKey Key)
{
	const FBushRuntime* Bush = FindBush(Record.BushId);
	if (Bush == nullptr || !Transition(Pawn, Record, EConcealState::Exiting))
	{
		return;
	}
	Record.Side = Side;
	Record.bCrossed = false;
	Record.bTransitionOpen = true;
	// Un-cull in the same server step as the exit execution (ADR-0003 section 5.6).
	if (Record.bCulled && Deps.Cull != nullptr)
	{
		Deps.Cull->SetCullState(&Pawn, false, Record.TransitionStartServerTime);
	}
	Record.bCulled = false;
	const double Start = Record.TransitionStartServerTime;
	Deps.Pawns->WritePose(Pawn, EActionPose::BushExit, Start);
	Deps.Pawns->StartWarp(Pawn, ConcealmentGeometry::ExitPoint(*Bush, Side, Deps.Pawns->GetCapsuleRadiusCm(Pawn)) + FVector(0.0, 0.0, Deps.Pawns->GetCapsuleHalfHeightCm(Pawn)),
		Params.Public.BushExitDurSec, /*bEnter*/ false);
	Schedule(Cause, Pawn, Record, EConcealPhase::Cross, Start + Params.Public.BushExitCrossSec);
	Schedule(Cause, Pawn, Record, EConcealPhase::End, Start + Params.Public.BushExitDurSec);
}

// ---- Timers ----------------------------------------------------------------------------------------------------------

EResolveResult FConcealmentCore::ResolveTimer(const FQueueItem& Item, FQueueKey Key)
{
	const FTimerPayload* Timer = Item.Type == EQueueItemType::TimerFired ? Item.Payload.TryGet<FTimerPayload>() : nullptr;
	if (Timer == nullptr || Timer->Kind != ETimerKind::ConcealmentTransition || Timer->Subject.Kind != ESubjectRefKind::Pawn || Deps.Pawns == nullptr)
	{
		return EResolveResult::Voided;
	}
	APawn* Pawn = Timer->Subject.Pawn.Pawn.Get();
	FConcealRecord* Record = Pawn != nullptr ? Records.Find(TWeakObjectPtr<APawn>(Pawn)) : nullptr;
	// Stale generation, a dead pawn or one pending teardown: Voided, no state, cull, flag or log write.
	if (Record == nullptr || Record->TransitionGeneration != Timer->Generation || !Deps.Pawns->IsLive(*Pawn))
	{
		return EResolveResult::Voided;
	}
	switch (static_cast<EConcealPhase>(Timer->Phase))
	{
	case EConcealPhase::Commit:
		if (Record->State != EConcealState::Entering)
		{
			return EResolveResult::Voided;
		}
		Commit(Item, *Pawn, *Record, Key);
		return EResolveResult::Executed;
	case EConcealPhase::Cross:
		if ((Record->State != EConcealState::Exiting && Record->State != EConcealState::Emerging) || Record->bCrossed)
		{
			return EResolveResult::Voided;
		}
		Cross(*Pawn, *Record, Key);
		return EResolveResult::Executed;
	case EConcealPhase::End:
		End(*Pawn, *Record);
		return EResolveResult::Executed;
	}
	return EResolveResult::Voided;
}

void FConcealmentCore::Commit(const FQueueItem& Item, APawn& Pawn, FConcealRecord& Record, FQueueKey Key)
{
	const FBushRuntime* Bush = FindBush(Record.BushId);
	const bool bInside = Bush != nullptr && ConcealmentGeometry::IsCapsuleInsideHideVolume(*Bush, Deps.Pawns->GetLocation(Pawn),
		Deps.Pawns->GetCapsuleRadiusCm(Pawn), Deps.Pawns->GetCapsuleHalfHeightCm(Pawn));
	if (!bInside)
	{
		// AC-CON-29: nothing commits; one ConcealGuard fault; the ordinary exit on the entry side follows, run by this
		// resolver as an internal transition (never a PawnCommand, which the latch would reject).
		Report(static_cast<int64>(EStealthFault::ConcealGuard), Record.BushId);
		StartExit(Item, Pawn, Record, Record.Side, Key);
		return;
	}
	if (!Transition(Pawn, Record, EConcealState::Concealed))
	{
		return;
	}
	// The commit instant (the clips and the latch keep reading the enter's start from FActionState).
	const double CommitTime = Record.TransitionStartServerTime;
	InsertOccupant(Pawn, Record);
	if (Deps.Cull != nullptr)
	{
		Deps.Cull->SetCullState(&Pawn, true, CommitTime);
	}
	Record.bCulled = true;
	SetFlags(Pawn, Record, true);
	LogRow(Key, Pawn, Record.BushId, EConcealmentLogEvent::Commit);
}

void FConcealmentCore::Cross(APawn& Pawn, FConcealRecord& Record, FQueueKey Key)
{
	Record.bCrossed = true;
	const bool bWasOccupant = Record.bInOccupancy;
	RemoveOccupant(Pawn, Record);
	SetFlags(Pawn, Record, false);
	if (bWasOccupant)
	{
		LogRow(Key, Pawn, Record.BushId, Record.State == EConcealState::Emerging ? EConcealmentLogEvent::Emerge : EConcealmentLogEvent::ExitCross);
	}
	Deps.Pawns->OnStateChanged(Pawn, Record.State, Record.BushId, Record.TransitionStartServerTime);
}

void FConcealmentCore::End(APawn& Pawn, FConcealRecord& Record)
{
	Record.bTransitionOpen = false;
	const FActionState Pose = Deps.Pawns->GetActionState(Pawn);
	switch (Record.State)
	{
	case EConcealState::Concealed:
		// The enter's End: the pose returns to None, the state stays Concealed (CON-R5).
		if (Pose.Pose == EActionPose::BushEnter)
		{
			Deps.Pawns->WritePose(Pawn, EActionPose::None, -1.0);
		}
		break;
	case EConcealState::Exiting:
	case EConcealState::Emerging:
	{
		const bool bExit = Record.State == EConcealState::Exiting;
		if (!Record.bCrossed)
		{
			Cross(Pawn, Record, FQueueKey()); // a crossing never skipped (same slot order: Cross is due first)
		}
		if (!Transition(Pawn, Record, EConcealState::Outside))
		{
			return;
		}
		Record.LastExitCompleteServerTime = Record.TransitionStartServerTime;
		Deps.Pawns->SetExitDepenetrationCap(Pawn, Params.Server.BushExitDepenetrationCapCm);
		if (bExit && Pose.Pose == EActionPose::BushExit)
		{
			Deps.Pawns->WritePose(Pawn, EActionPose::None, -1.0);
		}
		break;
	}
	default:
		break;
	}
}

// ---- Occupancy and flags ---------------------------------------------------------------------------------------------

void FConcealmentCore::InsertOccupant(APawn& Pawn, FConcealRecord& Record)
{
	Record.CommitSeq = NextCommitSeq++;
	Record.bInOccupancy = true;
	Occupancy.FindOrAdd(Record.BushId).Add(TWeakObjectPtr<APawn>(&Pawn));
}

void FConcealmentCore::RemoveOccupant(APawn& Pawn, FConcealRecord& Record)
{
	if (!Record.bInOccupancy)
	{
		return;
	}
	Record.bInOccupancy = false;
	if (TArray<TWeakObjectPtr<APawn>>* List = Occupancy.Find(Record.BushId))
	{
		List->Remove(TWeakObjectPtr<APawn>(&Pawn));
	}
}

void FConcealmentCore::SetFlags(APawn& Pawn, FConcealRecord& Record, bool bConcealed)
{
	if (Deps.Flags == nullptr)
	{
		return;
	}
	if (bConcealed)
	{
		// Every unbound pawn counts as Active and has no participant: the setters are not called for it.
		const FParticipantId Participant = Deps.Relations != nullptr ? Deps.Relations->GetParticipant(Pawn) : FParticipantId();
		if (!Participant.IsValid())
		{
			return;
		}
		Record.FlagParticipant = Participant;
		Deps.Flags->SetOwnerConcealed(Participant, true);
		Deps.Flags->SetTeammateConcealed(Participant, true);
		return;
	}
	if (Record.FlagParticipant.IsValid() && !Record.bKilled)
	{
		Deps.Flags->SetOwnerConcealed(Record.FlagParticipant, false);
		Deps.Flags->SetTeammateConcealed(Record.FlagParticipant, false);
		Record.FlagParticipant = FParticipantId();
	}
}

void FConcealmentCore::LogRow(FQueueKey Key, const APawn& Pawn, uint16 BushId, EConcealmentLogEvent Event, uint32 D2, uint32 D3, uint32 D4)
{
	if (Deps.Log == nullptr)
	{
		return;
	}
	FRoundLogEntry Entry;
	Entry.Key = Key;
	Entry.ServerTime = Now();
	Entry.Type = ERoundLogType::Concealment;
	Entry.Code = static_cast<uint16>(Event);
	const FParticipantId Participant = Deps.Relations != nullptr ? Deps.Relations->GetParticipant(Pawn) : FParticipantId();
	Entry.Participants[0] = Participant.Value;
	Entry.Data[0] = BushId;
	Entry.Data[1] = Deps.Relations != nullptr && Deps.Relations->IsPlayerPawn(Pawn) ? 1u : 0u;
	Entry.Data[2] = D2;
	Entry.Data[3] = D3;
	Entry.Data[4] = D4;
	Deps.Log->Append(Entry);
}

// ---- Events ----------------------------------------------------------------------------------------------------------

void FConcealmentCore::OnAttackExecuted(APawn& Attacker, double ContactServerTime, FQueueKey Key, const FQueueItem& Item)
{
	FConcealRecord* Record = Records.Find(TWeakObjectPtr<APawn>(&Attacker));
	if (Record == nullptr || Record->State != EConcealState::Concealed || Record->bKilled)
	{
		return;
	}
	const FBushRuntime* Bush = FindBush(Record->BushId);
	if (Bush == nullptr || !Transition(Attacker, *Record, EConcealState::Emerging))
	{
		return;
	}
	// The emerge side: the open side nearest the aim yaw (story con-005's tie rule).
	Record->Side = ConcealmentGeometry::NearestOpenSide(Bush->EntryAxisMask, Deps.Pawns->GetLookYawDeg(Attacker));
	Record->bCrossed = false;
	Record->bTransitionOpen = true;
	// OnConcealmentBroken: the un-cull in the Attack's slot.
	if (Record->bCulled && Deps.Cull != nullptr)
	{
		Deps.Cull->SetCullState(&Attacker, false, Record->TransitionStartServerTime);
	}
	Record->bCulled = false;
	const double Exec = Record->TransitionStartServerTime;
	// Cross at ContactServerTime - StrikeContactSec: the same formula clients use (they cannot round differently).
	const double CrossAt = ContactServerTime >= 0.0 ? ContactServerTime - Params.StrikeContactSec : Exec + Params.Public.BushStrikeEmergeSec;
	Deps.Pawns->StartWarp(Attacker, ConcealmentGeometry::ExitPoint(*Bush, Record->Side, Deps.Pawns->GetCapsuleRadiusCm(Attacker)) + FVector(0.0, 0.0, Deps.Pawns->GetCapsuleHalfHeightCm(Attacker)),
		FMath::Max(CrossAt - Exec, 0.05), /*bEnter*/ false);
	Schedule(Item, Attacker, *Record, EConcealPhase::Cross, CrossAt);
	Schedule(Item, Attacker, *Record, EConcealPhase::End, FMath::Max(Exec + Params.AttackLockoutSec, CrossAt));
}

void FConcealmentCore::OnPawnKilled(APawn& Victim, FQueueKey Key)
{
	FConcealRecord* Record = Records.Find(TWeakObjectPtr<APawn>(&Victim));
	if (Record == nullptr || Record->bKilled)
	{
		return;
	}
	switch (Record->State)
	{
	case EConcealState::Entering:
		// No immunity while entering: a visible pawn, no record, the Commit drops by generation, the swap is cancelled.
		Transition(Victim, *Record, EConcealState::Outside);
		break;
	case EConcealState::Concealed:
	case EConcealState::Exiting:
	case EConcealState::Emerging:
		if (Record->State != EConcealState::Concealed && Record->bCrossed)
		{
			break; // past the crossing: an ordinary visible pawn
		}
		Record->bKilled = true;
		++Record->TransitionGeneration; // no later Cross or End applies to the body
		{
			const bool bWasOccupant = Record->bInOccupancy;
			RemoveOccupant(Victim, *Record);
			if (bWasOccupant)
			{
				LogRow(Key, Victim, Record->BushId, EConcealmentLogEvent::Killed);
			}
		}
		break;
	default:
		break;
	}
}

bool FConcealmentCore::ShouldMarkDiedConcealed(const APawn& Victim) const
{
	const FConcealRecord* Record = FindRecord(&Victim);
	return Record != nullptr && Record->bKilled && Record->bCulled;
}

bool FConcealmentCore::OnDeathWrite(APawn& Victim)
{
	FConcealRecord* Record = Records.Find(TWeakObjectPtr<APawn>(&Victim));
	if (Record == nullptr || !Record->bKilled || Record->bDeathWritten)
	{
		return false;
	}
	Record->bDeathWritten = true;
	const bool bWasCulled = Record->bCulled;
	if (bWasCulled && Deps.Cull != nullptr)
	{
		Deps.Cull->SetCullState(&Victim, false, Now()); // the re-open with bDiedConcealed, in the death write's step
	}
	Record->bCulled = false;
	return bWasCulled;
}

void FConcealmentCore::OnBodyRemoved(APawn& Body)
{
	FConcealRecord* Record = Records.Find(TWeakObjectPtr<APawn>(&Body));
	if (Record == nullptr || !Record->bKilled || !Record->FlagParticipant.IsValid() || Deps.Flags == nullptr)
	{
		return;
	}
	Deps.Flags->SetOwnerConcealed(Record->FlagParticipant, false);
	Deps.Flags->SetTeammateConcealed(Record->FlagParticipant, false);
	Record->FlagParticipant = FParticipantId();
}

void FConcealmentCore::OnParticipantRebound(FParticipantId Participant, const APawn* NewPawn)
{
	if (!Participant.IsValid() || Deps.Flags == nullptr)
	{
		return;
	}
	for (TPair<TWeakObjectPtr<APawn>, FConcealRecord>& Pair : Records)
	{
		FConcealRecord& Record = Pair.Value;
		if (Record.FlagParticipant == Participant && Record.bKilled && Pair.Key.Get() != NewPawn)
		{
			Deps.Flags->SetOwnerConcealed(Participant, false);
			Deps.Flags->SetTeammateConcealed(Participant, false);
			Record.FlagParticipant = FParticipantId();
		}
	}
}

void FConcealmentCore::ResetAll()
{
	if (Deps.Flags != nullptr)
	{
		for (const TPair<TWeakObjectPtr<APawn>, FConcealRecord>& Pair : Records)
		{
			if (Pair.Value.FlagParticipant.IsValid())
			{
				Deps.Flags->SetOwnerConcealed(Pair.Value.FlagParticipant, false);
				Deps.Flags->SetTeammateConcealed(Pair.Value.FlagParticipant, false);
			}
		}
	}
	Records.Reset();
	Occupancy.Reset();
}

// ---- Queries ---------------------------------------------------------------------------------------------------------

bool FConcealmentCore::IsInBushOrTransition(const APawn* Pawn) const
{
	return GetState(Pawn) != EConcealState::Outside;
}

bool FConcealmentCore::IsPerceivable(const APawn* Pawn) const
{
	return GetState(Pawn) != EConcealState::Concealed;
}

bool FConcealmentCore::IsSteeringObstacle(const APawn* Pawn) const
{
	return GetState(Pawn) == EConcealState::Outside;
}

bool FConcealmentCore::IsConcealed(const APawn* Pawn) const
{
	const FConcealRecord* Record = FindRecord(Pawn);
	return Record != nullptr && Record->State == EConcealState::Concealed && !Record->bKilled;
}

TOptional<uint16> FConcealmentCore::TargetsBush(const FVector& WorldPoint) const
{
	for (const FBushRuntime& Bush : Bushes)
	{
		if (ConcealmentGeometry::TargetsBush(Bush, WorldPoint, Params.Server.BushSpotToleranceCm))
		{
			return Bush.Id;
		}
	}
	return TOptional<uint16>();
}

void FConcealmentCore::GetOccupancy(uint16 BushId, TArray<APawn*>& OutByCommitSeq) const
{
	OutByCommitSeq.Reset();
	if (const TArray<TWeakObjectPtr<APawn>>* List = Occupancy.Find(BushId))
	{
		for (const TWeakObjectPtr<APawn>& Weak : *List)
		{
			if (APawn* Pawn = Weak.Get())
			{
				OutByCommitSeq.Add(Pawn);
			}
		}
	}
}

void FConcealmentCore::GetSpotOccupants(uint16 BushId, const APawn* Attacker, TArray<APawn*>& OutByCommitSeq) const
{
	GetOccupancy(BushId, OutByCommitSeq);
	const FParticipantId AttackerId = Attacker != nullptr && Deps.Relations != nullptr ? Deps.Relations->GetParticipant(*Attacker) : FParticipantId();
	OutByCommitSeq.RemoveAll([this, Attacker, AttackerId](const APawn* Pawn)
	{
		return Pawn == Attacker || (AttackerId.IsValid() && Deps.Relations->IsAllyViewer(AttackerId, *Pawn));
	});
}

bool FConcealmentCore::IsConcealedFrom(FParticipantId Viewer, const APawn* Pawn) const
{
	const FConcealRecord* Record = FindRecord(Pawn);
	if (Record == nullptr)
	{
		return false;
	}
	bool bHidden = false;
	if (Record->bKilled)
	{
		bHidden = !Record->bDeathWritten; // killed while concealed: until the death write re-opens it
	}
	else
	{
		bHidden = Record->State == EConcealState::Concealed
			|| ((Record->State == EConcealState::Exiting || Record->State == EConcealState::Emerging) && !Record->bCrossed && Record->CommitSeq != 0);
	}
	if (!bHidden)
	{
		return false;
	}
	return !(Viewer.IsValid() && Deps.Relations != nullptr && Deps.Relations->IsAllyViewer(Viewer, *Pawn));
}
