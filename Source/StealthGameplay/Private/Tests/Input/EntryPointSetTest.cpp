// Story iif-005 (production/epics/input-interaction-framework/story-005-entry-point-set.md), GDD AC 1 (R1, Logic,
// U EntryPointSet): the command component's server entry points, listed by reflection, are exactly Move, Look,
// Traverse, Interact(Begin/End), Prepare(Begin/End), Attack and Hide.
//
// Decision recorded in StealthPawnCommandComponent.h: every entry point is a UFUNCTION() with no specifiers, so
// reflection lists exactly nine functions (Hide covers HideEnter and HideExit through FHideCommand) and none carries
// an FUNC_Net* flag. Also asserted: exactly one non-replicated UPawnCommandComponent per pawn (the class default
// object), and each discrete entry point submits the matching ECommandKind while Move and Look submit nothing.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/Core/StealthPawnCommandComponent.h"
#include "Server/StealthClock.h"
#include "View/Core/StealthPawn.h"

namespace EntryPointSetTestData
{
	const TCHAR* const ExpectedEntryPoints[] = {
		TEXT("Move"), TEXT("Look"), TEXT("Traverse"), TEXT("InteractBegin"), TEXT("InteractEnd"), TEXT("PrepareBegin"), TEXT("PrepareEnd"),
		TEXT("Attack"), TEXT("Hide"),
	};

	class FFixedClock : public IStealthClock
	{
	public:
		virtual double GetServerTimeSeconds() const override { return 1.0; }
		virtual uint64 GetServerTickIndex() const override { return 1; }
	};

	class FKindQueue : public IPawnCommandQueue
	{
	public:
		virtual void Submit(FMailboxId Mailbox, const FQueueItem& Item) override
		{
			Mailboxes.Add(Mailbox);
			if (const FCommandPayload* Payload = Item.Payload.TryGet<FCommandPayload>())
			{
				Kinds.Add(static_cast<ECommandKind>(Payload->Kind));
			}
		}
		virtual void ScheduleAt(double, const FQueueItem&) override {}

		TArray<ECommandKind> Kinds;
		TArray<FMailboxId> Mailboxes;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputEntryPointSetTest, "StealthGame.Input.EntryPointSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInputEntryPointSetTest::RunTest(const FString& Parameters)
{
	using namespace EntryPointSetTestData;

	// The reflected entry points: exactly the nine, none an RPC.
	TArray<FString> Declared;
	for (TFieldIterator<UFunction> It(UPawnCommandComponent::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		Declared.Add(It->GetName());
		TestFalse(FString::Printf(TEXT("%s carries no FUNC_Net* flag"), *It->GetName()), It->HasAnyFunctionFlags(FUNC_Net | FUNC_NetServer | FUNC_NetClient | FUNC_NetMulticast));
	}
	TArray<FString> Expected;
	for (const TCHAR* Name : ExpectedEntryPoints)
	{
		Expected.Add(Name);
	}
	Declared.Sort();
	Expected.Sort();
	TestTrue(FString::Printf(TEXT("the reflected entry points are exactly the R1 set (got %s)"), *FString::Join(Declared, TEXT(", "))), Declared == Expected);

	// One non-replicated component per pawn, created in the constructor (the class default object carries it).
	AStealthPawn* Defaults = GetMutableDefault<AStealthPawn>();
	TArray<UObject*> Subobjects;
	Defaults->GetDefaultSubobjects(Subobjects);
	TArray<UPawnCommandComponent*> Components;
	for (UObject* Subobject : Subobjects)
	{
		if (UPawnCommandComponent* Command = Cast<UPawnCommandComponent>(Subobject))
		{
			Components.Add(Command);
		}
	}
	TestEqual(TEXT("exactly one UPawnCommandComponent on the pawn"), Components.Num(), 1);
	if (Components.Num() == 1)
	{
		TestTrue(TEXT("it is the default subobject GetPawnCommand returns"), Components[0] == Defaults->GetPawnCommand());
		TestFalse(TEXT("it does not replicate"), Components[0]->GetIsReplicated());
		TestEqual(TEXT("its name is PawnCommand"), Components[0]->GetFName(), UPawnCommandComponent::ComponentName);
	}
	bool bAnyReplicatedProperty = false;
	for (TFieldIterator<FProperty> It(UPawnCommandComponent::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		bAnyReplicatedProperty |= It->HasAnyPropertyFlags(CPF_Net);
	}
	TestFalse(TEXT("the component declares no replicated property"), bAnyReplicatedProperty);

	// Each discrete entry point submits its kind; Move and Look submit nothing (continuous, never queue items).
	FFixedClock Clock;
	FKindQueue Queue;
	UPawnCommandComponent* Component = NewObject<UPawnCommandComponent>();
	FPawnCommandCoreDeps Deps;
	Deps.Clock = &Clock;
	Deps.Queue = &Queue;
	Component->InitializeCore(Deps);

	Component->Move(FVector2D(0.0, 1.0), EGait::Walk);
	Component->Look(FRotator(0.0, 90.0, 0.0));
	TestEqual(TEXT("Move and Look submit nothing"), Queue.Kinds.Num(), 0);
	TestTrue(TEXT("Move keeps the intent"), Component->GetMoveIntent().Equals(FVector2D(0.0, 1.0)));

	Component->Traverse(FTraversalHint());
	Component->InteractBegin(FCourtFixtureId(4));
	Component->InteractEnd();
	Component->PrepareBegin();
	Component->PrepareEnd();
	Component->Attack(FAttackPayload());
	Component->Hide(FHideCommand::Enter(2));
	Component->Hide(FHideCommand::Exit(1));
	const TArray<ECommandKind> WantedKinds = { ECommandKind::Traverse, ECommandKind::InteractBegin, ECommandKind::InteractEnd, ECommandKind::PrepareBegin,
		ECommandKind::PrepareEnd, ECommandKind::Attack, ECommandKind::HideEnter, ECommandKind::HideExit };
	TestTrue(TEXT("the discrete entry points submit the eight command kinds in call order"), Queue.Kinds == WantedKinds);
	TestTrue(TEXT("without an open origin every command goes to the server-local mailbox"),
		!Queue.Mailboxes.ContainsByPredicate([](const FMailboxId& Id) { return !Id.IsServerLocal(); }));

	// An origin opened by ingress attaches its mailbox for the calls inside its scope only.
	FCommandOrigin HumanOrigin;
	HumanOrigin.Mailbox = FMailboxId::ForParticipant(FParticipantId(9));
	{
		FScopedCommandOrigin Scope(*Component, HumanOrigin, FClientCommandSeq(), 0);
		Component->PrepareBegin();
	}
	Component->PrepareEnd();
	TestTrue(TEXT("the scoped origin's mailbox carried the scoped call"), Queue.Mailboxes.Num() == 10 && Queue.Mailboxes[8] == HumanOrigin.Mailbox);
	TestTrue(TEXT("after the scope the server-local mailbox is used again"), Queue.Mailboxes.Num() == 10 && Queue.Mailboxes[9].IsServerLocal());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
