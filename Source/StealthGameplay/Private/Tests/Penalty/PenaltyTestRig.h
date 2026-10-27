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

	/** IPenaltyRosterQuery: a state per participant, the epoch, the connection, the team and the live-pawn flag. */
	struct FFakeRoster final : IPenaltyRosterQuery
	{
		TMap<FParticipantId, EParticipantState> States;
		uint32 Epoch = FixtureEpoch;
		/** Participants whose projection is Disconnected (Unresponsive is Connected in the Roster projection). */
		TSet<FParticipantId> Disconnected;
		TMap<FParticipantId, ERosterTeam> Teams;
		TSet<FParticipantId> LivePawns;

		virtual TOptional<EParticipantState> GetState(FParticipantId Participant) const override
		{
			const EParticipantState* State = States.Find(Participant);
			return State != nullptr ? TOptional<EParticipantState>(*State) : TOptional<EParticipantState>();
		}
		virtual uint32 GetRoundEpoch() const override { return Epoch; }
		virtual bool IsDisconnected(FParticipantId Participant) const override { return Disconnected.Contains(Participant); }
		virtual ERosterTeam GetTeam(FParticipantId Participant) const override
		{
			const ERosterTeam* Team = Teams.Find(Participant);
			return Team != nullptr ? *Team : ERosterTeam::None;
		}
		virtual bool HasLivePawn(FParticipantId Participant) const override { return LivePawns.Contains(Participant); }
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
		/** One SetOwnerRespawnDue call (story nkp-011). */
		struct FDueCall
		{
			FParticipantId Participant;
			double DueServerTime = 0.0;
			uint32 Epoch = 0;
		};
		TArray<FDueCall> DueCalls;
		virtual void SetOwnerRespawnDue(FParticipantId Participant, double DueServerTime, uint32 ExpectedEpoch) override
		{
			DueCalls.Add({ Participant, DueServerTime, ExpectedEpoch });
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

	/**
	 * IPenaltyRespawnTimers recording every scheduled InformantRespawn item and every cancellation (story nkp-009's seam;
	 * the stub queue of the Logic tests). Handles are 1, 2, 3 ... in schedule order; Pending holds the uncancelled ones.
	 */
	struct FRecordingRespawns final : IPenaltyRespawnTimers
	{
		struct FCall
		{
			FParticipantId Participant;
			ERespawnCause Cause = ERespawnCause::Assassinated;
			double DueServerTime = 0.0;
			uint32 RequestSeq = 0;
			uint32 RoundEpoch = 0;
			uint64 Handle = 0;
		};
		TArray<FCall> Calls;
		TArray<uint64> Cancelled;
		/** Items the test fired (FRig::Fire): no longer pending. */
		TArray<uint64> Fired;
		uint64 NextHandle = 1;

		virtual uint64 ScheduleRespawnItem(double DueServerTime, const FPenaltyRespawnItem& Item) override
		{
			const uint64 Handle = NextHandle++;
			Calls.Add({ Item.Participant, Item.Cause, DueServerTime, Item.RequestSeq, Item.RoundEpoch, Handle });
			return Handle;
		}
		virtual void CancelRespawnItem(uint64 Handle) override { Cancelled.Add(Handle); }

		/** The items scheduled and neither cancelled nor fired. */
		TArray<FCall> Pending() const
		{
			return Calls.FilterByPredicate([this](const FCall& C) { return !Cancelled.Contains(C.Handle) && !Fired.Contains(C.Handle); });
		}
		/** Participant's pending items. */
		TArray<FCall> PendingFor(FParticipantId Participant) const
		{
			return Pending().FilterByPredicate([Participant](const FCall& C) { return C.Participant == Participant; });
		}
		/** The items scheduled for Participant. */
		TArray<FCall> For(FParticipantId Participant) const
		{
			return Calls.FilterByPredicate([Participant](const FCall& C) { return C.Participant == Participant; });
		}
	};

	/** IPenaltyDueTimeSink recording every OnDueTimeChanged (story nkp-011's input). */
	struct FRecordingDueTimes final : IPenaltyDueTimeSink
	{
		struct FCall
		{
			FParticipantId Participant;
			TOptional<double> DueServerTime;
		};
		TArray<FCall> Calls;
		/** Optional consumer the call is forwarded to (story nkp-011: the carrier adapter under test). */
		TFunction<void(FParticipantId, TOptional<double>)> Forward;
		virtual void OnDueTimeChanged(FParticipantId Participant, TOptional<double> DueServerTime) override
		{
			Calls.Add({ Participant, DueServerTime });
			if (Forward)
			{
				Forward(Participant, DueServerTime);
			}
		}
	};

	/**
	 * IPenaltyRespawnGateway recording every call in order with the frame it ran in (story nkp-012's fakes). Each step
	 * answers from a FIFO script, then from its default: one eligible pawn per pick (PickPawn), every check passing.
	 * Frame is the test's frame counter (FRig::Frame), set by the caller before each drain.
	 */
	struct FFakeRespawnGateway final : IPenaltyRespawnGateway
	{
		struct FCall
		{
			FString Name;
			FParticipantId Participant;
			FPenaltyPawnKey Pawn;
			uint32 RequestSeq = 0;
			int32 Frame = 0;
			TArray<FPenaltyPawnKey> Exclude;
		};
		TArray<FCall> Calls;
		int32 Frame = 0;
		/** The default pick when Picks is empty (unset: None). */
		FPenaltyPawnKey PickPawn = FPenaltyPawnKey(700);
		TArray<FPenaltyRespawnPick> Picks;
		TArray<bool> Eligible;
		TArray<bool> Releases;
		TArray<EPenaltyTakeoverResult> Takeovers;
		TArray<TOptional<uint16>> Cards;
		/** Pawns the pre-check rejects (dead, bound) on top of the scripts. */
		TSet<FPenaltyPawnKey> Ineligible;
		/** Called inside TakeOverCrowdPawn on Applied (the Roster's bind: a test marks the pawn bound / the participant embodied). */
		TFunction<void(FParticipantId, FPenaltyPawnKey)> OnApplied;
		/** One pick per frame (ADR-0014's guard) when true: a second pick in the same Frame answers Deferred. */
		bool bOnePickPerFrame = true;
		int32 LastPickFrame = -1;

		template <typename T>
		static T Next(TArray<T>& Script, const T& Default)
		{
			if (Script.Num() == 0)
			{
				return Default;
			}
			const T Value = Script[0];
			Script.RemoveAt(0);
			return Value;
		}
		FCall& Record(const TCHAR* Name)
		{
			FCall& Call = Calls.AddDefaulted_GetRef();
			Call.Name = Name;
			Call.Frame = Frame;
			return Call;
		}
		virtual FPenaltyRespawnPick PickRespawnPawn(ERosterTeam Team, uint32 RequestSeq, TConstArrayView<FPenaltyPawnKey> Exclude) override
		{
			FCall& Call = Record(TEXT("Pick"));
			Call.RequestSeq = RequestSeq;
			Call.Exclude = TArray<FPenaltyPawnKey>(Exclude.GetData(), Exclude.Num());
			if (bOnePickPerFrame && LastPickFrame == Frame)
			{
				FPenaltyRespawnPick Deferred;
				Deferred.Status = EPenaltyPickStatus::Deferred;
				return Deferred;
			}
			LastPickFrame = Frame;
			FPenaltyRespawnPick Default;
			Default.Status = PickPawn.IsSet() && !Exclude.Contains(PickPawn) && !Ineligible.Contains(PickPawn) ? EPenaltyPickStatus::Picked : EPenaltyPickStatus::None;
			Default.Pawn = Default.Status == EPenaltyPickStatus::Picked ? PickPawn : FPenaltyPawnKey();
			const FPenaltyRespawnPick Pick = Next(Picks, Default);
			Call.Pawn = Pick.Pawn;
			return Pick;
		}
		virtual bool CheckTakeOverEligibility(FPenaltyPawnKey Pawn) const override
		{
			FFakeRespawnGateway* Self = const_cast<FFakeRespawnGateway*>(this);
			Self->Record(TEXT("PreCheck")).Pawn = Pawn;
			return Next(Self->Eligible, true) && !Ineligible.Contains(Pawn);
		}
		virtual bool TryReleaseNpc(FPenaltyPawnKey Pawn, uint32 ExpectedRoundEpoch) override
		{
			Record(TEXT("Release")).Pawn = Pawn;
			return Next(Releases, true);
		}
		virtual EPenaltyTakeoverResult TakeOverCrowdPawn(FPenaltyPawnKey Pawn, FParticipantId Participant, uint32 ExpectedRoundEpoch) override
		{
			FCall& Call = Record(TEXT("Takeover"));
			Call.Pawn = Pawn;
			Call.Participant = Participant;
			const EPenaltyTakeoverResult Result = Next(Takeovers, EPenaltyTakeoverResult::Applied);
			if (Result == EPenaltyTakeoverResult::Applied && OnApplied)
			{
				OnApplied(Participant, Pawn);
			}
			return Result;
		}
		virtual void AdoptReleasedPawn(FPenaltyPawnKey Pawn) override { Record(TEXT("Adopt")).Pawn = Pawn; }
		virtual TOptional<uint16> TakeNpcCard(FParticipantId Participant, FPenaltyPawnKey Pawn) override
		{
			FCall& Call = Record(TEXT("Card"));
			Call.Pawn = Pawn;
			Call.Participant = Participant;
			return Next(Cards, TOptional<uint16>(42));
		}
		virtual void RecordAppearance(FParticipantId Participant, uint16 AppearanceId) override { Record(TEXT("RecordAppearance")).Participant = Participant; }

		/** The call names in order, joined by spaces. */
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
		TArray<FCall> Named(const TCHAR* Name) const
		{
			return Calls.FilterByPredicate([Name](const FCall& Call) { return Call.Name == Name; });
		}
	};

	/** One core on the fakes; the participants start Active. */
	struct FRig
	{
		/** Starts at StartServerTime (100.0 unless a test needs earlier history), tick 1. */
		FFakeStealthClock Clock;
		FFakeRoster Roster;
		FFakeTuning Tuning;
		FFakeSlot Slot;
		FRecordingOwnerView OwnerView;
		FRecordingLog Log;
		FStealthRecordingFaultSink Faults;
		FRecordingBodies Bodies;
		FRecordingRespawns Respawns;
		FRecordingDueTimes DueTimes;
		FFakeRespawnGateway Gateway;
		TUniquePtr<FPenaltyCore> Core;
		uint32 NextSeq = 1;

		/**
		 * bWithGateways (stories nkp-005, nkp-008): the core calls the recording body gateway. The respawn timers (the stub
		 * queue) and the OnDueTimeChanged sink are always recorded; bWithRespawnGateway (story nkp-012) adds the commit's fake.
		 */
		explicit FRig(TFunction<void(FFakeTuning&)> ConfigureTuning = nullptr, bool bWithGateways = false, bool bWithRespawnGateway = false,
			double StartServerTime = 100.0)
			: Clock(StartServerTime, 1)
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
			}
			Deps.Timers = &Respawns;
			Deps.DueTimes = &DueTimes;
			if (bWithRespawnGateway)
			{
				Deps.Respawn = &Gateway;
			}
			Core = MakeUnique<FPenaltyCore>(Deps);
		}

		/** Sets the fake clock to an absolute server time (the F2 boundary tests use literal seconds). */
		void SetTime(double ServerTime)
		{
			Clock.AdvanceSeconds(FMath::Max(0.0, ServerTime - Clock.GetServerTimeSeconds()));
		}

		/**
		 * The stub queue resolves Participant's pending InformantRespawn item in its own slot: the clock moves to the item's
		 * due time (never back), the frame counter of the respawn gateway advances by one (unless bSameFrame: a second item
		 * of the same drain), and the core's resolver runs. Returns the resolver's answer (false: Voided, or no pending item).
		 */
		bool Fire(FParticipantId Participant, bool bSameFrame = false)
		{
			const TArray<FRecordingRespawns::FCall> Items = Respawns.PendingFor(Participant);
			if (Items.Num() == 0)
			{
				return false;
			}
			const FRecordingRespawns::FCall Item = Items.Last();
			Respawns.Fired.Add(Item.Handle);
			SetTime(Item.DueServerTime);
			if (!bSameFrame)
			{
				Clock.AdvanceTicks(1);
				++Gateway.Frame;
			}
			Slot.NextSlot();
			return Core->ResolveRespawnItem(Item.Participant, Item.RequestSeq, Item.RoundEpoch);
		}

		/** A Player victim P killed in its own slot (OnPawnKilled with its death write time; Round's transition follows). */
		void KillPlayer(FParticipantId Victim, FParticipantId Killer, double DeathServerTime, EKillCause Cause = EKillCause::Strike)
		{
			Slot.NextSlot();
			FPenaltyKill Kill{ Pawn(800 + Victim.Value), Killer, EVictimKind::Player, Cause };
			Kill.VictimParticipant = Victim;
			Kill.VictimDeathServerTime = DeathServerTime;
			Core->OnPawnKilled(Kill);
			Roster.States.Add(Victim, EParticipantState::Informant);
		}

		/** An informant body of Victim killed by Killer in its own slot (VictimKind Informant, its death write time). */
		void KillInformant(FParticipantId Victim, FParticipantId Killer, uint64 Body, double DeathServerTime)
		{
			Slot.NextSlot();
			FPenaltyKill Kill{ Pawn(Body), Killer, EVictimKind::Informant, EKillCause::Strike };
			Kill.VictimParticipant = Victim;
			Kill.VictimDeathServerTime = DeathServerTime;
			Core->OnPawnKilled(Kill);
			Roster.LivePawns.Remove(Victim);
		}

		/** The rows of Type the in-memory Event Log holds, decoded (RowType's Decode). */
		template <typename RowType>
		TArray<RowType> Rows() const
		{
			TArray<RowType> Out;
			for (const FRoundLogEntry& Entry : Log.Entries)
			{
				RowType Row;
				if (PenaltyEventRows::Decode(Entry, Row))
				{
					Out.Add(Row);
				}
			}
			return Out;
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
