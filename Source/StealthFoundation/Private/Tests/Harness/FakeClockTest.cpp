// Story tdc-007 (production/epics/tuning-data-config/story-007-stealth-clock-interface.md):
// IStealthClock and the FFakeStealthClock test double (ADR-0008 section 4).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/Testing/StealthFakeClock.h"
#include "Server/Testing/StealthFaultSinkTestSupport.h"

namespace FakeClockTestData
{
	constexpr double StartSeconds = 12.5;
	constexpr uint64 StartTick = 40;
	constexpr double AdvanceBySeconds = 0.25;
	constexpr uint64 AdvanceByTicks = 3;
	/** Iterations of unrelated work done between two reads, so real time passes without a wall-clock wait. */
	constexpr int32 BusyWorkIterations = 200000;

	/** Stand-in plain core: reads only its injected clock, as every core must (ADR-0008 section 4). */
	class FClockReadingCore
	{
	public:
		explicit FClockReadingCore(const IStealthClock& InClock) : Clock(InClock) {}

		/** A result that depends only on the injected clock. */
		FString Sample() const
		{
			return FString::Printf(TEXT("%.6f@%llu"), Clock.GetServerTimeSeconds(), Clock.GetServerTickIndex());
		}

	private:
		const IStealthClock& Clock;
	};

	/** Unrelated work whose result is used, so the optimiser keeps it. */
	uint32 DoUnrelatedWork()
	{
		uint32 Hash = 2166136261u;
		for (int32 I = 0; I < BusyWorkIterations; ++I)
		{
			Hash = (Hash ^ static_cast<uint32>(I)) * 16777619u;
		}
		return Hash;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHarnessFakeClockAdvancesOnlyWhenToldTest, "StealthGame.Harness.FakeClockAdvancesOnlyWhenTold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHarnessFakeClockAdvancesOnlyWhenToldTest::RunTest(const FString& Parameters)
{
	using namespace FakeClockTestData;
	FStealthRecordingFaultSink Faults;

	// Arrange
	FFakeStealthClock Clock(StartSeconds, StartTick);
	const IStealthClock& AsInterface = Clock;
	const FClockReadingCore Core(AsInterface);

	// Act: read, do other work (real time passes, no sleep), read again.
	const double SecondsBefore = AsInterface.GetServerTimeSeconds();
	const uint64 TickBefore = AsInterface.GetServerTickIndex();
	const FString SampleBefore = Core.Sample();
	const uint32 WorkResult = DoUnrelatedWork();
	const double SecondsAfterWork = AsInterface.GetServerTimeSeconds();
	const uint64 TickAfterWork = AsInterface.GetServerTickIndex();
	const FString SampleAfterWork = Core.Sample();

	// Assert: nothing moved without being told.
	TestNotEqual(TEXT("unrelated work ran"), WorkResult, 0u);
	TestEqual(TEXT("initial seconds"), SecondsBefore, StartSeconds);
	TestEqual(TEXT("initial tick"), TickBefore, StartTick);
	TestEqual(TEXT("seconds unchanged while real time passes"), SecondsAfterWork, SecondsBefore);
	TestEqual(TEXT("tick unchanged while real time passes"), TickAfterWork, TickBefore);
	TestEqual(TEXT("core result independent of wall time"), SampleAfterWork, SampleBefore);

	// Act + assert: explicit advances change exactly the advanced quantity, by exactly the amount.
	Clock.AdvanceSeconds(AdvanceBySeconds);
	TestEqual(TEXT("AdvanceSeconds moves time by exactly the amount"), AsInterface.GetServerTimeSeconds(), StartSeconds + AdvanceBySeconds);
	TestEqual(TEXT("AdvanceSeconds leaves the tick alone (no invented tick length)"), AsInterface.GetServerTickIndex(), StartTick);

	Clock.AdvanceTicks(AdvanceByTicks);
	TestEqual(TEXT("AdvanceTicks moves the tick by exactly the amount"), AsInterface.GetServerTickIndex(), StartTick + AdvanceByTicks);
	TestEqual(TEXT("AdvanceTicks leaves time alone"), AsInterface.GetServerTimeSeconds(), StartSeconds + AdvanceBySeconds);

	Clock.Advance(AdvanceBySeconds, AdvanceByTicks);
	TestEqual(TEXT("Advance moves time by the caller's amount"), AsInterface.GetServerTimeSeconds(), StartSeconds + 2.0 * AdvanceBySeconds);
	TestEqual(TEXT("Advance moves the tick by the caller's amount"), AsInterface.GetServerTickIndex(), StartTick + 2 * AdvanceByTicks);
	TestNotEqual(TEXT("core result follows the injected clock"), Core.Sample(), SampleBefore);

	// Edge: two fakes are independent.
	FFakeStealthClock Other;
	TestEqual(TEXT("second fake starts at its own default time"), Other.GetServerTimeSeconds(), 0.0);
	TestEqual(TEXT("second fake starts at its own default tick"), Other.GetServerTickIndex(), static_cast<uint64>(0));
	Other.Advance(1.0, 1);
	TestEqual(TEXT("advancing one fake leaves the other's time"), Clock.GetServerTimeSeconds(), StartSeconds + 2.0 * AdvanceBySeconds);
	TestEqual(TEXT("advancing one fake leaves the other's tick"), Clock.GetServerTickIndex(), StartTick + 2 * AdvanceByTicks);

	// Teardown: no faults expected (ADR-0008 section 4).
	const FStealthFaultTeardownResult Teardown = Faults.EvaluateTeardown({});
	TestTrue(Teardown.Describe(), Teardown.IsClean());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
