#pragma once

#include "CoreMinimal.h"

#include "CommandReplyTypes.generated.h"

/**
 * Owner-visible command reply data of the pawn command interface (ADR-0011 Key Interfaces and section 7; story
 * iif-001). Lives in Foundation Public/View so Presentation reads the replies without including the root
 * PlayerController. Every enum is a closed uint8 set with contiguous values from 0; adding a value is an ADR-0011
 * amendment (the shape test StealthGame.Input.CommandReplyTypes fails on any change).
 *
 * Confirmed value sets (resolving the QL-STORY-READY gap of story iif-001, 2026-10-26):
 *   ECommandRejectCode = { NotNow, Cooldown, Busy, OutOfReach }            (IIF R6)
 *   EHoldOutcome       = { Active, Completed, Cancelled }                   (no cancel reason: section 7)
 *   EActionType        = { Interact, Prepare, Attack, HideEnter, HideExit } (section 6)
 *   ECommandReplyKind  = { Rejected, HoldStatus, StrikeResult, DefenceResult } (the FCommandReply tag; the
 *                        StrikeResult and DefenceResult payloads are ADR-0018's and are added by the assassination epic)
 * No struct here holds a UObject pointer, a cancel reason or any caller identity (ADR-0011 section 1).
 */

/** Why a resolution-voided command was rejected (IIF R6). Computed only from facts the owner already knows. */
UENUM()
enum class ECommandRejectCode : uint8
{
	NotNow,
	Cooldown,
	Busy,
	OutOfReach,
};

/** Number of ECommandRejectCode values. */
inline constexpr int32 GCommandRejectCodeCount = 4;

/** The owner-visible outcome of a hold (ADR-0011 section 7). Deliberately carries no cancel reason. */
UENUM()
enum class EHoldOutcome : uint8
{
	Active,
	Completed,
	Cancelled,
};

/** Number of EHoldOutcome values. */
inline constexpr int32 GHoldOutcomeCount = 3;

/** The action a slot runs (ADR-0011 section 6); the action hub's events name it. */
UENUM()
enum class EActionType : uint8
{
	Interact,
	Prepare,
	Attack,
	HideEnter,
	HideExit,
};

/** Number of EActionType values (predicate tables are sized by it). */
inline constexpr int32 GActionTypeCount = 5;

/** The tag of one FCommandReply entry (ADR-0011 section 8: one batch carries every owner reply kind). */
UENUM()
enum class ECommandReplyKind : uint8
{
	Rejected,
	HoldStatus,
	StrikeResult,
	DefenceResult,
};

/** Number of ECommandReplyKind values. */
inline constexpr int32 GCommandReplyKindCount = 4;

/**
 * The client command sequence `(PossessionEpoch, Seq)` (ADR-0011 section 2). Seq is strictly increasing per
 * controller; PossessionEpoch counts the controller's possessions and is owned by the server, which delivers it with
 * ClientSetPossessionEpoch before ClientRestart. A USTRUCT because it travels in ServerPawnCommand and the reply
 * batch; it was a plain struct in View/StealthQueueTypes.h until story iif-001 (that header now includes this one).
 */
USTRUCT()
struct STEALTHFOUNDATION_API FClientCommandSeq
{
	GENERATED_BODY()

	UPROPERTY()
	uint16 PossessionEpoch = 0;

	UPROPERTY()
	uint32 Seq = 0;

	constexpr bool operator==(const FClientCommandSeq& Other) const { return PossessionEpoch == Other.PossessionEpoch && Seq == Other.Seq; }
	constexpr bool operator!=(const FClientCommandSeq& Other) const { return !(*this == Other); }
};

/** The owner-only status of a hold (ADR-0011 section 7). No cancel reason: observers learn a cancel only from the pose. */
USTRUCT()
struct STEALTHFOUNDATION_API FHoldStatus
{
	GENERATED_BODY()

	/** The Begin command this status answers. */
	UPROPERTY()
	FClientCommandSeq Seq;

	UPROPERTY()
	EActionType ActionType = EActionType::Interact;

	UPROPERTY()
	EHoldOutcome Outcome = EHoldOutcome::Active;

	/** Server time the hold executed (ServerNow at the Begin's slot). */
	UPROPERTY()
	double ServerStartTime = 0.0;

	/** Server time the hold ended (0 while Active). */
	UPROPERTY()
	double ServerEndTime = 0.0;

	bool operator==(const FHoldStatus& Other) const
	{
		return Seq == Other.Seq && ActionType == Other.ActionType && Outcome == Other.Outcome && ServerStartTime == Other.ServerStartTime
			&& ServerEndTime == Other.ServerEndTime;
	}
	bool operator!=(const FHoldStatus& Other) const { return !(*this == Other); }
};

/**
 * One tagged owner reply (ADR-0011 section 8). Kind selects the meaningful member: Rejected uses Seq and Code,
 * HoldStatus uses Hold. StrikeResult and DefenceResult are reserved tags whose payload fields belong to ADR-0018.
 */
USTRUCT()
struct STEALTHFOUNDATION_API FCommandReply
{
	GENERATED_BODY()

	UPROPERTY()
	ECommandReplyKind Kind = ECommandReplyKind::Rejected;

	/** The command this reply answers. */
	UPROPERTY()
	FClientCommandSeq Seq;

	/** Meaningful for Kind == Rejected. */
	UPROPERTY()
	ECommandRejectCode Code = ECommandRejectCode::NotNow;

	/** Meaningful for Kind == HoldStatus. */
	UPROPERTY()
	FHoldStatus Hold;

	/** A Rejected reply for Seq. */
	static FCommandReply MakeRejected(const FClientCommandSeq& InSeq, ECommandRejectCode InCode)
	{
		FCommandReply Reply;
		Reply.Kind = ECommandReplyKind::Rejected;
		Reply.Seq = InSeq;
		Reply.Code = InCode;
		return Reply;
	}

	/** A HoldStatus reply. */
	static FCommandReply MakeHoldStatus(const FHoldStatus& InHold)
	{
		FCommandReply Reply;
		Reply.Kind = ECommandReplyKind::HoldStatus;
		Reply.Seq = InHold.Seq;
		Reply.Hold = InHold;
		return Reply;
	}

	bool operator==(const FCommandReply& Other) const { return Kind == Other.Kind && Seq == Other.Seq && Code == Other.Code && Hold == Other.Hold; }
	bool operator!=(const FCommandReply& Other) const { return !(*this == Other); }
};
