// Story asn-011, AC2 / AC-ASN-27 NpcsNeverStrike (ASN-R7, Crowd T6 rate 0). The reference crowd scene of the headless
// crowd simulation (the real crowd code over the SCL movement model; decision logged: headless instead of PIE): 50 NPCs,
// fixed crowd seed, every tell clock on, the reference herring sites, 10 simulated minutes at a fixed 1/60 s step. The
// run is non-vacuous (the NPCs issue Move calls and discrete commands, braces included, at the command boundary) and no
// NPC ever issues or even dry-runs an Attack. Logic half: the crowd's command set has no Attack path (compile-time
// member detection over the agent output interface, the full and the restricted sinks and the sim's NPC driver).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/CrowdSink.h"
#include "Tests/Crowd/CrowdHerringRecorder.h"
#include "Tests/Crowd/CrowdSimWorld.h"

namespace NpcsNeverStrikeTestData
{
	constexpr uint64 CrowdSeed = 0x5EED0A5E0011ull;
	constexpr double Minutes = 10.0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationNpcsNeverStrikeTest, "StealthGame.Assassination.NpcsNeverStrike",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationNpcsNeverStrikeTest::RunTest(const FString& Parameters)
{
	using namespace StealthCrowdSim;
	using namespace NpcsNeverStrikeTestData;

	// Logic: no Attack member anywhere on the crowd's command boundary (negative control: the detector finds one).
	TestFalse(TEXT("ICrowdAgentOutput has no Attack"), StealthCrowdSink::THasAttack<ICrowdAgentOutput>::value);
	TestFalse(TEXT("FCrowdCommandSink has no Attack"), StealthCrowdSink::THasAttack<FCrowdCommandSink>::value);
	TestFalse(TEXT("FCrowdMotionSink has no Attack"), StealthCrowdSink::THasAttack<FCrowdMotionSink>::value);
	TestFalse(TEXT("the sim's NPC driver has no Attack"), StealthCrowdSink::THasAttack<FNpc>::value);
	struct FWithAttack { void Attack() {} };
	TestTrue(TEXT("the detector finds an Attack member (negative control)"), StealthCrowdSink::THasAttack<FWithAttack>::value);

	// Integration: 50 NPCs x 10 simulated minutes, fixed seed and step.
	FConfig Config;
	Config.HalfSizeCm = 3000.0;
	Config.SubSeed = CrowdSeed;
	Config.StepSec = 1.0 / 60.0;
	Config.EnabledTellMask = FConfig::AllTells;
	FWorld World(Config);
	World.AddReferenceHerringSites();
	StealthCrowdHerringTest::FRecorder Recorder;
	World.SetObserver(&Recorder);
	World.SpawnNpcs();
	World.StartRound();
	World.RunUntil(Minutes * 60.0);

	TestEqual(TEXT("the reference scene has 50 NPCs"), World.NumNpcs(), 50);
	int64 MoveCalls = 0;
	for (int32 Index = 0; Index < World.NumNpcs(); ++Index)
	{
		MoveCalls += World.GetNpc(Index).MoveCalls;
	}
	int32 AttackDryRuns = 0, Discrete = 0, Prepares = 0;
	for (const FCommandRecord& Record : World.GetCommandLog())
	{
		if (Record.Kind == FCommandRecord::EKind::DryRun)
		{
			AttackDryRuns += Record.Arg == static_cast<int32>(EActionType::Attack) ? 1 : 0;
			Prepares += Record.Arg == static_cast<int32>(EActionType::Prepare) ? 1 : 0;
		}
		else
		{
			++Discrete;
		}
	}
	AddInfo(FString::Printf(TEXT("NPCSNEVERSTRIKE 10 min x 50 NPCs: Move calls %lld, discrete commands %d, Prepare dry runs %d, Attack dry runs %d"), MoveCalls, Discrete, Prepares, AttackDryRuns));
	TestTrue(TEXT("non-vacuous: the NPCs moved"), MoveCalls > 0);
	TestTrue(TEXT("non-vacuous: the NPCs issued discrete commands"), Discrete > 0);
	TestEqual(TEXT("0 Attack commands (not even a dry run) by any NPC"), AttackDryRuns, 0);
	return true;
}

#endif
