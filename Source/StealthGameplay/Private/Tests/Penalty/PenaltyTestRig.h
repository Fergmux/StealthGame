#pragma once

#include "CoreMinimal.h"

#if WITH_STEALTH_TEST_SEAMS

#include "Server/EventLog/StealthRoundLogWriter.h"
#include "View/StealthTuningPublicSubstructs.h"
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

	/**
	 * IPenaltyBodyGateway recording every call in order with its arguments and the slot it ran in (stories nkp-005,
	 * nkp-008). RequestStrikeLimit answers TransitionAnswer and, when Applied, makes the fake Roster state Informant (the
	 * Roster's own write); RetireBinding answers RetireAnswer.
	 */
	struct FRecordingBodies final : IPenaltyBodyGateway
	{
		struct FCall
		{
			FString Name;
			FParticipantId Participant;
			FPenaltyPawnKey Body;
			uint32 Epoch = 0;
			double Time = 0.0;
			bool bExit = false;
			FQueueKey SlotKey;
			bool bInSlot = false;
		};
		TArray<FCall> Calls;
		const IPenaltySlotContext* Slot = nullptr;
		FFakeRoster* Roster = nullptr;
		ERosterResult TransitionAnswer = ERosterResult::Applied;
		ERosterResult RetireAnswer = ERosterResult::Applied;
		/** Called inside RequestStrikeLimit after the state write (a test drives Round's elimination check from it). */
		TFunction<void(FParticipantId)> OnStrikeLimit;

		FCall& Record(const TCHAR* Name)
		{
			FCall& Call = Calls.AddDefaulted_GetRef();
			Call.Name = Name;
			Call.bInSlot = Slot != nullptr && Slot->IsInSlot();
			Call.SlotKey = Slot != nullptr ? Slot->CurrentKey() : FQueueKey();
			return Call;
		}
		virtual ERosterResult RequestStrikeLimit(FParticipantId Participant, uint32 ExpectedRoundEpoch) override
		{
			FCall& Call = Record(TEXT("RequestTransition"));
			Call.Participant = Participant;
			Call.Epoch = ExpectedRoundEpoch;
			if (TransitionAnswer == ERosterResult::Applied)
			{
				if (Roster != nullptr)
				{
					Roster->States.Add(Participant, EParticipantState::Informant);
				}
				if (OnStrikeLimit)
				{
					OnStrikeLimit(Participant);
				}
			}
			return TransitionAnswer;
		}
		virtual void ReleaseCard(FPenaltyPawnKey Body) override { Record(TEXT("ReleaseCard")).Body = Body; }
		virtual void AdoptReleasedPawn(FPenaltyPawnKey Body, double RestrictedUntilServerTime, bool bExitAtNextScheduledSlot) override
		{
			FCall& Call = Record(TEXT("AdoptReleasedPawn"));
			Call.Body = Body;
			Call.Time = RestrictedUntilServerTime;
			Call.bExit = bExitAtNextScheduledSlot;
		}
		virtual void RequestHandOverToAi(FPenaltyPawnKey Body, uint32 ExpectedRoundEpoch, double PossessNotBeforeServerTime) override
		{
			FCall& Call = Record(TEXT("RequestHandOver"));
			Call.Body = Body;
			Call.Epoch = ExpectedRoundEpoch;
			Call.Time = PossessNotBeforeServerTime;
		}
		virtual ERosterResult RetireBinding(FPenaltyPawnKey Body, double RetireAtServerTime) override
		{
			FCall& Call = Record(TEXT("RetireBinding"));
			Call.Body = Body;
			Call.Time = RetireAtServerTime;
			return RetireAnswer;
		}

		/** The recorded call names, in order, joined by spaces. */
		FString Sequence() const
		{
			FString Out;
			for (const FCall& Call : Calls)
			{
				Out += Out.IsEmpty() ? Call.Name : TEXT(" ") + Call.Name;
			}
			return Out;
		}
		int32 Count(const TCHAR* Name) const
		{
			return Calls.FilterByPredicate([Name](const FCall& Call) { return Call.Name == Name; }).Num();
		}
	};

	/** IPenaltyRespawnScheduler recording every ScheduleRespawn (story nkp-009's seam). */
	struct FRecordingRespawns final : IPenaltyRespawnScheduler
	{
		struct FCall
		{
			FParticipantId Participant;
			ERespawnCause Cause = ERespawnCause::Assassinated;
			double LostServerTime = 0.0;
		};
		TArray<FCall> Calls;

		virtual void ScheduleRespawn(FParticipantId Participant, ERespawnCause Cause, double LostServerTime) override
		{
			Calls.Add({ Participant, Cause, LostServerTime });
		}
	};

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
		FRecordingBodies Bodies;
		FRecordingRespawns Respawns;
		TUniquePtr<FPenaltyCore> Core;
		uint32 NextSeq = 1;

		/** bWithGateways (stories nkp-005, nkp-008): the core calls the recording body gateway and respawn scheduler. */
		explicit FRig(TFunction<void(FFakeTuning&)> ConfigureTuning = nullptr, bool bWithGateways = false)
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
			Bodies.Slot = &Slot;
			Bodies.Roster = &Roster;
			if (bWithGateways)
			{
				Deps.Bodies = &Bodies;
				Deps.Respawns = &Respawns;
			}
			Core = MakeUnique<FPenaltyCore>(Deps);
		}

		/** The lockout a strike leaves on its attacker in these tests: AttackAttemptLockoutSec after the slot (schema default). */
		static double LockoutSec() { return static_cast<double>(FAssassinationTuning_Public().AttackAttemptLockoutSec); }

		/**
		 * An ungraced NPC kill by Attacker whose body is AttackerBody, with the killer's lockout end as Assassination writes
		 * it (slot time + AttackAttemptLockoutSec). Returns the tally; OutSlotKey / OutSlotTime are the kill's slot.
		 */
		FStrikeTally NpcKillWithBody(FParticipantId Attacker, FPenaltyPawnKey AttackerBody, uint64 Victim, EPenaltyKillOutcome* OutOutcome = nullptr,
			FQueueKey* OutSlotKey = nullptr, double* OutSlotTime = nullptr)
		{
			Slot.NextSlot();
			Clock.Advance(0.25, 1);
			FPenaltyKill Kill{ Pawn(Victim), Attacker, EVictimKind::Npc, EKillCause::Strike };
			Kill.KillerPawn = AttackerBody;
			Kill.KillerLockoutEndServerTime = Clock.GetServerTimeSeconds() + LockoutSec();
			const EPenaltyKillOutcome Outcome = Core->OnPawnKilled(Kill);
			if (OutOutcome != nullptr)
			{
				*OutOutcome = Outcome;
			}
			if (OutSlotKey != nullptr)
			{
				*OutSlotKey = Slot.CurrentKey();
			}
			if (OutSlotTime != nullptr)
			{
				*OutSlotTime = Clock.GetServerTimeSeconds();
			}
			return Core->GetTallyForResult(Attacker, EStrikeResult::WrongTarget, Seq(NextSeq++), AttackerBody);
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
