// Story oc-007 (production/epics/objectives-chain/story-007-interact-predicate-and-feint-parity.md): the Interact predicate
// (ADR-0020 section 5.3; GDD OC-7, OC-11). Its own translation unit so the static check of
// StealthGame.Objectives.PredicateSiteAndClassOnly can scan it: the predicate reads ONLY the court, the court's public
// stand-point occupancy (active-hold claims) and the pawn's legality class. It calls no Roster team or binding query, no
// assignment accessor and no site-state accessor, so it cannot know which court is live (the hidden-site oracle guard,
// SP-1): the reply is identical at a live, locked, completed, opponent or hidden-final court. It never returns
// AllowNoEffect at Begin; whether a tend counts is decided only at the completion slot (OC-10).
//   1. legality class Informant or RoundEnded        -> Reject(NotNow) (the IIF's informant plate; no prompt shown)
//   2. claims at the court >= FixtureInteractSideCount -> Reject(NotNow) (a full fixture; identical at every court)
//   3. otherwise                                       -> Allow

#include "Server/ObjectivesService.h"

#include "ObjectivesCore.h"
#include "View/Core/StealthPawn.h"

namespace ObjectivesInteractPredicate
{
	/** The three steps over the three inputs. Pure. */
	FPredicateResult Evaluate(EObjectivesLegalityClass Legality, int32 ClaimCount, int32 SideCount)
	{
		if (Legality == EObjectivesLegalityClass::Informant || Legality == EObjectivesLegalityClass::RoundEnded)
		{
			return FPredicateResult::Reject(ECommandRejectCode::NotNow);
		}
		if (ClaimCount >= SideCount)
		{
			return FPredicateResult::Reject(ECommandRejectCode::NotNow);
		}
		return FPredicateResult::Allow();
	}
}

FPredicateResult UObjectivesService::CanBegin(const AStealthPawn& Pawn, const FCommandPayload& Cmd) const
{
	const EObjectivesLegalityClass Legality = Roster != nullptr ? Roster->GetLegalityClass(Pawn) : EObjectivesLegalityClass::RoundEnded;
	const FCourtFixtureId Court(Cmd.Target.Kind == ESubjectRefKind::CourtSocket ? Cmd.Target.CourtSocketId : 0);
	return ObjectivesInteractPredicate::Evaluate(Legality, Core->GetClaimCount(Court), Core->GetFixtureSideCount());
}
