// Story asn-011 (production/epics/assassination-defence-reversal/story-011-npc-brace-parity-never-strike-planted.md), AC1 /
// AC-ASN-26 NpcBraceSamePath (ASN-R7, Crowd T4). Headless integration (decision logged: the PIE harness adds nothing while
// the slot is not yet wired into play): real AStealthPawns, their real UPawnCommandComponent entry points feeding a real
// FResolutionQueueCore (FStealthQueueTestRig) through an IPawnCommandQueue adapter, the strike resolver registered for
// PawnCommand and HoldTimer. Run 1: a player defender braces through its owner path (participant mailbox, reply sink 5).
// Run 2: an NPC defender braces through the crowd's FCrowdCommandSink::PrepareBegin (server-local mailbox, null sink).
// In both a rival player strikes the braced defender 1 s later. The resolution traces are equal (outcome, transition,
// OnPawnKilled arguments with Killer = the defender, the defender's role and counter end, staged writes and their times,
// the stance writes, the Reversal row, the cooldown end = activation + 60 s); the only difference is the FDefenceResult
// (none for the NPC's null sink); nothing is charged to anyone. A second NPC brace during the cooldown is rejected.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Algo/Compare.h"
#include "Server/CrowdSink.h"
#include "Server/EncounterService.h"
#include "Server/Testing/StealthQueueTestRig.h"
#include "Server/Core/StealthPawnCommandComponent.h"
#include "Tests/Assassination/EncounterTestRig.h"

namespace NpcBraceSamePathTestData
{
	using namespace StealthEncounterTest;

	/** The pawn command cores submit into the test queue (the production adapter is the integration story's). */
	class FQueueSubmitter final : public IPawnCommandQueue
	{
	public:
		FResolutionQueueCore* Queue = nullptr;
		virtual void Submit(FMailboxId Mailbox, const FQueueItem& Item) override { Queue->Submit(Mailbox, Item); }
		virtual void ScheduleAt(double Due, const FQueueItem& Item) override { Queue->ScheduleAt(Due, Item); }
	};

	class FFixedEpochs final : public ICrowdEpochSource
	{
	public:
		virtual uint32 GetControlEpoch(const AStealthPawn*) const override { return 1; }
	};

	/** Everything compared between the two runs. */
	struct FTrace
	{
		EAttackOutcome Outcome = EAttackOutcome::Whiff;
		int32 Transitions = 0;
		ERosterReason Reason = ERosterReason::None;
		int32 Kills = 0;
		bool bKillArgs = false;
		EStrikeRole DefenderRole = EStrikeRole::Ready;
		double CounterEndAfterSlot = 0.0;
		TArray<TPair<EStagedWriteType, double>> Staged;
		int32 StanceWrites = 0;
		int32 ReversalRows = 0;
		double CooldownAfterActivation = 0.0;
		bool bDefenceResult = false;
		bool bDefenderCharged = false;

		bool SameAsExceptDefenceResult(const FTrace& O) const
		{
			return Outcome == O.Outcome && Transitions == O.Transitions && Reason == O.Reason && Kills == O.Kills && bKillArgs == O.bKillArgs
				&& DefenderRole == O.DefenderRole && FMath::IsNearlyEqual(CounterEndAfterSlot, O.CounterEndAfterSlot, 1e-9) && Staged.Num() == O.Staged.Num()
				&& StanceWrites == O.StanceWrites && ReversalRows == O.ReversalRows && CooldownAfterActivation == O.CooldownAfterActivation
				&& bDefenderCharged == O.bDefenderCharged
				&& Algo::Compare(Staged, O.Staged, [](const TPair<EStagedWriteType, double>& A, const TPair<EStagedWriteType, double>& B) { return A.Key == B.Key && FMath::IsNearlyEqual(A.Value, B.Value, 1e-9); });
		}
	};

	struct FHarness
	{
		StealthAssassinationTest::FScopedWorld World{ TEXT("NpcBraceSamePathWorld") };
		FStealthQueueTestRig Q;
		FQueueSubmitter Submitter;
		FFakePawns Pawns;
		FFakeRoster Roster;
		FFakeDefence Defence;
		FRecordingStance Stance;
		FFakeVisibility Visibility;
		FEncounterQueueAdapter Adapter{ Q.Queue.Get() };
		TUniquePtr<FAttackResolverCore> Core;
		TArray<FKill> Kills;
		double SpawnX = 0.0;

		FHarness()
		{
			Submitter.Queue = Q.Queue.Get();
			FAttackResolverDeps Deps;
			Deps.Clock = &Q.Clock;
			Deps.Queue = &Adapter;
			Deps.Roster = &Roster;
			Deps.Pawns = &Pawns;
			Deps.Defence = &Defence;
			Deps.Stance = &Stance;
			Deps.Visibility = &Visibility;
			Deps.Log = &Q.Log;
			Core = MakeUnique<FAttackResolverCore>(Deps, FEncounterParams::Defaults());
			Core->OnPawnKilled.AddLambda([this](APawn* V, APawn* K, EVictimKind Kind, EKillCause Cause) { Kills.Add({ V, K, Kind, Cause }); });
			Q.Queue->RegisterResolver(EQueueItemType::PawnCommand, Core.Get());
			Q.Queue->RegisterResolver(EQueueItemType::HoldTimer, Core.Get());
		}

		AStealthPawn* Spawn(const FVector& Location, int32 Team, uint32 Participant)
		{
			SpawnX += 500.0;
			AStealthPawn* Pawn = World.SpawnPawn(SpawnX);
			FPawnCommandCoreDeps Deps;
			Deps.Clock = &Q.Clock;
			Deps.Queue = &Submitter;
			Deps.Epoch = &Q.Epoch;
			Pawn->GetPawnCommand()->InitializeCore(Deps);
			FEncounterPawnFacts Facts;
			Facts.Pawn = Pawn;
			Facts.Location = Location;
			Facts.SpawnSerial = Pawns.Pawns.Num() + 1;
			Pawns.Pawns.Add(Facts);
			if (Team != NoTeam)
			{
				Roster.Members.Add(Pawn, { Participant, Team, EParticipantState::Active });
			}
			return Pawn;
		}

		static FCommandOrigin Owner(uint32 Participant, uint16 Sink)
		{
			FCommandOrigin Origin;
			Origin.Mailbox = FMailboxId::ForParticipant(FParticipantId(Participant));
			Origin.Sink.Index = Sink;
			Origin.Sink.Generation = 1;
			return Origin;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationNpcBraceSamePathTest, "StealthGame.Assassination.NpcBraceSamePath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationNpcBraceSamePathTest::RunTest(const FString& Parameters)
{
	using namespace NpcBraceSamePathTestData;
	FTrace Traces[2];
	for (int32 Run = 0; Run < 2; ++Run)
	{
		const bool bNpc = Run == 1;
		const TCHAR* Name = bNpc ? TEXT("NPC defender (crowd sink)") : TEXT("player defender (owner path)");
		FHarness H;
		AStealthPawn* A = H.Spawn(FVector::ZeroVector, 0, 1);
		AStealthPawn* D = bNpc ? H.Spawn(FVector(150.0, 0.0, 0.0), NoTeam, 0) : H.Spawn(FVector(150.0, 0.0, 0.0), 1, 2);
		FFixedEpochs Epochs;
		FCrowdCommandSink Sink(D, 1, &Epochs);

		// The brace enters through the IIF Prepare entry point: the crowd sink for the NPC, the owner origin for the player.
		if (bNpc)
		{
			TestTrue(TEXT("the crowd sink passes PrepareBegin to the command component"), Sink.PrepareBegin());
		}
		else
		{
			FScopedCommandOrigin Scope(*D->GetPawnCommand(), FHarness::Owner(2, 5), FClientCommandSeq(), 0);
			D->GetPawnCommand()->PrepareBegin();
		}
		H.Q.Tick();
		TestTrue(FString::Printf(TEXT("%s: the Prepare item executed in its slot (Bracing)"), Name), H.Core->GetRole(D) == EStrikeRole::Bracing);
		const double Activation = H.Defence.Find(D)->GetLastPrepareActivation().GetValue();
		for (int32 Frame = 0; Frame < 60; ++Frame)
		{
			H.Q.Tick();
		}

		// A rival player's Attack, through its owner path.
		{
			FScopedCommandOrigin Scope(*A->GetPawnCommand(), FHarness::Owner(1, 7), FClientCommandSeq(), 0);
			FAttackPayload Payload;
			Payload.WorldPoint = FVector(150.0, 0.0, 0.0);
			Payload.PawnRef = D;
			A->GetPawnCommand()->Attack(Payload);
		}
		H.Q.Tick();
		const double Slot = H.Q.Clock.GetServerTimeSeconds();
		TestEqual(FString::Printf(TEXT("%s: one strike resolved"), Name), H.Core->GetResolutions().Num(), 1);
		if (H.Core->GetResolutions().Num() != 1)
		{
			continue;
		}
		const FAttackResolution& Res = H.Core->GetResolutions()[0];
		FTrace& T = Traces[Run];
		T.Outcome = Res.Outcome;
		T.Transitions = H.Roster.Transitions.Num();
		T.Reason = T.Transitions > 0 ? H.Roster.Transitions[0].Reason : ERosterReason::None;
		T.Kills = H.Kills.Num();
		T.bKillArgs = H.Kills.Num() == 1 && H.Kills[0].Victim == A && H.Kills[0].Killer == D && H.Kills[0].Kind == EVictimKind::Player && H.Kills[0].Cause == EKillCause::Reversal;
		T.DefenderRole = H.Core->GetRole(D);
		T.CounterEndAfterSlot = H.Core->GetRoles().GetRoleEnd(D) - Slot;
		for (const FStagedWrite& Write : H.Q.Queue->GetPendingStagedWrites())
		{
			T.Staged.Add({ Write.Type, Write.ServerTime - Slot });
		}
		T.StanceWrites = H.Stance.Writes.Num();
		T.ReversalRows = H.Q.Log.CountOfType(ERoundLogType::Reversal);
		T.CooldownAfterActivation = H.Defence.Find(D)->GetCooldownEndServerTime(60.0).GetValue() - Activation;
		T.bDefenceResult = Res.HasDefenceResult();
		T.bDefenderCharged = H.Roster.Transitions.ContainsByPredicate([D](const FFakeRoster::FTransition& Tr) { return Tr.Pawn == D; }) || H.Core->IsDead(D)
			|| H.Defence.Find(D)->GetLastPrepareActivation().GetValue() != Activation;
		TestTrue(FString::Printf(TEXT("%s: a reversal eliminating the attacker"), Name), T.Outcome == EAttackOutcome::Reversal && T.Reason == ERosterReason::ReversedAgainst && T.bKillArgs);
		TestFalse(FString::Printf(TEXT("%s: nothing charged to the defender"), Name), T.bDefenderCharged);
		TestEqual(FString::Printf(TEXT("%s: the 60 s cooldown from activation"), Name), T.CooldownAfterActivation, 60.0);

		if (bNpc)
		{
			// A second NPC brace during the cooldown is rejected (the same stamp component as a player's).
			for (int32 Frame = 0; Frame < 180; ++Frame)
			{
				H.Q.Tick();
			}
			Sink.PrepareBegin();
			H.Q.Tick();
			TestTrue(TEXT("a second NPC brace inside the cooldown replies Cooldown"), H.Core->GetReplies().Last().Kind == ECommandKind::PrepareBegin
				&& !H.Core->GetReplies().Last().bExecuted && H.Core->GetReplies().Last().Code == ECommandRejectCode::Cooldown);
		}
	}
	TestTrue(TEXT("the NPC trace equals the player trace (outcome, transition, OnPawnKilled, roles, writes, rows, cooldown)"), Traces[1].SameAsExceptDefenceResult(Traces[0]));
	TestTrue(TEXT("the only difference: FDefenceResult for the bound player, none for the NPC's null sink"), Traces[0].bDefenceResult && !Traces[1].bDefenceResult);
	return true;
}

#endif
