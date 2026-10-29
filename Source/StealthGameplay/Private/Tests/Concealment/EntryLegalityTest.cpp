// Story con-007 (production/epics/concealment-bushes/story-007-hide-legality-predicate.md; GDD CON-R3, AC-CON-06,
// AC-CON-19 first half; ADR-0017 section 2):
//   - StealthGame.Concealment.EntryLegality: the table, every row at occupancy 0, 1 and 4: reach 59 and 60 cm Allow, 61
//     OutOfReach, 30 cm from a closed side OutOfReach; running, traversing, informant, RoundEnded NotNow; a held slot
//     Busy; 5.9 s after an exit Cooldown, 6.0 and 6.1 s Allow; 5.9 s after an emergence Cooldown.
//   - StealthGame.Concealment.SamePathForNpcs: the same rows for an unbound (AI) pawn give identical replies, identical at
//     every occupancy; the legality function reads no relation, participant, team or occupancy (source scan of its body).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ConcealmentTestRig.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

using namespace StealthConcealmentTest;

namespace ConcealmentEntryLegalityTestData
{
	/** Side 0 (+X) closed, the other three open. */
	constexpr uint8 Side0Closed = 0x0E;

	struct FRow
	{
		const TCHAR* Name;
		FPredicateResult Expected;
	};

	FString Describe(const FPredicateResult& R)
	{
		if (R.Verdict == EPredicateVerdict::Allow)
		{
			return TEXT("Allow");
		}
		switch (R.Code)
		{
		case ECommandRejectCode::NotNow: return TEXT("NotNow");
		case ECommandRejectCode::Busy: return TEXT("Busy");
		case ECommandRejectCode::OutOfReach: return TEXT("OutOfReach");
		case ECommandRejectCode::Cooldown: return TEXT("Cooldown");
		}
		return TEXT("?");
	}

	/** Runs every row for one pawn kind at one occupancy; returns the verdict strings in row order. */
	TArray<FString> RunTable(FAutomationTestBase& Test, bool bPlayer, int32 Occupancy)
	{
		TArray<FString> Out;
		FRig Rig(*FString::Printf(TEXT("ConcealmentLegality_%d_%d"), bPlayer ? 1 : 0, Occupancy), Side0Closed);
		// Occupants entering by the open -Y side (3).
		for (int32 Index = 0; Index < Occupancy; ++Index)
		{
			AStealthPawn* Other = Rig.Spawn(FVector(-60.0 + 30.0 * Index, -130.0, 86.0));
			Rig.SubmitHide(Other, ECommandKind::HideEnter, 1);
		}
		Rig.Run(1.0);
		TArray<APawn*> Record;
		Rig.Core->GetOccupancy(1, Record);
		Test.TestEqual(FString::Printf(TEXT("occupancy %d reached"), Occupancy), Record.Num(), Occupancy);

		AStealthPawn* Pawn = Rig.Spawn(FVector(0.0, 159.0, 86.0)); // 59 cm from the open +Y side
		if (bPlayer)
		{
			Rig.Relations.Bind(Pawn, 11, 0);
		}
		FPawnFacts& F = Rig.Pawns.Of(Pawn);
		const auto Ask = [&Rig, Pawn]() { return Rig.Core->CanBeginHide(*Pawn, ECommandKind::HideEnter, 1); };
		const auto Record_ = [&Out, &Ask]() { Out.Add(Describe(Ask())); };

		F.Location = FVector(0.0, 159.0, 86.0); Record_();           // reach 59
		F.Location = FVector(0.0, 160.0, 86.0); Record_();           // reach 60
		F.Location = FVector(0.0, 161.0, 86.0); Record_();           // reach 61
		F.Location = FVector(130.0, 0.0, 86.0); Record_();           // 30 cm from the closed +X side
		F.Location = FVector(0.0, 140.0, 86.0);
		F.bRunning = true; Record_(); F.bRunning = false;            // running
		F.bTraversing = true; Record_(); F.bTraversing = false;      // traversing
		F.Legality = ECommandLegalityClass::Informant; Record_();    // informant
		F.Legality = ECommandLegalityClass::RoundEnded; Record_();   // RoundEnded
		F.Legality = ECommandLegalityClass::Active;
		// Slot held: Entering.
		Rig.SubmitHide(Pawn, ECommandKind::HideEnter, 1);
		Rig.Tick();
		Record_();
		Rig.Run(1.0);
		// Exit, then the cooldown boundaries from the exit's completion.
		Rig.SubmitHide(Pawn, ECommandKind::HideExit, 1);
		Rig.Run(1.0);
		const double ExitDone = Rig.Core->FindRecord(Pawn)->LastExitCompleteServerTime;
		F.Location = FVector(0.0, 140.0, 86.0);
		for (const double After : { 5.9, 6.0, 6.1 })
		{
			Rig.Q.Clock.AdvanceSeconds(ExitDone + After - Rig.Now());
			Record_();
		}
		// An emergence: enter, settle, break by an Attack, complete; 5.9 s after its completion.
		Rig.EnterAndSettle(Pawn);
		Rig.Core->OnAttackExecuted(*Pawn, Rig.Now() + 0.75, FQueueKey(), Rig.Q.MakeTimer(ETimerKind::ConcealmentTransition, 0));
		Rig.Run(2.0);
		const double EmergeDone = Rig.Core->FindRecord(Pawn)->LastExitCompleteServerTime;
		F.Location = FVector(0.0, 140.0, 86.0);
		Rig.Q.Clock.AdvanceSeconds(EmergeDone + 5.9 - Rig.Now());
		Record_();
		return Out;
	}

	const TCHAR* const Expected[] = {
		TEXT("Allow"), TEXT("Allow"), TEXT("OutOfReach"), TEXT("OutOfReach"), TEXT("NotNow"), TEXT("NotNow"), TEXT("NotNow"), TEXT("NotNow"),
		TEXT("Busy"), TEXT("Cooldown"), TEXT("Allow"), TEXT("Allow"), TEXT("Cooldown"),
	};
	const TCHAR* const RowNames[] = {
		TEXT("reach 59"), TEXT("reach 60"), TEXT("reach 61"), TEXT("30 cm from a closed side"), TEXT("running"), TEXT("traversing"),
		TEXT("informant"), TEXT("RoundEnded"), TEXT("slot held"), TEXT("5.9 s after an exit"), TEXT("6.0 s after an exit"),
		TEXT("6.1 s after an exit"), TEXT("5.9 s after an emergence"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentEntryLegalityTest, "StealthGame.Concealment.EntryLegality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentEntryLegalityTest::RunTest(const FString& Parameters)
{
	using namespace ConcealmentEntryLegalityTestData;
	for (const int32 Occupancy : { 0, 1, 4 })
	{
		const TArray<FString> Got = RunTable(*this, /*bPlayer*/ true, Occupancy);
		if (!TestEqual(FString::Printf(TEXT("occupancy %d: every row answered"), Occupancy), Got.Num(), static_cast<int32>(UE_ARRAY_COUNT(Expected))))
		{
			continue;
		}
		for (int32 Index = 0; Index < Got.Num(); ++Index)
		{
			TestEqual(FString::Printf(TEXT("occupancy %d, %s"), Occupancy, RowNames[Index]), Got[Index], FString(Expected[Index]));
		}
	}
	// The reply on the wire carries the predicate's code: an out-of-reach Enter replies OutOfReach.
	FRig Rig(TEXT("ConcealmentLegalityReply"));
	AStealthPawn* Far = Rig.SpawnAtReach(61.0);
	Rig.SubmitHide(Far, ECommandKind::HideEnter, 1);
	Rig.Tick();
	TestEqual(TEXT("an out-of-reach Enter replies Rejected(OutOfReach)"), Rig.RigQueue.CountRejected(ECommandRejectCode::OutOfReach), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentSamePathForNpcsTest, "StealthGame.Concealment.SamePathForNpcs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentSamePathForNpcsTest::RunTest(const FString& Parameters)
{
	using namespace ConcealmentEntryLegalityTestData;
	TArray<FString> First;
	for (const int32 Occupancy : { 0, 1, 4 })
	{
		const TArray<FString> Player = RunTable(*this, true, Occupancy);
		const TArray<FString> Npc = RunTable(*this, false, Occupancy);
		TestEqual(FString::Printf(TEXT("occupancy %d: NPC and player replies identical"), Occupancy), Npc, Player);
		if (First.Num() == 0)
		{
			First = Player;
		}
		TestEqual(FString::Printf(TEXT("occupancy %d: replies identical to occupancy 0"), Occupancy), Player, First);
	}

	// Source scan: the legality function's body names no relation, participant, team or occupancy.
	FString Source;
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/StealthGameplay/Features/Concealment/Private/ConcealmentCore.cpp"));
	if (TestTrue(TEXT("the core source is readable"), FFileHelper::LoadFileToString(Source, *Path)))
	{
		const int32 Start = Source.Find(TEXT("FPredicateResult FConcealmentCore::CanBeginHide("));
		const int32 End = Start != INDEX_NONE ? Source.Find(TEXT("\n}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start) : INDEX_NONE;
		if (TestTrue(TEXT("CanBeginHide's body was found"), Start != INDEX_NONE && End != INDEX_NONE))
		{
			const FString Body = Source.Mid(Start, End - Start);
			for (const TCHAR* Forbidden : { TEXT("Relations"), TEXT("Participant"), TEXT("Team"), TEXT("Occupancy"), TEXT("IsPlayer"), TEXT("Controller"), TEXT("bInOccupancy") })
			{
				TestFalse(FString::Printf(TEXT("CanBeginHide names no %s"), Forbidden), Body.Contains(Forbidden, ESearchCase::CaseSensitive));
			}
			// Negative control: the scan sees a name that is there.
			TestTrue(TEXT("negative control: the body names BushEntryReachCm"), Body.Contains(TEXT("BushEntryReachCm")));
		}
	}
	return true;
}

#endif
