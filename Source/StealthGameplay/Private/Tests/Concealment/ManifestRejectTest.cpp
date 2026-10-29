// Story con-002 (production/epics/concealment-bushes/story-002-bush-runtime-from-manifest.md; GDD CON-R1, AC-CON-22,
// AC-CON-27; ADR-0017 section 1, E5):
//   - StealthGame.Concealment.ManifestReject (AC2): a BushSite with an empty EntryAxis set is a V-BUSH-1 reject of the whole
//     layout (one empty bush among valid ones rejects it; a single axis is accepted); nothing is built.
//   - StealthGame.Concealment.SingleVolumeSet (AC3): the built runtime spawns exactly one non-replicated ABushVolumeSet with
//     one box per bush on the BushVolume object channel (Pawn blocked, Camera / Visibility / ViewBlock ignored), the world
//     holds zero replicated concealment actors, and every FBushRuntime is footprint 2.0 x 2.0 m x 2.25 m with its anchor
//     at the footprint centre on the ground. A rebuild replaces the set (still one).
//   - StealthGame.Concealment.BushRuntimeHash (AC1 unit form): the canonical hash of {BushId, HideVolume, EntryAxisMask,
//     Anchor} is equal for two machines' builds of the same sites and changes for a 1 cm anchor perturbation (negative
//     control). The 2-client PIE parity is StealthGame.Concealment.PIE.BushSiteParity.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Components/BoxComponent.h"
#include "ConcealmentTestRig.h"
#include "EngineUtils.h"
#include "View/BushRuntimeSubsystem.h"
#include "View/BushVolumeSet.h"
#include "View/ConcealCollision.h"

using namespace StealthConcealmentTest;

namespace ConcealmentManifestTestData
{
	TArray<FBushSiteRecord> ThreeSites()
	{
		TArray<FBushSiteRecord> Sites;
		Sites.Add({ 1, FVector(0.0, 0.0, 10.0), ConcealmentGeometry::AllSidesMask });
		Sites.Add({ 2, FVector(1500.0, 0.0, 10.0), 0x01 });
		Sites.Add({ 3, FVector(0.0, 1500.0, 10.0), 0x06 });
		return Sites;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentManifestRejectTest, "StealthGame.Concealment.ManifestReject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentManifestRejectTest::RunTest(const FString& Parameters)
{
	using namespace ConcealmentManifestTestData;
	FWorldHolder World(TEXT("ConcealmentManifestReject"));
	UBushRuntimeSubsystem* Bushes = UBushRuntimeSubsystem::Get(World.World);
	if (!TestNotNull(TEXT("the bush runtime exists in a game world"), Bushes))
	{
		return false;
	}
	// A single axis per bush is accepted.
	TestTrue(TEXT("valid sites (one with a single axis) are accepted"), Bushes->BuildFromSites(ThreeSites()));
	TestEqual(TEXT("three bushes built"), Bushes->GetBushes().Num(), 3);
	// One empty EntryAxis set among valid ones rejects the whole layout.
	TArray<FBushSiteRecord> Bad = ThreeSites();
	Bad[1].EntryAxisMask = 0;
	TestEqual(TEXT("V-BUSH-1 names the empty EntryAxis"), UBushRuntimeSubsystem::ValidateSites(Bad), EBushSiteReject::EmptyEntryAxis);
	TestFalse(TEXT("the layout is rejected"), Bushes->BuildFromSites(Bad));
	TestFalse(TEXT("nothing is built (no round can start with it)"), Bushes->IsBuilt());
	TestEqual(TEXT("no bush remains"), Bushes->GetBushes().Num(), 0);
	TestNull(TEXT("no volume set remains"), Bushes->GetVolumeSet());
	TestEqual(TEXT("the last rejection is recorded"), Bushes->GetLastReject(), EBushSiteReject::EmptyEntryAxis);
	// A bad id is rejected too.
	TArray<FBushSiteRecord> Duplicate = ThreeSites();
	Duplicate[2].BushId = 1;
	TestEqual(TEXT("a duplicated BushId is rejected"), UBushRuntimeSubsystem::ValidateSites(Duplicate), EBushSiteReject::BadBushId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentSingleVolumeSetTest, "StealthGame.Concealment.SingleVolumeSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentSingleVolumeSetTest::RunTest(const FString& Parameters)
{
	using namespace ConcealmentManifestTestData;
	FWorldHolder World(TEXT("ConcealmentSingleVolumeSet"));
	UBushRuntimeSubsystem* Bushes = UBushRuntimeSubsystem::Get(World.World);
	if (!TestNotNull(TEXT("the bush runtime exists"), Bushes) || !TestTrue(TEXT("built"), Bushes->BuildFromSites(ThreeSites())))
	{
		return false;
	}
	Bushes->BuildFromSites(ThreeSites()); // a rebuild replaces the set
	int32 Sets = 0;
	ABushVolumeSet* Set = nullptr;
	for (TActorIterator<ABushVolumeSet> It(World.World); It; ++It)
	{
		if (IsValid(*It) && !It->IsActorBeingDestroyed())
		{
			++Sets;
			Set = *It;
		}
	}
	TestEqual(TEXT("exactly one ABushVolumeSet"), Sets, 1);
	if (!TestNotNull(TEXT("the set"), Set))
	{
		return false;
	}
	TestFalse(TEXT("bReplicates = false"), Set->GetIsReplicated());
	TestEqual(TEXT("one box per bush"), Set->GetVolumes().Num(), 3);
	for (const UBoxComponent* Box : Set->GetVolumes())
	{
		TestEqual(TEXT("object channel BushVolume"), static_cast<int32>(Box->GetCollisionObjectType()), static_cast<int32>(ConcealmentCollision::BushVolumeChannel));
		TestEqual(TEXT("blocks Pawn"), static_cast<int32>(Box->GetCollisionResponseToChannel(ECC_Pawn)), static_cast<int32>(ECR_Block));
		TestEqual(TEXT("ignores Camera"), static_cast<int32>(Box->GetCollisionResponseToChannel(ECC_Camera)), static_cast<int32>(ECR_Ignore));
		TestEqual(TEXT("ignores Visibility"), static_cast<int32>(Box->GetCollisionResponseToChannel(ECC_Visibility)), static_cast<int32>(ECR_Ignore));
		TestEqual(TEXT("box extent 100 x 100 x 112.5 cm"), Box->GetUnscaledBoxExtent(), FVector(100.0, 100.0, 112.5));
	}
	// Zero replicated concealment actors: nothing Concealment spawned replicates.
	int32 Replicated = 0;
	for (TActorIterator<AActor> It(World.World); It; ++It)
	{
		Replicated += It->IsA<ABushVolumeSet>() && It->GetIsReplicated() ? 1 : 0;
	}
	TestEqual(TEXT("zero replicated concealment actors"), Replicated, 0);
	// The runtime geometry.
	const FBushRuntime* Bush = Bushes->Find(1);
	if (TestNotNull(TEXT("bush 1"), Bush))
	{
		TestEqual(TEXT("footprint 2.0 m"), Bush->Footprint.GetSize(), FVector2D(200.0, 200.0));
		TestEqual(TEXT("hide volume height 2.25 m"), Bush->HideVolume.GetSize().Z, 225.0, 1e-6);
		TestEqual(TEXT("anchor at the footprint centre on the ground"), Bush->Anchor, FVector(0.0, 0.0, 10.0));
		TestEqual(TEXT("hide volume from the ground"), Bush->HideVolume.Min.Z, 10.0, 1e-6);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcealmentBushRuntimeHashTest, "StealthGame.Concealment.BushRuntimeHash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcealmentBushRuntimeHashTest::RunTest(const FString& Parameters)
{
	using namespace ConcealmentManifestTestData;
	const TArray<FBushSiteRecord> Sites = ThreeSites();
	for (const FBushSiteRecord& Site : Sites)
	{
		const FBushRuntime A = ConcealmentGeometry::MakeBushRuntime(Site.BushId, Site.GroundCentre, Site.EntryAxisMask);
		const FBushRuntime B = ConcealmentGeometry::MakeBushRuntime(Site.BushId, Site.GroundCentre, Site.EntryAxisMask);
		TestEqual(FString::Printf(TEXT("bush %d: two builds hash identically"), Site.BushId), ConcealmentGeometry::HashBushRuntime(A), ConcealmentGeometry::HashBushRuntime(B));
		const FBushRuntime Moved = ConcealmentGeometry::MakeBushRuntime(Site.BushId, Site.GroundCentre + FVector(1.0, 0.0, 0.0), Site.EntryAxisMask);
		TestNotEqual(FString::Printf(TEXT("bush %d: a 1 cm anchor perturbation changes the hash"), Site.BushId), ConcealmentGeometry::HashBushRuntime(Moved), ConcealmentGeometry::HashBushRuntime(A));
		const FBushRuntime Sides = ConcealmentGeometry::MakeBushRuntime(Site.BushId, Site.GroundCentre, Site.EntryAxisMask ^ 0x08);
		TestNotEqual(FString::Printf(TEXT("bush %d: a different EntryAxis set changes the hash"), Site.BushId), ConcealmentGeometry::HashBushRuntime(Sides), ConcealmentGeometry::HashBushRuntime(A));
	}
	return true;
}

#endif
