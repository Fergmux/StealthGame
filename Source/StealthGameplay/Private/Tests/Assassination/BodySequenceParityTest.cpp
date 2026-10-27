// Story asn-019 (production/epics/assassination-defence-reversal/story-019-server-body-sequence-and-reset.md), AC2 /
// ADR-0018 BodySequenceParity (the queue half, headless; the possession and Spectating half is
// StealthGame.Assassination.PIE.PossessionThroughFall), on the REAL round queue and Round core: three runs with identical
// kill geometry, an NPC, an Active player (its team keeps another Active member) and an informant victim:
//   - the body is released at DeathServerTime + DeathFallSec and removed at DeathServerTime + DeathFallSec + BodyLingerSec +
//     BodyRemovalGraceSec (+- 1 server frame), identically for the three kinds, through TimerFired(BodyRelease /
//     BodyRemoval) items (two due-time items per killed pawn), never an engine timer;
//   - the removal observer is called exactly once per body, before its teardown;
//   - latch half: a kill that decides the round (the latch committed in the kill's own slot, before both due times):
//     both items return Rejected_Latched, the observer is never called, the body is neither released nor torn down and
//     persists until Reset, which removes it;
//   - edge: a missing removal observer is a DeadRegistration fault and the body is still removed.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Assassination/EncounterRoundRig.h"

namespace BodySequenceParityTestData
{
	constexpr double Frame = RoundTestFixtures::RigFrameSec;
	constexpr double Epsilon = FResolutionQueueCore::DueEpsilonSec + 1e-6;

	enum class EKind : uint8 { Npc, Player, Informant };
	const TCHAR* KindName(EKind Kind) { return Kind == EKind::Npc ? TEXT("NPC") : (Kind == EKind::Player ? TEXT("player") : TEXT("informant")); }

	struct FTimes
	{
		double Release = -1.0;
		double Removal = -1.0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationBodySequenceParityTest, "StealthGame.Assassination.BodySequenceParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationBodySequenceParityTest::RunTest(const FString& Parameters)
{
	using namespace StealthEncounterTest;
	using namespace BodySequenceParityTestData;
	const FEncounterParams Params = FEncounterParams::Defaults();
	const double ReleaseAfter = Params.BodyReleaseDelaySec();
	const double RemovalAfter = Params.BodyRemovalDelaySec();
	TestTrue(TEXT("the removal delay is DeathFallSec + BodyLingerSec + BodyRemovalGraceSec (9.2 s at the defaults)"),
		FMath::IsNearlyEqual(RemovalAfter, 1.2 + 6.0 + 2.0, 1e-5) && FMath::IsNearlyEqual(ReleaseAfter, 1.2, 1e-5));

	FTimes Times[3];
	for (const EKind Kind : { EKind::Npc, EKind::Player, EKind::Informant })
	{
		const TCHAR* Name = KindName(Kind);
		FRoundEncounterRig R(TEXT("BodySequenceParityWorld"), /*bProductionWrites*/ true, /*bWithBodySequence*/ true);
		R.W.ToActive(RoundTestFixtures::Layout2v2.Team0, RoundTestFixtures::Layout2v2.Team1);
		if (Kind == EKind::Informant)
		{
			R.SetInformant(4);
		}
		APawn* A = R.Spawn(FVector::ZeroVector, 1);
		APawn* V = Kind == EKind::Npc ? R.Spawn(FVector(150.0, 0.0, 0.0)) : R.Spawn(FVector(150.0, 0.0, 0.0), Kind == EKind::Player ? 3 : 4);
		R.Drain({ R.AttackAt(A, V) });
		const FEncounterBody* Body = R.Core->FindBody(V);
		if (!TestNotNull(FString::Printf(TEXT("%s: the kill scheduled the body sequence"), Name), Body))
		{
			continue;
		}
		const double Death = Body->DeathServerTime;
		TArray<ETimerKind> Pending;
		R.W.Queue->GetPendingTimerKinds(Pending);
		TestTrue(FString::Printf(TEXT("%s: two due-time items (BodyRelease, BodyRemoval)"), Name),
			Pending.FilterByPredicate([](ETimerKind K) { return K == ETimerKind::BodyRelease || K == ETimerKind::BodyRemoval; }).Num() == 2);
		while (R.W.Rig.Clock.GetServerTimeSeconds() < Death + RemovalAfter + 0.25)
		{
			R.Tick();
		}
		TestFalse(FString::Printf(TEXT("%s: the round did not latch"), Name), R.W.Rig.Core->GetLatch().IsTerminated());
		TestTrue(FString::Printf(TEXT("%s: released once"), Name), R.HandOver.Released.Num() == 1 && R.HandOver.Released[0].Body == V);
		TestTrue(FString::Printf(TEXT("%s: the observer once, then one teardown"), Name), R.RemovalObserver.Committed.Num() == 1 && R.RemovalObserver.Committed[0] == V
			&& R.HandOver.TornDown.Num() == 1 && R.HandOver.TornDown[0].Body == V);
		if (R.HandOver.Released.Num() == 1 && R.HandOver.TornDown.Num() == 1)
		{
			FTimes& T = Times[static_cast<int32>(Kind)];
			T.Release = R.HandOver.Released[0].ServerTime - Death;
			T.Removal = R.HandOver.TornDown[0].ServerTime - Death;
			TestTrue(FString::Printf(TEXT("%s: released at DeathServerTime + DeathFallSec (+%.4f s)"), Name, T.Release), T.Release >= ReleaseAfter - Epsilon && T.Release <= ReleaseAfter + Frame + Epsilon);
			TestTrue(FString::Printf(TEXT("%s: removed at DeathServerTime + 9.2 s (+%.4f s)"), Name, T.Removal), T.Removal >= RemovalAfter - Epsilon && T.Removal <= RemovalAfter + Frame + Epsilon);
		}
		TestTrue(FString::Printf(TEXT("%s: no queue fault"), Name), R.W.Support.Faults.GetRecords().IsEmpty());
	}
	for (int32 Index = 1; Index < 3; ++Index)
	{
		TestTrue(FString::Printf(TEXT("%s and NPC: release and removal times match (+- 1 server frame)"), KindName(static_cast<EKind>(Index))),
			FMath::Abs(Times[Index].Release - Times[0].Release) <= Frame + Epsilon && FMath::Abs(Times[Index].Removal - Times[0].Removal) <= Frame + Epsilon);
	}

	// The latch half: the kill decides the round in its own slot (team 1's only Active player), so the latch precedes both
	// due times: both items are Rejected_Latched, the body lies until Reset.
	{
		FRoundEncounterRig R(TEXT("BodySequenceLatchWorld"), true, true);
		R.W.ToActive(RoundTestFixtures::Layout2v2.Team0, RoundTestFixtures::Layout2v2.Team1);
		R.SetInformant(4);
		APawn* A = R.Spawn(FVector::ZeroVector, 1);
		APawn* V = R.Spawn(FVector(150.0, 0.0, 0.0), 3);
		R.Drain({ R.AttackAt(A, V) });
		TestTrue(TEXT("latch: the kill latched the round"), R.W.Rig.Core->GetLatch().IsTerminated());
		const double Death = R.Core->FindBody(V) != nullptr ? R.Core->FindBody(V)->DeathServerTime : 0.0;
		while (R.W.Rig.Clock.GetServerTimeSeconds() < Death + RemovalAfter + 0.25)
		{
			R.Tick();
		}
		const FEncounterBody* Body = R.Core->FindBody(V);
		TestTrue(TEXT("latch: both body items were Rejected_Latched"), Body != nullptr && Body->LatchedItems == 2);
		TestTrue(TEXT("latch: never released, never observed, never torn down"), R.HandOver.Released.IsEmpty() && R.RemovalObserver.Committed.IsEmpty()
			&& R.HandOver.TornDown.IsEmpty() && Body != nullptr && !Body->IsReleased() && !Body->IsRemoved());
		R.Core->Reset();
		TestTrue(TEXT("latch: Reset removes the body"), R.HandOver.TornDown.Num() == 1 && R.HandOver.TornDown[0].Body == V && R.Core->GetBodies().IsEmpty());
	}

	// Edge: a missing removal observer is a fault; the body is still removed.
	{
		FRig R(TEXT("BodySequenceNoObserverWorld"), nullptr, false, true);
		FAttackResolverDeps Deps = R.Core->GetDeps();
		Deps.BodyObserver = nullptr;
		FAttackResolverCore Bare(Deps, R.Params);
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* V = R.Spawn(FVector(150.0, 0.0, 0.0));
		Bare.Resolve(R.AttackItem(A, R.Facts(V).Location, V), R.NextKey());
		for (const FFakeQueue::FScheduled& Entry : TArray<FFakeQueue::FScheduled>(R.Queue.Scheduled))
		{
			if (Entry.Item.Type == EQueueItemType::TimerFired)
			{
				Bare.Resolve(Entry.Item, R.NextKey());
			}
		}
		TestTrue(TEXT("no observer: one DeadRegistration fault (NoBodyRemovalObserver)"), R.Faults.GetRecords().ContainsByPredicate([](const FStealthFaultRecord& F)
		{
			return F.Fault == EStealthFault::DeadRegistration && F.Detail == EncounterFaultDetail::NoBodyRemovalObserver;
		}));
		TestTrue(TEXT("no observer: the body is still released and removed"), R.HandOver.CountReleased(V) == 1 && R.HandOver.CountTornDown(V) == 1);
	}
	return true;
}

#endif
