// Story oc-003 (production/epics/objectives-chain/story-003-core-chain-validation-and-court-binding.md), AC3 / AC-OC-25
// AssignmentServerOnly, three mechanical checks:
//   (1) reflection: no replicated property and no RPC parameter of any class in the runtime packages has a type that carries
//       the site assignment (FSiteAssignment is a plain C++ struct, so it cannot be reflected at all; the scan also rejects
//       any reflected struct named for it). A seeded name ("FSiteAssignmentMirror") is flagged by the same predicate.
//   (2) a recording Event Log sink sees nothing written during Bind and Active entry (Objectives keeps no header entry; the
//       only site data in the log is Round's server-only header, which Objectives never writes); a seeded log call that
//       carries a court id is caught by the same entry check.
//   (3) source scan: GetSiteAssignment_Server is named only by Objectives' seam declaration, FObjectivesCore's Bind and the
//       composition root's layout stand-in (the LSC side of the query); no client-module source names FSiteAssignment; the
//       bind spawns no actor (world spawn count 0). The full include lint is ObjectivesIncludeLint (story oc-022).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "ObjectivesSourceScan.h"
#include "ObjectivesTestRig.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace ObjectivesAssignmentServerOnlyTestData
{
	/** The reflection predicate: a struct type that carries the site assignment. */
	bool CarriesAssignment(const FProperty* Property)
	{
		const FStructProperty* Struct = CastField<FStructProperty>(Property);
		return Struct != nullptr && Struct->Struct != nullptr && Struct->Struct->GetName().Contains(TEXT("SiteAssignment"));
	}

	/** "Class.Member" of every replicated property or RPC parameter that carries the assignment, over the runtime classes. */
	TArray<FString> ScanReplicated()
	{
		TArray<FString> Out;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			const FString Package = It->GetOutermost()->GetName();
			if (!Package.StartsWith(TEXT("/Script/Stealth")))
			{
				continue;
			}
			for (TFieldIterator<FProperty> Prop(*It, EFieldIteratorFlags::ExcludeSuper); Prop; ++Prop)
			{
				if (Prop->HasAnyPropertyFlags(CPF_Net) && CarriesAssignment(*Prop))
				{
					Out.Add(FString::Printf(TEXT("%s.%s"), *It->GetName(), *Prop->GetName()));
				}
			}
			for (TFieldIterator<UFunction> Func(*It, EFieldIteratorFlags::ExcludeSuper); Func; ++Func)
			{
				if (!Func->HasAnyFunctionFlags(FUNC_Net))
				{
					continue;
				}
				for (TFieldIterator<FProperty> Param(*Func); Param; ++Param)
				{
					if (CarriesAssignment(*Param))
					{
						Out.Add(FString::Printf(TEXT("%s.%s(%s)"), *It->GetName(), *Func->GetName(), *Param->GetName()));
					}
				}
			}
		}
		return Out;
	}

	/** An entry that carries a court id (the only Objectives entries that may are TendCompleted, which is never written at bind). */
	bool CarriesSiteData(const FRoundLogEntry& Entry)
	{
		const uint32 Courts[] = { StealthObjectivesTest::CourtP1Team0, StealthObjectivesTest::CourtP1Team1, StealthObjectivesTest::CourtP2Team0,
			StealthObjectivesTest::CourtP2Team1, StealthObjectivesTest::CourtFinal };
		for (const uint32 Data : Entry.Data)
		{
			for (const uint32 Court : Courts)
			{
				if (Data == Court)
				{
					return true;
				}
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectivesAssignmentServerOnlyTest, "StealthGame.Objectives.AssignmentServerOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectivesAssignmentServerOnlyTest::RunTest(const FString& Parameters)
{
	using namespace ObjectivesAssignmentServerOnlyTestData;
	using namespace StealthObjectivesTest;

	// (1) Reflection.
	const TArray<FString> Replicated = ScanReplicated();
	TestEqual(TEXT("no replicated property or RPC parameter carries the site assignment"), Replicated.Num(), 0);
	for (const FString& Hit : Replicated)
	{
		AddError(Hit);
	}
	TestTrue(TEXT("seeded: a struct named FSiteAssignmentMirror is flagged by the same name rule"), FString(TEXT("SiteAssignmentMirror")).Contains(TEXT("SiteAssignment")));

	// (2) The Event Log sink during Bind and Active entry; (3) no actor spawned.
	{
		StealthAssassinationTest::FScopedWorld World(TEXT("ObjectivesAssignmentServerOnly"));
		int32 Spawned = 0;
		const FDelegateHandle Handle = World.World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([&Spawned](AActor*) { ++Spawned; }));
		FCoreRig Rig(/*bActive*/ true);
		World.World->RemoveOnActorSpawnedHandler(Handle);
		TestEqual(TEXT("the assignment is read once, through the query"), Rig.Assignment.Reads, 1);
		TestEqual(TEXT("nothing is logged during Bind and Active entry"), Rig.Round.Entries.Num(), 0);
		TestEqual(TEXT("no entry carries site data"), Rig.Round.Entries.FilterByPredicate([](const FRoundLogEntry& E) { return CarriesSiteData(E); }).Num(), 0);
		TestEqual(TEXT("no actor is spawned for a court, objective or site"), Spawned, 0);

		FRoundLogEntry Seeded;
		Seeded.Type = ERoundLogType::ObjectiveStage;
		Seeded.Data[4] = CourtFinal;
		TestTrue(TEXT("seeded: a log call carrying a court id is caught by the entry check"), CarriesSiteData(Seeded));
	}

	// (3) Source scans.
	const TArray<FString> Query = { TEXT("GetSiteAssignment_Server") };
	TArray<StealthObjectivesSourceScan::FSource> Sources;
	for (const TCHAR* Module : { TEXT("StealthFoundation"), TEXT("StealthGameplay"), TEXT("StealthPresentation"), TEXT("StealthGame") })
	{
		for (StealthObjectivesSourceScan::FSource& Source : StealthObjectivesSourceScan::LoadSources(Module))
		{
			if (!Source.Path.Contains(TEXT("/Tests/")))
			{
				Sources.Add(MoveTemp(Source));
			}
		}
	}
	const TArray<FString> Allowed = {
		TEXT("StealthGameplay/Features/Objectives/Public/Server/ObjectivesSeams.h: GetSiteAssignment_Server"),
		TEXT("StealthGameplay/Features/Objectives/Private/ObjectivesCore.cpp: GetSiteAssignment_Server"),
		TEXT("StealthGame/Private/Composition/StealthObjectivesWiring.h: GetSiteAssignment_Server"),
		TEXT("StealthGame/Private/Composition/StealthObjectivesWiring.cpp: GetSiteAssignment_Server"),
	};
	for (const FString& Finding : StealthObjectivesSourceScan::FindNames(Sources, Query))
	{
		TestTrue(FString::Printf(TEXT("only Objectives and the layout stand-in name the query: %s"), *Finding), Allowed.Contains(Finding));
	}
	const TArray<FString> ClientHits = StealthObjectivesSourceScan::FindNames(StealthObjectivesSourceScan::LoadClientSources(), { TEXT("FSiteAssignment") });
	TestEqual(TEXT("no client-module source names FSiteAssignment"), ClientHits.Num(), 0);
	for (const FString& Hit : ClientHits)
	{
		AddError(Hit);
	}
	StealthObjectivesSourceScan::FSource SeededClient;
	SeededClient.Path = TEXT("StealthPresentation/Private/Seeded.cpp");
	SeededClient.Text = TEXT("void F(const FSiteAssignment& A);");
	TestEqual(TEXT("seeded: a client source naming FSiteAssignment is reported"), StealthObjectivesSourceScan::FindNames({ SeededClient }, { TEXT("FSiteAssignment") }).Num(), 1);
	return true;
}

#endif
