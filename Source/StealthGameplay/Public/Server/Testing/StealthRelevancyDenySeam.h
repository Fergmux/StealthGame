#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

class AActor;
class AStealthPawn;

/**
 * The test-only per-connection relevancy deny seam (story asn-022, AC-ASN-45; ADR-0003 sections 5.6-5.8, ADR-0018 section
 * 3.2 step 3). In the MVP no pawn has a cull distance, dormancy or bOnlyRelevantToOwner (SCL R13, ADR-0003 section 3), so
 * a victim is never culled for a reason other than concealment; this seam stands for such a cull so the defensive
 * re-open path can be exercised:
 *  - Deny(Pawn, Viewer, Release) makes Pawn not relevant to Viewer's connection (AStealthPawn::IsNetRelevantFor) and closes
 *    that connection's open channel for it explicitly (EChannelCloseReason::Relevancy), as the S9 cull does in its tick;
 *  - Release AtDeathWrite: AStealthPawn::ApplyDeathWrite calls NotifyDeathWrite, which lifts every deny of the pawn in the
 *    death-write step, after FDeathState is set (the Visibility Policy hook's re-open: the record opens in that tick or the
 *    next with FDeathState in its initial block, never in the slot);
 *  - Release Manual: only Release() lifts it (the late-actor runs and the negative control, which releases in the slot).
 * The harness must leave at least one round trip between a close and the release (the S10 in-flight race, S10-R2).
 *
 * Usage (server world, inside a PIE test):
 *   StealthRelevancyDenySeam::Deny(VictimPawn, WitnessController, StealthRelevancyDenySeam::ERelease::AtDeathWrite);
 *   ... strike the victim; the witness's client receives it again only with its FDeathState ...
 *   StealthRelevancyDenySeam::ReleaseAll();   // in the test's teardown
 *
 * Compiled only under WITH_STEALTH_TEST_SEAMS (absent from Shipping). Game thread only.
 */
#if WITH_STEALTH_TEST_SEAMS
namespace StealthRelevancyDenySeam
{
	/** When a deny is lifted. */
	enum class ERelease : uint8
	{
		/** In the pawn's death-write step (NotifyDeathWrite). */
		AtDeathWrite,
		/** Only by Release / ReleaseAll. */
		Manual,
	};

	/** One lifted deny (observation for tests). */
	struct FReleaseRecord
	{
		const AActor* Pawn = nullptr;
		const AActor* Viewer = nullptr;
		/** GFrameCounter and the pawn world's time seconds at the release. */
		uint64 Frame = 0;
		double WorldTime = 0.0;
		bool bAtDeathWrite = false;
	};

	/**
	 * Denies Pawn to Viewer (the viewing connection's player controller) and closes that connection's open actor channel for
	 * Pawn (Relevancy), if any. Returns true when a channel was closed.
	 */
	STEALTHGAMEPLAY_API bool Deny(AStealthPawn* Pawn, const AActor* Viewer, ERelease Release);

	/** Lifts the denies of Pawn (every viewer when Viewer is null). */
	STEALTHGAMEPLAY_API void Release(const AActor* Pawn, const AActor* Viewer = nullptr);

	/** Lifts every deny and forgets the release records. */
	STEALTHGAMEPLAY_API void ReleaseAll();

	/** True while Pawn is denied to RealViewer (AStealthPawn::IsNetRelevantFor). */
	STEALTHGAMEPLAY_API bool IsDenied(const AActor* Pawn, const AActor* RealViewer);

	/** AStealthPawn::ApplyDeathWrite: lifts Pawn's AtDeathWrite denies in this step. */
	STEALTHGAMEPLAY_API void NotifyDeathWrite(const AActor* Pawn);

	/** Every release since the last ReleaseAll, in order. */
	STEALTHGAMEPLAY_API const TArray<FReleaseRecord>& GetReleases();
}
#endif
