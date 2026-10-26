#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "Misc/NetworkGuid.h"
#include "View/CommandReplyTypes.h"
#include "View/CourtFixtureId.h"

#include "NetPawnCommand.generated.h"

/**
 * The owner-to-server discrete command of the pawn command interface (ADR-0011 section 2; story iif-001).
 * It travels only in AStealthPlayerController::ServerPawnCommand; Move and Look never do (they ride ServerMove).
 *
 * Placement decision (story iif-001, 2026-10-26): Foundation Public/View, not Public/Server. The owning client builds
 * it (UPawnCommandClient) and the root controller's RPC carries it; it holds ids and quantised values only, so it is
 * safe on both sides. The client never names the pawn it commands: the server resolves the controller's own pawn.
 *
 * Confirmed ECommandKind set: the IIF R1 discrete entry points, Move and Look excluded (they are continuous):
 *   { Traverse, InteractBegin, InteractEnd, PrepareBegin, PrepareEnd, Attack, HideEnter, HideExit }
 */

/** The discrete command kinds that cross the uplink (ADR-0011 section 2). */
UENUM()
enum class ECommandKind : uint8
{
	Traverse,
	InteractBegin,
	InteractEnd,
	PrepareBegin,
	PrepareEnd,
	Attack,
	HideEnter,
	HideExit,
};

/** Number of ECommandKind values (WithValidation rejects anything at or above it). */
inline constexpr int32 GCommandKindCount = 8;

/** True for the kinds that open a hold (their End must never be dropped while the Begin is outstanding). */
inline constexpr bool IsHoldBeginKind(ECommandKind Kind) { return Kind == ECommandKind::InteractBegin || Kind == ECommandKind::PrepareBegin; }

/** True for the kinds that close a hold. */
inline constexpr bool IsHoldEndKind(ECommandKind Kind) { return Kind == ECommandKind::InteractEnd || Kind == ECommandKind::PrepareEnd; }

/**
 * The arguments of one command: ids and quantised values only (ADR-0011 section 2). Which fields are meaningful
 * depends on the kind: InteractBegin uses CourtFixtureId; HideEnter BushId; HideExit SideIndex (0-3); Attack
 * WorldPoint and optionally PawnRefGuidValue; Traverse TraversalPointHint and ClientTimestamp.
 *
 * PawnRefGuidValue is the raw value of an FNetworkGUID (FNetworkGUID::ObjectId), never an object-pointer field: the
 * server resolves it through the NetDriver's GUID cache (E7, spike V3). FNetworkGUID itself is not a reflected type,
 * so the value travels as a uint64 property; GetPawnRefGuid/SetPawnRefGuid convert.
 */
USTRUCT()
struct STEALTHFOUNDATION_API FNetCommandArgs
{
	GENERATED_BODY()

	UPROPERTY()
	FCourtFixtureId CourtFixtureId;

	UPROPERTY()
	uint16 BushId = 0;

	/** 2 bits used (0-3); WithValidation rejects larger values. */
	UPROPERTY()
	uint8 SideIndex = 0;

	UPROPERTY()
	FVector_NetQuantize10 WorldPoint = FVector_NetQuantize10(FVector::ZeroVector);

	/** FNetworkGUID::ObjectId of the aimed pawn; 0 = no PawnRef. */
	UPROPERTY()
	uint64 PawnRefGuidValue = 0;

	/** A Level Socket Contract traversal point id (0 = none). */
	UPROPERTY()
	uint16 TraversalPointHint = 0;

	/** The owner's CMC client timestamp, Traverse only (ADR-0010 section 7). */
	UPROPERTY()
	float ClientTimestamp = 0.f;

	/** The PawnRef as a network GUID (invalid when none was sent). */
	FNetworkGUID GetPawnRefGuid() const
	{
		FNetworkGUID Guid;
		Guid.ObjectId = PawnRefGuidValue;
		return Guid;
	}

	void SetPawnRefGuid(const FNetworkGUID& Guid) { PawnRefGuidValue = Guid.ObjectId; }

	bool operator==(const FNetCommandArgs& Other) const
	{
		return CourtFixtureId == Other.CourtFixtureId && BushId == Other.BushId && SideIndex == Other.SideIndex && WorldPoint == Other.WorldPoint
			&& PawnRefGuidValue == Other.PawnRefGuidValue && TraversalPointHint == Other.TraversalPointHint && ClientTimestamp == Other.ClientTimestamp;
	}
	bool operator!=(const FNetCommandArgs& Other) const { return !(*this == Other); }
};

/** One discrete command on the wire: `{ Kind, Seq, Args }` (ADR-0011 section 2). */
USTRUCT()
struct STEALTHFOUNDATION_API FNetPawnCommand
{
	GENERATED_BODY()

	UPROPERTY()
	ECommandKind Kind = ECommandKind::Traverse;

	UPROPERTY()
	FClientCommandSeq Seq;

	UPROPERTY()
	FNetCommandArgs Args;

	/**
	 * Structural validity only (the WithValidation rule, ADR-0011 section 2): the kind is in range, SideIndex fits 2 bits
	 * and every float is finite. Everything else is ingress's silent drop.
	 */
	bool IsStructurallyValid() const
	{
		return static_cast<int32>(Kind) < GCommandKindCount && Args.SideIndex < 4 && !Args.WorldPoint.ContainsNaN()
			&& FMath::IsFinite(Args.WorldPoint.X) && FMath::IsFinite(Args.WorldPoint.Y) && FMath::IsFinite(Args.WorldPoint.Z) && FMath::IsFinite(Args.ClientTimestamp);
	}

	bool operator==(const FNetPawnCommand& Other) const { return Kind == Other.Kind && Seq == Other.Seq && Args == Other.Args; }
	bool operator!=(const FNetPawnCommand& Other) const { return !(*this == Other); }
};
