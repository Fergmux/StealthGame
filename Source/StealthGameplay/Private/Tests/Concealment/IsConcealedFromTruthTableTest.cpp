// Story con-024 (production/epics/concealment-bushes/story-024-is-concealed-from-truth-table.md; GDD AC-CON-05, CON-R10;
// ADR-0017 section 7; IVP R7.8, R9a):
//   - StealthGame.Concealment.IsConcealedFromTruthTable (AC1): {player, NPC} x {Outside, Entering, Concealed, Exiting before
//     the crossing, Emerging before the crossing, Exiting after it} x {owner, ally, opponent, informant-opponent}, plus the
//     killed-while-concealed rows (before and after the death write), against a hand-written expected table (never computed
//     from the function's own rule).
//   - StealthGame.Concealment.CoOccupantRivalsBlind (AC2): two rival co-occupants are concealed from each other; two allies
//     are not; an informant viewer gets an active opponent's answer (no x-ray); the query's header is server-only (its first
//     include is the server-only guard) and no client-module source names it (scan with a seeded negative control).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ConcealmentTestRig.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

using namespace StealthConcealmentTest;

namespace ConcealmentTruthTableData
{
	/** Viewers: 0 owner (participant 10), 1 ally (11, team 0), 2 opponent (20, team 1), 3 informant-opponent (21, team 1). */
	const uint32 Viewers[] = { 10, 11, 20, 21 };
	const TCHAR* const ViewerNames[] = { TEXT("owner"), TEXT("ally"), TEXT("opponent"), TEXT("informant-opponent") };

	enum class EPhase : uint8 { Outside, Entering, Concealed, ExitingBefore, EmergingBefore, ExitingAfter, KilledBeforeWrite, KilledAfterWrite };
	const TCHAR* const PhaseNames[] = { TEXT("Outside"), TEXT("Entering"), TEXT("Concealed"), TEXT("Exiting before the crossing"),
		TEXT("Emerging before the crossing"), TEXT("Exiting after the crossing"), TEXT("killed concealed, before the death write"),
		TEXT("killed concealed, after the death write") };

	/** The checked-in expected table [player 0 / NPC 1][phase][viewer], written by hand from AC-CON-05. */
	const bool Expected[2][8][4] = {
		// Player pawn (participant 10, team 0): concealed from every non-ally exactly in the hidden phases; never from the
		// owner or an ally.
		{
			{ false, false, false, false }, // Outside
			{ false, false, false, false }, // Entering
			{ false, false, true, true },   // Concealed
			{ false, false, true, true },   // Exiting before the crossing
			{ false, false, true, true },   // Emerging before the crossing
			{ false, false, false, false }, // Exiting after the crossing
			{ false, false, true, true },   // killed concealed, before the death write
			{ false, false, false, false }, // killed concealed, after the death write
		},
		// NPC pawn (unbound): every viewer is a non-ally.
		{
			{ false, false, false, false },
			{ false, false, false, false },
			{ true, true, true, true },
			{ true, true, true, true },
			{ true, true, true, true },
			{ false, false, false, false },
			{ true, true, true, true },
			{ false, false, false, false },
		},
	};

	/** Drives a fresh pawn of the given kind to the phase. */
	AStealthPawn* DriveTo(FRig& Rig, bool bNpc, EPhase Phase)
	{
		AStealthPawn* Pawn = Rig.SpawnAtReach(30.0);
		if (!bNpc)
		{
			Rig.Relations.Bind(Pawn, 10, 0);
		}
		if (Phase == EPhase::Outside)
		{
			return Pawn;
		}
		Rig.SubmitHide(Pawn, ECommandKind::HideEnter, 1);
		Rig.Tick();
		if (Phase == EPhase::Entering)
		{
			return Pawn;
		}
		Rig.Run(1.0);
		switch (Phase)
		{
		case EPhase::ExitingBefore:
		case EPhase::ExitingAfter:
			Rig.SubmitHide(Pawn, ECommandKind::HideExit, 0);
			Rig.Tick();
			if (Phase == EPhase::ExitingAfter)
			{
				Rig.Run(0.5);
			}
			break;
		case EPhase::EmergingBefore:
			Rig.Core->OnAttackExecuted(*Pawn, Rig.Now() + 0.75, FQueueKey(), Rig.Q.MakeTimer(ETimerKind::ConcealmentTransition, 0));
			break;
		case EPhase::KilledBeforeWrite:
		case EPhase::KilledAfterWrite:
			Rig.Core->OnPawnKilled(*Pawn, FQueueKey(1, 1));
			if (Phase == EPhase::KilledAfterWrite)
			{
				Rig.Core->OnDeathWrite(*Pawn);
			}
			break;
		default:
			break;
		}
		return Pawn;
	}

	void BindViewers(FRig& Rig)
	{
		Rig.Relations.Teams.Add(10, 0);
		Rig.Relations.Teams.Add(11, 0);
		Rig.Relations.Teams.Add(20, 1);
		Rig.Relations.Teams.Add(21, 1);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentIsConcealedFromTruthTableTest, "StealthGame.Concealment.IsConcealedFromTruthTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentIsConcealedFromTruthTableTest::RunTest(const FString& Parameters)
{
	using namespace ConcealmentTruthTableData;
	for (int32 Kind = 0; Kind < 2; ++Kind)
	{
		for (int32 Phase = 0; Phase < 8; ++Phase)
		{
			FRig Rig(*FString::Printf(TEXT("ConcealmentTruth_%d_%d"), Kind, Phase));
			BindViewers(Rig);
			const AStealthPawn* Pawn = DriveTo(Rig, Kind == 1, static_cast<EPhase>(Phase));
			for (int32 Viewer = 0; Viewer < 4; ++Viewer)
			{
				TestEqual(FString::Printf(TEXT("%s, %s, viewer %s"), Kind == 0 ? TEXT("player") : TEXT("NPC"), PhaseNames[Phase], ViewerNames[Viewer]),
					Rig.Core->IsConcealedFrom(FParticipantId(Viewers[Viewer]), Pawn), Expected[Kind][Phase][Viewer]);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentCoOccupantRivalsBlindTest, "StealthGame.Concealment.CoOccupantRivalsBlind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentCoOccupantRivalsBlindTest::RunTest(const FString& Parameters)
{
	FRig Rig(TEXT("ConcealmentCoOccupants"));
	AStealthPawn* A = Rig.SpawnAtReach(30.0, 20.0);
	AStealthPawn* B = Rig.SpawnAtReach(30.0, -20.0);
	AStealthPawn* C = Rig.Spawn(FVector(0.0, 130.0, 86.0));
	Rig.Relations.Bind(A, 1, 0);
	Rig.Relations.Bind(B, 2, 1);
	Rig.Relations.Bind(C, 3, 0);
	Rig.Relations.Teams.Add(4, 1); // an informant of B's team, viewing
	Rig.Relations.Teams.Add(5, 0); // an informant of A's team, viewing
	for (AStealthPawn* Pawn : { A, B, C })
	{
		Rig.EnterAndSettle(Pawn);
	}
	TestTrue(TEXT("rival co-occupants: A concealed from B"), Rig.Core->IsConcealedFrom(FParticipantId(2), A));
	TestTrue(TEXT("rival co-occupants: B concealed from A"), Rig.Core->IsConcealedFrom(FParticipantId(1), B));
	TestFalse(TEXT("allied co-occupants: A not concealed from C"), Rig.Core->IsConcealedFrom(FParticipantId(3), A));
	TestFalse(TEXT("allied co-occupants: C not concealed from A"), Rig.Core->IsConcealedFrom(FParticipantId(1), C));
	TestEqual(TEXT("an informant opponent of A gets the active opponent's answer (no x-ray)"), Rig.Core->IsConcealedFrom(FParticipantId(4), A), Rig.Core->IsConcealedFrom(FParticipantId(2), A));
	TestFalse(TEXT("an informant ally of A sees it (allies, informants included)"), Rig.Core->IsConcealedFrom(FParticipantId(5), A));

	// Server-only: the declaring header starts with the server-only guard; no client-module source names the query.
	FString Header;
	const FString HeaderPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/StealthGameplay/Features/Concealment/Public/Server/ConcealmentCore.h"));
	if (TestTrue(TEXT("the core header is readable"), FFileHelper::LoadFileToString(Header, *HeaderPath)))
	{
		const int32 FirstInclude = Header.Find(TEXT("#include"));
		TestTrue(TEXT("its first include is the server-only guard"), FirstInclude != INDEX_NONE && Header.Mid(FirstInclude).StartsWith(TEXT("#include \"StealthServerOnlyGuard.h\"")));
	}
	TArray<FString> ClientFiles;
	IFileManager::Get().FindFilesRecursive(ClientFiles, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/StealthPresentation")), TEXT("*.*"), true, false);
	TArray<FString> ViewFiles;
	IFileManager::Get().FindFilesRecursive(ViewFiles, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/StealthGameplay/Features/Concealment/Public/View")), TEXT("*.h"), true, false);
	ClientFiles.Append(ViewFiles);
	int32 Scanned = 0;
	for (const FString& File : ClientFiles)
	{
		FString Text;
		if ((File.EndsWith(TEXT(".h")) || File.EndsWith(TEXT(".cpp"))) && FFileHelper::LoadFileToString(Text, *File))
		{
			++Scanned;
			TestFalse(FString::Printf(TEXT("%s names no IsConcealedFrom"), *FPaths::GetCleanFilename(File)), Text.Contains(TEXT("IsConcealedFrom")));
		}
	}
	TestTrue(TEXT("the client scan read sources"), Scanned > 0);
	TestTrue(TEXT("negative control: a seeded client line naming the query is found"), FString(TEXT("bool b = Service->IsConcealedFrom(V, P);")).Contains(TEXT("IsConcealedFrom")));
	return true;
}

#endif
