#pragma once

#include "CoreMinimal.h"
#include "View/ObjectiveChainData.h"

/**
 * The OC-16 table "The Ruler's Cacao" as committed constant data (GDD design/gdd/objectives-chain.md OC-16, read 2026-10-28;
 * story oc-002). The editor command Stealth.Objectives.BuildCacaoChain authors DA_ObjectiveChain_Cacao from it, and
 * StealthGame.Objectives.CacaoChainContent compares the loaded asset with it row by row. Card text follows the OC-16 framing
 * rules (the prompt verb is Tend; no objective, target, enemy, player or NPC; no place names).
 */
namespace StealthObjectivesCacaoFixture
{
	struct FRow
	{
		const TCHAR* ObjectiveId;
		EObjectiveStage Stage;
		EObjectiveEligibility Eligibility;
		EObjectiveSiteRole Role;
		const TCHAR* CardTitle;
		const TCHAR* CardLine;
	};

	/** The authored ChainSerial of the Cacao chain (stable; unique per chain). */
	inline constexpr uint16 ChainSerial = 1;

	/** The asset's package path and name. */
	inline constexpr const TCHAR* AssetFolder = TEXT("/Game/StealthGame/Objectives");
	inline constexpr const TCHAR* AssetName = TEXT("DA_ObjectiveChain_Cacao");

	/** The localisation namespace of the card text (stable keys: "<ObjectiveId>.Title" / ".Line"). */
	inline constexpr const TCHAR* TextNamespace = TEXT("ObjectiveChain_Cacao");

	inline constexpr FRow Rows[] = {
		{ TEXT("Cacao.T0.PayRunner"), EObjectiveStage::Prelim1, EObjectiveEligibility::Team0, EObjectiveSiteRole::P1,
			TEXT("Pay the Runner"), TEXT("Leave the beans for the steward's runner.") },
		{ TEXT("Cacao.T0.GatherRoot"), EObjectiveStage::Prelim2, EObjectiveEligibility::Team0, EObjectiveSiteRole::P2,
			TEXT("Gather the Root"), TEXT("Collect the bitter root set aside for your house.") },
		{ TEXT("Cacao.T1.HearRunner"), EObjectiveStage::Prelim1, EObjectiveEligibility::Team1, EObjectiveSiteRole::P1,
			TEXT("Hear the Runner"), TEXT("Hear the palace runner's news at a quiet stone.") },
		{ TEXT("Cacao.T1.TakeGourd"), EObjectiveStage::Prelim2, EObjectiveEligibility::Team1, EObjectiveSiteRole::P2,
			TEXT("Take the Gourd"), TEXT("Take the gourd left for your house.") },
		{ TEXT("Cacao.Final"), EObjectiveStage::Final, EObjectiveEligibility::Both, EObjectiveSiteRole::Final,
			TEXT("The Ruler's Cacao"), TEXT("Spoil the ruler's cup before the rival house does.") },
	};

	/** Words card text never contains as a whole word, case-insensitive (IIF language rule; OC-16). */
	inline constexpr const TCHAR* ForbiddenWords[] = { TEXT("objective"), TEXT("target"), TEXT("enemy"), TEXT("player"), TEXT("NPC") };

	/** The definition of a fixture row (stable localisation keys). */
	inline FObjectiveDef MakeDef(const FRow& Row)
	{
		FObjectiveDef Def;
		Def.ObjectiveId = FName(Row.ObjectiveId);
		Def.StageKind = Row.Stage;
		Def.TeamEligibility = Row.Eligibility;
		Def.SiteRole = Row.Role;
		Def.CompletionsRequiredCount = 1;
		Def.CardTitle = FText::AsLocalizable_Advanced(TextNamespace, FString::Printf(TEXT("%s.Title"), Row.ObjectiveId), Row.CardTitle);
		Def.CardLine = FText::AsLocalizable_Advanced(TextNamespace, FString::Printf(TEXT("%s.Line"), Row.ObjectiveId), Row.CardLine);
		Def.ClueForm = EClueForm::AreaAndFeature;
		return Def;
	}
}
