#include "Server/Core/Round/StealthRoundPreparing.h"

#include "Server/Queue/StealthResolutionQueueCore.h"
#include "Server/StealthClock.h"

FRoundPreparingSequencer::FRoundPreparingSequencer(const FRoundGateways& InGateways, const IStealthClock* InClock)
	: Gateways(InGateways)
	, Clock(InClock)
{
}

double FRoundPreparingSequencer::Now() const
{
	return Clock != nullptr ? Clock->GetServerTimeSeconds() : 0.0;
}

void FRoundPreparingSequencer::Begin(const FRoundRuleParams& InParams, int32 InSeed, FRoundAckEpoch InEpoch)
{
	Cancel();
	Params = InParams;
	Seed = InSeed;
	Epoch = InEpoch;
	EnteredAt = Now();
	// Until tuning freezes F6 cannot be evaluated: the configured watchdog bounds the wait for tuning.
	WatchdogDeadline = EnteredAt + Params.Preparing.PreparingWatchdogSec;
	Step = ERoundPreparingStep::AwaitTuning;
	// P2: "Preparing entered"; TDC freezes and raises OnTuningReady (or Failed).
	if (Gateways.Tuning != nullptr)
	{
		Gateways.Tuning->SignalPreparingEntered();
	}
}

void FRoundPreparingSequencer::Cancel()
{
	Step = ERoundPreparingStep::Idle;
	bTuningReady = false;
	TuningHash = 0;
	AllocateCalls = 0;
	bLayoutReady = false;
	LayoutId = 0;
	Acked.Reset();
	WorldReady.Reset();
	ExpectedPawnCount = 0;
	WorldReadyDeadline = 0.0;
	ActiveAtServerTime = 0.0;
	SpawnRecords.Reset();
	DisconnectedAtSpawn.Reset();
	Gated.Reset();
	SpawnEpoch = FEpochToken();
	WatchdogBound = StealthRoundFormulas::FWatchdogBound();
}

void FRoundPreparingSequencer::Abort(ERoundPreparingAbortCode Code)
{
	// Stop first: the owner's abort path runs Reset, which must find the sequence already idle.
	Step = ERoundPreparingStep::Idle;
	if (Callbacks.OnAbort)
	{
		Callbacks.OnAbort(Code);
	}
}

// --- Owner events --------------------------------------------------------------------------------------------------

void FRoundPreparingSequencer::OnTravelFailed()
{
	if (IsRunning())
	{
		Abort(ERoundPreparingAbortCode::BadParams);
	}
}

void FRoundPreparingSequencer::OnTuningReady(uint64 InTuningHash)
{
	// Exactly once (story rmf-031): a duplicate OnTuningReady, or one after the sequence moved on, is ignored.
	if (Step != ERoundPreparingStep::AwaitTuning || bTuningReady)
	{
		return;
	}
	bTuningReady = true;
	TuningHash = InTuningHash;

	// F6 at freeze (story rmf-035): the effective watchdog counts from Preparing entry.
	WatchdogBound = StealthRoundFormulas::ComputePreparingWatchdog(Params.Preparing);
	WatchdogDeadline = EnteredAt + WatchdogBound.EffectiveSec;
	if (WatchdogBound.bNeededWins && Callbacks.OnWatchdogWarning)
	{
		Callbacks.OnWatchdogWarning(WatchdogBound);
	}

	// P3: the header once, then the layout request and the deal (Players = every rostered id, connected or not).
	if (Callbacks.OnWriteHeader)
	{
		Callbacks.OnWriteHeader(TuningHash);
	}
	Step = ERoundPreparingStep::AwaitLayoutAndBarrier;
	if (Gateways.LevelSocket != nullptr)
	{
		Gateways.LevelSocket->RequestLayout(Seed, TuningHash);
		if (!IsRunning())
		{
			return; // a synchronous OnLayoutAborted
		}
	}
	if (Gateways.CharacterPool != nullptr && Gateways.Roster != nullptr)
	{
		TArray<FRoundRosterMember> Members;
		Gateways.Roster->GetRosteredParticipants(Members);
		TArray<FParticipantId> Players;
		for (const FRoundRosterMember& Member : Members)
		{
			Players.Add(Member.Participant);
		}
		++AllocateCalls;
		switch (Gateways.CharacterPool->AllocateAppearances(Players, Params.NpcCount))
		{
		case ERoundAllocateResult::Ok:
			break;
		case ERoundAllocateResult::DeckInvalid:
		case ERoundAllocateResult::Error_DeckTooSmall:
			Abort(ERoundPreparingAbortCode::DeckInvalid);
			return;
		case ERoundAllocateResult::Error_AlreadyDealt:
		default:
			// A second deal is a bug (R6 P3).
			Abort(ERoundPreparingAbortCode::AllocatorMisuse);
			return;
		}
	}
}

void FRoundPreparingSequencer::OnTuningFailed()
{
	if (IsRunning())
	{
		Abort(ERoundPreparingAbortCode::TuningLoadFailed);
	}
}

void FRoundPreparingSequencer::OnTuningSnapshotFault()
{
	if (IsRunning())
	{
		Abort(ERoundPreparingAbortCode::TuningSnapshotFault);
	}
}

void FRoundPreparingSequencer::OnLayoutReady(uint32 InLayoutId, int32 LayoutSeed)
{
	if (Step != ERoundPreparingStep::AwaitLayoutAndBarrier || bLayoutReady)
	{
		return;
	}
	if (LayoutSeed != Seed)
	{
		Abort(ERoundPreparingAbortCode::LayoutSeedMismatch);
		return;
	}
	bLayoutReady = true;
	LayoutId = InLayoutId;
}

void FRoundPreparingSequencer::OnLayoutAborted()
{
	if (Step == ERoundPreparingStep::AwaitTuning || Step == ERoundPreparingStep::AwaitLayoutAndBarrier)
	{
		Abort(ERoundPreparingAbortCode::LayoutAborted);
	}
}

bool FRoundPreparingSequencer::IsAckCurrent(const FRoundTravelAck& Ack) const
{
	// Keyed by ParticipantId and this Preparing's nonce; a replaced controller delivers under an older generation.
	return Ack.Participant.IsValid() && Ack.Epoch == Epoch && Gateways.Session != nullptr
		&& Ack.ConnectionGeneration == Gateways.Session->GetConnectionGeneration(Ack.Participant);
}

bool FRoundPreparingSequencer::OnParticipantTravelComplete(const FRoundTravelAck& Ack)
{
	// The barrier closes at P6; an acknowledgement after it is the rejoin path (R17.3), not this one.
	if (Step != ERoundPreparingStep::AwaitTuning && Step != ERoundPreparingStep::AwaitLayoutAndBarrier)
	{
		return false;
	}
	if (!IsAckCurrent(Ack))
	{
		// Dropped (story rmf-033): no wait-set change, no deadline started, extended or restarted, nothing logged.
		return false;
	}
	const uint32* Existing = Acked.Find(Ack.Participant.Value);
	if (Existing != nullptr && *Existing == Ack.ConnectionGeneration)
	{
		return false; // one acknowledgement per connection
	}
	Acked.Add(Ack.Participant.Value, Ack.ConnectionGeneration);
	return true;
}

bool FRoundPreparingSequencer::OnParticipantWorldReady(const FRoundTravelAck& Ack)
{
	if (Step != ERoundPreparingStep::AwaitWorldReady || !IsAckCurrent(Ack))
	{
		return false;
	}
	WorldReady.Add(Ack.Participant.Value, Ack.ConnectionGeneration);
	return true;
}

void FRoundPreparingSequencer::OnParticipantConnected(FParticipantId Participant)
{
	if (Gated.Contains(Participant.Value) && Gateways.Spawner != nullptr && Gateways.Spawner->OpenSpawnGate(Participant))
	{
		Gated.Remove(Participant.Value);
	}
}

bool FRoundPreparingSequencer::IsBarrierSatisfied() const
{
	if (Gateways.Roster == nullptr || Gateways.Session == nullptr)
	{
		return false;
	}
	TArray<FRoundRosterMember> Members;
	Gateways.Roster->GetRosteredParticipants(Members);
	for (const FRoundRosterMember& Member : Members)
	{
		if (Gateways.Session->GetConnectionState(Member.Participant) != ERoundConnectionState::Connected)
		{
			continue; // only Connected participants are in the wait set
		}
		const uint32* Generation = Acked.Find(Member.Participant.Value);
		if (Generation == nullptr || *Generation != Gateways.Session->GetConnectionGeneration(Member.Participant))
		{
			return false; // a rejoiner (new generation) must acknowledge again
		}
	}
	return true;
}

bool FRoundPreparingSequencer::AreAllConnectedWorldReady() const
{
	TArray<FRoundRosterMember> Members;
	Gateways.Roster->GetRosteredParticipants(Members);
	for (const FRoundRosterMember& Member : Members)
	{
		if (Gateways.Session->GetConnectionState(Member.Participant) != ERoundConnectionState::Connected)
		{
			continue;
		}
		const uint32* Generation = WorldReady.Find(Member.Participant.Value);
		if (Generation == nullptr || *Generation != Gateways.Session->GetConnectionGeneration(Member.Participant))
		{
			return false;
		}
	}
	return true;
}

bool FRoundPreparingSequencer::RunStartSpawn()
{
	TArray<FRoundRosterMember> Members;
	Gateways.Roster->GetRosteredParticipants(Members);
	Members.Sort([](const FRoundRosterMember& A, const FRoundRosterMember& B) { return A.Participant.Value < B.Participant.Value; });

	// P5b: >= MinPlayersPerTeamCount Connected per team after the barrier (0 for an empty team in a dev 1v0 round).
	int32 RosterCount[2] = { 0, 0 };
	int32 ConnectedCount[2] = { 0, 0 };
	TArray<ERoundConnectionState> States;
	for (const FRoundRosterMember& Member : Members)
	{
		const ERoundConnectionState State = Gateways.Session->GetConnectionState(Member.Participant);
		States.Add(State);
		const int32 Team = Member.Team == ERosterTeam::Team0 ? 0 : (Member.Team == ERosterTeam::Team1 ? 1 : INDEX_NONE);
		if (Team != INDEX_NONE)
		{
			++RosterCount[Team];
			ConnectedCount[Team] += State == ERoundConnectionState::Connected ? 1 : 0;
		}
	}
	for (int32 Team = 0; Team < 2; ++Team)
	{
		const int32 Min = (Params.bAllowEmptyTeamDev && RosterCount[Team] == 0) ? 0 : Params.MinPlayersPerTeamCount;
		if (ConnectedCount[Team] < Min)
		{
			Abort(ERoundPreparingAbortCode::TeamAbsent);
			return false;
		}
	}

	// P6: the placement, once.
	TArray<FParticipantId> Team0;
	TArray<FParticipantId> Team1;
	for (const FRoundRosterMember& Member : Members)
	{
		(Member.Team == ERosterTeam::Team1 ? Team1 : Team0).Add(Member.Participant);
	}
	if (Gateways.LevelSocket != nullptr)
	{
		const ERoundPlacementResult Placement = Gateways.LevelSocket->RequestStartPlacement(Team0, Team1, Params.NpcCount);
		if (Placement == ERoundPlacementResult::PlacementInfeasible)
		{
			Abort(ERoundPreparingAbortCode::PlacementInfeasible);
			return false;
		}
		if (Placement == ERoundPlacementResult::NpcCountTooLow)
		{
			Abort(ERoundPreparingAbortCode::NpcCountTooLow);
			return false;
		}
		// F5 zone share, asserted before any pawn exists (story rmf-038).
		TArray<FRoundPlacementZone> Zones;
		Gateways.LevelSocket->GetPlacementZones(Zones);
		if (!StealthRoundFormulas::PlacementSharesHold(Zones))
		{
			Abort(ERoundPreparingAbortCode::PlacementInvariant);
			return false;
		}
	}

	// The canonical list: players in ParticipantId order, then NpcCount NPC slots. Player-ness is payload only.
	TArray<FRoundSpawnEntry> Entries;
	for (int32 Index = 0; Index < Members.Num(); ++Index)
	{
		FRoundSpawnEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.Slot = Entries.Num() - 1;
		Entry.Participant = Members[Index].Participant;
		Entry.Team = Members[Index].Team;
		const bool bConnected = States[Index] == ERoundConnectionState::Connected;
		// A participant not Connected at P6 is spawned Ai-driven and unpossessed behind a closed gate (never Player).
		Entry.Control = bConnected ? ERoundSpawnControl::Player : ERoundSpawnControl::Ai;
		Entry.bGateClosed = !bConnected;
		if (Gateways.CharacterPool != nullptr)
		{
			Entry.AppearanceId = Gateways.CharacterPool->GetAssignedAppearance(Entry.Participant);
			Gateways.Roster->RecordAppearance(Entry.Participant, Entry.AppearanceId);
		}
		if (States[Index] == ERoundConnectionState::Disconnected || States[Index] == ERoundConnectionState::Dormant)
		{
			DisconnectedAtSpawn.Add(Entry.Participant);
		}
	}
	for (int32 Npc = 0; Npc < FMath::Max(Params.NpcCount, 0); ++Npc)
	{
		FRoundSpawnEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.Slot = Entries.Num() - 1;
	}

	const int32 D = Entries.Num();
	TArray<int32> Sigma;
	const uint64 SubSeed = Gateways.Session->GetServerSubSeed(ESeedDomain::SpawnOrder, /*LayoutAttempt*/ 0);
	StealthRoundFormulas::ComputeSpawnOrder(D, SubSeed, Sigma);

	// All D pawns in this one call: one server frame, in sigma order.
	const FEpochToken Expected = Gateways.Roster->GetEpochToken();
	SpawnEpoch = Expected;
	const uint64 Tick = Clock != nullptr ? Clock->GetServerTickIndex() : 0;
	for (const int32 Slot : Sigma)
	{
		const FRoundSpawnEntry& Entry = Entries[Slot];
		FRoundSpawnRecord& Record = SpawnRecords.AddDefaulted_GetRef();
		Record.Entry = Entry;
		Record.ServerTickIndex = Tick;
		Record.Result = Gateways.Spawner != nullptr ? Gateways.Spawner->SpawnStartPawn(Entry, Expected) : ERoundSpawnResult::Spawned;
		if (Entry.IsPlayer() && Record.Result != ERoundSpawnResult::Spawned)
		{
			// Roster I4 would break: the factory destroyed the pawn before FinishSpawning; the round aborts.
			Abort(ERoundPreparingAbortCode::BindFailed);
			return false;
		}
		if (Entry.bGateClosed)
		{
			Gated.Add(Entry.Participant.Value);
		}
	}

	ExpectedPawnCount = D;
	if (Callbacks.OnPublicFields)
	{
		Callbacks.OnPublicFields(ExpectedPawnCount, 0.0);
	}
	if (Gateways.Spawner != nullptr)
	{
		Gateways.Spawner->OnStartSpawnComplete(ExpectedPawnCount);
	}
	WorldReadyDeadline = Now() + Params.Preparing.WorldReadyTimeoutSec;
	Step = ERoundPreparingStep::AwaitWorldReady;
	return true;
}

void FRoundPreparingSequencer::Tick()
{
	if (!IsRunning())
	{
		return;
	}
	const double Time = Now();
	if (Time >= WatchdogDeadline)
	{
		Abort(ERoundPreparingAbortCode::PreparingWatchdog);
		return;
	}
	switch (Step)
	{
	case ERoundPreparingStep::AwaitLayoutAndBarrier:
		if (bLayoutReady && IsBarrierSatisfied())
		{
			RunStartSpawn();
		}
		return;

	case ERoundPreparingStep::AwaitWorldReady:
		// A client not world-ready at the timeout is not disconnected: the start proceeds.
		if (AreAllConnectedWorldReady() || Time >= WorldReadyDeadline)
		{
			ActiveAtServerTime = Time + Params.Preparing.StartLeadSec;
			Step = ERoundPreparingStep::AwaitActiveAt;
			if (Callbacks.OnPublicFields)
			{
				Callbacks.OnPublicFields(ExpectedPawnCount, ActiveAtServerTime);
			}
		}
		return;

	case ERoundPreparingStep::AwaitActiveAt:
		// The first drain at or after ActiveAtServerTime, with the one due tolerance (D22: DueEpsilonMs 1 ms).
		if (Time + FResolutionQueueCore::DueEpsilonSec >= ActiveAtServerTime)
		{
			Step = ERoundPreparingStep::Done;
			if (Callbacks.OnReadyForActive)
			{
				Callbacks.OnReadyForActive();
			}
		}
		return;

	default:
		return;
	}
}
