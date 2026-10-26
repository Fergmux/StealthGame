#pragma once

// The project's gameplay input data contract (story iif-007; ADR-0011 section 9, ADR-0021 section 7; GDD
// input-interaction-framework R4, R13, R15, R15a). One table, read by both the asset authoring commandlet and the
// editor data lint, so the lint asserts exactly what the authoring writes and the GDD lists.
//
// Table (confirmed at implementation, resolving QL-STORY-READY gap 1):
//   action            value type  context        default keys (keyboard/mouse | gamepad)
//   IA_Move           Axis2D      IMC_Common     W A S D | Gamepad_Left2D
//   IA_Look           Axis2D      IMC_Common     Mouse2D | Gamepad_Right2D
//   IA_CameraToggle   Boolean     IMC_Common     V | Gamepad_FaceButton_Top
//   IA_ShoulderSwap   Boolean     IMC_Common     X, MiddleMouseButton | Gamepad_DPad_Right
//   IA_WalkSlower     Axis1D      IMC_Common     LeftControl (x0.2), LeftAlt (x0.1) | none
//   IA_Ping           Boolean     IMC_Common     G | Gamepad_DPad_Up
//   IA_Menu           Boolean     IMC_Common     Escape | Gamepad_Special_Right
//   IA_Run            Boolean     IMC_Active     LeftShift | Gamepad_LeftThumbstick
//   IA_Traverse       Boolean     IMC_Active     SpaceBar | Gamepad_FaceButton_Bottom
//   IA_Interact       Boolean     IMC_Active     E | Gamepad_FaceButton_Left
//   IA_Prepare        Boolean     IMC_Active     Q | Gamepad_LeftTrigger
//   IA_Attack         Boolean     IMC_Active     LeftMouseButton | Gamepad_RightTrigger
//   IA_Hide           Boolean     IMC_Active     F | Gamepad_FaceButton_Right
//   IMC_Informant     no mapping at all (its presses are caught locally for the plate, story 072)
//
// Allowed triggers: none on the action and none on any mapping, except the single default trigger (Down or
// Pressed). Hold, HoldAndRelease, Tap, Pulse and Chorded-action triggers are banned everywhere (ADR-0011 section 9:
// the server owns every hold timer). Every non-axis action is Boolean (E12: bIgnoreAllPressedKeysUntilRelease covers
// Boolean actions only).
//
// IA_WalkSlower carries the Move-magnitude REDUCTION as its value (Ctrl 0.2, Alt 0.1; the action keeps the default
// TakeHighestAbsoluteValue accumulation, so both held give 0.2): magnitude = 1 - value gives R15a's 0.8 / 0.9 / 0.8.
// The scaling itself is story iif-017; the lint checks keys and value type only. IA_Menu is the R13 "pause/menu"
// entry (no GDD name; named here). ShowTeamNames is not bound in the MVP.

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"

namespace StealthInputSpec
{
	/** Content folder that holds the project's gameplay input assets (the lint scans nothing else). */
	inline const TCHAR* const InputFolder = TEXT("/Game/StealthGame/Input");

	inline const TCHAR* const CommonContext = TEXT("IMC_Common");
	inline const TCHAR* const ActiveContext = TEXT("IMC_Active");
	inline const TCHAR* const InformantContext = TEXT("IMC_Informant");

	/** How a key's raw value is shaped before it reaches a 2D or 1D action. */
	enum class EKeyShape : uint8
	{
		None,
		Swizzle,          // 1D key onto the Y axis (forward)
		SwizzleNegate,    // 1D key onto -Y (back)
		Negate,           // 1D key onto -X (left)
		Scalar,           // value scaled by FKeySpec::Scalar (walk-slower reduction)
	};

	/** One default key of an action. */
	struct FKeySpec
	{
		FKey Key;
		EKeyShape Shape = EKeyShape::None;
		float Scalar = 1.f;
	};

	/**
	 * One gameplay action: its asset name, value type, owning context, default keys and whether it consumes its keys.
	 * bConsumeInput false (D53 integration, story iif-094): IA_Traverse lets SpaceBar also reach the reparented sample
	 * Blueprint's IA_Jump (its traversal), because the SCL traversal stories that replace it have not landed. IA_Run
	 * consumes LeftShift since story SCL-010 (the production speed tiers replaced the sample sprint shim and its
	 * UpdateInputState_Server RPC). Every other action consumes its keys, so the sample's
	 * IMC_Sandbox (lower priority) never sees Move, Look, LeftControl (the sample's jog toggle, D37) or the command keys.
	 */
	struct FActionSpec
	{
		const TCHAR* Name;
		EInputActionValueType ValueType;
		const TCHAR* Context;
		TArray<FKeySpec> Keys;
		bool bConsumeInput = true;
	};

	/** The table above, in authoring order. */
	inline const TArray<FActionSpec>& Actions()
	{
		static const TArray<FActionSpec> Table = {
			{ TEXT("IA_Move"), EInputActionValueType::Axis2D, CommonContext,
				{ { EKeys::W, EKeyShape::Swizzle }, { EKeys::S, EKeyShape::SwizzleNegate }, { EKeys::A, EKeyShape::Negate },
				  { EKeys::D }, { EKeys::Gamepad_Left2D } } },
			{ TEXT("IA_Look"), EInputActionValueType::Axis2D, CommonContext, { { EKeys::Mouse2D }, { EKeys::Gamepad_Right2D } } },
			{ TEXT("IA_CameraToggle"), EInputActionValueType::Boolean, CommonContext, { { EKeys::V }, { EKeys::Gamepad_FaceButton_Top } } },
			{ TEXT("IA_ShoulderSwap"), EInputActionValueType::Boolean, CommonContext,
				{ { EKeys::X }, { EKeys::MiddleMouseButton }, { EKeys::Gamepad_DPad_Right } } },
			{ TEXT("IA_WalkSlower"), EInputActionValueType::Axis1D, CommonContext,
				{ { EKeys::LeftControl, EKeyShape::Scalar, 0.2f }, { EKeys::LeftAlt, EKeyShape::Scalar, 0.1f } } },
			{ TEXT("IA_Ping"), EInputActionValueType::Boolean, CommonContext, { { EKeys::G }, { EKeys::Gamepad_DPad_Up } } },
			{ TEXT("IA_Menu"), EInputActionValueType::Boolean, CommonContext, { { EKeys::Escape }, { EKeys::Gamepad_Special_Right } } },
			{ TEXT("IA_Run"), EInputActionValueType::Boolean, ActiveContext, { { EKeys::LeftShift }, { EKeys::Gamepad_LeftThumbstick } }, true },
			{ TEXT("IA_Traverse"), EInputActionValueType::Boolean, ActiveContext, { { EKeys::SpaceBar }, { EKeys::Gamepad_FaceButton_Bottom } }, false },
			{ TEXT("IA_Interact"), EInputActionValueType::Boolean, ActiveContext, { { EKeys::E }, { EKeys::Gamepad_FaceButton_Left } } },
			{ TEXT("IA_Prepare"), EInputActionValueType::Boolean, ActiveContext, { { EKeys::Q }, { EKeys::Gamepad_LeftTrigger } } },
			{ TEXT("IA_Attack"), EInputActionValueType::Boolean, ActiveContext, { { EKeys::LeftMouseButton }, { EKeys::Gamepad_RightTrigger } } },
			{ TEXT("IA_Hide"), EInputActionValueType::Boolean, ActiveContext, { { EKeys::F }, { EKeys::Gamepad_FaceButton_Right } } },
		};
		return Table;
	}

	/** The three gameplay contexts, in authoring order. */
	inline TArray<const TCHAR*> Contexts()
	{
		return { CommonContext, ActiveContext, InformantContext };
	}

	/** The spec row for an action name, or null. */
	inline const FActionSpec* FindAction(const FString& Name)
	{
		return Actions().FindByPredicate([&Name](const FActionSpec& Spec) { return Name == Spec.Name; });
	}
}
