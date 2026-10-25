#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Server/StealthTuningSubstructs.h"

#include "StealthTuningData.generated.h"

/**
 * A tuning preset (ADR-0004 section 1; stories tdc-039, tdc-040): the only preset class. One
 * UPROPERTY per owner system, each a _Public/_Server struct pair (an owner with no public key has
 * only _Server). The members mirror FStealthServerTuning exactly (same names and types), so a
 * preset's values copy into the server face member by member; the CanonicalKeys test checks it.
 *
 * Server code never reads this asset directly (it reads GetServerTuning() / FValidatedTuning);
 * client and presentation code never reference it (ADR-0004 guideline 3). No Config flag (CR-13).
 * Primary asset type StealthTuningPreset (registered with AlwaysCook by story tdc-057).
 */
UCLASS(NotBlueprintType, NotBlueprintable)
class STEALTHFOUNDATION_API UStealthTuningData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The Asset Manager primary asset type of every preset. */
	static const FPrimaryAssetType PrimaryAssetType;

	/** Returns {StealthTuningPreset, <object name>} (a UPrimaryDataAsset otherwise reports its class name as the type). */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, Category = "Round")
	FRoundTuning_Public Round_Public;

	UPROPERTY(EditAnywhere, Category = "Round")
	FRoundTuning_Server Round_Server;

	UPROPERTY(EditAnywhere, Category = "Crowd")
	FCrowdTuning_Server Crowd_Server;

	UPROPERTY(EditAnywhere, Category = "Assassination")
	FAssassinationTuning_Public Assassination_Public;

	UPROPERTY(EditAnywhere, Category = "Penalty")
	FPenaltyTuning_Public Penalty_Public;

	UPROPERTY(EditAnywhere, Category = "Objectives")
	FObjectivesTuning_Public Objectives_Public;
};
