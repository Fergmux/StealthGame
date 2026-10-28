// Objectives Chain story oc-016 (and oc-013's phrase table): the client-side presentation helpers. See View/ObjectiveViewHelpers.h.

#include "View/ObjectiveViewHelpers.h"

#include "StealthGameplayModule.h"
#include "View/ObjectiveChainData.h"

#define LOCTEXT_NAMESPACE "ObjectiveClues"

namespace ObjectiveViewHelpersLocal
{
	/** The area phrases (key = EClueAreaType). */
	bool AreaPhrase(uint8 Area, FText& Out)
	{
		switch (static_cast<EClueAreaType>(Area))
		{
		case EClueAreaType::Causeway: Out = LOCTEXT("Area_Causeway", "on the causeway"); return true;
		case EClueAreaType::Market: Out = LOCTEXT("Area_Market", "in the market"); return true;
		case EClueAreaType::Temple: Out = LOCTEXT("Area_Temple", "in the temple quarter"); return true;
		case EClueAreaType::Residential: Out = LOCTEXT("Area_Residential", "among the houses"); return true;
		default: return false;
		}
	}

	/** The feature phrases (key = EClueFeatureKind; None has no phrase: the area-only form). */
	bool FeaturePhrase(uint8 Feature, FText& Out)
	{
		switch (static_cast<EClueFeatureKind>(Feature))
		{
		case EClueFeatureKind::WaterSteps: Out = LOCTEXT("Feature_WaterSteps", "by the water steps"); return true;
		case EClueFeatureKind::Shrine: Out = LOCTEXT("Feature_Shrine", "by the shrine"); return true;
		case EClueFeatureKind::Stairway: Out = LOCTEXT("Feature_Stairway", "by the stairway"); return true;
		case EClueFeatureKind::Well: Out = LOCTEXT("Feature_Well", "by the well"); return true;
		case EClueFeatureKind::Stalls: Out = LOCTEXT("Feature_Stalls", "by the stalls"); return true;
		case EClueFeatureKind::GreatTree: Out = LOCTEXT("Feature_GreatTree", "by the great tree"); return true;
		case EClueFeatureKind::Archway: Out = LOCTEXT("Feature_Archway", "by the archway"); return true;
		default: return false;
		}
	}
}

bool ObjectiveViewHelpers::IsOwnLiveSite(const FObjectiveTeamView& View, FCourtFixtureId Court)
{
	return Court.IsSet() && View.FindLiveSite(Court) != nullptr;
}

ObjectiveViewHelpers::FStageCardRow ObjectiveViewHelpers::GetStageCardRow(const FObjectiveTeamView& View, const UObjectiveChainData* Chain)
{
	FStageCardRow Out;
	if (Chain == nullptr || View.IsEmpty() || View.LiveSites.Num() == 0 || Chain->ChainSerial != View.ChainSerial)
	{
		return Out;
	}
	const int32 Row = View.LiveSites[0].DefIndex;
	if (!Chain->Definitions.IsValidIndex(Row))
	{
		return Out;
	}
	Out.Title = Chain->Definitions[Row].CardTitle;
	Out.Line = Chain->Definitions[Row].CardLine;
	Out.DefIndex = Row;
	return Out;
}

FText ObjectiveViewHelpers::BuildClueText(const FClueTag& Clue)
{
	using namespace ObjectiveViewHelpersLocal;
	if (Clue.AreaType == 0 && Clue.FeatureKind == 0)
	{
		return FText::GetEmpty(); // no clue published: no clue line
	}
	FText Area;
	FText Feature;
	const bool bArea = AreaPhrase(Clue.AreaType, Area);
	const bool bFeature = Clue.FeatureKind == 0 || FeaturePhrase(Clue.FeatureKind, Feature);
	if (!bArea || !bFeature)
	{
		UE_LOGF(LogStealthGameplay, Warning, "[Objectives] clue phrase table has no key (area %d, feature %d): the safe fallback is shown", static_cast<int32>(Clue.AreaType),
			static_cast<int32>(Clue.FeatureKind));
		return LOCTEXT("Clue_Fallback", "A court");
	}
	if (Clue.FeatureKind == 0)
	{
		return FText::Format(LOCTEXT("Clue_AreaOnly", "A court {0}"), Area);
	}
	return FText::Format(LOCTEXT("Clue_AreaAndFeature", "A court {0}, {1}"), Area, Feature);
}

bool ObjectiveViewHelpers::RingIsMarigold(const FObjectiveTeamView& View, FCourtFixtureId OwnHoldTarget, double HoldServerStartTime, double EstimatedServerNow,
	double TendCueDelaySec, bool bIsInformant)
{
	return !bIsInformant && HoldServerStartTime >= 0.0 && IsOwnLiveSite(View, OwnHoldTarget) && (EstimatedServerNow - HoldServerStartTime) >= TendCueDelaySec;
}

#undef LOCTEXT_NAMESPACE
