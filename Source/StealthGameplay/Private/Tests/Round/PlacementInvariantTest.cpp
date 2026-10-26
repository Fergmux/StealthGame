// Story rmf-038 (production/epics/round-match-framework/story-038-placement-invariant.md), GDD AC 56 / F5, R6 P6:
//   - 5 players with 5 NPCs in a zone (share 0.5) passes; 5 with 4 (0.556) aborts PlacementInvariant before Active, with
//     no pawn spawned (the check runs after RequestStartPlacement and before any spawn);
//   - an empty zone has no share;
//   - GlobalShare is 0.167 at 5v5 with 50 NPCs and 0.074 at 2v2 with 50 (tolerance half the last printed digit).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/Core/Round/StealthRoundFormulas.h"
#include "StealthRoundExitBTestRig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundPlacementInvariantTest, "StealthGame.Round.PlacementInvariant",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundPlacementInvariantTest::RunTest(const FString& Parameters)
{
	using namespace RoundTestFixtures;

	// The F5 values.
	const FRoundPlacementZone Pass{ PlacementPlayers, PlacementNpcsPass };
	const FRoundPlacementZone Fail{ PlacementPlayers, PlacementNpcsFail };
	const FRoundPlacementZone Empty{ 0, PlacementNpcsPass };
	TestTrue(TEXT("5 players with 5 NPCs: share 0.5"), StealthRoundFormulas::ZoneShare(Pass).IsSet() && StealthRoundFormulas::ZoneShare(Pass).GetValue() == StealthRoundFormulas::MaxZoneShare);
	TestTrue(TEXT("5 with 5 passes"), StealthRoundFormulas::PlacementSharesHold(MakeArrayView(&Pass, 1)));
	TestFalse(TEXT("5 with 4 fails"), StealthRoundFormulas::PlacementSharesHold(MakeArrayView(&Fail, 1)));
	TestFalse(TEXT("an empty zone has no share"), StealthRoundFormulas::ZoneShare(Empty).IsSet());
	TestTrue(TEXT("an empty zone never fails the invariant"), StealthRoundFormulas::PlacementSharesHold(MakeArrayView(&Empty, 1)));
	TestEqual(TEXT("GlobalShare at 5v5 / 50"), StealthRoundFormulas::GlobalShare(Layout5v5.Team0 + Layout5v5.Team1, GlobalShareNpcCount), GlobalShare5v5, GlobalShareTolerance);
	TestEqual(TEXT("GlobalShare at 2v2 / 50"), StealthRoundFormulas::GlobalShare(Layout2v2.Team0 + Layout2v2.Team1, GlobalShareNpcCount), GlobalShare2v2, GlobalShareTolerance);

	// Through Round's P6: the passing zone spawns; the failing zone aborts PlacementInvariant before Active.
	for (const bool bFail : { false, true })
	{
		FRoundExitBRig Rig;
		Rig.OpenLobby();
		Rig.SetLayout(Layout2v2.Team0, Layout2v2.Team1);
		Rig.EnterPreparing();
		Rig.ScriptPreparingSequence();
		if (bFail)
		{
			Rig.Fakes.Session.Script(TEXT("SetAdmissionMode"), 0, 2);
			Rig.Fakes.CharacterPool.Script(TEXT("DiscardDeal"));
			Rig.Fakes.Session.Script(TEXT("RequestLobbyTravel"));
		}
		Rig.Fakes.LevelSocket.Zones = { bFail ? Fail : Pass, Empty };
		Rig.Core->NotifyTuningReady(PreparingRigTuningHash);
		Rig.Core->NotifyLayoutReady(1, Rig.Core->GetPreparing().GetSeed());
		Rig.AckAll(false);
		Rig.Frame();
		if (bFail)
		{
			TestTrue(TEXT("5 with 4: the round left Preparing for Reset (aborted)"), Rig.Core->GetPhase() == ERoundPhase::Reset);
			TestEqual(TEXT("5 with 4: no pawn was spawned"), Rig.Fakes.Spawner.Spawns.Num(), 0);
			const TArray<FRoundArchivedSlice>& Archive = Rig.Core->GetLogArchive().GetArchive();
			TestTrue(TEXT("5 with 4: PlacementInvariant through the abort entry"), Archive.Num() == 1
				&& Archive[0].AbortCode == static_cast<uint8>(ERoundPreparingAbortCode::PlacementInvariant));
			TestEqual(TEXT("5 with 4: Active never entered"), Rig.Core->GetActiveEntryCount(), 0);
		}
		else
		{
			TestTrue(TEXT("5 with 5: still Preparing, the start spawned"), Rig.Core->GetPhase() == ERoundPhase::Preparing && Rig.Fakes.Spawner.Spawns.Num() > 0);
		}
		TestTrue(TEXT("no unexpected call"), Rig.Log.Unexpected.IsEmpty());
	}
	return true;
}

#endif
