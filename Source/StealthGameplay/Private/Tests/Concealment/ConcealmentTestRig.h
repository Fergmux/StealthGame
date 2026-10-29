#pragma once

// Shared rig of the StealthGame.Concealment.* headless tests (stories con-003..con-024): the REAL round queue on a fake
// clock (FStealthQueueTestRig), a bare game world with real AStealthPawns, a scripted pawn world (positions, legality,
// locomotion, the warp landing at its target), a scripted roster (participants and teams), and recorders for the cull,
// the owner/team flags, the replies, the hub events and the Event Log. The FConcealmentCore under test is wired to all of
// them; its resolvers are registered on the queue exactly as the service registers them. Test-only.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Server/ConcealmentCore.h"
#include "Server/Queue/StealthQueueResolver.h"
#include "Server/Testing/StealthQueueTestRig.h"
#include "Server/Visibility/StealthConcealmentCullSource.h"
#include "View/CommandReplyTypes.h"
#include "View/ConcealmentTypes.h"
#include "View/Core/StealthPawn.h"
#include "View/NetPawnCommand.h"

namespace StealthConcealmentTest
{
	/** One server frame (60 Hz). */
	inline constexpr double FrameSec = 1.0 / 60.0;

	/** Scripted facts of one pawn. */
	struct FPawnFacts
	{
		FVector Location = FVector::ZeroVector;
		double LookYawDeg = 0.0;
		ECommandLegalityClass Legality = ECommandLegalityClass::Active;
		bool bRunning = false;
		bool bTraversing = false;
		bool bLive = true;
		FActionState Action;
		/** A started warp lands on its target when it ends (the server root motion, simulated). */
		bool bWarpLands = true;
		FVector WarpTarget = FVector::ZeroVector;
		double WarpEnd = -1.0;
		FVector WarpStart = FVector::ZeroVector;
		double WarpBegin = 0.0;
		double DepenetrationCapCm = -1.0;
		EConcealState Mirror = EConcealState::Outside;
	};

	/** The scripted pawn world (IConcealmentPawnWorld). */
	class FFakePawnWorld : public IConcealmentPawnWorld
	{
	public:
		const IStealthClock* Clock = nullptr;
		TMap<const APawn*, FPawnFacts> Facts;
		int32 PoseWrites = 0;
		int32 Warps = 0;
		TArray<TPair<const APawn*, EActionPose>> PoseLog;

		FPawnFacts& Of(const APawn* Pawn) { return Facts.FindOrAdd(Pawn); }

		/** Moves every running warp along its line to the clock's time (linear: MoveToForce at constant speed). */
		void AdvanceWarps(double Now)
		{
			for (TPair<const APawn*, FPawnFacts>& Pair : Facts)
			{
				FPawnFacts& F = Pair.Value;
				if (F.WarpEnd < 0.0 || !F.bWarpLands)
				{
					continue;
				}
				const double Alpha = F.WarpEnd > F.WarpBegin ? FMath::Clamp((Now - F.WarpBegin) / (F.WarpEnd - F.WarpBegin), 0.0, 1.0) : 1.0;
				F.Location = FMath::Lerp(F.WarpStart, F.WarpTarget, Alpha);
				if (Alpha >= 1.0)
				{
					F.WarpEnd = -1.0;
				}
			}
		}

		virtual bool IsLive(const APawn& Pawn) const override { const FPawnFacts* F = Facts.Find(&Pawn); return F == nullptr || F->bLive; }
		virtual FVector GetLocation(const APawn& Pawn) const override { const FPawnFacts* F = Facts.Find(&Pawn); return F != nullptr ? F->Location : FVector::ZeroVector; }
		virtual double GetCapsuleRadiusCm(const APawn&) const override { return 35.0; }
		virtual double GetCapsuleHalfHeightCm(const APawn&) const override { return 86.0; }
		virtual double GetLookYawDeg(const APawn& Pawn) const override { const FPawnFacts* F = Facts.Find(&Pawn); return F != nullptr ? F->LookYawDeg : 0.0; }
		virtual ECommandLegalityClass GetLegalityClass(const APawn& Pawn) const override { const FPawnFacts* F = Facts.Find(&Pawn); return F != nullptr ? F->Legality : ECommandLegalityClass::Active; }
		virtual bool IsRunning(const APawn& Pawn) const override { const FPawnFacts* F = Facts.Find(&Pawn); return F != nullptr && F->bRunning; }
		virtual bool IsTraversing(const APawn& Pawn) const override { const FPawnFacts* F = Facts.Find(&Pawn); return F != nullptr && F->bTraversing; }
		virtual void WritePose(APawn& Pawn, EActionPose Pose, double StartServerTime) override
		{
			FPawnFacts& F = Of(&Pawn);
			F.Action.Pose = Pose;
			F.Action.StartServerTime = StartServerTime >= 0.0 ? StartServerTime : (Clock != nullptr ? Clock->GetServerTimeSeconds() : 0.0);
			F.Action.Seq = FActionState::NextSeq(F.Action.Seq);
			++PoseWrites;
			PoseLog.Add({ &Pawn, Pose });
		}
		virtual FActionState GetActionState(const APawn& Pawn) const override { const FPawnFacts* F = Facts.Find(&Pawn); return F != nullptr ? F->Action : FActionState(); }
		virtual void StartWarp(APawn& Pawn, const FVector& Target, double DurationSec, bool, bool) override
		{
			FPawnFacts& F = Of(&Pawn);
			F.WarpStart = F.Location;
			F.WarpTarget = Target;
			F.WarpBegin = Clock != nullptr ? Clock->GetServerTimeSeconds() : 0.0;
			F.WarpEnd = F.WarpBegin + DurationSec;
			++Warps;
		}
		virtual void OnStateChanged(APawn& Pawn, EConcealState State, uint16, double) override { Of(&Pawn).Mirror = State; }
		virtual void GetPawnsNear(const FVector& Point, double RadiusCm, TArray<APawn*>& OutPawns) const override
		{
			OutPawns.Reset();
			for (const TPair<const APawn*, FPawnFacts>& Pair : Facts)
			{
				if (Pair.Value.bLive && FVector::Dist2D(Pair.Value.Location, Point) <= RadiusCm)
				{
					OutPawns.Add(const_cast<APawn*>(Pair.Key));
				}
			}
		}
		virtual void SetExitDepenetrationCap(APawn& Pawn, double CapCm) override { Of(&Pawn).DepenetrationCapCm = CapCm; }
	};

	/** The scripted roster (IConcealmentRelations). */
	class FFakeRelations : public IConcealmentRelations
	{
	public:
		TMap<const APawn*, FParticipantId> Participants;
		TMap<uint32, int32> Teams;

		void Bind(const APawn* Pawn, uint32 Id, int32 Team) { Participants.Add(Pawn, FParticipantId(Id)); Teams.Add(Id, Team); }

		virtual FParticipantId GetParticipant(const APawn& Pawn) const override { const FParticipantId* P = Participants.Find(&Pawn); return P != nullptr ? *P : FParticipantId(); }
		virtual bool IsAllyViewer(FParticipantId Viewer, const APawn& Pawn) const override
		{
			const FParticipantId Owner = GetParticipant(Pawn);
			if (!Viewer.IsValid() || !Owner.IsValid())
			{
				return false;
			}
			const int32* A = Teams.Find(Viewer.Value);
			const int32* B = Teams.Find(Owner.Value);
			return Viewer == Owner || (A != nullptr && B != nullptr && *A == *B);
		}
		virtual bool IsPlayerPawn(const APawn& Pawn) const override { return GetParticipant(Pawn).IsValid(); }
	};

	/** Records every flag write (IConcealmentFlagSink). */
	class FRecordingFlags : public IConcealmentFlagSink
	{
	public:
		TMap<uint32, bool> Owner;
		TMap<uint32, bool> Team;
		int32 Writes = 0;
		virtual void SetOwnerConcealed(FParticipantId Participant, bool b) override { Owner.Add(Participant.Value, b); ++Writes; }
		virtual void SetTeammateConcealed(FParticipantId Participant, bool b) override { Team.Add(Participant.Value, b); ++Writes; }
		bool OwnerOf(uint32 Id) const { const bool* B = Owner.Find(Id); return B != nullptr && *B; }
		bool TeamOf(uint32 Id) const { const bool* B = Team.Find(Id); return B != nullptr && *B; }
	};

	/** Records every SetCullState with the queue key in effect (IConcealmentCullSource). */
	class FRecordingCull : public IConcealmentCullSource
	{
	public:
		struct FCall
		{
			const APawn* Pawn = nullptr;
			bool bConcealed = false;
			double Time = 0.0;
			FQueueKey Key;
		};
		const FResolutionQueueCore* Queue = nullptr;
		TArray<FCall> Calls;
		virtual void SetCullState(APawn* Pawn, bool bConcealed, double Time) override { Calls.Add({ Pawn, bConcealed, Time, Queue != nullptr ? Queue->CurrentKey() : FQueueKey() }); }
		int32 CountFor(const APawn* Pawn, bool bConcealed) const
		{
			return Calls.FilterByPredicate([Pawn, bConcealed](const FCall& C) { return C.Pawn == Pawn && C.bConcealed == bConcealed; }).Num();
		}
	};

	/** IConcealmentQueue over the rig's real queue, with replies and hub events recorded. */
	class FRigQueue : public IConcealmentQueue
	{
	public:
		FResolutionQueueCore* Queue = nullptr;
		struct FReply
		{
			FReplySinkId Sink;
			FCommandReply Reply;
		};
		TArray<FReply> Replies;
		TArray<TPair<const APawn*, EActionType>> Started;
		virtual void ScheduleAt(double Due, const FQueueItem& Item) override { Queue->ScheduleAt(Due, Item); }
		virtual void SetVoidCode(uint32 Code) override { if (Queue->IsInSlot()) { Queue->SetVoidCode(Code); } }
		virtual void AppendReply(FReplySinkId Sink, FQueueKey, const FCommandReply& Reply) override { Replies.Add({ Sink, Reply }); }
		virtual void OnActionStarted(APawn& Pawn, EActionType Type) override { Started.Add({ &Pawn, Type }); }
		int32 CountRejected(ECommandRejectCode Code) const
		{
			return Replies.FilterByPredicate([Code](const FReply& R) { return R.Reply.Kind == ECommandReplyKind::Rejected && R.Reply.Code == Code; }).Num();
		}
	};

	/** The core's resolvers as the service registers them. */
	class FCoreResolver : public IQueueResolver
	{
	public:
		FConcealmentCore* Core = nullptr;
		/** Commands of another kind (Attack etc.): a test hook. */
		TFunction<EResolveResult(const FQueueItem&, FQueueKey)> OnOtherCommand;
		virtual EResolveResult Resolve(const FQueueItem& Item, FQueueKey Key) override
		{
			if (Item.Type == EQueueItemType::TimerFired)
			{
				return Core->ResolveTimer(Item, Key);
			}
			const FCommandPayload* Cmd = Item.Payload.TryGet<FCommandPayload>();
			const ECommandKind Kind = Cmd != nullptr ? static_cast<ECommandKind>(Cmd->Kind) : ECommandKind::Traverse;
			if (Kind != ECommandKind::HideEnter && Kind != ECommandKind::HideExit && OnOtherCommand)
			{
				return OnOtherCommand(Item, Key);
			}
			return Core->ResolveCommand(Item, Key);
		}
	};

	/** A bare server game world (BeginPlay run), destroyed with the rig. */
	struct FWorldHolder
	{
		UWorld* World = nullptr;
		explicit FWorldHolder(const TCHAR* Name)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, Name);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
		}
		~FWorldHolder()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};

	/** The whole rig: one bush (id 1) at the origin, every side open unless the test changes it. */
	struct FRig
	{
		FWorldHolder WorldHolder;
		FStealthQueueTestRig Q;
		FFakePawnWorld Pawns;
		FFakeRelations Relations;
		FRecordingFlags Flags;
		FRecordingCull Cull;
		FRigQueue RigQueue;
		FCoreResolver Resolver;
		TUniquePtr<FConcealmentCore> Core;
		FConcealmentParams Params;
		uint32 NextClientSeq = 1;

		explicit FRig(const TCHAR* Name, uint8 EntryAxisMask = ConcealmentGeometry::AllSidesMask, const FConcealmentParams& InParams = FConcealmentParams())
			: WorldHolder(Name)
			, Params(InParams)
		{
			Pawns.Clock = &Q.Clock;
			RigQueue.Queue = Q.Queue.Get();
			Cull.Queue = Q.Queue.Get();
			FConcealmentCoreDeps Deps;
			Deps.Clock = &Q.Clock;
			Deps.Pawns = &Pawns;
			Deps.Queue = &RigQueue;
			Deps.Relations = &Relations;
			Deps.Flags = &Flags;
			Deps.Cull = &Cull;
			Deps.Log = &Q.Log;
			Deps.Faults = &Q.Faults;
			Core = MakeUnique<FConcealmentCore>(Deps, Params);
			const FBushRuntime Bush = ConcealmentGeometry::MakeBushRuntime(1, FVector::ZeroVector, EntryAxisMask);
			Core->SetBushes(TConstArrayView<FBushRuntime>(&Bush, 1));
			Resolver.Core = Core.Get();
			Q.Queue->RegisterResolver(EQueueItemType::PawnCommand, &Resolver);
			Q.Queue->RegisterTimerResolver(ETimerKind::ConcealmentTransition, &Resolver);
		}

		/** A real AStealthPawn at Location (its facts scripted). */
		AStealthPawn* Spawn(const FVector& Location)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStealthPawn* Pawn = WorldHolder.World->SpawnActor<AStealthPawn>(Location + FVector(0.0, 0.0, 500.0), FRotator::ZeroRotator, SpawnParams);
			Pawns.Of(Pawn).Location = Location;
			return Pawn;
		}

		/** A pawn standing DistanceCm outside side 0's edge (the +X side), at capsule height. */
		AStealthPawn* SpawnAtReach(double DistanceCm, double Y = 0.0)
		{
			return Spawn(FVector(100.0 + DistanceCm, Y, 86.0));
		}

		/** Submits a Hide command for Pawn (Enter: Arg = BushId; Exit: Arg = side). */
		void SubmitHide(APawn* Pawn, ECommandKind Kind, uint32 Arg, FMailboxId Mailbox = FMailboxId::ServerLocal(), FReplySinkId Sink = FReplySinkId())
		{
			FCommandPayload Cmd;
			Cmd.Pawn.Pawn = Pawn;
			Cmd.Kind = static_cast<uint8>(Kind);
			Cmd.Arg = Arg;
			Cmd.Sink = Sink;
			Cmd.ClientSeq.Seq = NextClientSeq++;
			Q.Queue->Submit(Mailbox, FQueueItem::Make(EQueueItemType::PawnCommand, Q.Epoch.Current, Mailbox, Cmd));
		}

		/** One server frame: the clock, the simulated warps, one drain. */
		void Tick()
		{
			Q.Clock.Advance(FrameSec, 1);
			Pawns.AdvanceWarps(Q.Clock.GetServerTimeSeconds());
			Q.Queue->Drain();
		}

		/** Ticks until Seconds of server time passed. */
		void Run(double Seconds)
		{
			const int32 Frames = FMath::CeilToInt(Seconds / FrameSec - 1e-6);
			for (int32 Index = 0; Index < Frames; ++Index)
			{
				Tick();
			}
		}

		/** Enter, wait for the commit and the end: the pawn ends Concealed in bush 1. */
		void EnterAndSettle(APawn* Pawn)
		{
			SubmitHide(Pawn, ECommandKind::HideEnter, 1);
			Run(Params.Public.BushEnterDurSec + 2.0 * FrameSec);
		}

		EConcealState StateOf(const APawn* Pawn) const { return Core->GetState(Pawn); }
		double Now() const { return Q.Clock.GetServerTimeSeconds(); }
		int32 Count(EConcealmentLogEvent Event) const
		{
			return Q.Log.Entries.FilterByPredicate([Event](const FRoundLogEntry& E) { return E.Type == ERoundLogType::Concealment && E.Code == static_cast<uint16>(Event); }).Num();
		}
	};
}

#endif
