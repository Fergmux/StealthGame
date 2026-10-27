// Stories asn-007 / asn-011 (the facade): UEncounterService over its production adapters, registered on a real
// FResolutionQueueCore. In a bare game world (no Roster store: every pawn Unbound, i.e. an NPC with legality Active, the
// Roster's own Unbound answers) an Attack item from one AStealthPawn at another 150 cm ahead (the world pawn query reads
// their actors) kills it in its slot: OnPawnKilled(Npc, Strike) fires, the attacker's UDefenceStateComponent is created
// with LockoutEnd = t_exec + 1.75, and the DeathWrite is staged. A third pawn's Prepare plants the SCL Brace pose. The
// service registers for PawnCommand and HoldTimer once (a second registration is refused) and unregisters on Shutdown.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/DefenceStateComponent.h"
#include "Server/EncounterService.h"
#include "Server/Testing/StealthQueueTestRig.h"
#include "Tests/Assassination/AssassinationTestWorld.h"
#include "View/Core/StealthPawnMovementComponent.h"

namespace EncounterServiceSmokeTestData
{
	/** A probe applier (registers only if the service released the staged-write types). */
	class FCountingAppliers final : public IStagedWriteApplier
	{
	public:
		virtual void ApplyStagedWrite(const FStagedWrite& Write) override {}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationEncounterServiceSmokeTest, "StealthGame.Assassination.EncounterServiceSmoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationEncounterServiceSmokeTest::RunTest(const FString& Parameters)
{
	using namespace EncounterServiceSmokeTestData;
	StealthAssassinationTest::FScopedWorld World(TEXT("EncounterServiceSmokeWorld"));
	FStealthQueueTestRig Q;
	AStealthPawn* Attacker = World.SpawnPawn(0.0);
	AStealthPawn* Victim = World.SpawnPawn(150.0);
	AStealthPawn* Bracer = World.SpawnPawn(-3000.0);

	UEncounterService* Service = NewObject<UEncounterService>();
	FEncounterServiceInit Init;
	Init.World = World.World;
	Init.Clock = &Q.Clock;
	Init.Queue = Q.Queue.Get();
	Init.SubSeeds = &Q.Seeds;
	TestTrue(TEXT("the service initialises"), Service->Initialize(Init));
	TestTrue(TEXT("it registers for PawnCommand and HoldTimer"), Service->RegisterOn(*Q.Queue));
	TestFalse(TEXT("a second registration is refused"), Service->RegisterOn(*Q.Queue));
	int32 NpcStrikes = 0;
	Service->GetOnPawnKilled()->AddLambda([&NpcStrikes, Victim](APawn* V, APawn*, EVictimKind Kind, EKillCause Cause)
	{
		NpcStrikes += V == Victim && Kind == EVictimKind::Npc && Cause == EKillCause::Strike ? 1 : 0;
	});

	FCommandPayload Attack;
	Attack.Pawn.Pawn = Attacker;
	Attack.Kind = static_cast<uint8>(ECommandKind::Attack);
	Attack.WorldPoint = Victim->GetActorLocation();
	Attack.Target = FSubjectRef::ForPawn(FPawnHandle{ Victim });
	FCommandPayload Prepare;
	Prepare.Pawn.Pawn = Bracer;
	Prepare.Kind = static_cast<uint8>(ECommandKind::PrepareBegin);
	Q.Queue->Submit(FMailboxId::ServerLocal(), FQueueItem::Make(EQueueItemType::PawnCommand, Q.Epoch.Current, FMailboxId::ServerLocal(), Attack));
	Q.Queue->Submit(FMailboxId::ServerLocal(), FQueueItem::Make(EQueueItemType::PawnCommand, Q.Epoch.Current, FMailboxId::ServerLocal(), Prepare));
	Q.Tick();
	const double Exec = Q.Clock.GetServerTimeSeconds();

	TestTrue(TEXT("the Attack killed the victim in its slot"), Service->IsDead(Victim) && NpcStrikes == 1);
	const UDefenceStateComponent* AttackerState = UDefenceStateComponent::Find(*Attacker);
	TestTrue(TEXT("the attacker's defence component holds LockoutEnd = t_exec + 1.75"), AttackerState != nullptr
		&& AttackerState->GetCore().GetLockoutEnd() == Exec + static_cast<double>(FEncounterParams::Defaults().Public.AttackAttemptLockoutSec));
	TestTrue(TEXT("the victim's DeathWrite is staged"), Q.Queue->GetPendingStagedWrites().Num() == 1 && Q.Queue->GetPendingStagedWrites()[0].Type == EStagedWriteType::DeathWrite);
	TestTrue(TEXT("GetScheduledDeathServerTime is the contact time"), Service->GetScheduledDeathServerTime(Victim).IsSet());
	TestTrue(TEXT("the bracer is planted (SCL Brace pose)"), Bracer->GetStealthMovement()->GetActionPose() == EStealthActionPose::Brace);
	// Two timers: the bracer's CapEnd and the attacker's Strike pose end (story asn-012: the production pawn-state writer).
	TestTrue(TEXT("the bracer's CapEnd and the attacker's pose end are scheduled"), Q.Queue->GetPendingTimerCount() == 2);
	TestTrue(TEXT("the attacker's FActionState is the Strike (story asn-012)"), Attacker->GetActionState().Pose == EActionPose::Strike && Attacker->GetActionState().Seq == 2);
	TestTrue(TEXT("the bracer's FActionState is Brace"), Bracer->GetActionState().Pose == EActionPose::Brace);

	Service->Shutdown();
	FStealthStubResolver Probe;
	TestTrue(TEXT("Shutdown unregistered both types"), Q.Queue->RegisterResolver(EQueueItemType::PawnCommand, &Probe) && Q.Queue->RegisterResolver(EQueueItemType::HoldTimer, &Probe));
	FCountingAppliers Appliers;
	TestTrue(TEXT("Shutdown unregistered the DeathWrite and CounterPose appliers (story asn-013)"), Q.Queue->RegisterStagedWriteApplier(EStagedWriteType::DeathWrite, &Appliers)
		&& Q.Queue->RegisterStagedWriteApplier(EStagedWriteType::CounterPose, &Appliers));
	return true;
}

#endif
