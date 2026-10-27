#include "Server/Roster/StealthRosterCore.h"

// FRosterCore (GDD design/gdd/participant-state-roster.md R3-R12; ADR-0005 section 2 and the Technical Setup
// 2026-10-25b amendments). Stories psr-001..014. See StealthRosterCore.h for the contract of every call.

namespace RosterCoreLocal
{
	/** A record field list materialised once from the X-macro (no second, hand-kept list). */
	const FRosterFieldInfo RecordFields[] = { STEALTH_ROSTER_RECORD_FIELDS(STEALTH_ROSTER_FIELD_INFO) };
	const FRosterFieldInfo ConnectionFields[] = { STEALTH_ROSTER_CONNECTION_FIELDS(STEALTH_ROSTER_FIELD_INFO) };
}

TConstArrayView<FRosterFieldInfo> FRosterConnection::GetFieldInfo()
{
	return RosterCoreLocal::ConnectionFields;
}

TConstArrayView<FRosterFieldInfo> FParticipantRecord::GetFieldInfo()
{
	return RosterCoreLocal::RecordFields;
}

// ---------------------------------------------------------------------------------------------------------
// Construction, call scope, prechecks
// ---------------------------------------------------------------------------------------------------------

FRosterCore::FRosterCore(const FRosterCoreDeps& InDeps)
	: Deps(InDeps)
{
	check(Deps.Liveness && Deps.Authority && Deps.ThreadCheck && Deps.Clock && Deps.Random && Deps.FaultSink && Deps.RoundPhase);
}

FRosterCore::FCallScope::FCallScope(FRosterCore& InCore)
	: Core(InCore)
{
	++Core.CallDepth;
}

FRosterCore::FCallScope::~FCallScope()
{
	if (--Core.CallDepth == 0)
	{
		Core.FlushEvents();
	}
}

ERosterResult FRosterCore::CheckCallPreconditions(ERosterCall Call) const
{
	// R8 precedence steps 1 and 2 (and the same first two steps of every other mutator). Nothing below may
	// touch the store before this returns Applied: a worker thread must never read the record table.
	if (!Deps.Authority->HasAuthority())
	{
		Deps.FaultSink->Report(EStealthFault::RosterNotAuthority, RosterFaultSource(), static_cast<int64>(Call));
		return ERosterResult::Rejected_NotAuthority;
	}
	if (!Deps.ThreadCheck->IsGameThread())
	{
		Deps.FaultSink->Report(EStealthFault::RosterWrongThread, RosterFaultSource(), static_cast<int64>(Call));
		return ERosterResult::Rejected_WrongThread;
	}
	return ERosterResult::Applied;
}

bool FRosterCore::CheckQueryAuthority(ERosterCall Call) const
{
	if (!Deps.Authority->HasAuthority())
	{
		Deps.FaultSink->Report(EStealthFault::RosterNotAuthority, RosterFaultSource(), static_cast<int64>(Call));
		return false;
	}
	return true;
}

// ---------------------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------------------

ERosterTeam FRosterCore::TeamFromInt(int32 Team)
{
	return Team == 0 ? ERosterTeam::Team0 : (Team == 1 ? ERosterTeam::Team1 : ERosterTeam::None);
}

int32 FRosterCore::TeamIndex(ERosterTeam Team)
{
	return Team == ERosterTeam::Team0 ? 0 : (Team == ERosterTeam::Team1 ? 1 : INDEX_NONE);
}

const FParticipantRecord* FRosterCore::FindRecord(FParticipantId Id) const
{
	if (!Id.IsValid() || !Records.IsValidIndex(static_cast<int32>(Id.Value)))
	{
		return nullptr;
	}
	const TOptional<FParticipantRecord>& Slot = Records[static_cast<int32>(Id.Value)];
	return Slot.IsSet() ? &Slot.GetValue() : nullptr;
}

FParticipantRecord* FRosterCore::FindRecordMutable(FParticipantId Id)
{
	return const_cast<FParticipantRecord*>(FindRecord(Id));
}

const FRosterBindingEntry* FRosterCore::FindBinding(const FRosterPawnHandle& Pawn) const
{
	return Pawn.IsSet() ? PawnTable.Find(Pawn) : nullptr;
}

void FRosterCore::GetParticipantIds(TArray<FParticipantId>& OutIds) const
{
	OutIds.Reset();
	for (const TOptional<FParticipantRecord>& Slot : Records)
	{
		if (Slot.IsSet())
		{
			OutIds.Add(Slot->ParticipantId);
		}
	}
}

bool FRosterCore::FindLatestRetiredPawn(FParticipantId Id, FRosterPawnHandle& OutPawn, int64& OutRetireAtServerTimeMs) const
{
	bool bFound = false;
	for (const TPair<FRosterPawnHandle, FRosterBindingEntry>& Row : PawnTable)
	{
		if (Row.Value.Status == ERosterBindingStatus::Retired && Row.Value.Participant == Id
			&& (!bFound || Row.Value.RetireAtServerTimeMs > OutRetireAtServerTimeMs))
		{
			OutPawn = Row.Key;
			OutRetireAtServerTimeMs = Row.Value.RetireAtServerTimeMs;
			bFound = true;
		}
	}
	return bFound;
}

FParticipantId FRosterCore::FindIdByKey(const FIdentityKey& Key) const
{
	const FParticipantId* Found = IdByKey.Find(Key);
	return Found ? *Found : FParticipantId();
}

int32 FRosterCore::CountNonDormant() const
{
	int32 Count = 0;
	for (const TOptional<FParticipantRecord>& Slot : Records)
	{
		if (Slot.IsSet() && Slot->Connection.Projection != ERosterConnection::Dormant)
		{
			++Count;
		}
	}
	return Count;
}

int32 FRosterCore::CountNonDormantOnTeam(ERosterTeam Team, FParticipantId Skip) const
{
	int32 Count = 0;
	for (const TOptional<FParticipantRecord>& Slot : Records)
	{
		if (Slot.IsSet() && Slot->ParticipantId != Skip && Slot->Team == Team && Slot->Connection.Projection != ERosterConnection::Dormant)
		{
			++Count;
		}
	}
	return Count;
}

int32 FRosterCore::CountInState(int32 Team, EParticipantState InState) const
{
	const ERosterTeam TeamValue = TeamFromInt(Team);
	if (TeamValue == ERosterTeam::None)
	{
		return 0;
	}
	// The record table is bounded by 2 x TeamSize live records (at most 10); story psr-033 owns the
	// probe and allocation budget and may replace this scan with counters updated in each state change.
	int32 Count = 0;
	for (const TOptional<FParticipantRecord>& Slot : Records)
	{
		if (Slot.IsSet() && Slot->bInRound && Slot->Team == TeamValue && Slot->State == InState)
		{
			++Count;
		}
	}
	return Count;
}

int32 FRosterCore::CountRosterUnchecked(int32 Team) const
{
	const ERosterTeam TeamValue = TeamFromInt(Team);
	if (TeamValue == ERosterTeam::None)
	{
		return 0;
	}
	// F3: InRound records of the team; before the first ResetAll of the match (RoundEpoch still 0, InRound
	// false everywhere) the non-Dormant records of the team instead.
	if (RoundEpoch == 0)
	{
		return CountNonDormantOnTeam(TeamValue);
	}
	int32 Count = 0;
	for (const TOptional<FParticipantRecord>& Slot : Records)
	{
		if (Slot.IsSet() && Slot->bInRound && Slot->Team == TeamValue)
		{
			++Count;
		}
	}
	return Count;
}

uint32 FRosterCore::DrawPublicHandle()
{
	// ADR-0006 domain 15, counter-as-Attempt: a draw equal to 0 or to any handle already issued in the match
	// is discarded and the counter advances.
	for (;;)
	{
		const uint32 Candidate = static_cast<uint32>(Deps.Random->Draw(ERosterRandomDomain::PublicHandle, HandleDrawCounter++));
		if (Candidate != 0 && !IssuedHandles.Contains(Candidate))
		{
			IssuedHandles.Add(Candidate);
			return Candidate;
		}
	}
}

uint64 FRosterCore::DrawMatchId()
{
	// ADR-0006 domain 17 (decision D46), counter-as-Attempt per draw; 0 and a repeat of the previous match's
	// id are discarded, so a second BeginMatch always yields a different MatchId (AC-PSR-05).
	for (;;)
	{
		const uint64 Candidate = Deps.Random->Draw(ERosterRandomDomain::MatchId, MatchDrawCounter++);
		if (Candidate != 0 && Candidate != MatchId)
		{
			return Candidate;
		}
	}
}

int64 FRosterCore::NowMs() const
{
	return static_cast<int64>(FMath::FloorToDouble(Deps.Clock->GetServerTimeSeconds() * 1000.0));
}

void FRosterCore::ClearState()
{
	Records.Reset();
	IdByKey.Reset();
	IssuedHandles.Reset();
	PawnTable.Reset();
	RoundEpoch = 0;
	EventSeq = 0;
	bTeamsLocked = false;
	LastResetRoundIndex = 0;
	bRoundScopeReset = false;
	NextParticipantId = 0;
	// HandleDrawCounter is NOT reset: the session secret is opened once per store, so restarting the counter would
	// hand the same PublicHandles out in every match and let players be correlated across matches.
}

// ---------------------------------------------------------------------------------------------------------
// Events (R12, I6)
// ---------------------------------------------------------------------------------------------------------

uint64 FRosterCore::EmitEvent(FRosterEvent Event)
{
	// EventSeq is taken when the event is queued, in mutation order, so FIFO dispatch order equals EventSeq order.
	Event.EventSeq = ++EventSeq;
	Event.RoundEpoch = RoundEpoch;
	PendingEvents.Add(Event);
	return Event.EventSeq;
}

void FRosterCore::FlushEvents()
{
	if (bFlushing)
	{
		// A listener's re-entrant call returned: its events are already queued behind the current ones and
		// the outer drain loop dispatches them.
		return;
	}
	bFlushing = true;
	for (int32 Index = 0; Index < PendingEvents.Num(); ++Index)
	{
		const FRosterEvent Event = PendingEvents[Index];
		const TArray<IRosterEventListener*> ListenersNow = Listeners;
		for (IRosterEventListener* Listener : ListenersNow)
		{
			if (Listeners.Contains(Listener))
			{
				Listener->OnRosterEvent(Event);
			}
		}
	}
	PendingEvents.Reset();
	bFlushing = false;
}

void FRosterCore::AddListener(IRosterEventListener* Listener)
{
	if (Listener)
	{
		Listeners.AddUnique(Listener);
	}
}

void FRosterCore::RemoveListener(IRosterEventListener* Listener)
{
	Listeners.Remove(Listener);
}

// ---------------------------------------------------------------------------------------------------------
// Match lifetime
// ---------------------------------------------------------------------------------------------------------

ERosterResult FRosterCore::BeginMatch(int32 InTeamSize)
{
	const ERosterResult Pre = CheckCallPreconditions(ERosterCall::BeginMatch);
	if (Pre != ERosterResult::Applied)
	{
		return Pre;
	}
	FCallScope Scope(*this);
	if (InTeamSize < RosterMinTeamSize || InTeamSize > RosterMaxTeamSize)
	{
		return ERosterResult::Rejected_InvalidTeamSize;
	}
	ClearState();
	TeamSize = InTeamSize;
	MatchId = DrawMatchId();
	PawnTable.Reserve(Deps.PawnTableCapacity);
	Records.Reserve(2 * RosterMaxTeamSize);
	return ERosterResult::Applied;
}

ERosterResult FRosterCore::EndMatch()
{
	const ERosterResult Pre = CheckCallPreconditions(ERosterCall::EndMatch);
	if (Pre != ERosterResult::Applied)
	{
		return Pre;
	}
	FCallScope Scope(*this);
	ClearState();
	MatchId = 0;
	TeamSize = 0;
	return ERosterResult::Applied;
}

ERosterResult FRosterCore::SetTeamSize(int32 InTeamSize)
{
	const ERosterResult Pre = CheckCallPreconditions(ERosterCall::SetTeamSize);
	if (Pre != ERosterResult::Applied)
	{
		return Pre;
	}
	FCallScope Scope(*this);
	// AC-PSR-06 is authoritative for the locked case (R4 names no locked code: GDD gap recorded in story psr-005).
	if (bTeamsLocked)
	{
		return ERosterResult::Rejected_TeamsLocked;
	}
	if (InTeamSize < RosterMinTeamSize || InTeamSize > RosterMaxTeamSize)
	{
		return ERosterResult::Rejected_InvalidTeamSize;
	}
	const int32 Largest = FMath::Max(CountNonDormantOnTeam(ERosterTeam::Team0), CountNonDormantOnTeam(ERosterTeam::Team1));
	if (InTeamSize < Largest)
	{
		return ERosterResult::Rejected_TeamSizeBelowRoster;
	}
	TeamSize = InTeamSize;
	return ERosterResult::Applied;
}

// ---------------------------------------------------------------------------------------------------------
// Registration and teams
// ---------------------------------------------------------------------------------------------------------

bool FRosterCore::TryReclaim(const FIdentityKey& Key, FRosterRegisterResult& Out)
{
	FParticipantRecord* Existing = FindRecordMutable(FindIdByKey(Key));
	if (!Existing)
	{
		return false;
	}
	const bool bDormant = Existing->Connection.Projection == ERosterConnection::Dormant;
	if (bDormant && bTeamsLocked)
	{
		// A Dormant record is not part of the running round (Session rejects it as InProgress anyway).
		Out.Result = ERosterResult::Rejected_TeamsLocked;
		return true;
	}
	if (bDormant && Existing->Team != ERosterTeam::None && CountNonDormantOnTeam(Existing->Team, Existing->ParticipantId) >= TeamSize)
	{
		// R4: a Dormant record returns as itself; its Team is kept if that team has room under F5, else None.
		Existing->Team = ERosterTeam::None;
	}
	// R11 rejoin returns the same record. Connection is written only by Session's NotifyConnectionChanged (R3:
	// "the Roster never writes it itself"), which brings the projection back to Connected; the rest of the
	// reclaim (view rebind, C-TeamView re-push, pawn repossession) is story psr-029 and the facade's.
	FRosterEvent Event;
	Event.Type = ERosterEventType::ParticipantReclaimed;
	Event.Id = Existing->ParticipantId;
	EmitEvent(Event);
	Out.Result = ERosterResult::Reclaimed;
	Out.Id = Existing->ParticipantId;
	return true;
}

FRosterRegisterResult FRosterCore::RegisterParticipant(const FIdentityKey& Key, const FString& DisplayName)
{
	FRosterRegisterResult Out;
	Out.Result = CheckCallPreconditions(ERosterCall::RegisterParticipant);
	if (Out.Result != ERosterResult::Applied)
	{
		return Out;
	}
	FCallScope Scope(*this);

	// R4 check order: reclaim match, then TeamsLocked, then RosterFull.
	if (TryReclaim(Key, Out))
	{
		return Out;
	}
	if (bTeamsLocked)
	{
		Out.Result = ERosterResult::Rejected_TeamsLocked;
		return Out;
	}
	if (CountNonDormant() >= 2 * TeamSize)
	{
		Out.Result = ERosterResult::Rejected_RosterFull;
		return Out;
	}

	const FParticipantId Id(NextParticipantId++);
	FParticipantRecord Record;
	Record.ParticipantId = Id;
	Record.PublicHandle = DrawPublicHandle();
	Record.IdentityKey = Key;
	Record.DisplayName = DisplayName;
	// R4 defaults: State RoundEnded, Team None, Connection Connected (the member defaults).
	check(Records.Num() == static_cast<int32>(Id.Value));
	Records.Emplace(MoveTemp(Record));
	IdByKey.Add(Key, Id);

	FRosterEvent Event;
	Event.Type = ERosterEventType::ParticipantRegistered;
	Event.Id = Id;
	EmitEvent(Event);
	Out.Result = ERosterResult::Registered;
	Out.Id = Id;
	return Out;
}

ERosterResult FRosterCore::RemoveParticipant(FParticipantId Id)
{
	const ERosterResult Pre = CheckCallPreconditions(ERosterCall::RemoveParticipant);
	if (Pre != ERosterResult::Applied)
	{
		return Pre;
	}
	FCallScope Scope(*this);
	const FParticipantRecord* Record = FindRecord(Id);
	if (!Record)
	{
		return ERosterResult::Rejected_UnknownParticipant;
	}
	if (bTeamsLocked)
	{
		return ERosterResult::Rejected_TeamsLocked;
	}
	// Drop any rows of the pawn table that still name the participant, then retire the id for good.
	for (auto It = PawnTable.CreateIterator(); It; ++It)
	{
		if (It.Value().Participant == Id)
		{
			It.RemoveCurrent();
		}
	}
	IdByKey.Remove(Record->IdentityKey);
	Records[static_cast<int32>(Id.Value)].Reset();

	FRosterEvent Event;
	Event.Type = ERosterEventType::ParticipantRemoved;
	Event.Id = Id;
	EmitEvent(Event);
	return ERosterResult::Applied;
}

ERosterResult FRosterCore::SetTeamsLocked(bool bLocked)
{
	const ERosterResult Pre = CheckCallPreconditions(ERosterCall::SetTeamsLocked);
	if (Pre != ERosterResult::Applied)
	{
		return Pre;
	}
	FCallScope Scope(*this);
	if (bLocked == bTeamsLocked)
	{
		return ERosterResult::NoOp_Unchanged;
	}
	if (bLocked)
	{
		// I2: no non-Dormant record may be Team None while locked.
		for (const TOptional<FParticipantRecord>& Slot : Records)
		{
			if (Slot.IsSet() && Slot->Team == ERosterTeam::None && Slot->Connection.Projection != ERosterConnection::Dormant)
			{
				return ERosterResult::Rejected_UnassignedParticipants;
			}
		}
	}
	bTeamsLocked = bLocked;
	return ERosterResult::Applied;
}

ERosterResult FRosterCore::ValidateTeamBatch(TConstArrayView<FRosterTeamAssignment> Batch) const
{
	// R5 order: TeamsLocked, UnknownParticipant, InvalidBatch, TeamFull. Pure: reads only.
	if (bTeamsLocked)
	{
		return ERosterResult::Rejected_TeamsLocked;
	}
	for (const FRosterTeamAssignment& Entry : Batch)
	{
		if (!FindRecord(Entry.Id))
		{
			return ERosterResult::Rejected_UnknownParticipant;
		}
	}
	if (Batch.Num() == 0)
	{
		return ERosterResult::Rejected_InvalidBatch;
	}
	for (int32 Index = 0; Index < Batch.Num(); ++Index)
	{
		if (Batch[Index].Team != 0 && Batch[Index].Team != 1)
		{
			return ERosterResult::Rejected_InvalidBatch;
		}
		for (int32 Later = Index + 1; Later < Batch.Num(); ++Later)
		{
			if (Batch[Later].Id == Batch[Index].Id)
			{
				return ERosterResult::Rejected_InvalidBatch;
			}
		}
	}
	// F5 on the final state: overlay the batch on the current teams and count non-Dormant records per team.
	int32 FinalCount[2] = { 0, 0 };
	for (const TOptional<FParticipantRecord>& Slot : Records)
	{
		if (!Slot.IsSet() || Slot->Connection.Projection == ERosterConnection::Dormant)
		{
			continue;
		}
		ERosterTeam FinalTeam = Slot->Team;
		for (const FRosterTeamAssignment& Entry : Batch)
		{
			if (Entry.Id == Slot->ParticipantId)
			{
				FinalTeam = TeamFromInt(Entry.Team);
				break;
			}
		}
		const int32 TeamSlot = TeamIndex(FinalTeam);
		if (TeamSlot != INDEX_NONE)
		{
			++FinalCount[TeamSlot];
		}
	}
	if (FinalCount[0] > TeamSize || FinalCount[1] > TeamSize)
	{
		return ERosterResult::Rejected_TeamFull;
	}
	return ERosterResult::Applied;
}

ERosterResult FRosterCore::AssignTeams(TConstArrayView<FRosterTeamAssignment> Batch)
{
	const ERosterResult Pre = CheckCallPreconditions(ERosterCall::AssignTeams);
	if (Pre != ERosterResult::Applied)
	{
		return Pre;
	}
	FCallScope Scope(*this);
	// All-or-nothing by construction: every rejection returns before the first write.
	const ERosterResult Validation = ValidateTeamBatch(Batch);
	if (Validation != ERosterResult::Applied)
	{
		return Validation;
	}
	for (const FRosterTeamAssignment& Entry : Batch)
	{
		FindRecordMutable(Entry.Id)->Team = TeamFromInt(Entry.Team);
	}
	FRosterEvent Event;
	Event.Type = ERosterEventType::TeamsChanged;
	EmitEvent(Event);
	return ERosterResult::Applied;
}

// ---------------------------------------------------------------------------------------------------------
// Connection projection
// ---------------------------------------------------------------------------------------------------------

ERosterResult FRosterCore::NotifyConnectionChanged(FParticipantId Id, ERosterConnection Projection, int64 ServerTimeMs, bool bKicked)
{
	const ERosterResult Pre = CheckCallPreconditions(ERosterCall::NotifyConnectionChanged);
	if (Pre != ERosterResult::Applied)
	{
		return Pre;
	}
	FCallScope Scope(*this);
	FParticipantRecord* Record = FindRecordMutable(Id);
	if (!Record)
	{
		return ERosterResult::Rejected_UnknownParticipant;
	}
	// bKicked changes nothing in the core: it only rides on the event, where it selects the teammates'
	// bReconnecting clear cause in the C-TeamView push step (R6, ruling D23).
	FRosterConnection& Connection = Record->Connection;
	if (Connection.Projection == Projection)
	{
		// Disconnected -> Disconnected (Rejoining, Expired, a failed rejoin) changes neither the generation nor
		// DisconnectedAt (R11), so a client that keeps failing its rejoin cannot extend its grace. No event.
		return ERosterResult::NoOp_Unchanged;
	}
	if (Projection == ERosterConnection::Disconnected)
	{
		// Decision D45: the generation bumps only on entry to Disconnected (a supersede is such an entry, D24).
		++Connection.ConnectionGeneration;
		Connection.DisconnectedAtMs = ServerTimeMs;
	}
	Connection.Projection = Projection;

	FRosterEvent Event;
	Event.Type = ERosterEventType::ConnectionChanged;
	Event.Id = Id;
	Event.Connection = Projection;
	Event.bKicked = bKicked;
	EmitEvent(Event);
	return ERosterResult::Applied;
}

// ---------------------------------------------------------------------------------------------------------
// Pawn binding
// ---------------------------------------------------------------------------------------------------------

ERosterResult FRosterCore::BindPawn(FParticipantId Id, const FRosterPawnHandle& Pawn, const TOptional<FRosterEpochToken>& ExpectedEpoch)
{
	const ERosterResult Pre = CheckCallPreconditions(ERosterCall::BindPawn);
	if (Pre != ERosterResult::Applied)
	{
		return Pre;
	}
	FCallScope Scope(*this);
	FParticipantRecord* Record = FindRecordMutable(Id);
	if (!Record)
	{
		return ERosterResult::Rejected_UnknownParticipant;
	}
	if (ExpectedEpoch.IsSet() && ExpectedEpoch.GetValue() != GetEpochToken())
	{
		return ERosterResult::Rejected_StaleEpoch;
	}
	if (Record->State != EParticipantState::Active && Record->State != EParticipantState::Informant)
	{
		return ERosterResult::Rejected_WrongState;
	}
	if (Record->HasLivePawn())
	{
		return ERosterResult::Rejected_AlreadyBound;
	}
	if (!Pawn.IsSet() || Deps.Liveness->GetLiveness(Pawn) != ERosterPawnLiveness::Valid || PawnTable.Contains(Pawn))
	{
		return ERosterResult::Rejected_PawnInUse;
	}
	FRosterBindingEntry Entry;
	Entry.Status = ERosterBindingStatus::BoundLive;
	Entry.Participant = Id;
	PawnTable.Add(Pawn, Entry);
	Record->PawnBinding = Pawn;
	return ERosterResult::Applied;
}

void FRosterCore::RetireLivePawnInternal(FParticipantRecord& Record, int64 RetireAtServerTimeMs)
{
	if (!Record.HasLivePawn())
	{
		return;
	}
	if (FRosterBindingEntry* Entry = PawnTable.Find(Record.PawnBinding))
	{
		Entry->Status = ERosterBindingStatus::Retired;
		Entry->RetireAtServerTimeMs = RetireAtServerTimeMs;
	}
	Record.PawnBinding = FRosterPawnHandle();
}

void FRosterCore::ReleaseBindingInternal(FRosterPawnHandle Pawn)
{
	const FRosterBindingEntry* Entry = PawnTable.Find(Pawn);
	if (!Entry)
	{
		return;
	}
	if (FParticipantRecord* Owner = FindRecordMutable(Entry->Participant))
	{
		if (Owner->PawnBinding == Pawn)
		{
			Owner->PawnBinding = FRosterPawnHandle();
		}
	}
	PawnTable.Remove(Pawn);
}

ERosterResult FRosterCore::ReleaseToCrowd(const FRosterPawnHandle& Pawn)
{
	const ERosterResult Pre = CheckCallPreconditions(ERosterCall::ReleaseToCrowd);
	if (Pre != ERosterResult::Applied)
	{
		return Pre;
	}
	FCallScope Scope(*this);
	const FRosterBindingEntry* Entry = FindBinding(Pawn);
	if (!Entry)
	{
		// The GDD wrong-input table lists no code for an Unbound pawn; NoOp_NotBound (as ClearBinding) changes nothing.
		return ERosterResult::NoOp_NotBound;
	}
	if (const FParticipantRecord* Owner = FindRecord(Entry->Participant))
	{
		// R9: a connected player's pawn can never be handed to the crowd.
		if (Owner->Connection.Projection == ERosterConnection::Connected && Owner->State != EParticipantState::RoundEnded)
		{
			return ERosterResult::Rejected_NotDisconnected;
		}
	}
	ReleaseBindingInternal(Pawn);
	return ERosterResult::Applied;
}

ERosterResult FRosterCore::RetireBinding(const FRosterPawnHandle& Pawn, int64 RetireAtServerTimeMs, ERosterReason Reason)
{
	const ERosterResult Pre = CheckCallPreconditions(ERosterCall::RetireBinding);
	if (Pre != ERosterResult::Applied)
	{
		return Pre;
	}
	FCallScope Scope(*this);
	FRosterBindingEntry* Entry = PawnTable.Find(Pawn);
	if (!Entry)
	{
		return ERosterResult::NoOp_NotBound; // R9: an Unbound pawn (an NPC) changes nothing
	}
	if (Entry->Status == ERosterBindingStatus::Retired)
	{
		return ERosterResult::NoOp_AlreadyRetired; // R9: a second retire of the same body changes nothing
	}
	// R9 / D10: BoundLive -> Retired, the live pawn cleared. Not a transition: no state write, no event, no EventSeq.
	// Reason (InformantKilled) belongs to the ServerOnly log only; no log structure exists for it yet (story psr-036's
	// dev-only transition log is the nearest), so it is accepted and stored nowhere.
	(void)Reason;
	Entry->Status = ERosterBindingStatus::Retired;
	Entry->RetireAtServerTimeMs = RetireAtServerTimeMs;
	if (FParticipantRecord* Owner = FindRecordMutable(Entry->Participant))
	{
		if (Owner->PawnBinding == Pawn)
		{
			Owner->PawnBinding = FRosterPawnHandle();
		}
	}
	return ERosterResult::Applied;
}

ERosterResult FRosterCore::ClearBinding(const FRosterPawnHandle& Pawn, ERosterEndPlayReason Reason)
{
	const ERosterResult Pre = CheckCallPreconditions(ERosterCall::ClearBinding);
	if (Pre != ERosterResult::Applied)
	{
		return Pre;
	}
	FCallScope Scope(*this);
	const FRosterBindingEntry* Entry = FindBinding(Pawn);
	if (!Entry)
	{
		return ERosterResult::NoOp_NotBound;
	}
	const bool bWasLive = Entry->Status == ERosterBindingStatus::BoundLive;
	const FParticipantId Owner = Entry->Participant;
	// A Retired row is a tombstone that ends at EndPlay (R9); a live row clears the participant's live pawn.
	ReleaseBindingInternal(Pawn);
	if (bWasLive && Reason == ERosterEndPlayReason::Destroyed)
	{
		if (Deps.RoundPhase->IsRoundPhaseActive())
		{
			// The missing retire (R9): reported only for Destroyed during Round phase Active.
			Deps.FaultSink->Report(EStealthFault::RosterLiveBindingCleared, RosterFaultSource(), static_cast<int64>(Owner.Value));
		}
		const FParticipantRecord* Record = FindRecord(Owner);
		if (Record && Record->State == EParticipantState::Active)
		{
			// R9, R12: Round runs its own-card respawn on this event (Round R17.9). Teardown reasons never raise it.
			FRosterEvent Event;
			Event.Type = ERosterEventType::BindingLost;
			Event.Id = Owner;
			Event.Cause = ERosterBindingLostCause::PawnDestroyed;
			EmitEvent(Event);
		}
	}
	return ERosterResult::Applied;
}

ERosterResult FRosterCore::InvalidateAllBindings()
{
	const ERosterResult Pre = CheckCallPreconditions(ERosterCall::InvalidateAllBindings);
	if (Pre != ERosterResult::Applied)
	{
		return Pre;
	}
	FCallScope Scope(*this);
	PawnTable.Reset();
	for (TOptional<FParticipantRecord>& Slot : Records)
	{
		if (Slot.IsSet())
		{
			Slot->PawnBinding = FRosterPawnHandle();
		}
	}
	return ERosterResult::Applied;
}

// ---------------------------------------------------------------------------------------------------------
// Transitions
// ---------------------------------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------------------------------
// Round scope (R7)
// ---------------------------------------------------------------------------------------------------------

ERosterResult FRosterCore::ResetAll(uint32 RoundIndex)
{
	const ERosterResult Pre = CheckCallPreconditions(ERosterCall::ResetAll);
	if (Pre != ERosterResult::Applied)
	{
		return Pre;
	}
	FCallScope Scope(*this);
	if (!bTeamsLocked)
	{
		return ERosterResult::Rejected_TeamsUnlocked;
	}
	for (TOptional<FParticipantRecord>& Slot : Records)
	{
		if (!Slot.IsSet())
		{
			continue;
		}
		const bool bDormant = Slot->Connection.Projection == ERosterConnection::Dormant;
		Slot->State = bDormant ? EParticipantState::RoundEnded : EParticipantState::Active;
		Slot->bInRound = !bDormant;
		Slot->LastTransitionReason = ERosterReason::None;
		Slot->AssignedAppearanceId = INDEX_NONE;
		Slot->PawnBinding = FRosterPawnHandle();
	}
	PawnTable.Reset();
	++RoundEpoch;
	LastResetRoundIndex = RoundIndex;
	bRoundScopeReset = true;

	FRosterEvent Event;
	Event.Type = ERosterEventType::RosterReset;
	EmitEvent(Event);
	return ERosterResult::Applied;
}

bool FRosterCore::IsEnteringReason(EParticipantState NewState, ERosterReason Reason)
{
	switch (NewState)
	{
	case EParticipantState::Informant:
		return Reason == ERosterReason::Assassinated || Reason == ERosterReason::ReversedAgainst
			|| Reason == ERosterReason::StrikeLimit || Reason == ERosterReason::Disconnect;
	case EParticipantState::RoundEnded:
		return Reason == ERosterReason::RoundEnd;
	case EParticipantState::Active:
		return Reason == ERosterReason::Reset;
	}
	return false;
}

namespace RosterCoreLocal
{
	/**
	 * The legal single-participant transitions of the States and Transitions table (R8 step 8). Active ->
	 * RoundEnded and -> Active exist only as the batch calls EndRoundAll / ResetAll and never reach step 8
	 * (step 5 rejects their reasons). Story psr-027 checks this table against the CSV fixture.
	 */
	struct FLegalTransition
	{
		EParticipantState From;
		EParticipantState To;
	};
	constexpr FLegalTransition LegalSingleTransitions[] = {
		{ EParticipantState::Active, EParticipantState::Informant },
	};
}

bool FRosterCore::IsLegalSingleTransition(EParticipantState From, EParticipantState To)
{
	for (const RosterCoreLocal::FLegalTransition& Row : RosterCoreLocal::LegalSingleTransitions)
	{
		if (Row.From == From && Row.To == To)
		{
			return true;
		}
	}
	return false;
}

void FRosterCore::DisposeLivePawnOnElimination(FParticipantRecord& Record, ERosterReason Reason, const TOptional<int64>& RetireAtServerTimeMs)
{
	if (!Record.HasLivePawn())
	{
		return;
	}
	if (Reason == ERosterReason::StrikeLimit)
	{
		// ADR-0005 section 2 and decision D42: the exile releases, not retires; the pawn is Unbound (Npc) and
		// the Rejected_NotDisconnected rule of ReleaseToCrowd does not apply on this internal path.
		ReleaseBindingInternal(Record.PawnBinding);
		return;
	}
	RetireLivePawnInternal(Record, RetireAtServerTimeMs.IsSet() ? RetireAtServerTimeMs.GetValue() : NowMs());
}

uint64 FRosterCore::EmitStateChanged(const FParticipantRecord& Record, EParticipantState Old)
{
	FRosterEvent Event;
	Event.Type = ERosterEventType::ParticipantStateChanged;
	Event.Id = Record.ParticipantId;
	Event.Team = Record.Team;
	Event.Old = Old;
	Event.New = Record.State;
	Event.Reason = Record.LastTransitionReason;
	// F1 and F2 snapshots at emission (R12): payload counts are authoritative for ordering.
	for (int32 Team = 0; Team < 2; ++Team)
	{
		Event.ActiveCountAfter[Team] = CountInState(Team, EParticipantState::Active);
		Event.InformantCountAfter[Team] = CountInState(Team, EParticipantState::Informant);
	}
	// The returned EventSeq becomes the record's LastEventSeq inside the call, before dispatch.
	return EmitEvent(Event);
}

ERosterResult FRosterCore::RequestTransition(FParticipantId Id, EParticipantState NewState, ERosterReason Reason, const FRosterTransitionOptions& Options)
{
	// R8 result precedence: the first failing check decides.
	const ERosterResult Pre = CheckCallPreconditions(ERosterCall::RequestTransition);  // steps 1, 2
	if (Pre != ERosterResult::Applied)
	{
		return Pre;
	}
	FCallScope Scope(*this);
	FParticipantRecord* Record = FindRecordMutable(Id);
	if (!Record)
	{
		return ERosterResult::Rejected_UnknownParticipant;  // step 3
	}
	if (Options.ExpectedEpoch.IsSet() && Options.ExpectedEpoch.GetValue() != GetEpochToken())
	{
		return ERosterResult::Rejected_StaleEpoch;  // step 4
	}
	if (Reason == ERosterReason::RoundEnd || Reason == ERosterReason::Reset)
	{
		return ERosterResult::Rejected_BatchOnlyReason;  // step 5
	}
	if (!IsEnteringReason(NewState, Reason))
	{
		return ERosterResult::Rejected_IllegalReason;  // step 6 (InformantKilled always lands here, D10)
	}
	if (Record->State == NewState)
	{
		return ERosterResult::NoOp_AlreadyInState;  // step 7: first reason wins, no event
	}
	if (!IsLegalSingleTransition(Record->State, NewState))
	{
		return ERosterResult::Rejected_IllegalTransition;  // step 8
	}
	if (Reason == ERosterReason::Disconnect)
	{
		if (Record->Connection.Projection == ERosterConnection::Connected)
		{
			return ERosterResult::Rejected_NotDisconnected;  // step 9
		}
		if (!Options.ExpectedConnectionGeneration.IsSet()
			|| Options.ExpectedConnectionGeneration.GetValue() != Record->Connection.ConnectionGeneration)
		{
			return ERosterResult::Rejected_StaleConnection;  // step 10
		}
	}

	// Step 11, Applied: state, reason, pawn disposition and LastEventSeq are all written inside this call.
	const EParticipantState Old = Record->State;
	Record->State = NewState;
	Record->LastTransitionReason = Reason;
	if (Old == EParticipantState::Active && NewState == EParticipantState::Informant)
	{
		DisposeLivePawnOnElimination(*Record, Reason, Options.RetireAtServerTimeMs);
	}

	Record->LastEventSeq = EmitStateChanged(*Record, Old);
	return ERosterResult::Applied;
}

// ---------------------------------------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------------------------------------

FRosterVictimKind FRosterCore::ResolveVictimKind(const FRosterPawnHandle& Pawn) const
{
	if (!CheckQueryAuthority(ERosterCall::ResolveVictimKind))
	{
		return FRosterVictimKind::NotTargetable(ERosterNotTargetableReason::Stale);
	}
	// R10 evaluation order. (1) null, destroyed or pending kill: Stale.
	if (!Pawn.IsSet() || Deps.Liveness->GetLiveness(Pawn) != ERosterPawnLiveness::Valid)
	{
		return FRosterVictimKind::NotTargetable(ERosterNotTargetableReason::Stale);
	}
	const FRosterBindingEntry* Entry = PawnTable.Find(Pawn);
	if (!Entry)
	{
		// (5) a valid live Unbound pawn.
		return FRosterVictimKind::Make(ERosterVictimKind::Npc);
	}
	if (Entry->Status == ERosterBindingStatus::Retired)
	{
		// (2) Retired, including one whose participant is now RoundEnded.
		return FRosterVictimKind::NotTargetable(ERosterNotTargetableReason::AlreadyResolved);
	}
	const FParticipantRecord* Record = FindRecord(Entry->Participant);
	if (Record)
	{
		switch (Record->State)
		{
		case EParticipantState::RoundEnded:
			return FRosterVictimKind::NotTargetable(ERosterNotTargetableReason::RoundOver);  // (3)
		case EParticipantState::Active:
			return FRosterVictimKind::Make(ERosterVictimKind::Player);  // (4)
		case EParticipantState::Informant:
			return FRosterVictimKind::Make(ERosterVictimKind::Informant);  // (4)
		}
	}
	// Anything unrecognised is never the permissive Npc (R10): fault plus NotTargetable(Stale).
	Deps.FaultSink->Report(EStealthFault::RosterUnrecognisedState, RosterFaultSource(), static_cast<int64>(ERosterCall::ResolveVictimKind));
	return FRosterVictimKind::NotTargetable(ERosterNotTargetableReason::Stale);
}

bool FRosterCore::IsPawnBound(const FRosterPawnHandle& Pawn) const
{
	if (!CheckQueryAuthority(ERosterCall::IsPawnBound))
	{
		return false;
	}
	const FRosterBindingEntry* Entry = FindBinding(Pawn);
	return Entry && Entry->Status == ERosterBindingStatus::BoundLive;
}

EParticipantState FRosterCore::GetLegalityClass(const FRosterPawnHandle& Pawn) const
{
	if (!CheckQueryAuthority(ERosterCall::GetLegalityClass))
	{
		return EParticipantState::Active;
	}
	if (const FRosterBindingEntry* Entry = FindBinding(Pawn))
	{
		if (const FParticipantRecord* Record = FindRecord(Entry->Participant))
		{
			return Record->State;
		}
	}
	return EParticipantState::Active;
}

int32 FRosterCore::CountActive(int32 Team) const
{
	return CheckQueryAuthority(ERosterCall::CountActive) ? CountInState(Team, EParticipantState::Active) : 0;
}

int32 FRosterCore::CountInformant(int32 Team) const
{
	return CheckQueryAuthority(ERosterCall::CountInformant) ? CountInState(Team, EParticipantState::Informant) : 0;
}

int32 FRosterCore::CountRoster(int32 Team) const
{
	return CheckQueryAuthority(ERosterCall::CountRoster) ? CountRosterUnchecked(Team) : 0;
}

bool FRosterCore::IsTeamEliminated(int32 Team) const
{
	if (!CheckQueryAuthority(ERosterCall::IsTeamEliminated))
	{
		return false;
	}
	return Deps.RoundPhase->IsRoundPhaseActive() && CountRosterUnchecked(Team) >= 1 && CountInState(Team, EParticipantState::Active) == 0;
}

FRosterPawnHandle FRosterCore::GetPawn(FParticipantId Id) const
{
	if (!CheckQueryAuthority(ERosterCall::GetPawn))
	{
		return FRosterPawnHandle();
	}
	const FParticipantRecord* Record = FindRecord(Id);
	return Record ? Record->PawnBinding : FRosterPawnHandle();
}

FParticipantId FRosterCore::GetParticipant(const FRosterPawnHandle& Pawn) const
{
	if (!CheckQueryAuthority(ERosterCall::GetParticipant))
	{
		return FParticipantId();
	}
	const FRosterBindingEntry* Entry = FindBinding(Pawn);
	return Entry ? Entry->Participant : FParticipantId();
}

void FRosterCore::GetUnboundActiveParticipants(TArray<FParticipantId>& OutIds) const
{
	OutIds.Reset();
	if (!CheckQueryAuthority(ERosterCall::GetUnboundActiveParticipants))
	{
		return;
	}
	for (const TOptional<FParticipantRecord>& Slot : Records)
	{
		if (Slot.IsSet() && Slot->bInRound && Slot->State == EParticipantState::Active && !Slot->HasLivePawn())
		{
			OutIds.Add(Slot->ParticipantId);
		}
	}
}

FRosterSnapshot FRosterCore::Snapshot() const
{
	FRosterSnapshot Out;
	Out.Records = Records;
	for (const TPair<FRosterPawnHandle, FRosterBindingEntry>& Row : PawnTable)
	{
		Out.Bindings.Add(Row);
	}
	Out.Bindings.Sort([](const TPair<FRosterPawnHandle, FRosterBindingEntry>& A, const TPair<FRosterPawnHandle, FRosterBindingEntry>& B)
	{
		return A.Key.ObjectKey != B.Key.ObjectKey ? A.Key.ObjectKey < B.Key.ObjectKey : A.Key.Generation < B.Key.Generation;
	});
	Out.MatchId = MatchId;
	Out.RoundEpoch = RoundEpoch;
	Out.EventSeq = EventSeq;
	Out.TeamSize = TeamSize;
	Out.bTeamsLocked = bTeamsLocked;
	return Out;
}

#if WITH_STEALTH_TEST_SEAMS
void FRosterCore::SeamSetState(FParticipantId Id, EParticipantState NewState, bool bInRound)
{
	if (FParticipantRecord* Record = FindRecordMutable(Id))
	{
		Record->State = NewState;
		Record->bInRound = bInRound;
	}
}

void FRosterCore::SeamSetBinding(const FRosterPawnHandle& Pawn, FParticipantId Id, ERosterBindingStatus Status, int64 RetireAtServerTimeMs)
{
	FRosterBindingEntry Entry;
	Entry.Status = Status;
	Entry.Participant = Id;
	Entry.RetireAtServerTimeMs = RetireAtServerTimeMs;
	PawnTable.Add(Pawn, Entry);
	if (Status == ERosterBindingStatus::BoundLive)
	{
		if (FParticipantRecord* Record = FindRecordMutable(Id))
		{
			Record->PawnBinding = Pawn;
		}
	}
}
#endif
