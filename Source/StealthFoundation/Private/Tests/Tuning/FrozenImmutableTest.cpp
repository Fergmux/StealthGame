// Story tdc-054 (production/epics/tuning-data-config/story-054-validated-tuning-frozen-immutable.md), GDD AC-32
// (CR-7): a frozen set offers no mutator (const access, compile level) and its TuningHash is unchanged at Ended.
// In-process half: static_asserts that FValidatedTuning is not assignable, that every accessor is a const member
// function returning a value or a const reference, and that the resolve result holds it as const. The other
// half is the compile-probe pair tools/ci/compile-probes/MutateValidatedTuning.cpp (writing a member through a
// const FValidatedTuning& and through a TSharedRef<const FValidatedTuning> fails to compile) and its const-read
// control tools/ci/compile-probes/controls/ReadValidatedTuningControl.cpp (compiles), run by the story tdc-005
// runner. A deliberate const_cast is not covered: that is a code-review matter, not a type-system guarantee.
// The hot-reload path (story tdc-085), the one sanctioned producer of a new set, is not exercised here.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningHasher.h"
#include "Server/StealthTuningPipelineCore.h"
#include "Server/StealthTuningVersion.h"
#include "Server/StealthValidatedTuning.h"
#include "Server/Testing/StealthStubRound.h"
#include "Tests/Tuning/StealthTuningFakePresetSource.h"

#include <type_traits>

// Not assignable in any way: no mutator exists after construction.
static_assert(!std::is_copy_assignable_v<FValidatedTuning>, "FValidatedTuning must not be copy-assignable");
static_assert(!std::is_move_assignable_v<FValidatedTuning>, "FValidatedTuning must not be move-assignable");
static_assert(!std::is_assignable_v<FValidatedTuning&, const FValidatedTuning&>, "FValidatedTuning must not be assignable");
// Every accessor is a const member function returning a value or a const reference.
static_assert(std::is_same_v<decltype(&FValidatedTuning::Values), const FStealthServerTuning& (FValidatedTuning::*)() const>, "Values() is const and returns const&");
static_assert(std::is_same_v<decltype(&FValidatedTuning::TuningHash), uint64 (FValidatedTuning::*)() const>, "TuningHash() is const");
static_assert(std::is_same_v<decltype(&FValidatedTuning::ClientHash), uint64 (FValidatedTuning::*)() const>, "ClientHash() is const");
static_assert(std::is_same_v<decltype(&FValidatedTuning::SchemaVersion), int32 (FValidatedTuning::*)() const>, "SchemaVersion() is const");
static_assert(std::is_same_v<decltype(&FValidatedTuning::PresetId), FName (FValidatedTuning::*)() const>, "PresetId() is const");
static_assert(std::is_same_v<decltype(&FValidatedTuning::Revision), int32 (FValidatedTuning::*)() const>, "Revision() is const");
static_assert(std::is_same_v<decltype(&FValidatedTuning::FallbackLevel), ETuningFallback (FValidatedTuning::*)() const>, "FallbackLevel() is const");
static_assert(std::is_same_v<decltype(&FValidatedTuning::Report), const FValidationReport& (FValidatedTuning::*)() const>, "Report() is const and returns const&");
// The holder: the resolve result carries a pointer to const, convertible to TSharedRef<const FValidatedTuning>.
static_assert(std::is_same_v<decltype(FTuningResolveResult::Validated), TSharedPtr<const FValidatedTuning>>, "the result holds TSharedPtr<const FValidatedTuning>");
static_assert(std::is_same_v<decltype(std::declval<TSharedPtr<const FValidatedTuning>>().ToSharedRef()), TSharedRef<const FValidatedTuning>>, "held as TSharedRef<const FValidatedTuning>");
static_assert(std::is_const_v<std::remove_reference_t<decltype(std::declval<const FValidatedTuning&>().Values())>>, "the values are reached as const");

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTuningFrozenImmutableTest, "StealthGame.Tuning.FrozenImmutable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTuningFrozenImmutableTest::RunTest(const FString& Parameters)
{
	// Arrange: a round on the stub, resolving Default through the pipeline core.
	FStealthTuningFakePresetSource Source;
	Source.Add(FTuningPipelineCore::DefaultPresetId(), FStealthServerTuning(), true);
	FTuningResolveInput Input;
	Input.RequestedPreset = FTuningPipelineCore::DefaultPresetId();
	Input.Source = &Source;

	TSharedPtr<const FValidatedTuning> Frozen;
	FStubRound Round([&Frozen, &Input]() { Frozen = FTuningPipelineCore().Resolve(Input).Validated; });
	Round.EnterPhase(EStealthStubRoundPhase::Preparing);
	Round.BeginResolve();
	if (!TestTrue(TEXT("the set was frozen at Preparing"), Frozen.IsValid()))
	{
		return false;
	}
	const TSharedRef<const FValidatedTuning> Held = Frozen.ToSharedRef();
	const uint64 HashAtPreparing = Held->TuningHash();
	const FStealthServerTuning ValuesAtPreparing = Held->Values();
	Round.OnTuningReady(HashAtPreparing);

	// Act: the round runs to Ended; a second resolve (another server system asking again) makes a new object
	// and leaves the held one untouched.
	Round.EnterPhase(EStealthStubRoundPhase::Playing);
	const TSharedPtr<const FValidatedTuning> Other = FTuningPipelineCore().Resolve(Input).Validated;
	Round.EnterPhase(EStealthStubRoundPhase::Ended);

	// Assert: the frozen hash and values are unchanged at Ended.
	TestEqual(TEXT("the round reached Ended"), static_cast<int32>(Round.GetPhase()), static_cast<int32>(EStealthStubRoundPhase::Ended));
	TestEqual(TEXT("TuningHash unchanged at Ended"), Held->TuningHash(), HashAtPreparing);
	TestEqual(TEXT("the stub received the frozen hash"), Round.GetCalls().FilterByPredicate([](const FStealthStubRoundCall& C) { return C.Kind == EStealthStubRoundCall::OnTuningReady; })[0].TuningHash, HashAtPreparing);
	TestEqual(TEXT("the frozen values still hash to the frozen TuningHash (story tdc-045 hasher)"), FTuningHasher::Hash(Held->Values(), TuningSchemaVersion, false), HashAtPreparing);
	for (const FTuningSchemaRow& Row : GetTuningSchema())
	{
		TestEqual(FString::Printf(TEXT("%s unchanged at Ended"), *FString(Row.Key)), Row.Get(Held->Values()), Row.Get(ValuesAtPreparing));
	}
	TestTrue(TEXT("a second resolve produced a distinct object"), Other.IsValid() && Other.Get() != &Held.Get());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
