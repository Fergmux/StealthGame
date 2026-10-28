#pragma once

#include "CoreMinimal.h"
#include "View/CourtFixtureId.h"
#include "View/Core/ClueTag.h"
#include "View/ObjectiveChainData.h"

#include "ObjectiveTeamView.generated.h"

/**
 * The team view of the Objectives Chain (ADR-0020 section 4; GDD objectives-chain.md OC-5, OC-12, OC-17, SP-1; story oc-010).
 * Client-safe (View/): the ONLY objective data any client receives, carried owner-only by UObjectiveTeamViewComponent on each
 * team member's PlayerController (story oc-011). Every field encodes a fact the IVP R3 Team rows already grant:
 *
 *   StageIndex, CompletedDefMask   own-team ObjectiveProgress (never the final in the mask)
 *   LiveSites[].Court              SiteKnownToTeam: ring colour, stage card and credit display only; ingress never reads it
 *   LiveSites[].DefIndex           the stage card row in the client's own cooked chain asset (card text is local data)
 *   LiveSites[].Clue               a role-blind function of public court geometry (story oc-013 fills it)
 *   RivalPrelimCount               the OC-12 integer 0..2 (ruling B): no id, site, time or order
 *   RoundEpoch, ChainSerial        bookkeeping: a stale-view guard and which cooked chain the indices refer to
 *
 * It never holds a locked site, an opponent site, opponent progress other than RivalPrelimCount, anyone's hold progress, or
 * the final before the team's own preliminaries are complete; the final's completion is never written to it (the round-end
 * step presents it). No FString, FText or FName member: only indices and enums travel.
 *
 * Wire (NetSerialize, SerializeBits only, ADR-0018's fixed-width lesson): RoundEpoch 32 | ChainSerial 16 | StageIndex 2 |
 * RivalPrelimCount 2 | LiveSiteCount 2 | CompletedDefMask 16 (70 bits), then per live site Court 16 | DefIndex 5 | AreaType 8 |
 * FeatureKind 8 (37 bits). At most MaxLiveSites (3, the hard maximum of ObjectivesPerStageCount) sites travel, so the size is
 * at most 12 + 4 x LiveSiteCount bytes (StealthGame.Objectives.TeamViewWidthAndChangeOnly).
 */

/** One of the team's own live sites (its current stage only). */
USTRUCT()
struct STEALTHGAMEPLAY_API FObjectiveLiveSite
{
	GENERATED_BODY()

	/** The court (SiteKnownToTeam; presentation only). */
	UPROPERTY()
	FCourtFixtureId Court;

	/** The definition row in the cooked chain (the card row); 0xFF = none (never sent by the builder). */
	UPROPERTY()
	uint8 DefIndex = 0xFF;

	/** The court's role-blind clue (story oc-013; {0, 0} until then). */
	UPROPERTY()
	FClueTag Clue;

	bool operator==(const FObjectiveLiveSite& Other) const { return Court == Other.Court && DefIndex == Other.DefIndex && Clue == Other.Clue; }
	bool operator!=(const FObjectiveLiveSite& Other) const { return !(*this == Other); }
};

/** The whole objective view of one team (every member, Active or Informant, receives the same value). */
USTRUCT()
struct STEALTHGAMEPLAY_API FObjectiveTeamView
{
	GENERATED_BODY()

	/** The hard maximum of ObjectivesPerStageCount (1-3): at most this many live sites travel. */
	static constexpr int32 MaxLiveSites = 3;
	/** The fixed part of the wire, in bits. */
	static constexpr int32 FixedWireBits = 32 + 16 + 2 + 2 + 2 + 16;
	/** One live site on the wire, in bits. */
	static constexpr int32 SiteWireBits = 16 + 5 + 8 + 8;
	/** The DefIndex wire value that means "none" (0xFF in memory). */
	static constexpr uint8 NoDefWire = 0x1F;

	/** Serialised width in bits of a view with LiveSiteCount sites (capped at MaxLiveSites). */
	static constexpr int32 WireBits(int32 LiveSiteCount) { return FixedWireBits + SiteWireBits * (LiveSiteCount < MaxLiveSites ? LiveSiteCount : MaxLiveSites); }
	/** ADR-0020 section 4's budget: 12 bytes plus 4 per live site. */
	static constexpr int32 BudgetBytes(int32 LiveSiteCount) { return 12 + 4 * LiveSiteCount; }

	/** The round the view belongs to (0 = no round: the empty default). */
	UPROPERTY()
	uint32 RoundEpoch = 0;

	/** UObjectiveChainData::ChainSerial of the chain the indices refer to (0 = none). */
	UPROPERTY()
	uint16 ChainSerial = 0;

	/** The team's place in the chain (None before Active entry). */
	UPROPERTY()
	EObjectiveTeamStage StageIndex = EObjectiveTeamStage::None;

	/** The team's own current-stage live sites only (<= ObjectivesPerStageCount; the final only after its own prelims). */
	UPROPERTY()
	TArray<FObjectiveLiveSite> LiveSites;

	/** The team's own completed preliminary rows (bit = row index); never the final. */
	UPROPERTY()
	uint16 CompletedDefMask = 0;

	/** Rival preliminary completions released to this team by OC-12 (0..2; stories oc-014/015 write it). */
	UPROPERTY()
	uint8 RivalPrelimCount = 0;

	/** True for the empty default (no round, no stage, no site). */
	bool IsEmpty() const { return *this == FObjectiveTeamView(); }

	/** The live site at Court, or null. */
	const FObjectiveLiveSite* FindLiveSite(FCourtFixtureId Court) const
	{
		return LiveSites.FindByPredicate([Court](const FObjectiveLiveSite& Site) { return Site.Court == Court; });
	}

	/** Writes or reads exactly WireBits(LiveSites.Num()) bits with SerializeBits. Always true (no object references). */
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);

	/** Field-wise equality (the change-only write compares with it). */
	bool Identical(const FObjectiveTeamView* Other, uint32 PortFlags) const { return Other != nullptr && *this == *Other; }

	bool operator==(const FObjectiveTeamView& Other) const
	{
		return RoundEpoch == Other.RoundEpoch && ChainSerial == Other.ChainSerial && StageIndex == Other.StageIndex && LiveSites == Other.LiveSites
			&& CompletedDefMask == Other.CompletedDefMask && RivalPrelimCount == Other.RivalPrelimCount;
	}
	bool operator!=(const FObjectiveTeamView& Other) const { return !(*this == Other); }
};

template <>
struct TStructOpsTypeTraits<FObjectiveTeamView> : public TStructOpsTypeTraitsBase2<FObjectiveTeamView>
{
	enum
	{
		WithNetSerializer = true,
		WithNetSharedSerialization = true,
		WithIdentical = true,
	};
};
