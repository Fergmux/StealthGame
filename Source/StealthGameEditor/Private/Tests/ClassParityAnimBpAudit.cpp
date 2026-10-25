// Stories SCL-004 and SCL-005 (production/epics/shared-character-locomotion/story-004-class-parity-scan.md,
// story-005-animbp-islocallycontrolled-removal.md): the Blueprint half of the class parity scan
// (AC-SCL-01; ADR-0001 section 1, ADR-0010 "Kind-blind code", ADR-0013 section 1).
//
// The walker loads each fixture Blueprint root read-only, visits every graph (function, event,
// macro, anim and state-machine sub-graphs), and exports every node's reflected properties, its
// owned subobjects (anim property-access bindings) and its pins as text. Every identifier in that
// text is checked against the committed fixture's forbidden list, so no node type needs a special
// case and none can be skipped silently: a root that does not load, has no graphs, or holds a null
// graph or node is NOT ASSESSED and fails. Comment nodes and node comments are not reads and are
// excluded. Linked anim layers/graphs and Blueprint parents are walked as further roots.
//
// Tests:
//   StealthGame.Locomotion.ClassParity.Blueprint        live roots: zero hits outside the R9 allowlist
//   StealthGame.Locomotion.ClassParity.NegativeControl  the recorded walk of the unmodified sample
//                                                      (tests/fixtures/scl/sample-animbp-negative-control.txt)
//                                                      fails, and a seeded transient Blueprint is caught
//   StealthGame.Locomotion.AnimBpNoLocalControlRead     SCL-005: the AnimBP root alone has no local-control
//                                                      read; the recorded sample AnimBP still fails

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimBlueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Server/Testing/StealthClassParityScan.h"
#include "Server/Testing/StealthNegativeControl.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

namespace ClassParityBlueprintTestData
{
	/** Repo-relative path of the committed negative-control report (the walk of the unmodified sample). */
	const TCHAR* const NegativeControlReport = TEXT("tests/fixtures/scl/sample-animbp-negative-control.txt");
	/** Where each live run writes its walk, so a run on the unmodified sample can be committed as the report. */
	const TCHAR* const LiveReportName = TEXT("ClassParity/live-blueprint-refs.txt");
	/** The two sample references the negative control must find (story SCL-004 QA case). */
	const TCHAR* const SampleAnimBp = TEXT("/Game/Blueprints/ABP_SandboxCharacter.ABP_SandboxCharacter");
	const TCHAR* const SampleCharacterBp = TEXT("/Game/Blueprints/CBP_SandboxCharacter.CBP_SandboxCharacter");

	FString ProjectPath(const TCHAR* Relative)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), Relative));
	}

	/** Collects identifier text from one object's reflected properties and the subobjects it owns. */
	class FNodeTextCollector
	{
	public:
		explicit FNodeTextCollector(TArray<UBlueprint*>& InMoreRoots) : MoreRoots(InMoreRoots) {}

		void CollectObject(UObject* Object, UObject* OwnerNode, FString& OutText)
		{
			if (!Object || Visited.Contains(Object))
			{
				return;
			}
			Visited.Add(Object);
			const bool bLinkedAnimNode = OwnerNode->GetClass()->GetName().StartsWith(TEXT("AnimGraphNode_Linked"));
			for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
			{
				const FProperty* Property = *It;
				if (Property->HasAnyPropertyFlags(CPF_Transient) || Property->GetFName() == TEXT("NodeComment"))
				{
					continue;
				}
				FString Value;
				Property->ExportText_InContainer(0, Value, Object, nullptr, Object, PPF_None);
				OutText += Value + TEXT("\n");

				if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
				{
					UObject* Child = ObjectProperty->GetObjectPropertyValue_InContainer(Object);
					if (Child && Child->IsIn(OwnerNode))
					{
						CollectObject(Child, OwnerNode, OutText); // owned subobject, e.g. an anim binding
					}
					if (bLinkedAnimNode)
					{
						if (const UBlueprintGeneratedClass* LinkedClass = Cast<UBlueprintGeneratedClass>(Child))
						{
							if (UBlueprint* LinkedBlueprint = Cast<UBlueprint>(LinkedClass->ClassGeneratedBy))
							{
								MoreRoots.AddUnique(LinkedBlueprint);
							}
						}
					}
				}
			}
		}

		static void CollectPins(const UEdGraphNode& Node, FString& OutText)
		{
			for (const UEdGraphPin* Pin : Node.Pins)
			{
				if (!Pin)
				{
					continue;
				}
				OutText += Pin->PinName.ToString() + TEXT(" ") + Pin->DefaultValue + TEXT(" ") + Pin->DefaultTextValue.ToString() + TEXT(" ");
				OutText += Pin->PinType.PinCategory.ToString() + TEXT(" ") + Pin->PinType.PinSubCategory.ToString() + TEXT(" ");
				OutText += Pin->PinType.PinSubCategoryMemberReference.MemberName.ToString() + TEXT(" ");
				if (Pin->DefaultObject) { OutText += Pin->DefaultObject->GetPathName() + TEXT(" "); }
				if (const UObject* SubCategory = Pin->PinType.PinSubCategoryObject.Get()) { OutText += SubCategory->GetPathName() + TEXT(" "); }
				OutText += TEXT("\n");
			}
		}

	private:
		TArray<UBlueprint*>& MoreRoots;
		TSet<UObject*> Visited;
	};

	/** The graph directly owned by the Blueprint that contains Graph (its function or event graph name). */
	FString TopLevelGraphName(const UEdGraph* Graph)
	{
		const UEdGraph* Top = Graph;
		for (const UObject* Outer = Graph->GetOuter(); Outer && !Outer->IsA<UBlueprint>(); Outer = Outer->GetOuter())
		{
			if (const UEdGraph* OuterGraph = Cast<UEdGraph>(Outer))
			{
				Top = OuterGraph;
			}
		}
		return Top->GetName();
	}

	/** Walks one Blueprint; appends forbidden references and anything it could not assess. */
	void WalkBlueprint(UBlueprint* Blueprint, const FStealthParityFixture& Fixture, FStealthParityScanResult& Result, TArray<UBlueprint*>& MoreRoots)
	{
		const FString Root = Blueprint->GetPathName();
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		if (Graphs.IsEmpty())
		{
			Result.NotAssessed.Add(FString::Printf(TEXT("%s: no graphs"), *Root));
			return;
		}
		Result.Scanned.Add(Root);

		// A Blueprint parent is part of the class: walk it too (native parents are covered by the C++ roots).
		if (UBlueprint* ParentBlueprint = Blueprint->ParentClass ? Cast<UBlueprint>(Blueprint->ParentClass->ClassGeneratedBy) : nullptr)
		{
			MoreRoots.AddUnique(ParentBlueprint);
		}

		FNodeTextCollector Collector(MoreRoots);
		for (const UEdGraph* Graph : Graphs)
		{
			if (!Graph)
			{
				Result.NotAssessed.Add(FString::Printf(TEXT("%s: null graph"), *Root));
				continue;
			}
			const FString Scope = TopLevelGraphName(Graph);
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!Node)
				{
					Result.NotAssessed.Add(FString::Printf(TEXT("%s: null node in %s"), *Root, *Graph->GetName()));
					continue;
				}
				if (Node->IsA<UEdGraphNode_Comment>())
				{
					continue;
				}
				FString Text;
				Collector.CollectObject(Node, Node, Text);
				FNodeTextCollector::CollectPins(*Node, Text);

				TArray<FString> Tokens;
				StealthClassParity::Tokenize(Text, Tokens);
				TSet<FString> Seen;
				for (const FString& Token : Tokens)
				{
					if (!Seen.Contains(Token) && Fixture.IsForbidden(Token, EStealthParityDomain::Blueprint))
					{
						Seen.Add(Token);
						Result.Refs.Add({ Root, Scope, Graph->GetName() + TEXT("/") + Node->GetName(), Token });
					}
				}
			}
		}
	}

	/** Loads each root read-only and walks it and every further root it names. */
	FStealthParityScanResult WalkRoots(const TArray<FString>& RootPaths, const FStealthParityFixture& Fixture, TArray<UPackage*>& OutTouchedPackages)
	{
		FStealthParityScanResult Result;
		TArray<UBlueprint*> Queue;
		for (const FString& Path : RootPaths)
		{
			UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Path, nullptr, LOAD_NoWarn);
			if (!Blueprint)
			{
				Result.NotAssessed.Add(FString::Printf(TEXT("%s: cannot be loaded as a Blueprint"), *Path));
				continue;
			}
			Queue.AddUnique(Blueprint);
		}
		for (int32 Index = 0; Index < Queue.Num(); ++Index)
		{
			OutTouchedPackages.AddUnique(Queue[Index]->GetOutermost());
			TArray<UBlueprint*> More;
			WalkBlueprint(Queue[Index], Fixture, Result, More);
			for (UBlueprint* Next : More)
			{
				Queue.AddUnique(Next);
			}
		}
		return Result;
	}

	/** The roots (object paths) a report covers, for the judge's unused-allowlist check. */
	TArray<FString> RootsOf(const TArray<FStealthParityRef>& Refs)
	{
		TArray<FString> Roots;
		for (const FStealthParityRef& Ref : Refs)
		{
			Roots.AddUnique(Ref.Root);
		}
		return Roots;
	}

	bool AnyRef(const TArray<FStealthParityRef>& Refs, const FString& Root, const FString& Scope, const FString& Token)
	{
		return Refs.ContainsByPredicate([&](const FStealthParityRef& R) { return R.Root == Root && R.Scope == Scope && R.Token == Token; });
	}

	/** Loads the committed negative-control report; false with a NOT ASSESSED message when unusable. */
	bool LoadNegativeControl(TArray<FStealthParityRef>& OutRefs, FString& OutError)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *ProjectPath(NegativeControlReport)))
		{
			OutError = FString::Printf(TEXT("NOT ASSESSED: negative-control report unreadable: %s"), NegativeControlReport);
			return false;
		}
		TArray<FString> Errors;
		OutRefs = StealthClassParity::ReadReport(Text, Errors);
		if (!Errors.IsEmpty() || OutRefs.IsEmpty())
		{
			OutError = FString::Printf(TEXT("NOT ASSESSED: negative-control report malformed or empty (%d bad lines)"), Errors.Num());
			return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocomotionClassParityBlueprintTest, "StealthGame.Locomotion.ClassParity.Blueprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLocomotionClassParityBlueprintTest::RunTest(const FString& Parameters)
{
	using namespace ClassParityBlueprintTestData;

	FStealthParityFixture Fixture;
	FString FixtureError;
	if (!FStealthParityFixture::LoadFromProject(Fixture, FixtureError))
	{
		AddError(FixtureError);
		return false;
	}
	TestTrue(TEXT("Fixture names at least one Blueprint root"), !Fixture.BlueprintRoots.IsEmpty());

	// Act: walk the live roots read-only.
	TArray<UPackage*> Packages;
	const FStealthParityScanResult Result = WalkRoots(Fixture.BlueprintRoots, Fixture, Packages);
	for (UPackage* Package : Packages)
	{
		TestFalse(FString::Printf(TEXT("Walk leaves %s unmodified (read-only; never-resave list)"), *Package->GetName()), Package->IsDirty());
	}

	// Keep the walk on disk: a run on the unmodified sample is the committed negative-control report.
	const FString LiveReport = FPaths::Combine(FPaths::ProjectSavedDir(), LiveReportName);
	FFileHelper::SaveStringToFile(StealthClassParity::WriteReport(TEXT("Class parity Blueprint walk (StealthGame.Locomotion.ClassParity.Blueprint)"), Result.Refs), *LiveReport);
	AddInfo(FString::Printf(TEXT("Blueprint walk written to %s"), *LiveReport));

	// Assert: zero hits outside the R9 owner-local allowlist, nothing unassessed, no stale entry.
	TArray<FString> Roots;
	for (const FString& Scanned : Result.Scanned)
	{
		Roots.Add(Scanned);
	}
	const FStealthParityVerdict Verdict = StealthClassParity::Judge(Result, Fixture, Roots);
	AddInfo(Verdict.Describe());
	TestTrue(FString::Printf(TEXT("AC-SCL-01 Blueprint scan: no forbidden read outside the R9 allowlist (%s)"), *Verdict.Describe()), Verdict.Passed());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocomotionClassParityNegativeControlTest, "StealthGame.Locomotion.ClassParity.NegativeControl",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLocomotionClassParityNegativeControlTest::RunTest(const FString& Parameters)
{
	using namespace ClassParityBlueprintTestData;

	FStealthParityFixture Fixture;
	FString Error;
	if (!FStealthParityFixture::LoadFromProject(Fixture, Error))
	{
		AddError(Error);
		return false;
	}

	// (a) The recorded walk of the unmodified sample Blueprints must FAIL the same judge.
	TArray<FStealthParityRef> SampleRefs;
	if (!LoadNegativeControl(SampleRefs, Error))
	{
		AddError(Error);
		return false;
	}
	const FStealthNegativeControl SampleControl(TEXT("UnmodifiedSampleBlueprints"));
	SampleControl.RunAndReport(*this, [&](FStealthCheckContext& Ctx)
	{
		FStealthParityScanResult Recorded;
		Recorded.Refs = SampleRefs;
		Recorded.Scanned = RootsOf(SampleRefs);
		const FStealthParityVerdict Verdict = StealthClassParity::Judge(Recorded, Fixture, Recorded.Scanned);
		Ctx.TestTrue(FString::Printf(TEXT("sample passes the class parity judge (%s)"), *Verdict.Describe()), Verdict.Passed());
	});
	// The failure is for the right reasons: the two sample branches named in spike S14.
	TestTrue(TEXT("Report holds ABP_SandboxCharacter Get_AOValue -> IsLocallyControlled"), AnyRef(SampleRefs, SampleAnimBp, TEXT("Get_AOValue"), TEXT("IsLocallyControlled")));
	TestTrue(TEXT("Report holds CBP_SandboxCharacter CanSprint -> IsLocallyControlled"), AnyRef(SampleRefs, SampleCharacterBp, TEXT("CanSprint"), TEXT("IsLocallyControlled")));

	// (b) The walker itself catches a seeded read: a transient Blueprint calling IsLocallyControlled.
	UBlueprint* Seeded = FKismetEditorUtilities::CreateBlueprint(APawn::StaticClass(), GetTransientPackage(),
		MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("SclSeededLocalControlRead")),
		BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	UEdGraph* EventGraph = Seeded ? FBlueprintEditorUtils::FindEventGraph(Seeded) : nullptr;
	if (!TestNotNull(TEXT("Seeded Blueprint and its event graph exist"), EventGraph))
	{
		return false;
	}
	UK2Node_CallFunction* Call = NewObject<UK2Node_CallFunction>(EventGraph);
	Call->SetFromFunction(APawn::StaticClass()->FindFunctionByName(TEXT("IsLocallyControlled")));
	EventGraph->AddNode(Call, false, false);
	Call->AllocateDefaultPins();

	const FStealthNegativeControl SeededControl(TEXT("SeededIsLocallyControlledCall"));
	SeededControl.RunAndReport(*this, [&](FStealthCheckContext& Ctx)
	{
		FStealthParityScanResult Walk;
		TArray<UBlueprint*> More;
		WalkBlueprint(Seeded, Fixture, Walk, More);
		const FStealthParityVerdict Verdict = StealthClassParity::Judge(Walk, Fixture, Walk.Scanned);
		Ctx.TestTrue(FString::Printf(TEXT("seeded Blueprint passes (%s)"), *Verdict.Describe()), Verdict.Passed());
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocomotionAnimBpNoLocalControlReadTest, "StealthGame.Locomotion.AnimBpNoLocalControlRead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLocomotionAnimBpNoLocalControlReadTest::RunTest(const FString& Parameters)
{
	using namespace ClassParityBlueprintTestData;

	FStealthParityFixture Fixture;
	FString Error;
	if (!FStealthParityFixture::LoadFromProject(Fixture, Error))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("Fixture lists the local-control set (spike S1/S2 flags)"), Fixture.LocalControlTokens.Contains(TEXT("IsLocallyControlled")));

	// Act: the AnimBP root alone (plus any linked layers and Blueprint parent it names).
	TArray<UPackage*> Packages;
	const FStealthParityScanResult Result = WalkRoots({ SampleAnimBp }, Fixture, Packages);
	TestTrue(TEXT("The AnimBP was walked"), Result.Scanned.Contains(SampleAnimBp));
	TestTrue(FString::Printf(TEXT("Nothing NOT ASSESSED (%s)"), *FString::Join(Result.NotAssessed, TEXT("; "))), Result.NotAssessed.IsEmpty());
	for (const FStealthParityRef& Ref : Result.Refs)
	{
		AddError(FString::Printf(TEXT("AnimBP graph reads a forbidden symbol: %s"), *Ref.ToLine()));
	}
	for (UPackage* Package : Packages)
	{
		TestFalse(FString::Printf(TEXT("Walk leaves %s unmodified"), *Package->GetName()), Package->IsDirty());
	}

	// Negative control: the recorded walk of the unmodified sample AnimBP holds a local-control read.
	TArray<FStealthParityRef> SampleRefs;
	if (!LoadNegativeControl(SampleRefs, Error))
	{
		AddError(Error);
		return false;
	}
	const FStealthNegativeControl Control(TEXT("UnmodifiedSampleAnimBp"));
	Control.RunAndReport(*this, [&](FStealthCheckContext& Ctx)
	{
		for (const FStealthParityRef& Ref : SampleRefs)
		{
			if (Ref.Root == SampleAnimBp && Fixture.LocalControlTokens.Contains(Ref.Token))
			{
				Ctx.AddFailure(FString::Printf(TEXT("sample AnimBP reads %s in %s"), *Ref.Token, *Ref.Scope));
			}
		}
	});
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
