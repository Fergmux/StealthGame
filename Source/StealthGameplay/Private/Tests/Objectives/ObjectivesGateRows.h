#pragma once

// The six-row completion-gate table of AC-OC-06 (stories oc-008 stage rows, oc-009 final rows): each row sets up one completer
// on a fresh FCoreRig, then the HoldTimer(Complete) slot runs once. Test-only.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ObjectivesTestRig.h"

namespace StealthObjectivesTest
{
	enum class EGateRow : uint8
	{
		/** (1) Active + Connected + human-controlled on an eligible team. */
		HumanEligible,
		/** (2) AI-driven: a hand-over committed mid-hold (the control epoch changed and the possession is pending). */
		AiDriven,
		/** (3) Unresponsive (Session's state). */
		Unresponsive,
		/** (4) Disconnected. */
		Disconnected,
		/** (5) an Informant at the slot. */
		Informant,
		/** (6) Active on a team for which the site is not Live. */
		SiteNotLiveForTeam,
	};

	inline const TCHAR* LexGateRow(EGateRow Row)
	{
		switch (Row)
		{
		case EGateRow::HumanEligible: return TEXT("(1) human, eligible");
		case EGateRow::AiDriven: return TEXT("(2) AI-driven hand-over");
		case EGateRow::Unresponsive: return TEXT("(3) Unresponsive");
		case EGateRow::Disconnected: return TEXT("(4) Disconnected");
		case EGateRow::Informant: return TEXT("(5) Informant");
		default: return TEXT("(6) site not Live for the team");
		}
	}

	inline const EGateRow AllGateRows[] = { EGateRow::HumanEligible, EGateRow::AiDriven, EGateRow::Unresponsive, EGateRow::Disconnected,
		EGateRow::Informant, EGateRow::SiteNotLiveForTeam };

	/**
	 * Runs one row at Court (Live for team 0): the completer (participant 1, team 0; team 1 for row 6) begins (claim, epoch 1),
	 * the row's condition is applied, then the Complete slot runs with Key.
	 */
	inline EObjectivesCompletion RunGateRow(FCoreRig& Rig, const StealthAssassinationTest::FScopedWorld& World, EGateRow Row, uint16 Court, FQueueKey Key)
	{
		AStealthPawn* Pawn = FCoreRig::PawnAt(World, Court, 0);
		check(Pawn != nullptr);
		Rig.Roster.Bind(Pawn, 1, Row == EGateRow::SiteNotLiveForTeam ? 1 : 0);
		Rig.PawnControl.Epochs.Add(Pawn, 1);
		Rig.Core.OnInteractExecuted(*Pawn, FCourtFixtureId(Court), FQueueKey(1, 1));
		switch (Row)
		{
		case EGateRow::AiDriven:
			Rig.PawnControl.Epochs[Pawn] = 2; // the hand-over commit bumped the epoch
			Rig.PawnControl.Pending.Add(Pawn); // and its possession is pending
			break;
		case EGateRow::Unresponsive:
			Rig.Session.States.Add(1, EObjectivesConnectionState::Unresponsive);
			break;
		case EGateRow::Disconnected:
			Rig.Session.States.Add(1, EObjectivesConnectionState::Disconnected);
			break;
		case EGateRow::Informant:
			Rig.Roster.States[1] = EParticipantState::Informant;
			break;
		default:
			break;
		}
		Rig.Round.Key = Key;
		return Rig.Core.OnInteractEnded(*Pawn, FCourtFixtureId(Court), EObjectivesInteractEnd::Completed, Key);
	}
}

#endif
