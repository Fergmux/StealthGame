#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

/**
 * The camera's collision vocabulary (ADR-0015 section 4.1, Migration Plan 3; story cs-007). The eye-origin mask (story
 * cs-012/013) traces ViewBlock: what a body's eye cannot see through. The boom (story cs-006) traces the engine Camera
 * channel. Both are set explicitly on every production profile; the expectation table below is the single source the
 * ini guard (StealthGame.Camera.CollisionProfiles) and the asset validator (StealthCollisionProfileValidator) read.
 *
 *   ViewBlock  ECC_GameTraceChannel2, trace type, default Block (Config/DefaultEngine.ini +DefaultChannelResponses).
 *              Walls, doors, props, kit modules and court fixtures Block it; pawns, bush foliage, glass, decoration and
 *              the water pawn-blocking volume Ignore it.
 *   Camera     the engine channel the boom probes: pawns and bush foliage Ignore it (the lens may sit inside the leaf dome,
 *              ADR-0017), walls and kit Block it.
 *
 * Usage:
 *   World->LineTraceSingleByChannel(Hit, Eye, Probe, StealthCollision::ViewBlock);
 */
namespace StealthCollision
{
	/** The one ViewBlock channel constant (the ini names ECC_GameTraceChannel2 "ViewBlock"). */
	inline constexpr ECollisionChannel ViewBlock = ECC_GameTraceChannel2;
	/** The channel's ini name. */
	inline constexpr const TCHAR* ViewBlockName = TEXT("ViewBlock");

	/** One profile's expected camera responses (the table this story writes; ADR-0017 supplies the foliage row). */
	struct FProfileExpectation
	{
		const TCHAR* Profile;
		/** What the profile is for (the AC2 role it stands for). */
		const TCHAR* Role;
		ECollisionResponse ViewBlock;
		ECollisionResponse Camera;
		/** Both responses must be written in the profile's CustomResponses (not inherited from a channel default). */
		bool bExplicit;
	};

	/** The production profiles and their ViewBlock / Camera responses. */
	inline constexpr FProfileExpectation ProfileTable[] = {
		{ TEXT("Pawn"), TEXT("pawn capsule"), ECR_Ignore, ECR_Ignore, true },
		{ TEXT("CharacterMesh"), TEXT("pawn mesh"), ECR_Ignore, ECR_Ignore, true },
		{ TEXT("StealthKit"), TEXT("walls, doors, kit modules, court fixtures"), ECR_Block, ECR_Block, true },
		{ TEXT("StealthProp"), TEXT("props"), ECR_Block, ECR_Block, true },
		{ TEXT("StealthFoliage"), TEXT("bush foliage (ADR-0017)"), ECR_Ignore, ECR_Ignore, true },
		{ TEXT("StealthGlass"), TEXT("glass"), ECR_Ignore, ECR_Block, true },
		{ TEXT("StealthDecoration"), TEXT("decoration"), ECR_Ignore, ECR_Ignore, true },
		{ TEXT("WaterBodyCollision"), TEXT("the water pawn-blocking volume"), ECR_Ignore, ECR_Ignore, true },
		{ TEXT("BlockAll"), TEXT("stock static blocker (kit fallback)"), ECR_Block, ECR_Block, false },
		{ TEXT("BlockAllDynamic"), TEXT("stock dynamic blocker (doors fallback)"), ECR_Block, ECR_Block, false },
	};

	/** The expectation of Profile, or null for a profile the table does not govern. */
	inline const FProfileExpectation* FindExpectation(FName Profile)
	{
		for (const FProfileExpectation& Row : ProfileTable)
		{
			if (Profile == FName(Row.Profile))
			{
				return &Row;
			}
		}
		return nullptr;
	}
}
