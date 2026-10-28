// Objectives Chain story oc-014 AC3 (ADR-0020 section 7 steps 1-2 [U]): StealthGame.Objectives.RivalSignalGolden.
// All arithmetic is integer milliseconds (the bounds converted once at tuning freeze); the draw is
// UniformInRange(FSecretStream(GetServerSubSeed(StageSignal, k), Tag("OC_STAGE")), MinMs, MaxMs); the ceiling-division numerator
// is asserted positive (t_c >= t0 and Delta > 0), and the results for k = 1..3 equal pinned golden vectors.
//
// The vectors are at the fixed test secret of StealthGame.Session.SeedDerivationGolden (ServerSecret 0x10..0x1f, MatchNonce
// 0x20..0x27, RoundIndex 0), Active at 100 s and the completion at 200 s. They were re-derived independently by
// tools/analysis/oc_rival_signal_golden.py (its own SipHash-2-4, sub-seed derivation, FSecretStream and Lemire draw; no shared
// code; 2026-10-28 output pinned below):
//   k=1 subseed=0xD7705C629EA0163C delta_ms=44172 release_ms=250000
//   k=2 subseed=0xAAEB13360113B229 delta_ms=66957 release_ms=280000
//   k=3 subseed=0xFDFEC78088411212 delta_ms=112399 release_ms=340000

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ObjectivesCore.h"
#include "ObjectivesRivalSignal.h"
#include "Server/Random/StealthServerSeeds.h"
#include "Server/Testing/StealthFaultSinkTestSupport.h"

namespace RivalSignalGoldenTestData
{
	struct FGolden
	{
		uint32 Ordinal;
		uint64 SubSeed;
		int32 DeltaMs;
		int64 ReleaseMs;
	};

	const FGolden Vectors[] = {
		{ 1, 0xD7705C629EA0163Cull, 44172, 250000 },
		{ 2, 0xAAEB13360113B229ull, 66957, 280000 },
		{ 3, 0xFDFEC78088411212ull, 112399, 340000 },
	};

	/** The IObjectivesSecretDraw seam over an opened FStealthServerSeeds (the production adapter's shape). */
	class FSeedsDraw : public IObjectivesSecretDraw
	{
	public:
		const FStealthServerSeeds* Seeds = nullptr;
		virtual uint64 GetStageSignalSubSeed(uint32 Ordinal) const override { return Seeds->GetServerSubSeed(ESeedDomain::StageSignal, Ordinal); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesRivalSignalGoldenTest, "StealthGame.Objectives.RivalSignalGolden",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesRivalSignalGoldenTest::RunTest(const FString& Parameters)
{
	using namespace RivalSignalGoldenTestData;
	// Arrange: the fixed test secret through the entropy hook.
	TArray<uint8> Bytes;
	for (int32 Index = 0; Index < 24; ++Index)
	{
		Bytes.Add(static_cast<uint8>(0x10 + Index));
	}
	FDeterministicEntropySource Entropy(Bytes);
	FStealthRecordingFaultSink Faults;
	FStealthServerSeeds Seeds;
	if (!TestTrue(TEXT("the fixture secret opens"), Seeds.Open(Entropy, &Faults)))
	{
		return false;
	}
	FSeedsDraw Draw;
	Draw.Seeds = &Seeds;

	// The bounds in whole milliseconds, converted once from the schema defaults (as FObjectivesCore::SetTuning does).
	const FObjectivesTuning_Server Tuning;
	const int32 MinMs = static_cast<int32>(FObjectivesRivalSignal::ToMs(Tuning.RivalSignalDelayMinSec));
	const int32 MaxMs = static_cast<int32>(FObjectivesRivalSignal::ToMs(Tuning.RivalSignalDelayMaxSec));
	const int64 QMs = FObjectivesRivalSignal::ToMs(Tuning.RivalSignalReleaseQuantumSec);
	TestTrue(TEXT("the defaults are 30 000 / 165 000 / 30 000 ms"), MinMs == 30000 && MaxMs == 165000 && QMs == 30000);
	FObjectivesCore Core;
	Core.SetTuning(FObjectivesTuning_Public(), Tuning);
	TestTrue(TEXT("the core converted the same integers at freeze"), Core.GetRivalDelayMinMs() == MinMs && Core.GetRivalDelayMaxMs() == MaxMs && Core.GetRivalQuantumMs() == QMs);

	// Act + assert: the golden vectors.
	for (const FGolden& Golden : Vectors)
	{
		const uint64 SubSeed = Draw.GetStageSignalSubSeed(Golden.Ordinal);
		const int32 Delta = FObjectivesRivalSignal::DrawRivalDelayMs(Golden.Ordinal, Draw, MinMs, MaxMs);
		const int64 Release = FObjectivesRivalSignal::ComputeReleaseMs(100000, 200000, Delta, QMs);
		AddInfo(FString::Printf(TEXT("OC14 k=%u subseed=0x%016llX delta_ms=%d release_ms=%lld"), Golden.Ordinal, SubSeed, Delta, Release));
		TestEqual(*FString::Printf(TEXT("k=%u: the StageSignal sub-seed"), Golden.Ordinal), SubSeed, Golden.SubSeed);
		TestEqual(*FString::Printf(TEXT("k=%u: Delta_ms"), Golden.Ordinal), Delta, Golden.DeltaMs);
		TestEqual(*FString::Printf(TEXT("k=%u: t_rel_ms"), Golden.Ordinal), Release, Golden.ReleaseMs);
		TestEqual(*FString::Printf(TEXT("k=%u: the explicit-sub-seed form agrees"), Golden.Ordinal), FObjectivesRivalSignal::DrawRivalDelayMsFromSubSeed(Golden.SubSeed, MinMs, MaxMs), Golden.DeltaMs);
	}

	// The ceiling numerator must be positive: refused inputs give -1 (never a release).
	TestEqual(TEXT("t_c before t0 is refused"), FObjectivesRivalSignal::ComputeReleaseMs(100000, 99999, 30000, QMs), static_cast<int64>(-1));
	TestEqual(TEXT("a zero delay is refused"), FObjectivesRivalSignal::ComputeReleaseMs(100000, 100000, 0, QMs), static_cast<int64>(-1));
	TestEqual(TEXT("a zero quantum is refused"), FObjectivesRivalSignal::ComputeReleaseMs(100000, 100000, 30000, 0), static_cast<int64>(-1));
	TestEqual(TEXT("an exact grid hit stays on its tick"), FObjectivesRivalSignal::ComputeReleaseMs(0, 30000, 30000, 30000), static_cast<int64>(60000));
	TestEqual(TEXT("one millisecond past a tick goes to the next"), FObjectivesRivalSignal::ComputeReleaseMs(0, 30001, 30000, 30000), static_cast<int64>(90000));
	TestEqual(TEXT("no secret held (sub-seed 0): no draw"), FObjectivesRivalSignal::DrawRivalDelayMsFromSubSeed(0, MinMs, MaxMs), -1);
	TestEqual(TEXT("ToMs rounds to the nearest millisecond"), FObjectivesRivalSignal::ToMs(100.0004), static_cast<int64>(100000));
	return true;
}

#endif
