// Story tdc-018 (production/epics/tuning-data-config/story-018-replication-system-assertion.md):
// the harness reads the game driver's replication system through IStealthReplicationSystemProbe, fails
// the run when it differs from the pinned expectation (legacy) and writes the result JSON
// {scenario, build, replicationSystem, netProfile, passed, negativeControlDetected, faults[], captures[]}
// (ADR-0008 section 5). A fake probe reporting Iris is the negative control: the suite passes only
// when that run is detected as failed. The real Iris-versus-legacy mismatch through two processes is
// story tdc-020.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StealthHarnessScenarioRunner.h"

#include "Dom/JsonObject.h"
#include "Engine/NetDriver.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Server/Testing/StealthNegativeControl.h"

namespace ReplicationSystemAssertedTestData
{
	/** A scratch folder under Saved/, emptied before and after the test. */
	FString ScratchDir()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Harness"), TEXT("Tests"), TEXT("ReplicationSystemAsserted")));
	}

	/** Parses a result file; null when unreadable or not a JSON object. */
	TSharedPtr<FJsonObject> LoadJson(const FString& Path)
	{
		FString Text;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			return nullptr;
		}
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		return FJsonSerializer::Deserialize(Reader, Root) ? Root : nullptr;
	}

	/** The eight keys the result record carries, nothing more. */
	const TCHAR* const ResultKeys[] = {
		TEXT("scenario"), TEXT("build"), TEXT("replicationSystem"), TEXT("netProfile"), TEXT("passed"), TEXT("negativeControlDetected"),
		TEXT("faults"), TEXT("captures"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHarnessReplicationSystemAssertedTest, "StealthGame.Harness.ReplicationSystemAsserted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHarnessReplicationSystemAssertedTest::RunTest(const FString& Parameters)
{
	using namespace StealthHarness;
	using namespace ReplicationSystemAssertedTestData;

	const FString Scratch = ScratchDir();
	IFileManager::Get().DeleteDirectory(*Scratch, false, true);
	IFileManager::Get().MakeDirectory(*Scratch, true);

	// The pinned expectation is legacy (ADR-0001): a single named constant.
	TestTrue(TEXT("The pinned replication system is legacy"), PinnedReplicationSystem == EHarnessReplicationSystem::Legacy);

	// A fake reporting legacy passes, and the runner's own control (a probe reporting the other system) is detected.
	{
		const FStealthFixedReplicationProbe Probe(EHarnessReplicationSystem::Legacy);
		FStealthHarnessScenarioRunner Runner(Probe, TEXT("Smoke"), TEXT("Development"), TEXT("StealthLeak1"));
		TestTrue(TEXT("Legacy is asserted"), Runner.AssertReplicationSystem());
		TestTrue(TEXT("The run is passing"), Runner.IsPassing());
		TestTrue(TEXT("Replication system recorded as legacy"), Runner.GetResult().ReplicationSystem == EHarnessReplicationSystem::Legacy);
		TestTrue(TEXT("The negative control was detected"), Runner.GetResult().bNegativeControlDetected);

		const FString Path = FPaths::Combine(Scratch, TEXT("legacy.json"));
		Runner.AddCapture(TEXT("Saved/Harness/x/server.jsonl"));
		TestTrue(TEXT("The result was written"), Runner.WriteResult(Path) == EStealthHarnessResultWrite::Written);
		const TSharedPtr<FJsonObject> Json = LoadJson(Path);
		if (TestTrue(TEXT("The result file parses"), Json.IsValid()))
		{
			TestEqual(TEXT("Exactly the eight result keys"), Json->Values.Num(), (int32)UE_ARRAY_COUNT(ResultKeys));
			for (const TCHAR* Key : ResultKeys)
			{
				TestTrue(FString::Printf(TEXT("Result has key %s"), Key), Json->HasField(Key));
			}
			TestEqual(TEXT("scenario"), Json->GetStringField(TEXT("scenario")), FString(TEXT("Smoke")));
			TestEqual(TEXT("build"), Json->GetStringField(TEXT("build")), FString(TEXT("Development")));
			TestEqual(TEXT("replicationSystem"), Json->GetStringField(TEXT("replicationSystem")), FString(TEXT("legacy")));
			TestEqual(TEXT("netProfile"), Json->GetStringField(TEXT("netProfile")), FString(TEXT("StealthLeak1")));
			TestTrue(TEXT("passed"), Json->GetBoolField(TEXT("passed")));
			TestTrue(TEXT("negativeControlDetected"), Json->GetBoolField(TEXT("negativeControlDetected")));
			TestEqual(TEXT("faults is empty"), Json->GetArrayField(TEXT("faults")).Num(), 0);
			TestEqual(TEXT("captures lists the capture"), Json->GetArrayField(TEXT("captures")).Num(), 1);
		}
	}

	// A fake reporting Iris is the negative control of the suite: the run must be detected as failed.
	{
		const FStealthFixedReplicationProbe Probe(EHarnessReplicationSystem::Iris);
		FStealthHarnessScenarioRunner Runner(Probe, TEXT("Smoke"), TEXT("Development"), FString());
		const FStealthNegativeControl Control(TEXT("IrisDriverFailsTheRun"));
		const FStealthNegativeControlResult Detected = Control.Run([&Runner](FStealthCheckContext& Context)
		{
			Context.TestTrue(TEXT("a run on Iris passes the legacy expectation"), Runner.AssertReplicationSystem());
		});
		FStealthNegativeControl::Report(*this, Detected);
		TestFalse(TEXT("The Iris run is not passing"), Runner.IsPassing());
		TestTrue(TEXT("Replication system recorded as iris"), Runner.GetResult().ReplicationSystem == EHarnessReplicationSystem::Iris);

		const FString Path = FPaths::Combine(Scratch, TEXT("iris.json"));
		TestTrue(TEXT("The failing result was written"), Runner.WriteResult(Path) == EStealthHarnessResultWrite::Written);
		const TSharedPtr<FJsonObject> Json = LoadJson(Path);
		if (TestTrue(TEXT("The iris result file parses"), Json.IsValid()))
		{
			TestEqual(TEXT("replicationSystem is iris"), Json->GetStringField(TEXT("replicationSystem")), FString(TEXT("iris")));
			TestFalse(TEXT("passed is false"), Json->GetBoolField(TEXT("passed")));
			TestEqual(TEXT("netProfile defaults to none"), Json->GetStringField(TEXT("netProfile")), FString(TEXT("none")));
		}
	}

	// A driver that cannot be read is never taken for legacy.
	{
		const FStealthFixedReplicationProbe Unknown(EHarnessReplicationSystem::Unknown);
		FStealthHarnessScenarioRunner Runner(Unknown, TEXT("Smoke"), TEXT("Development"), FString());
		const FStealthNegativeControl Control(TEXT("UnreadableDriverFailsTheRun"));
		FStealthNegativeControl::Report(*this, Control.Run([&Runner](FStealthCheckContext& Context)
		{
			Context.TestTrue(TEXT("an unreadable driver passes"), Runner.AssertReplicationSystem());
		}));
	}

	// The production probe: no driver reads Unknown; a real UNetDriver's IsUsingIrisReplication() is read (the class default is legacy).
	{
		const FStealthNetDriverReplicationProbe NoDriver([]() -> const UNetDriver* { return nullptr; });
		TestTrue(TEXT("A missing driver reads Unknown"), NoDriver.Read() == EHarnessReplicationSystem::Unknown);
		const FStealthNetDriverReplicationProbe DefaultDriver([]() -> const UNetDriver* { return GetDefault<UNetDriver>(); });
		TestTrue(TEXT("UNetDriver::IsUsingIrisReplication() of the class default reads legacy"), DefaultDriver.Read() == EHarnessReplicationSystem::Legacy);
	}

	// faults[] is filled from the fault sink and a fault fails the run.
	{
		const FStealthFixedReplicationProbe Probe(EHarnessReplicationSystem::Legacy);
		FStealthHarnessScenarioRunner Runner(Probe, TEXT("Smoke"), TEXT("Development"), FString());
		Runner.AssertReplicationSystem();
		IStealthFaultSink& Sink = Runner;
		Sink.Report(EStealthFault::StaleEpoch, TEXT("TestSystem"), 7);
		TestFalse(TEXT("A reported fault fails the run"), Runner.IsPassing());
		const FString Path = FPaths::Combine(Scratch, TEXT("fault.json"));
		Runner.WriteResult(Path);
		const TSharedPtr<FJsonObject> Json = LoadJson(Path);
		if (TestTrue(TEXT("The fault result parses"), Json.IsValid()) && TestEqual(TEXT("One fault recorded"), Json->GetArrayField(TEXT("faults")).Num(), 1))
		{
			const TSharedPtr<FJsonObject> Fault = Json->GetArrayField(TEXT("faults"))[0]->AsObject();
			TestEqual(TEXT("fault category"), Fault->GetStringField(TEXT("category")), FString(TEXT("StaleEpoch")));
			TestEqual(TEXT("fault source"), Fault->GetStringField(TEXT("source")), FString(TEXT("TestSystem")));
			TestEqual(TEXT("fault detail"), (int32)Fault->GetNumberField(TEXT("detail")), 7);
			TestFalse(TEXT("passed is false"), Json->GetBoolField(TEXT("passed")));
		}
	}

	// -StealthHarnessOut absent: no file is written, the result is still evaluated.
	{
		const FStealthFixedReplicationProbe Probe(EHarnessReplicationSystem::Legacy);
		FStealthHarnessScenarioRunner Runner(Probe, TEXT("Smoke"), TEXT("Development"), FString());
		Runner.AssertReplicationSystem();
		TestTrue(TEXT("No path: nothing requested"), Runner.WriteResult(FString()) == EStealthHarnessResultWrite::NotRequested);
		TestTrue(TEXT("No path: the result is still evaluated as passed"), Runner.GetResult().bPassed);
		TestEqual(TEXT("An absent -StealthHarnessOut resolves to no path"), FStealthHarnessScenarioRunner::ResolveResultPath(FString(), true, 0), FString());
	}

	// An unwritable path fails the run (a result that cannot be recorded is a failed run).
	{
		const FString Blocker = FPaths::Combine(Scratch, TEXT("blocker.txt"));
		FFileHelper::SaveStringToFile(TEXT("a file where a folder is needed"), *Blocker);
		const FStealthFixedReplicationProbe Probe(EHarnessReplicationSystem::Legacy);
		FStealthHarnessScenarioRunner Runner(Probe, TEXT("Smoke"), TEXT("Development"), FString());
		Runner.AssertReplicationSystem();
		TestTrue(TEXT("An unwritable path is reported as failed"), Runner.WriteResult(FPaths::Combine(Blocker, TEXT("result.json"))) == EStealthHarnessResultWrite::Failed);
		TestFalse(TEXT("An unwritable path fails the run"), Runner.IsPassing());
		TestFalse(TEXT("An unwritable path leaves passed false"), Runner.GetResult().bPassed);
	}

	// Path resolution: a .json value is the file; anything else is a folder with a per-role file name.
	TestEqual(TEXT("A .json value is the file"), FStealthHarnessScenarioRunner::ResolveResultPath(TEXT("C:/x/out.json"), true, 0), FString(TEXT("C:/x/out.json")));
	TestEqual(TEXT("A folder gets server-result.json for the host"), FStealthHarnessScenarioRunner::ResolveResultPath(TEXT("C:/x/run"), true, 0), FString(TEXT("C:/x/run/server-result.json")));
	TestEqual(TEXT("A folder gets client<N>-result.json for a client"), FStealthHarnessScenarioRunner::ResolveResultPath(TEXT("C:/x/run"), false, 2), FString(TEXT("C:/x/run/client2-result.json")));

	// A run that never asserted the replication system is not a pass.
	{
		const FStealthFixedReplicationProbe Probe(EHarnessReplicationSystem::Legacy);
		FStealthHarnessScenarioRunner Runner(Probe, TEXT("Smoke"), TEXT("Development"), FString());
		Runner.WriteResult(FString());
		TestFalse(TEXT("Never asserted: not passed"), Runner.GetResult().bPassed);
	}

	IFileManager::Get().DeleteDirectory(*Scratch, false, true);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
