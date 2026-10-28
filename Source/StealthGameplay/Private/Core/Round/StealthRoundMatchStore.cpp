#include "Server/Core/Round/StealthRoundMatchStore.h"

#include "Server/Session/StealthSessionService.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/OutputDevice.h"
#include "Server/Roster/StealthRosterStore.h"
#include "View/CommandReplyTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogStealthRound, Log, All);

// URoundMatchStore (stories rmf-018..029; ADR-0005 section 2 and section 8; GDD round-match-framework R22).

const TCHAR* const URoundMatchStore::CommandNames[] = {
	TEXT("stealth.round.start"),
	TEXT("stealth.round.reset"),
	TEXT("stealth.round.abort"),
	TEXT("stealth.round.swapteam"),
	TEXT("stealth.round.remove"),
	TEXT("stealth.round.preset"),
	TEXT("stealth.round.debugwin"),
	TEXT("stealth.round.debugeliminate"),
	TEXT("stealth.round.dumplog"),
};
static_assert(UE_ARRAY_COUNT(URoundMatchStore::CommandNames) == URoundMatchStore::CommandCount, "one name per ERoundHostCommand");

namespace RoundMatchStoreLocal
{
	/** Process-wide console registration shared by every game instance (PIE runs several in one process). */
	int32 RegistrationCount = 0;
	TArray<IConsoleObject*> Registered;

	/** Open FRoundRemoteExecScope count on the game thread. */
	int32 RemoteExecDepth = 0;

	const TCHAR* ResultName(ERoundCommandResult Result)
	{
		switch (Result)
		{
		case ERoundCommandResult::Executed: return TEXT("Executed");
		case ERoundCommandResult::NoOp: return TEXT("NoOp");
		case ERoundCommandResult::Held: return TEXT("Held");
		case ERoundCommandResult::NotHost: return TEXT("NotHost");
		case ERoundCommandResult::WrongPhase: return TEXT("WrongPhase");
		case ERoundCommandResult::Refused: return TEXT("Refused");
		case ERoundCommandResult::Unavailable: return TEXT("Unavailable");
		default: return TEXT("?");
		}
	}

	FParticipantId ParseParticipant(const TArray<FString>& Args, int32 Index)
	{
		return Args.IsValidIndex(Index) && Args[Index].IsNumeric() ? FParticipantId(static_cast<uint32>(FCString::Atoi(*Args[Index]))) : FParticipantId();
	}
}

FRoundRemoteExecScope::FRoundRemoteExecScope() { ++RoundMatchStoreLocal::RemoteExecDepth; }
FRoundRemoteExecScope::~FRoundRemoteExecScope() { --RoundMatchStoreLocal::RemoteExecDepth; }
bool FRoundRemoteExecScope::IsActive() { return RoundMatchStoreLocal::RemoteExecDepth > 0; }

double FRoundStoreWorldClock::GetServerTimeSeconds() const
{
	const UGameInstance* GameInstance = Store.GetGameInstance();
	const UWorld* World = GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
	return World != nullptr ? World->GetTimeSeconds() : FPlatformTime::Seconds();
}

URoundMatchStore::URoundMatchStore() = default;
URoundMatchStore::~URoundMatchStore() = default;

void URoundMatchStore::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	URosterStore* Roster = Collection.InitializeDependency<URosterStore>();
	RosterStore = Roster;
	if (Roster != nullptr)
	{
		RosterEventHandle = Roster->OnRosterEvent.AddUObject(this, &URoundMatchStore::HandleRosterEvent);
		Roster->RegisterRoundPhase(this, this);
	}
	RegisterConsoleCommands();
	// Session's EndMatch fan-out (ADR-0005 section 2): every session exit path clears this store.
	if (USessionService* Session = Collection.InitializeDependency<USessionService>())
	{
		Session->RegisterMatchStore(this, this);
	}
}

void URoundMatchStore::EndMatch(ESessionEndReason Reason)
{
	if (Core && Reason == ESessionEndReason::SessionEnding)
	{
		// R8: OnSessionEnding is a terminal event: SessionEnded logged, processing stops, nothing released, no phase change.
		Core->OnSessionEnding();
	}
	if (Queue)
	{
		OnRoundQueueReleasing.Broadcast();
	}
	Queue.Reset();
	DebugResolver.Reset();
	Core.Reset();
	EventLog.Reset();
	Mirror = FRoundPublicStateMirror();
	GatewayOwner.Reset();
	HostParticipant = FParticipantId();
}

bool URoundMatchStore::IsMatchStateEmpty() const
{
	return !Core.IsValid() || Core->GetPhase() == ERoundPhase::Unknown;
}

void URoundMatchStore::Deinitialize()
{
	if (USessionService* Session = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<USessionService>() : nullptr)
	{
		Session->UnregisterMatchStore(this);
	}
	if (URosterStore* Roster = RosterStore.Get())
	{
		Roster->OnRosterEvent.Remove(RosterEventHandle);
		Roster->UnregisterRoundPhase(this);
	}
	if (Queue)
	{
		OnRoundQueueReleasing.Broadcast();
	}
	Queue.Reset();
	DebugResolver.Reset();
	Core.Reset();
	EventLog.Reset();
	UnregisterConsoleCommands();
	Super::Deinitialize();
}

URoundMatchStore* URoundMatchStore::Get(const UWorld* World)
{
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	return GameInstance != nullptr ? GameInstance->GetSubsystem<URoundMatchStore>() : nullptr;
}

// --- Wiring ------------------------------------------------------------------------------------------------------

void URoundMatchStore::RegisterGateways(const FRoundGateways& Gateways, UObject* Owner, const IServerSubSeedSource* SubSeeds)
{
	if (Gateways.Session == nullptr || Gateways.Roster == nullptr)
	{
		return;
	}
	GatewayOwner = Owner;
	if (Queue)
	{
		OnRoundQueueReleasing.Broadcast();
	}
	Queue.Reset();
	DebugResolver.Reset();
	Core.Reset();
	EventLog = MakeUnique<FRoundEventLog>(*this, Clock, QueueFaults);
	Core = MakeUnique<FRoundMatchCore>(Gateways, this, EventLog.Get(), &Clock);
	Core->SetParams(PendingParams);
	Core->GetPhaseCore().SetOnPhaseChanged([this](ERoundPhase Old, ERoundPhase New) { HandlePhaseChanged(Old, New); });
	Core->OnRoundActive = [this](uint32 Epoch, double ActivePhaseStart) { OnRoundActive.Broadcast(Epoch, ActivePhaseStart); };
	Core->OnRoundEnded = [this](const FRoundResult& Result)
	{
		UE_LOGF(LogStealthRound, Log, "Round %u round-end step: team %d wins (public reason %d); summary T0 {%d, %d, %d} T1 {%d, %d, %d}.",
			Result.RoundIndex, static_cast<int32>(Result.WinningTeam), static_cast<int32>(Result.PublicReason),
			static_cast<int32>(Result.TeamSummary[0].StepsCompleted), static_cast<int32>(Result.TeamSummary[0].EliminatedCount), static_cast<int32>(Result.TeamSummary[0].WrongTargetCount),
			static_cast<int32>(Result.TeamSummary[1].StepsCompleted), static_cast<int32>(Result.TeamSummary[1].EliminatedCount), static_cast<int32>(Result.TeamSummary[1].WrongTargetCount));
		OnRoundEnded.Broadcast(Result);
	};
	Core->OnWatchdogWarning = [](const StealthRoundFormulas::FWatchdogBound& Bound)
	{
		UE_LOGF(LogStealthRound, Warning, "F6: T_needed %.2f s exceeds PreparingWatchdogSec; the effective Preparing watchdog is %.2f s.", Bound.NeededSec, Bound.EffectiveSec);
	};
	Core->OnWriteLogHeader = [this](uint64 TuningHash) { WriteLogHeader(TuningHash); };
	Core->SetDumpLogHandler([this]()
	{
		TArray<FRoundLogEntry> Entries;
		const int32 Count = EventLog.IsValid() && EventLog->ReadEntries(Entries) == ERoundLogReadResult::Ok ? Entries.Num() : 0;
		const int32 Archived = Core.IsValid() ? Core->GetLogArchive().GetArchive().Num() : 0;
		UE_LOGF(LogStealthRound, Log, "Round log: %d entries this round, %d archived round slices.", Count, Archived);
	});

	// The round's resolution queue (ADR-0002): opened at Active entry, drained once per server frame while Active.
	if (SubSeeds != nullptr)
	{
		EpochSource.Roster = Gateways.Roster;
		GenerationSource.Session = Gateways.Session;
		FResolutionQueueDeps Deps;
		Deps.Clock = &Clock;
		Deps.SubSeeds = SubSeeds;
		Deps.Epoch = &EpochSource;
		Deps.Generations = &GenerationSource;
		Deps.Log = Core->GetRoundLogWriter();
		Deps.FaultSink = &QueueFaults;
		Queue = MakeUnique<FResolutionQueueCore>(Deps);
		DebugResolver = MakeUnique<FRoundDebugResolver>(*Core->GetRoundLogWriter(), Clock);
		DebugResolver->OnDebug = [this](const FDebugPayload& Payload, FQueueKey Key)
		{
			if (Core)
			{
				Core->ApplyDebugItem(Payload, Key);
			}
		};
		Queue->RegisterResolver(EQueueItemType::DebugItem, DebugResolver.Get());
		Queue->RegisterTimerResolver(ETimerKind::GraceExpiry, Core->GetGraceResolver());
		Queue->RegisterResolver(EQueueItemType::ConnectionTransition, Core->GetConnectionResolver());
		Core->SetRoundQueue(Queue.Get());
		Core->SetDebugQueue(Queue.Get());
	}
}

void URoundMatchStore::SetStartSpawner(IRoundStartSpawner* Spawner)
{
	if (Core)
	{
		Core->SetStartSpawner(Spawner);
	}
}

void URoundMatchStore::WriteLogHeader(uint64 TuningHash)
{
	if (!EventLog || !Core)
	{
		return;
	}
	// R19 header at P3: the public Seed and hashes only (never a sub-seed, the secret or entropy).
	FRoundLogHeader Header;
	Header.RoundIndex = Core->GetPhaseCore().GetCurrentRoundIndex();
	Header.Seed = Core->GetPreparing().GetSeed();
	Header.PresetId = Core->GetParams().PresetId;
	Header.TuningHash = TuningHash;
	Header.HostParticipantId = HostParticipant.Value;
	EventLog->WriteHeader(Header);
}

bool URoundMatchStore::IsLogReleasable() const
{
	const ERoundPhase Phase = GetPhase();
	return Phase == ERoundPhase::Ended || Phase == ERoundPhase::Reset;
}

void URoundMatchStore::SetRuleParams(const FRoundRuleParams& Params)
{
	PendingParams = Params;
	if (Core)
	{
		Core->SetParams(Params);
	}
}

void URoundMatchStore::RegisterPublicStateSink(IRoundPublicStateSink* Sink, UObject* Owner)
{
	if (Core)
	{
		Mirror.RegisterSink(Sink, Owner, Core->GetPhaseCore());
	}
}

void URoundMatchStore::PublishPublicState()
{
	if (Core)
	{
		Mirror.Publish(Core->GetPhaseCore());
	}
}

// --- Match flow ----------------------------------------------------------------------------------------------------

ERosterResult URoundMatchStore::OpenMatch()
{
	if (!Core)
	{
		return ERosterResult::Rejected_NotAuthority;
	}
	// Participant ids restart with every match: the host is recorded again by the first local registration.
	HostParticipant = FParticipantId();
	const ERosterResult Result = Core->OpenMatch();
	PublishPublicState();
	return Result;
}

bool URoundMatchStore::NotifyRoundTravelAccepted(int32 Seed) { return Core && Core->NotifyRoundTravelAccepted(Seed); }
void URoundMatchStore::NotifyTuningReady(uint64 TuningHash) { if (Core) { Core->NotifyTuningReady(TuningHash); } }
void URoundMatchStore::NotifyTuningFailed() { if (Core) { Core->NotifyTuningFailed(); } }
void URoundMatchStore::NotifyLayoutReady(uint32 LayoutId, int32 Seed) { if (Core) { Core->NotifyLayoutReady(LayoutId, Seed); } }
void URoundMatchStore::NotifyLayoutAborted() { if (Core) { Core->NotifyLayoutAborted(); } }
void URoundMatchStore::NotifyObjectiveChainInvalid() { if (Core) { Core->NotifyObjectiveChainInvalid(); } }
bool URoundMatchStore::NotifyParticipantTravelComplete(const FRoundTravelAck& Ack) { return Core && Core->NotifyParticipantTravelComplete(Ack); }
bool URoundMatchStore::NotifyParticipantWorldReady(const FRoundTravelAck& Ack) { return Core && Core->NotifyParticipantWorldReady(Ack); }
FRoundAckEpoch URoundMatchStore::GetPreparingAckEpoch() const { return Core ? Core->GetPreparing().GetAckEpoch() : FRoundAckEpoch(); }

bool URoundMatchStore::SubmitObjectiveCompletion(const FRoundObjectiveCompletion& Completion)
{
	return Core && Core->SubmitObjectiveCompletion(Completion);
}

TOptional<ECommandRejectCode> URoundMatchStore::AdmitCommand() const
{
	return Core ? Core->AdmitCommand() : TOptional<ECommandRejectCode>(ECommandRejectCode::NotNow);
}

bool URoundMatchStore::IsRoundLive() const
{
	return Core && Core->GetPhase() == ERoundPhase::Active && !Core->IsTerminated();
}
bool URoundMatchStore::EnterActive() { return Core && Core->EnterActive(); }

bool URoundMatchStore::EndRound(uint8 WinningTeam, uint8 PublicReason)
{
	return Core && Core->EndRound(WinningTeam, PublicReason);
}

ERoundPhase URoundMatchStore::GetPhase() const
{
	return Core ? Core->GetPhase() : ERoundPhase::Unknown;
}

void URoundMatchStore::HandlePhaseChanged(ERoundPhase Old, ERoundPhase New)
{
	UE_LOGF(LogStealthRound, Log, "Round phase %ls -> %ls (change %u, round %u)", LexRoundPhase(Old), LexRoundPhase(New),
		Core ? Core->GetPhaseCore().GetPhaseChangeCount() : 0u, Core ? Core->GetPhaseCore().GetCurrentRoundIndex() : 0u);
	if (New == ERoundPhase::Preparing && EventLog)
	{
		// The round's log starts with Preparing (the header follows at P3).
		EventLog->BeginRound(FRoundEventLog::MaxEntriesDefault);
	}
	if (New == ERoundPhase::Ended && Core)
	{
		const FRoundTerminalOutcome& Outcome = Core->GetLatch().GetOutcome();
		// The two-step end (R9): the latch step's WinLatched time against this round-end step's time.
		double LatchedAt = Clock.GetServerTimeSeconds();
		for (const FRoundLogEntry& Entry : Core->GetLogArchive().GetCurrentEntries())
		{
			if (Entry.Type == ERoundLogType::WinLatched && Entry.Key == Core->GetLatch().GetTerminalKey())
			{
				LatchedAt = Entry.ServerTime;
			}
		}
		UE_LOGF(LogStealthRound, Log, "Round %u decided by the production rules: team %d wins (public reason %d, end reason %d); latched at %.3f s, round-end step at %.3f s (+%.3f s).",
			Core->GetPhaseCore().GetCurrentRoundIndex(), static_cast<int32>(Outcome.WinningTeam), static_cast<int32>(Outcome.GetPublicReason()),
			static_cast<int32>(Outcome.EndReason), LatchedAt, Clock.GetServerTimeSeconds(), Clock.GetServerTimeSeconds() - LatchedAt);
	}
	PublishPublicState();
	OnPhaseChanged.Broadcast(Old, New);
}

void URoundMatchStore::HandleRosterEvent(const FRosterEvent& Event)
{
	if (!Core)
	{
		return;
	}
	switch (Event.Type)
	{
	case ERosterEventType::ParticipantRegistered:
		Core->OnParticipantRegistered(Event.Id);
		break;
	case ERosterEventType::ParticipantReclaimed:
		Core->OnParticipantReclaimed(Event.Id);
		break;
	case ERosterEventType::ParticipantStateChanged:
	{
		// F2: every applied single-participant transition, in EventSeq order, inside the causing slot's Roster flush.
		FRoundTransitionEvent Transition;
		Transition.Participant = Event.Id;
		Transition.Team = Event.Team;
		Transition.Old = Event.Old;
		Transition.New = Event.New;
		Transition.Reason = Event.Reason;
		Transition.ActiveCountAfter[0] = Event.ActiveCountAfter[0];
		Transition.ActiveCountAfter[1] = Event.ActiveCountAfter[1];
		UE_LOGF(LogStealthRound, Log, "Round: participant %u state %d -> %d (reason %d), Active after T0 %d T1 %d.", Event.Id.Value,
			static_cast<int32>(Event.Old), static_cast<int32>(Event.New), static_cast<int32>(Event.Reason), Event.ActiveCountAfter[0], Event.ActiveCountAfter[1]);
		Core->OnRosterTransition(Transition);
		break;
	}
	case ERosterEventType::ConnectionChanged:
	{
		UE_LOGF(LogStealthRound, Log, "Round: participant %u connection %ls%ls in phase %ls.", Event.Id.Value,
			Event.Connection == ERosterConnection::Connected ? TEXT("Connected") : TEXT("Disconnected"), Event.bKicked ? TEXT(" (kicked)") : TEXT(""),
			LexRoundPhase(Core->GetPhase()));
		const ERoundConnectionState State = Event.Connection == ERosterConnection::Connected ? ERoundConnectionState::Connected
			: Event.Connection == ERosterConnection::Dormant ? ERoundConnectionState::Dormant
			: ERoundConnectionState::Disconnected;
		Core->OnParticipantConnectionChanged(Event.Id, State, Event.bKicked, Clock.GetServerTimeSeconds());
		const FRoundGraceRecord* Grace = Core->GetGraceTable().Find(Event.Id);
		UE_LOGF(LogStealthRound, Log, "Round: grace records open %d; participant %u %ls.", Core->GetGraceTable().Num(), Event.Id.Value,
			Grace != nullptr ? *FString::Printf(TEXT("converts at %.3f s unless it rejoins"), Grace->GraceExpiry) : TEXT("has no grace record"));
		break;
	}
	default:
		// The batch events (RosterRoundEnded, RosterReset) are never evaluated as eliminations (F2 proof item 5).
		break;
	}
}

bool URoundMatchStore::IsRoundPhaseActive() const
{
	return GetPhase() == ERoundPhase::Active;
}

// --- Host commands --------------------------------------------------------------------------------------------------

FRoundCommandCaller URoundMatchStore::MakeConsoleCaller(const UWorld* World) const
{
	FRoundCommandCaller Caller;
	Caller.HostId = HostParticipant;
	if (World == nullptr)
	{
		return Caller;
	}
	const ENetMode NetMode = World->GetNetMode();
	Caller.bServerNetMode = NetMode == NM_ListenServer || NetMode == NM_Standalone;
	const APlayerController* Local = World->GetFirstPlayerController();
	Caller.bLocalController = !FRoundRemoteExecScope::IsActive() && Local != nullptr && Local->IsLocalController();
	Caller.Caller = LocalParticipantResolver ? LocalParticipantResolver(World) : FParticipantId();
	return Caller;
}

ERoundCommandResult URoundMatchStore::ExecuteCommand(const FRoundCommandCaller& Caller, ERoundHostCommand Command, const FRoundCommandArgs& Args)
{
	if (!Core)
	{
		// A client game instance holds no Round core: every command is NotHost there.
		return ERoundCommandResult::NotHost;
	}
	const ERoundCommandResult Result = Core->ExecuteHostCommand(Caller, Command, Args);
	PublishPublicState();
	if (Caller.IsHost())
	{
		OnHostMessage.Broadcast(FString::Printf(TEXT("%s: %s"), CommandNames[static_cast<int32>(Command)], RoundMatchStoreLocal::ResultName(Result)));
	}
	return Result;
}

ERoundCommandResult URoundMatchStore::ExecuteConsoleCommand(const UWorld* World, ERoundHostCommand Command, const FRoundCommandArgs& Args)
{
	return ExecuteCommand(MakeConsoleCaller(World), Command, Args);
}

void URoundMatchStore::OnStartRefused(const FStartRefusal& Refusal)
{
	FString Ids;
	for (const FParticipantId& Id : Refusal.ParticipantIds)
	{
		Ids += FString::Printf(TEXT(" %u"), Id.Value);
	}
	const FString Message = FString::Printf(TEXT("Start refused: code %d sub %d%s"), static_cast<int32>(Refusal.Code), static_cast<int32>(Refusal.TeamsReason), *Ids);
	UE_LOGF(LogStealthRound, Log, "%ls", *Message);
	OnHostMessage.Broadcast(Message);
}

void URoundMatchStore::OnCommandRefused(ERoundHostCommand Command, ERoundCommandResult Result, uint8 Detail)
{
	const FString Message = FString::Printf(TEXT("%s refused: %s (%d)"), CommandNames[static_cast<int32>(Command)], RoundMatchStoreLocal::ResultName(Result), static_cast<int32>(Detail));
	UE_LOGF(LogStealthRound, Log, "%ls", *Message);
	OnHostMessage.Broadcast(Message);
}

void URoundMatchStore::HandleConsole(ERoundHostCommand Command, const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	URoundMatchStore* Store = Get(World);
	if (Store == nullptr)
	{
		Ar.Logf(TEXT("%s: NotHost"), CommandNames[static_cast<int32>(Command)]);
		return;
	}
	FRoundCommandArgs CommandArgs;
	switch (Command)
	{
	case ERoundHostCommand::SwapTeam:
		CommandArgs.Participant = RoundMatchStoreLocal::ParseParticipant(Args, 0);
		CommandArgs.Other = RoundMatchStoreLocal::ParseParticipant(Args, 1);
		break;
	case ERoundHostCommand::Remove:
	case ERoundHostCommand::DebugEliminate:
		CommandArgs.Participant = RoundMatchStoreLocal::ParseParticipant(Args, 0);
		break;
	case ERoundHostCommand::Preset:
		CommandArgs.PresetId = Args.IsValidIndex(0) ? FName(*Args[0]) : NAME_None;
		CommandArgs.PresetTeamSize = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 0;
		break;
	case ERoundHostCommand::DebugWin:
		CommandArgs.Team = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 0;
		break;
	default:
		break;
	}
	const ERoundCommandResult Result = Store->ExecuteConsoleCommand(World, Command, CommandArgs);
	Ar.Logf(TEXT("%s: %s"), CommandNames[static_cast<int32>(Command)], RoundMatchStoreLocal::ResultName(Result));
}

void URoundMatchStore::RegisterConsoleCommands()
{
	using namespace RoundMatchStoreLocal;
	if (RegistrationCount++ > 0)
	{
		return;
	}
	for (int32 Index = 0; Index < CommandCount; ++Index)
	{
		const ERoundHostCommand Command = static_cast<ERoundHostCommand>(Index);
#if UE_BUILD_SHIPPING
		if (Command == ERoundHostCommand::DebugWin || Command == ERoundHostCommand::DebugEliminate || Command == ERoundHostCommand::DumpLog)
		{
			continue;
		}
#endif
		Registered.Add(IConsoleManager::Get().RegisterConsoleCommand(CommandNames[Index],
			TEXT("Round host command (R22): host only; NotHost for anyone else."),
			FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([Command](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				HandleConsole(Command, Args, World, Ar);
			}),
			ECVF_Default));
	}
}

void URoundMatchStore::UnregisterConsoleCommands()
{
	using namespace RoundMatchStoreLocal;
	if (RegistrationCount <= 0 || --RegistrationCount > 0)
	{
		return;
	}
	for (IConsoleObject* Object : Registered)
	{
		if (Object != nullptr)
		{
			IConsoleManager::Get().UnregisterConsoleObject(Object);
		}
	}
	Registered.Reset();
}

// --- Tick ------------------------------------------------------------------------------------------------------------

void URoundMatchStore::Tick(float DeltaTime)
{
	if (Core)
	{
		Core->Tick();
		// One drain per server frame while Active, and in Ended so items that arrived around an out-of-slot latch still
		// resolve Rejected_Latched (logged) and an earlier strike's death write still plays; the queue is reset at Reset
		// and is closed in the other phases. Nothing is processed after OnSessionEnding (R8).
		if (Queue && !Core->HasSessionEnded() && (Core->GetPhase() == ERoundPhase::Active || Core->GetPhase() == ERoundPhase::Ended))
		{
			Queue->Drain();
			OnRoundQueueDrained.Broadcast(); // the reply step's flush (story asn-015)
		}
		PublishPublicState();
	}
}

bool URoundMatchStore::IsTickable() const
{
	return !IsTemplate() && Core.IsValid();
}

TStatId URoundMatchStore::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URoundMatchStore, STATGROUP_Tickables);
}
