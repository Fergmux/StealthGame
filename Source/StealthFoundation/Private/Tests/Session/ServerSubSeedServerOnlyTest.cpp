// Story st-011 (production/epics/session-travel/story-011-server-sub-seed-server-only.md): GetServerSubSeed is
// unreachable from clients and from generator scope (GDD AC 53, Rule 14, ServerSubSeedServerOnly [S] + [U];
// ADR-0006 section 3 and Validation StealthGame.Random.GeneratorCannotReadSecrets).
//   [U] Without authority (the owning facade's query answers false), off the game thread, and inside FPublicOnlyScope
//       (on the game thread and on a worker thread: the depth is thread_local) GetServerSubSeed returns 0, reports a
//       SeedMisuse fault with the domain, and raises the misuse assert (counted by the expected-error hook). With
//       authority on the game thread outside any scope it answers. The misuse branch is a plain if, not check/ensure.
//   [S] No reflected (UFUNCTION) declaration in any module names a sub-seed, and no file that names GetServerSubSeed
//       declares a UFUNCTION (so no RPC, Blueprint or console function can reach it), lives in a client-safe View
//       folder, or belongs to the presentation module.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Server/Random/StealthServerSeeds.h"
#include "Server/Testing/StealthFaultSinkTestSupport.h"

namespace ServerSubSeedServerOnlyTestData
{
	TArray<uint8> FixtureBytes()
	{
		TArray<uint8> Bytes;
		for (int32 Index = 0; Index < 24; ++Index)
		{
			Bytes.Add(static_cast<uint8>(0x40 + Index));
		}
		return Bytes;
	}

	int32 MisuseFaults(const FStealthRecordingFaultSink& Faults, EStealthSeedMisuse Misuse, ESeedDomain Domain)
	{
		const int64 Detail = (static_cast<int64>(Misuse) << 32) | static_cast<int64>(static_cast<uint32>(Domain));
		return Faults.GetRecords().FilterByPredicate([Detail](const FStealthFaultRecord& R) { return R.Fault == EStealthFault::SeedMisuse && R.Detail == Detail; }).Num();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionServerSubSeedServerOnlyTest, "StealthGame.Session.ServerSubSeedServerOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSessionServerSubSeedServerOnlyTest::RunTest(const FString& Parameters)
{
	using namespace ServerSubSeedServerOnlyTestData;

	// Arrange: an opened seed core with a switchable authority query.
	const TArray<uint8> Bytes = FixtureBytes();
	FDeterministicEntropySource Entropy(Bytes);
	FStealthRecordingFaultSink Faults;
	FStealthServerSeeds Seeds;
	TestTrue(TEXT("Fixture secret opens"), Seeds.Open(Entropy, &Faults));
	bool bAuthority = true;
	Seeds.SetAuthorityQuery([&bAuthority]() { return bAuthority; });
	FStealthSeedMisuseExpectScope ExpectedAsserts;

	// With authority, on the game thread: answers.
	const uint64 Answer = Seeds.GetServerSubSeed(ESeedDomain::SpawnOrder, 1);
	TestTrue(TEXT("With authority on the game thread GetServerSubSeed answers"), Answer != 0ull);

	// Act + assert: no authority (a client).
	bAuthority = false;
	TestEqual(TEXT("Without authority GetServerSubSeed returns 0 (error)"), Seeds.GetServerSubSeed(ESeedDomain::SpawnOrder, 1), 0ull);
	TestEqual(TEXT("Without authority the round seed is refused too"), Seeds.GetRoundSeedCandidate(1, 0), 0);
	TestEqual(TEXT("No-authority misuse raised the assert twice"), ExpectedAsserts.Count(EStealthSeedMisuse::NoAuthority), 2);
	TestEqual(TEXT("No-authority misuse reported with the domain"), MisuseFaults(Faults, EStealthSeedMisuse::NoAuthority, ESeedDomain::SpawnOrder), 1);
	bAuthority = true;

	// Off the game thread.
	uint64 WorkerValue = 1;
	Async(EAsyncExecution::Thread, [&Seeds, &WorkerValue]() { WorkerValue = Seeds.GetServerSubSeed(ESeedDomain::NpcRoutes, 3); }).Wait();
	TestEqual(TEXT("Off the game thread GetServerSubSeed returns 0"), WorkerValue, 0ull);
	TestEqual(TEXT("Off-thread misuse raised the assert"), ExpectedAsserts.Count(EStealthSeedMisuse::NotGameThread), 1);
	TestEqual(TEXT("Off-thread misuse reported with the domain"), MisuseFaults(Faults, EStealthSeedMisuse::NotGameThread, ESeedDomain::NpcRoutes), 1);

	// Recovery: a correct call after the misuse still answers the same value.
	TestEqual(TEXT("A correct call after misuse answers unchanged"), Seeds.GetServerSubSeed(ESeedDomain::SpawnOrder, 1), Answer);
	TestEqual(TEXT("A correct call raises nothing"), ExpectedAsserts.Total(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRandomGeneratorCannotReadSecretsTest, "StealthGame.Random.GeneratorCannotReadSecrets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRandomGeneratorCannotReadSecretsTest::RunTest(const FString& Parameters)
{
	using namespace ServerSubSeedServerOnlyTestData;

	// Arrange.
	const TArray<uint8> Bytes = FixtureBytes();
	FDeterministicEntropySource Entropy(Bytes);
	FStealthRecordingFaultSink Faults;
	FStealthServerSeeds Seeds;
	TestTrue(TEXT("Fixture secret opens"), Seeds.Open(Entropy, &Faults));
	FStealthSeedMisuseExpectScope ExpectedAsserts;

	// Act + assert: inside the Layout Service's scope on the game thread.
	{
		FPublicOnlyScope Generator;
		TestTrue(TEXT("The scope is active"), FPublicOnlyScope::IsActive());
		TestEqual(TEXT("Inside FPublicOnlyScope GetServerSubSeed returns 0"), Seeds.GetServerSubSeed(ESeedDomain::ObjectiveSites, 0), 0ull);
		{
			FPublicOnlyScope Nested;
			TestEqual(TEXT("Nested scopes still refuse"), Seeds.GetServerSubSeed(ESeedDomain::ObjectiveSites, 1), 0ull);
		}
		TestTrue(TEXT("Leaving a nested scope keeps the outer one"), FPublicOnlyScope::IsActive());
	}
	TestFalse(TEXT("The scope ends with its owner"), FPublicOnlyScope::IsActive());
	TestEqual(TEXT("Scope misuse raised the assert for each call"), ExpectedAsserts.Count(EStealthSeedMisuse::InsidePublicOnlyScope), 2);
	TestEqual(TEXT("Scope misuse reported with the domain"), MisuseFaults(Faults, EStealthSeedMisuse::InsidePublicOnlyScope, ESeedDomain::ObjectiveSites), 2);

	// A generator stage on a worker thread inside its own scope: the thread_local depth covers it (the off-thread
	// check would refuse anyway; the scope state itself must be visible on that thread and not leak to this one).
	bool bWorkerScoped = false;
	Async(EAsyncExecution::Thread, [&bWorkerScoped]()
	{
		FPublicOnlyScope Stage;
		bWorkerScoped = FPublicOnlyScope::IsActive();
	}).Wait();
	TestTrue(TEXT("A worker-thread stage sees its own scope (thread_local)"), bWorkerScoped);
	TestFalse(TEXT("A worker's scope does not leak to the game thread"), FPublicOnlyScope::IsActive());
	TestTrue(TEXT("Outside every scope the server answers again"), Seeds.GetServerSubSeed(ESeedDomain::ObjectiveSites, 0) != 0ull);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionServerSubSeedStaticReachTest, "StealthGame.Session.ServerSubSeedStaticReach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSessionServerSubSeedStaticReachTest::RunTest(const FString& Parameters)
{
	// Arrange: every C++ source of the project's modules (tests excluded: they call the API on purpose).
	TArray<FString> Files;
	const FString SourceDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"));
	IFileManager::Get().FindFilesRecursive(Files, *SourceDir, TEXT("*.h"), true, false);
	TArray<FString> Cpp;
	IFileManager::Get().FindFilesRecursive(Cpp, *SourceDir, TEXT("*.cpp"), true, false);
	Files.Append(Cpp);

	// Act.
	int32 Callers = 0;
	TArray<FString> Violations;
	for (const FString& File : Files)
	{
		FString Rel = File;
		FPaths::MakePathRelativeTo(Rel, *(FPaths::ProjectDir()));
		Rel.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (Rel.Contains(TEXT("/Tests/")))
		{
			continue;
		}
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *File))
		{
			continue;
		}
		// No reflected function anywhere names a sub-seed (no RPC, Blueprint or exec entry point to it).
		int32 Search = 0;
		while ((Search = Text.Find(TEXT("UFUNCTION("), ESearchCase::CaseSensitive, ESearchDir::FromStart, Search)) != INDEX_NONE)
		{
			const int32 LineEnd = Text.Find(TEXT(";"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Search);
			const FString Declaration = Text.Mid(Search, LineEnd == INDEX_NONE ? 400 : LineEnd - Search);
			if (Declaration.Contains(TEXT("SubSeed")) || Declaration.Contains(TEXT("ServerSecret")) || Declaration.Contains(TEXT("MatchNonce")))
			{
				Violations.Add(FString::Printf(TEXT("%s: a UFUNCTION names a secret seed API"), *Rel));
			}
			Search += 10;
		}
		if (!Text.Contains(TEXT("GetServerSubSeed"), ESearchCase::CaseSensitive))
		{
			continue;
		}
		++Callers;
		if (Text.Contains(TEXT("UFUNCTION("), ESearchCase::CaseSensitive))
		{
			Violations.Add(FString::Printf(TEXT("%s: names GetServerSubSeed and declares a UFUNCTION"), *Rel));
		}
		if (Rel.Contains(TEXT("/Public/View/")) || Rel.Contains(TEXT("/Private/View/")) || Rel.StartsWith(TEXT("Source/StealthPresentation/")))
		{
			Violations.Add(FString::Printf(TEXT("%s: client-safe or presentation code names GetServerSubSeed"), *Rel));
		}
	}

	// Assert.
	TestTrue(TEXT("The scan found the GetServerSubSeed declaration and its callers"), Callers >= 2);
	for (const FString& Violation : Violations)
	{
		AddError(Violation);
	}
	TestEqual(TEXT("No client-callable path reaches GetServerSubSeed"), Violations.Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
