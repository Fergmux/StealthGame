// Story con-023 (production/epics/concealment-bushes/story-023-owner-and-team-concealed-flags.md; GDD CON-R2; ADR-0017
// sections 1 and 3), AC1 and AC3, StealthGame.Concealment.OwnerAndTeamFlags:
//   - the commit sets the owner flag and the team flag once each through the flag sink (their only writers), the crossing
//     clears both, and an unbound (AI) pawn calls neither;
//   - UConcealmentOwnerViewComponent replicates exactly one property of its own, bIsConcealed, COND_OwnerOnly and
//     push-based, with a change-only writer; no BushId is replicated anywhere in Concealment's components;
//   - the IVP allowlist has exactly the one owner-only row for the component (audit passes; controls fail without the row
//     and with a wrong condition). The Roster half (teammates only) is StealthGame.Roster.TeammateConcealedTeamOnly; the
//     PIE rejoin (AC2) is StealthGame.Concealment.PIE.RejoinConcealed.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ConcealmentTestRig.h"
#include "Net/UnrealNetwork.h"
#include "Server/Testing/StealthReplicationAllowlist.h"
#include "View/ConcealmentComponent.h"
#include "View/ConcealmentOwnerViewComponent.h"

using namespace StealthConcealmentTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentOwnerAndTeamFlagsTest, "StealthGame.Concealment.OwnerAndTeamFlags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentOwnerAndTeamFlagsTest::RunTest(const FString& Parameters)
{
	// The flag writes of one enter-and-exit, player and NPC.
	{
		FRig Rig(TEXT("ConcealmentFlags"));
		AStealthPawn* Player = Rig.SpawnAtReach(30.0, 20.0);
		AStealthPawn* Npc = Rig.SpawnAtReach(30.0, -20.0);
		Rig.Relations.Bind(Player, 12, 0);
		Rig.EnterAndSettle(Npc);
		TestEqual(TEXT("an unbound (AI) pawn calls no setter"), Rig.Flags.Writes, 0);
		Rig.SubmitHide(Player, ECommandKind::HideEnter, 1);
		Rig.Tick();
		TestEqual(TEXT("nothing before the commit"), Rig.Flags.Writes, 0);
		Rig.Run(Rig.Params.Public.BushEnterDurSec + 0.05);
		TestTrue(TEXT("the commit sets both flags"), Rig.Flags.OwnerOf(12) && Rig.Flags.TeamOf(12));
		TestEqual(TEXT("... once each"), Rig.Flags.Writes, 2);
		Rig.SubmitHide(Player, ECommandKind::HideExit, 0);
		Rig.Tick();
		TestTrue(TEXT("exit execution keeps them (until the crossing)"), Rig.Flags.OwnerOf(12) && Rig.Flags.TeamOf(12));
		Rig.Run(Rig.Params.Public.BushExitCrossSec);
		TestTrue(TEXT("the crossing clears both"), !Rig.Flags.OwnerOf(12) && !Rig.Flags.TeamOf(12));
		TestEqual(TEXT("... once each"), Rig.Flags.Writes, 4);
	}

	// The owner component: one replicated property, owner only, push-based, change-only writes.
	TArray<FLifetimeProperty> Lifetime;
	GetDefault<UConcealmentOwnerViewComponent>()->GetLifetimeReplicatedProps(Lifetime);
	const FProperty* Flag = UConcealmentOwnerViewComponent::StaticClass()->FindPropertyByName(TEXT("bIsConcealed"));
	if (!TestNotNull(TEXT("bIsConcealed exists"), Flag))
	{
		return false;
	}
	const FLifetimeProperty* Registered = Lifetime.FindByPredicate([Flag](const FLifetimeProperty& L) { return L.RepIndex == Flag->RepIndex; });
	TestTrue(TEXT("bIsConcealed replicates COND_OwnerOnly"), Registered != nullptr && Registered->Condition == COND_OwnerOnly);
	TestTrue(TEXT("... push-based"), Registered != nullptr && Registered->bIsPushBased);
	int32 Own = 0;
	for (TFieldIterator<FProperty> It(UConcealmentOwnerViewComponent::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		Own += It->HasAnyPropertyFlags(CPF_Net) ? 1 : 0;
		TestFalse(FString::Printf(TEXT("%s is not a BushId"), *It->GetName()), It->GetName().Contains(TEXT("Bush")));
	}
	TestEqual(TEXT("exactly one replicated property of its own"), Own, 1);
	UConcealmentOwnerViewComponent* Component = NewObject<UConcealmentOwnerViewComponent>(GetTransientPackage());
	TestTrue(TEXT("a change writes"), Component->SetConcealed(true));
	TestFalse(TEXT("the same value does not"), Component->SetConcealed(true));
	TestTrue(TEXT("the clear writes"), Component->SetConcealed(false));
	TestEqual(TEXT("two writes"), Component->GetServerWriteCount(), 2);
	// No Concealment pawn component replicates anything (BushId never leaves the server).
	int32 PawnNet = 0;
	for (TFieldIterator<FProperty> It(UConcealmentComponent::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		PawnNet += It->HasAnyPropertyFlags(CPF_Net) ? 1 : 0;
	}
	TestEqual(TEXT("UConcealmentComponent replicates nothing"), PawnNet, 0);

	// The allowlist row and its audit.
	TArray<FReplicationAllowlistRow> Rows;
	FString Error;
	if (!TestTrue(TEXT("the allowlist parses"), StealthReplicationAllowlist::Load(Rows, Error)))
	{
		AddError(Error);
		return false;
	}
	const TCHAR* const ClassName = TEXT("UConcealmentOwnerViewComponent");
	const TArray<FReplicationAllowlistRow> Ours = Rows.FilterByPredicate([ClassName](const FReplicationAllowlistRow& R) { return R.Class == ClassName; });
	if (TestEqual(TEXT("exactly one allowlist row for the component"), Ours.Num(), 1))
	{
		TestEqual(TEXT("audience Owner"), Ours[0].Audience, FString(TEXT("Owner")));
		TestEqual(TEXT("condition COND_OwnerOnly"), Ours[0].Condition, FString(TEXT("COND_OwnerOnly")));
		TestTrue(TEXT("the IVP R3 IsConcealed row"), Ours[0].PolicyRows.Contains(TEXT("IsConcealed")));
	}
	TArray<FString> Findings;
	StealthReplicationAllowlist::AuditClass(UConcealmentOwnerViewComponent::StaticClass(), Rows, Findings);
	TestEqual(TEXT("the reflection audit passes"), Findings.Num(), 0);
	for (const FString& Finding : Findings)
	{
		AddError(Finding);
	}
	{
		const TArray<FReplicationAllowlistRow> Removed = Rows.FilterByPredicate([ClassName](const FReplicationAllowlistRow& R) { return R.Class != ClassName; });
		TArray<FString> Out;
		StealthReplicationAllowlist::AuditClass(UConcealmentOwnerViewComponent::StaticClass(), Removed, Out);
		TestTrue(TEXT("control: the audit fails without the row"), Out.Num() > 0);
	}
	{
		TArray<FReplicationAllowlistRow> Wrong = Rows;
		for (FReplicationAllowlistRow& R : Wrong)
		{
			if (R.Class == ClassName)
			{
				R.Condition = TEXT("COND_None");
			}
		}
		TArray<FString> Out;
		StealthReplicationAllowlist::AuditClass(UConcealmentOwnerViewComponent::StaticClass(), Wrong, Out);
		TestTrue(TEXT("control: the audit fails on a wrong condition"), Out.Num() > 0);
	}
	return true;
}

#endif
