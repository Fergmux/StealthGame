#pragma once

// Shared rig of the headless StealthGame.Objectives.* tests (stories oc-003..oc-009): recording stubs of every seam of
// Server/ObjectivesSeams.h, the Cacao chain built in code from the OC-16 fixture (a transient asset, never saved), a fixture
// FSiteAssignment and a ready-to-drive FObjectivesCore. Pawns come from a bare game world (StealthAssassinationTest::
// FScopedWorld); the core only uses them as identities and positions. Test-only.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "../Assassination/AssassinationTestWorld.h"
#include "ObjectivesCore.h"
#include "Server/ObjectivesSeams.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "View/ObjectiveChainData.h"
#include "View/ObjectiveTeamViewComponent.h"
#include "View/ObjectivesChainRegistry.h"
#include "View/Testing/ObjectiveCacaoChainFixture.h"

namespace StealthObjectivesTest
{
	// --- Fixture data -------------------------------------------------------------------------------------------------

	/** The fixture assignment's courts (SocketIds), and an ordinary court that holds no role. */
	inline constexpr uint16 CourtP1Team0 = 11;
	inline constexpr uint16 CourtP1Team1 = 12;
	inline constexpr uint16 CourtP2Team0 = 21;
	inline constexpr uint16 CourtP2Team1 = 22;
	inline constexpr uint16 CourtFinal = 30;
	inline constexpr uint16 CourtOrdinary = 40;

	/** The Cacao rows' indices (OC-16 order). */
	inline constexpr int32 DefP1Team0 = 0;
	inline constexpr int32 DefP2Team0 = 1;
	inline constexpr int32 DefP1Team1 = 2;
	inline constexpr int32 DefP2Team1 = 3;
	inline constexpr int32 DefFinal = 4;

	/** A fixed round epoch and t0. */
	inline constexpr uint32 RoundEpoch = 7;
	inline constexpr double ActivePhaseStart = 100.0;

	inline FSiteAssignment MakeAssignment()
	{
		FSiteAssignment A;
		A.P1[0] = FCourtFixtureId(CourtP1Team0);
		A.P1[1] = FCourtFixtureId(CourtP1Team1);
		A.P2[0] = FCourtFixtureId(CourtP2Team0);
		A.P2[1] = FCourtFixtureId(CourtP2Team1);
		A.Final = FCourtFixtureId(CourtFinal);
		return A;
	}

	/** A transient chain with the five OC-16 rows (Mutate edits it before returning). */
	inline UObjectiveChainData* MakeCacaoChain(TFunctionRef<void(UObjectiveChainData&)> Mutate = [](UObjectiveChainData&) {})
	{
		UObjectiveChainData* Chain = NewObject<UObjectiveChainData>(GetTransientPackage(), NAME_None, RF_Transient);
		Chain->ChainSerial = StealthObjectivesCacaoFixture::ChainSerial;
		for (const StealthObjectivesCacaoFixture::FRow& Row : StealthObjectivesCacaoFixture::Rows)
		{
			Chain->Definitions.Add(StealthObjectivesCacaoFixture::MakeDef(Row));
		}
		Mutate(*Chain);
		return Chain;
	}

	// --- Stubs ---------------------------------------------------------------------------------------------------------

	/** The assignment source; counts reads (the "stub LSC read count"). */
	class FStubAssignment : public IObjectivesAssignmentSource
	{
	public:
		FSiteAssignment Assignment = MakeAssignment();
		bool bReady = true;
		mutable int32 Reads = 0;

		virtual bool GetSiteAssignment_Server(FSiteAssignment& Out) const override
		{
			++Reads;
			Out = Assignment;
			return bReady;
		}
	};

	/** The Roster: pawn bindings, participant states and teams, and the legality class (an unbound pawn is Active). */
	class FStubRoster : public IObjectivesRosterView
	{
	public:
		TMap<const APawn*, FParticipantId> Bindings;
		TMap<uint32, EParticipantState> States;
		TMap<uint32, int32> Teams;
		mutable int32 ParticipantQueries = 0;

		void Bind(const APawn* Pawn, uint32 Participant, int32 Team, EParticipantState State = EParticipantState::Active)
		{
			Bindings.Add(Pawn, FParticipantId(Participant));
			States.Add(Participant, State);
			Teams.Add(Participant, Team);
		}

		virtual EObjectivesLegalityClass GetLegalityClass(const APawn& Pawn) const override
		{
			const FParticipantId* Id = Bindings.Find(&Pawn);
			if (Id == nullptr)
			{
				return EObjectivesLegalityClass::Active; // unbound maps to Active (ADR-0001)
			}
			const EParticipantState State = GetState(*Id);
			return State == EParticipantState::Active ? EObjectivesLegalityClass::Active
				: State == EParticipantState::Informant ? EObjectivesLegalityClass::Informant : EObjectivesLegalityClass::RoundEnded;
		}
		virtual FParticipantId GetParticipant(const APawn& Pawn) const override
		{
			++ParticipantQueries;
			const FParticipantId* Id = Bindings.Find(&Pawn);
			return Id != nullptr ? *Id : FParticipantId();
		}
		virtual EParticipantState GetState(FParticipantId Participant) const override
		{
			const EParticipantState* State = States.Find(Participant.Value);
			return State != nullptr ? *State : EParticipantState::RoundEnded;
		}
		virtual int32 GetTeam(FParticipantId Participant) const override
		{
			const int32* Team = Teams.Find(Participant.Value);
			return Team != nullptr ? *Team : INDEX_NONE;
		}
	};

	class FStubSession : public IObjectivesSessionView
	{
	public:
		TMap<uint32, EObjectivesConnectionState> States;

		virtual EObjectivesConnectionState GetConnectionState(FParticipantId Participant) const override
		{
			const EObjectivesConnectionState* State = States.Find(Participant.Value);
			return State != nullptr ? *State : EObjectivesConnectionState::Connected;
		}
	};

	class FStubPawnControl : public IObjectivesPawnControlView
	{
	public:
		TMap<const APawn*, uint32> Epochs;
		TSet<const APawn*> Pending;

		virtual uint32 GetControlEpoch(const APawn& Pawn) const override
		{
			const uint32* Epoch = Epochs.Find(&Pawn);
			return Epoch != nullptr ? *Epoch : 1u;
		}
		virtual bool IsHandOverPending(const APawn& Pawn) const override { return Pending.Contains(&Pawn); }
	};

	/** Round's hooks: phase, latch, the declared wins and every appended entry, the slot key and time. */
	class FStubRound : public IRoundObjectiveHooks
	{
	public:
		bool bActive = true;
		bool bLatched = false;
		/** When true, the first DeclareObjectiveWin latches (as Round's TryTerminate does). */
		bool bLatchOnWin = true;
		FQueueKey Key = FQueueKey(1, 1);
		double Now = ActivePhaseStart;
		TArray<TPair<uint8, FQueueKey>> Wins;
		TArray<FRoundLogEntry> Entries;

		virtual bool IsActive() const override { return bActive; }
		virtual bool IsLatched() const override { return bLatched; }
		virtual bool DeclareObjectiveWin(uint8 Team, FQueueKey InKey) override
		{
			Wins.Emplace(Team, InKey);
			if (bLatched)
			{
				return false;
			}
			bLatched = bLatchOnWin;
			return true;
		}
		virtual void AppendEvent(const FRoundLogEntry& Entry) override { Entries.Add(Entry); }
		virtual FQueueKey CurrentKey() const override { return Key; }
		virtual double ServerNow() const override { return Now; }

		int32 Count(ERoundLogType Type) const
		{
			return Entries.FilterByPredicate([Type](const FRoundLogEntry& E) { return E.Type == Type; }).Num();
		}
		TArray<FRoundLogEntry> OfType(ERoundLogType Type) const
		{
			return Entries.FilterByPredicate([Type](const FRoundLogEntry& E) { return E.Type == Type; });
		}
	};

	/** Four faces 120 cm around a court's centre (X = court id * 1000 cm), in face order +X, +Y, -X, -Y. */
	class FStubCourts : public IObjectivesCourtGeometry
	{
	public:
		static FVector CentreOf(FCourtFixtureId Court) { return FVector(static_cast<double>(Court.Value) * 1000.0, 0.0, 0.0); }
		static FVector FaceOf(FCourtFixtureId Court, int32 Face)
		{
			const FVector Offsets[] = { FVector(120, 0, 0), FVector(0, 120, 0), FVector(-120, 0, 0), FVector(0, -120, 0) };
			return CentreOf(Court) + Offsets[Face];
		}
		virtual bool GetFixtureFaces(FCourtFixtureId Court, TArray<FVector, TInlineAllocator<4>>& Out) const override
		{
			Out.Reset();
			for (int32 Face = 0; Face < 4; ++Face)
			{
				Out.Add(FaceOf(Court, Face));
			}
			return Court.IsSet();
		}
	};

	/** Records every team-level view push (the core's change-only PushView) and the last value per team. */
	class FStubViews : public IObjectivesViewSink
	{
	public:
		TArray<uint8> Pushes;
		FObjectiveTeamView Last[2];
		virtual void PushTeamView(uint8 Team, const FObjectiveTeamView& View) override
		{
			Pushes.Add(Team);
			if (Team < 2)
			{
				Last[Team] = View;
			}
		}
		int32 PushesFor(uint8 Team) const { return Pushes.FilterByPredicate([Team](uint8 T) { return T == Team; }).Num(); }
	};

	/**
	 * The service's view targets (story oc-011): members per team and one transient UObjectiveTeamViewComponent per member
	 * (no controller: ApplyView still counts and compares; nothing replicates in a headless rig).
	 */
	class FStubViewTargets : public IObjectivesViewTargets
	{
	public:
		struct FMember
		{
			FParticipantId Id;
			int32 Team = 0;
			TStrongObjectPtr<UObjectiveTeamViewComponent> View;
		};
		TArray<FMember> Members;

		UObjectiveTeamViewComponent* Add(uint32 Participant, int32 Team)
		{
			FMember& Member = Members.AddDefaulted_GetRef();
			Member.Id = FParticipantId(Participant);
			Member.Team = Team;
			Member.View.Reset(NewObject<UObjectiveTeamViewComponent>(GetTransientPackage(), NAME_None, RF_Transient));
			return Member.View.Get();
		}
		/** Replaces Participant's component (a new controller after a rejoin or travel). */
		UObjectiveTeamViewComponent* Replace(uint32 Participant)
		{
			for (FMember& Member : Members)
			{
				if (Member.Id.Value == Participant)
				{
					Member.View.Reset(NewObject<UObjectiveTeamViewComponent>(GetTransientPackage(), NAME_None, RF_Transient));
					return Member.View.Get();
				}
			}
			return nullptr;
		}
		UObjectiveTeamViewComponent* ViewOf(uint32 Participant) const
		{
			const FMember* Found = Members.FindByPredicate([Participant](const FMember& M) { return M.Id.Value == Participant; });
			return Found != nullptr ? Found->View.Get() : nullptr;
		}
		virtual void GetTeamMembers(uint8 Team, TArray<FParticipantId>& Out) const override
		{
			Out.Reset();
			for (const FMember& Member : Members)
			{
				if (Member.Team == Team)
				{
					Out.Add(Member.Id);
				}
			}
		}
		virtual UObjectiveTeamViewComponent* FindTeamView(FParticipantId Participant) const override { return ViewOf(Participant.Value); }
	};

	/** Records every release request: Calls holds (completing team, ordinal); Requests the full drawn release. */
	class FStubRival : public IObjectivesRivalRelease
	{
	public:
		TArray<TPair<uint8, uint32>> Calls;
		TArray<FObjectivesRivalRelease> Requests;
		virtual void ScheduleRivalRelease(const FObjectivesRivalRelease& Release) override
		{
			Calls.Emplace(Release.CompletingTeam, Release.Ordinal);
			Requests.Add(Release);
		}
	};

	/** Fixed StageSignal sub-seeds: SubSeedBase + ordinal (or the explicit per-ordinal map); counts the reads. */
	class FStubSecretDraw : public IObjectivesSecretDraw
	{
	public:
		uint64 SubSeedBase = 0x5EED5EED00000000ull;
		TMap<uint32, uint64> PerOrdinal;
		mutable TArray<uint32> Reads;
		virtual uint64 GetStageSignalSubSeed(uint32 Ordinal) const override
		{
			Reads.Add(Ordinal);
			const uint64* Fixed = PerOrdinal.Find(Ordinal);
			return Fixed != nullptr ? *Fixed : SubSeedBase + Ordinal;
		}
	};

	/** Public ClueTag records per court (story oc-013); empty: no clue. */
	class FStubClues : public IObjectivesCourtClues
	{
	public:
		TArray<FCourtClueRecord> Records;
		mutable int32 Reads = 0;
		void Add(uint16 Court, EClueAreaType Area, EClueFeatureKind Feature = EClueFeatureKind::None)
		{
			FCourtClueRecord& Record = Records.AddDefaulted_GetRef();
			Record.Court = FCourtFixtureId(Court);
			Record.AreaTypeId = static_cast<uint8>(Area);
			Record.NearestFeatureKind = static_cast<uint8>(Feature);
		}
		virtual void GetCourtClueRecords(TArray<FCourtClueRecord>& Out) const override
		{
			++Reads;
			Out = Records;
		}
	};

	class FStubChains : public IObjectivesChainSource
	{
	public:
		TArray<const UObjectiveChainData*> Chains;
		virtual void GetCookedChains(TArray<const UObjectiveChainData*>& Out) const override { Out = Chains; }
	};

	// --- The rig --------------------------------------------------------------------------------------------------------

	/** A core over the stubs, bound to the Cacao chain (and Active when asked). */
	struct FCoreRig
	{
		FStubAssignment Assignment;
		FStubRoster Roster;
		FStubSession Session;
		FStubPawnControl PawnControl;
		FStubRound Round;
		FStubCourts Courts;
		FStubViews Views;
		FStubRival Rival;
		FStubSecretDraw SecretDraw;
		FStubClues Clues;
		FObjectivesCore Core;
		TStrongObjectPtr<UObjectiveChainData> Chain;

		explicit FCoreRig(bool bActive = true)
		{
			FObjectivesCoreDeps Deps;
			Deps.Roster = &Roster;
			Deps.Session = &Session;
			Deps.PawnControl = &PawnControl;
			Deps.Round = &Round;
			Deps.Courts = &Courts;
			Deps.Views = &Views;
			Deps.RivalRelease = &Rival;
			Deps.SecretDraw = &SecretDraw;
			Deps.Clues = &Clues;
			Core.Initialize(Deps);
			Core.SetTuning(FObjectivesTuning_Public(), FObjectivesTuning_Server());
			Chain.Reset(MakeCacaoChain());
			Core.Bind(Chain.Get(), Assignment);
			if (bActive)
			{
				Core.HandleActiveEntry(RoundEpoch, ActivePhaseStart);
			}
		}

		/** A pawn standing at Court's face (0..3), spawned in World. */
		static AStealthPawn* PawnAt(const StealthAssassinationTest::FScopedWorld& World, uint16 Court, int32 Face = 0)
		{
			AStealthPawn* Pawn = World.SpawnPawn(0.0);
			if (Pawn != nullptr)
			{
				Pawn->SetActorLocation(FStubCourts::FaceOf(FCourtFixtureId(Court), Face) + FVector(0, 0, 100));
			}
			return Pawn;
		}

		/** A full tend in one call: Begin (claim) then the Complete slot with Key. */
		EObjectivesCompletion Tend(const APawn& Pawn, uint16 Court, FQueueKey Key = FQueueKey(2, 1))
		{
			Core.OnInteractExecuted(Pawn, FCourtFixtureId(Court), FQueueKey(1, 1));
			Round.Key = Key;
			return Core.OnInteractEnded(Pawn, FCourtFixtureId(Court), EObjectivesInteractEnd::Completed, Key);
		}

		/** Every site state of both teams of Core (a snapshot for "nothing changed"). */
		static TArray<EObjectiveSiteState> SnapshotOf(const FObjectivesCore& Core)
		{
			TArray<EObjectiveSiteState> Out;
			for (int32 Team = 0; Team < 2; ++Team)
			{
				for (int32 Def = 0; Def < Core.GetDefinitionCount(); ++Def)
				{
					Out.Add(Core.GetSiteState(Team, Def));
				}
			}
			return Out;
		}

		/** Every site state of both teams (a snapshot for "nothing changed"). */
		TArray<EObjectiveSiteState> Snapshot() const
		{
			TArray<EObjectiveSiteState> Out;
			for (int32 Team = 0; Team < 2; ++Team)
			{
				for (int32 Def = 0; Def < Core.GetDefinitionCount(); ++Def)
				{
					Out.Add(Core.GetSiteState(Team, Def));
				}
			}
			return Out;
		}
	};
}

#endif
