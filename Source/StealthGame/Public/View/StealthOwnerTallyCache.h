#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"

#include "StealthOwnerTallyCache.generated.h"

/**
 * The HUD's client-local copy of the last shown wrong-target tally (HUD-R7 display cache; NPC-Kill Penalty story nkp-017,
 * PEN-R3): a local-player subsystem, so it outlives a rebuilt PlayerController (reconnect, travel). It is keyed by the round
 * (FRoundPublicState::RoundIndex, the client-visible round identity: a new round shows 0), bridges a rebuild until the
 * C-TeamView owner re-push lands, and never overrides a re-push that passes the LastResultSeq gate. Written only when the
 * shown tally changes (at a plate's show time or an applied re-push), so it can never move before the plate and never
 * decreases within a round. Client-local; nothing replicates from it.
 */
UCLASS()
class STEALTHGAME_API UStealthOwnerTallyCache : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	/** Stores the shown tally of RoundIndex (a lower value within the same round is ignored: the cache never decreases). */
	void Store(uint32 RoundIndex, int32 Tally, uint32 LastSeq)
	{
		if (bHasValue && RoundIndex == CachedRoundIndex && Tally < CachedTally)
		{
			return;
		}
		bHasValue = true;
		CachedRoundIndex = RoundIndex;
		CachedTally = Tally;
		CachedSeq = LastSeq;
	}

	/** True with the cached {tally, seq} when the cache holds a value for RoundIndex. */
	bool Find(uint32 RoundIndex, int32& OutTally, uint32& OutSeq) const
	{
		if (!bHasValue || RoundIndex != CachedRoundIndex)
		{
			return false;
		}
		OutTally = CachedTally;
		OutSeq = CachedSeq;
		return true;
	}

	/** The cached tally of RoundIndex, else 0 (tests sample it every frame). */
	int32 GetTally(uint32 RoundIndex) const
	{
		int32 Tally = 0;
		uint32 Seq = 0;
		return Find(RoundIndex, Tally, Seq) ? Tally : 0;
	}

private:
	bool bHasValue = false;
	uint32 CachedRoundIndex = 0;
	int32 CachedTally = 0;
	uint32 CachedSeq = 0;
};
