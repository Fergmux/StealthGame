// Story asn-015 (production/epics/assassination-defence-reversal/story-015-owner-results-reply-entries-and-tally.md), AC2 /
// AC-ASN-16 payload clause (data half, headless; the PIE half is StealthGame.Assassination.PIE.OwnerResultShape), with
// the QA rewrite: each strike runs in a fresh resolver at the same scripted slot time, an attacker on reply sink 3, a
// bystander on sink 9 and (reversal) a bound defender on sink 4. Strikes that kill an Active rival, an NPC and an
// informant, a whiff and a reversed strike:
//   - every FStrikeResult goes to the attacker's sink only, once, as an FCommandReply entry of the same serialised size;
//   - the NPC and informant kills' entries are identical apart from ClientCommandSeq and TallyAfter (bit for bit once those
//     two are equalised), and TallyAfter rises by exactly 1 for each of them (unchanged for the rival kill, the whiff and
//     the reversal);
//   - the bystander's sink receives nothing; the bound defender receives only FDefenceResult {Reversed} answering its
//     Prepare's seq; an NPC defender (null sink) produces no FDefenceResult anywhere;
//   - a Rejected_Latched Attack sends no FStrikeResult (story asn-009, AC-ASN-19 c);
//   - the FCommandReply wire round-trips every Kind at its fixed width.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Tests/Assassination/EncounterTestRig.h"

namespace OwnerResultShapeTestData
{
	constexpr uint16 AttackerSink = 3;
	constexpr uint16 DefenderSink = 4;
	constexpr uint16 BystanderSink = 9;
	constexpr int32 TallyBefore = 1;

	enum class ERun : uint8 { Rival, Npc, Informant, Whiff, Reversal, NpcDefenderReversal, Count };
	const TCHAR* const RunNames[] = { TEXT("rival kill"), TEXT("NPC kill"), TEXT("informant kill"), TEXT("whiff"), TEXT("reversal"), TEXT("NPC-defender reversal") };

	TArray<uint8> Bytes(const FCommandReply& Reply, int64& OutBits)
	{
		FBitWriter Writer(0, true);
		FCommandReply Copy = Reply;
		bool bSuccess = false;
		Copy.NetSerialize(Writer, nullptr, bSuccess);
		OutBits = Writer.GetNumBits();
		return *Writer.GetBuffer();
	}

	FCommandReply RoundTrip(const FCommandReply& Reply, int64& OutBits, bool& bOutSuccess)
	{
		FBitWriter Writer(0, true);
		FCommandReply Copy = Reply;
		Copy.NetSerialize(Writer, nullptr, bOutSuccess);
		OutBits = Writer.GetNumBits();
		FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
		FCommandReply Out;
		Out.NetSerialize(Reader, nullptr, bOutSuccess);
		return Out;
	}

	struct FRunResult
	{
		TArray<FCommandReply> AttackerEntries;
		TArray<FCommandReply> DefenderEntries;
		int32 BystanderEntries = 0;
		int32 DefenceResultsAnywhere = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationOwnerResultShapeTest, "StealthGame.Assassination.OwnerResultShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationOwnerResultShapeTest::RunTest(const FString& Parameters)
{
	using namespace StealthEncounterTest;
	using namespace OwnerResultShapeTestData;

	FRunResult Runs[static_cast<int32>(ERun::Count)];
	for (int32 Index = 0; Index < static_cast<int32>(ERun::Count); ++Index)
	{
		const ERun Run = static_cast<ERun>(Index);
		FRig R(TEXT("OwnerResultShapeWorld"));
		int32 Tally = TallyBefore;
		R.Core->OnPawnKilled.AddLambda([&Tally](APawn*, APawn*, EVictimKind Kind, EKillCause Cause)
		{
			Tally += Cause == EKillCause::Strike && Kind != EVictimKind::Player ? 1 : 0; // the owner tally: every WrongTarget
		});
		R.Tally.Answer = [&Tally](const APawn*, EStrikeResult) { FStrikeTally Out; Out.TallyAfter = static_cast<uint8>(Tally); return Out; };

		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		R.Spawn(FVector(-4000.0, 0.0, 0.0), 0); // the bystander (its sink is never named by any command)
		const FVector Spot(150.0, 0.0, 0.0);
		APawn* Target = nullptr;
		switch (Run)
		{
		case ERun::Rival: Target = R.Spawn(Spot, 1); break;
		case ERun::Npc: Target = R.Spawn(Spot); break;
		case ERun::Informant:
			Target = R.Spawn(Spot, 1);
			R.Roster.Members[Target].State = EParticipantState::Informant;
			break;
		case ERun::Reversal:
			Target = R.Spawn(Spot, 1);
			R.BraceFrom(Target, DefenderSink, 77);
			break;
		case ERun::NpcDefenderReversal:
			Target = R.Spawn(Spot);
			R.BraceFrom(Target, 0, 0);
			break;
		default: break;
		}
		// The same slot time in every run (a fresh rig; the brace and the strike share the tick, ordered by key).
		R.AttackFrom(A, Spot, Target, AttackerSink, 10 + Index);
		FRunResult& Out = Runs[Index];
		Out.AttackerEntries = R.Replies.For(AttackerSink);
		Out.DefenderEntries = R.Replies.For(DefenderSink);
		Out.BystanderEntries = R.Replies.For(BystanderSink).Num();
		Out.DefenceResultsAnywhere = R.Replies.Count(ECommandReplyKind::DefenceResult);

		if (Run == ERun::Rival)
		{
			// Edge: a Rejected_Latched Attack sends no FStrikeResult (only the held NotNow reply is recorded).
			R.Replies.Entries.Reset();
			R.Advance(2.0);
			R.Core->OnRejectedLatched(R.AttackItem(A, FVector(150.0, 500.0, 0.0), nullptr, AttackerSink), R.NextKey());
			TestEqual(TEXT("a Rejected_Latched Attack appends no result"), R.Replies.Count(ECommandReplyKind::StrikeResult), 0);
			TestTrue(TEXT("its held NotNow reply is recorded"), R.Core->GetReplies().Last().bHeld && R.Core->GetReplies().Last().Code == ECommandRejectCode::NotNow);
		}
	}

	const EStrikeResult Expected[] = { EStrikeResult::RivalEliminated, EStrikeResult::WrongTarget, EStrikeResult::WrongTarget, EStrikeResult::NoContact,
		EStrikeResult::Reversed, EStrikeResult::Reversed };
	const int32 ExpectedTally[] = { TallyBefore, TallyBefore + 1, TallyBefore + 1, TallyBefore, TallyBefore, TallyBefore };
	TSet<int64> Widths;
	for (int32 Index = 0; Index < static_cast<int32>(ERun::Count); ++Index)
	{
		const FRunResult& Out = Runs[Index];
		const TCHAR* Name = RunNames[Index];
		if (!TestEqual(FString::Printf(TEXT("%s: exactly one entry on the attacker's sink"), Name), Out.AttackerEntries.Num(), 1))
		{
			continue;
		}
		const FCommandReply& Entry = Out.AttackerEntries[0];
		TestTrue(FString::Printf(TEXT("%s: it is the FStrikeResult (%d)"), Name, static_cast<int32>(Expected[Index])),
			Entry.Kind == ECommandReplyKind::StrikeResult && Entry.Strike.Result == Expected[Index]);
		TestEqual(FString::Printf(TEXT("%s: TallyAfter"), Name), static_cast<int32>(Entry.Strike.TallyAfter), ExpectedTally[Index]);
		TestEqual(FString::Printf(TEXT("%s: the bystander receives nothing"), Name), Out.BystanderEntries, 0);
		int64 Bits = 0;
		Bytes(Entry, Bits);
		Widths.Add(Bits);
	}
	TestTrue(TEXT("every FStrikeResult entry has the same serialised size"), Widths.Num() == 1 && Widths.Contains(FCommandReply::WireBits(ECommandReplyKind::StrikeResult)));

	// The NPC and informant kills: identical apart from ClientCommandSeq and TallyAfter (equalise those two, compare bits).
	const FRunResult& Npc = Runs[static_cast<int32>(ERun::Npc)];
	const FRunResult& Informant = Runs[static_cast<int32>(ERun::Informant)];
	if (Npc.AttackerEntries.Num() == 1 && Informant.AttackerEntries.Num() == 1)
	{
		FCommandReply A = Npc.AttackerEntries[0];
		FCommandReply B = Informant.AttackerEntries[0];
		TestTrue(TEXT("the NPC and informant entries differ in ClientCommandSeq (different commands)"), A.Seq != B.Seq);
		B.Seq = A.Seq;
		B.Strike.ClientCommandSeq = A.Strike.ClientCommandSeq;
		B.Strike.TallyAfter = A.Strike.TallyAfter;
		int64 BitsA = 0, BitsB = 0;
		TestTrue(TEXT("apart from ClientCommandSeq and TallyAfter the NPC and informant payloads are byte-identical"), Bytes(A, BitsA) == Bytes(B, BitsB) && A == B);
	}

	// FDefenceResult: the bound defender only, answering its Prepare's seq; nothing for an NPC defender.
	const FRunResult& Reversal = Runs[static_cast<int32>(ERun::Reversal)];
	TestTrue(TEXT("reversal: the bound defender receives exactly one FDefenceResult {Reversed} answering its Prepare (seq 77)"), Reversal.DefenderEntries.Num() == 1
		&& Reversal.DefenderEntries[0].Kind == ECommandReplyKind::DefenceResult && Reversal.DefenderEntries[0].Defence.Result == EDefenceResult::Reversed
		&& Reversal.DefenderEntries[0].Seq.Seq == 77);
	TestEqual(TEXT("reversal by an NPC defender: no FDefenceResult anywhere"), Runs[static_cast<int32>(ERun::NpcDefenderReversal)].DefenceResultsAnywhere, 0);
	for (int32 Index = 0; Index < static_cast<int32>(ERun::Count); ++Index)
	{
		if (static_cast<ERun>(Index) != ERun::Reversal)
		{
			TestEqual(FString::Printf(TEXT("%s: no FDefenceResult on any sink"), RunNames[Index]), Runs[Index].DefenceResultsAnywhere, 0);
		}
	}

	// The wire: every Kind round-trips at its fixed width.
	{
		FClientCommandSeq Seq;
		Seq.PossessionEpoch = 65535;
		Seq.Seq = 0xFFFFFFFFu;
		FStrikeResult Strike;
		Strike.ClientCommandSeq = Seq;
		Strike.Result = EStrikeResult::Reversed;
		Strike.TallyAfter = 255;
		Strike.bExiled = true;
		Strike.ContactServerTime = 1234.5;
		FHoldStatus Hold;
		Hold.Seq = Seq;
		Hold.ActionType = EActionType::Prepare;
		Hold.Outcome = EHoldOutcome::Cancelled;
		Hold.ServerStartTime = 10.0;
		Hold.ServerEndTime = 12.0;
		FDefenceResult Defence;
		const FCommandReply Cases[] = { FCommandReply::MakeRejected(Seq, ECommandRejectCode::OutOfReach), FCommandReply::MakeHoldStatus(Hold),
			FCommandReply::MakeStrikeResult(Strike), FCommandReply::MakeDefenceResult(Seq, Defence) };
		for (const FCommandReply& Case : Cases)
		{
			int64 Bits = 0;
			bool bSuccess = false;
			const FCommandReply Back = RoundTrip(Case, Bits, bSuccess);
			TestTrue(FString::Printf(TEXT("kind %d round-trips"), static_cast<int32>(Case.Kind)), bSuccess && Back == Case);
			TestEqual(FString::Printf(TEXT("kind %d is %d bits"), static_cast<int32>(Case.Kind), FCommandReply::WireBits(Case.Kind)), Bits, static_cast<int64>(FCommandReply::WireBits(Case.Kind)));
		}
	}
	return true;
}

#endif
