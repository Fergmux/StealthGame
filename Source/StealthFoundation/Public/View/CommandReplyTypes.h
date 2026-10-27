#pragma once

#include "CoreMinimal.h"

#include "CommandReplyTypes.generated.h"

class UPackageMap;

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
 *                        StrikeResult and DefenceResult payloads are ADR-0018's, added by Assassination story asn-015)
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

/** The attacker's owner-only strike result (one shape for every value, ASN-R19). */
UENUM()
enum class EStrikeResult : uint8
{
	RivalEliminated,
	WrongTarget,
	NoContact,
	Reversed,
};

/** Number of EStrikeResult values. */
inline constexpr int32 GStrikeResultCount = 4;

/** The defender's owner-only result (sent only on a reversal by a bound defender). */
UENUM()
enum class EDefenceResult : uint8
{
	Reversed,
};

/**
 * The attacker's owner result (ADR-0018 section 1.3; story asn-015): an entry of the batched ClientCommandReplies, never a
 * property and never a stand-alone RPC; one shape for every Result. TallyAfter (the owner's "Wrong targets" tally,
 * saturating at 255; it rises by one for every WrongTarget whatever the victim kind) and bExiled (true only on the kill
 * that triggers the strike-limit removal) come from Penalty's IStrikeTallySource in the slot; ContactServerTime equals the
 * strike write's (for NoContact the whiff's nominal contact time). It never carries the true strike count. (Declared in
 * Foundation by story asn-004 in View/StealthEncounterWire.h; moved here by story asn-015 so FCommandReply carries it.)
 */
USTRUCT()
struct STEALTHFOUNDATION_API FStrikeResult
{
	GENERATED_BODY()

	UPROPERTY()
	FClientCommandSeq ClientCommandSeq;

	UPROPERTY()
	EStrikeResult Result = EStrikeResult::NoContact;

	UPROPERTY()
	uint8 TallyAfter = 0;

	UPROPERTY()
	bool bExiled = false;

	UPROPERTY()
	double ContactServerTime = -1.0;

	/** Field equality (ContactServerTime by value). */
	bool operator==(const FStrikeResult& Other) const
	{
		return ClientCommandSeq == Other.ClientCommandSeq && Result == Other.Result && TallyAfter == Other.TallyAfter && bExiled == Other.bExiled
			&& ContactServerTime == Other.ContactServerTime;
	}
	bool operator!=(const FStrikeResult& Other) const { return !(*this == Other); }
};

/** The defender's owner result (ADR-0018 section 1.4): no time, no attacker reference. */
USTRUCT()
struct STEALTHFOUNDATION_API FDefenceResult
{
	GENERATED_BODY()

	UPROPERTY()
	EDefenceResult Result = EDefenceResult::Reversed;

	bool operator==(const FDefenceResult& Other) const { return Result == Other.Result; }
	bool operator!=(const FDefenceResult& Other) const { return !(*this == Other); }
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
 * HoldStatus uses Hold, StrikeResult uses Strike (Seq = Strike.ClientCommandSeq), DefenceResult uses Defence (Seq = the
 * defender's Prepare(Begin) it answers; ADR-0018 sections 1.3 and 1.4, story asn-015).
 *
 * Wire (NetSerialize, SerializeBits only): Kind 8 | Seq 48 (PossessionEpoch 16, Seq 32) | the Kind's member only, at a fixed
 * width per Kind: Rejected Code 8 (64 bits); HoldStatus ActionType 8, Outcome 8, Start 64, End 64 (200 bits);
 * StrikeResult Result 8, TallyAfter 8, bExiled 1, ContactServerTime 64 (137 bits, the same for every Result: the size of
 * a result never encodes the outcome, ASN-R19); DefenceResult Result 8 (64 bits). Loading clears the other members.
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

	/** Meaningful for Kind == StrikeResult (its ClientCommandSeq equals Seq). */
	UPROPERTY()
	FStrikeResult Strike;

	/** Meaningful for Kind == DefenceResult. */
	UPROPERTY()
	FDefenceResult Defence;

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

	/** A StrikeResult reply (the attacker's, story asn-015). */
	static FCommandReply MakeStrikeResult(const FStrikeResult& InStrike)
	{
		FCommandReply Reply;
		Reply.Kind = ECommandReplyKind::StrikeResult;
		Reply.Seq = InStrike.ClientCommandSeq;
		Reply.Strike = InStrike;
		return Reply;
	}

	/** A DefenceResult reply for the defender's Prepare(Begin) InSeq (story asn-015). */
	static FCommandReply MakeDefenceResult(const FClientCommandSeq& InSeq, const FDefenceResult& InDefence)
	{
		FCommandReply Reply;
		Reply.Kind = ECommandReplyKind::DefenceResult;
		Reply.Seq = InSeq;
		Reply.Defence = InDefence;
		return Reply;
	}

	/** The serialised width in bits of a reply of Kind (fixed per Kind). */
	static int32 WireBits(ECommandReplyKind InKind);

	/** Writes or reads exactly WireBits(Kind) with SerializeBits. An unknown Kind on load fails (bOutSuccess false). */
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);

	bool operator==(const FCommandReply& Other) const
	{
		return Kind == Other.Kind && Seq == Other.Seq && Code == Other.Code && Hold == Other.Hold && Strike == Other.Strike && Defence == Other.Defence;
	}
	bool operator!=(const FCommandReply& Other) const { return !(*this == Other); }
};

template <>
struct TStructOpsTypeTraits<FCommandReply> : public TStructOpsTypeTraitsBase2<FCommandReply>
{
	enum
	{
		WithNetSerializer = true,
	};
};
