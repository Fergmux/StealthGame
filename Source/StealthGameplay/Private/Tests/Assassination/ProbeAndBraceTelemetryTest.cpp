// Story asn-026 AC2 (the producer half of AC-ASN-33; ADR-0023 section 2.1): StealthGame.Assassination.ProbeAndBraceTelemetryProbes.
// UEncounterService over a real FResolutionQueueCore in a bare game world (every pawn Unbound, the same path for every
// pawn) with a recording IStealthTelemetrySink:
//   - a kill, a reversal and a whiff each record exactly one StrikeResolved in their slot, with the slot's key, the
//     attacker and victim as pawn refs (none for a whiff) and the outcome; the reversed defender's VictimBraceAgeSec is
//     contact - its brace start (> 0) and its defence is not ready (its cooldown runs from the activation); the unbraced
//     victim's is -1 and ready;
//   - every brace the core ends records one BraceEnded with its start: Reversal (the defender), Expired (run to the cap),
//     Removed (still up at the round Reset);
//   - with no sink bound (the null object, the production state until the Event Log's telemetry section exists) the same
//     commands resolve identically;
//   - the cost meter counts every resolution while enabled.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/EncounterService.h"
#include "Server/Testing/StealthQueueTestRig.h"
#include "Tests/Assassination/AssassinationTestWorld.h"

namespace ProbeAndBraceTelemetryTestData
{
	class FRecordingSink final : public IStealthTelemetrySink
	{
	public:
		TArray<FTelemetryStrikeResolved> Strikes;
		TArray<FTelemetryBraceEnded> Braces;
		int32 Others = 0;
		virtual void Record(const FTelemetryStrikeResolved& Event) override { Strikes.Add(Event); }
		virtual void Record(const FTelemetryBraceEnded& Event) override { Braces.Add(Event); }
		virtual void Record(const FTelemetryTendEnded&) override { ++Others; }
		virtual void Record(const FTelemetryBushTransition&) override { ++Others; }
		virtual void Record(const FTelemetryNpcTellAccepted&) override { ++Others; }

		const FTelemetryBraceEnded* BraceOf(const APawn* Pawn) const
		{
			return Braces.FindByPredicate([Pawn](const FTelemetryBraceEnded& E) { return E.Subject.Pawn.Pawn.Get() == Pawn; });
		}
	};

	FQueueItem Command(const FStealthQueueTestRig& Q, APawn* Pawn, ECommandKind Kind, const FVector& WorldPoint = FVector::ZeroVector, APawn* Target = nullptr)
	{
		FCommandPayload P;
		P.Pawn.Pawn = Pawn;
		P.Kind = static_cast<uint8>(Kind);
		P.WorldPoint = WorldPoint;
		if (Target != nullptr)
		{
			P.Target = FSubjectRef::ForPawn(FPawnHandle{ Target });
		}
		return FQueueItem::Make(EQueueItemType::PawnCommand, Q.Epoch.Current, FMailboxId::ServerLocal(), P);
	}

	void Submit(FStealthQueueTestRig& Q, const FQueueItem& Item)
	{
		Q.Queue->Submit(FMailboxId::ServerLocal(), Item);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationProbeAndBraceTelemetryTest, "StealthGame.Assassination.ProbeAndBraceTelemetryProbes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationProbeAndBraceTelemetryTest::RunTest(const FString& Parameters)
{
	using namespace ProbeAndBraceTelemetryTestData;
	// Arrange: five pairs far apart (each pawn faces +X, so a pawn 150 cm ahead is in reach).
	StealthAssassinationTest::FScopedWorld World(TEXT("ProbeAndBraceTelemetryWorld"));
	FStealthQueueTestRig Q;
	AStealthPawn* Killer = World.SpawnPawn(0.0);
	AStealthPawn* Victim = World.SpawnPawn(150.0);
	AStealthPawn* Striker = World.SpawnPawn(3000.0);
	AStealthPawn* Defender = World.SpawnPawn(3150.0);
	AStealthPawn* Whiffer = World.SpawnPawn(6000.0);
	AStealthPawn* Expirer = World.SpawnPawn(9000.0);
	AStealthPawn* Holder = World.SpawnPawn(12000.0);
	FRecordingSink Sink;
	UEncounterService* Service = NewObject<UEncounterService>();
	FEncounterServiceInit Init;
	Init.World = World.World;
	Init.Clock = &Q.Clock;
	Init.Queue = Q.Queue.Get();
	Init.SubSeeds = &Q.Seeds;
	Init.Telemetry = &Sink;
	if (!TestTrue(TEXT("the service initialises and registers"), Service->Initialize(Init) && Service->RegisterOn(*Q.Queue)))
	{
		return false;
	}
	const FEncounterParams Params = FEncounterParams::Defaults();
	FAssassinationCostMeter::Get().TakeMs();
	FAssassinationCostMeter::Get().bEnabled = true;

	// Act 1: the defender and the expirer brace; a kill and a whiff resolve.
	Submit(Q, Command(Q, Defender, ECommandKind::PrepareBegin));
	Submit(Q, Command(Q, Expirer, ECommandKind::PrepareBegin));
	Q.Tick();
	const double BraceStart = Q.Clock.GetServerTimeSeconds();
	for (int32 Frame = 0; Frame < 12; ++Frame)
	{
		Q.Tick();
	}
	Submit(Q, Command(Q, Killer, ECommandKind::Attack, Victim->GetActorLocation(), Victim));
	Submit(Q, Command(Q, Striker, ECommandKind::Attack, Defender->GetActorLocation(), Defender));
	Submit(Q, Command(Q, Whiffer, ECommandKind::Attack, Whiffer->GetActorLocation() + FVector(150.0, 0.0, 0.0)));
	Q.Tick();
	const double Exec = Q.Clock.GetServerTimeSeconds();

	// Assert 1: one StrikeResolved per Attack, in its slot.
	TestEqual(TEXT("three StrikeResolved records (kill, reversal, whiff)"), Sink.Strikes.Num(), 3);
	const FTelemetryStrikeResolved* Kill = Sink.Strikes.FindByPredicate([Killer](const FTelemetryStrikeResolved& E) { return E.Attacker.Pawn.Pawn.Get() == Killer; });
	const FTelemetryStrikeResolved* Reversal = Sink.Strikes.FindByPredicate([Striker](const FTelemetryStrikeResolved& E) { return E.Attacker.Pawn.Pawn.Get() == Striker; });
	const FTelemetryStrikeResolved* Whiff = Sink.Strikes.FindByPredicate([Whiffer](const FTelemetryStrikeResolved& E) { return E.Attacker.Pawn.Pawn.Get() == Whiffer; });
	if (!TestTrue(TEXT("each Attack has its record"), Kill != nullptr && Reversal != nullptr && Whiff != nullptr))
	{
		return false;
	}
	TestTrue(TEXT("kill: outcome Kill, victim ref, unbraced (-1), defence ready"), Kill->Outcome == ETelemetryStrikeOutcome::Kill && Kill->Victim.Pawn.Pawn.Get() == Victim
		&& Kill->VictimBraceAgeSec < 0.f && Kill->VictimSinceBraceEndSec < 0.f && Kill->bVictimDefenceReady);
	TestTrue(TEXT("kill: recorded with its slot key, refs only (pawn subjects)"), Kill->Key.IsSet() && Kill->Attacker.Kind == ESubjectRefKind::Pawn && Kill->Victim.Kind == ESubjectRefKind::Pawn);
	const double Contact = Exec + Params.Public.StrikeContactSec;
	TestTrue(TEXT("reversal: outcome Reversal, the defender as victim"), Reversal->Outcome == ETelemetryStrikeOutcome::Reversal && Reversal->Victim.Pawn.Pawn.Get() == Defender);
	TestEqual(TEXT("reversal: VictimBraceAgeSec = contact - brace start"), static_cast<double>(Reversal->VictimBraceAgeSec), Contact - BraceStart, 1e-3);
	TestTrue(TEXT("reversal: braced at contact (no time since a brace end) and not ready (cooldown from the activation)"), Reversal->VictimSinceBraceEndSec < 0.f && !Reversal->bVictimDefenceReady);
	TestTrue(TEXT("whiff: outcome Whiff, no victim"), Whiff->Outcome == ETelemetryStrikeOutcome::Whiff && Whiff->Victim.Kind == ESubjectRefKind::None);
	TestTrue(TEXT("court fields unknown until Objectives publishes them"), Kill->DistToNearestCourtCm < 0.f && !Kill->bNearestCourtIsFinalCandidate && !Kill->bTendingAtContact);
	const FTelemetryBraceEnded* Reversed = Sink.BraceOf(Defender);
	TestTrue(TEXT("the reversed defender's brace ended Reversal, from its start"), Reversed != nullptr && Reversed->Cause == ETelemetryBraceEnd::Reversal
		&& FMath::IsNearlyEqual(Reversed->StartTime, BraceStart, 1e-6));

	// Act 2: the expirer's brace runs to its cap; the holder braces and the round resets.
	for (double T = 0.0; T < Params.Public.DefenceDurationSec + 0.5; T += 1.0 / 60.0)
	{
		Q.Tick();
	}
	Submit(Q, Command(Q, Holder, ECommandKind::PrepareBegin));
	Q.Tick();
	Service->ResetRound();

	// Assert 2
	const FTelemetryBraceEnded* Expired = Sink.BraceOf(Expirer);
	TestTrue(TEXT("a brace run to its cap ended Expired"), Expired != nullptr && Expired->Cause == ETelemetryBraceEnd::Expired && FMath::IsNearlyEqual(Expired->StartTime, BraceStart, 1e-6));
	const FTelemetryBraceEnded* Removed = Sink.BraceOf(Holder);
	TestTrue(TEXT("a brace up at the round Reset ended Removed"), Removed != nullptr && Removed->Cause == ETelemetryBraceEnd::Removed);
	TestEqual(TEXT("one BraceEnded per brace"), Sink.Braces.Num(), 3);
	TestEqual(TEXT("no other telemetry record from Assassination"), Sink.Others, 0);
	TestTrue(TEXT("the cost meter counted the resolutions"), FAssassinationCostMeter::Get().Calls > 0);
	FAssassinationCostMeter::Get().bEnabled = false;
	FAssassinationCostMeter::Get().TakeMs();
	Service->Shutdown();

	// Act 3 + Assert 3: no sink (the null object): the same kill resolves the same way.
	StealthAssassinationTest::FScopedWorld NullWorld(TEXT("ProbeAndBraceTelemetryNullWorld"));
	FStealthQueueTestRig NullQ;
	AStealthPawn* NullKiller = NullWorld.SpawnPawn(0.0);
	AStealthPawn* NullVictim = NullWorld.SpawnPawn(150.0);
	UEncounterService* NullService = NewObject<UEncounterService>();
	FEncounterServiceInit NullInit;
	NullInit.World = NullWorld.World;
	NullInit.Clock = &NullQ.Clock;
	NullInit.Queue = NullQ.Queue.Get();
	NullInit.SubSeeds = &NullQ.Seeds;
	TestTrue(TEXT("null sink: the service initialises and registers"), NullService->Initialize(NullInit) && NullService->RegisterOn(*NullQ.Queue));
	NullQ.Queue->Submit(FMailboxId::ServerLocal(), Command(NullQ, NullKiller, ECommandKind::Attack, NullVictim->GetActorLocation(), NullVictim));
	NullQ.Tick();
	TestTrue(TEXT("null sink: the kill resolves in its slot"), NullService->IsDead(NullVictim));
	TestTrue(TEXT("null sink: the probe uses the null object"), NullService->GetTelemetryProbe() != nullptr && &NullService->GetTelemetryProbe()->GetSink() == &FNullStealthTelemetrySink::Get());
	NullService->Shutdown();
	return true;
}

#endif
