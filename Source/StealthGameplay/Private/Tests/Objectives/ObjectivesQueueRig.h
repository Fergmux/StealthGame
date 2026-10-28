#pragma once

// The slot-level rig of the StealthGame.Objectives.* tests (stories oc-005..oc-009): the REAL resolution queue (Foundation's
// FStealthQueueTestRig, fake clock), a bare game world with its UActionHub and UObjectivesService, the Interact hold stand-in
// (the IIF path of story oc-005) registered for PawnCommand and HoldTimer, Round hooks over the queue (DeclareObjectiveWin
// latches it, the recording Event Log writer), and a strike stub for same-tick ordering cases (an Attack command whose slot
// removes its victim from the Roster). Commands are submitted from the server-local mailbox; one Tick is one server frame.
// Test-only.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesTestRig.h"
#include "Server/ActionHub.h"
#include "Server/Core/InteractHoldStandIn.h"
#include "Server/Core/PawnCommandCore.h"
#include "Server/ObjectivesService.h"
#include "Server/StealthServerTuning.h"
#include "Server/Testing/StealthFaultSinkTestSupport.h"
#include "Server/Testing/StealthQueueTestRig.h"
#include "View/NetPawnCommand.h"

namespace StealthObjectivesTest
{
	/** Round's hooks over the real queue: DeclareObjectiveWin latches it (Round's TryTerminate), the log is the rig's writer. */
	class FQueueRoundHooks : public IRoundObjectiveHooks
	{
	public:
		FStealthQueueTestRig* Q = nullptr;
		bool bActive = true;
		TArray<TPair<uint8, FQueueKey>> Wins;
		/** The queue's slot key at each DeclareObjectiveWin call (unset if called outside a slot). */
		TArray<FQueueKey> SlotKeysAtWin;

		virtual bool IsActive() const override { return bActive; }
		virtual bool IsLatched() const override { return Q->Queue->IsLatched(); }
		virtual bool DeclareObjectiveWin(uint8 Team, FQueueKey Key) override
		{
			Wins.Emplace(Team, Key);
			SlotKeysAtWin.Add(Q->Queue->CurrentKey());
			return Q->Queue->Latch(Key);
		}
		virtual void AppendEvent(const FRoundLogEntry& Entry) override { Q->Log.Append(Entry); }
		virtual FQueueKey CurrentKey() const override { return Q->Queue->CurrentKey(); }
		virtual double ServerNow() const override { return Q->Clock.GetServerTimeSeconds(); }
	};

	/** IPawnCommandQueue over the rig's queue (the hold stand-in schedules its HoldTimer(Complete) here). */
	class FRigCommandQueue : public IPawnCommandQueue
	{
	public:
		FStealthQueueTestRig* Q = nullptr;
		virtual void Submit(FMailboxId Mailbox, const FQueueItem& Item) override { Q->Queue->Submit(Mailbox, Item); }
		virtual void ScheduleAt(double DueServerTime, const FQueueItem& Item) override { Q->Queue->ScheduleAt(DueServerTime, Item); }
	};

	/** PawnCommand: Attack -> the strike stub (removes its victim), every other kind -> the hold stand-in. HoldTimer -> stand-in. */
	class FRigCommandResolver : public IQueueResolver
	{
	public:
		IQueueResolver* Holds = nullptr;
		TFunction<EResolveResult(const FCommandPayload&, FQueueKey)> OnStrike;
		TArray<FQueueKey> StrikeKeys;
		int32 StrikesLatched = 0;

		virtual EResolveResult Resolve(const FQueueItem& Item, FQueueKey Key) override
		{
			const FCommandPayload* Cmd = Item.Type == EQueueItemType::PawnCommand ? Item.Payload.TryGet<FCommandPayload>() : nullptr;
			if (Cmd != nullptr && static_cast<ECommandKind>(Cmd->Kind) == ECommandKind::Attack)
			{
				StrikeKeys.Add(Key);
				return OnStrike ? OnStrike(*Cmd, Key) : EResolveResult::Voided;
			}
			return Holds != nullptr ? Holds->Resolve(Item, Key) : EResolveResult::Voided;
		}
		virtual void OnRejectedLatched(const FQueueItem& Item, FQueueKey Key) override
		{
			const FCommandPayload* Cmd = Item.Type == EQueueItemType::PawnCommand ? Item.Payload.TryGet<FCommandPayload>() : nullptr;
			if (Cmd != nullptr && static_cast<ECommandKind>(Cmd->Kind) == ECommandKind::Attack)
			{
				++StrikesLatched; // Round's Rejected_Latched for a strike (owner reply NotNow)
				return;
			}
			if (Holds != nullptr)
			{
				Holds->OnRejectedLatched(Item, Key);
			}
		}
	};

	/** The full slot-level rig. */
	struct FQueueRig
	{
		FStealthQueueTestRig Q;
		StealthAssassinationTest::FScopedWorld World{ TEXT("ObjectivesQueueRig") };
		FStubAssignment Assignment;
		FStubRoster Roster;
		FStubSession Session;
		FStubPawnControl PawnControl;
		FQueueRoundHooks Round;
		FStubCourts Courts;
		FStubViews Views;
		FStubViewTargets ViewTargets;
		FStubRival Rival;
		FStubChains Chains;
		/** The Init the service was bound with (tests may change it and Bind again). */
		FObjectivesServiceInit Init;
		FRigCommandQueue CommandQueue;
		FRigCommandResolver Resolver;
		/** Round's DebugItem resolver stand-in: CommandDebugObjectiveStage -> the service's ApplyDebugStage in its own slot. */
		FStealthStubResolver DebugResolver;
		TArray<FQueueKey> DebugKeys;
		FStealthRecordingFaultSink HubFaults;
		TUniquePtr<FInteractHoldStandIn> Holds;
		UActionHub* Hub = nullptr;
		UObjectivesService* Service = nullptr;
		TStrongObjectPtr<UObjectiveChainData> Chain;
		uint32 NextTag = 1;

		FQueueRig()
		{
			Round.Q = &Q;
			CommandQueue.Q = &Q;
			Hub = UActionHub::Get(World.World);
			Service = UObjectivesService::Get(World.World);
			check(Hub != nullptr && Service != nullptr);
			Hub->SetFaultSink(&HubFaults);
			Chain.Reset(MakeCacaoChain());
			Chains.Chains = { Chain.Get() };

			Init.Roster = &Roster;
			Init.Session = &Session;
			Init.PawnControl = &PawnControl;
			Init.Round = &Round;
			Init.Courts = &Courts;
			Init.Assignment = &Assignment;
			Init.Chains = &Chains;
			Init.Views = &Views;
			Init.ViewTargets = &ViewTargets;
			Init.RivalRelease = &Rival;
			Init.Queue = Q.Queue.Get();
			Init.Faults = &Q.Faults;
			Init.SubmitDebugStage = [this](uint8 Team)
			{
				FDebugPayload Payload;
				Payload.Command = 3; // FRoundDebugResolver::CommandDebugObjectiveStage
				Payload.Arg = Team;
				Q.Queue->Submit(FMailboxId::ServerLocal(), FQueueItem::Make(EQueueItemType::DebugItem, Q.Epoch.Current, FMailboxId::ServerLocal(), Payload));
				return true;
			};
			Service->Bind(Init);
			Hub->RegisterPredicate(EActionType::Interact, TWeakInterfacePtr<ICommandPredicate>(Service));
			Hub->RegisterInteractTargetOwner(TWeakInterfacePtr<IInteractTargetOwner>(Service));

			FInteractHoldStandIn::FDeps Deps;
			Deps.Hub = Hub;
			Deps.Queue = &CommandQueue;
			Deps.Clock = &Q.Clock;
			Deps.IsLegalityActive = [this](const APawn& Pawn) { return Roster.GetLegalityClass(Pawn) == EObjectivesLegalityClass::Active; };
			Holds = MakeUnique<FInteractHoldStandIn>(Deps);
			Resolver.Holds = Holds.Get();
			Q.Queue->RegisterResolver(EQueueItemType::PawnCommand, &Resolver);
			Q.Queue->RegisterResolver(EQueueItemType::HoldTimer, &Resolver);
			DebugResolver.OnResolve = [this](const FQueueItem& Item, FQueueKey Key)
			{
				const FDebugPayload& Payload = Item.Payload.Get<FDebugPayload>();
				DebugKeys.Add(Key);
				return Payload.Command == 3 && Service->ApplyDebugStage(static_cast<uint8>(Payload.Arg), Key) ? EResolveResult::Executed : EResolveResult::Voided;
			};
			Q.Queue->RegisterResolver(EQueueItemType::DebugItem, &DebugResolver);
		}

		~FQueueRig()
		{
			Q.Queue->ClearRegistrations();
			if (Service != nullptr)
			{
				Service->Unbind();
			}
			if (Hub != nullptr)
			{
				Hub->SetFaultSink(nullptr);
			}
		}

		/** Preparing -> OnLayoutReady -> OnRoundActive through the service. */
		bool ToActive()
		{
			return Service->ValidateSelectedChain(FStealthServerTuning()).bValid && Service->HandleLayoutReady(1, 0)
				&& Service->HandleRoundActive(Q.Epoch.Current, Q.Clock.GetServerTimeSeconds());
		}

		const FObjectivesCore& Core() const { return Service->GetCore(); }

		/** A pawn at Court's face, bound to Participant on Team (Participant 0: an unbound crowd pawn). */
		AStealthPawn* Pawn(uint16 Court, int32 Face, uint32 Participant = 0, int32 Team = 0)
		{
			AStealthPawn* P = FCoreRig::PawnAt(World, Court, Face);
			if (P != nullptr && Participant != 0)
			{
				Roster.Bind(P, Participant, Team);
			}
			return P;
		}

		FQueueItem Command(APawn* Pawn, ECommandKind Kind, uint16 Court = 0, APawn* Target = nullptr)
		{
			FCommandPayload P;
			P.Pawn.Pawn = Pawn;
			P.Kind = static_cast<uint8>(Kind);
			P.Arg = NextTag++;
			if (Court != 0)
			{
				P.Target = FSubjectRef::ForCourtSocket(Court);
			}
			if (Target != nullptr)
			{
				FPawnHandle Handle;
				Handle.Pawn = Target;
				P.Target = FSubjectRef::ForPawn(Handle);
			}
			return FQueueItem::Make(EQueueItemType::PawnCommand, Q.Epoch.Current, FMailboxId::ServerLocal(), P);
		}

		/** Submits Interact(Begin) for Pawn at Court (resolved at the next drain). */
		void Begin(APawn* Pawn, uint16 Court) { Q.Queue->Submit(FMailboxId::ServerLocal(), Command(Pawn, ECommandKind::InteractBegin, Court)); }
		void End(APawn* Pawn) { Q.Queue->Submit(FMailboxId::ServerLocal(), Command(Pawn, ECommandKind::InteractEnd)); }
		void Strike(APawn* Attacker, APawn* Victim) { Q.Queue->Submit(FMailboxId::ServerLocal(), Command(Attacker, ECommandKind::Attack, 0, Victim)); }

		/** Frames of Delta seconds until Seconds elapsed (each drains once). */
		void Run(double Seconds, double Delta = 0.1)
		{
			const int32 Frames = FMath::CeilToInt(Seconds / Delta);
			for (int32 Frame = 0; Frame < Frames; ++Frame)
			{
				Q.Tick(Delta);
			}
		}

		/** Ticks (0.1 s frames) until the NEXT frame reaches DueServerTime: items submitted then resolve in the due frame's drain. */
		void RunToFrameBefore(double DueServerTime)
		{
			while (Q.Clock.GetServerTimeSeconds() + 0.1 < DueServerTime - 0.0005)
			{
				Q.Tick(0.1);
			}
		}

		int32 Logged(ERoundLogType Type) const { return Q.Log.CountOfType(Type); }
		TArray<FRoundLogEntry> LoggedOf(ERoundLogType Type) const
		{
			return Q.Log.Entries.FilterByPredicate([Type](const FRoundLogEntry& E) { return E.Type == Type; });
		}

		/** Completes Team's current stage through the debug path's shared effect (fast set-up; ApplyCountedCompletion). */
		void CompleteStageFast(uint8 Team) { Service->ApplyDebugStage(Team, FQueueKey(1, 1)); }
	};
}

#endif
