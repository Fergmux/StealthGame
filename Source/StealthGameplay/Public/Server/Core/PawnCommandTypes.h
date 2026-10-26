#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "PawnCommandTypes.generated.h"

/**
 * Server-side argument types of the command component's discrete entry points (ADR-0011 section 1; story iif-005).
 * Ids, quantised values and one weak pawn handle only; no caller identity (StealthGame.Input.NoCallerIdentityParameter
 * reflects every field). The wire form is FNetCommandArgs (Foundation View); ingress converts it to these.
 */

/** Traverse argument (ADR-0010 section 7): the Level Socket Contract point id the owner aimed at, and its CMC timestamp. */
USTRUCT()
struct STEALTHGAMEPLAY_API FTraversalHint
{
	GENERATED_BODY()

	/** 0 = no hint (the resolver picks from the pawn's own probe). */
	UPROPERTY()
	uint16 TraversalPointHint = 0;

	UPROPERTY()
	float ClientTimestamp = 0.f;
};

/**
 * Attack argument (IIF R10): the camera aim ray's world point, and optionally the pawn that ray hit, as a weak handle
 * resolved at ingress (never evidence of visibility: only CanViewerKnow and PawnRefAgreementCm admit it).
 */
USTRUCT()
struct STEALTHGAMEPLAY_API FAttackPayload
{
	GENERATED_BODY()

	UPROPERTY()
	FVector WorldPoint = FVector::ZeroVector;

	UPROPERTY()
	TWeakObjectPtr<APawn> PawnRef;
};

/** Which hide transition a Hide command asks for. */
UENUM()
enum class EHideDirection : uint8
{
	Enter,
	Exit,
};

/** Hide argument: Enter(BushId) or Exit(SideIndex) (ADR-0011 section 1). */
USTRUCT()
struct STEALTHGAMEPLAY_API FHideCommand
{
	GENERATED_BODY()

	UPROPERTY()
	EHideDirection Direction = EHideDirection::Enter;

	/** Enter only. */
	UPROPERTY()
	uint16 BushId = 0;

	/** Exit only (0-3). */
	UPROPERTY()
	uint8 SideIndex = 0;

	static FHideCommand Enter(uint16 InBushId)
	{
		FHideCommand Cmd;
		Cmd.Direction = EHideDirection::Enter;
		Cmd.BushId = InBushId;
		return Cmd;
	}

	static FHideCommand Exit(uint8 InSideIndex)
	{
		FHideCommand Cmd;
		Cmd.Direction = EHideDirection::Exit;
		Cmd.SideIndex = InSideIndex;
		return Cmd;
	}
};
