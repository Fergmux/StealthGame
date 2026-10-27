// Story asn-021 AC2 headless half (ADR-0018 RoleChangeGate, section 5.2) and the story asn-020 Refresh wiring:
// StealthGame.Assassination.RoleChangeDispatchOnce. On a real Shared Pawn in a server world, with the presentation's
// engine half and owner side replaced by recorders and a controlled clock:
//  - the death write's direct present (E7, the listen host's path) reaches UEncounterPresentationSubsystem::Refresh;
//  - OnRoleChangePresented fires for the locally viewed pawn no earlier than DeathServerTime, exactly once per FDeathState
//    however often Refresh runs (the per-frame tick, a re-open), with the victim's grade on the same frame;
//  - an FDeathState first seen 300 ms after its DeathServerTime dispatches on arrival, once;
//  - the reason follows the owner's transition: ReversedAgainst when the pawn's own strike contact is the death,
//    InformantKilled for a pawn possessed as an informant, else Assassinated; a pawn not locally viewed dispatches nothing;
//  - a bound defender's grade fires at its Counter's ContactServerTime, never on the Counter's receipt;
//  - the body sequence's engine calls: Death_Fall at DeathServerTime, collision off at + DeathFallSec, a body in view cut
//    at + 9.2 s, and the per-frame schedule ends with it.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/Core/StealthPawnCommandComponent.h"
#include "Tests/Assassination/AssassinationTestWorld.h"
#include "View/EncounterPresentation.h"

namespace RoleChangeDispatchOnceTestData
{
	struct FRecorder final : IEncounterClipPlayer, IEncounterBodyViewQuery, IEncounterOwnerPresentation
	{
		TArray<TPair<const AStealthPawn*, EEncounterClip>> Started;
		TSet<const AStealthPawn*> CollisionOff;
		TSet<const AStealthPawn*> Hidden;
		TSet<const AStealthPawn*> Viewed;
		TSet<const AStealthPawn*> Informants;
		struct FRole { const AStealthPawn* Pawn; ERoleChangeReason Reason; double Now; double Death; };
		TArray<FRole> Roles;
		struct FGrade { const AStealthPawn* Pawn; ETransientGradeCause Cause; double Now; double Due; };
		TArray<FGrade> Grades;

		virtual bool Play(AStealthPawn& Pawn, const FEncounterPresentationPlan& Plan) override { Started.Add({ &Pawn, Plan.Clip }); return true; }
		virtual double GetPosition(const AStealthPawn&, EEncounterClip) const override { return -1.0; }
		virtual void Seek(AStealthPawn&, EEncounterClip, double, double) override {}
		virtual void BeginStrikeWarp(AStealthPawn&, const FEncounterPresentationPlan&, const FEncounterPresentationParams&) override {}
		virtual void SetPawnCollisionOff(AStealthPawn& Pawn) override { CollisionOff.Add(&Pawn); }
		virtual void HideBody(AStealthPawn& Pawn) override { Hidden.Add(&Pawn); }
		virtual bool HasViewer(const UWorld&) const override { return true; }
		virtual void Query(const AStealthPawn&, bool& bInFrustum, bool& bOccluded) const override { bInFrustum = true; bOccluded = false; }
		virtual bool IsLocallyViewed(const AStealthPawn& Pawn) const override { return Viewed.Contains(&Pawn); }
		virtual bool WasPossessedAsInformant(const AStealthPawn& Pawn) const override { return Informants.Contains(&Pawn); }
		virtual void DispatchRoleChange(const AStealthPawn& Pawn, ERoleChangeReason Reason, double Now, double Death) override { Roles.Add({ &Pawn, Reason, Now, Death }); }
		virtual void ApplyTransientGrade(const AStealthPawn& Pawn, ETransientGradeCause Cause, double Now, double Due) override { Grades.Add({ &Pawn, Cause, Now, Due }); }

		int32 RolesOf(const AStealthPawn* Pawn) const { return Roles.FilterByPredicate([Pawn](const FRole& R) { return R.Pawn == Pawn; }).Num(); }
		const FRole* RoleOf(const AStealthPawn* Pawn) const { return Roles.FindByPredicate([Pawn](const FRole& R) { return R.Pawn == Pawn; }); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationRoleChangeDispatchOnceTest, "StealthGame.Assassination.RoleChangeDispatchOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationRoleChangeDispatchOnceTest::RunTest(const FString& Parameters)
{
	using namespace RoleChangeDispatchOnceTestData;
	StealthAssassinationTest::FScopedWorld World(TEXT("RoleChangeDispatchWorld"));
	UEncounterPresentationSubsystem* Presentation = UEncounterPresentationSubsystem::Get(World.World);
	if (!TestNotNull(TEXT("the presentation subsystem exists in a game world"), Presentation))
	{
		return false;
	}
	FRecorder Rec;
	double Clock = 100.0;
	Presentation->SetClipPlayer(&Rec);
	Presentation->SetBodyViewQuery(&Rec);
	Presentation->SetOwnerPresentation(&Rec);
	Presentation->SetClockOverride([&Clock]() { return Clock; }, []() { return true; });
	const double Frame = StealthAssassinationTest::ServerTickSec;
	const FEncounterPresentationParams& P = Presentation->GetParams();
	auto Run = [&](double From, double To)
	{
		for (Clock = From; Clock <= To + 1e-9; Clock += Frame)
		{
			Presentation->Tick(static_cast<float>(Frame));
		}
	};

	// 1 Assassinated: the death write arrives StrikeNetLeadSec before DeathServerTime (the host's direct path).
	AStealthPawn* Victim = World.SpawnPawn(0.0);
	Rec.Viewed.Add(Victim);
	FDeathState Death;
	Death.DeathServerTime = Clock + 0.15;
	Victim->ApplyDeathWrite(Death);
	TestEqual(TEXT("written but not due: no dispatch"), Rec.RolesOf(Victim), 0);
	TestTrue(TEXT("the pawn is scheduled until its DeathServerTime"), Presentation->IsScheduled(Victim));
	Run(Clock, Death.DeathServerTime + 0.5);
	const FRecorder::FRole* Role = Rec.RoleOf(Victim);
	TestEqual(TEXT("exactly one dispatch per FDeathState (many Refreshes)"), Rec.RolesOf(Victim), 1);
	TestTrue(TEXT("dispatched no earlier than DeathServerTime, within one frame of it"), Role != nullptr && Role->Now + 0.001 >= Death.DeathServerTime && Role->Now <= Death.DeathServerTime + Frame + 0.001);
	TestTrue(TEXT("reason Assassinated"), Role != nullptr && Role->Reason == ERoleChangeReason::Assassinated);
	TestTrue(TEXT("the victim's grade on the dispatch frame"), Rec.Grades.ContainsByPredicate([Victim, Role](const FRecorder::FGrade& G)
		{ return G.Pawn == Victim && G.Cause == ETransientGradeCause::VictimDeath && Role != nullptr && G.Now == Role->Now; }));
	TestTrue(TEXT("Death_Fall started at DeathServerTime"), Rec.Started.Contains(TPair<const AStealthPawn*, EEncounterClip>(Victim, EEncounterClip::DeathFall)));
	Presentation->Refresh(*Victim, Clock); // a re-open
	TestEqual(TEXT("a re-open does not fire again"), Rec.RolesOf(Victim), 1);
	Run(Clock, Death.DeathServerTime + P.DeathFallSec + 0.05);
	TestTrue(TEXT("collision off after the fall"), Rec.CollisionOff.Contains(Victim));
	TestFalse(TEXT("not hidden during the linger"), Rec.Hidden.Contains(Victim));
	Run(Clock, Death.DeathServerTime + 9.2 + 2.0 * Frame);
	TestTrue(TEXT("in view throughout: cut at the grace expiry"), Rec.Hidden.Contains(Victim));
	TestFalse(TEXT("the schedule ends with the cut"), Presentation->IsScheduled(Victim));

	// 2 Late: the FDeathState is first seen 300 ms after its DeathServerTime: dispatch on arrival, once.
	AStealthPawn* Late = World.SpawnPawn(400.0);
	Rec.Viewed.Add(Late);
	Rec.Informants.Add(Late);
	FDeathState LateDeath;
	LateDeath.DeathServerTime = Clock - 0.3;
	Late->ApplyDeathWrite(LateDeath);
	const FRecorder::FRole* LateRole = Rec.RoleOf(Late);
	TestTrue(TEXT("late FDeathState: dispatched on arrival"), LateRole != nullptr && FMath::IsNearlyEqual(LateRole->Now, Clock, 1e-9));
	TestTrue(TEXT("reason InformantKilled for a pawn possessed as an informant"), LateRole != nullptr && LateRole->Reason == ERoleChangeReason::InformantKilled);
	Run(Clock, Clock + 0.5);
	TestEqual(TEXT("late: still exactly once"), Rec.RolesOf(Late), 1);

	// 3 ReversedAgainst: the pawn's own strike's contact is its death.
	AStealthPawn* Reversed = World.SpawnPawn(800.0);
	Rec.Viewed.Add(Reversed);
	FActionStateWrite Strike;
	Strike.Pose = EActionPose::Strike;
	Strike.StartServerTime = Clock;
	Strike.ContactServerTime = Clock + P.StrikeContactSec;
	Strike.WarpPoint = FFixedWarpOffset::FromOffsetCm(FVector(150.0, 0.0, 0.0));
	Reversed->GetPawnCommand()->WriteActionState(Strike);
	FDeathState ReversedDeath;
	ReversedDeath.DeathServerTime = Strike.ContactServerTime;
	Clock += 0.25;
	Reversed->ApplyDeathWrite(ReversedDeath);
	TestEqual(TEXT("reversed attacker: no dispatch during its lunge"), Rec.RolesOf(Reversed), 0);
	Run(Clock, ReversedDeath.DeathServerTime + 0.1);
	const FRecorder::FRole* ReversedRole = Rec.RoleOf(Reversed);
	TestTrue(TEXT("reason ReversedAgainst, at its contact"), ReversedRole != nullptr && ReversedRole->Reason == ERoleChangeReason::ReversedAgainst
		&& ReversedRole->Now + 0.001 >= ReversedDeath.DeathServerTime);

	// 4 Not locally viewed: nothing for this machine's owner side.
	AStealthPawn* Remote = World.SpawnPawn(1200.0);
	FDeathState RemoteDeath;
	RemoteDeath.DeathServerTime = Clock;
	Remote->ApplyDeathWrite(RemoteDeath);
	Run(Clock, Clock + 0.2);
	TestEqual(TEXT("a pawn another machine owns: no dispatch, no grade here"), Rec.RolesOf(Remote), 0);
	TestTrue(TEXT("but its fall plays here"), Rec.Started.Contains(TPair<const AStealthPawn*, EEncounterClip>(Remote, EEncounterClip::DeathFall)));

	// 5 The bound defender's grade at its Counter's contact, never on receipt.
	AStealthPawn* Defender = World.SpawnPawn(1600.0);
	Rec.Viewed.Add(Defender);
	FActionStateWrite Counter;
	Counter.Pose = EActionPose::Counter;
	Counter.StartServerTime = Clock;
	Counter.ContactServerTime = Clock + 0.15;
	Defender->GetPawnCommand()->WriteActionState(Counter);
	auto DefenderGrades = [&Rec, Defender]() { return Rec.Grades.FilterByPredicate([Defender](const FRecorder::FGrade& G) { return G.Pawn == Defender; }); };
	TestEqual(TEXT("counter received before contact: no grade yet"), DefenderGrades().Num(), 0);
	Run(Clock, Counter.ContactServerTime + 0.3);
	const TArray<FRecorder::FGrade> Graded = DefenderGrades();
	TestTrue(TEXT("one defender grade, at its ContactServerTime"), Graded.Num() == 1 && Graded[0].Cause == ETransientGradeCause::DefenderCounter
		&& Graded[0].Now + 0.001 >= Counter.ContactServerTime && Graded[0].Now <= Counter.ContactServerTime + Frame + 0.001);

	Presentation->SetClipPlayer(nullptr);
	Presentation->SetBodyViewQuery(nullptr);
	Presentation->SetOwnerPresentation(nullptr);
	Presentation->SetClockOverride(nullptr, nullptr);
	return true;
}

#endif
