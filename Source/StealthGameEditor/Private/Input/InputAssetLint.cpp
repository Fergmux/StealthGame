#include "Input/InputAssetLint.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EnhancedActionKeyMapping.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "Input/InputAssetSpec.h"

namespace StealthInputLint
{
	namespace
	{
		/** Display name of a trigger when it is one of the banned classes, else empty. */
		FString BannedTriggerName(const UInputTrigger* Trigger)
		{
			if (Trigger->IsA<UInputTriggerHold>()) { return TEXT("Hold"); }
			if (Trigger->IsA<UInputTriggerHoldAndRelease>()) { return TEXT("HoldAndRelease"); }
			if (Trigger->IsA<UInputTriggerTap>()) { return TEXT("Tap"); }
			if (Trigger->IsA<UInputTriggerPulse>()) { return TEXT("Pulse"); }
			if (Trigger->IsA<UInputTriggerChordAction>()) { return TEXT("ChordedAction"); }
			return FString();
		}

		/** True for the one default trigger an action or mapping may carry (Down or Pressed). */
		bool IsDefaultTrigger(const UInputTrigger* Trigger)
		{
			return Trigger->IsA<UInputTriggerDown>() || Trigger->IsA<UInputTriggerPressed>();
		}

		/** Findings for a trigger list: banned classes, and anything beyond the single default trigger. */
		void LintTriggers(const TArray<TObjectPtr<UInputTrigger>>& Triggers, const FString& Where, TArray<FString>& Out)
		{
			int32 Defaults = 0;
			for (const UInputTrigger* Trigger : Triggers)
			{
				if (!Trigger)
				{
					continue;
				}
				const FString Banned = BannedTriggerName(Trigger);
				if (!Banned.IsEmpty())
				{
					Out.Add(FString::Printf(TEXT("%s: banned trigger %s (ADR-0011 section 9: the server owns every hold timer)"), *Where, *Banned));
				}
				else if (!IsDefaultTrigger(Trigger))
				{
					Out.Add(FString::Printf(TEXT("%s: trigger %s is not the default trigger (only one Down or Pressed is allowed)"), *Where, *Trigger->GetClass()->GetName()));
				}
				else
				{
					++Defaults;
				}
			}
			if (Defaults > 1)
			{
				Out.Add(FString::Printf(TEXT("%s: %d default triggers (at most one is allowed)"), *Where, Defaults));
			}
		}

		FString ValueTypeName(EInputActionValueType Type)
		{
			return StaticEnum<EInputActionValueType>()->GetNameStringByValue(static_cast<int64>(Type));
		}

		/** Every mapping of a context: the default mappings and every profile override. */
		TArray<const FEnhancedActionKeyMapping*> AllMappings(const UInputMappingContext* Context, TArray<FString>& OutWhere)
		{
			TArray<const FEnhancedActionKeyMapping*> Mappings;
			for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
			{
				Mappings.Add(&Mapping);
				OutWhere.Add(Context->GetName());
			}
			for (const FString& Profile : Context->GetProfilesWithOverridenMappings())
			{
				for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappingsForProfile(Profile))
				{
					Mappings.Add(&Mapping);
					OutWhere.Add(FString::Printf(TEXT("%s (profile %s)"), *Context->GetName(), *Profile));
				}
			}
			return Mappings;
		}

		FString JoinSorted(TArray<FString> Items)
		{
			Items.Sort();
			return FString::Join(Items, TEXT(", "));
		}
	}

	TArray<FString> LintInputData(TConstArrayView<const UInputAction*> Actions, TConstArrayView<const UInputMappingContext*> Contexts)
	{
		using namespace StealthInputSpec;
		TArray<FString> Findings;

		// Actions: every spec action present with its value type and no trigger beyond the default; no unknown IA_*.
		TMap<FString, const UInputAction*> ActionsByName;
		for (const UInputAction* Action : Actions)
		{
			if (Action)
			{
				ActionsByName.Add(Action->GetName(), Action);
			}
		}
		for (const FActionSpec& Spec : StealthInputSpec::Actions())
		{
			const UInputAction* const* Found = ActionsByName.Find(Spec.Name);
			if (!Found)
			{
				Findings.Add(FString::Printf(TEXT("%s: missing from %s"), Spec.Name, InputFolder));
				continue;
			}
			if ((*Found)->ValueType != Spec.ValueType)
			{
				Findings.Add(FString::Printf(TEXT("%s: value type %s, expected %s"), Spec.Name, *ValueTypeName((*Found)->ValueType), *ValueTypeName(Spec.ValueType)));
			}
			LintTriggers((*Found)->Triggers, Spec.Name, Findings);
		}
		for (const TPair<FString, const UInputAction*>& Pair : ActionsByName)
		{
			if (!FindAction(Pair.Key))
			{
				Findings.Add(FString::Printf(TEXT("%s: not in the input table (InputAssetSpec.h); every action in %s needs a row"), *Pair.Key, InputFolder));
			}
		}

		// Contexts: membership and keys per the table; IMC_Informant maps nothing; no other context maps a gameplay action.
		TMap<FString, const UInputMappingContext*> ContextsByName;
		for (const UInputMappingContext* Context : Contexts)
		{
			if (Context)
			{
				ContextsByName.Add(Context->GetName(), Context);
			}
		}
		for (const TCHAR* Name : StealthInputSpec::Contexts())
		{
			if (!ContextsByName.Contains(Name))
			{
				Findings.Add(FString::Printf(TEXT("%s: missing from %s"), Name, InputFolder));
			}
		}
		for (const TPair<FString, const UInputMappingContext*>& Pair : ContextsByName)
		{
			const FString& ContextName = Pair.Key;
			const bool bGameplayContext = ContextName == CommonContext || ContextName == ActiveContext;
			TArray<FString> Where;
			const TArray<const FEnhancedActionKeyMapping*> Mappings = AllMappings(Pair.Value, Where);

			TMap<FString, TArray<FString>> KeysByAction;
			for (int32 Index = 0; Index < Mappings.Num(); ++Index)
			{
				const FEnhancedActionKeyMapping& Mapping = *Mappings[Index];
				const FString ActionName = Mapping.Action ? Mapping.Action->GetName() : FString(TEXT("<none>"));
				const FString Here = FString::Printf(TEXT("%s mapping %s -> %s"), *Where[Index], *Mapping.Key.ToString(), *ActionName);
				LintTriggers(Mapping.Triggers, Here, Findings);
				if (ContextName == InformantContext)
				{
					Findings.Add(FString::Printf(TEXT("%s: IMC_Informant maps no gameplay action (GDD R13)"), *Here));
				}
				else if (!bGameplayContext && FindAction(ActionName))
				{
					Findings.Add(FString::Printf(TEXT("%s: only IMC_Common and IMC_Active may map a gameplay IA_* action (ADR-0021 section 7: IMC_Menu maps UI navigation only)"), *Here));
				}
				KeysByAction.FindOrAdd(ActionName).Add(Mapping.Key.ToString());
			}
			if (!bGameplayContext)
			{
				continue;
			}

			// Exact membership and exact default keys (GDD R13 and R15).
			TArray<FString> Expected;
			for (const FActionSpec& Spec : StealthInputSpec::Actions())
			{
				if (ContextName != Spec.Context)
				{
					continue;
				}
				Expected.Add(Spec.Name);
				TArray<FString> WantKeys;
				for (const FKeySpec& Key : Spec.Keys)
				{
					WantKeys.Add(Key.Key.ToString());
				}
				const TArray<FString>* HaveKeys = KeysByAction.Find(Spec.Name);
				const FString Want = JoinSorted(WantKeys);
				const FString Have = HaveKeys ? JoinSorted(*HaveKeys) : FString();
				if (Want != Have)
				{
					Findings.Add(FString::Printf(TEXT("%s: %s keys [%s], expected [%s] (GDD R15)"), *ContextName, Spec.Name, *Have, *Want));
				}
			}
			TArray<FString> Have;
			KeysByAction.GetKeys(Have);
			if (JoinSorted(Expected) != JoinSorted(Have))
			{
				Findings.Add(FString::Printf(TEXT("%s: maps [%s], expected exactly [%s] (GDD R13)"), *ContextName, *JoinSorted(Have), *JoinSorted(Expected)));
			}
		}
		return Findings;
	}

	TArray<FString> LintProjectInputAssets()
	{
		IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
		Registry.ScanPathsSynchronous({ StealthInputSpec::InputFolder }, /*bForceRescan*/ false);

		FARFilter Filter;
		Filter.PackagePaths.Add(FName(StealthInputSpec::InputFolder));
		Filter.bRecursivePaths = true;
		Filter.ClassPaths.Add(UInputAction::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(UInputMappingContext::StaticClass()->GetClassPathName());
		TArray<FAssetData> Assets;
		Registry.GetAssets(Filter, Assets);

		TArray<const UInputAction*> Actions;
		TArray<const UInputMappingContext*> Contexts;
		for (const FAssetData& Asset : Assets)
		{
			UObject* Loaded = Asset.GetAsset();
			if (const UInputAction* Action = Cast<UInputAction>(Loaded))
			{
				Actions.Add(Action);
			}
			else if (const UInputMappingContext* Context = Cast<UInputMappingContext>(Loaded))
			{
				Contexts.Add(Context);
			}
		}
		TArray<FString> Findings = LintInputData(Actions, Contexts);

		// No UPlayerMappableInputConfig anywhere (deprecated since 5.3; ADR-0011 E4). The class is named by path so
		// this file needs no deprecated header.
		Registry.SearchAllAssets(/*bSynchronousSearch*/ true);
		TArray<FAssetData> Configs;
		Registry.GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/EnhancedInput"), TEXT("PlayerMappableInputConfig")), Configs, /*bSearchSubClasses*/ true);
		for (const FAssetData& Config : Configs)
		{
			Findings.Add(FString::Printf(TEXT("%s: UPlayerMappableInputConfig asset (ADR-0011: rebinding uses UEnhancedInputUserSettings)"), *Config.GetObjectPathString()));
		}
		return Findings;
	}
}
