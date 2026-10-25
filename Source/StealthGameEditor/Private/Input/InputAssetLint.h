#pragma once

// Editor data lint for the project's gameplay input assets (story iif-007; ADR-0011 section 9 and Validation
// Criteria; ADR-0021 section 7). It checks the assets under StealthInputSpec::InputFolder against the table in
// InputAssetSpec.h and fails the per-commit gate through StealthGame.Input.InputAssetsAndTriggerLint. It only
// loads assets: it never marks a package dirty and never saves. The Game Animation Sample's own input assets
// (Content/Input) are never scanned.

#include "CoreMinimal.h"

class UInputAction;
class UInputMappingContext;

namespace StealthInputLint
{
	/**
	 * Lints a set of input actions and mapping contexts (the pure part; tests pass transient copies).
	 * Findings, one human-readable line each (empty when clean):
	 *   - a spec action missing, or an IA_* action in the set that the spec does not list
	 *   - an action whose value type differs from the spec (Move/Look Axis2D, WalkSlower Axis1D, the rest Boolean)
	 *   - a banned trigger (Hold, HoldAndRelease, Tap, Pulse, Chorded action) on an action or on any mapping of it,
	 *     in the default mappings or any profile override
	 *   - any trigger other than the single default (Down or Pressed) on an action or mapping
	 *   - a gameplay context missing, or its action set or its keys differing from the spec; IMC_Informant
	 *     holding any mapping; any other context in the set mapping a gameplay IA_* action (ADR-0021 IMC_Menu rule)
	 * @param Actions   the input actions to check (by object name)
	 * @param Contexts  the mapping contexts to check (by object name)
	 */
	TArray<FString> LintInputData(TConstArrayView<const UInputAction*> Actions, TConstArrayView<const UInputMappingContext*> Contexts);

	/**
	 * Lints the project: loads every input action and mapping context under StealthInputSpec::InputFolder through
	 * the asset registry, runs LintInputData on them, and adds one finding per UPlayerMappableInputConfig asset
	 * anywhere in the project (ADR-0011: rebinding uses UEnhancedInputUserSettings).
	 */
	TArray<FString> LintProjectInputAssets();
}
