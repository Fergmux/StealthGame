#pragma once

// Shared rig of the strike-resolver tests (stories asn-005 to asn-011): FAttackResolverCore over recording fakes of every
// seam (pawn facts, Roster, visibility, bushes, defence store, queue, sub-seeds, hub events, stance), a fake clock and a
// bare game world whose AStealthPawns serve only as identities (their positions are the scripted facts). Commands are
// resolved by calling the core directly with explicit keys (the stub-queue harness of ADR-0008), or registered on a real
// FResolutionQueueCore by the drain tests. Fixture values come from the tuning defaults (FEncounterParams::Defaults()).

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/AttackResolverCore.h"
#include "Server/BushSpotQuery.h"
#include "Server/DefenceStateCore.h"
#include "Server/Random/StealthServerSeeds.h"
#include "Server/Testing/StealthFakeClock.h"
#include "Server/Testing/StealthFaultSinkTestSupport.h"
#include "Tests/Assassination/AssassinationTestWorld.h"
#include "UObject/ObjectKey.h"

namespace StealthEncounterTest
{
	/** Team of an unbound pawn (an NPC). */
	inline constexpr int32 NoTeam = -1;

	/** A shared ordered call log (RequestTransition, OnPawnKilled, VacateCard, ...). */
	using FCallLog = TArray<FString>;

	class FFakePawns final : public IEncounterPawnQuery
	{
	public:
		TArray<FEncounterPawnFacts> Pawns;

		FEncounterPawnFacts* Find(const APawn* Pawn) { return Pawns.FindByPredicate([Pawn](const FEncounterPawnFacts& F) { return F.Pawn == Pawn; }); }
		void Remove(const APawn* Pawn) { Pawns.RemoveAll([Pawn](const FEncounterPawnFacts& F) { return F.Pawn == Pawn; }); }

		virtual void GetPawns(TArray<FEncounterPawnFacts>& Out) const override { Out = Pawns; }
		virtual bool GetFacts(const APawn* Pawn, FEncounterPawnFacts& Out) const override
		{
			const FEncounterPawnFacts* Found = Pawns.FindByPredicate([Pawn](const FEncounterPawnFacts& F) { return F.Pawn == Pawn; });
			if (Found == nullptr)
			{
				return false;
			}
			Out = *Found;
			return true;
		}
	};

	class FFakeRoster final : public IEncounterRosterGateway
	{
	public:
		struct FMember
		{
			uint32 Participant = 0;
			int32 Team = NoTeam;
			EParticipantState State = EParticipantState::Active;
		};
		struct FTransition
		{
			const APawn* Pawn = nullptr;
			ERosterReason Reason = ERosterReason::None;
			double RetireAt = 0.0;
		};

		TMap<const APawn*, FMember> Members;
		TArray<FTransition> Transitions;
		FCallLog* Calls = nullptr;
		/** Pawns the Roster answers NotTargetable(AlreadyResolved) for (a retired body). */
		TSet<const APawn*> Resolved;
		/** Called after the state write (the Round rig forwards the transition to Round's F2 check). */
		TFunction<void(const APawn*, ERosterReason)> OnTransition;

		virtual FRosterVictimKind ResolveVictimKind(const APawn* Pawn) override
		{
			if (Resolved.Contains(Pawn))
			{
				return FRosterVictimKind::NotTargetable(ERosterNotTargetableReason::AlreadyResolved);
			}
			const FMember* Member = Members.Find(Pawn);
			if (Member == nullptr)
			{
				return FRosterVictimKind::Make(ERosterVictimKind::Npc);
			}
			return FRosterVictimKind::Make(Member->State == EParticipantState::Active ? ERosterVictimKind::Player : ERosterVictimKind::Informant);
		}
		virtual bool AreTeammates(const APawn* A, const APawn* B) override
		{
			const FMember* MA = Members.Find(A);
			const FMember* MB = Members.Find(B);
			return MA != nullptr && MB != nullptr && MA->Team == MB->Team;
		}
		virtual bool IsLegalityActive(const APawn* Pawn) override
		{
			const FMember* Member = Members.Find(Pawn);
			return Member == nullptr || Member->State == EParticipantState::Active;
		}
		virtual void RequestElimination(const APawn* Pawn, ERosterReason Reason, double RetireAt) override
		{
			if (Calls != nullptr)
			{
				Calls->Add(TEXT("RequestTransition"));
			}
			Transitions.Add({ Pawn, Reason, RetireAt });
			if (FMember* Member = Members.Find(Pawn))
			{
				Member->State = EParticipantState::Informant;
			}
			if (OnTransition)
			{
				OnTransition(Pawn, Reason);
			}
		}
		virtual FParticipantId GetParticipant(const APawn* Pawn) override
		{
			const FMember* Member = Members.Find(Pawn);
			return Member != nullptr ? FParticipantId(Member->Participant) : FParticipantId();
		}
		virtual uint32 GetTeamIndex(const APawn* Pawn) override
		{
			const FMember* Member = Members.Find(Pawn);
			return Member != nullptr && Member->Team >= 0 ? static_cast<uint32>(Member->Team) : 0xFFu;
		}
	};

	class FFakeVisibility final : public IEncounterVisibilityQuery
	{
	public:
		TSet<const APawn*> Culled;
		virtual bool CanViewerKnow(const APawn* Viewer, const APawn* Pawn) override { return !Culled.Contains(Pawn); }
	};

	/** Round bushes: a footprint radius, a zone radius (footprint + BushSpotToleranceCm) and an explicit occupant list. */
	class FFakeBushes final : public IBushSpotQuery
	{
	public:
		struct FBush
		{
			uint16 Id = 0;
			FVector Center = FVector::ZeroVector;
			double ZoneRadiusCm = 0.0;
			/** The square footprint (story asn-012's WarpPoint clamp reads it). */
			FBox2D Footprint = FBox2D(ForceInit);
			TArray<APawn*> Occupants;
		};
		TArray<FBush> Bushes;
		TSet<const APawn*> Concealed;
		TArray<APawn*> Removed;

		virtual bool FindSpotZone(const FVector& WorldPoint, uint16& OutBushId) const override
		{
			for (const FBush& Bush : Bushes)
			{
				if (FVector::Dist2D(WorldPoint, Bush.Center) <= Bush.ZoneRadiusCm)
				{
					OutBushId = Bush.Id;
					return true;
				}
			}
			return false;
		}
		virtual void GetSpotOccupants(uint16 BushId, const APawn* Attacker, TArray<APawn*>& Out) const override
		{
			for (const FBush& Bush : Bushes)
			{
				if (Bush.Id == BushId)
				{
					Out = Bush.Occupants;
				}
			}
		}
		virtual bool IsConcealed(const APawn* Pawn) const override { return Concealed.Contains(Pawn); }
		virtual bool GetFootprint(uint16 BushId, FBox2D& Out) const override
		{
			for (const FBush& Bush : Bushes)
			{
				if (Bush.Id == BushId)
				{
					Out = Bush.Footprint;
					return Out.bIsValid;
				}
			}
			return false;
		}
		virtual void RemoveOccupant(APawn* Pawn) override
		{
			Removed.Add(Pawn);
			Concealed.Remove(Pawn);
			for (FBush& Bush : Bushes)
			{
				Bush.Occupants.Remove(Pawn);
			}
		}
	};

	class FFakeDefence final : public IEncounterDefenceStore
	{
	public:
		TMap<FObjectKey, TUniquePtr<FDefenceStateCore>> States;

		virtual FDefenceStateCore* Find(const APawn* Pawn) override
		{
			TUniquePtr<FDefenceStateCore>* Found = States.Find(FObjectKey(Pawn));
			return Found != nullptr ? Found->Get() : nullptr;
		}
		virtual FDefenceStateCore* FindOrAdd(const APawn* Pawn) override
		{
			TUniquePtr<FDefenceStateCore>& Slot = States.FindOrAdd(FObjectKey(Pawn));
			if (!Slot.IsValid())
			{
				Slot = MakeUnique<FDefenceStateCore>();
			}
			return Slot.Get();
		}
		int32 ResetCount = 0;
		virtual void ResetAll() override
		{
			++ResetCount;
			for (TPair<FObjectKey, TUniquePtr<FDefenceStateCore>>& Pair : States)
			{
				if (Pair.Value.IsValid())
				{
					Pair.Value->Reset();
				}
			}
		}
	};

	/** A scripted tally source (story asn-015): per-attacker answers, every call logged with its Result. */
	class FScriptedTally final : public IStrikeTallySource
	{
	public:
		struct FCall
		{
			const APawn* Attacker = nullptr;
			EStrikeResult Result = EStrikeResult::NoContact;
		};
		/** The answer per attacker (missing: Default). */
		TMap<const APawn*, FStrikeTally> Scripted;
		FStrikeTally Default;
		TArray<FCall> Calls;
		FCallLog* Log = nullptr;
		/** Called on every query (a test may script the answer from the call's Result). */
		TFunction<FStrikeTally(const APawn*, EStrikeResult)> Answer;

		virtual FStrikeTally GetTallyForResult(const APawn* Attacker, EStrikeResult Result) override
		{
			Calls.Add({ Attacker, Result });
			if (Log != nullptr)
			{
				Log->Add(TEXT("GetTallyForResult"));
			}
			if (Answer)
			{
				return Answer(Attacker, Result);
			}
			const FStrikeTally* Found = Scripted.Find(Attacker);
			return Found != nullptr ? *Found : Default;
		}
	};

	/** Records every appended owner reply with its sink and slot key (story asn-015). Index 0 (the null sink) discards. */
	class FRecordingReplies final : public IEncounterReplySink
	{
	public:
		struct FEntry
		{
			FReplySinkId Sink;
			FQueueKey Key;
			FCommandReply Reply;
		};
		TArray<FEntry> Entries;
		/** Appends to the null sink (counted, then discarded as the table does). */
		int32 NullSinkAppends = 0;
		FCallLog* Log = nullptr;

		virtual void AppendReply(FReplySinkId Sink, FQueueKey Key, const FCommandReply& Reply) override
		{
			if (Log != nullptr)
			{
				Log->Add(Reply.Kind == ECommandReplyKind::StrikeResult ? TEXT("AppendStrikeResult") : TEXT("AppendReply"));
			}
			if (Sink.Index == 0)
			{
				++NullSinkAppends;
				return;
			}
			Entries.Add({ Sink, Key, Reply });
		}
		TArray<FCommandReply> For(uint16 SinkIndex, TOptional<ECommandReplyKind> Kind = {}) const
		{
			TArray<FCommandReply> Out;
			for (const FEntry& Entry : Entries)
			{
				if (Entry.Sink.Index == SinkIndex && (!Kind.IsSet() || Entry.Reply.Kind == Kind.GetValue()))
				{
					Out.Add(Entry.Reply);
				}
			}
			return Out;
		}
		int32 Count(ECommandReplyKind Kind) const
		{
			return Entries.FilterByPredicate([Kind](const FEntry& E) { return E.Reply.Kind == Kind; }).Num();
		}
	};

	/** Records the owner-only cooldown publications (story asn-016). */
	class FRecordingOwnerCooldown final : public IEncounterOwnerCooldown
	{
	public:
		TArray<TPair<const APawn*, double>> Writes;
		TMap<const APawn*, double> Current;
		int32 ResetCount = 0;
		virtual void PublishCooldownEnd(APawn* Pawn, double End) override { Writes.Add({ Pawn, End }); Current.Add(Pawn, End); }
		virtual void ResetAll() override { ++ResetCount; Current.Reset(); }
		double Get(const APawn* Pawn) const { const double* V = Current.Find(Pawn); return V != nullptr ? *V : -1.0; }
	};

	/** Records the body hand-over calls with the clock time of each (story asn-019). */
	class FRecordingHandOver final : public IEncounterBodyHandOver
	{
	public:
		struct FCall
		{
			APawn* Body = nullptr;
			double ServerTime = 0.0;
		};
		const IStealthClock* Clock = nullptr;
		TArray<FCall> Released;
		TArray<FCall> TornDown;
		FCallLog* Log = nullptr;
		virtual void ReleaseBody(APawn* Body) override { Released.Add({ Body, Now() }); if (Log != nullptr) { Log->Add(TEXT("ReleaseBody")); } }
		virtual void RequestTeardown(APawn* Body) override { TornDown.Add({ Body, Now() }); if (Log != nullptr) { Log->Add(TEXT("RequestTeardown")); } }
		double Now() const { return Clock != nullptr ? Clock->GetServerTimeSeconds() : 0.0; }
		int32 CountReleased(const APawn* Body) const { return Released.FilterByPredicate([Body](const FCall& C) { return C.Body == Body; }).Num(); }
		int32 CountTornDown(const APawn* Body) const { return TornDown.FilterByPredicate([Body](const FCall& C) { return C.Body == Body; }).Num(); }
	};

	/** Records the committed body removals (story asn-019). */
	class FRecordingRemovalObserver final : public IBodyRemovalObserver
	{
	public:
		TArray<APawn*> Committed;
		FCallLog* Log = nullptr;
		virtual void OnBodyRemovalCommitted(APawn* Body) override { Committed.Add(Body); if (Log != nullptr) { Log->Add(TEXT("OnBodyRemovalCommitted")); } }
	};

	/** Records what the core asks of the queue (the direct-call tests; the drain tests use FEncounterQueueAdapter). */
	class FFakeQueue final : public IEncounterQueue
	{
	public:
		struct FScheduled
		{
			double Due = 0.0;
			FQueueItem Item;
		};
		TArray<FScheduled> Scheduled;
		TArray<FStagedWrite> Staged;
		TArray<uint32> VoidCodes;

		virtual void ScheduleAt(double Due, const FQueueItem& Item) override { Scheduled.Add({ Due, Item }); }
		virtual void SetVoidCode(uint32 Code) override { VoidCodes.Add(Code); }
		virtual bool StageWrite(const FStagedWrite& Write) override { Staged.Add(Write); return true; }
	};

	/** A fixed sub-seed per domain, and a spy of the domains asked (never RoundSeed). */
	class FFakeSeeds final : public IServerSubSeedSource
	{
	public:
		uint64 Seed = 0x5EEDB05Bu;
		mutable TArray<ESeedDomain> Asked;
		mutable int32 RoundSeedCalls = 0;

		virtual uint64 GetServerSubSeed(ESeedDomain Domain, uint32 Attempt) const override { Asked.Add(Domain); return Seed ^ (static_cast<uint64>(Attempt) << 32); }
		virtual int32 GetRoundSeedCandidate(uint32 RoundIndex, uint32 Draw) const override { ++RoundSeedCalls; return 1; }
	};

	class FRecordingEvents final : public IEncounterActionEvents
	{
	public:
		struct FEvent
		{
			APawn* Pawn = nullptr;
			EActionType Type = EActionType::Attack;
			int32 Kind = 0; // 0 started, 1 cancelled, 2 completed
			ECancelReason Reason = ECancelReason::Released;
		};
		TArray<FEvent> Events;
		virtual void OnActionStarted(APawn* Pawn, EActionType Type) override { Events.Add({ Pawn, Type, 0 }); }
		virtual void OnActionCancelled(APawn* Pawn, EActionType Type, ECancelReason Reason) override { Events.Add({ Pawn, Type, 1, Reason }); }
		virtual void OnActionCompleted(APawn* Pawn, EActionType Type) override { Events.Add({ Pawn, Type, 2 }); }
		int32 Count(APawn* Pawn, EActionType Type, int32 Kind) const
		{
			return Events.FilterByPredicate([&](const FEvent& E) { return E.Pawn == Pawn && E.Type == Type && E.Kind == Kind; }).Num();
		}
	};

	class FRecordingStance final : public IEncounterBraceStanceSink
	{
	public:
		TMap<const APawn*, bool> Braced;
		TArray<TPair<const APawn*, bool>> Writes;
		virtual void SetBraceStance(APawn* Pawn, bool bBraced) override { Braced.Add(Pawn, bBraced); Writes.Add({ Pawn, bBraced }); }
	};

	class FRecordingCards final : public IEncounterCardLedger
	{
	public:
		FCallLog* Calls = nullptr;
		TArray<APawn*> Vacated;
		virtual void VacateCard(APawn* Pawn) override { Vacated.Add(Pawn); if (Calls != nullptr) { Calls->Add(TEXT("VacateCard")); } }
	};

	/**
	 * Records the replicated pawn-state writes (stories asn-012, 013) as UPawnCommandComponent::WriteActionState and
	 * AStealthPawn::ApplyDeathWrite would make them: Seq assigned per pawn (from the default 1), StartServerTime filled from
	 * the clock when unset, one death write per pawn. Each record carries the clock time of the write.
	 */
	class FRecordingPawnState final : public IEncounterPawnStateWriter
	{
	public:
		struct FWrite
		{
			/** false: an FActionState write; true: a death write. */
			bool bDeath = false;
			APawn* Pawn = nullptr;
			FActionState State;
			FDeathState Death;
			double ServerTime = 0.0;
		};
		const IStealthClock* Clock = nullptr;
		TArray<FWrite> Writes;
		TMap<const APawn*, FActionState> Current;
		TMap<const APawn*, FDeathState> Deaths;

		virtual void WriteActionState(APawn* Pawn, const FActionStateWrite& Write) override
		{
			FActionState& State = Current.FindOrAdd(Pawn);
			FActionState Next;
			Next.Pose = Write.Pose;
			Next.Seq = FActionState::NextSeq(State.Seq);
			Next.Variant = Write.Variant;
			Next.StartServerTime = Write.StartServerTime >= 0.0 ? Write.StartServerTime : Now();
			Next.ContactServerTime = Write.ContactServerTime;
			Next.WarpPoint = Write.WarpPoint;
			State = Next;
			FWrite& Record = Writes.AddDefaulted_GetRef();
			Record.Pawn = Pawn;
			Record.State = Next;
			Record.ServerTime = Now();
		}
		virtual EActionPose GetPose(const APawn* Pawn) const override
		{
			const FActionState* State = Current.Find(Pawn);
			return State != nullptr ? State->Pose : EActionPose::None;
		}
		virtual bool WriteDeath(APawn* Pawn, const FDeathState& State) override
		{
			if (Deaths.Contains(Pawn))
			{
				return false;
			}
			Deaths.Add(Pawn, State);
			FWrite& Record = Writes.AddDefaulted_GetRef();
			Record.bDeath = true;
			Record.Pawn = Pawn;
			Record.Death = State;
			Record.ServerTime = Now();
			return true;
		}
		double Now() const { return Clock != nullptr ? Clock->GetServerTimeSeconds() : 0.0; }
		/** The records on Pawn (an FActionState write when bDeath is false). */
		TArray<FWrite> For(const APawn* Pawn) const { return Writes.FilterByPredicate([Pawn](const FWrite& W) { return W.Pawn == Pawn; }); }
	};

	/** One OnPawnKilled call. */
	struct FKill
	{
		APawn* Victim = nullptr;
		APawn* Killer = nullptr;
		EVictimKind Kind = EVictimKind::Npc;
		EKillCause Cause = EKillCause::Strike;
		/** GetScheduledDeathServerTime(Victim) read inside the fan-out. */
		TOptional<double> ScheduledDeath;
	};

	/** The rig: every fake, the core and helpers that build items and call Resolve with explicit keys. */
	struct FRig
	{
		StealthAssassinationTest::FScopedWorld World;
		FFakeStealthClock Clock{ 100.0, 6000 };
		FFakePawns Pawns;
		FFakeRoster Roster;
		FFakeVisibility Visibility;
		FFakeBushes Bushes;
		FFakeDefence Defence;
		FFakeQueue Queue;
		FFakeSeeds Seeds;
		FRecordingEvents Events;
		FRecordingStance Stance;
		FRecordingCards Cards;
		/** Bound only when the rig is built with bWithPawnState (the wire-write tests, stories asn-012 / 013). */
		FRecordingPawnState PawnState;
		/** Always bound (stories asn-015, asn-016): the tally answers {0, false} unless scripted. */
		FScriptedTally Tally;
		FRecordingReplies Replies;
		FRecordingOwnerCooldown OwnerCooldown;
		/** Bound only with bWithBodySequence (story asn-019; the body timers go to the rig's queue). */
		FRecordingHandOver HandOver;
		FRecordingRemovalObserver RemovalObserver;
		FStealthRecordingFaultSink Faults;
		FCallLog Calls;
		TArray<FKill> Kills;
		TUniquePtr<FAttackResolverCore> Core;
		FEncounterParams Params = FEncounterParams::Defaults();
		uint64 NextSeq = 1;
		uint64 NextSerial = 1;
		double SpawnX = 0.0;

		explicit FRig(const TCHAR* Name, IEncounterQueue* QueueOverride = nullptr, bool bWithPawnState = false, bool bWithBodySequence = false)
			: World(Name)
		{
			PawnState.Clock = &Clock;
			HandOver.Clock = &Clock;
			Roster.Calls = &Calls;
			Cards.Calls = &Calls;
			Tally.Log = &Calls;
			HandOver.Log = &Calls;
			RemovalObserver.Log = &Calls;
			FAttackResolverDeps Deps;
			Deps.Clock = &Clock;
			Deps.Queue = QueueOverride != nullptr ? QueueOverride : &Queue;
			Deps.Roster = &Roster;
			Deps.Pawns = &Pawns;
			Deps.Defence = &Defence;
			Deps.SubSeeds = &Seeds;
			Deps.Visibility = &Visibility;
			Deps.Bushes = &Bushes;
			Deps.Events = &Events;
			Deps.Stance = &Stance;
			Deps.Cards = &Cards;
			Deps.PawnState = bWithPawnState ? &PawnState : nullptr;
			Deps.Tally = &Tally;
			Deps.Replies = &Replies;
			Deps.OwnerCooldown = &OwnerCooldown;
			Deps.BodyHandOver = bWithBodySequence ? &HandOver : nullptr;
			Deps.BodyObserver = bWithBodySequence ? &RemovalObserver : nullptr;
			Deps.Faults = &Faults;
			Core = MakeUnique<FAttackResolverCore>(Deps, Params);
			Core->OnPawnKilled.AddLambda([this](APawn* Victim, APawn* Killer, EVictimKind Kind, EKillCause Cause)
			{
				Calls.Add(TEXT("OnPawnKilled"));
				Kills.Add({ Victim, Killer, Kind, Cause, Core->GetScheduledDeathServerTime(Victim) });
			});
		}

		/** A pawn identity with facts at Location (Z 0), facing Yaw; Team NoTeam = an unbound NPC, else a bound player. */
		APawn* Spawn(const FVector& Location, int32 Team = NoTeam, double YawDeg = 0.0, uint32 Participant = 0)
		{
			SpawnX += 500.0;
			APawn* Pawn = World.SpawnPawn(SpawnX);
			FEncounterPawnFacts Facts;
			Facts.Pawn = Pawn;
			Facts.Location = Location;
			Facts.FacingYawDeg = YawDeg;
			Facts.SpawnSerial = NextSerial++;
			Pawns.Pawns.Add(Facts);
			if (Team != NoTeam)
			{
				FFakeRoster::FMember Member;
				Member.Participant = Participant != 0 ? Participant : static_cast<uint32>(Roster.Members.Num() + 1);
				Member.Team = Team;
				Roster.Members.Add(Pawn, Member);
			}
			return Pawn;
		}

		FEncounterPawnFacts& Facts(const APawn* Pawn) { return *Pawns.Find(Pawn); }

		/** The next key in the current tick. */
		FQueueKey NextKey() { return FQueueKey(Clock.GetServerTickIndex(), NextSeq++); }

		/** Advances time and ticks (60 Hz relation) by Seconds. */
		void Advance(double Seconds)
		{
			const uint64 Ticks = static_cast<uint64>(FMath::RoundToDouble(Seconds * 60.0));
			Clock.Advance(Seconds, FMath::Max<uint64>(Ticks, 1));
			NextSeq = 1;
		}

		FCommandPayload Payload(APawn* Pawn, ECommandKind Kind, uint16 SinkIndex = 0) const
		{
			FCommandPayload P;
			P.Pawn.Pawn = Pawn;
			P.Kind = static_cast<uint8>(Kind);
			P.Sink.Index = SinkIndex;
			return P;
		}

		static FQueueItem Item(const FCommandPayload& P)
		{
			return FQueueItem::Make(EQueueItemType::PawnCommand, FEpochToken(), FMailboxId::ServerLocal(), P);
		}

		FQueueItem AttackItem(APawn* Attacker, const FVector& WorldPoint, APawn* Ref = nullptr, uint16 SinkIndex = 0) const
		{
			FCommandPayload P = Payload(Attacker, ECommandKind::Attack, SinkIndex);
			P.WorldPoint = WorldPoint;
			if (Ref != nullptr)
			{
				P.Target = FSubjectRef::ForPawn(FPawnHandle{ Ref });
			}
			return Item(P);
		}

		EResolveResult Attack(APawn* Attacker, const FVector& WorldPoint, APawn* Ref = nullptr, TOptional<FQueueKey> Key = {})
		{
			return Core->Resolve(AttackItem(Attacker, WorldPoint, Ref), Key.IsSet() ? Key.GetValue() : NextKey());
		}

		/** Attack at the named pawn's own location (an aimed PawnRef strike). */
		EResolveResult AttackRef(APawn* Attacker, APawn* Ref, TOptional<FQueueKey> Key = {})
		{
			return Attack(Attacker, Facts(Ref).Location, Ref, Key);
		}

		EResolveResult Command(APawn* Pawn, ECommandKind Kind, TOptional<FQueueKey> Key = {}, uint16 SinkIndex = 0)
		{
			return Core->Resolve(Item(Payload(Pawn, Kind, SinkIndex)), Key.IsSet() ? Key.GetValue() : NextKey());
		}

		/** An Attack from SinkIndex with ClientSeq Seq (the owner-result tests, story asn-015). */
		EResolveResult AttackFrom(APawn* Attacker, const FVector& WorldPoint, APawn* Ref, uint16 SinkIndex, uint32 Seq, TOptional<FQueueKey> Key = {})
		{
			FQueueItem I = AttackItem(Attacker, WorldPoint, Ref, SinkIndex);
			I.Payload.Get<FCommandPayload>().ClientSeq.Seq = Seq;
			return Core->Resolve(I, Key.IsSet() ? Key.GetValue() : NextKey());
		}

		/** A Prepare(Begin) from SinkIndex with ClientSeq Seq. */
		EResolveResult BraceFrom(APawn* Pawn, uint16 SinkIndex, uint32 Seq, TOptional<FQueueKey> Key = {})
		{
			FCommandPayload P = Payload(Pawn, ECommandKind::PrepareBegin, SinkIndex);
			P.ClientSeq.Seq = Seq;
			return Core->Resolve(Item(P), Key.IsSet() ? Key.GetValue() : NextKey());
		}

		/** Resolves every scheduled TimerFired item of Kind for Body (the body timers; story asn-019). */
		EResolveResult FireBodyTimer(APawn* Body, ETimerKind Kind)
		{
			for (int32 Index = Queue.Scheduled.Num() - 1; Index >= 0; --Index)
			{
				const FTimerPayload* Timer = Queue.Scheduled[Index].Item.Payload.TryGet<FTimerPayload>();
				if (Timer != nullptr && Timer->Kind == Kind && Timer->Subject.Pawn.Pawn.Get() == Body)
				{
					return Core->Resolve(Queue.Scheduled[Index].Item, NextKey());
				}
			}
			return EResolveResult::Voided;
		}

		/** The due time of Body's scheduled TimerFired item of Kind (unset when none). */
		TOptional<double> BodyTimerDue(const APawn* Body, ETimerKind Kind) const
		{
			for (const FFakeQueue::FScheduled& Entry : Queue.Scheduled)
			{
				const FTimerPayload* Timer = Entry.Item.Payload.TryGet<FTimerPayload>();
				if (Timer != nullptr && Timer->Kind == Kind && Timer->Subject.Pawn.Pawn.Get() == Body)
				{
					return Entry.Due;
				}
			}
			return {};
		}

		/** Resolves the latest scheduled hold timer of Pawn (the CapEnd or DebounceExpiry the core scheduled) at Key. */
		EResolveResult FireHold(APawn* Pawn, EHoldTimerKind Kind, FQueueKey Key)
		{
			for (int32 Index = Queue.Scheduled.Num() - 1; Index >= 0; --Index)
			{
				const FHoldTimerPayload* Hold = Queue.Scheduled[Index].Item.Payload.TryGet<FHoldTimerPayload>();
				if (Hold != nullptr && Hold->Pawn.Pawn.Get() == Pawn && Hold->Kind == Kind)
				{
					return Core->Resolve(Queue.Scheduled[Index].Item, Key);
				}
			}
			return EResolveResult::Voided;
		}

		const FAttackResolution& Last() const { return Core->GetResolutions().Last(); }
		const FEncounterCommandReply& LastReply() const { return Core->GetReplies().Last(); }
		FDefenceStateCore* State(const APawn* Pawn) { return Defence.Find(Pawn); }

		/** A bush at Center with zone radius footprint + BushSpotToleranceCm (the stand-in tolerance = OpenSpotToleranceCm). */
		FFakeBushes::FBush& AddBush(uint16 Id, const FVector& Center, double FootprintCm)
		{
			FFakeBushes::FBush& Bush = Bushes.Bushes.AddDefaulted_GetRef();
			Bush.Id = Id;
			Bush.Center = Center;
			Bush.ZoneRadiusCm = FootprintCm + Params.Server.OpenSpotToleranceCm;
			Bush.Footprint = FBox2D(FVector2D(Center) - FVector2D(FootprintCm, FootprintCm), FVector2D(Center) + FVector2D(FootprintCm, FootprintCm));
			return Bush;
		}

		void Conceal(APawn* Pawn, uint16 BushId)
		{
			Bushes.Concealed.Add(Pawn);
			for (FFakeBushes::FBush& Bush : Bushes.Bushes)
			{
				if (Bush.Id == BushId)
				{
					Bush.Occupants.Add(Pawn);
				}
			}
		}
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS
