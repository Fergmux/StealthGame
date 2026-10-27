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
#include "Server/StealthFaultSink.h"

#if WITH_STEALTH_TEST_SEAMS
bool StealthEncounterTestSeams::bWriteOutcomeIntoVariant = false;
#endif

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
	// The IIF's reply of the slot (ADR-0011 section 8): the owner learns the code at drain end with every other reply.
	AppendReply(Cmd.Sink, Key, FCommandReply::MakeRejected(Cmd.ClientSeq, Code));
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
		if (Hold != nullptr && Hold->ActionType == static_cast<uint8>(EActionType::Attack))
		{
			return ResolvePoseEnd(*Hold);
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
	if (Item.Type == EQueueItemType::TimerFired)
	{
		if (const FTimerPayload* Timer = Item.Payload.TryGet<FTimerPayload>())
		{
			return ResolveBodyTimer(*Timer);
		}
	}
	return EResolveResult::Voided;
}

void FAttackResolverCore::OnRejectedLatched(const FQueueItem& Item, FQueueKey Key)
{
	if (Item.Type == EQueueItemType::TimerFired)
	{
		// A body item after the Ended latch changes nothing: the body keeps its controller and collision and lies until
		// Reset (ADR-0018 section 6.3; the round-end step froze every pawn's movement). Only the observation is kept.
		const FTimerPayload* Timer = Item.Payload.TryGet<FTimerPayload>();
		if (FEncounterBody* Body = Timer != nullptr && Timer->Generation == BodyGeneration ? FindBodyMutable(Timer->Subject.Pawn.Pawn.Get()) : nullptr)
		{
			++Body->LatchedItems;
		}
		return;
	}
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
		SetBraceStance(Attacker, false, /*bWritePose: the Strike write below supersedes the Brace pose*/ false);
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

	// Slot step 2 (ADR-0018 section 2.1, story asn-012): the attacker's one outcome-blind FActionState write. Its every
	// field (variant, start, contact, WarpPoint) comes from the request, the attacker's capsule and public geometry, so
	// a kill, a whiff and a reversal at the same request write the same bits; nothing is written on any other pawn here.
	const FStrikeWarpPointResult Warp = ComputeWarp(AttackerFacts, Cmd.WorldPoint, Victim, Variant);
	Resolution.StrikeVariantBits = Warp.Variant;
	Resolution.WarpPoint = Warp.WarpPoint;
	Resolution.WarpTarget = Warp.TargetLocation;
	uint8 WireVariant = Warp.Variant;
#if WITH_STEALTH_TEST_SEAMS
	if (StealthEncounterTestSeams::bWriteOutcomeIntoVariant)
	{
		WireVariant |= static_cast<uint8>((static_cast<uint8>(Resolution.Outcome) & 0x3) << 3); // the forbidden leak (AC2 seam)
	}
#endif
	WritePose(Attacker, EActionPose::Strike, WireVariant, ExecTime, Contact, Warp.WarpPoint);
	SchedulePoseEnd(Item, Attacker, Key, ExecTime + Params.Public.AttackAttemptLockoutSec, StrikePoseEndEpoch);

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

	// (8) The owner results, after the OnPawnKilled fan-out (Penalty's tally and exile have applied), in this slot.
	DeliverOwnerResults(Cmd, Key, Resolution);

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
	if (Resolution.bVictimConcealed)
	{
		DiedConcealed.Add(FObjectKey(Victim)); // FDeathState.bDiedConcealed at its death write (story asn-023 reads it)
	}
	// (6) OnPawnKilled for every kind.
	OnPawnKilled.Broadcast(Victim, Attacker, Kind, EKillCause::Strike);
	// (7) The contact-time presentation write (story asn-013 applies it) and the body sequence (story asn-019).
	StageDeath(Item, Victim, Resolution.ContactServerTime);
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
	Resolution.DefenderSeq = Roles.GetBraceSeq(Defender);
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
	SchedulePoseEnd(Item, Defender, Key, Resolution.ExecServerTime + Params.Public.CounterDurationSec, CounterPoseEndEpoch);
	if (Deps.Events != nullptr)
	{
		Deps.Events->OnActionCancelled(Defender, EActionType::Prepare, ECancelReason::Reversed);
	}
	// Staged: DeathWrite{A} before CounterPose{V} (TypeRank), the same server step.
	StageDeath(Item, Attacker, Resolution.ContactServerTime);
	FStagedWrite Counter;
	Counter.Type = EStagedWriteType::CounterPose;
	Counter.ServerTime = Resolution.ContactServerTime - Params.Server.StrikeNetLeadSec;
	FCounterPayload Payload;
	Payload.Defender.Pawn = Defender;
	Payload.ContactServerTime = Resolution.ContactServerTime;
	Counter.Payload.Set<FCounterPayload>(Payload);
	if (Deps.Queue->StageWrite(Counter))
	{
		PendingCounters.Add(FObjectKey(Defender));
	}
	LogRow(Item, Key, /*bReversal*/ true, Attacker, Defender, Kind);
}

void FAttackResolverCore::StageDeath(const FQueueItem& Item, APawn* Fallen, double ContactServerTime)
{
	FStagedWrite Death;
	Death.Type = EStagedWriteType::DeathWrite;
	Death.ServerTime = ContactServerTime - Params.Server.StrikeNetLeadSec;
	FDeathWritePayload Payload;
	Payload.Victim.Pawn = Fallen;
	Payload.DeathServerTime = ContactServerTime;
	Death.Payload.Set<FDeathWritePayload>(Payload);
	if (Deps.Queue->StageWrite(Death))
	{
		PendingDeaths.Add(FObjectKey(Fallen));
	}

	// Story asn-019: the body sequence, the same two due-time items for every victim kind (ADR-0018 section 6). The
	// controller keeps the dying pawn until the release, so every ownership change follows the visible fall.
	if (Deps.BodyHandOver == nullptr || Fallen == nullptr || FindBodyMutable(Fallen) != nullptr)
	{
		return;
	}
	FEncounterBody& Body = Bodies.AddDefaulted_GetRef();
	Body.Pawn = Fallen;
	Body.DeathServerTime = ContactServerTime;
	Body.ReleaseDueServerTime = ContactServerTime + Params.BodyReleaseDelaySec();
	Body.RemovalDueServerTime = ContactServerTime + Params.BodyRemovalDelaySec();
	FTimerPayload Timer;
	Timer.Subject = FSubjectRef::ForPawn(FPawnHandle{ Fallen });
	Timer.Generation = BodyGeneration;
	Timer.Kind = ETimerKind::BodyRelease;
	Deps.Queue->ScheduleAt(Body.ReleaseDueServerTime, FQueueItem::Make(EQueueItemType::TimerFired, Item.Expected, FMailboxId::ServerLocal(), Timer));
	Timer.Kind = ETimerKind::BodyRemoval;
	Deps.Queue->ScheduleAt(Body.RemovalDueServerTime, FQueueItem::Make(EQueueItemType::TimerFired, Item.Expected, FMailboxId::ServerLocal(), Timer));
}

void FAttackResolverCore::DeliverOwnerResults(const FCommandPayload& Cmd, FQueueKey Key, FAttackResolution& Resolution)
{
	// Every Result asks the source (a uniform call pattern: the call itself never tells which outcome it was).
	FStrikeTally Tally;
	if (Deps.Tally != nullptr)
	{
		Tally = Deps.Tally->GetTallyForStrike(Resolution.Attacker, Resolution.StrikeResult.Result, Resolution.StrikeResult.ClientCommandSeq);
	}
	else
	{
		ReportFault(EncounterFaultDetail::NoTallySource);
	}
	Resolution.StrikeResult.TallyAfter = Tally.TallyAfter;
	Resolution.StrikeResult.bExiled = Tally.bExiled;
	AppendReply(Cmd.Sink, Key, FCommandReply::MakeStrikeResult(Resolution.StrikeResult));
	if (Resolution.HasDefenceResult())
	{
		FDefenceResult Defence;
		Defence.Result = EDefenceResult::Reversed;
		AppendReply(Resolution.DefenderSink, Key, FCommandReply::MakeDefenceResult(Resolution.DefenderSeq, Defence));
	}
}

void FAttackResolverCore::AppendReply(FReplySinkId Sink, FQueueKey Key, const FCommandReply& Reply)
{
	if (Deps.Replies != nullptr)
	{
		Deps.Replies->AppendReply(Sink, Key, Reply); // the null sink discards in the table, never by a check here
	}
}

void FAttackResolverCore::ReportFault(int64 Detail) const
{
	if (Deps.Faults != nullptr)
	{
		Deps.Faults->Report(EStealthFault::DeadRegistration, TEXT("Assassination"), Detail);
	}
}

FEncounterBody* FAttackResolverCore::FindBodyMutable(const APawn* Pawn)
{
	return Pawn != nullptr ? Bodies.FindByPredicate([Pawn](const FEncounterBody& Body) { return Body.Pawn == Pawn; }) : nullptr;
}

const FEncounterBody* FAttackResolverCore::FindBody(const APawn* Pawn) const
{
	return const_cast<FAttackResolverCore*>(this)->FindBodyMutable(Pawn);
}

EResolveResult FAttackResolverCore::ResolveBodyTimer(const FTimerPayload& Timer)
{
	APawn* Pawn = Timer.Subject.Pawn.Pawn.Get();
	FEncounterBody* Body = Timer.Generation == BodyGeneration ? FindBodyMutable(Pawn) : nullptr;
	if (Body == nullptr || Pawn == nullptr)
	{
		return EResolveResult::Voided; // a reset round's item, or a body already gone
	}
	if (Timer.Kind == ETimerKind::BodyRelease)
	{
		if (Body->IsReleased())
		{
			return EResolveResult::Voided;
		}
		Body->ReleasedAtServerTime = Now();
		if (Deps.BodyHandOver != nullptr)
		{
			Deps.BodyHandOver->ReleaseBody(Pawn);
		}
		else
		{
			ReportFault(EncounterFaultDetail::NoBodyHandOver);
		}
		OnBodyReleased.Broadcast(Pawn);
		return EResolveResult::Executed;
	}
	if (Timer.Kind == ETimerKind::BodyRemoval)
	{
		if (Body->IsRemoved())
		{
			return EResolveResult::Voided;
		}
		Body->RemovedAtServerTime = Now();
		// The observer runs for every body (Concealment clears its killed-concealed flags), then the teardown.
		if (Deps.BodyObserver != nullptr)
		{
			Deps.BodyObserver->OnBodyRemovalCommitted(Pawn);
		}
		else
		{
			ReportFault(EncounterFaultDetail::NoBodyRemovalObserver);
		}
		OnBodyRemoved.Broadcast(Pawn);
		if (Deps.BodyHandOver != nullptr)
		{
			Deps.BodyHandOver->RequestTeardown(Pawn);
		}
		return EResolveResult::Executed;
	}
	return EResolveResult::Voided;
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
	Roles.SetBracing(Pawn, Cmd.Sink, Cmd.ClientSeq);
	// Story asn-016: the owner's only defence datum, from the same stamp as the Cooldown verdict (F3).
	if (Deps.OwnerCooldown != nullptr)
	{
		if (const TOptional<double> CooldownEnd = State->GetCooldownEndServerTime(Params.Public.DefenceCooldownSec))
		{
			Deps.OwnerCooldown->PublishCooldownEnd(Pawn, CooldownEnd.GetValue());
		}
	}
	SetBraceStance(Pawn, true);
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
		if (!Roles.IsDead(Pawn))
		{
			SetBraceStance(Pawn, false);
		}
		return EResolveResult::Voided;
	}
	State->EndWindow(Key);
	Roles.EndBrace(Pawn);
	SetBraceStance(Pawn, false);
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
	DiedConcealed.Reset();
	AppliedWrites.Reset();
	PendingDeaths.Reset();
	PendingCounters.Reset();
	if (Deps.Defence != nullptr)
	{
		Deps.Defence->ResetAll(); // every stamp, window and lockout: none
	}
	if (Deps.OwnerCooldown != nullptr)
	{
		Deps.OwnerCooldown->ResetAll();
	}
	// Bodies go with the round (ADR-0018 section 6.4): the ones still lying are torn down now; the reset round's body items
	// can never resolve again (generation), whatever the queue still holds.
	TArray<FEncounterBody> Lying = MoveTemp(Bodies);
	Bodies.Reset();
	++BodyGeneration;
	if (Deps.BodyHandOver != nullptr)
	{
		for (const FEncounterBody& Body : Lying)
		{
			if (!Body.IsRemoved() && Body.Pawn != nullptr)
			{
				Deps.BodyHandOver->RequestTeardown(Body.Pawn);
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------
// Story asn-012: the slot write, the warp point, the stance through FActionState
// ---------------------------------------------------------------------------------------------------------

FStrikeWarpPointResult FAttackResolverCore::ComputeWarp(const FEncounterPawnFacts& Attacker, const FVector& WorldPoint, const FVictimResolution& Victim, EStrikeReachVariant Variant) const
{
	FStrikeWarpPointInput In;
	In.AttackerLocation = Attacker.Location;
	In.AttackerFacingYawDeg = Attacker.FacingYawDeg;
	In.WorldPoint = WorldPoint;
	In.bBushPreface = Variant == EStrikeReachVariant::BushStrike;
	if (Victim.bRefHonoured)
	{
		// An honoured reference is knowable to the attacker (CanViewerKnow passed): its F1 extrapolation, hit or whiff.
		In.Source = EStrikeWarpSource::PawnRef;
		In.RefVictimAtContact = Victim.RefAtContact;
	}
	else if (Victim.bInBushZone)
	{
		// Rule 2, hit or whiff: the WorldPoint and the public footprint only; never an occupant's position.
		In.Source = EStrikeWarpSource::BushSpot;
		FBox2D Footprint(ForceInit);
		if (Deps.Bushes != nullptr && Deps.Bushes->GetFootprint(Victim.BushId, Footprint))
		{
			In.BushFootprint = Footprint;
		}
	}
	else
	{
		// Rule 3 or 4 (an unhonoured, culled reference falls through here too): the submitted WorldPoint.
		In.Source = EStrikeWarpSource::OpenSpot;
	}
	return FStrikeWarpPointCore::Evaluate(FStrikeWarpPointParams::FromEncounter(Params), In);
}

void FAttackResolverCore::WritePose(APawn* Pawn, EActionPose Pose, uint8 Variant, double Start, double Contact, const FFixedWarpOffset& WarpPoint)
{
	if (Deps.PawnState == nullptr || Pawn == nullptr)
	{
		return;
	}
	FActionStateWrite Write;
	Write.Pose = Pose;
	Write.Variant = Variant;
	Write.StartServerTime = Start;
	Write.ContactServerTime = Contact;
	Write.WarpPoint = WarpPoint;
	Deps.PawnState->WriteActionState(Pawn, Write);
}

void FAttackResolverCore::SetBraceStance(APawn* Pawn, bool bBraced, bool bWritePose)
{
	if (Deps.Stance != nullptr)
	{
		Deps.Stance->SetBraceStance(Pawn, bBraced);
	}
	if (!bWritePose || Deps.PawnState == nullptr)
	{
		return;
	}
	if (bBraced)
	{
		WritePose(Pawn, EActionPose::Brace, 0, Now(), -1.0, FFixedWarpOffset());
		return;
	}
	// Never clears a pose another writer set, never touches a reversing defender before its CounterPose (ADR-0018 section
	// 3: nothing changes on a defender before the death write) and never a fallen pawn.
	if (Deps.PawnState->GetPose(Pawn) == EActionPose::Brace && !Roles.IsDead(Pawn) && GetRole(Pawn) != EStrikeRole::Countering)
	{
		WritePose(Pawn, EActionPose::None, 0, Now(), -1.0, FFixedWarpOffset());
	}
}

void FAttackResolverCore::SchedulePoseEnd(const FQueueItem& Item, APawn* Pawn, FQueueKey Key, double Due, uint32 Epoch)
{
	if (Deps.PawnState == nullptr || Pawn == nullptr)
	{
		return; // nothing on the wire to end (the headless rule tests)
	}
	FHoldTimerPayload Hold;
	Hold.Pawn.Pawn = Pawn;
	Hold.Kind = EHoldTimerKind::Complete;
	Hold.ActionType = static_cast<uint8>(EActionType::Attack);
	Hold.ActionEpoch = Epoch;
	Hold.BeginKey = Key;
	Deps.Queue->ScheduleAt(Due, FQueueItem::Make(EQueueItemType::HoldTimer, Item.Expected, FMailboxId::ServerLocal(), Hold));
}

EResolveResult FAttackResolverCore::ResolvePoseEnd(const FHoldTimerPayload& Hold)
{
	APawn* Pawn = Hold.Pawn.Pawn.Get();
	if (Pawn == nullptr || Deps.PawnState == nullptr || Roles.IsDead(Pawn))
	{
		return EResolveResult::Voided; // a fallen pawn keeps its last pose under the death write
	}
	const EActionPose Expected = Hold.ActionEpoch == CounterPoseEndEpoch ? EActionPose::Counter : EActionPose::Strike;
	if (Deps.PawnState->GetPose(Pawn) != Expected)
	{
		return EResolveResult::Voided; // a later write owns the pose
	}
	WritePose(Pawn, EActionPose::None, 0, Now(), -1.0, FFixedWarpOffset());
	return EResolveResult::Executed;
}

// ---------------------------------------------------------------------------------------------------------
// Story asn-013: the DeathWrite and CounterPose applier (apply-only)
// ---------------------------------------------------------------------------------------------------------

void FAttackResolverCore::ApplyStagedWrite(const FStagedWrite& Write)
{
	if (Write.Type == EStagedWriteType::DeathWrite)
	{
		const FDeathWritePayload* Payload = Write.Payload.TryGet<FDeathWritePayload>();
		APawn* Victim = Payload != nullptr ? Payload->Victim.Pawn.Get() : nullptr;
		if (Victim == nullptr || PendingDeaths.Remove(FObjectKey(Victim)) == 0)
		{
			return; // a stale handle (the body is gone), or a write of a reset round (story asn-019): nothing to write
		}
		FDeathState State;
		State.DeathServerTime = Payload->DeathServerTime;
		State.bDiedConcealed = DiedConcealed.Contains(FObjectKey(Victim));
		if (Deps.PawnState != nullptr && !Deps.PawnState->WriteDeath(Victim, State))
		{
			return; // already written (one death write per pawn)
		}
		FAppliedEncounterWrite& Applied = AppliedWrites.AddDefaulted_GetRef();
		Applied.Type = Write.Type;
		Applied.Pawn = Victim;
		Applied.AppliedServerTime = Now();
		Applied.EventServerTime = State.DeathServerTime;
		Applied.bDiedConcealed = State.bDiedConcealed;
		OnDeathWriteApplied.Broadcast(Victim, State.DeathServerTime, State.bDiedConcealed);
		return;
	}
	if (Write.Type == EStagedWriteType::CounterPose)
	{
		const FCounterPayload* Payload = Write.Payload.TryGet<FCounterPayload>();
		APawn* Defender = Payload != nullptr ? Payload->Defender.Pawn.Get() : nullptr;
		if (Defender == nullptr || PendingCounters.Remove(FObjectKey(Defender)) == 0 || Roles.IsDead(Defender))
		{
			return; // gone, a reset round's write, or struck down since: its own death write owns it
		}
		WritePose(Defender, EActionPose::Counter, 0, Now(), Payload->ContactServerTime, FFixedWarpOffset());
		FAppliedEncounterWrite& Applied = AppliedWrites.AddDefaulted_GetRef();
		Applied.Type = Write.Type;
		Applied.Pawn = Defender;
		Applied.AppliedServerTime = Now();
		Applied.EventServerTime = Payload->ContactServerTime;
		OnCounterPoseApplied.Broadcast(Defender, Payload->ContactServerTime);
	}
}
