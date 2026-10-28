// NPC-Kill Penalty stories nkp-016 / nkp-017 (built in the Roster for its epic; participant-state-roster R6, R8, AC-PSR-57,
// Penalty CR-19):
//   StealthGame.Roster.OwnerTallyOwnerOnly: the tally half of story psr-038. SetOwnerTally(P, t, LastResultSeq, epoch) rewrites
//   only P's own view and only its owner part carries {WrongTargetTally, LastResultSeq}; a stale epoch is Rejected_StaleEpoch,
//   the same values NoOp_Unchanged (no write); a Roster reset clears every owner tally.
//   StealthGame.Roster.ExileRetiringUpdateMatchesAssassinated: an exile (StrikeLimit releases the pawn instead of retiring it,
//   decision D42) reaches the ally's C-TeamView entry in the same shape as an Assassinated removal: one retiring update with
//   State Informant, the removed body's pawn reference and RetireAtServerTime = the call's server time, then one update
//   clearing both (AC-PSR-57's StrikeLimit clause; AC-NKP-21). Negative control: before the fix the exile's update carried
//   no RetireAtServerTime and no body (the shapes differed), so the shape comparison is what this test pins.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Roster/StealthRosterPushStep.h"
#include "Tests/Roster/StealthRosterTestFixture.h"
#include "View/StealthRosterTeamView.h"

namespace RosterOwnerTallyAndExileRetireTestData
{
	inline constexpr uint8 Tally = 2;
	inline constexpr uint32 LastSeq = 0x51u;
	using RosterTestData::FullTeamSize;
	using RosterTestData::Pawn;

	/** The views of a push step, one per participant (rooted until Release). */
	struct FViews
	{
		TArray<URosterTeamViewComponent*> Views;

		void Register(FRosterPushStep& Push, const TArray<FParticipantId>& Ids)
		{
			for (const FParticipantId& Id : Ids)
			{
				URosterTeamViewComponent* View = NewObject<URosterTeamViewComponent>(GetTransientPackage());
				View->AddToRoot();
				Views.Add(View);
				Push.RegisterView(Id, View);
			}
		}

		void Release()
		{
			for (URosterTeamViewComponent* View : Views)
			{
				View->RemoveFromRoot();
			}
		}
	};

	/** One removal of P (team 0, ally Q) by Reason: the sequence of Q's entry-of-P updates over three flushes. */
	struct FShape
	{
		TArray<FString> Updates;
		TArray<double> RetireTimes;
		int32 BodyLookups = 0;
		double CallTime = 0.0;
	};

	FShape RemovalShape(ERosterReason Reason)
	{
		FShape Out;
		FRosterTestFixture F;
		const TArray<FParticipantId> Ids = F.BeginAndRegister(FullTeamSize, 3);
		F.AssignSplit(Ids, 2);
		for (const FParticipantId Id : Ids)
		{
			F.Core.SeamSetState(Id, EParticipantState::Active, true);
		}
		const FParticipantId P = Ids[0];
		const FRosterPawnHandle Body = Pawn(1);
		F.Core.BindPawn(P, Body);
		// The resolver counts the lookups of the removed body (the entry's PawnRef source); no actor exists in this test.
		FRosterPushStep Push(F.Core, F.RoundPhase, [&Out, Body](const FRosterPawnHandle& Handle) -> APawn*
		{
			Out.BodyLookups += Handle == Body ? 1 : 0;
			return nullptr;
		});
		F.Core.AddListener(&Push);
		FViews Views;
		Views.Register(Push, Ids);
		Push.Flush();
		Out.BodyLookups = 0;
		const FTeamViewTeammateEntry* Entry = Views.Views[1]->GetView().Teammates.Num() > 0 ? &Views.Views[1]->GetView().Teammates[0] : nullptr;
		FTeamViewTeammateEntry Last = Entry != nullptr ? *Entry : FTeamViewTeammateEntry();
		F.Clock.AdvanceSeconds(1.5);
		Out.CallTime = static_cast<double>(static_cast<int64>(F.Clock.GetServerTimeSeconds() * 1000.0)) / 1000.0;
		F.Core.RequestTransition(P, EParticipantState::Informant, Reason, FRosterTransitionOptions());
		for (int32 Flush = 0; Flush < 3; ++Flush)
		{
			const int32 LookupsBefore = Out.BodyLookups;
			Push.Flush();
			const TArray<FTeamViewTeammateEntry>& Mates = Views.Views[1]->GetView().Teammates;
			const FTeamViewTeammateEntry Now = Mates.Num() > 0 ? Mates[0] : FTeamViewTeammateEntry();
			TArray<FString> Fields;
			if (Now.State != Last.State) { Fields.Add(TEXT("State")); }
			if (Now.RetireAtServerTime != Last.RetireAtServerTime) { Fields.Add(TEXT("RetireAtServerTime")); }
			if (Out.BodyLookups != LookupsBefore) { Fields.Add(TEXT("BodyRef")); }
			if (!Fields.IsEmpty())
			{
				Out.Updates.Add(FString::Join(Fields, TEXT("+")));
				Out.RetireTimes.Add(Now.RetireAtServerTime);
			}
			Last = Now;
		}
		F.Core.RemoveListener(&Push);
		Views.Release();
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterOwnerTallyOwnerOnlyTest, "StealthGame.Roster.OwnerTallyOwnerOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterOwnerTallyOwnerOnlyTest::RunTest(const FString& Parameters)
{
	using namespace RosterOwnerTallyAndExileRetireTestData;

	// Arrange: a 3v3 push step with one view per participant.
	FRosterTestFixture F;
	const TArray<FParticipantId> Ids = F.BeginAndRegister(3, 6);
	F.AssignSplit(Ids, 3);
	F.Core.SetTeamsLocked(true);
	F.Core.ResetAll(1);
	FRosterPushStep Push(F.Core, F.RoundPhase, FRosterPushStep::FPawnResolver());
	F.Core.AddListener(&Push);
	FViews Views;
	Views.Register(Push, Ids);
	Push.Flush();
	TArray<int32> WritesBefore;
	for (const URosterTeamViewComponent* View : Views.Views)
	{
		WritesBefore.Add(View->GetServerWriteCount());
	}
	const uint32 Epoch = F.Core.GetRoundEpoch();

	// Act + Assert: the owner's re-push.
	TestTrue(TEXT("SetOwnerTally applies"), Push.SetOwnerTally(Ids[0], Tally, LastSeq, Epoch) == ERosterResult::Applied);
	TestEqual(TEXT("the next flush writes only the owner's view"), Push.Flush(), 1);
	TestEqual(TEXT("the owner part carries the tally"), static_cast<int32>(Views.Views[0]->GetView().Owner.WrongTargetTally), static_cast<int32>(Tally));
	TestEqual(TEXT("the owner part carries LastResultSeq"), Views.Views[0]->GetView().Owner.LastResultSeq, LastSeq);
	for (int32 Index = 1; Index < Views.Views.Num(); ++Index)
	{
		TestEqual(FString::Printf(TEXT("view %d (ally or opponent) is not rewritten"), Index), Views.Views[Index]->GetServerWriteCount(), WritesBefore[Index]);
		TestEqual(FString::Printf(TEXT("view %d's owner part has no tally"), Index), static_cast<int32>(Views.Views[Index]->GetView().Owner.WrongTargetTally), 0);
	}
	TestTrue(TEXT("a stale epoch is Rejected_StaleEpoch"), Push.SetOwnerTally(Ids[0], Tally + 1, LastSeq + 1, Epoch + 1) == ERosterResult::Rejected_StaleEpoch);
	TestTrue(TEXT("the same values are NoOp_Unchanged"), Push.SetOwnerTally(Ids[0], Tally, LastSeq, Epoch) == ERosterResult::NoOp_Unchanged);
	TestEqual(TEXT("neither writes a view"), Push.Flush(), 0);

	// A reset clears every owner tally.
	F.Core.ResetAll(2);
	Push.Flush();
	TestEqual(TEXT("a Roster reset clears the owner tally"), static_cast<int32>(Views.Views[0]->GetView().Owner.WrongTargetTally), 0);
	TestEqual(TEXT("a Roster reset clears LastResultSeq"), Views.Views[0]->GetView().Owner.LastResultSeq, 0u);

	F.Core.RemoveListener(&Push);
	Views.Release();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRosterExileRetiringUpdateTest, "StealthGame.Roster.ExileRetiringUpdateMatchesAssassinated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRosterExileRetiringUpdateTest::RunTest(const FString& Parameters)
{
	using namespace RosterOwnerTallyAndExileRetireTestData;
	const FShape Assassinated = RemovalShape(ERosterReason::Assassinated);
	const FShape Exile = RemovalShape(ERosterReason::StrikeLimit);
	AddInfo(FString::Printf(TEXT("ROSTER exile shape: Assassinated [%s]; StrikeLimit [%s]"), *FString::Join(Assassinated.Updates, TEXT(" | ")),
		*FString::Join(Exile.Updates, TEXT(" | "))));
	TestTrue(TEXT("fixture: the Assassinated removal produced a retiring update and its clearing update"), Assassinated.Updates.Num() == 2);
	TestEqual(TEXT("the exile reaches the ally in the same shape as an Assassinated removal"), FString::Join(Exile.Updates, TEXT(" | ")),
		FString::Join(Assassinated.Updates, TEXT(" | ")));
	if (TestTrue(TEXT("the exile's retiring update exists"), Exile.RetireTimes.Num() >= 1))
	{
		TestEqual(TEXT("the exile's retiring update carries RetireAtServerTime = the call's server time"), Exile.RetireTimes[0], Exile.CallTime);
	}
	TestTrue(TEXT("the exile's retiring update names the released body (its pawn reference is looked up)"), Exile.BodyLookups > 0);
	return true;
}

#endif
