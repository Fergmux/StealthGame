// Story con-009 (production/epics/concealment-bushes/story-009-crowd-perception-filter-and-steering-exclusion.md; GDD
// CON-R4, CON-R14; ADR-0017 section 7; ADR-0016 section 5), AC1, StealthGame.Concealment.PerceptionFilter: for NPC and
// player pawns in every EConcealState, IsPerceivable is false only while Concealed and IsSteeringObstacle is true only while
// Outside, the same for every pawn kind, with no viewer input; the interface Crowd reads (ICrowdPerceptionFilter) exposes no
// occupancy query, and no Crowd source includes a Concealment header (header scan, with a seeded negative control).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ConcealmentTestRig.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

using namespace StealthConcealmentTest;

namespace ConcealmentPerceptionFilterTestData
{
	struct FObserved
	{
		EConcealState State;
		bool bPerceivable;
		bool bObstacle;
	};

	TArray<FObserved> Walk(bool bPlayer)
	{
		TArray<FObserved> Out;
		FRig Rig(bPlayer ? TEXT("ConcealmentFilterPlayer") : TEXT("ConcealmentFilterNpc"));
		AStealthPawn* Pawn = Rig.SpawnAtReach(30.0);
		if (bPlayer)
		{
			Rig.Relations.Bind(Pawn, 5, 1);
		}
		const auto Observe = [&Out, &Rig, Pawn]() { Out.Add({ Rig.StateOf(Pawn), Rig.Core->IsPerceivable(Pawn), Rig.Core->IsSteeringObstacle(Pawn) }); };
		Observe();                                   // Outside
		Rig.SubmitHide(Pawn, ECommandKind::HideEnter, 1);
		Rig.Tick();
		Observe();                                   // Entering
		Rig.Run(1.0);
		Observe();                                   // Concealed
		Rig.SubmitHide(Pawn, ECommandKind::HideExit, 0);
		Rig.Tick();
		Observe();                                   // Exiting
		Rig.Run(7.5);
		Rig.Pawns.Of(Pawn).Location = FVector(130.0, 0.0, 86.0);
		Rig.EnterAndSettle(Pawn);
		Rig.Core->OnAttackExecuted(*Pawn, Rig.Now() + 0.75, FQueueKey(), Rig.Q.MakeTimer(ETimerKind::ConcealmentTransition, 0));
		Observe();                                   // Emerging
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentPerceptionFilterTest, "StealthGame.Concealment.PerceptionFilter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentPerceptionFilterTest::RunTest(const FString& Parameters)
{
	using namespace ConcealmentPerceptionFilterTestData;
	const TArray<FObserved> Npc = Walk(false);
	const TArray<FObserved> Player = Walk(true);
	const EConcealState Expected[] = { EConcealState::Outside, EConcealState::Entering, EConcealState::Concealed, EConcealState::Exiting, EConcealState::Emerging };
	if (TestEqual(TEXT("five states observed (NPC)"), Npc.Num(), 5) && TestEqual(TEXT("five states observed (player)"), Player.Num(), 5))
	{
		for (int32 Index = 0; Index < 5; ++Index)
		{
			TestEqual(FString::Printf(TEXT("row %d state"), Index), Npc[Index].State, Expected[Index]);
			TestEqual(FString::Printf(TEXT("state %d: perceivable iff not Concealed"), Index), Npc[Index].bPerceivable, Expected[Index] != EConcealState::Concealed);
			TestEqual(FString::Printf(TEXT("state %d: a steering obstacle iff Outside"), Index), Npc[Index].bObstacle, Expected[Index] == EConcealState::Outside);
			TestTrue(FString::Printf(TEXT("state %d: identical for a player pawn"), Index),
				Player[Index].State == Npc[Index].State && Player[Index].bPerceivable == Npc[Index].bPerceivable && Player[Index].bObstacle == Npc[Index].bObstacle);
		}
	}

	// The interface Crowd reads: two pawn-only queries, no occupancy, no viewer.
	FString Header;
	const FString HeaderPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/StealthGameplay/Features/Crowd/Public/Server/CrowdPerceptionFilter.h"));
	if (TestTrue(TEXT("the filter header is readable"), FFileHelper::LoadFileToString(Header, *HeaderPath)))
	{
		int32 Virtuals = 0;
		for (int32 At = Header.Find(TEXT("virtual ")); At != INDEX_NONE; At = Header.Find(TEXT("virtual "), ESearchCase::CaseSensitive, ESearchDir::FromStart, At + 1))
		{
			++Virtuals;
		}
		TestEqual(TEXT("ICrowdPerceptionFilter declares exactly two queries"), Virtuals, 2);
		TestFalse(TEXT("... neither is an occupancy query"), Header.Contains(TEXT("Occupan")) || Header.Contains(TEXT("BushId")));
	}
	// No Crowd source includes a Concealment header (Concealment exposes no occupancy to Crowd).
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/StealthGameplay/Features/Crowd")), TEXT("*.*"), true, false);
	int32 Scanned = 0;
	const auto IncludesConcealment = [](const FString& Text)
	{
		return Text.Contains(TEXT("#include \"Server/Concealment")) || Text.Contains(TEXT("#include \"View/Concealment")) || Text.Contains(TEXT("BushRuntimeSubsystem.h"));
	};
	for (const FString& File : Files)
	{
		FString Text;
		if ((File.EndsWith(TEXT(".h")) || File.EndsWith(TEXT(".cpp"))) && FFileHelper::LoadFileToString(Text, *File))
		{
			++Scanned;
			TestFalse(FString::Printf(TEXT("%s includes no Concealment header"), *FPaths::GetCleanFilename(File)), IncludesConcealment(Text));
		}
	}
	TestTrue(TEXT("the Crowd scan read sources"), Scanned > 10);
	TestTrue(TEXT("negative control: a seeded include is found"), IncludesConcealment(TEXT("#include \"Server/ConcealmentService.h\"\n")));
	return true;
}

#endif
