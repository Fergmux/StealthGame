// Story nkp-016 (production/epics/npc-kill-penalty-informants/story-016-strike-count-never-leaves-and-removal-audiences.md),
// AC1 / AC-NKP-04 part (a) (PEN-R2, CR-1; ADR-0019 sections 1-2; ADR-0001 owner-only carriers):
// StealthGame.Penalty.StrikeCountNeverLeaves.
//
// Two halves:
//  1. Reflection over every UClass and UScriptStruct of the four project runtime packages (/Script/StealthFoundation,
//     StealthGameplay, StealthPresentation, StealthGame): every replicated property (CPF_Net) and every RPC parameter, walked
//     recursively through struct and array members, is checked by name for the strike count (StrikeCount, NpcKill,
//     StrikeAfter); every USTRUCT field anywhere in those packages is checked the same way. The fields whose name carries a
//     tally are exactly FStrikeResult.TallyAfter and FTeamViewOwnerPart.WrongTargetTally, and every root that reaches one
//     is owner-only (a Client RPC of a PlayerController, or a COND_OwnerOnly property). Negative control: the same walker,
//     seeded with "TallyAfter" as a forbidden name, flags the ClientCommandReplies parameter (the walk reaches a nested
//     struct member of an RPC parameter).
//  2. A live probe on the real FPenaltyCore: StrikeCount is set to the sentinel 7, then (a fresh core) to 200, with the
//     same tally; the same informant kill, NPC kill and reclaim re-push run in each; the wire bytes of the resulting
//     FCommandReply (FStrikeResult, NetSerialize), of a DefenceResult reply and of the C-TeamView owner part built from the
//     re-push are identical across the two sentinels (a byte search for one uint8 would give false positives, hence the
//     two-sentinel comparison). Positive control: a tally one lower changes the bytes, so the comparison can see a change.
//
// The public carriers (PlayerState allowlist, FRoundPublicState, FDeathState) are covered by the reflection half: Penalty
// writes none of them (the Roster's and IVP's own audits own their contents, story Out of Scope).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/BitWriter.h"
#include "Serialization/MemoryWriter.h"
#include "Server/Testing/StealthNegativeControl.h"
#include "Tests/Penalty/PenaltyTestRig.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "View/CommandReplyTypes.h"
#include "View/StealthRosterTeamView.h"

namespace PenaltyStrikeCountNeverLeavesTestData
{
	/** The project's runtime script packages (ADR-0007 modules; the editor and harness modules never ship). */
	const TCHAR* const RuntimePackages[] = { TEXT("/Script/StealthFoundation"), TEXT("/Script/StealthGameplay"), TEXT("/Script/StealthPresentation"),
		TEXT("/Script/StealthGame") };

	/**
	 * Name fragments that would carry the true strike count (Penalty's k, the prototype's NpcKills counter, the Event Log
	 * row field). "NpcKills", not "NpcKill": the public tuning key NpcKillStrikeLimit (L) is public by design (PEN-R15).
	 */
	const TCHAR* const ForbiddenNames[] = { TEXT("StrikeCount"), TEXT("NpcKills"), TEXT("StrikeAfter") };

	/** The only tally fields any client may receive (ADR-0019 section 2.2a): "<Struct>.<Field>". */
	const TCHAR* const AllowedTallyFields[] = { TEXT("FStrikeResult.TallyAfter"), TEXT("FTeamViewOwnerPart.WrongTargetTally") };

	/** The two strike-count sentinels (no legal round reaches either: L is at most 5). */
	constexpr uint8 SentinelA = 7;
	constexpr uint8 SentinelB = 200;
	/** The probe's tally (k <= t holds for both sentinels, so no F1 invariant fault is raised). */
	constexpr uint8 ProbeTally = 250;
	/** The probe's fixed contact time (the same in both runs). */
	constexpr double ProbeContactServerTime = 123.5;

	bool IsRuntimePackage(const UObject* Object)
	{
		const FString Package = Object->GetOutermost()->GetName();
		for (const TCHAR* Name : RuntimePackages)
		{
			if (Package == Name)
			{
				return true;
			}
		}
		return false;
	}

	/** "FStrikeResult" / "UPenaltyService" / "AStealthPlayerController". */
	FString CppName(const UStruct* Struct)
	{
		if (const UClass* Class = Cast<UClass>(Struct))
		{
			return FString(Class->GetPrefixCPP()) + Class->GetName();
		}
		return FString(TEXT("F")) + Struct->GetName();
	}

	/**
	 * One client-bound root (a replicated property or an RPC). A replicated property's lifetime condition is read only for
	 * the roots that reach a tally field (ConditionOf calls the class default object's GetLifetimeReplicatedProps, which some
	 * engine classes assert on when called a second time).
	 */
	struct FRoot
	{
		FString Name;
		bool bOwnerOnly = false;
		const UClass* Class = nullptr;
		const FProperty* Property = nullptr;
	};

	struct FFinding
	{
		FString Root;
		FString Field;
	};

	/** The recursive walk of one root: every member reachable through struct and array members. */
	struct FWalker
	{
		TArrayView<const TCHAR* const> Forbidden;
		TArray<FFinding> ForbiddenHits;
		/** "<Struct>.<Field>" -> the roots that reach it. */
		TMap<FString, TArray<FRoot>> TallyFields;

		explicit FWalker(TArrayView<const TCHAR* const> InForbidden) : Forbidden(InForbidden) {}

		void Visit(const FProperty* Property, const FRoot& Root, int32 Depth = 0)
		{
			if (Property == nullptr || Depth > 8)
			{
				return;
			}
			const FString Name = Property->GetName();
			const UStruct* Owner = Property->GetOwnerStruct();
			const FString Qualified = (Owner != nullptr ? CppName(Owner) : FString(TEXT("?"))) + TEXT(".") + Name;
			for (const TCHAR* Fragment : Forbidden)
			{
				if (Name.Contains(Fragment, ESearchCase::CaseSensitive))
				{
					ForbiddenHits.Add({ Root.Name, Qualified });
				}
			}
			if (Name.Contains(TEXT("Tally"), ESearchCase::IgnoreCase))
			{
				TallyFields.FindOrAdd(Qualified).Add(Root);
			}
			if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
			{
				for (TFieldIterator<FProperty> It(Struct->Struct); It; ++It)
				{
					Visit(*It, Root, Depth + 1);
				}
			}
			else if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
			{
				Visit(Array->Inner, Root, Depth + 1);
			}
		}
	};

	/** The lifetime condition of a replicated property, read from the class default object. */
	ELifetimeCondition ConditionOf(const UClass* Class, const FProperty* Property, FString* OutTrace = nullptr)
	{
		TArray<FLifetimeProperty> Lifetime;
		Class->GetDefaultObject()->GetLifetimeReplicatedProps(Lifetime);
		// The lifetime entries index the class's replicated-property table: every super class's records first, then the
		// class's own replicated properties in declaration order (one record per array element).
		const UClass* OwnerClass = Property->GetOwnerClass() != nullptr ? Property->GetOwnerClass() : Class;
		int32 RepIndex = 0;
		if (const UClass* Super = OwnerClass->GetSuperClass())
		{
			TArray<FLifetimeProperty> SuperLifetime;
			Super->GetDefaultObject()->GetLifetimeReplicatedProps(SuperLifetime);
			for (const FLifetimeProperty& Entry : SuperLifetime)
			{
				RepIndex = FMath::Max(RepIndex, static_cast<int32>(Entry.RepIndex) + 1);
			}
		}
		for (TFieldIterator<FProperty> It(OwnerClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			if (*It == Property)
			{
				break;
			}
			RepIndex += It->HasAnyPropertyFlags(CPF_Net) ? It->ArrayDim : 0;
		}
		if (OutTrace != nullptr)
		{
			*OutTrace = FString::Printf(TEXT("replicated-property index %d, entries:"), RepIndex);
			for (const FLifetimeProperty& Entry : Lifetime)
			{
				*OutTrace += FString::Printf(TEXT(" [%d cond %d]"), static_cast<int32>(Entry.RepIndex), static_cast<int32>(Entry.Condition));
			}
		}
		for (const FLifetimeProperty& Entry : Lifetime)
		{
			if (Entry.RepIndex == RepIndex)
			{
				return Entry.Condition;
			}
		}
		return COND_Max;
	}

	/** Walks every client-bound root of the runtime packages. OutClassCount: classes per package found (a positive control). */
	void WalkRuntimeRoots(FWalker& Walker, TMap<FString, int32>& OutClassCount)
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			const UClass* Class = *It;
			if (!IsRuntimePackage(Class) || Class->HasAnyClassFlags(CLASS_NewerVersionExists))
			{
				continue;
			}
			OutClassCount.FindOrAdd(Class->GetOutermost()->GetName())++;
			for (TFieldIterator<FProperty> Prop(Class, EFieldIteratorFlags::ExcludeSuper); Prop; ++Prop)
			{
				if (!Prop->HasAnyPropertyFlags(CPF_Net))
				{
					continue;
				}
				FRoot Root{ FString::Printf(TEXT("%s::%s (replicated)"), *CppName(Class), *Prop->GetName()), false, Class, *Prop };
				Walker.Visit(*Prop, Root);
			}
			for (TFieldIterator<UFunction> Func(Class, EFieldIteratorFlags::ExcludeSuper); Func; ++Func)
			{
				if (!Func->HasAnyFunctionFlags(FUNC_Net))
				{
					continue;
				}
				const bool bOwnerOnly = Func->HasAnyFunctionFlags(FUNC_NetClient) && Class->IsChildOf(APlayerController::StaticClass());
				FRoot Root{ FString::Printf(TEXT("%s::%s() (RPC%s)"), *CppName(Class), *Func->GetName(), bOwnerOnly ? TEXT(", owner-only client") : TEXT("")), bOwnerOnly };
				for (TFieldIterator<FProperty> Param(*Func); Param; ++Param)
				{
					if (Param->HasAnyPropertyFlags(CPF_Parm) && !Param->HasAnyPropertyFlags(CPF_ReturnParm))
					{
						Walker.Visit(*Param, Root);
					}
				}
			}
		}
	}

	/** The wire bytes of one owner reply (FCommandReply::NetSerialize, exactly as ClientCommandReplies carries it). */
	TArray<uint8> ReplyBytes(const FCommandReply& Reply)
	{
		FCommandReply Copy = Reply;
		FBitWriter Writer(512, true);
		bool bOk = false;
		Copy.NetSerialize(Writer, nullptr, bOk);
		TArray<uint8> Out(Writer.GetData(), static_cast<int32>(Writer.GetNumBytes()));
		Out.Add(bOk ? 1 : 0);
		return Out;
	}

	/** The property bytes of the C-TeamView owner part (the default property serializer it uses until psr-037). */
	TArray<uint8> OwnerPartBytes(FTeamViewOwnerPart Part)
	{
		TArray<uint8> Out;
		FMemoryWriter Writer(Out);
		FTeamViewOwnerPart::StaticStruct()->SerializeBin(Writer, &Part);
		return Out;
	}

	/** Everything Penalty supplies to the owner in one probe, as wire bytes. */
	struct FProbeBytes
	{
		TArray<uint8> InformantKillReply;
		TArray<uint8> NpcKillReply;
		TArray<uint8> DefenceReply;
		TArray<uint8> OwnerPart;
		uint8 StrikeAfterProbe = 0;
	};

	FStrikeResult MakeResult(const FStrikeTally& Tally, uint32 Seq)
	{
		FStrikeResult Result;
		Result.ClientCommandSeq.Seq = Seq;
		Result.Result = EStrikeResult::WrongTarget;
		Result.TallyAfter = Tally.TallyAfter;
		Result.bExiled = Tally.bExiled;
		Result.ContactServerTime = ProbeContactServerTime;
		return Result;
	}

	/** One probe on a fresh core with k = Strikes and t = Tally: an informant kill, an NPC kill, then the reclaim re-push. */
	FProbeBytes Probe(uint8 Strikes, uint8 Tally)
	{
		using namespace StealthPenaltyTest;
		FRig Rig;
		Rig.Core->SetCountersForTest(P, Strikes, Tally);
		FProbeBytes Out;
		const FStrikeTally Informant = Rig.InformantKill(P);
		Out.InformantKillReply = ReplyBytes(FCommandReply::MakeStrikeResult(MakeResult(Informant, 1)));
		const FStrikeTally Npc = Rig.NpcKill(P);
		Out.NpcKillReply = ReplyBytes(FCommandReply::MakeStrikeResult(MakeResult(Npc, 2)));
		FClientCommandSeq DefenceSeq;
		DefenceSeq.Seq = 3;
		FDefenceResult Defence;
		Out.DefenceReply = ReplyBytes(FCommandReply::MakeDefenceResult(DefenceSeq, Defence));
		Rig.Core->OnParticipantReclaimed(P);
		FTeamViewOwnerPart Part;
		if (Rig.OwnerView.Calls.Num() > 0)
		{
			Part.WrongTargetTally = Rig.OwnerView.Calls.Last().Tally;
			Part.LastResultSeq = Rig.OwnerView.Calls.Last().LastResultSeq.Seq;
			Part.RoundEpoch = Rig.OwnerView.Calls.Last().Epoch;
		}
		Out.OwnerPart = OwnerPartBytes(Part);
		Out.StrikeAfterProbe = Rig.Core->GetStrikeCount(P);
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPenaltyStrikeCountNeverLeavesTest, "StealthGame.Penalty.StrikeCountNeverLeaves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPenaltyStrikeCountNeverLeavesTest::RunTest(const FString& Parameters)
{
	using namespace PenaltyStrikeCountNeverLeavesTestData;

	// ---- 1. Reflection -------------------------------------------------------------------------------------------
	FWalker Walker(ForbiddenNames);
	TMap<FString, int32> ClassCount;
	WalkRuntimeRoots(Walker, ClassCount);
	for (const TCHAR* Package : { TEXT("/Script/StealthFoundation"), TEXT("/Script/StealthGameplay"), TEXT("/Script/StealthGame") })
	{
		TestTrue(FString::Printf(TEXT("the scan saw the classes of %s (%d)"), Package, ClassCount.FindRef(Package)), ClassCount.FindRef(Package) > 0);
	}
	for (const FFinding& Hit : Walker.ForbiddenHits)
	{
		AddError(FString::Printf(TEXT("client-bound root %s carries %s (the strike count must never leave the server)"), *Hit.Root, *Hit.Field));
	}
	TestEqual(TEXT("no replicated property or RPC parameter carries the strike count"), Walker.ForbiddenHits.Num(), 0);

	// Every USTRUCT of the runtime packages: no field named for the strike count (any USTRUCT can become a carrier).
	int32 StructHits = 0;
	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		if (!IsRuntimePackage(*It))
		{
			continue;
		}
		for (TFieldIterator<FProperty> Field(*It, EFieldIteratorFlags::ExcludeSuper); Field; ++Field)
		{
			for (const TCHAR* Fragment : ForbiddenNames)
			{
				if (Field->GetName().Contains(Fragment, ESearchCase::CaseSensitive))
				{
					++StructHits;
					AddError(FString::Printf(TEXT("USTRUCT %s.%s is named for the strike count"), *CppName(*It), *Field->GetName()));
				}
			}
		}
	}
	TestEqual(TEXT("no USTRUCT field of the runtime packages is named for the strike count"), StructHits, 0);

	// Only the two owner tallies reach a client, both owner-only.
	TArray<FString> Found;
	Walker.TallyFields.GetKeys(Found);
	Found.Sort();
	TestEqual(TEXT("exactly two tally fields reach a client"), Found.Num(), static_cast<int32>(UE_ARRAY_COUNT(AllowedTallyFields)));
	for (const TCHAR* Allowed : AllowedTallyFields)
	{
		TestTrue(FString::Printf(TEXT("%s reaches a client (positive control: the walk follows nested struct members)"), Allowed), Walker.TallyFields.Contains(Allowed));
	}
	for (const TPair<FString, TArray<FRoot>>& Pair : Walker.TallyFields)
	{
		bool bAllowed = false;
		for (const TCHAR* Allowed : AllowedTallyFields)
		{
			bAllowed |= Pair.Key == Allowed;
		}
		TestTrue(FString::Printf(TEXT("tally field %s is one of the two allowed"), *Pair.Key), bAllowed);
		for (const FRoot& Root : Pair.Value)
		{
			bool bOwnerOnly = Root.bOwnerOnly;
			FString Condition;
			if (Root.Property != nullptr && Root.Class != nullptr)
			{
				FString Trace;
				const ELifetimeCondition Lifetime = ConditionOf(Root.Class, Root.Property, &Trace);
				AddInfo(FString::Printf(TEXT("NKP016 lifetime of %s: %s"), *Root.Name, *Trace));
				bOwnerOnly = Lifetime == COND_OwnerOnly;
				Condition = FString::Printf(TEXT(", condition %d"), static_cast<int32>(Lifetime));
			}
			AddInfo(FString::Printf(TEXT("NKP016 tally %s reaches the client through %s%s"), *Pair.Key, *Root.Name, *Condition));
			TestTrue(FString::Printf(TEXT("%s reaches the client only through an owner-only root (%s%s)"), *Pair.Key, *Root.Name, *Condition), bOwnerOnly);
		}
	}

	// Negative control: the same walker with a seeded forbidden name must flag the reply RPC's nested FStrikeResult member.
	FStealthNegativeControl Control(TEXT("SeededForbiddenTallyAfter"));
	FStealthNegativeControl::Report(*this, Control.Run([](FStealthCheckContext& Ctx)
	{
		const TCHAR* const Seeded[] = { TEXT("TallyAfter") };
		FWalker Seeding(Seeded);
		TMap<FString, int32> Ignored;
		WalkRuntimeRoots(Seeding, Ignored);
		Ctx.TestTrue(TEXT("no root carries the seeded name"), Seeding.ForbiddenHits.IsEmpty());
	}));

	// ---- 2. Live sentinel probe ----------------------------------------------------------------------------------
	const FProbeBytes A = Probe(SentinelA, ProbeTally);
	const FProbeBytes B = Probe(SentinelB, ProbeTally);
	TestTrue(TEXT("the probe ran with different strike counts (the sentinels reached the core)"), A.StrikeAfterProbe != B.StrikeAfterProbe);
	AddInfo(FString::Printf(TEXT("NKP016 probe: k %u vs %u after the kills; reply bytes %d, owner part bytes %d"), A.StrikeAfterProbe, B.StrikeAfterProbe,
		A.NpcKillReply.Num(), A.OwnerPart.Num()));
	TestTrue(TEXT("informant-kill FStrikeResult reply bytes are identical under both strike-count sentinels"), A.InformantKillReply == B.InformantKillReply);
	TestTrue(TEXT("NPC-kill FStrikeResult reply bytes are identical under both strike-count sentinels"), A.NpcKillReply == B.NpcKillReply);
	TestTrue(TEXT("DefenceResult reply bytes are identical under both strike-count sentinels"), A.DefenceReply == B.DefenceReply);
	TestTrue(TEXT("C-TeamView owner part bytes (re-push) are identical under both strike-count sentinels"), A.OwnerPart == B.OwnerPart);
	// Positive control: the serialisation sees a one-step tally difference.
	const FProbeBytes Lower = Probe(SentinelA, ProbeTally - 1);
	TestTrue(TEXT("positive control: a tally one lower changes the reply bytes"), Lower.NpcKillReply != A.NpcKillReply);
	TestTrue(TEXT("positive control: a tally one lower changes the owner part bytes"), Lower.OwnerPart != A.OwnerPart);
	return true;
}

#endif
