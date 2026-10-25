#include "Misc/AutomationTest.h"
#include "StealthPlayToolset.h"
#include "ToolsetRegistry/ToolCallExceptionHandler.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FStealthPlayToolsetSpec, "StealthGame.EditorTools.PlayToolset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	TUniquePtr<UE::ToolsetRegistry::FToolCallExceptionHandler> ExceptionHandler;

	void ExpectExceptionContains(const FString& Message)
	{
		TestTrue(*FString::Printf(TEXT("Error contains '%s'"), *Message), ExceptionHandler->GetException().Contains(Message));
	}

END_DEFINE_SPEC(FStealthPlayToolsetSpec)

void FStealthPlayToolsetSpec::Define()
{
	// These run outside a play session (headless automation), which is the state they assert on.
	BeforeEach([this]()
	{
		ExceptionHandler = MakeUnique<UE::ToolsetRegistry::FToolCallExceptionHandler>();
	});

	AfterEach([this]()
	{
		ExceptionHandler.Reset();
	});

	Describe("ListPlayInstances", [this]()
	{
		It("returns an empty list when no play session is running", [this]()
		{
			TestTrue(TEXT("No PIE instances"), UStealthPlayToolset::ListPlayInstances().IsEmpty());
		});
	});

	Describe("RunPlayConsoleCommand", [this]()
	{
		It("raises when no play session is running", [this]()
		{
			ExceptionHandler->CaptureErrorsIn([]() { UStealthPlayToolset::RunPlayConsoleCommand(0, TEXT("stat fps")); });
			ExpectExceptionContains(TEXT("No running PIE instance"));
		});

		It("raises when the command is empty", [this]()
		{
			ExceptionHandler->CaptureErrorsIn([]() { UStealthPlayToolset::RunPlayConsoleCommand(0, FString()); });
			ExpectExceptionContains(TEXT("Command must not be empty"));
		});
	});
}

#endif
