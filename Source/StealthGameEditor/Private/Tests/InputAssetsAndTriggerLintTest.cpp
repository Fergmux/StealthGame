// Story iif-007 (production/epics/input-interaction-framework/story-007-input-assets-and-trigger-lint.md):
// the editor data lint over /Game/StealthGame/Input (ADR-0011 section 9 and Validation Criteria; ADR-0021 section 7).
//   1. The real assets lint clean: Move/Look Axis2D, WalkSlower Axis1D, every other action Boolean with no trigger
//      beyond the default, IMC_Common/IMC_Active hold exactly the GDD R13 actions on the R15 keys, IMC_Informant maps
//      nothing, and no UPlayerMappableInputConfig asset exists. The lint leaves every package it loaded clean.
//   2. Seeded negatives on transient copies (the real assets are never edited): a Hold trigger on IA_Interact, each
//      other banned trigger class, a mapping-level Tap, a wrong value type, a gameplay action in IMC_Informant, a menu
//      context mapping a gameplay action, and a missing action are each reported.
//   3. No project source names UPlayerMappableInputConfig outside the lint and this test.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "EnhancedActionKeyMapping.h"
#include "HAL/FileManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "Input/InputAssetLint.h"
#include "Input/InputAssetSpec.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

namespace InputAssetsLintTestData
{
	/** Loads a project input asset by name from the input folder. */
	template <typename T>
	T* LoadInputAsset(const TCHAR* Name)
	{
		return LoadObject<T>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), StealthInputSpec::InputFolder, Name, Name));
	}

	/** A set of transient copies of the real input assets that a case may break without touching the originals. */
	struct FCopies
	{
		TArray<UInputAction*> Actions;
		TArray<UInputMappingContext*> Contexts;

		UInputAction* Action(const TCHAR* Name) const
		{
			UInputAction* const* Found = Actions.FindByPredicate([Name](const UInputAction* A) { return A->GetName() == Name; });
			return Found ? *Found : nullptr;
		}

		UInputMappingContext* Context(const TCHAR* Name) const
		{
			UInputMappingContext* const* Found = Contexts.FindByPredicate([Name](const UInputMappingContext* C) { return C->GetName() == Name; });
			return Found ? *Found : nullptr;
		}

		TArray<FString> Lint() const
		{
			return StealthInputLint::LintInputData(TArray<const UInputAction*>(Actions), TArray<const UInputMappingContext*>(Contexts));
		}
	};

	/** Copies every spec action and gameplay context into a fresh transient package. */
	FCopies MakeCopies()
	{
		UPackage* Package = NewObject<UPackage>(nullptr, MakeUniqueObjectName(nullptr, UPackage::StaticClass(), TEXT("StealthInputLintTest")), RF_Transient);
		FCopies Copies;
		for (const StealthInputSpec::FActionSpec& Spec : StealthInputSpec::Actions())
		{
			if (const UInputAction* Real = LoadInputAsset<UInputAction>(Spec.Name))
			{
				Copies.Actions.Add(DuplicateObject<UInputAction>(Real, Package, Real->GetFName()));
			}
		}
		for (const TCHAR* Name : StealthInputSpec::Contexts())
		{
			if (const UInputMappingContext* Real = LoadInputAsset<UInputMappingContext>(Name))
			{
				Copies.Contexts.Add(DuplicateObject<UInputMappingContext>(Real, Package, Real->GetFName()));
			}
		}
		return Copies;
	}

	/** True when any finding contains every given fragment. */
	bool HasFinding(const TArray<FString>& Findings, std::initializer_list<const TCHAR*> Fragments)
	{
		return Findings.ContainsByPredicate([&Fragments](const FString& Finding)
		{
			for (const TCHAR* Fragment : Fragments)
			{
				if (!Finding.Contains(Fragment))
				{
					return false;
				}
			}
			return true;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputInputAssetsAndTriggerLintTest, "StealthGame.Input.InputAssetsAndTriggerLint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInputInputAssetsAndTriggerLintTest::RunTest(const FString& Parameters)
{
	using namespace InputAssetsLintTestData;

	// ---- 1. the real assets lint clean, and the lint dirties nothing ---------------------------------------------
	const TArray<FString> RealFindings = StealthInputLint::LintProjectInputAssets();
	for (const FString& Finding : RealFindings)
	{
		AddError(FString::Printf(TEXT("Real input assets: %s"), *Finding));
	}
	TestEqual(TEXT("Real input assets: zero lint findings"), RealFindings.Num(), 0);
	for (const StealthInputSpec::FActionSpec& Spec : StealthInputSpec::Actions())
	{
		const UInputAction* Action = LoadInputAsset<UInputAction>(Spec.Name);
		if (TestNotNull(FString::Printf(TEXT("%s exists"), Spec.Name), Action))
		{
			TestFalse(FString::Printf(TEXT("%s package left clean by the lint"), Spec.Name), Action->GetPackage()->IsDirty());
		}
	}
	for (const TCHAR* Name : StealthInputSpec::Contexts())
	{
		const UInputMappingContext* Context = LoadInputAsset<UInputMappingContext>(Name);
		if (TestNotNull(FString::Printf(TEXT("%s exists"), Name), Context))
		{
			TestFalse(FString::Printf(TEXT("%s package left clean by the lint"), Name), Context->GetPackage()->IsDirty());
		}
	}

	// Unmodified copies lint clean too (the copy mechanism itself adds no finding).
	TestEqual(TEXT("Unmodified transient copies: zero findings"), MakeCopies().Lint().Num(), 0);

	// ---- 2. seeded negatives on transient copies ------------------------------------------------------------------
	{
		// The story's seeded negative: a Hold trigger on IA_Interact fails.
		const FCopies Copies = MakeCopies();
		UInputAction* Interact = Copies.Action(TEXT("IA_Interact"));
		if (TestNotNull(TEXT("IA_Interact copy"), Interact))
		{
			Interact->Triggers.Add(NewObject<UInputTriggerHold>(Interact));
			TestTrue(TEXT("Hold on IA_Interact is reported"), HasFinding(Copies.Lint(), { TEXT("IA_Interact"), TEXT("banned trigger Hold") }));
		}
	}
	{
		// Every other banned trigger class, on an action.
		const TPair<UClass*, const TCHAR*> Banned[] = {
			{ UInputTriggerHoldAndRelease::StaticClass(), TEXT("banned trigger HoldAndRelease") },
			{ UInputTriggerTap::StaticClass(), TEXT("banned trigger Tap") },
			{ UInputTriggerPulse::StaticClass(), TEXT("banned trigger Pulse") },
			{ UInputTriggerChordAction::StaticClass(), TEXT("banned trigger ChordedAction") },
		};
		for (const TPair<UClass*, const TCHAR*>& Case : Banned)
		{
			const FCopies Copies = MakeCopies();
			UInputAction* Prepare = Copies.Action(TEXT("IA_Prepare"));
			if (TestNotNull(TEXT("IA_Prepare copy"), Prepare))
			{
				Prepare->Triggers.Add(NewObject<UInputTrigger>(Prepare, Case.Key));
				TestTrue(FString::Printf(TEXT("%s on IA_Prepare is reported"), Case.Value), HasFinding(Copies.Lint(), { TEXT("IA_Prepare"), Case.Value }));
			}
		}
	}
	{
		// A trigger set at the mapping level (not on the action) fails too.
		const FCopies Copies = MakeCopies();
		UInputMappingContext* Active = Copies.Context(StealthInputSpec::ActiveContext);
		if (TestNotNull(TEXT("IMC_Active copy"), Active))
		{
			const UInputAction* Interact = LoadInputAsset<UInputAction>(TEXT("IA_Interact"));
			for (int32 Index = 0; Index < Active->GetMappings().Num(); ++Index)
			{
				FEnhancedActionKeyMapping& Mapping = Active->GetMapping(Index);
				if (Mapping.Action == Interact && Mapping.Key == EKeys::E)
				{
					Mapping.Triggers.Add(NewObject<UInputTriggerTap>(Active));
				}
			}
			TestTrue(TEXT("Mapping-level Tap on E -> IA_Interact is reported"), HasFinding(Copies.Lint(), { TEXT("IMC_Active mapping E -> IA_Interact"), TEXT("banned trigger Tap") }));
		}
	}
	{
		// A server-meaning action that is not Boolean fails.
		const FCopies Copies = MakeCopies();
		UInputAction* Attack = Copies.Action(TEXT("IA_Attack"));
		if (TestNotNull(TEXT("IA_Attack copy"), Attack))
		{
			Attack->ValueType = EInputActionValueType::Axis1D;
			TestTrue(TEXT("IA_Attack as Axis1D is reported"), HasFinding(Copies.Lint(), { TEXT("IA_Attack: value type Axis1D, expected Boolean") }));
		}
	}
	{
		// IMC_Informant holds no gameplay action.
		const FCopies Copies = MakeCopies();
		UInputMappingContext* Informant = Copies.Context(StealthInputSpec::InformantContext);
		if (TestNotNull(TEXT("IMC_Informant copy"), Informant))
		{
			Informant->MapKey(LoadInputAsset<UInputAction>(TEXT("IA_Attack")), EKeys::LeftMouseButton);
			TestTrue(TEXT("IA_Attack in IMC_Informant is reported"), HasFinding(Copies.Lint(), { TEXT("IMC_Informant mapping LeftMouseButton -> IA_Attack") }));
		}
	}
	{
		// A menu context (ADR-0021 IMC_Menu) mapping a gameplay action fails.
		FCopies Copies = MakeCopies();
		UInputMappingContext* Menu = NewObject<UInputMappingContext>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UInputMappingContext::StaticClass(), TEXT("IMC_Menu")));
		Menu->MapKey(LoadInputAsset<UInputAction>(TEXT("IA_Interact")), EKeys::Enter);
		Copies.Contexts.Add(Menu);
		TestTrue(TEXT("A menu context mapping IA_Interact is reported"), HasFinding(Copies.Lint(), { TEXT("-> IA_Interact"), TEXT("only IMC_Common and IMC_Active") }));
	}
	{
		// A gameplay action moved out of its context, and a missing action, are reported.
		FCopies Copies = MakeCopies();
		UInputMappingContext* Common = Copies.Context(StealthInputSpec::CommonContext);
		if (TestNotNull(TEXT("IMC_Common copy"), Common))
		{
			Common->UnmapAllKeysFromAction(LoadInputAsset<UInputAction>(TEXT("IA_Ping")));
			TestTrue(TEXT("IA_Ping missing from IMC_Common is reported"), HasFinding(Copies.Lint(), { TEXT("IMC_Common: maps"), TEXT("expected exactly") }));
		}
		Copies.Actions.RemoveAll([](const UInputAction* A) { return A->GetName() == TEXT("IA_Move"); });
		TestTrue(TEXT("A missing IA_Move is reported"), HasFinding(Copies.Lint(), { TEXT("IA_Move: missing") }));
	}

	// ---- 3. no project source names UPlayerMappableInputConfig ----------------------------------------------------
	TArray<FString> SourceFiles;
	IFileManager::Get().FindFilesRecursive(SourceFiles, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Source")), TEXT("*.*"), true, false);
	int32 Scanned = 0;
	for (const FString& File : SourceFiles)
	{
		const FString Ext = FPaths::GetExtension(File);
		const FString Base = FPaths::GetBaseFilename(File);
		if ((Ext != TEXT("h") && Ext != TEXT("cpp") && Ext != TEXT("cs")) || Base == TEXT("InputAssetLint") || Base == TEXT("InputAssetsAndTriggerLintTest"))
		{
			continue;
		}
		FString Text;
		if (FFileHelper::LoadFileToString(Text, *File))
		{
			++Scanned;
			TestFalse(FString::Printf(TEXT("%s does not name UPlayerMappableInputConfig"), *File), Text.Contains(TEXT("PlayerMappableInputConfig")));
		}
	}
	TestTrue(TEXT("Source files were scanned for UPlayerMappableInputConfig"), Scanned > 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
