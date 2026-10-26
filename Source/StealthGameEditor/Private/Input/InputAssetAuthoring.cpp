// Authoring of the project's gameplay input assets (story iif-007). The editor console commands
//   Stealth.Input.CreateAssets   create or refresh every IA_*/IMC_* of InputAssetSpec.h under /Game/StealthGame/Input,
//                                save exactly those packages by explicit path, then lint
//   Stealth.Input.Lint           lint only (loads, never saves)
// run headless with: UnrealEditor-Cmd.exe StealthGame.uproject -ExecCmds="Stealth.Input.CreateAssets,QUIT_EDITOR"
//   -unattended -nullrhi -stdout   (the summary line is "STEALTH INPUT LINT: PASS|FAIL (N finding(s))").
// A console command, not a commandlet: StealthGameEditor loads at PostEngineInit, after commandlet lookup.
// Nothing outside /Game/StealthGame/Input is created, edited or saved.

#include "HAL/IConsoleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EnhancedActionKeyMapping.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Input/InputAssetLint.h"
#include "Input/InputAssetSpec.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogStealthInputAssets, Log, All);

namespace StealthInputAuthoring
{
	/** Loads the asset at <InputFolder>/<Name>, or creates it in a new package. Never touches another folder. */
	template <typename T>
	T* LoadOrCreate(const TCHAR* Name, TArray<UPackage*>& OutPackages)
	{
		const FString PackageName = FString::Printf(TEXT("%s/%s"), StealthInputSpec::InputFolder, Name);
		UPackage* Package = FPackageName::DoesPackageExist(PackageName) ? LoadPackage(nullptr, *PackageName, LOAD_None) : CreatePackage(*PackageName);
		check(Package);
		T* Asset = FindObject<T>(Package, Name);
		if (!Asset)
		{
			Asset = NewObject<T>(Package, Name, RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Asset);
		}
		Package->MarkPackageDirty();
		OutPackages.Add(Package);
		return Asset;
	}

	/** Adds the modifiers that shape a 1D key for its action (Move WASD swizzle/negate, walk-slower scalar). */
	void AddShapeModifiers(FEnhancedActionKeyMapping& Mapping, const StealthInputSpec::FKeySpec& Key, UObject* Outer)
	{
		using StealthInputSpec::EKeyShape;
		if (Key.Shape == EKeyShape::Swizzle || Key.Shape == EKeyShape::SwizzleNegate)
		{
			UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Outer);
			Swizzle->Order = EInputAxisSwizzle::YXZ;
			Mapping.Modifiers.Add(Swizzle);
		}
		if (Key.Shape == EKeyShape::Negate || Key.Shape == EKeyShape::SwizzleNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Outer));
		}
		if (Key.Shape == EKeyShape::Scalar)
		{
			UInputModifierScalar* Scalar = NewObject<UInputModifierScalar>(Outer);
			Scalar->Scalar = FVector(Key.Scalar, 1.0, 1.0);
			Mapping.Modifiers.Add(Scalar);
		}
	}

	/** Creates or refreshes every action and context of the spec; returns the packages it wrote into. */
	TArray<UPackage*> AuthorAll()
	{
		using namespace StealthInputSpec;
		TArray<UPackage*> Packages;
		TMap<FString, UInputAction*> ActionsByName;
		for (const FActionSpec& Spec : Actions())
		{
			UInputAction* Action = LoadOrCreate<UInputAction>(Spec.Name, Packages);
			Action->ValueType = Spec.ValueType;
			Action->bConsumeInput = Spec.bConsumeInput;
			Action->Triggers.Empty();
			Action->Modifiers.Empty();
			ActionsByName.Add(Spec.Name, Action);
		}
		for (const TCHAR* ContextName : Contexts())
		{
			UInputMappingContext* Context = LoadOrCreate<UInputMappingContext>(ContextName, Packages);
			Context->UnmapAll();
			for (const FActionSpec& Spec : Actions())
			{
				if (FCString::Strcmp(Spec.Context, ContextName) != 0)
				{
					continue;
				}
				for (const FKeySpec& Key : Spec.Keys)
				{
					FEnhancedActionKeyMapping& Mapping = Context->MapKey(ActionsByName[Spec.Name], Key.Key);
					AddShapeModifiers(Mapping, Key, Context);
				}
			}
		}
		return Packages;
	}

	/** Saves exactly the given packages to their own files. Returns false on any failure. */
	bool SavePackages(const TArray<UPackage*>& Packages)
	{
		bool bAllSaved = true;
		for (UPackage* Package : Packages)
		{
			const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
			FSavePackageArgs Args;
			Args.TopLevelFlags = RF_Public | RF_Standalone;
			Args.Error = GError;
			const bool bSaved = UPackage::SavePackage(Package, nullptr, *File, Args);
			UE_LOGF(LogStealthInputAssets, Display, "%ls %ls", bSaved ? TEXT("saved") : TEXT("FAILED to save"), *File);
			bAllSaved &= bSaved;
		}
		return bAllSaved;
	}
}

namespace StealthInputAuthoring
{
	/** Runs the project lint and logs one error line per finding and a PASS/FAIL summary line. */
	void RunLint()
	{
		const TArray<FString> Findings = StealthInputLint::LintProjectInputAssets();
		for (const FString& Finding : Findings)
		{
			UE_LOGF(LogStealthInputAssets, Error, "INPUT LINT: %ls", *Finding);
		}
		UE_LOGF(LogStealthInputAssets, Display, "STEALTH INPUT LINT: %ls (%d finding(s))", Findings.IsEmpty() ? TEXT("PASS") : TEXT("FAIL"), Findings.Num());
	}

	/** Creates or refreshes the assets, saves exactly those packages, then lints. */
	void CreateAssets()
	{
		const TArray<UPackage*> Packages = AuthorAll();
		if (!SavePackages(Packages))
		{
			UE_LOGF(LogStealthInputAssets, Error, "STEALTH INPUT ASSETS: save failed");
			return;
		}
		RunLint();
	}

	FAutoConsoleCommand CreateCommand(
		TEXT("Stealth.Input.CreateAssets"),
		TEXT("Story iif-007: creates or refreshes every IA_*/IMC_* asset of InputAssetSpec.h under /Game/StealthGame/Input, saves exactly those packages, then lints them."),
		FConsoleCommandDelegate::CreateStatic(&CreateAssets));

	FAutoConsoleCommand LintCommand(
		TEXT("Stealth.Input.Lint"),
		TEXT("Story iif-007: lints the gameplay input assets under /Game/StealthGame/Input (loads only, never saves)."),
		FConsoleCommandDelegate::CreateStatic(&RunLint));
}
