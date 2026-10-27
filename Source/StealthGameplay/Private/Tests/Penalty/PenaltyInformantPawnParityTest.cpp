// Story nkp-015 (production/epics/npc-kill-penalty-informants/story-015-informant-contract-and-pawn-parity.md), AC1 /
// AC-NKP-17 (PEN-R13, Tell Matrix T7, Crowd AC-CNB-30, SCL AC-SCL-16; ADR-0019 section 5): Penalty's view of the owners'
// enforcement, on the production Shared Pawn class (AStealthPawn), never a test double:
//   1. Parity dump: GIVEN an informant pawn (possessed by a PlayerController, its gait legality Informant through
//      IStealthGaitLegalitySource) and an NPC pawn (possessed by an AIController, legality Active), WHEN their replication
//      settings, collision profiles and responses, capsule, avoidance and observable movement settings are dumped, THEN the
//      two dumps are equal, and the dumped field names are exactly the checked-in fixture list (a dropped field fails).
//   2. Run clamp (SCL R6, ADR-0010 section 6): the informant's Gait = Run on the owner path is clamped to Walk (speed never
//      above WalkSpeedCmPerSec); an Active pawn's Run is faster (control).
//   3. NotNow (IIF R7, through the slot's legality re-check, the IIF legality stand-in): an informant's Attack and Prepare
//      are voided NotNow; an Active pawn's Prepare executes (control).
// Not asserted here (the owner is not built; story nkp-015 is Blocked on it): Traverse, Interact and Hide NotNow for the
// Informant legality class are input-interaction-framework story 057's (no Informant predicate is registered for those
// three kinds yet). Penalty adds no informant-only rule of its own (ADR-0019 section 5).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Tests/Assassination/EncounterTestRig.h"
#include "Tests/Locomotion/LocomotionTestWorld.h"

namespace PenaltyInformantPawnParityTestData
{
	class FStubLegality final : public IStealthGaitLegalitySource
	{
	public:
		explicit FStubLegality(EStealthGaitLegality InClass) : Class(InClass) {}
		virtual EStealthGaitLegality GetGaitLegality(const APawn&) const override { return Class; }
		EStealthGaitLegality Class;
	};

	/** Frames of the Run drive (4 s at 60 Hz: past the run acceleration). */
	constexpr int32 RunFrames = 240;

	/**
	 * The checked-in expected-name fixture (hand-derived from SCL R3 and R13: replication, collision, capsule, avoidance and
	 * the movement settings that change what an observer sees). The dump must produce exactly these names, in this order.
	 */
	const TCHAR* const ExpectedFields[] = {
		TEXT("Actor.bReplicates"), TEXT("Actor.bReplicateMovement"), TEXT("Actor.NetUpdateFrequency"), TEXT("Actor.MinNetUpdateFrequency"),
		TEXT("Actor.NetPriority"), TEXT("Actor.bAlwaysRelevant"), TEXT("Actor.bOnlyRelevantToOwner"), TEXT("Actor.bNetUseOwnerRelevancy"),
		TEXT("Actor.NetDormancy"), TEXT("Actor.NetCullDistanceSquared"), TEXT("Actor.bUseControllerRotationYaw"),
		TEXT("Capsule.Radius"), TEXT("Capsule.HalfHeight"), TEXT("Capsule.Profile"), TEXT("Capsule.CollisionEnabled"), TEXT("Capsule.ObjectType"),
		TEXT("Capsule.Responses"), TEXT("Mesh.Profile"), TEXT("Mesh.CollisionEnabled"), TEXT("Mesh.Responses"),
		TEXT("Move.bUseRVOAvoidance"), TEXT("Move.AvoidanceConsiderationRadius"), TEXT("Move.AvoidanceWeight"), TEXT("Move.AvoidanceGroup"),
		TEXT("Move.GroupsToAvoid"), TEXT("Move.GroupsToIgnore"), TEXT("Move.MaxAcceleration"), TEXT("Move.BrakingDecelerationWalking"),
		TEXT("Move.GroundFriction"), TEXT("Move.RotationRate"), TEXT("Move.bOrientRotationToMovement"), TEXT("Move.bUseControllerDesiredRotation"),
		TEXT("Move.NetworkSmoothingMode"), TEXT("Move.MaxStepHeight"), TEXT("Move.WalkableFloorAngle"), TEXT("Move.bCanWalkOffLedges"),
		TEXT("Move.NavAgentRadius"), TEXT("Move.NavAgentHeight"),
	};

	FString Responses(const UPrimitiveComponent* Component)
	{
		FString Out;
		const FCollisionResponseContainer& Container = Component->GetCollisionResponseToChannels();
		for (int32 Channel = 0; Channel < ECC_OverlapAll_Deprecated; ++Channel) // the container holds 32 channels (ECC_MAX is past it)
		{
			Out.AppendInt(static_cast<int32>(Container.GetResponse(static_cast<ECollisionChannel>(Channel))));
		}
		return Out;
	}

	/** The fixed field list of one pawn (names in ExpectedFields order). */
	TArray<TPair<FString, FString>> Dump(const AStealthPawn* Pawn)
	{
		TArray<TPair<FString, FString>> Out;
		const auto Add = [&Out](const TCHAR* Name, const FString& Value) { Out.Emplace(Name, Value); };
		const auto Num = [](double Value) { return FString::Printf(TEXT("%.4f"), Value); };
		const auto Flag = [](bool bValue) { return FString(bValue ? TEXT("1") : TEXT("0")); };

		Add(TEXT("Actor.bReplicates"), Flag(Pawn->GetIsReplicated()));
		Add(TEXT("Actor.bReplicateMovement"), Flag(Pawn->IsReplicatingMovement()));
		Add(TEXT("Actor.NetUpdateFrequency"), Num(Pawn->GetNetUpdateFrequency()));
		Add(TEXT("Actor.MinNetUpdateFrequency"), Num(Pawn->GetMinNetUpdateFrequency()));
		Add(TEXT("Actor.NetPriority"), Num(Pawn->NetPriority));
		Add(TEXT("Actor.bAlwaysRelevant"), Flag(Pawn->bAlwaysRelevant));
		Add(TEXT("Actor.bOnlyRelevantToOwner"), Flag(Pawn->bOnlyRelevantToOwner));
		Add(TEXT("Actor.bNetUseOwnerRelevancy"), Flag(Pawn->bNetUseOwnerRelevancy));
		Add(TEXT("Actor.NetDormancy"), FString::FromInt(static_cast<int32>(Pawn->NetDormancy)));
		Add(TEXT("Actor.NetCullDistanceSquared"), Num(Pawn->GetNetCullDistanceSquared()));
		Add(TEXT("Actor.bUseControllerRotationYaw"), Flag(Pawn->bUseControllerRotationYaw));

		const UCapsuleComponent* Capsule = Pawn->GetCapsuleComponent();
		Add(TEXT("Capsule.Radius"), Num(Capsule->GetUnscaledCapsuleRadius()));
		Add(TEXT("Capsule.HalfHeight"), Num(Capsule->GetUnscaledCapsuleHalfHeight()));
		Add(TEXT("Capsule.Profile"), Capsule->GetCollisionProfileName().ToString());
		Add(TEXT("Capsule.CollisionEnabled"), FString::FromInt(static_cast<int32>(Capsule->GetCollisionEnabled())));
		Add(TEXT("Capsule.ObjectType"), FString::FromInt(static_cast<int32>(Capsule->GetCollisionObjectType())));
		Add(TEXT("Capsule.Responses"), Responses(Capsule));
		const USkeletalMeshComponent* Mesh = Pawn->GetMesh();
		Add(TEXT("Mesh.Profile"), Mesh != nullptr ? Mesh->GetCollisionProfileName().ToString() : FString(TEXT("none")));
		Add(TEXT("Mesh.CollisionEnabled"), Mesh != nullptr ? FString::FromInt(static_cast<int32>(Mesh->GetCollisionEnabled())) : FString(TEXT("none")));
		Add(TEXT("Mesh.Responses"), Mesh != nullptr ? Responses(Mesh) : FString(TEXT("none")));

		const UStealthMovementComponent* Move = Pawn->GetStealthMovement();
		Add(TEXT("Move.bUseRVOAvoidance"), Flag(Move->bUseRVOAvoidance));
		Add(TEXT("Move.AvoidanceConsiderationRadius"), Num(Move->AvoidanceConsiderationRadius));
		Add(TEXT("Move.AvoidanceWeight"), Num(Move->AvoidanceWeight));
		Add(TEXT("Move.AvoidanceGroup"), FString::FromInt(Move->AvoidanceGroup.Packed));
		Add(TEXT("Move.GroupsToAvoid"), FString::FromInt(Move->GroupsToAvoid.Packed));
		Add(TEXT("Move.GroupsToIgnore"), FString::FromInt(Move->GroupsToIgnore.Packed));
		Add(TEXT("Move.MaxAcceleration"), Num(Move->MaxAcceleration));
		Add(TEXT("Move.BrakingDecelerationWalking"), Num(Move->BrakingDecelerationWalking));
		Add(TEXT("Move.GroundFriction"), Num(Move->GroundFriction));
		Add(TEXT("Move.RotationRate"), Move->RotationRate.ToCompactString());
		Add(TEXT("Move.bOrientRotationToMovement"), Flag(Move->bOrientRotationToMovement));
		Add(TEXT("Move.bUseControllerDesiredRotation"), Flag(Move->bUseControllerDesiredRotation));
		Add(TEXT("Move.NetworkSmoothingMode"), FString::FromInt(static_cast<int32>(Move->NetworkSmoothingMode)));
		Add(TEXT("Move.MaxStepHeight"), Num(Move->MaxStepHeight));
		Add(TEXT("Move.WalkableFloorAngle"), Num(Move->GetWalkableFloorAngle()));
		Add(TEXT("Move.bCanWalkOffLedges"), Flag(Move->bCanWalkOffLedges));
		Add(TEXT("Move.NavAgentRadius"), Num(Move->GetNavAgentPropertiesRef().AgentRadius));
		Add(TEXT("Move.NavAgentHeight"), Num(Move->GetNavAgentPropertiesRef().AgentHeight));
		return Out;
	}

	bool IsVoided(const FEncounterCommandReply& Reply, ECommandRejectCode Code)
	{
		return !Reply.bExecuted && Reply.Code == Code;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyInformantPawnParityTest, "StealthGame.Penalty.InformantPawnParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyInformantPawnParityTest::RunTest(const FString& Parameters)
{
	using namespace PenaltyInformantPawnParityTestData;
	StealthLocomotionTest::FWorld TestWorld(TEXT("PenaltyInformantPawnParityWorld"));
	FStubLegality Informant(EStealthGaitLegality::Informant);
	FStubLegality Active(EStealthGaitLegality::Active);

	// 1. Parity dump: an informant pawn (player-possessed, Informant legality) and an NPC pawn (AI-possessed).
	{
		AStealthPawn* InformantPawn = TestWorld.SpawnPawn(FVector2D(-6000.0, 0.0));
		AStealthPawn* NpcPawn = TestWorld.SpawnPawn(FVector2D(-6000.0, 1000.0));
		APlayerController* Player = TestWorld.World->SpawnActor<APlayerController>();
		AAIController* Ai = TestWorld.World->SpawnActor<AAIController>();
		if (Player == nullptr || Ai == nullptr)
		{
			AddError(TEXT("fixture: the two controllers spawn"));
			return false;
		}
		Player->Possess(InformantPawn);
		Ai->Possess(NpcPawn);
		InformantPawn->GetStealthMovement()->SetGaitLegalitySource(&Informant);
		NpcPawn->GetStealthMovement()->SetGaitLegalitySource(&Active);
		TestTrue(TEXT("fixture: the informant pawn is player-possessed, the NPC pawn AI-possessed"), InformantPawn->GetController() == Player && NpcPawn->GetController() == Ai);

		const TArray<TPair<FString, FString>> A = Dump(InformantPawn);
		const TArray<TPair<FString, FString>> B = Dump(NpcPawn);
		TestEqual(TEXT("the dump has exactly the fixture's fields"), A.Num(), static_cast<int32>(UE_ARRAY_COUNT(ExpectedFields)));
		for (int32 Index = 0; Index < A.Num() && Index < B.Num(); ++Index)
		{
			const bool bNamed = Index < static_cast<int32>(UE_ARRAY_COUNT(ExpectedFields)) && A[Index].Key == ExpectedFields[Index] && B[Index].Key == A[Index].Key;
			TestTrue(FString::Printf(TEXT("field %d is the fixture's %s"), Index, Index < static_cast<int32>(UE_ARRAY_COUNT(ExpectedFields)) ? ExpectedFields[Index] : TEXT("?")), bNamed);
			TestEqual(FString::Printf(TEXT("%s: informant pawn == NPC pawn"), *A[Index].Key), A[Index].Value, B[Index].Value);
		}
	}

	// 2. Run is clamped to Walk for the informant (the owner path); an Active pawn's Run is faster (control).
	{
		AStealthPawn* InformantPawn = TestWorld.SpawnPawn(FVector2D(-2000.0, 0.0));
		AStealthPawn* ActivePawn = TestWorld.SpawnPawn(FVector2D(-2000.0, 2000.0));
		InformantPawn->GetStealthMovement()->SetGaitLegalitySource(&Informant);
		ActivePawn->GetStealthMovement()->SetGaitLegalitySource(&Active);
		double InformantPeak = 0.0;
		double ActivePeak = 0.0;
		for (int32 Frame = 0; Frame < RunFrames; ++Frame)
		{
			StealthLocomotionTest::DriveOwner(InformantPawn, 0.0f, 1.0f, EGait::Run);
			StealthLocomotionTest::DriveOwner(ActivePawn, 0.0f, 1.0f, EGait::Run);
			StealthLocomotionTest::StepPawn(InformantPawn);
			StealthLocomotionTest::StepPawn(ActivePawn);
			InformantPeak = FMath::Max(InformantPeak, StealthLocomotionTest::Speed2D(InformantPawn));
			ActivePeak = FMath::Max(ActivePeak, StealthLocomotionTest::Speed2D(ActivePawn));
		}
		const double Walk = InformantPawn->GetStealthMovement()->GetLocomotionTuning().WalkSpeedCmPerSec;
		AddInfo(FString::Printf(TEXT("NKP015 Run: informant peak %.2f cm/s, Active peak %.2f cm/s, walk %.2f"), InformantPeak, ActivePeak, Walk));
		TestTrue(FString::Printf(TEXT("informant Run: speed stays <= WalkSpeedCmPerSec (peak %.2f)"), InformantPeak), InformantPeak <= Walk + 0.01);
		TestTrue(TEXT("informant Run: it does walk (the input is not dropped)"), InformantPeak > 0.5 * Walk);
		TestTrue(TEXT("informant Run: the effective gait is Walk"), InformantPawn->GetStealthMovement()->GetEffectiveGait() == EGait::Walk);
		TestTrue(TEXT("control: an Active pawn's Run is faster than Walk"), ActivePeak > Walk + 1.0);
	}

	// 3. Attack and Prepare by an informant: NotNow (the slot's IIF legality re-check); an Active pawn's Prepare executes.
	{
		using namespace StealthEncounterTest;
		FRig R(TEXT("PenaltyInformantNotNow"));
		APawn* InformantPawn = R.Spawn(FVector::ZeroVector, 0);
		APawn* Target = R.Spawn(FVector(150.0, 0.0, 0.0));
		APawn* ActivePawn = R.Spawn(FVector(0.0, 1200.0, 0.0), 1);
		R.Roster.Members[InformantPawn].State = EParticipantState::Informant;
		R.AttackRef(InformantPawn, Target);
		TestTrue(TEXT("informant Attack: NotNow"), IsVoided(R.LastReply(), ECommandRejectCode::NotNow));
		TestTrue(TEXT("informant Attack: nothing executed, the target untouched"), R.Core->GetResolutions().Num() == 0 && !R.Core->IsDead(Target));
		R.Command(InformantPawn, ECommandKind::PrepareBegin);
		TestTrue(TEXT("informant Prepare: NotNow"), IsVoided(R.LastReply(), ECommandRejectCode::NotNow));
		TestTrue(TEXT("control: an Active pawn's Prepare executes"), R.Command(ActivePawn, ECommandKind::PrepareBegin) == EResolveResult::Executed);
	}
	return true;
}

#endif
