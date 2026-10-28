// Story oc-005 (production/epics/objectives-chain/story-005-service-shell-and-composition-root.md), AC1 / AC-OC-01
// PreparingFailsOnBadChain:
//   service  UObjectivesService::ValidateSelectedChain over a stub chain source: an invalid chain asset, two cooked chains with
//            no selection rule and zero cooked chains each fail with ObjectiveChainInvalid; HandleLayoutReady then binds nothing
//            and the stub LSC read count stays 0. Exactly one valid chain succeeds and binds (read count 1).
//   Round    the composition root's mapping: on a failure Round's Preparing receives NotifyObjectiveChainInvalid and aborts
//            PreparingFailed(ObjectiveChainInvalid) (the archived slice's code), on the real Round core (Exit B rig).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "../Round/StealthRoundExitBTestRig.h"
#include "ObjectivesQueueRig.h"

namespace ObjectivesPreparingFailsOnBadChainTestData
{
	using namespace StealthObjectivesTest;

	enum class ECase : uint8
	{
		Invalid,
		TwoChains,
		NoChain,
		OneValid,
	};

	const TCHAR* NameOf(ECase Case)
	{
		switch (Case)
		{
		case ECase::Invalid: return TEXT("an invalid chain");
		case ECase::TwoChains: return TEXT("two cooked chains, no selection rule");
		case ECase::NoChain: return TEXT("zero cooked chains");
		default: return TEXT("exactly one valid chain");
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesPreparingFailsOnBadChainTest, "StealthGame.Objectives.PreparingFailsOnBadChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesPreparingFailsOnBadChainTest::RunTest(const FString& Parameters)
{
	using namespace ObjectivesPreparingFailsOnBadChainTestData;

	for (const ECase Case : { ECase::Invalid, ECase::TwoChains, ECase::NoChain, ECase::OneValid })
	{
		// Arrange: the service over the stubs, with the case's cooked chains.
		FQueueRig Rig;
		TStrongObjectPtr<UObjectiveChainData> Second(MakeCacaoChain());
		TStrongObjectPtr<UObjectiveChainData> Broken(MakeCacaoChain([](UObjectiveChainData& C) { C.Definitions[0].CardTitle = FText::GetEmpty(); }));
		switch (Case)
		{
		case ECase::Invalid: Rig.Chains.Chains = { Broken.Get() }; break;
		case ECase::TwoChains: Rig.Chains.Chains = { Rig.Chain.Get(), Second.Get() }; break;
		case ECase::NoChain: Rig.Chains.Chains.Reset(); break;
		default: break;
		}

		// Act: Preparing's selection, then OnLayoutReady.
		const FObjectivesSelection Selection = Rig.Service->ValidateSelectedChain(FStealthServerTuning());
		const bool bBound = Rig.Service->HandleLayoutReady(1, 0);

		// Assert.
		const bool bWantValid = Case == ECase::OneValid;
		TestEqual(FString::Printf(TEXT("%s: valid"), NameOf(Case)), Selection.bValid, bWantValid);
		TestEqual(FString::Printf(TEXT("%s: result"), NameOf(Case)), static_cast<int32>(Selection.Result),
			static_cast<int32>(bWantValid ? EChainValidation::Valid : EChainValidation::ObjectiveChainInvalid));
		TestEqual(FString::Printf(TEXT("%s: bound"), NameOf(Case)), bBound, bWantValid);
		TestEqual(FString::Printf(TEXT("%s: the stub LSC read count"), NameOf(Case)), Rig.Assignment.Reads, bWantValid ? 1 : 0);
		if (Case == ECase::TwoChains)
		{
			TestEqual(TEXT("two chains: the AmbiguousChain clause (never a silent pick)"), static_cast<int32>(Selection.Clause), static_cast<int32>(EChainValidationClause::AmbiguousChain));
		}
		if (Case == ECase::NoChain)
		{
			TestEqual(TEXT("zero chains: NoChain"), static_cast<int32>(Selection.Clause), static_cast<int32>(EChainValidationClause::NoChain));
		}
	}

	// Round receives PreparingFailed(ObjectiveChainInvalid).
	{
		FRoundExitBRig Round;
		Round.OpenLobby();
		Round.SetLayout(RoundTestFixtures::Layout2v2.Team0, RoundTestFixtures::Layout2v2.Team1);
		Round.EnterPreparing();
		Round.Fakes.Session.Script(TEXT("SetAdmissionMode"), 0, 2);
		Round.Fakes.CharacterPool.Script(TEXT("DiscardDeal"));
		Round.Fakes.Session.Script(TEXT("RequestLobbyTravel"));
		Round.Fakes.Roster.Script(TEXT("SetTeamsLocked"), static_cast<int64>(ERosterResult::Applied));
		TestTrue(TEXT("Round is Preparing"), Round.Core->GetPhase() == ERoundPhase::Preparing);
		Round.Core->NotifyObjectiveChainInvalid();
		TestTrue(TEXT("Preparing aborted into Reset"), Round.Core->GetPhase() == ERoundPhase::Reset);
		const TArray<FRoundArchivedSlice>& Archive = Round.Core->GetLogArchive().GetArchive();
		if (TestEqual(TEXT("one archived slice"), Archive.Num(), 1))
		{
			TestTrue(TEXT("the end reason is PreparingFailed"), Archive[0].EndReason == ERoundEndReason::PreparingFailed);
			TestEqual(TEXT("the abort code is ObjectiveChainInvalid"), Archive[0].AbortCode, static_cast<uint8>(ERoundPreparingAbortCode::ObjectiveChainInvalid));
		}
	}
	return true;
}

#endif
