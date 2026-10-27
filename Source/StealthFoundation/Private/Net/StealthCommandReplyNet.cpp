// Story asn-015 (production/epics/assassination-defence-reversal/story-015-owner-results-reply-entries-and-tally.md):
// the fixed-width serialiser of FCommandReply, the entry of the batched ClientCommandReplies (ADR-0011 section 8,
// ADR-0018 sections 1.3 and 1.4). Every Kind has one width; a StrikeResult entry is 137 bits for every Result, so the
// size of an owner result never encodes the outcome (ASN-R19, AC-ASN-16 payload clause). SerializeBits only, as the
// encounter wire structs (story asn-004).

#include "View/CommandReplyTypes.h"

namespace
{
	constexpr int32 KindBits = 8;
	constexpr int32 SeqBits = 16 + 32;

	void SerializeReplyDouble(FArchive& Ar, double& Value)
	{
		Ar.SerializeBits(&Value, 64);
	}

	void SerializeSeq(FArchive& Ar, FClientCommandSeq& Seq)
	{
		Ar.SerializeBits(&Seq.PossessionEpoch, 16);
		Ar.SerializeBits(&Seq.Seq, 32);
	}

	/** One closed-enum byte: written as is, read back clamped into the set (the width never changes). */
	template <typename TEnum>
	void SerializeEnum(FArchive& Ar, TEnum& Value, int32 Count, TEnum Fallback)
	{
		uint8 Byte = static_cast<uint8>(Value);
		Ar.SerializeBits(&Byte, 8);
		if (Ar.IsLoading())
		{
			Value = Byte < Count ? static_cast<TEnum>(Byte) : Fallback;
		}
	}
}

int32 FCommandReply::WireBits(ECommandReplyKind InKind)
{
	switch (InKind)
	{
	case ECommandReplyKind::Rejected: return KindBits + SeqBits + 8;
	case ECommandReplyKind::HoldStatus: return KindBits + SeqBits + 8 + 8 + 64 + 64;
	case ECommandReplyKind::StrikeResult: return KindBits + SeqBits + 8 + 8 + 1 + 64;
	case ECommandReplyKind::DefenceResult: return KindBits + SeqBits + 8;
	default: return KindBits + SeqBits;
	}
}

bool FCommandReply::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	uint8 KindByte = static_cast<uint8>(Kind);
	Ar.SerializeBits(&KindByte, KindBits);
	if (Ar.IsLoading())
	{
		if (KindByte >= GCommandReplyKindCount)
		{
			bOutSuccess = false;
			return true;
		}
		*this = FCommandReply();
		Kind = static_cast<ECommandReplyKind>(KindByte);
	}
	SerializeSeq(Ar, Seq);
	switch (Kind)
	{
	case ECommandReplyKind::Rejected:
		SerializeEnum(Ar, Code, GCommandRejectCodeCount, ECommandRejectCode::NotNow);
		break;
	case ECommandReplyKind::HoldStatus:
		if (Ar.IsLoading())
		{
			Hold.Seq = Seq;
		}
		SerializeEnum(Ar, Hold.ActionType, GActionTypeCount, EActionType::Interact);
		SerializeEnum(Ar, Hold.Outcome, GHoldOutcomeCount, EHoldOutcome::Active);
		SerializeReplyDouble(Ar, Hold.ServerStartTime);
		SerializeReplyDouble(Ar, Hold.ServerEndTime);
		break;
	case ECommandReplyKind::StrikeResult:
	{
		if (Ar.IsLoading())
		{
			Strike.ClientCommandSeq = Seq;
		}
		SerializeEnum(Ar, Strike.Result, GStrikeResultCount, EStrikeResult::NoContact);
		Ar.SerializeBits(&Strike.TallyAfter, 8);
		uint8 Exiled = Strike.bExiled ? 1 : 0;
		Ar.SerializeBits(&Exiled, 1);
		if (Ar.IsLoading())
		{
			Strike.bExiled = (Exiled & 1) != 0;
		}
		SerializeReplyDouble(Ar, Strike.ContactServerTime);
		break;
	}
	case ECommandReplyKind::DefenceResult:
		SerializeEnum(Ar, Defence.Result, 1, EDefenceResult::Reversed);
		break;
	default:
		break;
	}
	bOutSuccess = !Ar.IsError();
	return true;
}
