// Story oc-010 (production/epics/objectives-chain/story-010-team-view-struct-and-builder.md), AC3 (ADR-0020 sections 4 and 6):
// StealthGame.Objectives.TeamViewWidthAndChangeOnly.
//   Width     FObjectiveTeamView::NetSerialize on an FNetBitWriter writes exactly WireBits(n) bits and at most 12 + 4 x n bytes for
//             n = 0, 1, 2 and the maximum (3) live sites, with every field at its widest value; the round trip through an
//             FNetBitReader reproduces the value (DefIndex, the row index, travels: no FName).
//   Shape     the view, its live site and the clue have no FString, FText or FName member at any depth (negative control: the
//             same check reports the seeded FString card def), and DefIndex is a byte.
//   Writes    with two Active members and one Informant of team 0 (and one member of team 1): Active entry writes each team-0
//             component once and all three hold the same view; an identical second push is a no-op (core and component);
//             a changed view (a counted stage) is written once more to each; RoundEpoch and ChainSerial are set;
//             CompletedDefMask never includes the final, also after the win.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "ObjectivesQueueRig.h"
#include "ObjectivesTestTypes.h"
#include "UObject/CoreNet.h"
#include "UObject/UnrealType.h"

namespace ObjectivesTeamViewWidthTestData
{
	/** A view with N sites and every field at its widest value. */
	FObjectiveTeamView Widest(int32 N)
	{
		FObjectiveTeamView View;
		View.RoundEpoch = 0xFFFFFFFFu;
		View.ChainSerial = 0xFFFF;
		View.StageIndex = EObjectiveTeamStage::Final;
		View.CompletedDefMask = 0x7FFF;
		View.RivalPrelimCount = 2;
		for (int32 Index = 0; Index < N; ++Index)
		{
			FObjectiveLiveSite& Site = View.LiveSites.AddDefaulted_GetRef();
			Site.Court = FCourtFixtureId(static_cast<uint16>(0xFFFF - Index));
			Site.DefIndex = static_cast<uint8>(15 - Index);
			Site.Clue.AreaType = 0xFF;
			Site.Clue.FeatureKind = static_cast<uint8>(0xF0 + Index);
		}
		return View;
	}

	/** Names of FString / FText / FName properties of Struct at any depth. */
	void FindStringMembers(const UStruct* Struct, TArray<FString>& Out)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FProperty* Property = *It;
			if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
			{
				Property = Array->Inner;
			}
			if (Property->IsA<FStrProperty>() || Property->IsA<FTextProperty>() || Property->IsA<FNameProperty>())
			{
				Out.Add(It->GetName());
			}
			if (const FStructProperty* AsStruct = CastField<FStructProperty>(Property))
			{
				FindStringMembers(AsStruct->Struct, Out);
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesTeamViewWidthAndChangeOnlyTest, "StealthGame.Objectives.TeamViewWidthAndChangeOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesTeamViewWidthAndChangeOnlyTest::RunTest(const FString& Parameters)
{
	using namespace StealthObjectivesTest;
	using namespace ObjectivesTeamViewWidthTestData;

	// --- Width (n = 0, 1, 2 and the maximum) ---------------------------------------------------------------------------
	for (int32 N = 0; N <= FObjectiveTeamView::MaxLiveSites; ++N)
	{
		FObjectiveTeamView Out = Widest(N);
		FNetBitWriter Writer(nullptr, 1024);
		bool bSuccess = false;
		TestTrue(FString::Printf(TEXT("n=%d: NetSerialize returns true"), N), Out.NetSerialize(Writer, nullptr, bSuccess) && bSuccess);
		const int64 Bits = Writer.GetNumBits();
		const int64 Bytes = (Bits + 7) / 8;
		AddInfo(FString::Printf(TEXT("OC010 width n=%d: %lld bits, %lld bytes (budget %d)"), N, Bits, Bytes, FObjectiveTeamView::BudgetBytes(N)));
		TestEqual(FString::Printf(TEXT("n=%d: exactly WireBits(n)"), N), Bits, static_cast<int64>(FObjectiveTeamView::WireBits(N)));
		TestTrue(FString::Printf(TEXT("n=%d: at most 12 + 4n bytes"), N), Bytes <= FObjectiveTeamView::BudgetBytes(N));

		FNetBitReader Reader(nullptr, Writer.GetData(), Writer.GetNumBits());
		FObjectiveTeamView In;
		bool bReadSuccess = false;
		In.NetSerialize(Reader, nullptr, bReadSuccess);
		TestTrue(FString::Printf(TEXT("n=%d: the round trip reproduces the value"), N), bReadSuccess && !Reader.IsError() && In == Out);
	}
	// The default (empty) view round-trips too, and DefIndex 0xFF (none) survives the 5-bit field.
	{
		FObjectiveTeamView Out;
		Out.LiveSites.AddDefaulted();
		FNetBitWriter Writer(nullptr, 256);
		bool bSuccess = false;
		Out.NetSerialize(Writer, nullptr, bSuccess);
		FNetBitReader Reader(nullptr, Writer.GetData(), Writer.GetNumBits());
		FObjectiveTeamView In;
		In.NetSerialize(Reader, nullptr, bSuccess);
		TestTrue(TEXT("DefIndex none (0xFF) round-trips"), In == Out && In.LiveSites[0].DefIndex == 0xFF);
	}

	// --- Shape: no string members; DefIndex is a byte --------------------------------------------------------------------
	TArray<FString> Strings;
	FindStringMembers(FObjectiveTeamView::StaticStruct(), Strings);
	TestEqual(TEXT("the view has no FString / FText / FName member"), Strings.Num(), 0);
	TArray<FString> Seeded;
	FindStringMembers(FObjectivesSeededStringCardDef::StaticStruct(), Seeded);
	TestEqual(TEXT("negative control: the seeded FString card def is reported"), Seeded.Num(), 1);
	const FProperty* DefIndex = FObjectiveLiveSite::StaticStruct()->FindPropertyByName(TEXT("DefIndex"));
	TestTrue(TEXT("DefIndex travels as a byte (the row index, not an FName)"), DefIndex != nullptr && DefIndex->IsA<FByteProperty>());

	// --- Writes: change-only, the same view for every member -----------------------------------------------------------
	FQueueRig Rig;
	const uint32 Active1 = 1, Active2 = 2, Informant = 3, Opponent = 4;
	for (const uint32 Id : { Active1, Active2, Informant })
	{
		Rig.ViewTargets.Add(Id, 0);
		Rig.Roster.Teams.Add(Id, 0);
	}
	Rig.ViewTargets.Add(Opponent, 1);
	Rig.Roster.Teams.Add(Opponent, 1);
	Rig.Roster.States.Add(Active1, EParticipantState::Active);
	Rig.Roster.States.Add(Active2, EParticipantState::Active);
	Rig.Roster.States.Add(Informant, EParticipantState::Informant);
	Rig.Roster.States.Add(Opponent, EParticipantState::Active);
	TestTrue(TEXT("to Active"), Rig.ToActive());

	const FObjectiveTeamView Team0 = Rig.ViewTargets.ViewOf(Active1)->GetView();
	for (const uint32 Id : { Active1, Active2, Informant })
	{
		TestEqual(FString::Printf(TEXT("member %u written once at Active entry"), Id), Rig.ViewTargets.ViewOf(Id)->GetServerWriteCount(), 1);
		TestTrue(FString::Printf(TEXT("member %u (Active or Informant) holds the same view"), Id), Rig.ViewTargets.ViewOf(Id)->GetView() == Team0);
	}
	TestTrue(TEXT("RoundEpoch set"), Team0.RoundEpoch == Rig.Q.Epoch.Current.RoundEpoch && Team0.RoundEpoch != 0);
	TestEqual(TEXT("ChainSerial set"), static_cast<int32>(Team0.ChainSerial), static_cast<int32>(StealthObjectivesCacaoFixture::ChainSerial));

	// An identical second write is a no-op: the core pushes nothing, the component writes nothing.
	FObjectivesCore& Core = const_cast<FObjectivesCore&>(Rig.Core());
	const int32 PushesBefore = Rig.Views.Pushes.Num();
	Core.PushView(0);
	TestEqual(TEXT("core: an identical push is a no-op"), Rig.Views.Pushes.Num(), PushesBefore);
	TestEqual(TEXT("service: an identical write changes no component"), Rig.Service->WriteTeamView(0, Team0), 0);
	TestFalse(TEXT("component: ApplyView of the same value returns false"), Rig.ViewTargets.ViewOf(Active2)->ApplyView(Team0));
	TestEqual(TEXT("component: still written once"), Rig.ViewTargets.ViewOf(Active2)->GetServerWriteCount(), 1);

	// A changed view (team 0 banks Prelim1) is written once more to each team-0 member, never to the opponent.
	Rig.CompleteStageFast(0);
	for (const uint32 Id : { Active1, Active2, Informant })
	{
		TestEqual(FString::Printf(TEXT("member %u written once more after the stage"), Id), Rig.ViewTargets.ViewOf(Id)->GetServerWriteCount(), 2);
	}
	TestEqual(TEXT("the opponent's component is untouched"), Rig.ViewTargets.ViewOf(Opponent)->GetServerWriteCount(), 1);

	// CompletedDefMask never includes the final, also after the win.
	Rig.CompleteStageFast(0); // Prelim2: the final is live for team 0
	TestTrue(TEXT("team 0's view holds the final once its prelims are done"), Rig.ViewTargets.ViewOf(Active1)->GetView().FindLiveSite(FCourtFixtureId(CourtFinal)) != nullptr);
	Rig.CompleteStageFast(0); // the final: the win
	TestEqual(TEXT("the final declared the win"), Rig.Round.Wins.Num(), 1);
	const uint16 FinalBit = static_cast<uint16>(1u << DefFinal);
	TestEqual(TEXT("built view: the final's mask bit stays 0 after the win"), static_cast<int32>(Rig.Core().BuildTeamView(0).CompletedDefMask & FinalBit), 0);
	TestEqual(TEXT("written view: the final's mask bit stays 0 after the win"), static_cast<int32>(Rig.ViewTargets.ViewOf(Active1)->GetView().CompletedDefMask & FinalBit), 0);
	TestEqual(TEXT("written view: both prelims in the mask"), static_cast<int32>(Rig.ViewTargets.ViewOf(Active1)->GetView().CompletedDefMask), (1 << DefP1Team0) | (1 << DefP2Team0));
	return true;
}

#endif
