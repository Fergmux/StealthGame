// Story psr-061 (production/epics/participant-state-roster/story-061-roster-store-facade-and-match-lifetime.md):
// URosterStore wraps FRosterCore with engine adapters, and BeginMatch/EndMatch run on every session exit path.
//
//   StealthGame.Roster.StoreFacadeAdapters  AC-PSR-61a: the store owns one core whose dependencies are the engine
//                                           adapters (authority, game thread, pawn liveness, object-key + generation
//                                           handle, clock, fault sink, ADR-0006 sub-seeds for domains 15 and 17,
//                                           Round's phase registered weakly).
//   StealthGame.Roster.StoreMatchLifetime   AC-PSR-61b: every exit path leaves no record, binding or MatchId;
//                                           OpenSession runs BeginMatch with the latched TeamSize.
//   StealthGame.Roster.StoreClientEmpty     AC-PSR-61c: a client-process store answers the client value, faults
//                                           once per query and holds no record.
//
// Deterministic: the session secret comes from the fixture entropy (RoundTestSeeds S1), the clock is the fake clock,
// and no wall-clock value is asserted. The authority adapter's net-mode branch needs a client world; the client
// process is driven through the store's authority override (FRosterStoreInitOverrides), and the default branch (no
// game world, editor process) is asserted to answer authority.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Async/Async.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Server/Roster/StealthRosterStore.h"
#include "Server/Testing/StealthFakeClock.h"
#include "Server/Testing/StealthFaultSinkTestSupport.h"
#include "Server/Testing/StealthQueueTestRig.h"
#include "View/StealthRosterTeamView.h"

namespace RosterStoreTestData
{
	constexpr int32 TeamSize = 3;
	constexpr double ClockStartSeconds = 50.0;
	constexpr double DisconnectAtSeconds = 61.25;

	inline FIdentityKey Key(int32 Index) { return FIdentityKey(0x5704E00000000000ull, 7000ull + static_cast<uint64>(Index)); }

	/** A bare game world for the test; torn down by the destructor. */
	struct FScopedTestWorld
	{
		UWorld* World = nullptr;

		FScopedTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RosterStoreTestWorld"));
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
		}

		~FScopedTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};

	/** Round phase stand-in the test flips. */
	class FFakeRoundPhase : public IRosterRoundPhase
	{
	public:
		bool bActive = false;
		virtual bool IsRoundPhaseActive() const override { return bActive; }
	};

	/** A store with the fixture secret, the fake clock and a recording sink. */
	struct FStoreFixture
	{
		FFakeStealthClock Clock{ ClockStartSeconds };
		FStealthRecordingFaultSink Sink;
		FStealthFakeEntropySource Entropy{ RoundTestFixtures::SecretS1 };
		URosterStore* Store = nullptr;

		/** The store's outer (a game-instance subsystem must live inside a UGameInstance). */
		UGameInstance* GameInstance = nullptr;

		explicit FStoreFixture(TOptional<bool> bAuthority = TOptional<bool>())
		{
			GameInstance = NewObject<UGameInstance>(GetTransientPackage());
			GameInstance->AddToRoot();
			Store = NewObject<URosterStore>(GameInstance);
			Store->AddToRoot();
			FRosterStoreInitOverrides Overrides;
			Overrides.bAuthority = bAuthority;
			Overrides.FaultSink = &Sink;
			Overrides.Entropy = &Entropy;
			Overrides.Clock = &Clock;
			Store->InitializeStore(Overrides);
		}

		~FStoreFixture()
		{
			Store->ShutdownStore();
			Store->RemoveFromRoot();
			GameInstance->RemoveFromRoot();
		}

		/** OpenSession then registers Count participants; returns their ids. */
		TArray<FParticipantId> OpenAndRegister(int32 Count)
		{
			Store->OpenSession(TeamSize);
			TArray<FParticipantId> Ids;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Ids.Add(Store->RegisterParticipant(Key(Index), FString::Printf(TEXT("P%d"), Index)).Id);
			}
			return Ids;
		}

		/** Splits Ids 0..Split-1 / rest, locks and resets round 1 so everyone is Active. */
		void StartRound(const TArray<FParticipantId>& Ids, int32 Split)
		{
			TArray<FRosterTeamAssignment> Batch;
			for (int32 Index = 0; Index < Ids.Num(); ++Index)
			{
				Batch.Add({ Ids[Index], Index < Split ? 0 : 1 });
			}
			Store->AssignTeams(Batch);
			Store->SetTeamsLocked(true);
			Store->ResetAll(1);
		}
	};
}

// -------------------------------------------------------------------------------------------------------------------
// AC-PSR-61a
// -------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterStoreFacadeAdaptersTest, "StealthGame.Roster.StoreFacadeAdapters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterStoreFacadeAdaptersTest::RunTest(const FString& Parameters)
{
	using namespace RosterStoreTestData;

	// Default authority adapter (no game world, editor process): the store answers as a server.
	{
		UGameInstance* PlainInstance = NewObject<UGameInstance>(GetTransientPackage());
		URosterStore* Plain = NewObject<URosterStore>(PlainInstance);
		FStealthRecordingFaultSink PlainSink;
		FRosterStoreInitOverrides Overrides;
		Overrides.FaultSink = &PlainSink;
		Plain->InitializeStore(Overrides);
		TestTrue(TEXT("default authority adapter: OpenSession applies outside a client world"), Plain->OpenSession(TeamSize) == ERosterResult::Applied);
		TestEqual(TEXT("default adapters raised no fault"), PlainSink.GetRecords().Num(), 0);
		Plain->ShutdownStore();
	}

	FStoreFixture F;
	TestTrue(TEXT("one core per store"), F.Store->IsStoreInitialized());

	// --- Random: ADR-0006 domains 15 and 17 through GetServerSubSeed on the session secret -------------------------
	FStealthFakeEntropySource ReferenceEntropy(RoundTestFixtures::SecretS1);
	FStealthServerSeeds Reference;
	Reference.Open(ReferenceEntropy);
	const TArray<FParticipantId> Ids = F.OpenAndRegister(4);
	TestTrue(TEXT("MatchId equals GetServerSubSeed(MatchId 17, counter 0) of the session secret"),
		F.Store->GetCore().GetMatchId() == Reference.GetServerSubSeed(ESeedDomain::MatchId, 0));
	TestTrue(TEXT("first PublicHandle equals GetServerSubSeed(PublicHandle 15, counter 0) of the session secret"),
		F.Store->FindRecord(Ids[0])->PublicHandle == static_cast<uint32>(Reference.GetServerSubSeed(ESeedDomain::PublicHandle, 0)));
	TestTrue(TEXT("second PublicHandle equals GetServerSubSeed(PublicHandle 15, counter 1)"),
		F.Store->FindRecord(Ids[1])->PublicHandle == static_cast<uint32>(Reference.GetServerSubSeed(ESeedDomain::PublicHandle, 1)));
	TestEqual(TEXT("ERosterRandomDomain::PublicHandle is ADR-0006 domain 15"), static_cast<uint32>(ERosterRandomDomain::PublicHandle), static_cast<uint32>(ESeedDomain::PublicHandle));
	TestEqual(TEXT("ERosterRandomDomain::MatchId is ADR-0006 domain 17"), static_cast<uint32>(ERosterRandomDomain::MatchId), static_cast<uint32>(ESeedDomain::MatchId));

	// --- Clock: the connection notify stamps the store clock -------------------------------------------------------
	F.StartRound(Ids, 2);
	F.Clock.AdvanceSeconds(DisconnectAtSeconds - ClockStartSeconds);
	TestTrue(TEXT("disconnect notify applies"), F.Store->NotifyConnectionChanged(Ids[3], ERosterConnection::Disconnected) == ERosterResult::Applied);
	TestEqual(TEXT("DisconnectedAtMs is the store clock in ms"), F.Store->FindRecord(Ids[3])->Connection.DisconnectedAtMs, static_cast<int64>(DisconnectAtSeconds * 1000.0));

	// --- Pawn liveness and the object-key + generation handle ------------------------------------------------------
	FScopedTestWorld TestWorld;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APawn* PlayerPawn = TestWorld.World->SpawnActor<APawn>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	APawn* NpcPawn = TestWorld.World->SpawnActor<APawn>(FVector(300.0, 0.0, 0.0), FRotator::ZeroRotator, Params);
	F.Store->NotifyPawnSpawned(PlayerPawn);
	F.Store->NotifyPawnSpawned(NpcPawn);
	const FRosterPawnHandle Handle = F.Store->MakePawnHandle(PlayerPawn);
	TestTrue(TEXT("a pawn handle is non-null"), Handle.IsSet());
	TestEqual(TEXT("the handle carries the spawn generation"), Handle.Generation, 1u);
	TestTrue(TEXT("the handle resolves back to the pawn"), F.Store->ResolvePawn(Handle) == PlayerPawn);
	TestTrue(TEXT("two pawns get different object keys"), F.Store->MakePawnHandle(NpcPawn).ObjectKey != Handle.ObjectKey);
	TestTrue(TEXT("BindPawn through the facade applies"), F.Store->BindPawn(Ids[0], PlayerPawn) == ERosterResult::Applied);
	TestTrue(TEXT("a live bound pawn resolves Player"), F.Store->ResolveVictimKind(PlayerPawn) == FRosterVictimKind::Make(ERosterVictimKind::Player));
	TestTrue(TEXT("an unbound pawn resolves Npc"), F.Store->ResolveVictimKind(NpcPawn) == FRosterVictimKind::Make(ERosterVictimKind::Npc));
	TestTrue(TEXT("GetPawn maps the participant back to the engine pawn"), F.Store->GetPawn(Ids[0]) == PlayerPawn);
	// A new spawn generation of the same object is a different handle (a recycled pawn never inherits a binding).
	F.Store->NotifyPawnSpawned(PlayerPawn);
	TestFalse(TEXT("after a new spawn generation the old binding is not this pawn's"), F.Store->IsPawnBound(PlayerPawn));
	// Liveness: a destroyed pawn is Stale, never Npc.
	TestTrue(TEXT("BindPawn the NPC body to participant 1"), F.Store->BindPawn(Ids[1], NpcPawn) == ERosterResult::Applied);
	NpcPawn->Destroy();
	TestTrue(TEXT("a destroyed pawn resolves NotTargetable(Stale) through the liveness adapter"),
		F.Store->ResolveVictimKind(NpcPawn) == FRosterVictimKind::NotTargetable(ERosterNotTargetableReason::Stale));

	// --- Game thread: an off-thread mutator is rejected through the IsInGameThread adapter ------------------------
	const int32 FaultsBeforeThread = F.Sink.GetRecords().Num();
	URosterStore* Store = F.Store;
	const ERosterResult OffThread = Async(EAsyncExecution::Thread, [Store]() { return Store->SetTeamsLocked(false); }).Get();
	TestTrue(TEXT("an off-game-thread mutator is Rejected_WrongThread"), OffThread == ERosterResult::Rejected_WrongThread);
	TestEqual(TEXT("the wrong-thread call reported one fault to the injected sink"), F.Sink.GetRecords().Num(), FaultsBeforeThread + 1);

	// --- Round phase: none registered = not Active; registered weakly; a dead owner reads not Active ---------------
	TestTrue(TEXT("eliminate team 1's last active member"), F.Store->RequestTransition(Ids[2], EParticipantState::Informant, ERosterReason::Assassinated) == ERosterResult::Applied);
	TestTrue(TEXT("transition the other team-1 member"), F.Store->RequestTransition(Ids[3], EParticipantState::Informant, ERosterReason::Assassinated) == ERosterResult::Applied);
	TestFalse(TEXT("no Round registered: IsTeamEliminated answers false (phase not Active)"), F.Store->IsTeamEliminated(1));
	FFakeRoundPhase Phase;
	Phase.bActive = true;
	URosterTeamViewComponent* PhaseOwner = NewObject<URosterTeamViewComponent>(GetTransientPackage());
	F.Store->RegisterRoundPhase(&Phase, PhaseOwner);
	TestTrue(TEXT("Round registered and Active: IsTeamEliminated(1) is true"), F.Store->IsTeamEliminated(1));
	const int32 FaultsBeforeDead = F.Sink.GetRecords().Num();
	PhaseOwner->MarkAsGarbage();
	TestFalse(TEXT("a dead weak registration reads not Active"), F.Store->IsTeamEliminated(1));
	TestFalse(TEXT("and keeps reading not Active"), F.Store->IsTeamEliminated(1));
	TestEqual(TEXT("the dead registration faulted exactly once"), F.Sink.GetRecords().Num(), FaultsBeforeDead + 1);
	TestTrue(TEXT("the fault is DeadRegistration"), F.Sink.GetRecords().Last().Fault == EStealthFault::DeadRegistration);
	return true;
}

// -------------------------------------------------------------------------------------------------------------------
// AC-PSR-61b
// -------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterStoreMatchLifetimeTest, "StealthGame.Roster.StoreMatchLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterStoreMatchLifetimeTest::RunTest(const FString& Parameters)
{
	using namespace RosterStoreTestData;

	enum class EExitPath : uint8 { HostQuit, ReturnToMainMenu, SessionEnding, GameInstanceShutdown, LastWorldCleanup };
	const TPair<EExitPath, const TCHAR*> Paths[] = {
		{ EExitPath::HostQuit, TEXT("host quit") },
		{ EExitPath::ReturnToMainMenu, TEXT("return to main menu") },
		{ EExitPath::SessionEnding, TEXT("OnSessionEnding") },
		{ EExitPath::GameInstanceShutdown, TEXT("UGameInstance::Shutdown") },
		{ EExitPath::LastWorldCleanup, TEXT("OnWorldCleanup of the last game world") },
	};

	for (const TPair<EExitPath, const TCHAR*>& Path : Paths)
	{
		FStoreFixture F;
		const TArray<FParticipantId> Ids = F.OpenAndRegister(4);
		TestTrue(FString::Printf(TEXT("%s: OpenSession opened the match"), Path.Value), F.Store->IsMatchOpen());
		TestEqual(FString::Printf(TEXT("%s: BeginMatch got the latched TeamSize"), Path.Value), F.Store->GetCore().GetTeamSize(), TeamSize);
		F.StartRound(Ids, 2);

		FScopedTestWorld TestWorld;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APawn* Pawn = TestWorld.World->SpawnActor<APawn>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		F.Store->NotifyPawnSpawned(Pawn);
		F.Store->BindPawn(Ids[0], Pawn);
		TestTrue(FString::Printf(TEXT("%s: arranged a live binding"), Path.Value), F.Store->IsPawnBound(Pawn));
		TestTrue(FString::Printf(TEXT("%s: arranged a MatchId"), Path.Value), F.Store->GetCore().GetMatchId() != 0);

		switch (Path.Key)
		{
		case EExitPath::HostQuit: F.Store->NotifyHostQuit(); break;
		case EExitPath::ReturnToMainMenu: F.Store->NotifyReturnToMainMenu(); break;
		case EExitPath::SessionEnding: F.Store->NotifySessionEnding(); break;
		case EExitPath::GameInstanceShutdown: F.Store->HandleGameInstanceShutdown(); break;
		case EExitPath::LastWorldCleanup:
			TestWorld.World->SetGameInstance(F.GameInstance);
			F.Store->HandleWorldCleanup(TestWorld.World, true, true);
			TestWorld.World->SetGameInstance(nullptr);
			break;
		}

		TArray<FParticipantId> Remaining;
		F.Store->GetParticipantIds(Remaining);
		TestEqual(FString::Printf(TEXT("%s: no record survives"), Path.Value), Remaining.Num(), 0);
		TestEqual(FString::Printf(TEXT("%s: no binding survives"), Path.Value), F.Store->GetCore().Snapshot().Bindings.Num(), 0);
		TestTrue(FString::Printf(TEXT("%s: no MatchId survives"), Path.Value), F.Store->GetCore().GetMatchId() == 0);
		TestFalse(FString::Printf(TEXT("%s: the match is closed"), Path.Value), F.Store->IsMatchOpen());
	}

	// A travel's old-world cleanup is not an exit path.
	{
		FStoreFixture F;
		F.OpenAndRegister(2);
		FScopedTestWorld TestWorld;
		F.Store->SetTravelInProgress(true);
		TestWorld.World->SetGameInstance(F.GameInstance);
		F.Store->HandleWorldCleanup(TestWorld.World, false, true);
		TestWorld.World->SetGameInstance(nullptr);
		TestTrue(TEXT("travel in progress: the cleanup keeps the match"), F.Store->IsMatchOpen());
		TArray<FParticipantId> Remaining;
		F.Store->GetParticipantIds(Remaining);
		TestEqual(TEXT("travel in progress: records kept"), Remaining.Num(), 2);
	}
	return true;
}

// -------------------------------------------------------------------------------------------------------------------
// AC-PSR-61c
// -------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterStoreClientEmptyTest, "StealthGame.Roster.StoreClientEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterStoreClientEmptyTest::RunTest(const FString& Parameters)
{
	using namespace RosterStoreTestData;

	FStoreFixture F(/*bAuthority*/ false);
	FScopedTestWorld TestWorld;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APawn* Pawn = TestWorld.World->SpawnActor<APawn>(FVector::ZeroVector, FRotator::ZeroRotator, Params);

	// A client cannot open a session or register: the store stays empty.
	TestTrue(TEXT("client OpenSession is Rejected_NotAuthority"), F.Store->OpenSession(TeamSize) == ERosterResult::Rejected_NotAuthority);
	TestFalse(TEXT("client RegisterParticipant issues no id"), F.Store->RegisterParticipant(Key(0), TEXT("Client")).Id.IsValid());

	struct FQueryRow
	{
		const TCHAR* Name;
		TFunction<bool()> RunAndCheck;
	};
	URosterStore* Store = F.Store;
	const FQueryRow Rows[] = {
		{ TEXT("ResolveVictimKind"), [Store, Pawn]() { return Store->ResolveVictimKind(Pawn) == FRosterVictimKind::NotTargetable(ERosterNotTargetableReason::Stale); } },
		{ TEXT("IsPawnBound"), [Store, Pawn]() { return !Store->IsPawnBound(Pawn); } },
		{ TEXT("GetLegalityClass"), [Store, Pawn]() { return Store->GetLegalityClass(Pawn) == EParticipantState::Active; } },
		{ TEXT("CountActive"), [Store]() { return Store->CountActive(0) == 0; } },
		{ TEXT("CountInformant"), [Store]() { return Store->CountInformant(1) == 0; } },
		{ TEXT("CountRoster"), [Store]() { return Store->CountRoster(0) == 0; } },
		{ TEXT("IsTeamEliminated"), [Store]() { return !Store->IsTeamEliminated(0); } },
		{ TEXT("GetPawn"), [Store]() { return Store->GetPawn(FParticipantId(0)) == nullptr; } },
		{ TEXT("GetParticipant"), [Store, Pawn]() { return !Store->GetParticipant(Pawn).IsValid(); } },
		{ TEXT("GetUnboundActiveParticipants"), [Store]()
			{
				TArray<FParticipantId> Out;
				Store->GetUnboundActiveParticipants(Out);
				return Out.Num() == 0;
			} },
	};
	for (const FQueryRow& Row : Rows)
	{
		const int32 Before = F.Sink.GetRecords().Num();
		TestTrue(FString::Printf(TEXT("client %s returns the client answer"), Row.Name), Row.RunAndCheck());
		if (TestEqual(FString::Printf(TEXT("client %s reports exactly one fault"), Row.Name), F.Sink.GetRecords().Num(), Before + 1))
		{
			TestTrue(FString::Printf(TEXT("client %s fault is RosterNotAuthority"), Row.Name), F.Sink.GetRecords().Last().Fault == EStealthFault::RosterNotAuthority);
		}
	}

	TArray<FParticipantId> Ids;
	F.Store->GetParticipantIds(Ids);
	TestEqual(TEXT("the client store holds no record"), Ids.Num(), 0);
	TestFalse(TEXT("the client store never opened a match"), F.Store->IsMatchOpen());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS
