// Story iif-003 (production/epics/input-interaction-framework/story-003-action-hub-and-predicate-registry.md), ADR-0011
// section 6: UActionHub registers predicates weakly and fires native events only.
//   - A stub ICommandPredicate per EActionType: its verdict (Allow, AllowNoEffect, Reject(Busy)) is returned.
//   - A type with no predicate, or whose predicate object was destroyed (stale weak pointer): Reject(NotNow), no crash,
//     exactly one CommandNoPredicate fault per resolve.
//   - Pinned: re-registering replaces; registering null is refused with a fault; CanBegin is side-effect-free (the stub
//     changes nothing but its own call counter, and the hub's state is unchanged by a resolve).
//   - Reflection: NotBlueprintType / NotBlueprintable, zero FMulticastDelegateProperty or BlueprintAssignable
//     properties, and three native TMulticastDelegate events with the exact signatures (static_asserts).
// Lives in the harness (not StealthGameplay/Private/Tests as first planned) because the stub predicate must be a
// UObject for the weak registration to go stale, and test UObjects belong to the Shipping-excluded harness.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Input/StubDoubles.h"
#include "Server/ActionHub.h"
#include "Server/Testing/StealthFaultSinkTestSupport.h"
#include "UObject/StrongObjectPtr.h"
#include "View/Core/StealthPawn.h"
#include <type_traits>

static_assert(std::is_same_v<decltype(UActionHub::OnActionStarted), TMulticastDelegate<void(AStealthPawn*, EActionType)>>, "OnActionStarted(AStealthPawn*, EActionType)");
static_assert(std::is_same_v<decltype(UActionHub::OnActionCompleted), TMulticastDelegate<void(AStealthPawn*, EActionType)>>, "OnActionCompleted(AStealthPawn*, EActionType)");
static_assert(std::is_same_v<decltype(UActionHub::OnActionCancelled), TMulticastDelegate<void(AStealthPawn*, EActionType, ECancelReason)>>,
	"OnActionCancelled(AStealthPawn*, EActionType, ECancelReason)");

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputActionHubPredicateRegistryTest, "StealthGame.Input.ActionHubPredicateRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInputActionHubPredicateRegistryTest::RunTest(const FString& Parameters)
{
	// Arrange: a hub (no world needed for the registry), a recording fault sink, one stub per action type.
	TStrongObjectPtr<UActionHub> Hub(NewObject<UActionHub>(GetTransientPackage()));
	FStealthRecordingFaultSink Faults;
	Hub->SetFaultSink(&Faults);
	const AStealthPawn& Pawn = *GetDefault<AStealthPawn>();
	const FCommandPayload Cmd;

	const FPredicateResult Verdicts[] = { FPredicateResult::Allow(), FPredicateResult::AllowNoEffect(), FPredicateResult::Reject(ECommandRejectCode::Busy),
		FPredicateResult::Allow(), FPredicateResult::Reject(ECommandRejectCode::Busy) };
	static_assert(UE_ARRAY_COUNT(Verdicts) == GActionTypeCount, "one verdict per EActionType");
	TArray<TStrongObjectPtr<UStubCommandPredicate>> Stubs;
	for (int32 Index = 0; Index < GActionTypeCount; ++Index)
	{
		UStubCommandPredicate* Stub = NewObject<UStubCommandPredicate>(GetTransientPackage());
		Stub->Verdict = Verdicts[Index];
		Stubs.Emplace(Stub);
		TestTrue(FString::Printf(TEXT("type %d registers"), Index), Hub->RegisterPredicate(static_cast<EActionType>(Index), TWeakInterfacePtr<ICommandPredicate>(Stub)));
	}

	// Act + assert: each registered verdict is returned, one call per resolve, no fault.
	for (int32 Index = 0; Index < GActionTypeCount; ++Index)
	{
		const FPredicateResult Got = Hub->ResolvePredicate(static_cast<EActionType>(Index), Pawn, Cmd);
		TestTrue(FString::Printf(TEXT("type %d returns its predicate's verdict"), Index), Got == Verdicts[Index]);
		TestEqual(FString::Printf(TEXT("type %d predicate called once"), Index), Stubs[Index]->Calls, 1);
	}
	TestTrue(TEXT("no fault while every predicate is live"), Faults.GetRecords().IsEmpty());

	// Missing: unregister Prepare -> Reject(NotNow), one fault.
	Hub->UnregisterPredicate(EActionType::Prepare);
	const FPredicateResult Missing = Hub->ResolvePredicate(EActionType::Prepare, Pawn, Cmd);
	TestTrue(TEXT("a missing predicate resolves Reject(NotNow)"), Missing == FPredicateResult::Reject(ECommandRejectCode::NotNow));
	TestTrue(TEXT("one CommandNoPredicate fault naming Prepare"), Faults.GetRecords().Num() == 1 && Faults.GetRecords()[0].Fault == EStealthFault::CommandNoPredicate
		&& Faults.GetRecords()[0].Detail == static_cast<int64>(EActionType::Prepare));

	// Stale: the Attack predicate object is destroyed -> Reject(NotNow), no crash, one more fault.
	Stubs[static_cast<int32>(EActionType::Attack)]->MarkAsGarbage();
	TestFalse(TEXT("the destroyed predicate no longer counts as registered"), Hub->HasPredicate(EActionType::Attack));
	const FPredicateResult Stale = Hub->ResolvePredicate(EActionType::Attack, Pawn, Cmd);
	TestTrue(TEXT("a stale predicate resolves Reject(NotNow)"), Stale == FPredicateResult::Reject(ECommandRejectCode::NotNow));
	TestEqual(TEXT("the stale resolve reported exactly one more fault"), Faults.GetRecords().Num(), 2);
	TestEqual(TEXT("the destroyed predicate was not called"), Stubs[static_cast<int32>(EActionType::Attack)]->Calls, 1);

	// Pinned: re-registering replaces.
	UStubCommandPredicate* Replacement = NewObject<UStubCommandPredicate>(GetTransientPackage());
	TStrongObjectPtr<UStubCommandPredicate> KeepReplacement(Replacement);
	Replacement->Verdict = FPredicateResult::Reject(ECommandRejectCode::Cooldown);
	Hub->RegisterPredicate(EActionType::Interact, TWeakInterfacePtr<ICommandPredicate>(Replacement));
	TestTrue(TEXT("re-registering a type replaces its predicate"), Hub->ResolvePredicate(EActionType::Interact, Pawn, Cmd) == FPredicateResult::Reject(ECommandRejectCode::Cooldown));
	TestEqual(TEXT("the replaced predicate is no longer called"), Stubs[static_cast<int32>(EActionType::Interact)]->Calls, 1);

	// Pinned: registering null is refused with a fault and keeps the current registration.
	TestFalse(TEXT("registering a null predicate is refused"), Hub->RegisterPredicate(EActionType::Interact, TWeakInterfacePtr<ICommandPredicate>()));
	TestEqual(TEXT("the null registration reported a fault"), Faults.GetRecords().Num(), 3);
	TestTrue(TEXT("the previous registration is kept"), Hub->HasPredicate(EActionType::Interact));

	// Side-effect-free: a resolve changes nothing in the hub (same answer twice, no subscriber fired).
	int32 EventsFired = 0;
	Hub->OnActionStarted.AddLambda([&EventsFired](AStealthPawn*, EActionType) { ++EventsFired; });
	Hub->OnActionCancelled.AddLambda([&EventsFired](AStealthPawn*, EActionType, ECancelReason) { ++EventsFired; });
	Hub->OnActionCompleted.AddLambda([&EventsFired](AStealthPawn*, EActionType) { ++EventsFired; });
	const FPredicateResult First = Hub->ResolvePredicate(EActionType::HideEnter, Pawn, Cmd);
	const FPredicateResult Second = Hub->ResolvePredicate(EActionType::HideEnter, Pawn, Cmd);
	TestTrue(TEXT("resolving twice answers the same"), First == Second);
	TestEqual(TEXT("resolving fires no hub event"), EventsFired, 0);

	// Reflection: not a Blueprint type, no reflected delegate property at all.
	const UClass* HubClass = UActionHub::StaticClass();
#if WITH_METADATA
	TestFalse(TEXT("UActionHub is not BlueprintType"), HubClass->GetBoolMetaData(TEXT("BlueprintType")));
	TestFalse(TEXT("UActionHub is not Blueprintable"), HubClass->GetBoolMetaData(TEXT("IsBlueprintBase")));
#endif
	int32 DelegateProperties = 0;
	int32 Assignable = 0;
	for (TFieldIterator<FProperty> It(HubClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		DelegateProperties += CastField<FMulticastDelegateProperty>(*It) ? 1 : 0;
		Assignable += It->HasAnyPropertyFlags(CPF_BlueprintAssignable) ? 1 : 0;
	}
	TestEqual(TEXT("zero FMulticastDelegateProperty on the hub"), DelegateProperties, 0);
	TestEqual(TEXT("zero BlueprintAssignable property on the hub"), Assignable, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS
