#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Misc/TVariant.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "View/StealthQueueTypes.h"

/**
 * Queue payload types (ADR-0002 section 2a): every type that crosses the queue boundary is declared here,
 * in Foundation Public/Server, never in a feature module (ADR-0007 forbids Foundation from naming a
 * StealthGameplay type). Feature ADRs own the MEANING of the fields they fill, not the structs.
 *
 * Every field is an id, an epoch, a time, a small enum or a raw uint8/uint16/uint32 whose meaning the named
 * owner defines; none holds a raw UObject* or a feature type. Server-only: never replicated, never logged as
 * text. Adding an item type, a timer kind, a staged-write type or a payload field is an ADR-0002 amendment.
 *
 * Built by the Round & Match Framework Exit A stories rmf-004 (item types), rmf-008 (results) and rmf-010
 * (epoch token on every item).
 */

// ---------------------------------------------------------------------------------------------------------
// Closed enums
// ---------------------------------------------------------------------------------------------------------

/** The closed item-type set (ADR-0002 section 1). DebugItem's producers and resolver are compiled out of Shipping. */
enum class EQueueItemType : uint8
{
	PawnCommand,
	HoldTimer,
	TimerFired,
	PawnLost,
	ConnectionTransition,
	HostCommand,
	DebugItem,
};

/** Number of EQueueItemType values (registration tables are sized by it). */
inline constexpr int32 GQueueItemTypeCount = 7;

/** TimerFired subkinds (ADR-0002 section 1 and the section 2a mapping table). */
enum class ETimerKind : uint8
{
	GraceExpiry,
	InformantRespawn,
	StageSignal,
	DefenceWindowEnd,
	PawnLossRetry,
	CrowdSchedule,
	BodyRelease,
	BodyRemoval,
	/** Post-latch exemption (ADR-0002 section 4). */
	ConcealmentTransition,
	/** Post-latch lane (ADR-0002 section 4). */
	EndedMinDwell,
	/** Post-latch lane (ADR-0002 section 4). */
	EndedAutoReset,
};

/** Number of ETimerKind values. */
inline constexpr int32 GTimerKindCount = 11;

/** HoldTimer subkinds (ADR-0002 section 1; ADR-0011 owns their meaning). */
enum class EHoldTimerKind : uint8
{
	Complete,
	CapEnd,
	DebounceExpiry,
	ConditionCancel,
};

/** A resolver's answer (ADR-0002 section 5). */
enum class EResolveResult : uint8
{
	Executed,
	Voided,
};

/** Why an item changed nothing (ADR-0002 Key Interfaces); the owner-specific code travels in the log entry. */
enum class EVoidReason : uint8
{
	Rejected_Latched,
	Rejected_StaleEpoch,
	Dropped_StaleConnection,
	Revalidation,
};

/** The closed staged-write type set (ADR-0002 section 6). */
enum class EStagedWriteType : uint8
{
	DeathWrite,
	CounterPose,
	RoundEndStep,
};

// ---------------------------------------------------------------------------------------------------------
// References
// ---------------------------------------------------------------------------------------------------------

/**
 * The one server-only pawn reference in payloads (ADR-0002 section 2a). Its generation is the weak pointer's
 * own object serial number, so a destroyed pawn or a reused object slot never resolves. Typed APawn (engine),
 * never AStealthPawn. Never replicated, never logged as text.
 */
struct FPawnHandle
{
	TWeakObjectPtr<APawn> Pawn;

	bool operator==(const FPawnHandle& Other) const { return Pawn == Other.Pawn; }
	bool operator!=(const FPawnHandle& Other) const { return !(*this == Other); }
};

/** Which member of FSubjectRef is meaningful. */
enum class ESubjectRefKind : uint8
{
	None,
	Pawn,
	Participant,
	CourtSocket,
};

/**
 * A tagged subject reference (ADR-0002 section 2a): a pawn, a participant or a court fixture (the raw LSC
 * socket id; the typed FCourtFixtureId stays with ADR-0014 and converts at the boundary).
 */
struct FSubjectRef
{
	ESubjectRefKind Kind = ESubjectRefKind::None;
	FPawnHandle Pawn;
	FParticipantId Participant;
	uint16 CourtSocketId = 0;

	static FSubjectRef ForPawn(const FPawnHandle& InPawn) { FSubjectRef Ref; Ref.Kind = ESubjectRefKind::Pawn; Ref.Pawn = InPawn; return Ref; }
	static FSubjectRef ForParticipant(FParticipantId InParticipant) { FSubjectRef Ref; Ref.Kind = ESubjectRefKind::Participant; Ref.Participant = InParticipant; return Ref; }
	static FSubjectRef ForCourtSocket(uint16 InSocketId) { FSubjectRef Ref; Ref.Kind = ESubjectRefKind::CourtSocket; Ref.CourtSocketId = InSocketId; return Ref; }
};

/**
 * A slot of the IIF server reply-sink table (ADR-0002 section 2a, Technical Setup 2026-10-25 N1; ADR-0011 section
 * 8 owns the table). Index 0 is the null sink (AI and server-local sources); Generation guards a reused slot.
 */
struct FReplySinkId
{
	uint16 Index = 0;
	uint16 Generation = 0;
};

// ---------------------------------------------------------------------------------------------------------
// Item payloads (one per item type)
// ---------------------------------------------------------------------------------------------------------

/** PawnCommand (ADR-0011 owns Kind, Phase and Arg). */
struct FCommandPayload
{
	FPawnHandle Pawn;
	uint32 ControlEpoch = 0;
	FClientCommandSeq ClientSeq;
	FReplySinkId Sink;
	uint8 Kind = 0;
	uint8 Phase = 0;
	FSubjectRef Target;
	FVector WorldPoint = FVector::ZeroVector;
	uint32 Arg = 0;
};

/** HoldTimer (ADR-0011 owns ActionType). BeginKey orders equal-DueTime holds (IIF R8). */
struct FHoldTimerPayload
{
	FPawnHandle Pawn;
	EHoldTimerKind Kind = EHoldTimerKind::Complete;
	uint8 ActionType = 0;
	uint32 ActionEpoch = 0;
	FQueueKey BeginKey;
};

/** TimerFired: one generic fixed shape for every ETimerKind (mapping table in ADR-0002 section 2a). */
struct FTimerPayload
{
	ETimerKind Kind = ETimerKind::GraceExpiry;
	FSubjectRef Subject;
	uint32 Generation = 0;
	uint32 Seq = 0;
	uint8 Phase = 0;
};

/** PawnLost (Round R17.9). */
struct FPawnLostPayload
{
	FParticipantId Participant;
	FPawnHandle Pawn;
};

/** ConnectionTransition (Session/Round own Transition). */
struct FConnectionTransitionPayload
{
	FParticipantId Participant;
	uint32 ConnectionGeneration = 0;
	uint8 Transition = 0;
};

/** HostCommand: ResetRound or AbortRound (Round owns the values); parked while latched (ADR-0002 section 8.1). */
struct FHostCommandPayload
{
	uint8 Command = 0;
};

/** DebugItem: declared in every configuration so the variant layout never differs; Round owns Command. */
struct FDebugPayload
{
	uint8 Command = 0;
	FSubjectRef Subject;
	uint32 Arg = 0;
};

/** One queue item (ADR-0002 section 2a): ID-only, with the epoch token every kind carries. */
struct FQueueItem
{
	using FPayload = TVariant<FCommandPayload, FHoldTimerPayload, FTimerPayload, FPawnLostPayload,
		FConnectionTransitionPayload, FHostCommandPayload, FDebugPayload>;

	EQueueItemType Type = EQueueItemType::PawnCommand;
	FEpochToken Expected;
	FMailboxId Source;
	FPayload Payload;

	/** Builds an item whose Type matches its payload. */
	template <typename TPayload>
	static FQueueItem Make(EQueueItemType InType, const FEpochToken& InExpected, FMailboxId InSource, const TPayload& InPayload)
	{
		FQueueItem Item;
		Item.Type = InType;
		Item.Expected = InExpected;
		Item.Source = InSource;
		Item.Payload.Set<TPayload>(InPayload);
		return Item;
	}
};

// ---------------------------------------------------------------------------------------------------------
// Staged writes (ADR-0002 section 6; the staging pass is a later Exit A story)
// ---------------------------------------------------------------------------------------------------------

/** DeathWrite (meaning, times and applier: ADR-0018). */
struct FDeathWritePayload
{
	FPawnHandle Victim;
	double DeathServerTime = 0.0;
};

/** CounterPose (meaning, times and applier: ADR-0018). */
struct FCounterPayload
{
	FPawnHandle Defender;
	double ContactServerTime = 0.0;
};

/** RoundEndStep (Round, ADR-0002 section 8): routed to ITerminalDecision, never to a registered applier. */
struct FRoundEndPayload
{
	/** The deciding slot's key (the latch's TerminalKey). */
	FQueueKey TerminalKey;
};

/** One typed staged write; CauseKey is the staging slot's RootCauseKey. */
struct FStagedWrite
{
	using FPayload = TVariant<FDeathWritePayload, FCounterPayload, FRoundEndPayload>;

	EStagedWriteType Type = EStagedWriteType::DeathWrite;
	double ServerTime = 0.0;
	FEpochToken Expected;
	FQueueKey CauseKey;
	FPayload Payload;
};
