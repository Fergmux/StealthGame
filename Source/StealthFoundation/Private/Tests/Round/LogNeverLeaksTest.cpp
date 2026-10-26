// Story rmf-014 (production/epics/round-match-framework/story-014-log-never-leaks.md): GDD AC 60 (Foundation half).
// GIVEN a full fixture round of sentinel ids and hashes (queue and Event Log driven together through Lobby,
// Preparing, Active, Ended and Reset with the fixed secret S3), WHEN every log device is captured (all categories,
// all verbosities) and the ClientMessage and trace paths are checked, THEN no captured output contains a sentinel;
// no log file is written before Ended or Reset; and no sub-seed derived from the fixture secret appears in any log
// entry, header or file.
//
// Capture mechanism (RMF Exit A ruling, decision log): log devices by a GLog output device for the whole run;
// ClientMessage and trace channels by a static scan of the queue and Event Log sources, which must contain no
// UE_LOG/UE_LOGF, ClientMessage, TRACE_, CSV_, SCOPE_CYCLE_COUNTER, ensureMsgf or checkf call at all (the
// include-lint rule (i) is the CI half). A planted leaking sink is the negative control: the capture must see it.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/CriticalSection.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Server/Testing/StealthNegativeControl.h"
#include "Server/Testing/StealthQueueTestRig.h"
#include "Server/Testing/StealthRoundLogTestSupport.h"

DEFINE_LOG_CATEGORY_STATIC(LogStealthRoundLeakControl, Log, All);

namespace LogNeverLeaksTestData
{
	/** Captures every line of every category and verbosity while in scope. */
	class FAllLinesCapture : public FOutputDevice
	{
	public:
		FAllLinesCapture() { GLog->AddOutputDevice(this); bArmed = true; }
		virtual ~FAllLinesCapture() override { Stop(); }
		void Stop()
		{
			if (bArmed)
			{
				bArmed = false;
				GLog->RemoveOutputDevice(this);
			}
		}
		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			if (bArmed && Message != nullptr)
			{
				FScopeLock Lock(&Guard);
				Lines.Add(FString::Printf(TEXT("%s: %s"), *Category.ToString(), Message));
			}
		}
		virtual bool CanBeUsedOnAnyThread() const override { return true; }
		virtual bool CanBeUsedOnMultipleThreads() const override { return true; }
		TArray<FString> GetLines() const { FScopeLock Lock(&Guard); return Lines; }

	private:
		mutable FCriticalSection Guard;
		TArray<FString> Lines;
		bool bArmed = false;
	};

	/** Every decimal and hex rendering of a value a leak could take. */
	void AddRenderings(TArray<FString>& Out, uint64 Value)
	{
		Out.Add(FString::Printf(TEXT("%llu"), Value));
		Out.Add(FString::Printf(TEXT("%llx"), Value));
		Out.Add(FString::Printf(TEXT("%llX"), Value));
		Out.Add(FString::Printf(TEXT("%016llx"), Value));
	}

	/** True when Text contains any of Needles (case-insensitive for hex). */
	bool ContainsAny(const FString& Text, const TArray<FString>& Needles)
	{
		for (const FString& Needle : Needles)
		{
			if (Text.Contains(Needle, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	/** A deliberately leaking sink (test module only, never Shipping): writes an entry's participant to the log. */
	void PlantedLeak(const FRoundLogEntry& Entry)
	{
		UE_LOGF(LogStealthRoundLeakControl, Log, "planted leak participant %u", Entry.Participants[0]);
	}

	/** Macro tokens that would route content to logs, ClientMessage or traces. */
	const TCHAR* const ForbiddenTokens[] = {
		TEXT("UE_LOG"), TEXT("ClientMessage"), TEXT("TRACE_"), TEXT("CSV_"), TEXT("SCOPE_CYCLE_COUNTER"), TEXT("ensureMsgf"), TEXT("checkf"),
	};

	const TCHAR* const ScannedSourceDirs[] = {
		TEXT("Source/StealthFoundation/Private/EventLog"),
		TEXT("Source/StealthFoundation/Private/Queue"),
		TEXT("Source/StealthFoundation/Public/Server/EventLog"),
		TEXT("Source/StealthFoundation/Public/Server/Queue"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundLogNeverLeaksTest, "StealthGame.Round.LogNeverLeaks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundLogNeverLeaksTest::RunTest(const FString& Parameters)
{
	using namespace LogNeverLeaksTestData;
	using namespace RoundTestFixtures;
	using namespace RoundLogFixtures;

	// Arrange: the queue (secret S3) writes into the real Event Log; sentinel ids and hashes everywhere.
	TArray<FString> Sentinels;
	for (const uint32 Id : SentinelParticipantIds)
	{
		AddRenderings(Sentinels, Id);
	}
	AddRenderings(Sentinels, SentinelTuningHash);
	AddRenderings(Sentinels, SentinelLayoutHash);
	AddRenderings(Sentinels, SentinelDealDigest);

	FStealthQueueTestRig Queue(SecretS3);
	FStealthRoundLogRig Log;
	FResolutionQueueDeps Deps;
	Deps.Clock = &Queue.Clock;
	Deps.SubSeeds = &Queue.Seeds;
	Deps.Epoch = &Queue.Epoch;
	Deps.Generations = &Queue.Generations;
	Deps.Log = &Log.Log;
	Deps.FaultSink = &Queue.Faults;
	FResolutionQueueCore Core(Deps);
	FStealthStubResolver Stub;
	Stub.OnResolve = [&Core](const FQueueItem& Item, FQueueKey) { Core.SetVoidCode(1); return EResolveResult::Voided; };
	Core.RegisterResolver(EQueueItemType::PawnCommand, &Stub);
	FStealthStubResolver Grace;
	Core.RegisterTimerResolver(ETimerKind::GraceExpiry, &Grace);
	FStealthSpyLogFileSink Disk;

	// The sub-seeds derivable from S3 for this round (every domain, the attempts the round used).
	TArray<FString> SubSeeds;
	const FRoundLogHeader Header = MakeHeader();
	Queue.Seeds.SetRoundIndex(Header.RoundIndex);

	FAllLinesCapture Capture;
	const auto AttemptDump = [&](const TCHAR* Phase)
	{
		const int32 Before = Disk.Files.Num();
		const bool bWritten = Log.Log.WriteToDisk(Disk, TEXT("round.log"));
		TestFalse(FString::Printf(TEXT("%s: no log file is written"), Phase), bWritten || Disk.Files.Num() != Before);
	};

	// Act: Lobby, Preparing (header at P3), Active (commands and timers from sentinel participants), Ended, Reset.
	AttemptDump(TEXT("Lobby"));
	Log.Log.Append(MakePhaseChanged(PhaseLobby, PhasePreparing, FQueueKey(1, 0), 1.0));
	Log.Log.WriteHeader(Header);
	AttemptDump(TEXT("Preparing"));
	Log.Log.Append(MakePhaseChanged(PhasePreparing, PhaseActive, FQueueKey(2, 0), 2.0));
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		for (const uint32 Id : SentinelParticipantIds)
		{
			const FMailboxId Source = FMailboxId::ForParticipant(FParticipantId(Id));
			Core.Submit(Source, Queue.MakeItem(EQueueItemType::PawnCommand, Id, Source));
		}
		Core.ScheduleAt(Queue.Clock.GetServerTimeSeconds(), Queue.MakeTimer(ETimerKind::GraceExpiry, Frame, FParticipantId(SentinelParticipantIds[Frame % 4])));
		Log.Clock.AdvanceSeconds(1.0);  // the log's rate-limit clock: one Voided entry per source per frame here
		Queue.Clock.Advance(1.0 / 60.0, 1);
		Core.Drain();
		AddRenderings(SubSeeds, Queue.Seeds.GetServerSubSeed(ESeedDomain::SeqOrder, static_cast<uint32>(Queue.Clock.GetServerTickIndex())));
		AddRenderings(SubSeeds, Queue.Seeds.GetServerSubSeed(ESeedDomain::TieRank, static_cast<uint32>(Queue.Clock.GetServerTickIndex())));
	}
	FRoundLogEntry Kill = MakeEntry(ERoundLogType::KillResolved, FQueueKey(500, 1), 30.0);
	Kill.Participants[0] = SentinelParticipantIds[1];
	Kill.Participants[1] = SentinelParticipantIds[2];
	Log.Log.Append(Kill);
	AttemptDump(TEXT("Active"));
	Log.Log.Append(MakeWinLatched(ERoundEndReason::ObjectiveWin, 0, FQueueKey(501, 2), 31.0));
	AttemptDump(TEXT("latch step"));
	Log.Log.Append(MakePhaseChanged(PhaseActive, PhaseEnded, FQueueKey(501, 2), 31.0));
	Log.Gate.bReleasable = true;
	TestTrue(TEXT("Ended: the store's single disk writer runs"), Log.Log.WriteToDisk(Disk, TEXT("round.log")));
	Log.Log.Append(MakePhaseChanged(PhaseEnded, PhaseReset, FQueueKey(600, 0), 40.0));
	TestTrue(TEXT("Reset: a file written exactly at Reset is allowed"), Log.Log.WriteToDisk(Disk, TEXT("round-reset.log")));
	Capture.Stop();

	for (uint32 Domain = 1; Domain <= GStealthSeedDomainMax; ++Domain)
	{
		for (const uint32 Attempt : { 0u, 1u, Header.LayoutAttempt, 2u })
		{
			AddRenderings(SubSeeds, Queue.Seeds.GetServerSubSeed(static_cast<ESeedDomain>(Domain), Attempt));
		}
	}

	// Assert: no sentinel in any captured log line.
	int32 LeakingLines = 0;
	for (const FString& Line : Capture.GetLines())
	{
		LeakingLines += ContainsAny(Line, Sentinels) || ContainsAny(Line, SubSeeds) ? 1 : 0;
	}
	TestEqual(TEXT("no captured log line carries a sentinel id, hash or sub-seed"), LeakingLines, 0);

	// Assert: files only at Ended and Reset; none contains a sub-seed; the header and entries hold none either.
	TestEqual(TEXT("exactly two files (Ended, Reset)"), Disk.Files.Num(), 2);
	for (const TPair<FString, FString>& File : Disk.Files)
	{
		TestFalse(FString::Printf(TEXT("file %s holds no sub-seed"), *File.Key), ContainsAny(File.Value, SubSeeds));
		TestTrue(FString::Printf(TEXT("file %s holds the round (non-vacuous)"), *File.Key), File.Value.Contains(TEXT("[Entries]")) && File.Value.Len() > 1000);
	}
	bool bHeaderClean = true;
	for (uint32 Domain = 1; Domain <= GStealthSeedDomainMax; ++Domain)
	{
		const uint64 SubSeed = Queue.Seeds.GetServerSubSeed(static_cast<ESeedDomain>(Domain), Header.LayoutAttempt);
		bHeaderClean &= Header.LayoutHash != SubSeed && Header.TuningHash != SubSeed && Header.DealDigest != SubSeed;
	}
	TestTrue(TEXT("the header holds the public seed and hashes only, no sub-seed"), bHeaderClean);
	TestTrue(TEXT("the queue wrote Voided entries into the log (the path was exercised)"), Log.Log.Num() > 100);

	// Assert: the queue and Event Log sources call no log, ClientMessage, trace, CSV, stat, ensureMsgf or checkf macro.
	int32 SourcesScanned = 0;
	for (const TCHAR* Dir : ScannedSourceDirs)
	{
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *FPaths::Combine(FPaths::ProjectDir(), Dir), TEXT("*.*"), true, false);
		for (const FString& File : Files)
		{
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *File))
			{
				continue;
			}
			++SourcesScanned;
			for (const TCHAR* Token : ForbiddenTokens)
			{
				// A mention inside a comment is allowed; a call is the token followed by "(".
				TestFalse(FString::Printf(TEXT("%s calls %s"), *FPaths::GetCleanFilename(File), Token), Text.Contains(FString::Printf(TEXT("%s("), Token)) || Text.Contains(FString::Printf(TEXT("%sF("), Token)));
			}
		}
	}
	TestTrue(FString::Printf(TEXT("the source scan read files (%d)"), SourcesScanned), SourcesScanned >= 6);

	// Negative control: a planted leaking sink writes a sentinel to the log; the capture must see it.
	FStealthNegativeControl Control(TEXT("PlantedLogLeak"));
	const FStealthNegativeControlResult Result = Control.Run([&Sentinels, &Kill](FStealthCheckContext& Ctx)
	{
		FAllLinesCapture PlantedCapture;
		PlantedLeak(Kill);
		PlantedCapture.Stop();
		int32 Leaks = 0;
		for (const FString& Line : PlantedCapture.GetLines())
		{
			Leaks += ContainsAny(Line, Sentinels) ? 1 : 0;
		}
		Ctx.TestTrue(TEXT("no captured line carries a sentinel"), Leaks == 0);
	});
	FStealthNegativeControl::Report(*this, Result);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
