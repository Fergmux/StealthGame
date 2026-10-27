// Story asn-015 (production/epics/assassination-defence-reversal/story-015-owner-results-reply-entries-and-tally.md), AC1 /
// AC-ASN-46 ExileFlagAndTally (ASN-R19, Penalty CR-2), with the QA rewrite: a fake Penalty (subscribed to OnPawnKilled,
// answering through IStrikeTallySource) keeps the server-only strike count and the owner tally. An attacker with two NPC
// strikes and a tally of 2:
//   - its third NPC kill's FStrikeResult carries WrongTarget, TallyAfter 3, bExiled true, filled inside the slot: the tally
//     source is asked after the OnPawnKilled fan-out and before the result is appended (the call log's order);
//   - an informant kill at the same counts carries WrongTarget, TallyAfter 3, bExiled false;
//   - ContactServerTime equals the attacker's strike write's contact time exactly;
//   - the serialised entry is the same size for tallies 0, 1, 2, 3 and 255 (no field or size reveals the count);
//   - edge cases: a whiff (NoContact) and a reversed strike (Reversed) still ask the source; a missing source gives
//     {0, false} plus one DeadRegistration fault, never a crash.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Serialization/BitWriter.h"
#include "Tests/Assassination/EncounterTestRig.h"

namespace ExileFlagAndTallyTestData
{
	constexpr uint16 AttackerSink = 7;
	constexpr int32 StrikeLimit = 3;

	/** The fake Penalty: strike count (NPC kills only) and owner tally (every WrongTarget), exile on the limit's kill. */
	struct FFakePenalty
	{
		int32 Strikes = 0;
		int32 Tally = 0;
		bool bExilePending = false;

		void Bind(StealthEncounterTest::FRig& R)
		{
			R.Core->OnPawnKilled.AddLambda([this](APawn*, APawn*, EVictimKind Kind, EKillCause Cause)
			{
				if (Cause != EKillCause::Strike || Kind == EVictimKind::Player)
				{
					return;
				}
				++Tally;
				if (Kind == EVictimKind::Npc && ++Strikes == StrikeLimit)
				{
					bExilePending = true;
				}
			});
			R.Tally.Answer = [this](const APawn*, EStrikeResult)
			{
				FStrikeTally Out;
				Out.TallyAfter = static_cast<uint8>(FMath::Min(Tally, 255));
				Out.bExiled = bExilePending;
				bExilePending = false;
				return Out;
			};
		}
	};

	int64 WireBits(const FCommandReply& Reply)
	{
		FBitWriter Writer(0, true);
		FCommandReply Copy = Reply;
		bool bSuccess = false;
		Copy.NetSerialize(Writer, nullptr, bSuccess);
		return Writer.GetNumBits();
	}

	/** The contact time of the attacker's last Strike write. */
	double StrikeWriteContact(const StealthEncounterTest::FRig& R, const APawn* Attacker)
	{
		double Contact = -1.0;
		for (const StealthEncounterTest::FRecordingPawnState::FWrite& W : R.PawnState.For(Attacker))
		{
			if (!W.bDeath && W.State.Pose == EActionPose::Strike)
			{
				Contact = W.State.ContactServerTime;
			}
		}
		return Contact;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationExileFlagAndTallyTest, "StealthGame.Assassination.ExileFlagAndTally",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationExileFlagAndTallyTest::RunTest(const FString& Parameters)
{
	using namespace StealthEncounterTest;
	using namespace ExileFlagAndTallyTestData;

	for (const bool bInformant : { false, true })
	{
		const TCHAR* Name = bInformant ? TEXT("informant kill") : TEXT("third NPC kill");
		FRig R(TEXT("ExileFlagAndTallyWorld"), nullptr, /*bWithPawnState*/ true);
		FFakePenalty Penalty;
		Penalty.Strikes = 2;
		Penalty.Tally = 2;
		Penalty.Bind(R);
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* V = bInformant ? R.Spawn(FVector(150.0, 0.0, 0.0), 1) : R.Spawn(FVector(150.0, 0.0, 0.0));
		if (bInformant)
		{
			R.Roster.Members[V].State = EParticipantState::Informant;
		}

		R.Calls.Reset();
		R.AttackFrom(A, R.Facts(V).Location, V, AttackerSink, 31);
		const FAttackResolution& Res = R.Last();
		const TArray<FCommandReply> Results = R.Replies.For(AttackerSink, ECommandReplyKind::StrikeResult);
		TestTrue(FString::Printf(TEXT("%s: exactly one FStrikeResult on the attacker's sink"), Name), Results.Num() == 1);
		if (Results.Num() != 1)
		{
			continue;
		}
		const FStrikeResult& Strike = Results[0].Strike;
		TestTrue(FString::Printf(TEXT("%s: WrongTarget"), Name), Strike.Result == EStrikeResult::WrongTarget);
		TestEqual(FString::Printf(TEXT("%s: TallyAfter 3"), Name), static_cast<int32>(Strike.TallyAfter), 3);
		TestEqual(FString::Printf(TEXT("%s: bExiled only on the limit's NPC kill"), Name), Strike.bExiled, !bInformant);
		TestTrue(FString::Printf(TEXT("%s: the entry answers the Attack's ClientCommandSeq"), Name), Results[0].Seq.Seq == 31 && Strike.ClientCommandSeq.Seq == 31);
		TestTrue(FString::Printf(TEXT("%s: the resolution carries the same result"), Name), Res.StrikeResult == Strike);
		TestTrue(FString::Printf(TEXT("%s: ContactServerTime = the attacker's strike write's contact, exactly"), Name),
			Strike.ContactServerTime == StrikeWriteContact(R, A) && Strike.ContactServerTime == Res.ContactServerTime);
		// Filled inside the slot: OnPawnKilled (Penalty applies) -> the tally query -> the appended result.
		const int32 Killed = R.Calls.IndexOfByKey(TEXT("OnPawnKilled"));
		const int32 Asked = R.Calls.IndexOfByKey(TEXT("GetTallyForResult"));
		TestTrue(FString::Printf(TEXT("%s: the tally is read after the OnPawnKilled fan-out, once (calls: %s)"), Name, *FString::Join(R.Calls, TEXT(", "))),
			Killed != INDEX_NONE && Asked > Killed && R.Tally.Calls.Num() == 1 && R.Tally.Calls[0].Attacker == A);
	}

	// No field or size reveals the count: the entry's serialised width for every tally, exiled or not.
	{
		FStrikeResult Base;
		Base.Result = EStrikeResult::WrongTarget;
		Base.ContactServerTime = 12.25;
		TSet<int64> Widths;
		for (const uint8 Tally : { 0, 1, 2, 3, 255 })
		{
			for (const bool bExiled : { false, true })
			{
				FStrikeResult Probe = Base;
				Probe.TallyAfter = Tally;
				Probe.bExiled = bExiled;
				Widths.Add(WireBits(FCommandReply::MakeStrikeResult(Probe)));
			}
		}
		TestTrue(TEXT("one serialised width for tallies 0..3 and 255, exiled or not"), Widths.Num() == 1 && Widths.Contains(FCommandReply::WireBits(ECommandReplyKind::StrikeResult)));
	}

	// Edge: a whiff and a reversed strike still ask the source (a uniform call pattern), and carry the tally unchanged.
	{
		FRig R(TEXT("ExileFlagAndTallyEdgeWorld"));
		FFakePenalty Penalty;
		Penalty.Tally = 2;
		Penalty.Bind(R);
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* B = R.Spawn(FVector(5000.0, 0.0, 0.0), 0);
		APawn* D = R.Spawn(FVector(5150.0, 0.0, 0.0), 1);
		R.AttackFrom(A, FVector(150.0, 900.0, 0.0), nullptr, AttackerSink, 1); // nobody there: a whiff
		R.Command(D, ECommandKind::PrepareBegin);
		R.Advance(0.5);
		R.AttackFrom(B, R.Facts(D).Location, D, AttackerSink + 1, 2); // the braced defender reverses B
		TestTrue(TEXT("edge: the whiff and the reversal both asked the source"), R.Tally.Calls.Num() == 2 && R.Tally.Calls[0].Result == EStrikeResult::NoContact
			&& R.Tally.Calls[1].Result == EStrikeResult::Reversed);
		const TArray<FCommandReply> Whiff = R.Replies.For(AttackerSink, ECommandReplyKind::StrikeResult);
		const TArray<FCommandReply> Reversed = R.Replies.For(AttackerSink + 1, ECommandReplyKind::StrikeResult);
		TestTrue(TEXT("edge: NoContact and Reversed carry the tally unchanged (2)"), Whiff.Num() == 1 && Reversed.Num() == 1 && Whiff[0].Strike.TallyAfter == 2
			&& Reversed[0].Strike.TallyAfter == 2 && !Whiff[0].Strike.bExiled && !Reversed[0].Strike.bExiled);
	}

	// Edge: a missing tally source is a fault: {0, false} and one DeadRegistration report, never a crash.
	{
		FRig R(TEXT("ExileFlagAndTallyMissingWorld"));
		FAttackResolverDeps Deps = R.Core->GetDeps();
		Deps.Tally = nullptr;
		FAttackResolverCore Bare(Deps, R.Params);
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* V = R.Spawn(FVector(150.0, 0.0, 0.0));
		Bare.Resolve(R.AttackItem(A, R.Facts(V).Location, V, AttackerSink), R.NextKey());
		const TArray<FCommandReply> Results = R.Replies.For(AttackerSink, ECommandReplyKind::StrikeResult);
		TestTrue(TEXT("missing source: the result still goes out, carrying {0, false}"), Results.Num() == 1 && Results[0].Strike.Result == EStrikeResult::WrongTarget
			&& Results[0].Strike.TallyAfter == 0 && !Results[0].Strike.bExiled);
		TestTrue(TEXT("missing source: one DeadRegistration fault (NoTallySource)"), R.Faults.GetRecords().Num() == 1 && R.Faults.GetRecords()[0].Fault == EStealthFault::DeadRegistration
			&& R.Faults.GetRecords()[0].Detail == EncounterFaultDetail::NoTallySource);
	}
	return true;
}

#endif
