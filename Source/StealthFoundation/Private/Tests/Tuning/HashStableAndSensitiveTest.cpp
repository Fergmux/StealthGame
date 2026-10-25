// Story tdc-045 (production/epics/tuning-data-config/story-045-hasher-golden.md), GDD AC-5 (CR-7, F-2):
// the F-2 canonical hash equals the committed golden of a fixed fixture (one key per type tag, SchemaVersion
// 7), is independent of key order, changes on a one-bit change of any field, maps -0.0 and +0.0 equally,
// and separates TuningHash (all keys) from ClientHash (public keys only). The golden values were computed
// outside the code under test (a throwaway Python implementation of the GDD F-2 byte layout, kept in the
// story evidence), never generated from FTuningHasher.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "Server/StealthServerTuning.h"
#include "Server/StealthTuningHasher.h"
#include "Server/StealthTuningVersion.h"
#include "Server/Testing/StealthNegativeControl.h"

#include <cstring>

namespace HashStableAndSensitiveTestData
{
	/** Fixture schema version (not the live TuningSchemaVersion: the golden must not move on a bump). */
	constexpr int32 FixtureSchemaVersion = 7;

	const FAnsiStringView FlagKey = "bFixtureFlag";
	const FAnsiStringView CountKey = "FixtureCount";
	const FAnsiStringView DelayKey = "FixtureDelaySec";
	const FAnsiStringView ModeKey = "FixtureMode";
	const FAnsiStringView AssetKey = "FixtureMotionAsset";
	const TCHAR* const AssetPathMixed = TEXT("/Game/StealthGame/AI/DA_CrowdMotionProfile_Default.DA_CrowdMotionProfile_Default");
	const TCHAR* const AssetPathLower = TEXT("/game/stealthgame/ai/da_crowdmotionprofile_default.da_crowdmotionprofile_default");

	/** Golden values: independently computed (production/qa/evidence/story-045-hasher-golden/golden.py). */
	constexpr uint64 GoldenTuningHash = 0xe7bde85917144648ull;
	constexpr uint64 GoldenClientHash = 0x49026584c2ec125eull;

	/** The fixture: one key per type tag (bool, int, float, enum, SoftRef); public = bool, int, enum. */
	struct FFixture
	{
		FTuningHashEntry Entries[5];
		bool bPublic[5] = { true, true, false, true, false };

		FFixture()
		{
			Entries[0] = { FlagKey, StealthTuningTypeTag::Bool, 1.0, FStringView() };
			Entries[1] = { CountKey, StealthTuningTypeTag::Int, 42.0, FStringView() };
			Entries[2] = { DelayKey, StealthTuningTypeTag::Float, 2.5, FStringView() };
			Entries[3] = { ModeKey, StealthTuningTypeTag::Enum, 3.0, FStringView() };
			Entries[4] = { AssetKey, StealthTuningTypeTag::SoftRef, 0.0, FStringView(AssetPathMixed) };
		}

		TArray<FTuningHashEntry> All() const { return TArray<FTuningHashEntry>(Entries, UE_ARRAY_COUNT(Entries)); }

		TArray<FTuningHashEntry> PublicOnly() const
		{
			TArray<FTuningHashEntry> Out;
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Entries); ++Index)
			{
				if (bPublic[Index])
				{
					Out.Add(Entries[Index]);
				}
			}
			return Out;
		}
	};

	uint64 TuningHashOf(const FFixture& Fixture) { return FTuningHasher::HashEntries(FixtureSchemaVersion, Fixture.All()); }
	uint64 ClientHashOf(const FFixture& Fixture) { return FTuningHasher::HashEntries(FixtureSchemaVersion, Fixture.PublicOnly()); }

	/** Flips one bit of a float value's IEEE-754 representation. */
	double FlipFloatBit(double Value, int32 Bit)
	{
		float AsFloat = static_cast<float>(Value);
		uint32 Bits = 0;
		std::memcpy(&Bits, &AsFloat, sizeof(Bits));
		Bits ^= (1u << Bit);
		std::memcpy(&AsFloat, &Bits, sizeof(Bits));
		return static_cast<double>(AsFloat);
	}

	/** A broken hasher for the negative control: it ignores the value of the float key. */
	uint64 HashIgnoringFloatValues(const FFixture& Fixture)
	{
		TArray<FTuningHashEntry> Entries = Fixture.All();
		for (FTuningHashEntry& Entry : Entries)
		{
			if (Entry.TypeTag == StealthTuningTypeTag::Float)
			{
				Entry.Value = 0.0;
			}
		}
		return FTuningHasher::HashEntries(FixtureSchemaVersion, Entries);
	}

	FString Hex(TConstArrayView<uint8> Bytes)
	{
		return BytesToHex(Bytes.GetData(), Bytes.Num()).ToLower();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTuningHashStableAndSensitiveTest, "StealthGame.Tuning.HashStableAndSensitive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTuningHashStableAndSensitiveTest::RunTest(const FString& Parameters)
{
	using namespace HashStableAndSensitiveTestData;

	// Byte level: the two GDD F-2 example encodings.
	{
		TArray<uint8> TeamSize;
		FTuningHasher::AppendEntry(TeamSize, { "TeamSize", StealthTuningTypeTag::Int, 5.0, FStringView() });
		TestEqual(TEXT("F-2 example: TeamSize int 5"), Hex(TeamSize), FString(TEXT("085465616d53697a65010500000000000000")));
		TArray<uint8> Duration;
		FTuningHasher::AppendEntry(Duration, { "DefenceDurationSec", StealthTuningTypeTag::Float, 4.0, FStringView() });
		TestEqual(TEXT("F-2 example: DefenceDurationSec float 4.0"), Hex(Duration), FString(TEXT("12446566656e63654475726174696f6e5365630200008040")));
	}

	// Golden: the committed fixture hashes to the independently computed values.
	const FFixture Fixture;
	TestEqual(TEXT("TuningHash of the fixture equals the golden"), TuningHashOf(Fixture), GoldenTuningHash);
	TestEqual(TEXT("ClientHash of the fixture equals the golden"), ClientHashOf(Fixture), GoldenClientHash);
	TestEqual(TEXT("the stream starts with u32le(SchemaVersion)"), Hex(TArrayView<const uint8>(FTuningHasher::BuildStream(FixtureSchemaVersion, Fixture.All()).GetData(), 4)), FString(TEXT("07000000")));

	// Key order: a permutation gives the same hash.
	{
		TArray<FTuningHashEntry> Reversed = Fixture.All();
		Algo::Reverse(Reversed);
		TestEqual(TEXT("reversed input order hashes equally"), FTuningHasher::HashEntries(FixtureSchemaVersion, Reversed), GoldenTuningHash);
		TArray<FTuningHashEntry> Rotated = Fixture.All();
		const FTuningHashEntry FirstEntry = Rotated[0];
		Rotated.RemoveAt(0);
		Rotated.Add(FirstEntry);
		TestEqual(TEXT("rotated input order hashes equally"), FTuningHasher::HashEntries(FixtureSchemaVersion, Rotated), GoldenTuningHash);
	}

	// SoftRef: a lower-case twin of the path hashes equally (package names are case-insensitive).
	{
		FFixture Lower;
		Lower.Entries[4].SoftPath = FStringView(AssetPathLower);
		TestEqual(TEXT("SoftRef lower-case twin hashes equally"), TuningHashOf(Lower), GoldenTuningHash);
		FFixture Null;
		Null.Entries[4].SoftPath = FStringView();
		TestNotEqual(TEXT("SoftRef null path changes the hash"), TuningHashOf(Null), GoldenTuningHash);
	}

	// One-bit changes: every field and every type tag.
	{
		FFixture Flag; Flag.Entries[0].Value = 0.0;
		TestNotEqual(TEXT("bool flipped changes the hash"), TuningHashOf(Flag), GoldenTuningHash);
		for (int32 Bit = 0; Bit < 31; ++Bit)
		{
			FFixture Count; Count.Entries[1].Value = static_cast<double>(42 ^ (1 << Bit));
			TestNotEqual(FString::Printf(TEXT("int bit %d flipped changes the hash"), Bit), TuningHashOf(Count), GoldenTuningHash);
		}
		for (int32 Bit = 0; Bit < 32; ++Bit)
		{
			FFixture Delay; Delay.Entries[2].Value = FlipFloatBit(2.5, Bit);
			TestNotEqual(FString::Printf(TEXT("float bit %d flipped changes the hash"), Bit), TuningHashOf(Delay), GoldenTuningHash);
		}
		for (int32 Bit = 0; Bit < 8; ++Bit)
		{
			FFixture Mode; Mode.Entries[3].Value = static_cast<double>(3 ^ (1 << Bit));
			TestNotEqual(FString::Printf(TEXT("enum bit %d flipped changes the hash"), Bit), TuningHashOf(Mode), GoldenTuningHash);
		}
		FFixture Path; Path.Entries[4].SoftPath = FStringView(TEXT("/Game/StealthGame/AI/DA_CrowdMotionProfile_Defaulu.DA_CrowdMotionProfile_Default"));
		TestNotEqual(TEXT("SoftRef path one character changed changes the hash"), TuningHashOf(Path), GoldenTuningHash);
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Fixture.Entries); ++Index)
		{
			FFixture Tag; Tag.Entries[Index].TypeTag ^= 1;
			if (Tag.Entries[Index].TypeTag != StealthTuningTypeTag::SoftRef && Fixture.Entries[Index].TypeTag != StealthTuningTypeTag::SoftRef)
			{
				TestNotEqual(FString::Printf(TEXT("type tag of entry %d flipped changes the hash"), Index), TuningHashOf(Tag), GoldenTuningHash);
			}
		}
		FFixture Name; Name.Entries[1].Key = "FixtureCounu";
		TestNotEqual(TEXT("key name one bit changed changes the hash"), TuningHashOf(Name), GoldenTuningHash);
		TestNotEqual(TEXT("SchemaVersion one bit changed changes the hash"), FTuningHasher::HashEntries(FixtureSchemaVersion ^ 1, Fixture.All()), GoldenTuningHash);
	}

	// -0.0 and +0.0 hash equally (tested on the hash function directly; F-1 never emits -0.0).
	{
		FFixture Plus; Plus.Entries[2].Value = 0.0;
		FFixture Minus; Minus.Entries[2].Value = -0.0;
		TestEqual(TEXT("-0.0 and +0.0 hash equally"), TuningHashOf(Minus), TuningHashOf(Plus));
	}

	// Audience: a server-only change moves TuningHash only; a public change moves both.
	{
		FFixture Server; Server.Entries[2].Value = 3.5;
		TestNotEqual(TEXT("server-only change alters TuningHash"), TuningHashOf(Server), GoldenTuningHash);
		TestEqual(TEXT("server-only change leaves ClientHash"), ClientHashOf(Server), GoldenClientHash);
		FFixture Public; Public.Entries[1].Value = 43.0;
		TestNotEqual(TEXT("public change alters TuningHash"), TuningHashOf(Public), GoldenTuningHash);
		TestNotEqual(TEXT("public change alters ClientHash"), ClientHashOf(Public), GoldenClientHash);
	}

	// The real schema: the same audience rule through the generated accessors, and Default (the schema
	// defaults) hashes the same on every call. Cross-platform equality is claimed only for the platforms
	// actually run (MSVC x64 today); the value is logged for the platform matrix.
	{
		FStealthServerTuning Defaults;
		const uint64 DefaultTuning = FTuningHasher::Hash(Defaults, TuningSchemaVersion, false);
		const uint64 DefaultClient = FTuningHasher::Hash(Defaults, TuningSchemaVersion, true);
		TestEqual(TEXT("Default TuningHash is stable across calls"), FTuningHasher::Hash(Defaults, TuningSchemaVersion, false), DefaultTuning);
		TestNotEqual(TEXT("TuningHash and ClientHash of Default differ (server-only keys exist)"), DefaultTuning, DefaultClient);
		AddInfo(FString::Printf(TEXT("Default schema defaults: TuningHash 0x%016llx ClientHash 0x%016llx (SchemaVersion %d)"),
			static_cast<unsigned long long>(DefaultTuning), static_cast<unsigned long long>(DefaultClient), TuningSchemaVersion));

		FStealthServerTuning ServerChanged = Defaults;
		ServerChanged.Crowd_Server.NpcCount = 47;
		TestNotEqual(TEXT("NpcCount (ServerOnly) change alters TuningHash"), FTuningHasher::Hash(ServerChanged, TuningSchemaVersion, false), DefaultTuning);
		TestEqual(TEXT("NpcCount (ServerOnly) change leaves ClientHash"), FTuningHasher::Hash(ServerChanged, TuningSchemaVersion, true), DefaultClient);

		FStealthServerTuning PublicChanged = Defaults;
		PublicChanged.Assassination_Public.DefenceDurationSec = 4.5f;
		TestNotEqual(TEXT("DefenceDurationSec (All) change alters TuningHash"), FTuningHasher::Hash(PublicChanged, TuningSchemaVersion, false), DefaultTuning);
		TestNotEqual(TEXT("DefenceDurationSec (All) change alters ClientHash"), FTuningHasher::Hash(PublicChanged, TuningSchemaVersion, true), DefaultClient);
	}

	// Negative control: a hasher that ignores the float value must be caught by the sensitivity check.
	{
		FStealthNegativeControl Control(TEXT("HasherIgnoresFloatValue"));
		const FStealthNegativeControlResult Result = Control.Run([](FStealthCheckContext& Ctx)
		{
			FFixture Changed; Changed.Entries[2].Value = 3.5;
			Ctx.TestTrue(TEXT("float change alters the hash"), HashIgnoringFloatValues(Changed) != HashIgnoringFloatValues(FFixture()));
		});
		FStealthNegativeControl::Report(*this, Result);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
