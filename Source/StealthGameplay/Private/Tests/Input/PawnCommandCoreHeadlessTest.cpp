// Story iif-002 (production/epics/input-interaction-framework/story-002-pawn-command-core-skeleton.md), ADR-0011
// section 1: FPawnCommandCore runs headless with an injected clock and a stub queue.
//   - InteractBegin, clock +0.5 s, InteractEnd: the stub queue records exactly two PawnCommand items (Begin, then End)
//     with the given payload, and every time read went through the injected clock.
//   - "Runs headless" is not observable by running inside the editor, so the include scan of the core's own files is
//     part of the test: no UWorld, AActor, UActorComponent, timer-manager or engine-time header, and no engine-time
//     call; a seeded negative fixture must fail the same scanner.
//   - Pinned gaps: a null queue or clock is a construction error (fault, every submit refused, no crash); a clock
//     that goes backwards is reported and clamped (never back-dated).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Server/Core/PawnCommandCore.h"
#include "Server/StealthClock.h"
#include "Server/Testing/StealthFaultSinkTestSupport.h"

namespace PawnCommandCoreHeadlessTestData
{
	/** A clock that counts its reads and may be moved backwards (the fake clock refuses that). */
	class FCountingClock : public IStealthClock
	{
	public:
		virtual double GetServerTimeSeconds() const override { ++Reads; return Seconds; }
		virtual uint64 GetServerTickIndex() const override { ++Reads; return Tick; }

		double Seconds = 100.0;
		uint64 Tick = 0;
		mutable int32 Reads = 0;
	};

	/** Records every Submit and ScheduleAt. */
	class FRecordingQueue : public IPawnCommandQueue
	{
	public:
		struct FRecord
		{
			FMailboxId Mailbox;
			FQueueItem Item;
		};

		virtual void Submit(FMailboxId Mailbox, const FQueueItem& Item) override { Submitted.Add({ Mailbox, Item }); }
		virtual void ScheduleAt(double DueServerTime, const FQueueItem& Item) override { ++Scheduled; }

		TArray<FRecord> Submitted;
		int32 Scheduled = 0;
	};

	/** The include and token scan of story iif-002: forbidden include targets and engine-time tokens. */
	TArray<FString> ScanForEngineDependencies(const FString& Text)
	{
		static const TCHAR* const ForbiddenIncludes[] = {
			TEXT("Engine/World.h"), TEXT("GameFramework/Actor.h"), TEXT("Components/ActorComponent.h"), TEXT("TimerManager.h"),
			TEXT("Engine/TimerHandle.h"), TEXT("HAL/PlatformTime.h"), TEXT("Misc/App.h"), TEXT("Engine/Engine.h"),
		};
		static const TCHAR* const ForbiddenTokens[] = {
			TEXT("UWorld"), TEXT("GetWorld"), TEXT("AActor"), TEXT("UActorComponent"), TEXT("FTimerManager"), TEXT("FPlatformTime"),
			TEXT("FApp::"), TEXT("GetTimeSeconds"), TEXT("GEngine"),
		};
		TArray<FString> Findings;
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, false);
		bool bInBlock = false;
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			// Strip comments (doc comments may name what the core must not use).
			FString Code;
			FString Line = Lines[Index];
			while (!Line.IsEmpty())
			{
				if (bInBlock)
				{
					const int32 End = Line.Find(TEXT("*/"));
					if (End == INDEX_NONE) { Line.Empty(); break; }
					Line.RightChopInline(End + 2);
					bInBlock = false;
					continue;
				}
				const int32 Block = Line.Find(TEXT("/*"));
				const int32 LineComment = Line.Find(TEXT("//"));
				if (LineComment != INDEX_NONE && (Block == INDEX_NONE || LineComment < Block)) { Code += Line.Left(LineComment); break; }
				if (Block != INDEX_NONE) { Code += Line.Left(Block); Line.RightChopInline(Block + 2); bInBlock = true; continue; }
				Code += Line;
				break;
			}
			for (const TCHAR* Include : ForbiddenIncludes)
			{
				if (Code.Contains(TEXT("#include")) && Code.Contains(Include))
				{
					Findings.Add(FString::Printf(TEXT("line %d includes %s"), Index + 1, Include));
				}
			}
			for (const TCHAR* Token : ForbiddenTokens)
			{
				if (!Code.Contains(TEXT("#include")) && Code.Contains(Token, ESearchCase::CaseSensitive))
				{
					Findings.Add(FString::Printf(TEXT("line %d uses %s"), Index + 1, Token));
				}
			}
		}
		return Findings;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputPawnCommandCoreHeadlessTest, "StealthGame.Input.PawnCommandCoreHeadless",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInputPawnCommandCoreHeadlessTest::RunTest(const FString& Parameters)
{
	using namespace PawnCommandCoreHeadlessTestData;

	// Arrange: a core with an injected clock, a recording queue and a recording fault sink; no UWorld anywhere.
	FCountingClock Clock;
	FRecordingQueue Queue;
	FStealthRecordingFaultSink Faults;
	FPawnCommandCoreDeps Deps;
	Deps.Clock = &Clock;
	Deps.Queue = &Queue;
	Deps.FaultSink = &Faults;
	FPawnCommandCore Core(Deps);
	TestTrue(TEXT("the core is valid with a clock and a queue"), Core.IsValid());

	FCommandPayload Payload;
	Payload.Target = FSubjectRef::ForCourtSocket(7);
	Payload.ClientSeq.PossessionEpoch = 3;
	Payload.ClientSeq.Seq = 11;
	Payload.ControlEpoch = 5;
	FCommandOrigin Origin = FCommandOrigin::ServerLocal();

	// Act: InteractBegin, the clock advances 0.5 s, InteractEnd.
	const int32 ReadsBefore = Clock.Reads;
	TestTrue(TEXT("InteractBegin submitted"), Core.Submit(Origin, ECommandKind::InteractBegin, Payload));
	const double BeginTime = Core.GetLastSubmitServerTime();
	Clock.Seconds += 0.5;
	FCommandPayload EndPayload = Payload;
	EndPayload.ClientSeq.Seq = 12;
	TestTrue(TEXT("InteractEnd submitted"), Core.Submit(Origin, ECommandKind::InteractEnd, EndPayload));

	// Assert: exactly two items, Begin then End, with the given payload; every time read through the clock.
	if (TestEqual(TEXT("the stub queue recorded exactly two items"), Queue.Submitted.Num(), 2))
	{
		const ECommandKind Kinds[] = { ECommandKind::InteractBegin, ECommandKind::InteractEnd };
		const uint32 Seqs[] = { 11, 12 };
		for (int32 Index = 0; Index < 2; ++Index)
		{
			const FQueueItem& Item = Queue.Submitted[Index].Item;
			TestEqual(FString::Printf(TEXT("item %d is a PawnCommand"), Index), static_cast<int32>(Item.Type), static_cast<int32>(EQueueItemType::PawnCommand));
			TestTrue(FString::Printf(TEXT("item %d went to the server-local mailbox"), Index), Queue.Submitted[Index].Mailbox == FMailboxId::ServerLocal());
			const FCommandPayload* Got = Item.Payload.TryGet<FCommandPayload>();
			if (TestNotNull(FString::Printf(TEXT("item %d carries an FCommandPayload"), Index), Got))
			{
				TestEqual(FString::Printf(TEXT("item %d kind"), Index), static_cast<int32>(Got->Kind), static_cast<int32>(Kinds[Index]));
				TestEqual(FString::Printf(TEXT("item %d target court socket"), Index), static_cast<int32>(Got->Target.CourtSocketId), 7);
				TestEqual(FString::Printf(TEXT("item %d client seq"), Index), Got->ClientSeq.Seq, Seqs[Index]);
				TestEqual(FString::Printf(TEXT("item %d control epoch"), Index), Got->ControlEpoch, 5u);
				TestTrue(FString::Printf(TEXT("item %d has the null reply sink"), Index), Got->Sink.Index == 0);
			}
		}
	}
	TestEqual(TEXT("the Begin was stamped at the injected time"), BeginTime, 100.0);
	TestEqual(TEXT("the End was stamped 0.5 s later, from the injected clock"), Core.GetLastSubmitServerTime(), 100.5);
	TestEqual(TEXT("one clock read per submit, all through the injected clock"), Clock.Reads - ReadsBefore, 2);
	TestEqual(TEXT("nothing was scheduled by the skeleton"), Queue.Scheduled, 0);
	TestEqual(TEXT("the slot stays Idle in the skeleton"), static_cast<int32>(Core.GetSlotState()), static_cast<int32>(EPawnSlotState::Idle));
	TestTrue(TEXT("no fault on the happy path"), Faults.GetRecords().IsEmpty());

	// Pinned: a clock that goes backwards is reported and clamped, never back-dated.
	Clock.Seconds = 90.0;
	Core.Submit(Origin, ECommandKind::PrepareBegin, Payload);
	TestEqual(TEXT("a backwards clock is clamped to the last time read"), Core.GetLastSubmitServerTime(), 100.5);
	TestTrue(TEXT("the regression was reported as CommandMisuse(ClockRegressed)"),
		Faults.GetRecords().Num() == 1 && Faults.GetRecords()[0].Fault == EStealthFault::CommandMisuse && Faults.GetRecords()[0].Detail == static_cast<int64>(EPawnCommandMisuse::ClockRegressed));

	// Pinned: a null queue is a construction error, not a crash.
	FStealthRecordingFaultSink NullFaults;
	FPawnCommandCoreDeps NoQueue;
	NoQueue.Clock = &Clock;
	NoQueue.FaultSink = &NullFaults;
	FPawnCommandCore Broken(NoQueue);
	TestFalse(TEXT("a core without a queue is invalid"), Broken.IsValid());
	TestFalse(TEXT("its submits are refused"), Broken.Submit(Origin, ECommandKind::Attack, Payload));
	TestTrue(TEXT("construction and the refused submit each reported MissingSeam"),
		NullFaults.GetRecords().Num() == 2 && NullFaults.GetRecords()[0].Detail == static_cast<int64>(EPawnCommandMisuse::MissingSeam));

	// The include scan: the core's own files carry no engine-world or engine-time dependency.
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	const TCHAR* const CoreFiles[] = {
		TEXT("Source/StealthGameplay/Public/Server/Core/PawnCommandCore.h"),
		TEXT("Source/StealthGameplay/Private/Core/PawnCommandCore.cpp"),
	};
	for (const TCHAR* File : CoreFiles)
	{
		FString Text;
		if (TestTrue(FString::Printf(TEXT("%s is readable"), File), FFileHelper::LoadFileToString(Text, *FPaths::Combine(Root, File))))
		{
			const TArray<FString> Findings = ScanForEngineDependencies(Text);
			TestTrue(FString::Printf(TEXT("%s has no engine-world/time dependency (%s)"), File, *FString::Join(Findings, TEXT("; "))), Findings.IsEmpty());
		}
	}
	// Seeded negative: the same scanner must catch a UWorld include and an engine-time call.
	const FString Seeded = TEXT("#include \"Engine/World.h\"\ndouble Now() { return FPlatformTime::Seconds(); }\n");
	TestEqual(TEXT("the seeded negative fixture fails the scan (2 findings)"), ScanForEngineDependencies(Seeded).Num(), 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS
