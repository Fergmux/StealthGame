#pragma once

#include "CoreMinimal.h"

#if WITH_STEALTH_TEST_SEAMS

#include "Server/EventLog/StealthRoundLogWriter.h"
#include "Server/StealthServerTuning.h"
#include "Server/Testing/StealthFakeClock.h"
#include "Server/Testing/StealthFaultSinkTestSupport.h"
#include "StealthPenaltyCore.h"

/**
 * Shared fakes of the StealthGame.Penalty.* Logic tests (stories nkp-001 to nkp-004; GDD "Test seams": a fake server
 * clock, a stub queue slot, a fake Roster and an in-memory Event Log sink, all injected, no singleton). FPenaltyCore runs
 * on them with no UWorld, no UObject and no engine singleton.
 */
namespace StealthPenaltyTest
{
	/** Fixture constants (never inline magic numbers in the tests). */
	inline constexpr uint64 kPenaltyTestSeed = 0x5EEDC0DE4E4B5001ull;
	inline constexpr FParticipantId P{ 1 };
	inline constexpr FParticipantId Q{ 2 };
	inline constexpr uint32 FixtureEpoch = 7;
	/** The fixture limit L of the default preset (the schema default; never a literal in Penalty code). */
	inline int32 DefaultLimit() { return FPenaltyParams::FromTuning(FStealthServerTuning()).NpcKillStrikeLimit; }

	/** IPenaltyRosterQuery: a state per participant and the epoch. */
	struct FFakeRoster final : IPenaltyRosterQuery
	{
		TMap<FParticipantId, EParticipantState> States;
		uint32 Epoch = FixtureEpoch;

		virtual TOptional<EParticipantState> GetState(FParticipantId Participant) const override
		{
			const EParticipantState* State = States.Find(Participant);
			return State != nullptr ? TOptional<EParticipantState>(*State) : TOptional<EParticipantState>();
		}
		virtual uint32 GetRoundEpoch() const override { return Epoch; }
	};

	/** IPenaltyTuningSource: the live values, with a read counter (zero reads mid-round, AC2 of nkp-001). */
	struct FFakeTuning final : IPenaltyTuningSource
	{
		FPenaltyParams Live = FPenaltyParams::FromTuning(FStealthServerTuning());
		mutable int32 Reads = 0;

		virtual FPenaltyParams ReadPenaltyParams() const override
		{
			++Reads;
			return Live;
		}
	};

	/** IPenaltySlotContext: the stub queue's slot (in a slot by default; NextSlot moves to a new key). */
	struct FFakeSlot final : IPenaltySlotContext
	{
		bool bInSlot = true;
		FQueueKey Key = FQueueKey(1, 1);

		virtual bool IsInSlot() const override { return bInSlot; }
		virtual FQueueKey CurrentKey() const override { return bInSlot ? Key : FQueueKey(); }
		void NextSlot() { Key = FQueueKey(Key.ServerTickIndex + 1, Key.Seq + 1); }
	};

	/** IPenaltyOwnerViewFeed recording every SetOwnerTally call. */
	struct FRecordingOwnerView final : IPenaltyOwnerViewFeed
	{
		struct FCall
		{
			FParticipantId Participant;
			uint8 Tally = 0;
			FClientCommandSeq LastResultSeq;
			uint32 Epoch = 0;
		};
		TArray<FCall> Calls;

		virtual void SetOwnerTally(FParticipantId Participant, uint8 Tally, FClientCommandSeq LastResultSeq, uint32 ExpectedEpoch) override
		{
			Calls.Add({ Participant, Tally, LastResultSeq, ExpectedEpoch });
		}
		int32 CountFor(FParticipantId Participant) const
		{
			return Calls.FilterByPredicate([Participant](const FCall& C) { return C.Participant == Participant; }).Num();
		}
	};

	/** The in-memory Event Log sink: every appended entry with the slot context seen at the write. */
	struct FRecordingLog final : IRoundLogWriter
	{
		const IPenaltySlotContext* Slot = nullptr;
		TArray<FRoundLogEntry> Entries;
		TArray<bool> InSlotAtWrite;

		virtual void Append(const FRoundLogEntry& Entry) override
		{
			Entries.Add(Entry);
			InSlotAtWrite.Add(Slot != nullptr && Slot->IsInSlot());
		}
		TArray<FRoundLogEntry> OfType(ERoundLogType Type) const
		{
			return Entries.FilterByPredicate([Type](const FRoundLogEntry& E) { return E.Type == Type; });
		}
	};

	/** A pawn key from a number (no UObject). */
	inline FPenaltyPawnKey Pawn(uint64 Id) { return FPenaltyPawnKey(Id); }

	/** One core on the fakes; the participants start Active. */
	struct FRig
	{
		FFakeStealthClock Clock{ 100.0, 1 };
		FFakeRoster Roster;
		FFakeTuning Tuning;
		FFakeSlot Slot;
		FRecordingOwnerView OwnerView;
		FRecordingLog Log;
		FStealthRecordingFaultSink Faults;
		TUniquePtr<FPenaltyCore> Core;
		uint32 NextSeq = 1;

		explicit FRig(TFunction<void(FFakeTuning&)> ConfigureTuning = nullptr)
		{
			if (ConfigureTuning)
			{
				ConfigureTuning(Tuning);
			}
			Roster.States.Add(P, EParticipantState::Active);
			Roster.States.Add(Q, EParticipantState::Active);
			Log.Slot = &Slot;
			FPenaltyCoreDeps Deps;
			Deps.Clock = &Clock;
			Deps.Roster = &Roster;
			Deps.Tuning = &Tuning;
			Deps.Slot = &Slot;
			Deps.Log = &Log;
			Deps.OwnerView = &OwnerView;
			Deps.Faults = &Faults;
			Core = MakeUnique<FPenaltyCore>(Deps);
		}

		static FClientCommandSeq Seq(uint32 Value)
		{
			FClientCommandSeq Out;
			Out.Seq = Value;
			return Out;
		}

		/**
		 * One executed Attack by Attacker in its own slot: OnPawnKilled for a kill or a reversal (Kind / Cause), then the
		 * tally query with Result, exactly as the resolver orders them (ADR-0018 section 2.1 steps 6 and 8).
		 */
		FStrikeTally Attack(FParticipantId Attacker, TOptional<FPenaltyKill> Kill, EStrikeResult Result, EPenaltyKillOutcome* OutOutcome = nullptr)
		{
			Slot.NextSlot();
			Clock.Advance(0.25, 1);
			if (Kill.IsSet())
			{
				const EPenaltyKillOutcome Outcome = Core->OnPawnKilled(Kill.GetValue());
				if (OutOutcome != nullptr)
				{
					*OutOutcome = Outcome;
				}
			}
			return Core->GetTallyForResult(Attacker, Result, Seq(NextSeq++));
		}

		/** An ungraced NPC kill by Attacker (WrongTarget). */
		FStrikeTally NpcKill(FParticipantId Attacker, uint64 Victim = 900)
		{
			return Attack(Attacker, FPenaltyKill{ Pawn(Victim), Attacker, EVictimKind::Npc, EKillCause::Strike }, EStrikeResult::WrongTarget);
		}

		/** An informant kill by Attacker (WrongTarget, no strike). */
		FStrikeTally InformantKill(FParticipantId Attacker, uint64 Victim = 901)
		{
			return Attack(Attacker, FPenaltyKill{ Pawn(Victim), Attacker, EVictimKind::Informant, EKillCause::Strike }, EStrikeResult::WrongTarget);
		}
	};
}

#endif // WITH_STEALTH_TEST_SEAMS
