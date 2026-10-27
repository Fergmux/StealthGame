// Story nkp-009 (production/epics/npc-kill-penalty-informants/story-009-respawn-due-time-scheduling-and-cancellation.md),
// AC4 (derived, ADR-0019 section 3.2 and Implementation Guidelines): GIVEN each Active -> Informant cause (Assassinated,
// ReversedAgainst, StrikeLimit), WHEN Round calls IInformantBodyProvider::OnInformantAwaitingBody after its elimination
// check, THEN exactly one TimerFired(InformantRespawn) item is scheduled on the REAL queue with payload {Subject = P, Seq =
// RespawnRequestSeq, Generation = 0, Phase = 0} and the RoundEpoch as its Expected, and the state is Awaiting; AND the REAL
// Round core makes the call after its elimination check for each cause and NOT when the transition latched the round (the
// deciding slot) or for another reason (Disconnect); AND the one Roster state event Penalty handles (grace expiry) schedules
// nothing; AND a source lint of Features/Penalty finds no engine timer, tick, latent action or OnParticipantStateChanged
// binding (negative control: a seeded SetTimer is caught). Edge: the InformantKilled cause, which Penalty schedules itself,
// gives one item too; a Disconnected P goes to Absent (story nkp-010 asserts the rest of that path).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/Testing/StealthNegativeControl.h"
#include "Server/Testing/StealthQueueTestRig.h"
#include "StealthPenaltyQueueAdapter.h"
#include "Tests/Penalty/PenaltySourceScan.h"
#include "Tests/Penalty/PenaltyTestRig.h"
#include "Tests/Round/StealthRoundWinQueueRig.h"

namespace PenaltySchedulingSourcesTestData
{
	/** Captures every TimerFired item the real queue stamps (in place of Penalty's resolver, to read the payload). */
	class FCapturingResolver final : public IQueueResolver
	{
	public:
		TArray<FQueueItem> Items;
		virtual EResolveResult Resolve(const FQueueItem& Item, FQueueKey Key) override
		{
			Items.Add(Item);
			return EResolveResult::Voided;
		}
	};

	/** The identifiers an engine timer, tick, latent action or a state-event scheduling binding would use. */
	const TCHAR* const Forbidden[] = {
		TEXT("SetTimer"), TEXT("GetTimerManager"), TEXT("FTimerHandle"), TEXT("FTimerDelegate"), TEXT("FTickableGameObject"),
		TEXT("TickComponent"), TEXT("PrimaryActorTick"), TEXT("FLatentAction"), TEXT("FPendingLatentAction"), TEXT("OnParticipantStateChanged"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltySchedulingSourcesTest, "StealthGame.Penalty.SchedulingSources",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltySchedulingSourcesTest::RunTest(const FString& Parameters)
{
	using namespace StealthPenaltyTest;
	using namespace PenaltySchedulingSourcesTestData;

	// --- Penalty on the REAL queue: one item per cause, the ADR-0002 payload shape. ---
	for (const ERespawnCause Cause : { ERespawnCause::Assassinated, ERespawnCause::ReversedAgainst, ERespawnCause::StrikeLimit, ERespawnCause::InformantKilled })
	{
		FStealthQueueTestRig Q2;
		FFakeRoster Roster;
		Roster.Epoch = Q2.Epoch.Current.RoundEpoch;
		Roster.States.Add(P, EParticipantState::Informant);
		FFakeTuning Tuning;
		FFakeSlot Slot;
		FRecordingLog Log;
		Log.Slot = &Slot;
		FStealthRecordingFaultSink Faults;
		FPenaltyRespawnQueueAdapter Adapter(Q2.Queue.Get(), [&Q2]() { return Q2.Epoch.Current.MatchId; });
		FPenaltyCoreDeps Deps;
		Deps.Clock = &Q2.Clock;
		Deps.Roster = &Roster;
		Deps.Tuning = &Tuning;
		Deps.Slot = &Slot;
		Deps.Log = &Log;
		Deps.Timers = &Adapter;
		Deps.Faults = &Faults;
		FPenaltyCore Core(Deps);
		FCapturingResolver Capture;
		Q2.Queue->RegisterTimerResolver(ETimerKind::InformantRespawn, &Capture);

		const double Now = Q2.Clock.GetServerTimeSeconds();
		if (Cause == ERespawnCause::InformantKilled)
		{
			FPenaltyKill Kill{ Pawn(901), Q, EVictimKind::Informant, EKillCause::Strike };
			Kill.VictimParticipant = P;
			Core.OnPawnKilled(Kill); // Penalty schedules this cause itself, inline (no Round call)
		}
		else
		{
			Core.OnInformantAwaitingBody(P, Cause, Now);
		}
		const FString Name = FString::Printf(TEXT("cause %d"), static_cast<int32>(Cause));
		TArray<ETimerKind> Kinds;
		Q2.Queue->GetPendingTimerKinds(Kinds);
		TestTrue(Name + TEXT(": exactly one TimerFired(InformantRespawn) item"), Kinds.Num() == 1 && Kinds[0] == ETimerKind::InformantRespawn);
		TestTrue(Name + TEXT(": Awaiting"), Core.GetLifeState(P) == EInformantLifeState::Awaiting);
		const uint32 Seq = Core.FindRecord(P)->RespawnRequestSeq;

		Q2.Tick(Core.GetDueServerTime(P).Get(0.0) - Now + 0.1);
		if (TestEqual(Name + TEXT(": the item fires once at T_due"), Capture.Items.Num(), 1))
		{
			const FTimerPayload* Payload = Capture.Items[0].Payload.TryGet<FTimerPayload>();
			TestTrue(Name + TEXT(": payload {Kind = InformantRespawn, Subject = P, Seq = RespawnRequestSeq, Generation = 0, Phase = 0}"), Payload != nullptr
				&& Payload->Kind == ETimerKind::InformantRespawn && Payload->Subject.Kind == ESubjectRefKind::Participant && Payload->Subject.Participant == P
				&& Payload->Seq == Seq && Seq != 0 && Payload->Generation == 0 && Payload->Phase == 0);
			TestTrue(Name + TEXT(": Expected = {MatchId, RoundEpoch}"), Capture.Items[0].Expected == Q2.Epoch.Current);
		}
		Q2.Queue->UnregisterTimerResolver(ETimerKind::InformantRespawn);
	}

	// --- The REAL Round core (3v3): the call after the elimination check, never in a latching slot, only for the causes. ---
	{
		FRoundWinQueueRig W;
		W.ToActive(3, 3);
		TArray<uint32> Team0;
		TArray<uint32> Team1;
		for (const FRoundRosterMember& Member : W.Rig.Fakes.Roster.Members)
		{
			(Member.Team == ERosterTeam::Team0 ? Team0 : Team1).Add(Member.Participant.Value);
		}
		const TArray<FRoundFakeInformantBodies::FAwaitingCall>& Awaiting = W.Rig.Fakes.InformantBodies.AwaitingCalls;
		if (TestTrue(TEXT("Round fixture: 3v3"), Team0.Num() == 3 && Team1.Num() == 3))
		{
			W.Kill(Team0[0], ERosterReason::Assassinated);
			TestTrue(TEXT("Round: Assassinated -> one OnInformantAwaitingBody(P, Assassinated, slot time)"), Awaiting.Num() == 1 && Awaiting[0].Participant.Value == Team0[0]
				&& Awaiting[0].Cause == ERespawnCause::Assassinated && Awaiting[0].LostServerTime == W.Rig.Clock.GetServerTimeSeconds());
			W.Kill(Team1[0], ERosterReason::ReversedAgainst);
			TestTrue(TEXT("Round: ReversedAgainst -> one call"), Awaiting.Num() == 2 && Awaiting[1].Participant.Value == Team1[0] && Awaiting[1].Cause == ERespawnCause::ReversedAgainst);
			W.Kill(Team1[1], ERosterReason::Disconnect);
			TestEqual(TEXT("Round: a grace expiry (Disconnect) is no respawn cause: no call"), Awaiting.Num(), 2);
			W.Kill(Team0[1], ERosterReason::StrikeLimit);
			TestTrue(TEXT("Round: StrikeLimit -> one call"), Awaiting.Num() == 3 && Awaiting[2].Participant.Value == Team0[1] && Awaiting[2].Cause == ERespawnCause::StrikeLimit);
			TestFalse(TEXT("Round: not latched yet"), W.Rig.Core->GetLatch().IsTerminated());
			W.Kill(Team0[2], ERosterReason::Assassinated);
			TestTrue(TEXT("Round: the deciding transition latched the round"), W.Rig.Core->GetLatch().IsTerminated());
			TestEqual(TEXT("Round: no call from the latching (deciding) slot"), Awaiting.Num(), 3);
		}
	}

	// --- The one Roster state event Penalty handles (grace expiry) schedules nothing; a Disconnected cause is Absent. ---
	{
		StealthPenaltyTest::FRig Rig;
		Rig.Roster.States.Add(P, EParticipantState::Informant);
		Rig.Core->OnGraceExpired(P);
		TestTrue(TEXT("grace expiry: Absent, no item"), Rig.Core->GetLifeState(P) == EInformantLifeState::Absent && Rig.Respawns.Calls.Num() == 0);
		Rig.Roster.Disconnected.Add(Q);
		Rig.Core->OnInformantAwaitingBody(Q, ERespawnCause::Assassinated, Rig.Clock.GetServerTimeSeconds());
		TestTrue(TEXT("a Disconnected victim: Absent, no item"), Rig.Core->GetLifeState(Q) == EInformantLifeState::Absent && Rig.Respawns.Calls.Num() == 0);
	}

	// --- Lint: no engine timer, tick, latent action or state-event binding in Penalty. ---
	{
		using namespace StealthPenaltySourceScan;
		const TArray<FSource> Sources = LoadSources();
		TestTrue(TEXT("lint: the Penalty sources were found"), Sources.Num() >= 8);
		TArray<FFinding> Findings;
		for (const FSource& Source : Sources)
		{
			FindIdentifierPrefixes(Source, Forbidden, Findings);
		}
		TestTrue(FString::Printf(TEXT("lint: no engine timer, tick, latent action or OnParticipantStateChanged binding (%s)"), *Describe(Findings)), Findings.IsEmpty());
		FStealthNegativeControl Control(TEXT("SeededPenaltyTimer"));
		FStealthNegativeControl::Report(*this, Control.Run([](FStealthCheckContext& Ctx)
		{
			TArray<FFinding> Seeded;
			FindIdentifierPrefixes(FSource{ TEXT("Seeded.cpp"), TEXT("// SetTimer in a comment is fine\nWorld->GetTimerManager().SetTimer(Handle, 15.0f, false);\n") }, Forbidden, Seeded);
			Ctx.TestTrue(TEXT("seeded source is clean"), Seeded.IsEmpty());
		}));
	}
	return true;
}

#endif
