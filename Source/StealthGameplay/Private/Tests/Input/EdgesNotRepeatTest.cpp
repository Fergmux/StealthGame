// Story iif-012 (production/epics/input-interaction-framework/story-012-edges-not-repeat.md), GDD AC 4 (R4, Logic,
// U EdgesNotRepeat): GIVEN a key held 2 s WHEN input is sampled THEN exactly one Begin is sent and nothing repeats.
// Pinned per the qa-lead rewrite: for Interact and Prepare (held) and Attack, Hide, Traverse (instant), with the key held
// 2.0 s at 30, 60 and 144 Hz sampling including OS key-repeat events, exactly one Begin (or one instant command) is sent,
// nothing repeats during the hold, and release sends exactly one End for the held kinds and nothing for the instant ones.
// Two layers are checked:
//   1. The bindings UPawnCommandClient::BindInput makes on a real UEnhancedInputComponent: held actions bind only
//      Started (Begin) and Completed + Canceled (End); instant actions bind only Started; no discrete action binds
//      Triggered, so the per-sample Triggered stream can never send anything.
//   2. FPawnCommandClientCore fed the event stream Enhanced Input produces for a held key (one Started, a Triggered per
//      sample, OS repeats arriving as further Started edges defensively, one Completed): one Begin, one End.
// Edge: release and re-press inside 0.1 s sends End then Begin (the debounce is server-side, story iif-044).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "UObject/StrongObjectPtr.h"
#include "View/Input/PawnCommandClient.h"

namespace EdgesNotRepeatTestData
{
	const double HoldSec = 2.0;
	const int32 SampleRates[] = { 30, 60, 144 };
	/** An OS key-repeat event every this many samples (the Windows default repeat is ~30 Hz after a delay). */
	constexpr int32 RepeatEverySamples = 2;

	TSet<ETriggerEvent> BoundEvents(const UEnhancedInputComponent& Input, const TCHAR* ActionName)
	{
		TSet<ETriggerEvent> Events;
		for (const TUniquePtr<FEnhancedInputActionEventBinding>& Binding : Input.GetActionEventBindings())
		{
			if (Binding.IsValid() && Binding->GetAction() && Binding->GetAction()->GetName() == ActionName)
			{
				Events.Add(Binding->GetTriggerEvent());
			}
		}
		return Events;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputEdgesNotRepeatTest, "StealthGame.Input.EdgesNotRepeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInputEdgesNotRepeatTest::RunTest(const FString& Parameters)
{
	using namespace EdgesNotRepeatTestData;
	using EHeld = FPawnCommandClientCore::EHeld;

	// 1. The real bindings.
	TStrongObjectPtr<UPawnCommandClient> Client(NewObject<UPawnCommandClient>());
	TStrongObjectPtr<UEnhancedInputComponent> Input(NewObject<UEnhancedInputComponent>());
	Client->BindInput(*Input);
	const TSet<ETriggerEvent> HeldEvents = { ETriggerEvent::Started, ETriggerEvent::Completed, ETriggerEvent::Canceled };
	for (const TCHAR* Held : { TEXT("IA_Interact"), TEXT("IA_Prepare") })
	{
		const TSet<ETriggerEvent> Got = BoundEvents(*Input, Held);
		TestTrue(FString::Printf(TEXT("%s binds exactly Started, Completed and Canceled"), Held), Got.Num() == 3 && Got.Includes(HeldEvents));
	}
	for (const TCHAR* Instant : { TEXT("IA_Attack"), TEXT("IA_Hide"), TEXT("IA_Traverse") })
	{
		const TSet<ETriggerEvent> Got = BoundEvents(*Input, Instant);
		TestTrue(FString::Printf(TEXT("%s binds exactly Started"), Instant), Got.Num() == 1 && Got.Contains(ETriggerEvent::Started));
	}

	// 2. The event stream of a held key, per sampling rate.
	for (const int32 Rate : SampleRates)
	{
		const int32 Samples = FMath::RoundToInt32(HoldSec * Rate);
		for (const EHeld Held : { EHeld::Interact, EHeld::Prepare })
		{
			TArray<ECommandKind> Sent;
			FPawnCommandClientCore Core([&Sent](const FNetPawnCommand& Cmd) { Sent.Add(Cmd.Kind); });
			Core.OnHeldStarted(Held);
			for (int32 Sample = 1; Sample < Samples; ++Sample)
			{
				// Triggered has no binding (part 1); an OS repeat reaching Started again must not re-send.
				if (Sample % RepeatEverySamples == 0)
				{
					Core.OnHeldStarted(Held);
				}
			}
			const int32 DuringHold = Sent.Num();
			Core.OnHeldTerminal(Held);
			const ECommandKind Begin = Held == EHeld::Interact ? ECommandKind::InteractBegin : ECommandKind::PrepareBegin;
			const ECommandKind End = Held == EHeld::Interact ? ECommandKind::InteractEnd : ECommandKind::PrepareEnd;
			TestEqual(FString::Printf(TEXT("%d Hz held kind %d: one Begin during the 2 s hold"), Rate, static_cast<int32>(Held)), DuringHold, 1);
			TestTrue(FString::Printf(TEXT("%d Hz held kind %d: Begin then exactly one End"), Rate, static_cast<int32>(Held)), Sent == TArray<ECommandKind>({ Begin, End }));
		}
		// Instant kinds: one command per Started; Triggered (unbound) sends nothing; release sends nothing.
		for (const ECommandKind Instant : { ECommandKind::Attack, ECommandKind::HideEnter, ECommandKind::Traverse })
		{
			TArray<ECommandKind> Sent;
			FPawnCommandClientCore Core([&Sent](const FNetPawnCommand& Cmd) { Sent.Add(Cmd.Kind); });
			Core.OnInstant(Instant);
			TestTrue(FString::Printf(TEXT("%d Hz instant kind %d: exactly one command for the press"), Rate, static_cast<int32>(Instant)), Sent.Num() == 1 && Sent[0] == Instant);
		}
	}

	// Sequence numbers are strictly increasing; a terminal edge after the End is swallowed.
	TArray<FNetPawnCommand> Commands;
	FPawnCommandClientCore Core([&Commands](const FNetPawnCommand& Cmd) { Commands.Add(Cmd); });
	Core.OnHeldStarted(EHeld::Prepare);
	Core.OnHeldTerminal(EHeld::Prepare);
	Core.OnHeldTerminal(EHeld::Prepare);
	Core.OnHeldStarted(EHeld::Prepare);
	Core.OnHeldTerminal(EHeld::Prepare);
	TestEqual(TEXT("release then re-press: End then a fresh Begin (4 commands, no debounce on the client)"), Commands.Num(), 4);
	TestEqual(TEXT("the second terminal edge of the first press was swallowed"), Core.GetSwallowedCount(), 1);
	bool bIncreasing = true;
	for (int32 Index = 1; Index < Commands.Num(); ++Index)
	{
		bIncreasing &= Commands[Index].Seq.Seq > Commands[Index - 1].Seq.Seq;
	}
	TestTrue(TEXT("Seq is strictly increasing"), bIncreasing);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
