// Objectives Chain stories oc-005..oc-009 and oc-011 (production/epics/objectives-chain/): the composition root's Objectives adapters.
// See StealthObjectivesWiring.h.

#include "StealthObjectivesWiring.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Server/Core/Round/StealthRoundMatchCore.h"
#include "Server/Core/Round/StealthRoundMatchStore.h"
#include "Server/Core/StealthPawnFactory.h"
#include "Server/Queue/StealthResolutionQueueCore.h"
#include "Server/Random/StealthServerSeeds.h"
#include "Server/Roster/StealthRosterStore.h"
#include "Server/Session/StealthSessionService.h"
#include "Server/StealthClock.h"
#include "Server/StealthCompositionRoot.h"
#include "StealthGameMode.h"
#include "View/StealthCourtSocket.h"
#include "StealthSettings.h"
#include "StealthTypes.h"
#include "View/ObjectiveTeamViewComponent.h"

FStealthObjectivesAdapters::FStealthObjectivesAdapters(UWorld* InWorld, UStealthCompositionRoot* InRoot, const IStealthClock* InClock)
	: World(InWorld)
	, Root(InRoot)
	, Clock(InClock)
{
}

URosterStore* FStealthObjectivesAdapters::GetRoster() const
{
	const UStealthCompositionRoot* Composition = Root.Get();
	URosterStore* Store = Composition != nullptr ? Composition->GetRosterStore() : nullptr;
	return Store != nullptr && Store->IsStoreInitialized() ? Store : nullptr;
}

URoundMatchStore* FStealthObjectivesAdapters::GetRound() const
{
	const UStealthCompositionRoot* Composition = Root.Get();
	return Composition != nullptr ? Composition->GetRoundStore() : nullptr;
}

// --- IObjectivesRosterView --------------------------------------------------------------------------------------------

EObjectivesLegalityClass FStealthObjectivesAdapters::GetLegalityClass(const APawn& Pawn) const
{
	URosterStore* Roster = GetRoster();
	if (Roster == nullptr)
	{
		return EObjectivesLegalityClass::RoundEnded;
	}
	switch (Roster->GetLegalityClass(const_cast<APawn*>(&Pawn)))
	{
	case EParticipantState::Active: return EObjectivesLegalityClass::Active;
	case EParticipantState::Informant: return EObjectivesLegalityClass::Informant;
	default: return EObjectivesLegalityClass::RoundEnded;
	}
}

FParticipantId FStealthObjectivesAdapters::GetParticipant(const APawn& Pawn) const
{
	URosterStore* Roster = GetRoster();
	return Roster != nullptr ? Roster->GetParticipant(const_cast<APawn*>(&Pawn)) : FParticipantId();
}

EParticipantState FStealthObjectivesAdapters::GetState(FParticipantId Participant) const
{
	const URosterStore* Roster = GetRoster();
	const FParticipantRecord* Record = Roster != nullptr ? Roster->FindRecord(Participant) : nullptr;
	return Record != nullptr ? Record->State : EParticipantState::RoundEnded;
}

int32 FStealthObjectivesAdapters::GetTeam(FParticipantId Participant) const
{
	const URosterStore* Roster = GetRoster();
	const FParticipantRecord* Record = Roster != nullptr ? Roster->FindRecord(Participant) : nullptr;
	if (Record == nullptr || Record->Team == ERosterTeam::None)
	{
		return INDEX_NONE;
	}
	return Record->Team == ERosterTeam::Team0 ? 0 : 1;
}

// --- IObjectivesSessionView / IObjectivesPawnControlView ------------------------------------------------------------

EObjectivesConnectionState FStealthObjectivesAdapters::GetConnectionState(FParticipantId Participant) const
{
	const UStealthCompositionRoot* Composition = Root.Get();
	const UStealthMatchAdapters* Adapters = Composition != nullptr ? Composition->GetAdapters() : nullptr;
	if (Adapters == nullptr)
	{
		return EObjectivesConnectionState::Disconnected;
	}
	switch (Adapters->GetConnectionState(Participant))
	{
	case ERoundConnectionState::Connecting: return EObjectivesConnectionState::Connecting;
	case ERoundConnectionState::Connected: return EObjectivesConnectionState::Connected;
	case ERoundConnectionState::Unresponsive: return EObjectivesConnectionState::Unresponsive;
	case ERoundConnectionState::Dormant: return EObjectivesConnectionState::Dormant;
	default: return EObjectivesConnectionState::Disconnected;
	}
}

uint32 FStealthObjectivesAdapters::GetControlEpoch(const APawn& Pawn) const
{
	const UStealthPawnFactory* Factory = UStealthPawnFactory::Get(World.Get());
	return Factory != nullptr ? Factory->GetControlEpoch(&Pawn) : 0u;
}

bool FStealthObjectivesAdapters::IsHandOverPending(const APawn& Pawn) const
{
	const UStealthPawnFactory* Factory = UStealthPawnFactory::Get(World.Get());
	return Factory != nullptr && Factory->IsHandOverPending(&Pawn);
}

// --- IRoundObjectiveHooks -------------------------------------------------------------------------------------------

bool FStealthObjectivesAdapters::IsActive() const
{
	const URoundMatchStore* Round = GetRound();
	return Round != nullptr && Round->GetPhase() == ERoundPhase::Active;
}

bool FStealthObjectivesAdapters::IsLatched() const
{
	const URoundMatchStore* Round = GetRound();
	const FResolutionQueueCore* Queue = Round != nullptr ? Round->GetRoundQueue() : nullptr;
	return Queue != nullptr && Queue->IsLatched();
}

bool FStealthObjectivesAdapters::DeclareObjectiveWin(uint8 Team, FQueueKey Key)
{
	URoundMatchStore* Round = GetRound();
	FRoundMatchCore* Core = Round != nullptr ? Round->GetMatchCore() : nullptr;
	return Core != nullptr && Core->DeclareObjectiveWin(Team, Key);
}

void FStealthObjectivesAdapters::AppendEvent(const FRoundLogEntry& Entry)
{
	URoundMatchStore* Round = GetRound();
	FRoundMatchCore* Core = Round != nullptr ? Round->GetMatchCore() : nullptr;
	if (IRoundLogWriter* Log = Core != nullptr ? Core->GetRoundLogWriter() : nullptr)
	{
		Log->Append(Entry);
	}
}

FQueueKey FStealthObjectivesAdapters::CurrentKey() const
{
	const URoundMatchStore* Round = GetRound();
	const FResolutionQueueCore* Queue = Round != nullptr ? Round->GetRoundQueue() : nullptr;
	return Queue != nullptr ? Queue->CurrentKey() : FQueueKey();
}

double FStealthObjectivesAdapters::ServerNow() const
{
	return Clock != nullptr ? Clock->GetServerTimeSeconds() : 0.0;
}

// --- The L_Prototype court stand-in --------------------------------------------------------------------------------

void FStealthObjectivesAdapters::GetCourts(TArray<const AStealthCourtSocket*>& Out) const
{
	AStealthCourtSocket::GetCourts(World.Get(), Out);
}

const AStealthCourtSocket* FStealthObjectivesAdapters::FindCourtActor(FCourtFixtureId Court) const
{
	return AStealthCourtSocket::FindCourt(World.Get(), Court);
}

bool FStealthObjectivesAdapters::GetSiteAssignment_Server(FSiteAssignment& OutAssignment) const
{
	TArray<const AStealthCourtSocket*> Courts;
	GetCourts(Courts);
	OutAssignment = FSiteAssignment();
	for (int32 Index = 0; Index < Courts.Num(); ++Index)
	{
		const AStealthCourtSocket* Court = Courts[Index];
		const FCourtFixtureId Id(static_cast<uint16>(Index + 1));
		if (Court->Stage >= STEALTH_FINAL_STAGE)
		{
			OutAssignment.Final = Id;
		}
		else if (Court->OwningTeam < 2)
		{
			FCourtFixtureId* Slot = Court->Stage == 1 ? &OutAssignment.P1[Court->OwningTeam] : &OutAssignment.P2[Court->OwningTeam];
			*Slot = Id;
		}
	}
	return Courts.Num() > 0;
}

bool FStealthObjectivesAdapters::GetFixtureFaces(FCourtFixtureId Court, TArray<FVector, TInlineAllocator<4>>& OutFaces) const
{
	OutFaces.Reset();
	const AStealthCourtSocket* Actor = FindCourtActor(Court);
	if (Actor == nullptr)
	{
		return false;
	}
	const FVector Centre = Actor->GetActorLocation();
	OutFaces.Add(Centre + FVector(FaceDistanceCm, 0.0, 0.0));
	OutFaces.Add(Centre + FVector(0.0, FaceDistanceCm, 0.0));
	OutFaces.Add(Centre + FVector(-FaceDistanceCm, 0.0, 0.0));
	OutFaces.Add(Centre + FVector(0.0, -FaceDistanceCm, 0.0));
	return true;
}

FCourtFixtureId FStealthObjectivesAdapters::FindCourtInReach(const APawn& Pawn) const
{
	// The same public-geometry rule a client uses for its own hold's ring (AStealthCourtSocket::FindCourtInReach).
	return AStealthCourtSocket::FindCourtInReach(World.Get(), Pawn.GetActorLocation(), UStealthSettings::Get()->ObjectiveInteractRange);
}

bool FStealthObjectivesAdapters::IsLegalityActive(const APawn& Pawn) const
{
	return GetLegalityClass(Pawn) == EObjectivesLegalityClass::Active;
}

// --- IObjectivesViewTargets (story oc-011) -------------------------------------------------------------------------

void FStealthObjectivesAdapters::GetTeamMembers(uint8 Team, TArray<FParticipantId>& OutMembers) const
{
	OutMembers.Reset();
	const URosterStore* Roster = GetRoster();
	if (Roster == nullptr)
	{
		return;
	}
	TArray<FParticipantId> Ids;
	Roster->GetParticipantIds(Ids);
	const ERosterTeam Wanted = Team == 0 ? ERosterTeam::Team0 : ERosterTeam::Team1;
	for (const FParticipantId Id : Ids)
	{
		const FParticipantRecord* Record = Roster->FindRecord(Id);
		if (Record != nullptr && Record->Team == Wanted)
		{
			OutMembers.Add(Id);
		}
	}
}

UObjectiveTeamViewComponent* FStealthObjectivesAdapters::FindTeamView(FParticipantId Participant) const
{
	const UStealthCompositionRoot* Composition = Root.Get();
	const APlayerController* Controller = Composition != nullptr ? Composition->GetController(Participant) : nullptr;
	return Controller != nullptr ? Controller->FindComponentByClass<UObjectiveTeamViewComponent>() : nullptr;
}

// --- IObjectivesSecretDraw (story oc-014) ---------------------------------------------------------------------------

uint64 FStealthObjectivesAdapters::GetStageSignalSubSeed(uint32 Ordinal) const
{
	const USessionService* Session = USessionService::Get(World.Get());
	return Session != nullptr ? Session->GetSubSeedSource().GetServerSubSeed(ESeedDomain::StageSignal, Ordinal) : 0;
}

// --- IObjectivesCourtClues (story oc-013; the L_Prototype stand-in) -------------------------------------------------

void FStealthObjectivesAdapters::GetCourtClueRecords(TArray<FCourtClueRecord>& OutRecords) const
{
	OutRecords.Reset();
	TArray<const AStealthCourtSocket*> Courts;
	GetCourts(Courts);
	if (Courts.Num() == 0)
	{
		return;
	}
	// Public geometry only: the courts' positions (never the assignment or an actor's stage or team).
	FVector Centroid = FVector::ZeroVector;
	for (const AStealthCourtSocket* Court : Courts)
	{
		Centroid += Court->GetActorLocation();
	}
	Centroid /= static_cast<double>(Courts.Num());
	TArray<double> Distances;
	for (const AStealthCourtSocket* Court : Courts)
	{
		Distances.Add(FVector::Dist2D(Court->GetActorLocation(), Centroid));
	}
	TArray<double> Sorted = Distances;
	Sorted.Sort();
	const double Median = Sorted[Sorted.Num() / 2];
	for (int32 Index = 0; Index < Courts.Num(); ++Index)
	{
		const FVector Offset = Courts[Index]->GetActorLocation() - Centroid;
		EClueAreaType Area = EClueAreaType::Causeway;
		if (Offset.X >= 0.0 && Offset.Y >= 0.0)
		{
			Area = EClueAreaType::Market;
		}
		else if (Offset.X < 0.0 && Offset.Y >= 0.0)
		{
			Area = EClueAreaType::Temple;
		}
		else if (Offset.X < 0.0)
		{
			Area = EClueAreaType::Residential;
		}
		FCourtClueRecord& Record = OutRecords.AddDefaulted_GetRef();
		Record.Court = FCourtFixtureId(static_cast<uint16>(Index + 1));
		Record.AreaTypeId = static_cast<uint8>(Area);
		Record.NearestFeatureKind = static_cast<uint8>(Distances[Index] > Median ? EClueFeatureKind::WaterSteps : EClueFeatureKind::Shrine);
	}
}
