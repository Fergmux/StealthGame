// Story asn-012 AC3 (AC-ASN-47, ADR-0018 section 2.2): StealthGame.Assassination.WarpPointBlind. The attacker at the
// origin facing +X; each case is a fresh resolver rig with the recording pawn-state writer, and the WarpPoint is read from
// the attacker's FActionState write (and the resolution record):
//   bush group  BushStrikes (no PawnRef) at one WorldPoint into bush 7 (footprint 140..260 x -60..60): empty, ally-only
//               (a concealed teammate), a concealed NPC, a concealed rival. Every occupant stands >= 50 cm from the
//               WorldPoint. The WarpPoint is byte-identical in the group, equals the WorldPoint clamped to the footprint's
//               attacker-side edge (140, 0, 0), and differs from every occupant's offset by more than the 0.1 cm quantum;
//               a WorldPoint outside the footprint (in the tolerance band) clamps into it, then to the attacker-side edge.
//   open group  TargetSpot strikes at one WorldPoint with no pawn there and with a culled rival there (its PawnRef
//               unhonoured): identical, = the WorldPoint, never the culled pawn's offset.
//   PawnRef     an honoured, visible, moving rival: WarpPoint = quantised(F1 P_V(T_v) - P_A) exactly.
//   edges       |dz| clamps at StrikeVerticalReachCm; theta = +-30 exactly takes the side variant; a strike from a bush
//               sets the BushPreface bit.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Tests/Assassination/EncounterTestRig.h"

namespace WarpPointBlindTestData
{
	using namespace StealthEncounterTest;

	constexpr uint16 BushId = 7;
	const FVector BushCenter(200.0, 0.0, 0.0);
	constexpr double FootprintHalfCm = 60.0;
	const FVector BushPoint(200.0, 0.0, 0.0);
	const FVector OpenPoint(150.0, 0.0, 0.0);
	/** The minimum distance of every occupant from the WorldPoint (QA rewrite of AC3: equality must be distinguishable). */
	constexpr double MinOccupantDistanceCm = 50.0;

	enum class EOccupant : uint8
	{
		None,
		Ally,
		Npc,
		Rival,
	};

	struct FResult
	{
		bool bWritten = false;
		FFixedWarpOffset WarpPoint;
		uint8 Variant = 0;
		EAttackOutcome Outcome = EAttackOutcome::Whiff;
		/** The occupant's offset from the attacker (zero when none). */
		FVector OccupantOffset = FVector::ZeroVector;
		bool bHasOccupant = false;
	};

	/** Reads the attacker's one Strike write and checks it matches the resolution record. */
	FResult Read(FRig& R, APawn* Attacker)
	{
		FResult Out;
		const TArray<FRecordingPawnState::FWrite> Writes = R.PawnState.For(Attacker);
		if (Writes.Num() == 1 && Writes[0].State.Pose == EActionPose::Strike && R.Core->GetResolutions().Num() == 1
			&& R.Last().WarpPoint == Writes[0].State.WarpPoint && R.Last().StrikeVariantBits == Writes[0].State.Variant)
		{
			Out.bWritten = true;
			Out.WarpPoint = Writes[0].State.WarpPoint;
			Out.Variant = Writes[0].State.Variant;
			Out.Outcome = R.Last().Outcome;
		}
		return Out;
	}

	FResult BushStrike(EOccupant Occupant, const FVector& WorldPoint, const FVector& OccupantAt)
	{
		FRig R(TEXT("WarpPointBlindWorld"), nullptr, /*bWithPawnState*/ true);
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		R.AddBush(BushId, BushCenter, FootprintHalfCm);
		FResult Out;
		if (Occupant != EOccupant::None)
		{
			APawn* Pawn = Occupant == EOccupant::Npc ? R.Spawn(OccupantAt) : R.Spawn(OccupantAt, Occupant == EOccupant::Ally ? 0 : 1);
			R.Conceal(Pawn, BushId);
			Out.bHasOccupant = true;
			Out.OccupantOffset = OccupantAt;
		}
		R.Attack(A, WorldPoint);
		const FResult Written = Read(R, A);
		Out.bWritten = Written.bWritten;
		Out.WarpPoint = Written.WarpPoint;
		Out.Variant = Written.Variant;
		Out.Outcome = Written.Outcome;
		return Out;
	}

	/** Units (0.1 cm) between a WarpPoint and an offset in cm. */
	double DistanceUnits(const FFixedWarpOffset& WarpPoint, const FVector& OffsetCm)
	{
		return FVector::Dist(WarpPoint.ToOffsetCm(), OffsetCm) / FFixedWarpOffset::UnitCm;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationWarpPointBlindTest, "StealthGame.Assassination.WarpPointBlind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationWarpPointBlindTest::RunTest(const FString& Parameters)
{
	using namespace WarpPointBlindTestData;
	const FEncounterParams Params = FEncounterParams::Defaults();

	// --- Bush group ----------------------------------------------------------------------------------------------------
	struct FCase
	{
		const TCHAR* Name;
		EOccupant Occupant;
		FVector At;
	};
	const FCase Cases[] = {
		{ TEXT("empty bush"), EOccupant::None, FVector::ZeroVector },
		{ TEXT("ally-only bush"), EOccupant::Ally, FVector(200.0, 55.0, 0.0) },
		{ TEXT("concealed NPC"), EOccupant::Npc, FVector(230.0, -45.0, 0.0) },
		{ TEXT("concealed rival"), EOccupant::Rival, FVector(175.0, 50.0, 0.0) },
	};
	const FFixedWarpOffset ExpectedBush = FFixedWarpOffset::FromOffsetCm(FVector(BushCenter.X - FootprintHalfCm, 0.0, 0.0));
	for (const FCase& Case : Cases)
	{
		const FResult Result = BushStrike(Case.Occupant, BushPoint, Case.At);
		AddInfo(FString::Printf(TEXT("WARPBLIND %s: outcome %d, WarpPoint (%d, %d, %d)"), Case.Name, static_cast<int32>(Result.Outcome), Result.WarpPoint.X, Result.WarpPoint.Y, Result.WarpPoint.Z));
		TestTrue(FString::Printf(TEXT("%s: the attacker's one Strike write carries the WarpPoint"), Case.Name), Result.bWritten);
		TestTrue(FString::Printf(TEXT("%s: WarpPoint = the WorldPoint clamped to the attacker-side footprint edge (140, 0, 0)"), Case.Name), Result.WarpPoint == ExpectedBush);
		if (Result.bHasOccupant)
		{
			TestTrue(FString::Printf(TEXT("%s: the occupant stands >= 50 cm from the WorldPoint"), Case.Name), FVector::Dist2D(Case.At, BushPoint) >= MinOccupantDistanceCm);
			TestTrue(FString::Printf(TEXT("%s: WarpPoint differs from the occupant's offset by more than the quantum"), Case.Name), DistanceUnits(Result.WarpPoint, Result.OccupantOffset) > 1.0);
		}
	}
	TestTrue(TEXT("the bush group includes a kill (the rule ran: the outcome differs while the WarpPoint does not)"),
		BushStrike(EOccupant::Rival, BushPoint, Cases[3].At).Outcome == EAttackOutcome::Kill && BushStrike(EOccupant::None, BushPoint, FVector::ZeroVector).Outcome == EAttackOutcome::Whiff);

	// Edge: a WorldPoint outside the footprint (inside the tolerance band) is clamped into it, then to the attacker's edge:
	// (200, 75) -> (200, 60) -> the segment from the origin enters the box at x = 140: (140, 42).
	{
		const FResult Outside = BushStrike(EOccupant::None, FVector(200.0, 75.0, 0.0), FVector::ZeroVector);
		TestTrue(TEXT("outside the footprint: clamped to the attacker-side edge (140, 42, 0)"), Outside.WarpPoint == FFixedWarpOffset::FromOffsetCm(FVector(140.0, 42.0, 0.0)));
	}

	// --- Open group ----------------------------------------------------------------------------------------------------
	FFixedWarpOffset OpenEmpty;
	FFixedWarpOffset OpenCulled;
	const FVector CulledAt(150.0, 55.0, 0.0);
	{
		FRig R(TEXT("WarpPointBlindWorld"), nullptr, true);
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		R.Attack(A, OpenPoint);
		OpenEmpty = Read(R, A).WarpPoint;
		TestTrue(TEXT("open, nobody there: a whiff"), R.Last().Outcome == EAttackOutcome::Whiff);
	}
	{
		FRig R(TEXT("WarpPointBlindWorld"), nullptr, true);
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* Rival = R.Spawn(CulledAt, 1);
		R.Visibility.Culled.Add(Rival);
		R.Attack(A, OpenPoint, Rival); // the culled reference is unhonoured: identical to a TargetSpot
		OpenCulled = Read(R, A).WarpPoint;
		TestTrue(TEXT("open, culled rival there: the open-spot kill"), R.Last().Outcome == EAttackOutcome::Kill && R.Last().Rule == EVictimRule::OpenSpot);
	}
	TestTrue(TEXT("open group: WarpPoint identical with and without the culled pawn"), OpenEmpty == OpenCulled);
	TestTrue(TEXT("open group: WarpPoint = the WorldPoint"), OpenEmpty == FFixedWarpOffset::FromOffsetCm(OpenPoint));
	TestTrue(TEXT("open group: never the culled pawn's offset"), DistanceUnits(OpenCulled, CulledAt) > 1.0);

	// --- Honoured PawnRef: the victim's F1 extrapolation, exactly -------------------------------------------------------
	{
		FRig R(TEXT("WarpPointBlindWorld"), nullptr, true);
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		APawn* Rival = R.Spawn(OpenPoint, 1);
		R.Facts(Rival).Velocity = FVector(0.0, 100.0, 0.0);
		const FStrikeReachResult Reach = FVictimResolverCore::Reach(Params, EStrikeReachVariant::Standard, R.Facts(A), R.Facts(Rival));
		R.AttackRef(A, Rival);
		const FResult Result = Read(R, A);
		const FFixedWarpOffset Expected = FFixedWarpOffset::FromOffsetCm(Reach.VictimAtContact - R.Facts(A).Location);
		AddInfo(FString::Printf(TEXT("WARPBLIND PawnRef: P_V(T_v) %s, WarpPoint (%d, %d, %d)"), *Reach.VictimAtContact.ToCompactString(), Result.WarpPoint.X, Result.WarpPoint.Y, Result.WarpPoint.Z));
		TestTrue(TEXT("PawnRef: a kill by rule 1"), R.Last().Outcome == EAttackOutcome::Kill && R.Last().Rule == EVictimRule::PawnRef);
		TestTrue(TEXT("PawnRef: WarpPoint = quantised(P_V(T_v) - P_A) exactly"), Result.WarpPoint == Expected);
		TestTrue(TEXT("PawnRef: the extrapolation moved the target off the WorldPoint (the check is not vacuous)"), Expected != FFixedWarpOffset::FromOffsetCm(OpenPoint));
	}

	// --- Edges -----------------------------------------------------------------------------------------------------------
	{
		FRig R(TEXT("WarpPointBlindWorld"), nullptr, true);
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		R.Attack(A, FVector(150.0, 0.0, 500.0));
		TestTrue(TEXT("|dz| clamps at StrikeVerticalReachCm"), Read(R, A).WarpPoint == FFixedWarpOffset::FromOffsetCm(FVector(150.0, 0.0, Params.Server.StrikeVerticalReachCm)));
	}
	TestTrue(TEXT("theta = +30 exactly takes Strike_R60"), FStrikeWarpPointCore::EntryVariantForBearing(30.0) == StrikeVariant::Strike_R60);
	TestTrue(TEXT("theta = -30 exactly takes Strike_L60"), FStrikeWarpPointCore::EntryVariantForBearing(-30.0) == StrikeVariant::Strike_L60);
	TestTrue(TEXT("|theta| just below 30 takes Strike_F"), FStrikeWarpPointCore::EntryVariantForBearing(29.999) == StrikeVariant::Strike_F && FStrikeWarpPointCore::EntryVariantForBearing(-29.999) == StrikeVariant::Strike_F);
	{
		FRig R(TEXT("WarpPointBlindWorld"), nullptr, true);
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		R.Attack(A, FVector(150.0, 150.0, 0.0)); // 45 degrees to the right
		TestTrue(TEXT("a target 45 degrees right writes Strike_R60"), (Read(R, A).Variant & StrikeVariant::EntryMask) == StrikeVariant::Strike_R60);
	}
	{
		FRig R(TEXT("WarpPointBlindWorld"), nullptr, true);
		APawn* A = R.Spawn(FVector::ZeroVector, 0);
		R.Bushes.Concealed.Add(A); // striking from a bush
		R.Attack(A, OpenPoint);
		const FResult Result = Read(R, A);
		TestTrue(TEXT("a strike from a bush sets the BushPreface bit"), (Result.Variant & StrikeVariant::BushPreface) != 0 && (Result.Variant & StrikeVariant::EntryMask) == StrikeVariant::Strike_F);
		TestTrue(TEXT("its ContactServerTime adds BushStrikeEmergeSec"), R.Core->GetResolutions().Num() == 1
			&& FMath::IsNearlyEqual(R.PawnState.For(A)[0].State.ContactServerTime - R.PawnState.For(A)[0].State.StartServerTime, Params.Public.StrikeContactSec + Params.BushStrikeEmergeSec, 1e-6));
	}
	return true;
}

#endif
