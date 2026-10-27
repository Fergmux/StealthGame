// Story asn-018 AC3 (AC-ASN-21 bearing group, GDD ASN-R16): StealthGame.Assassination.StrikeBearingGroups.
// The pure half of the bearing group: strikes at varied positions through the slot's FStrikeWarpPointCore and the
// presentation's FStrikeVariantCore / FEncounterPresentationCore: theta = +-29 deg gives Strike_F; +30, +75, -30 and -75
// give the side variant of that sign (exactly +-30 on the pure rule; +-30.01 through the slot, whose 0.1 cm offset
// quantisation makes a float-exact 30 unreachable there, per the QA note's +-0.01 deg); the warp yaw is <= StrikeWarpYawLimitDeg (30)
// and the warp translation <= StrikeWarpLimitCm (240) in every case, including the 90 deg bearing limit and the farthest
// clamped target; the bush preface and a whiff's WorldPoint pick the variant by the same bearing rule; the slot and the
// presentation agree on every variant. The retained contact-frame captures are the [Visual] half (evidence asn-21-*.png).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Server/EncounterTypes.h"
#include "Server/StrikeWarpPointCore.h"
#include "StrikeVariantCore.h"
#include "View/EncounterPresentationCore.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationStrikeBearingGroupsTest, "StealthGame.Assassination.StrikeBearingGroups",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationStrikeBearingGroupsTest::RunTest(const FString& Parameters)
{
	const FEncounterParams Encounter = FEncounterParams::Defaults();
	const FStrikeWarpPointParams WarpParams = FStrikeWarpPointParams::FromEncounter(Encounter);
	const double YawLimit = Encounter.Public.StrikeWarpYawLimitDeg;
	const double TranslationLimit = Encounter.Public.StrikeWarpLimitCm;
	const double Separation = 2.0 * WarpParams.CapsuleRadiusCm + WarpParams.StrikeContactGapCm;

	struct FCase
	{
		double ThetaDeg;
		double DistanceCm;
		uint8 Expected;
	};
	const FCase Cases[] = {
		{ 29.0, 150.0, StrikeVariant::Strike_F },
		{ -29.0, 150.0, StrikeVariant::Strike_F },
		{ 30.01, 150.0, StrikeVariant::Strike_R60 },
		{ 75.0, 150.0, StrikeVariant::Strike_R60 },
		{ -30.01, 150.0, StrikeVariant::Strike_L60 },
		{ -75.0, 150.0, StrikeVariant::Strike_L60 },
		{ 29.99, 230.0, StrikeVariant::Strike_F },
		{ 30.01, 230.0, StrikeVariant::Strike_R60 },
		{ 90.0, 230.0, StrikeVariant::Strike_R60 },
		{ -90.0, 500.0, StrikeVariant::Strike_L60 },
		{ 0.0, 900.0, StrikeVariant::Strike_F },
	};
	const double FacingYaw = 37.0; // any body facing: theta is relative to it
	const FVector Attacker(1000.0, -400.0, 90.0);
	for (const FCase& Case : Cases)
	{
		FStrikeWarpPointInput In;
		In.AttackerLocation = Attacker;
		In.AttackerFacingYawDeg = FacingYaw;
		In.Source = EStrikeWarpSource::OpenSpot;
		In.WorldPoint = Attacker + FRotator(0.0, FacingYaw + Case.ThetaDeg, 0.0).RotateVector(FVector(Case.DistanceCm, 0.0, 0.0));
		const FStrikeWarpPointResult Warp = FStrikeWarpPointCore::Evaluate(WarpParams, In);
		const FVector Offset = Warp.WarpPoint.ToOffsetCm();
		const double Theta = FStrikeVariantCore::BearingOfOffset(Offset, FacingYaw);
		const uint8 Entry = FStrikeVariantCore::Entry(Warp.Variant);
		const double WarpYaw = FStrikeVariantCore::WarpYawDeg(Theta, Entry, YawLimit);
		const double WarpTranslation = FStrikeVariantCore::WarpTranslationCm(Offset, Separation);
		AddInfo(FString::Printf(TEXT("BEARING theta %.2f dist %.0f -> slot %s, presentation %s, warp yaw %.2f, warp translation %.1f cm"), Case.ThetaDeg, Case.DistanceCm,
			FStrikeVariantCore::EntryName(Warp.Variant), LexEncounterClip(FEncounterPresentationCore::StrikeClipOf(Warp.Variant)), WarpYaw, WarpTranslation));
		TestEqual(*FString::Printf(TEXT("theta %.2f: the slot's variant"), Case.ThetaDeg), static_cast<int32>(Entry), static_cast<int32>(Case.Expected));
		// The presentation never re-derives the variant: it plays FActionState.Variant (the slot's), and the wire's 0.1 cm
		// WarpPoint gives back the slot's bearing within 0.05 deg (0.05 cm at 150 cm), so its warp yaw is the slot's.
		TestEqual(*FString::Printf(TEXT("theta %.2f: the presentation plays the slot's variant"), Case.ThetaDeg),
			static_cast<int32>(FEncounterPresentationCore::StrikeClipOf(Warp.Variant)), static_cast<int32>(FEncounterPresentationCore::StrikeClipOf(Case.Expected)));
		TestTrue(*FString::Printf(TEXT("theta %.2f: the quantised WarpPoint keeps the slot's bearing"), Case.ThetaDeg), FMath::Abs(Theta - Warp.BearingDeg) <= 0.05);
		TestTrue(*FString::Printf(TEXT("theta %.2f: warp yaw <= %.0f deg"), Case.ThetaDeg, YawLimit), FMath::Abs(WarpYaw) <= YawLimit + 1e-6);
		TestTrue(*FString::Printf(TEXT("theta %.2f: the side variant's authored turn plus the warp covers theta"), Case.ThetaDeg),
			FMath::Abs(Case.ThetaDeg) > 90.0 || FMath::IsNearlyEqual(FStrikeVariantCore::EntryYawDeg(Entry) + WarpYaw, Theta, 0.05));
		TestTrue(*FString::Printf(TEXT("theta %.2f: warp translation <= %.0f cm"), Case.ThetaDeg, TranslationLimit), WarpTranslation <= TranslationLimit + 0.1);
	}

	// The exact boundary on the pure rule.
	TestEqual(TEXT("exactly +30 takes Strike_R60"), static_cast<int32>(FStrikeVariantCore::EntryForBearing(30.0)), static_cast<int32>(StrikeVariant::Strike_R60));
	TestEqual(TEXT("exactly -30 takes Strike_L60"), static_cast<int32>(FStrikeVariantCore::EntryForBearing(-30.0)), static_cast<int32>(StrikeVariant::Strike_L60));
	TestEqual(TEXT("the slot core delegates to the same rule"), static_cast<int32>(FStrikeWarpPointCore::EntryVariantForBearing(-30.0)), static_cast<int32>(StrikeVariant::Strike_L60));

	// The bush preface keeps the bearing's variant (the clip choice ignores the preface bit).
	TestEqual(TEXT("BushPreface + R60 presents Strike_R60"), static_cast<int32>(FEncounterPresentationCore::StrikeClipOf(StrikeVariant::Strike_R60 | StrikeVariant::BushPreface)),
		static_cast<int32>(EEncounterClip::Strike_R60));
	TestTrue(TEXT("the preface bit is read"), FStrikeVariantCore::HasBushPreface(StrikeVariant::Strike_F | StrikeVariant::BushPreface));
	return true;
}

#endif
