// Story iif-071 (production/epics/input-interaction-framework/story-071-context-swap-on-state-and-possession.md), logic
// half: the pure role-context gate StealthOwnerContext::UseInformantContext (ADR-0011 section 9 amendment, ADR-0021
// section 8). IMC_Informant only for a raw Informant state, and then only when no presenter is bound, the presenter shows
// Informant, or the hold cap has elapsed; a return to Active is immediate. The PIE half (the swap within one frame of
// the C-TeamView update, and the flush) is StealthGame.Input.PIE.ContextSwapOnStateAndPossession.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "View/Core/OwnerContextGate.h"

namespace OwnerContextGateTestData
{
	constexpr double CapSec = 3.0;
	constexpr double Arrival = 10.0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputOwnerContextGateTest, "StealthGame.Input.OwnerContextGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInputOwnerContextGateTest::RunTest(const FString& Parameters)
{
	using namespace OwnerContextGateTestData;
	using StealthOwnerContext::UseInformantContext;

	TestFalse(TEXT("Active never selects IMC_Informant"), UseInformantContext(EParticipantState::Active, false, EPresentedOwnRole::Unknown, Arrival, Arrival, CapSec));
	TestFalse(TEXT("RoundEnded never selects IMC_Informant"), UseInformantContext(EParticipantState::RoundEnded, false, EPresentedOwnRole::Unknown, Arrival, Arrival, CapSec));
	TestTrue(TEXT("Informant with no presenter bound: at once (the MVP)"), UseInformantContext(EParticipantState::Informant, false, EPresentedOwnRole::Unknown, Arrival, Arrival, CapSec));
	TestFalse(TEXT("Informant, presenter still showing Active, inside the cap: keep IMC_Active"),
		UseInformantContext(EParticipantState::Informant, true, EPresentedOwnRole::Active, Arrival + CapSec - 0.01, Arrival, CapSec));
	TestTrue(TEXT("Informant, presenter shows Informant: at once"), UseInformantContext(EParticipantState::Informant, true, EPresentedOwnRole::Informant, Arrival, Arrival, CapSec));
	TestTrue(TEXT("Informant, presenter lagging, cap elapsed: swap"), UseInformantContext(EParticipantState::Informant, true, EPresentedOwnRole::Active, Arrival + CapSec, Arrival, CapSec));
	TestFalse(TEXT("a return to Active is immediate, whatever the presenter shows"), UseInformantContext(EParticipantState::Active, true, EPresentedOwnRole::Informant, Arrival, Arrival, CapSec));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
