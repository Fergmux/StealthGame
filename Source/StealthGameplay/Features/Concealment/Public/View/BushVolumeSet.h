#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "BushVolumeSet.generated.h"

class UBoxComponent;
struct FBushRuntime;

/**
 * The one locally spawned, non-replicated actor holding every hide volume of the layout (ADR-0017 section 1, E5; story
 * con-002): one UBoxComponent per bush on the BushVolume object channel, blocking Pawn only (it ignores Camera, ViewBlock,
 * Visibility and every other trace, so a camera or aim inside a bush passes through). Spawned by UBushRuntimeSubsystem on
 * every machine from the same Manifest; bReplicates = false, never relevant to anyone, never saved. The hide volumes
 * affect navigation on the server (NPC routes go around a planter, not into it).
 */
UCLASS(NotBlueprintable, NotPlaceable, Transient)
class STEALTHGAMEPLAY_API ABushVolumeSet : public AActor
{
	GENERATED_BODY()

public:
	ABushVolumeSet();

	/** Creates one box per bush (replacing any previous set). */
	void BuildVolumes(TConstArrayView<FBushRuntime> Bushes);

	/** The boxes, in bush order. */
	const TArray<TObjectPtr<UBoxComponent>>& GetVolumes() const { return Volumes; }

	/** The bush id of each box, in the same order. */
	const TArray<uint16>& GetVolumeBushIds() const { return VolumeBushIds; }

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBoxComponent>> Volumes;

	TArray<uint16> VolumeBushIds;
};
