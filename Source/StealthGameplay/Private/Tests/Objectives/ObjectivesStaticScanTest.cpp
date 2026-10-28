// Objectives Chain story oc-022 AC1 (ADR-0020 Guidelines [U]): StealthGame.Objectives.ObjectivesStaticScan.
// A source scan of the production modules (StealthFoundation, StealthGameplay, StealthPresentation, StealthGame; tests excluded)
// for the forbidden objective-actor shapes: a class named AObjective*, ACourt* or ASite*, any AActor subclass declared in an
// Objectives folder, and a replicated bCompleted / InteractorCount. Objectives never spawns an actor for a court, objective or
// site; the prototype AStealthObjective (replicated bCompleted / InteractorCount) was replaced, not ported.
//   - The real tree reports 0 violations with NO allowlist (the prototype is removed, story oc-022, 2026-10-28).
//   - The before-removal mode: the retired prototype's own declaration (kept here as text) passes only with the allowlist of its
//     named symbols (AStealthObjective, bCompleted, InteractorCount) and nothing else, and fails without it.
//   - Negative controls: a seeded AObjectiveCourt actor and a seeded replicated bCompleted each fail the scan.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ObjectivesSourceScan.h"

namespace ObjectivesStaticScanTestData
{
	using StealthObjectivesSourceScan::FSource;

	/** The class name declared on a "class [API] Name : ..." line, or empty. */
	FString DeclaredClass(const FString& Line, FString& OutBase)
	{
		FString Trimmed = Line.TrimStartAndEnd();
		if (!Trimmed.StartsWith(TEXT("class ")) || !Trimmed.Contains(TEXT(":")) || Trimmed.EndsWith(TEXT(";")))
		{
			return FString();
		}
		TArray<FString> Words;
		Trimmed.Replace(TEXT(":"), TEXT(" : ")).ParseIntoArrayWS(Words);
		int32 Colon = Words.IndexOfByKey(TEXT(":"));
		if (Colon < 2)
		{
			return FString();
		}
		OutBase = Words.IsValidIndex(Colon + 2) ? Words[Colon + 2].Replace(TEXT(","), TEXT("")) : FString();
		return Words[Colon - 1];
	}

	/** Every violation of Sources ("path: what"), minus the allowlisted symbol names. */
	TArray<FString> Scan(TConstArrayView<FSource> Sources, const TSet<FString>& Allow)
	{
		TArray<FString> Out;
		for (const FSource& Source : Sources)
		{
			const FString Code = StealthObjectivesSourceScan::StripCommentsAndLiterals(Source.Text);
			TArray<FString> Lines;
			Code.ParseIntoArrayLines(Lines, false);
			const bool bObjectivesFolder = Source.Path.Contains(TEXT("/Objectives/"));
			for (const FString& Line : Lines)
			{
				FString Base;
				const FString Class = DeclaredClass(Line, Base);
				if (!Class.IsEmpty() && !Allow.Contains(Class))
				{
					if (Class.StartsWith(TEXT("AObjective")) || Class.StartsWith(TEXT("ACourt")) || Class.StartsWith(TEXT("ASite")))
					{
						Out.Add(FString::Printf(TEXT("%s: objective-shaped actor class %s"), *Source.Path, *Class));
					}
					else if (bObjectivesFolder && Class.StartsWith(TEXT("A")) && (Base == TEXT("AActor") || Base.StartsWith(TEXT("A"))))
					{
						Out.Add(FString::Printf(TEXT("%s: actor class %s in an Objectives folder"), *Source.Path, *Class));
					}
				}
				for (const TCHAR* Field : { TEXT("bCompleted"), TEXT("InteractorCount") })
				{
					if (Allow.Contains(Field) || !StealthObjectivesSourceScan::ContainsIdentifier(Line, Field))
					{
						continue;
					}
					if (Line.Contains(TEXT("DOREPLIFETIME")) || Line.Contains(TEXT("UPROPERTY(Replicated")))
					{
						Out.Add(FString::Printf(TEXT("%s: replicated %s"), *Source.Path, Field));
					}
				}
			}
			// A replicated flag declared on the line after its UPROPERTY(Replicated...).
			for (int32 Index = 1; Index < Lines.Num(); ++Index)
			{
				if (!Lines[Index - 1].Contains(TEXT("UPROPERTY(Replicated")))
				{
					continue;
				}
				for (const TCHAR* Field : { TEXT("bCompleted"), TEXT("InteractorCount") })
				{
					if (!Allow.Contains(Field) && StealthObjectivesSourceScan::ContainsIdentifier(Lines[Index], Field))
					{
						Out.Add(FString::Printf(TEXT("%s: replicated %s"), *Source.Path, Field));
					}
				}
			}
		}
		return Out;
	}

	TArray<FSource> ProductionSources()
	{
		TArray<FSource> Out;
		for (FSource& Source : StealthObjectivesSourceScan::LoadProjectSources())
		{
			if (!Source.Path.Contains(TEXT("/Tests/")) && !Source.Path.Contains(TEXT("/Testing/")))
			{
				Out.Add(MoveTemp(Source));
			}
		}
		return Out;
	}

	/** The retired prototype objective's declaration (a text copy of the deleted StealthObjective.h's shape). */
	FSource RetiredPrototype()
	{
		FSource Source;
		Source.Path = TEXT("StealthGame/Public/StealthObjective.h");
		Source.Text = TEXT("UCLASS()\nclass STEALTHGAME_API AStealthObjective : public AActor\n{\n")
			TEXT("\tUPROPERTY(ReplicatedUsing = OnRep_Visuals)\n\tbool bCompleted = false;\n")
			TEXT("\tUPROPERTY(ReplicatedUsing = OnRep_Visuals)\n\tint32 InteractorCount = 0;\n};\n")
			TEXT("void AStealthObjective::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const\n{\n")
			TEXT("\tDOREPLIFETIME(AStealthObjective, bCompleted);\n\tDOREPLIFETIME(AStealthObjective, InteractorCount);\n}\n");
		return Source;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesStaticScanTest, "StealthGame.Objectives.ObjectivesStaticScan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesStaticScanTest::RunTest(const FString& Parameters)
{
	using namespace ObjectivesStaticScanTestData;
	const TArray<FSource> Tree = ProductionSources();
	TestTrue(TEXT("the scan reads the production tree"), Tree.Num() > 200);
	const TArray<FString> Findings = Scan(Tree, TSet<FString>());
	AddInfo(FString::Printf(TEXT("OC22 static scan: %d production sources, %d violation(s) %s"), Tree.Num(), Findings.Num(), *FString::Join(Findings, TEXT("; "))));
	TestEqual(TEXT("the real tree: 0 violations with no allowlist (prototype removed)"), Findings.Num(), 0);
	TestFalse(TEXT("the prototype objective actor is gone from the tree"), Tree.ContainsByPredicate([](const FSource& S) { return S.Path.EndsWith(TEXT("StealthObjective.h")); }));

	// The before-removal mode: the prototype passes only with exactly its named symbols allowlisted.
	const FSource Prototype = RetiredPrototype();
	const TSet<FString> PrototypeAllow = { TEXT("AStealthObjective"), TEXT("bCompleted"), TEXT("InteractorCount") };
	TestEqual(TEXT("before removal: the prototype passes with its allowlist"), Scan({ Prototype }, PrototypeAllow).Num(), 0);
	TestTrue(TEXT("before removal: without the allowlist it fails"), Scan({ Prototype }, TSet<FString>()).Num() >= 2);

	// Negative controls.
	FSource SeededActor;
	SeededActor.Path = TEXT("StealthGameplay/Features/Objectives/Public/View/SeededCourt.h");
	SeededActor.Text = TEXT("UCLASS()\nclass STEALTHGAMEPLAY_API AObjectiveCourt : public AActor\n{\n\tGENERATED_BODY()\n};\n");
	TestEqual(TEXT("control: a seeded AObjectiveCourt actor fails"), Scan({ SeededActor }, PrototypeAllow).Num(), 1);
	FSource SeededFlag;
	SeededFlag.Path = TEXT("StealthGameplay/Features/Objectives/Private/SeededFlag.cpp");
	SeededFlag.Text = TEXT("void UThing::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const\n{\n\tDOREPLIFETIME(UThing, bCompleted);\n}\n");
	TestEqual(TEXT("control: a seeded replicated bCompleted fails"), Scan({ SeededFlag }, TSet<FString>()).Num(), 1);
	FSource FolderActor;
	FolderActor.Path = TEXT("StealthGameplay/Features/Objectives/Private/SeededMarker.h");
	FolderActor.Text = TEXT("class AStealthMarker : public AActor\n{\n};\n");
	TestEqual(TEXT("control: any actor class in an Objectives folder fails"), Scan({ FolderActor }, TSet<FString>()).Num(), 1);
	return true;
}

#endif
