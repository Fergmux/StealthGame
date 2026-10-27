// Stories asn-005 to asn-011 (production/epics/assassination-defence-reversal/): the strike resolver in the round queue's
// slot (ASN-R2, R5, R8-R15, F2-F4; ADR-0002 sections 5 and 8, ADR-0018 section 2.1). See Server/AttackResolverCore.h.
// Replaces AStealthGameMode::HandleAssassinationRequest / HandleDefenceRequest and AStealthCharacter::KillCharacter's
// immediate ragdoll and one-second hold; FStealthRules::ResolveAssassination (StealthGame.Rules.Assassination) is the
// behavioural reference for the outcome table, re-expressed on the queue slot.

#include "Server/AttackResolverCore.h"

#include "Server/BushSpotQuery.h"
#include "Server/DefenceStateCore.h"
#include "Server/EventLog/StealthRoundLogTypes.h"
#include "Server/EventLog/StealthRoundLogWriter.h"
#include "Server/StealthClock.h"

FAttackResolverCore::FAttackResolverCore(const FAttackResolverDeps& InDeps, const FEncounterParams& InParams)
	: Deps(InDeps)
	, Params(InParams)
{
}

double FAttackResolverCore::Now() const
{
	return Deps.Clock != nullptr ? Deps.Clock->GetServerTimeSeconds() : 0.0;
}

EVictimKind FAttackResolverCore::ToVictimKind(ERosterVictimKind Kind)
{
	switch (Kind)
	{
	case ERosterVictimKind::Player: return EVictimKind::Player;
	case ERosterVictimKind::Informant: return EVictimKind::Informant;
	default: return EVictimKind::Npc;
	}
}

FVictimResolverDeps FAttackResolverCore::MakeResolverDeps() const
{
	FVictimResolverDeps Out;
	Out.Pawns = Deps.Pawns;
	Out.Roster = Deps.Roster;
	Out.Visibility = Deps.Visibility;
	Out.Bushes = Deps.Bushes;
	Out.Roles = &Roles;
	Out.SubSeeds = Deps.SubSeeds;
	return Out;
}

EStrikeRole FAttackResolverCore::GetRole(const APawn* Pawn) const
{
	const FDefenceStateCore* State = Deps.Defence != nullptr ? Deps.Defence->Find(Pawn) : nullptr;
	return Roles.GetRole(Pawn, Now(), State, Params.Public.DefenceCooldownSec);
}

TOptional<ECommandRejectCode> FAttackResolverCore::CheckSlot(const APawn* Pawn, ECommandKind Kind) const
{
	if (Pawn == nullptr)
	{
		return ECommandRejectCode::NotNow;
	}
	switch (GetRole(Pawn))
	{
	case EStrikeRole::Dead:
		return ECommandRejectCode::NotNow;
	case EStrikeRole::Striking:
	case EStrikeRole::Countering:
		return ECommandRejectCode::Busy;
	case EStrikeRole::Bracing:
		// The slot is the brace's: Attack preempts it (Superseded), the Prepare kinds are its own; anything else waits.
		if (Kind == ECommandKind::Attack || Kind == ECommandKind::PrepareBegin || Kind == ECommandKind::PrepareEnd)
		{
			return TOptional<ECommandRejectCode>();
		}
		return ECommandRejectCode::Busy;
	default:
		return TOptional<ECommandRejectCode>();
	}
}

EResolveResult FAttackResolverCore::Void(const FCommandPayload& Cmd, FQueueKey Key, ECommandRejectCode Code)
{
	if (Deps.Queue != nullptr)
	{
		Deps.Queue->SetVoidCode(static_cast<uint32>(Code));
	}
	FEncounterCommandReply& Reply = Replies.AddDefaulted_GetRef();
	Reply.Key = Key;
	Reply.Pawn = Cmd.Pawn.Pawn.Get();
	Reply.Kind = static_cast<ECommandKind>(Cmd.Kind);
	Reply.bExecuted = false;
	Reply.Code = Code;
	Reply.Sink = Cmd.Sink;
	Reply.ClientSeq = Cmd.ClientSeq;
	return EResolveResult::Voided;
}

void FAttackResolverCore::RecordExecuted(const FCommandPayload& Cmd, FQueueKey Key)
{
	FEncounterCommandReply& Reply = Replies.AddDefaulted_GetRef();
	Reply.Key = Key;
	Reply.Pawn = Cmd.Pawn.Pawn.Get();
	Reply.Kind = static_cast<ECommandKind>(Cmd.Kind);
	Reply.bExecuted = true;
	Reply.Sink = Cmd.Sink;
	Reply.ClientSeq = Cmd.ClientSeq;
}

EResolveResult FAttackResolverCore::Resolve(const FQueueItem& Item, FQueueKey Key)
{
	if (Deps.Clock == nullptr || Deps.Queue == nullptr || Deps.Roster == nullptr || Deps.Pawns == nullptr || Deps.Defence == nullptr)
	{
		return EResolveResult::Voided; // a wiring defect: nothing executes without the required seams
	}
	if (Item.Type == EQueueItemType::HoldTimer)
	{
		const FHoldTimerPayload* Hold = Item.Payload.TryGet<FHoldTimerPayload>();
		if (Hold != nullptr && Hold->ActionType == static_cast<uint8>(EActionType::Prepare))
		{
			return ResolveHoldTimer(Item, *Hold, Key);
		}
		return FallbackHolds != nullptr ? FallbackHolds->Resolve(Item, Key) : EResolveResult::Voided;
	}
	if (Item.Type == EQueueItemType::PawnCommand)
	{
		if (const FCommandPayload* Cmd = Item.Payload.TryGet<FCommandPayload>())
		{
			return ResolveCommand(Item, *Cmd, Key);
		}
	}
	return EResolveResult::Voided;
}

void FAttackResolverCore::OnRejectedLatched(const FQueueItem& Item, FQueueKey Key)
{
	const FCommandPayload* Cmd = Item.Type == EQueueItemType::PawnCommand ? Item.Payload.TryGet<FCommandPayload>() : nullptr;
	const ECommandKind Kind = Cmd != nullptr ? static_cast<ECommandKind>(Cmd->Kind) : ECommandKind::Traverse;
	if (Cmd != nullptr && (Kind == ECommandKind::Attack || Kind == ECommandKind::PrepareBegin || Kind == ECommandKind::PrepareEnd))
	{
		// No lockout, pose, outcome or result: only the held owner reply NotNow (released at the round-end step).
		FEncounterCommandReply& Reply = Replies.AddDefaulted_GetRef();
		Reply.Key = Key;
		Reply.Pawn = Cmd->Pawn.Pawn.Get();
		Reply.Kind = Kind;
		Reply.bExecuted = false;
		Reply.Code = ECommandRejectCode::NotNow;
		Reply.bHeld = true;
		Reply.Sink = Cmd->Sink;
		Reply.ClientSeq = Cmd->ClientSeq;
		return;
	}
	IQueueResolver* Fallback = Item.Type == EQueueItemType::HoldTimer ? FallbackHolds : FallbackCommands;
	if (Fallback != nullptr)
	{
		Fallback->OnRejectedLatched(Item, Key);
	}
}

EResolveResult FAttackResolverCore::ResolveCommand(const FQueueItem& Item, const FCommandPayload& Cmd, FQueueKey Key)
{
	const ECommandKind Kind = static_cast<ECommandKind>(Cmd.Kind);
	switch (Kind)
	{
	case ECommandKind::Attack:
		return ResolveAttack(Item, Cmd, Key);
	case ECommandKind::PrepareBegin:
		return ResolvePrepareBegin(Item, Cmd, Key);
	case ECommandKind::PrepareEnd:
		return ResolvePrepareEnd(Item, Cmd, Key);
	default:
		break;
	}
	// The other discrete commands share the action slot: the lockout and the counter make them Busy (ASN-R14, R15).
	if (const TOptional<ECommandRejectCode> Code = CheckSlot(Cmd.Pawn.Pawn.Get(), Kind))
	{
		return Void(Cmd, Key, *Code);
	}
	if (FallbackCommands != nullptr)
	{
		return FallbackCommands->Resolve(Item, Key);
	}
	return Void(Cmd, Key, ECommandRejectCode::NotNow);
}

EResolveResult FAttackResolverCore::ResolveAttack(const FQueueItem& Item, const FCommandPayload& Cmd, FQueueKey Key)
{
	APawn* Attacker = Cmd.Pawn.Pawn.Get();
	FEncounterPawnFacts AttackerFacts;
	// ASN-R2: the live re-check at the slot. An attacker killed, reversed or struck out earlier in the drain is NotNow.
	if (Attacker == nullptr || !Deps.Pawns->GetFacts(Attacker, AttackerFacts) || Roles.IsDead(Attacker) || !Deps.Roster->IsLegalityActive(Attacker))
	{
		return Void(Cmd, Key, ECommandRejectCode::NotNow);
	}
	if (AttackerFacts.bRunning || AttackerFacts.bTraversing)
	{
		return Void(Cmd, Key, ECommandRejectCode::NotNow); // IIF R7: not while running or traversing
	}
	if (const TOptional<ECommandRejectCode> Code = CheckSlot(Attacker, ECommandKind::Attack))
	{
		return Void(Cmd, Key, *Code); // Striking (the probe limiter) or Countering
	}

	// Executed, whatever the target (ASN-R1). Bracing never protects a striker: its own brace is superseded (ASN-R13).
	const double ExecTime = Now();
	if (GetRole(Attacker) == EStrikeRole::Bracing)
	{
		EndWindow(Attacker, Key);
		Roles.EndBrace(Attacker);
		if (Deps.Stance != nullptr)
		{
			Deps.Stance->SetBraceStance(Attacker, false);
		}
		if (Deps.Events != nullptr)
		{
			Deps.Events->OnActionCancelled(Attacker, EActionType::Prepare, ECancelReason::Superseded);
		}
	}
	const EStrikeReachVariant Variant = Deps.Bushes != nullptr && Deps.Bushes->IsConcealed(Attacker) ? EStrikeReachVariant::BushStrike : EStrikeReachVariant::Standard;
	const double Contact = ExecTime + Params.ContactSec(Variant);

	// (1) Victim and outcome, on the state after every item with a lower key (F4).
	FVictimRequest Request;
	Request.Attacker = AttackerFacts;
	Request.WorldPoint = Cmd.WorldPoint;
	Request.PawnRef = Cmd.Target.Kind == ESubjectRefKind::Pawn ? Cmd.Target.Pawn.Pawn.Get() : nullptr;
	Request.Key = Key;
	Request.Variant = Variant;
	const FVictimResolution Victim = FVictimResolverCore::Resolve(Params, MakeResolverDeps(), Request);

	FAttackResolution Resolution;
	Resolution.Key = Key;
	Resolution.Attacker = Attacker;
	Resolution.Victim = Victim.Victim;
	Resolution.Rule = Victim.Rule;
	Resolution.BushId = Victim.BushId;
	Resolution.Variant = Variant;
	Resolution.Reach = Victim.Reach;
	Resolution.ExecServerTime = ExecTime;
	Resolution.ContactServerTime = Contact;
	Resolution.AttackerSink = Cmd.Sink;
	Resolution.StrikeResult.ClientCommandSeq = Cmd.ClientSeq;
	Resolution.StrikeResult.ContactServerTime = Contact;
	Resolution.StrikeResult.Result = EStrikeResult::NoContact;
	if (Victim.Victim != nullptr)
	{
		const bool bBraced = FBracedAtAttackCore::IsBraced(GetRole(Victim.Victim), Deps.Defence->Find(Victim.Victim), Key);
		Resolution.Outcome = bBraced ? EAttackOutcome::Reversal : EAttackOutcome::Kill;
		Resolution.bVictimConcealed = !bBraced && Deps.Bushes != nullptr && Deps.Bushes->IsConcealed(Victim.Victim);
		// The fallen pawn is Dead from this slot (its death write still pending), with its scheduled death time.
		Roles.SetDead(bBraced ? Attacker : Victim.Victim, Contact);
	}

	// Every executed Attack: Striking and the lockout stamp (F3), and the outcome-blind execution event.
	if (FDefenceStateCore* AttackerState = Deps.Defence->FindOrAdd(Attacker))
	{
		AttackerState->BeginLockout(ExecTime, Params.Public.AttackAttemptLockoutSec);
	}
	Roles.SetStriking(Attacker, ExecTime + Params.Public.AttackAttemptLockoutSec);
	if (Deps.Events != nullptr)
	{
		Deps.Events->OnActionStarted(Attacker, EActionType::Attack);
	}

	if (Resolution.Outcome == EAttackOutcome::Kill)
	{
		ApplyKill(Item, Key, Attacker, Resolution);
	}
	else if (Resolution.Outcome == EAttackOutcome::Reversal)
	{
		ApplyReversal(Item, Key, Attacker, Victim.Victim, Resolution);
	}

	Resolutions.Add(Resolution);
	RecordExecuted(Cmd, Key);
	return EResolveResult::Executed;
}

void FAttackResolverCore::ApplyKill(const FQueueItem& Item, FQueueKey Key, APawn* Attacker, FAttackResolution& Resolution)
{
	APawn* Victim = Resolution.Victim;
	const EVictimKind Kind = ToVictimKind(Deps.Roster->ResolveVictimKind(Victim).Kind); // before the transition
	Resolution.FallenKind = Kind;
	Resolution.StrikeResult.Result = Kind == EVictimKind::Player ? EStrikeResult::RivalEliminated : EStrikeResult::WrongTarget;
	// (3) The transition first, so Round's elimination check and Penalty see one consistent Roster (an informant kill is
	// not a transition, Roster story 020).
	if (Kind == EVictimKind::Player)
	{
		Deps.Roster->RequestElimination(Victim, ERosterReason::Assassinated, Resolution.ContactServerTime);
	}
	// (4) The card ledger and (5) Concealment's occupancy record are server-only state: in the slot.
	if (Deps.Cards != nullptr)
	{
		Deps.Cards->VacateCard(Victim);
	}
	if (Resolution.bVictimConcealed && Deps.Bushes != nullptr)
	{
		Deps.Bushes->RemoveOccupant(Victim);
	}
	// (6) OnPawnKilled for every kind.
	OnPawnKilled.Broadcast(Victim, Attacker, Kind, EKillCause::Strike);
	// (7) The contact-time presentation write (story asn-013 applies it).
	StageDeath(Victim, Resolution.ContactServerTime);
	// (9) Round's Event Log row.
	LogRow(Item, Key, /*bReversal*/ false, Attacker, Victim, Kind);
}

void FAttackResolverCore::ApplyReversal(const FQueueItem& Item, FQueueKey Key, APawn* Attacker, APawn* Defender, FAttackResolution& Resolution)
{
	// The attacker is eliminated whoever reversed it (a player or an NPC): the trigger reads team counts (Round F2).
	const EVictimKind Kind = ToVictimKind(Deps.Roster->ResolveVictimKind(Attacker).Kind);
	Resolution.FallenKind = Kind;
	Resolution.StrikeResult.Result = EStrikeResult::Reversed;
	Resolution.DefenderSink = Roles.GetBraceSink(Defender);
	if (Kind == EVictimKind::Player)
	{
		Deps.Roster->RequestElimination(Attacker, ERosterReason::ReversedAgainst, Resolution.ContactServerTime);
	}
	if (Deps.Cards != nullptr)
	{
		Deps.Cards->VacateCard(Attacker);
	}
	OnPawnKilled.Broadcast(Attacker, Defender, Kind, EKillCause::Reversal);
	// The defender: its brace is consumed and it counters (never Disabled, never charged, NOT protected; ASN-R14).
	EndWindow(Defender, Key);
	Roles.SetCountering(Defender, Resolution.ExecServerTime + Params.Public.CounterDurationSec);
	if (Deps.Events != nullptr)
	{
		Deps.Events->OnActionCancelled(Defender, EActionType::Prepare, ECancelReason::Reversed);
	}
	// Staged: DeathWrite{A} before CounterPose{V} (TypeRank), the same server step.
	StageDeath(Attacker, Resolution.ContactServerTime);
	FStagedWrite Counter;
	Counter.Type = EStagedWriteType::CounterPose;
	Counter.ServerTime = Resolution.ContactServerTime - Params.Server.StrikeNetLeadSec;
	FCounterPayload Payload;
	Payload.Defender.Pawn = Defender;
	Payload.ContactServerTime = Resolution.ContactServerTime;
	Counter.Payload.Set<FCounterPayload>(Payload);
	Deps.Queue->StageWrite(Counter);
	LogRow(Item, Key, /*bReversal*/ true, Attacker, Defender, Kind);
}

void FAttackResolverCore::StageDeath(APawn* Fallen, double ContactServerTime)
{
	FStagedWrite Death;
	Death.Type = EStagedWriteType::DeathWrite;
	Death.ServerTime = ContactServerTime - Params.Server.StrikeNetLeadSec;
	FDeathWritePayload Payload;
	Payload.Victim.Pawn = Fallen;
	Payload.DeathServerTime = ContactServerTime;
	Death.Payload.Set<FDeathWritePayload>(Payload);
	Deps.Queue->StageWrite(Death);
}

void FAttackResolverCore::LogRow(const FQueueItem& Item, FQueueKey Key, bool bReversal, APawn* Attacker, APawn* Other, EVictimKind Kind)
{
	if (Deps.Log == nullptr)
	{
		return;
	}
	FRoundLogEntry Entry;
	Entry.Key = Key;
	Entry.ServerTime = Now();
	Entry.RoundEpoch = Item.Expected.RoundEpoch;
	Entry.SourceMailbox = Item.Source;
	Entry.Participants[0] = Deps.Roster->GetParticipant(Attacker).Value;
	Entry.Participants[1] = Deps.Roster->GetParticipant(Other).Value;
	Entry.Data[0] = Deps.Roster->GetTeamIndex(Attacker);
	if (bReversal)
	{
		Entry.Type = ERoundLogType::Reversal;
		Entry.Data[1] = Deps.Roster->GetTeamIndex(Other);
	}
	else
	{
		Entry.Type = ERoundLogType::KillResolved;
		Entry.Code = static_cast<uint16>(Kind == EVictimKind::Player ? ERoundKillResult::ParticipantKilled : ERoundKillResult::WrongTarget);
		Entry.Data[1] = static_cast<uint32>(Kind);
	}
	Deps.Log->Append(Entry);
}

void FAttackResolverCore::EndWindow(APawn* Pawn, FQueueKey Key)
{
	if (FDefenceStateCore* State = Deps.Defence->Find(Pawn))
	{
		State->EndWindow(Key);
	}
}

EResolveResult FAttackResolverCore::ResolvePrepareBegin(const FQueueItem& Item, const FCommandPayload& Cmd, FQueueKey Key)
{
	APawn* Pawn = Cmd.Pawn.Pawn.Get();
	FEncounterPawnFacts Facts;
	if (Pawn == nullptr || !Deps.Pawns->GetFacts(Pawn, Facts) || Roles.IsDead(Pawn) || !Deps.Roster->IsLegalityActive(Pawn))
	{
		return Void(Cmd, Key, ECommandRejectCode::NotNow); // a victim struck earlier in this drain writes no stamp (F2, ASN-R5)
	}
	if (Facts.bRunning || Facts.bTraversing || (Deps.Bushes != nullptr && Deps.Bushes->IsConcealed(Pawn)))
	{
		return Void(Cmd, Key, ECommandRejectCode::NotNow); // IIF R7, R9; CON-R7
	}
	if (const TOptional<ECommandRejectCode> Code = CheckSlot(Pawn, ECommandKind::PrepareBegin))
	{
		return Void(Cmd, Key, *Code);
	}
	const double Time = Now();
	const FDefenceStateCore* Existing = Deps.Defence->Find(Pawn);
	if (Existing != nullptr && !Existing->CanPrepare(Time, Params.Public.DefenceCooldownSec))
	{
		return Void(Cmd, Key, ECommandRejectCode::Cooldown); // F3; nothing resets, extends or refunds the stamp
	}
	FDefenceStateCore* State = Deps.Defence->FindOrAdd(Pawn);
	if (State == nullptr || State->BeginPrepare(Time, Params.Public.DefenceCooldownSec, Key) != EDefenceBeginResult::Executed)
	{
		return Void(Cmd, Key, ECommandRejectCode::Cooldown);
	}
	Roles.SetBracing(Pawn, Cmd.Sink);
	if (Deps.Stance != nullptr)
	{
		Deps.Stance->SetBraceStance(Pawn, true);
	}
	if (Deps.Events != nullptr)
	{
		Deps.Events->OnActionStarted(Pawn, EActionType::Prepare);
	}
	// The window's cap: HoldTimer(CapEnd) at T_act + DefenceDurationSec (IIF hold stand-in).
	FHoldTimerPayload Hold;
	Hold.Pawn = Cmd.Pawn;
	Hold.Kind = EHoldTimerKind::CapEnd;
	Hold.ActionType = static_cast<uint8>(EActionType::Prepare);
	Hold.BeginKey = Key;
	Deps.Queue->ScheduleAt(State->GetCapEndServerTime(Params.Public.DefenceDurationSec).Get(Time), FQueueItem::Make(EQueueItemType::HoldTimer, Item.Expected, FMailboxId::ServerLocal(), Hold));
	RecordExecuted(Cmd, Key);
	return EResolveResult::Executed;
}

EResolveResult FAttackResolverCore::ResolvePrepareEnd(const FQueueItem& Item, const FCommandPayload& Cmd, FQueueKey Key)
{
	APawn* Pawn = Cmd.Pawn.Pawn.Get();
	if (const TOptional<ECommandRejectCode> Code = CheckSlot(Pawn, ECommandKind::PrepareEnd))
	{
		return Void(Cmd, Key, *Code);
	}
	const FDefenceStateCore* State = Deps.Defence->Find(Pawn);
	if (GetRole(Pawn) != EStrikeRole::Bracing || State == nullptr || !State->IsWindowOpenAt(Key))
	{
		return Void(Cmd, Key, ECommandRejectCode::NotNow); // nothing open to release
	}
	// The release protects until arrival + ReleaseDebounceSec (ASN-R3, R5): the window ends at the debounce item's key.
	FHoldTimerPayload Hold;
	Hold.Pawn = Cmd.Pawn;
	Hold.Kind = EHoldTimerKind::DebounceExpiry;
	Hold.ActionType = static_cast<uint8>(EActionType::Prepare);
	Hold.BeginKey = State->GetWindow().BeginKey;
	Deps.Queue->ScheduleAt(Now() + Params.ReleaseDebounceSec, FQueueItem::Make(EQueueItemType::HoldTimer, Item.Expected, FMailboxId::ServerLocal(), Hold));
	RecordExecuted(Cmd, Key);
	return EResolveResult::Executed;
}

EResolveResult FAttackResolverCore::ResolveHoldTimer(const FQueueItem& Item, const FHoldTimerPayload& Hold, FQueueKey Key)
{
	APawn* Pawn = Hold.Pawn.Pawn.Get();
	FDefenceStateCore* State = Pawn != nullptr ? Deps.Defence->Find(Pawn) : nullptr;
	if (State == nullptr || State->GetWindow().BeginKey != Hold.BeginKey)
	{
		return EResolveResult::Voided; // a timer of an older activation
	}
	if (State->GetWindow().EndKey.IsSet())
	{
		// The window already ended (the other end item, the pawn's own Attack, or a reversal): the planted stance of a
		// reversing defender ends here; the CounterPose write (story asn-013) owns its pose from contact.
		if (Deps.Stance != nullptr && !Roles.IsDead(Pawn))
		{
			Deps.Stance->SetBraceStance(Pawn, false);
		}
		return EResolveResult::Voided;
	}
	State->EndWindow(Key);
	Roles.EndBrace(Pawn);
	if (Deps.Stance != nullptr)
	{
		Deps.Stance->SetBraceStance(Pawn, false);
	}
	if (Deps.Events != nullptr)
	{
		if (Hold.Kind == EHoldTimerKind::CapEnd)
		{
			Deps.Events->OnActionCompleted(Pawn, EActionType::Prepare);
		}
		else
		{
			Deps.Events->OnActionCancelled(Pawn, EActionType::Prepare, ECancelReason::Released);
		}
	}
	return EResolveResult::Executed;
}

void FAttackResolverCore::Reset()
{
	Roles.Reset();
	Resolutions.Reset();
	Replies.Reset();
}
