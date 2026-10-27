#pragma once

// The strike resolver on the REAL round queue with the REAL Round core (stories asn-007, 009, 010): the Round same-tick
// win rig (FRoundWinQueueRig: FRoundMatchCore over the recording fakes, driven to Active, its FResolutionQueueCore on the
// rig's fake clock) with FAttackResolverCore registered for PawnCommand in place of the strike stub. The Roster seam is
// FFakeRoster bound to the rig's participants: its RequestElimination writes the state and forwards the transition to
// Round's F2 check inline (as the Roster event does in production), so Round may latch inside the Attack's slot. The
// HoldTimer type stays the rig's completion stub (the final-completion item); keys are forced by one mailbox's FIFO.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/EncounterService.h"
#include "Tests/Assassination/EncounterTestRig.h"
#include "Tests/Round/StealthRoundWinQueueRig.h"

namespace StealthEncounterTest
{
	/** Counts CounterPose writes (the CounterPose applier is story asn-013's; registered so no write lacks one). */
	class FCountingCounterApplier final : public IStagedWriteApplier
	{
	public:
		int32 Applied = 0;
		virtual void ApplyStagedWrite(const FStagedWrite& Write) override { ++Applied; }
	};

	struct FRoundEncounterRig
	{
		FRoundWinQueueRig W;
		StealthAssassinationTest::FScopedWorld World;
		FFakePawns Pawns;
		FFakeRoster Roster;
		FFakeDefence Defence;
		FFakeSeeds Seeds;
		/** Every pawn knowable to every attacker (the IVP stand-in); with no query the core is default-deny. */
		FFakeVisibility Visibility;
		FEncounterQueueAdapter QueueAdapter;
		FCountingCounterApplier Counters;
		/** Production writes (bProductionWrites): the recorded FActionState / death writes, applied by the core's applier. */
		FRecordingPawnState PawnState;
		/** Story asn-019 (bWithBodySequence): the body hand-over and removal observer, the core on both body timer kinds. */
		FRecordingHandOver HandOver;
		FRecordingRemovalObserver RemovalObserver;
		FScriptedTally Tally;
		FRecordingReplies Replies;
		FRecordingOwnerCooldown OwnerCooldown;
		FCallLog Calls;
		TArray<FKill> Kills;
		TUniquePtr<FAttackResolverCore> Core;
		double SpawnX = 0.0;
		uint64 NextSerial = 1;

		/**
		 * bProductionWrites (stories asn-012, 013): the core writes FActionState and applies its own DeathWrite and CounterPose
		 * (the rig's counting appliers are replaced by FAttackResolverCore::GetStagedWriteApplier()), recorded in PawnState.
		 */
		explicit FRoundEncounterRig(const TCHAR* Name, bool bProductionWrites = false, bool bWithBodySequence = false)
			: World(Name)
			, QueueAdapter(W.Queue.Get())
		{
			PawnState.Clock = &W.Rig.Clock;
			HandOver.Clock = &W.Rig.Clock;
			Roster.Calls = &Calls;
			Roster.OnTransition = [this](const APawn* Pawn, ERosterReason Reason)
			{
				if (const FFakeRoster::FMember* Member = Roster.Members.Find(Pawn))
				{
					W.Kill(Member->Participant, Reason);
				}
			};
			FAttackResolverDeps Deps;
			Deps.Clock = &W.Rig.Clock;
			Deps.Queue = &QueueAdapter;
			Deps.Roster = &Roster;
			Deps.Pawns = &Pawns;
			Deps.Defence = &Defence;
			Deps.SubSeeds = &Seeds;
			Deps.Visibility = &Visibility;
			Deps.Log = W.Rig.Core->GetRoundLogWriter();
			Deps.PawnState = bProductionWrites ? &PawnState : nullptr;
			Deps.Tally = &Tally;
			Deps.Replies = &Replies;
			Deps.OwnerCooldown = &OwnerCooldown;
			Deps.BodyHandOver = bWithBodySequence ? &HandOver : nullptr;
			Deps.BodyObserver = bWithBodySequence ? &RemovalObserver : nullptr;
			Core = MakeUnique<FAttackResolverCore>(Deps, FEncounterParams::Defaults());
			Core->OnPawnKilled.AddLambda([this](APawn* Victim, APawn* Killer, EVictimKind Kind, EKillCause Cause)
			{
				Calls.Add(TEXT("OnPawnKilled"));
				Kills.Add({ Victim, Killer, Kind, Cause, Core->GetScheduledDeathServerTime(Victim) });
			});
			W.Queue->UnregisterResolver(EQueueItemType::PawnCommand);
			W.Queue->RegisterResolver(EQueueItemType::PawnCommand, Core.Get());
			if (bWithBodySequence)
			{
				W.Queue->RegisterTimerResolver(ETimerKind::BodyRelease, Core.Get());
				W.Queue->RegisterTimerResolver(ETimerKind::BodyRemoval, Core.Get());
			}
			if (bProductionWrites)
			{
				W.Queue->UnregisterStagedWriteApplier(EStagedWriteType::DeathWrite);
				W.Queue->RegisterStagedWriteApplier(EStagedWriteType::DeathWrite, Core->GetStagedWriteApplier());
				W.Queue->RegisterStagedWriteApplier(EStagedWriteType::CounterPose, Core->GetStagedWriteApplier());
			}
			else
			{
				W.Queue->RegisterStagedWriteApplier(EStagedWriteType::CounterPose, &Counters);
			}
		}

		/** Drains one empty tick (time advances one rig frame; due timers and the staging pass run). */
		void Tick() { W.DrainOneTick({}); }

		/** A pawn at Location bound to Participant (its team from the rig's layout), or an NPC for 0. */
		APawn* Spawn(const FVector& Location, uint32 Participant = 0, double YawDeg = 0.0)
		{
			SpawnX += 500.0;
			APawn* Pawn = World.SpawnPawn(SpawnX);
			FEncounterPawnFacts Facts;
			Facts.Pawn = Pawn;
			Facts.Location = Location;
			Facts.FacingYawDeg = YawDeg;
			Facts.SpawnSerial = NextSerial++;
			Pawns.Pawns.Add(Facts);
			if (Participant != 0)
			{
				const FRoundRosterMember* Member = W.Rig.Fakes.Roster.Members.FindByPredicate([Participant](const FRoundRosterMember& M) { return M.Participant.Value == Participant; });
				FFakeRoster::FMember Bound;
				Bound.Participant = Participant;
				Bound.Team = Member != nullptr && Member->Team == ERosterTeam::Team1 ? 1 : 0;
				Bound.State = Member != nullptr ? Member->State : EParticipantState::Active;
				Roster.Members.Add(Pawn, Bound);
			}
			return Pawn;
		}

		/** Marks a rostered participant Informant in both the Round fake and the Assassination fake (no event). */
		void SetInformant(uint32 Participant)
		{
			W.Rig.Fakes.Roster.SetState(FParticipantId(Participant), EParticipantState::Informant);
			for (TPair<const APawn*, FFakeRoster::FMember>& Pair : Roster.Members)
			{
				if (Pair.Value.Participant == Participant)
				{
					Pair.Value.State = EParticipantState::Informant;
				}
			}
		}

		FQueueItem Command(APawn* Pawn, ECommandKind Kind, const FVector& WorldPoint = FVector::ZeroVector, APawn* Ref = nullptr)
		{
			FCommandPayload P;
			P.Pawn.Pawn = Pawn;
			P.Kind = static_cast<uint8>(Kind);
			P.WorldPoint = WorldPoint;
			if (Ref != nullptr)
			{
				P.Target = FSubjectRef::ForPawn(FPawnHandle{ Ref });
			}
			return FQueueItem::Make(EQueueItemType::PawnCommand, W.Support.Epoch.Current, FMailboxId::ServerLocal(), P);
		}

		FQueueItem AttackAt(APawn* Attacker, APawn* Ref)
		{
			const FEncounterPawnFacts* Facts = Pawns.Find(Ref);
			return Command(Attacker, ECommandKind::Attack, Facts != nullptr ? Facts->Location : FVector::ZeroVector, Ref);
		}

		/** A filler item (an unmatched completion tag: the rig's stub voids it) to place keys at fixed Seqs. */
		FQueueItem Filler() { return W.Support.MakeItem(EQueueItemType::HoldTimer, 999999); }

		void Drain(TConstArrayView<FQueueItem> Items) { W.DrainOneTick(Items); }

		/** KillResolved rows Assassination wrote with Key (Round's Event Log writer forwards to the rig's recorder). */
		int32 CountLoggedKill(FQueueKey Key) const
		{
			return W.Rig.EventLog.Entries.FilterByPredicate([Key](const FRoundLogEntry& E) { return E.Type == ERoundLogType::KillResolved && E.Key == Key; }).Num();
		}
		int32 CountLoggedReversal(FQueueKey Key) const
		{
			return W.Rig.EventLog.Entries.FilterByPredicate([Key](const FRoundLogEntry& E) { return E.Type == ERoundLogType::Reversal && E.Key == Key; }).Num();
		}

		/** The slot record of the Index-th PawnCommand item of the last drain. */
		const FQueueSlotRecord* CommandRecord(int32 Index) const
		{
			int32 Seen = 0;
			for (const FQueueSlotRecord& Record : W.Queue->GetLastDrainRecords())
			{
				if (Record.Type == EQueueItemType::PawnCommand && Seen++ == Index)
				{
					return &Record;
				}
			}
			return nullptr;
		}
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS
