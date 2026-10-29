// Stories asn-005 (rules 1, 3, 4 and the candidate filter) and asn-006 (rule 2, the BushSpotPick draw)
// (production/epics/assassination-defence-reversal/). See Server/VictimResolverCore.h. Replaces the prototype's
// FindAssassinationTarget cone and BlindStrikeRadius sphere in AStealthGameMode::HandleAssassinationRequest.

#include "Server/VictimResolverCore.h"

#include "Server/BushSpotQuery.h"
#include "Server/Random/StealthSecretStream.h"
#include "Server/Random/StealthServerSeeds.h"
#include "Server/StrikeRoleTable.h"
#include "View/Random/StealthRangedDraw.h"

namespace
{
	/**
	 * The BushSpotPick stream for one strike: counter-mode FSecretStream draws keyed (ServerTickIndex, Seq, BushId, i), so
	 * a strike's pick depends only on its own key and no shared stream position exists (ADR-0006 section 5, mode 0x02).
	 */
	struct FBushPickStream
	{
		const FSecretStream& Stream;
		uint64 Tick = 0;
		uint64 Seq = 0;
		uint64 BushId = 0;
		uint64 Counter = 0;
		int32 Draws = 0;

		uint32 NextU32()
		{
			++Draws;
			const uint64 Tuple[4] = { Tick, Seq, BushId, Counter++ };
			return static_cast<uint32>(Stream.Draw(MakeArrayView(Tuple, 4)));
		}
	};

	double HorizontalDistance(const FVector& A, const FVector& B)
	{
		return FVector::Dist2D(A, B);
	}
}

bool FVictimResolverCore::IsCandidate(const FVictimResolverDeps& Deps, const APawn* Attacker, const APawn* Pawn)
{
	if (Pawn == nullptr || Pawn == Attacker)
	{
		return false;
	}
	if (Deps.Roles != nullptr && Deps.Roles->IsDead(Pawn))
	{
		return false;
	}
	if (Deps.Roster->AreTeammates(Attacker, Pawn))
	{
		return false;
	}
	return Deps.Roster->ResolveVictimKind(Pawn).Kind != ERosterVictimKind::NotTargetable;
}

FStrikeReachResult FVictimResolverCore::Reach(const FEncounterParams& Params, EStrikeReachVariant Variant, const FEncounterPawnFacts& Attacker, const FEncounterPawnFacts& Victim)
{
	FStrikeReachInput Input;
	Input.AttackerLocation = Attacker.Location;
	Input.AttackerFacingYawDeg = Attacker.FacingYawDeg;
	Input.VictimLocation = Victim.Location;
	Input.VictimVelocity = Victim.Velocity;
	Input.VictimBrakingCmPerSec2 = Victim.BrakingCmPerSec2;
	return FStrikeReachCore::Evaluate(Params.ReachParams(Variant), Input);
}

FVictimResolution FVictimResolverCore::Resolve(const FEncounterParams& Params, const FVictimResolverDeps& Deps, const FVictimRequest& Request)
{
	FVictimResolution Out;
	APawn* Attacker = Request.Attacker.Pawn;
	if (Deps.Pawns == nullptr || Deps.Roster == nullptr)
	{
		return Out;
	}

	// Rule 1: an honoured PawnRef (knowable and in agreement with the WorldPoint).
	FEncounterPawnFacts RefFacts;
	if (Request.PawnRef != nullptr && Request.PawnRef != Attacker && Deps.Visibility != nullptr && Deps.Pawns->GetFacts(Request.PawnRef, RefFacts)
		&& HorizontalDistance(RefFacts.Location, Request.WorldPoint) <= Params.PawnRefAgreementCm && Deps.Visibility->CanViewerKnow(Attacker, Request.PawnRef))
	{
		const bool bDeadRole = Deps.Roles != nullptr && Deps.Roles->IsDead(Request.PawnRef);
		const FRosterVictimKind Kind = Deps.Roster->ResolveVictimKind(Request.PawnRef);
		if (bDeadRole || (Kind.Kind == ERosterVictimKind::NotTargetable && Kind.Reason == ERosterNotTargetableReason::AlreadyResolved))
		{
			Out.bRefHonoured = true;
			Out.RefAtContact = FVictimResolverCore::Reach(Params, Request.Variant, Request.Attacker, RefFacts).VictimAtContact;
			return Out; // a body: terminal whiff (never falls through to a spot rule, so a bystander beside it is never struck)
		}
		if (IsCandidate(Deps, Attacker, Request.PawnRef))
		{
			const FStrikeReachResult Reach = FVictimResolverCore::Reach(Params, Request.Variant, Request.Attacker, RefFacts);
			Out.bRefHonoured = true;
			Out.RefAtContact = Reach.VictimAtContact;
			if (Reach.bInReach)
			{
				Out.Victim = Request.PawnRef;
				Out.Rule = EVictimRule::PawnRef;
				Out.Reach = Reach;
			}
			return Out; // an honoured candidate out of reach is a whiff, never a fall-through
		}
		// An honoured ally (forged ref) or a stale ref: allies are excluded and resolution continues (ASN-R9).
	}

	// Rule 2: the bush spot (terminal).
	uint16 BushId = 0;
	if (Deps.Bushes != nullptr && Deps.Bushes->FindSpotZone(Request.WorldPoint, BushId))
	{
		Out.BushId = BushId;
		Out.bInBushZone = true;
		TArray<APawn*> Occupants;
		Deps.Bushes->GetSpotOccupants(BushId, Attacker, Occupants);
		struct FReachable
		{
			APawn* Pawn;
			uint64 Serial;
			FStrikeReachResult Reach;
		};
		TArray<FReachable, TInlineAllocator<8>> Reachable;
		for (APawn* Occupant : Occupants)
		{
			FEncounterPawnFacts Facts;
			if (!IsCandidate(Deps, Attacker, Occupant) || !Deps.Pawns->GetFacts(Occupant, Facts))
			{
				continue;
			}
			++Out.CandidateCount;
			const FStrikeReachResult Reach = FVictimResolverCore::Reach(Params, Request.Variant, Request.Attacker, Facts);
			if (Reach.bInReach)
			{
				Reachable.Add({ Occupant, Facts.SpawnSerial, Reach });
			}
		}
		// Canonical order before the draw, so the pick never depends on Concealment's container order.
		Reachable.Sort([](const FReachable& A, const FReachable& B) { return A.Serial < B.Serial; });
		Out.ReachableCount = Reachable.Num();
		if (Deps.SubSeeds == nullptr)
		{
			return Out; // no secret stream bound: a wiring defect; never pick from a public source
		}
		const FSecretStream Stream(Deps.SubSeeds->GetServerSubSeed(ESeedDomain::BushSpotPick, Params.LayoutAttempt), BushSpotPickTag);
		FBushPickStream Pick{ Stream, Request.Key.ServerTickIndex, Request.Key.Seq, BushId };
		if (Reachable.Num() == 0)
		{
			Pick.NextU32(); // one draw per resolving bush strike, whatever |Reachable| is
			Out.BushDraws = Pick.Draws;
			return Out;
		}
		const uint32 Index = StealthRandom::UniformBelow(Pick, static_cast<uint32>(Reachable.Num()));
		Out.BushDraws = Pick.Draws;
		Out.Victim = Reachable[Index].Pawn;
		Out.Rule = EVictimRule::BushSpot;
		Out.Reach = Reachable[Index].Reach;
		return Out;
	}

	// Rule 3: the open spot (non-concealed candidates only).
	TArray<FEncounterPawnFacts> Pawns;
	Deps.Pawns->GetPawns(Pawns);
	const double Radius = Params.OpenSpotRadiusCm();
	const FEncounterPawnFacts* Best = nullptr;
	double BestDistance = TNumericLimits<double>::Max();
	FStrikeReachResult BestReach;
	for (const FEncounterPawnFacts& Facts : Pawns)
	{
		const double Distance = HorizontalDistance(Facts.Location, Request.WorldPoint);
		if (Distance > Radius || !IsCandidate(Deps, Attacker, Facts.Pawn) || (Deps.Bushes != nullptr && Deps.Bushes->IsConcealed(Facts.Pawn)))
		{
			continue;
		}
		const FStrikeReachResult Reach = FVictimResolverCore::Reach(Params, Request.Variant, Request.Attacker, Facts);
		if (!Reach.bInReach)
		{
			continue;
		}
		if (Best == nullptr || Distance < BestDistance || (Distance == BestDistance && Facts.SpawnSerial < Best->SpawnSerial))
		{
			Best = &Facts;
			BestDistance = Distance;
			BestReach = Reach;
		}
	}
	if (Best != nullptr)
	{
		Out.Victim = Best->Pawn;
		Out.Rule = EVictimRule::OpenSpot;
		Out.Reach = BestReach;
	}
	// Rule 4: otherwise a whiff.
	return Out;
}
