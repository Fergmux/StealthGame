// Story asn-004 (production/epics/assassination-defence-reversal/story-004-wire-structs-fixed-width.md), AC-ASN-20 width
// clause and ADR-0018 Validation Criteria WireWidth.
//   AC1  a fixed sweep of FActionState values (defaults/sentinels, zero, int16 extremes of WarpPoint, Seq 1 and 65535,
//        time bit patterns 0, -1, -0, DBL_MIN, DBL_MAX, a denormal, +-infinity, quiet and payload NaNs, every pose, plus
//        1000 values from the constant seed WireSweepSeed) and of FDeathState values is written to an FNetBitWriter: every
//        FActionState is exactly 208 bits and every FDeathState exactly 65, NetSerialize returns true, and the round trip
//        reproduces every field bit for bit.
//   AC3  negative control (ADR-0008 section 3): the WITH_STEALTH_TEST_SEAMS serialiser that packs WarpPoint by magnitude
//        is installed, the same sweep must report at least one FActionState whose width differs from 208 (control
//        caught); the production serialiser is restored and the sweep passes again.
// The sweep's bit patterns come from SplitMix64 over a constant seed (deterministic; no engine random source).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/Testing/StealthNegativeControl.h"
#include "UObject/CoreNet.h"
#include "View/StealthEncounterWire.h"

#include <cfloat>
#include <limits>

namespace AssassinationWireWidthTestData
{
	/** The sweep's constant seed (QA: "values from a fixed constant seed WireSweepSeed"). */
	constexpr uint64 WireSweepSeed = 0x57495245u; // "WIRE"
	constexpr int32 SweepSeededCount = 1000;

	/** SplitMix64 step: a deterministic bit-pattern generator. */
	uint64 NextPattern(uint64& State)
	{
		State += 0x9E3779B97F4A7C15ull;
		uint64 Z = State;
		Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
		Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
		return Z ^ (Z >> 31);
	}

	double FromBits(uint64 Bits)
	{
		double Out;
		FMemory::Memcpy(&Out, &Bits, sizeof(double));
		return Out;
	}

	uint64 ToBits(double Value)
	{
		uint64 Out;
		FMemory::Memcpy(&Out, &Value, sizeof(double));
		return Out;
	}

	/** The special time patterns. */
	TArray<double> SpecialTimes()
	{
		return { 0.0, -1.0, -0.0, DBL_MIN, DBL_MAX, -DBL_MAX, std::numeric_limits<double>::denorm_min(), std::numeric_limits<double>::infinity(),
			-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(), FromBits(0x7FF0000000000001ull), FromBits(0xFFFFFFFFFFFFFFFFull),
			123.456, 1.0e9 };
	}

	TArray<FActionState> ActionSweep()
	{
		TArray<FActionState> Out;
		Out.Add(FActionState()); // the replicated defaults (sentinels)
		FActionState Zero;
		Zero.Seq = 0;
		Zero.StartServerTime = 0.0;
		Zero.ContactServerTime = 0.0;
		Out.Add(Zero);
		const int16 Extremes[] = { 0, 1, -1, 127, 128, -129, MAX_int16, MIN_int16 };
		for (const int16 E : Extremes)
		{
			FActionState S;
			S.Pose = EActionPose::Strike;
			S.WarpPoint.X = E;
			S.WarpPoint.Y = E;
			S.WarpPoint.Z = E;
			Out.Add(S);
		}
		for (const uint16 Seq : { static_cast<uint16>(1), static_cast<uint16>(65535), static_cast<uint16>(0), static_cast<uint16>(256) })
		{
			FActionState S;
			S.Seq = Seq;
			Out.Add(S);
		}
		for (const double T : SpecialTimes())
		{
			FActionState S;
			S.StartServerTime = T;
			S.ContactServerTime = T;
			Out.Add(S);
		}
		for (uint8 Pose = 0; Pose < GActionPoseCount; ++Pose)
		{
			FActionState S;
			S.Pose = static_cast<EActionPose>(Pose);
			S.Variant = Pose == static_cast<uint8>(EActionPose::Strike) ? (StrikeVariant::Strike_R60 | StrikeVariant::BushPreface) : 0;
			Out.Add(S);
		}
		uint64 State = WireSweepSeed;
		for (int32 Index = 0; Index < SweepSeededCount; ++Index)
		{
			FActionState S;
			const uint64 A = NextPattern(State);
			S.Pose = static_cast<EActionPose>(A % GActionPoseCount);
			S.Seq = static_cast<uint16>(A >> 8);
			S.Variant = static_cast<uint8>(A >> 24);
			S.StartServerTime = FromBits(NextPattern(State));
			S.ContactServerTime = FromBits(NextPattern(State));
			const uint64 W = NextPattern(State);
			S.WarpPoint.X = static_cast<int16>(W);
			S.WarpPoint.Y = static_cast<int16>(W >> 16);
			S.WarpPoint.Z = static_cast<int16>(W >> 32);
			Out.Add(S);
		}
		return Out;
	}

	TArray<FDeathState> DeathSweep()
	{
		TArray<FDeathState> Out;
		Out.Add(FDeathState()); // the sentinel
		for (const double T : SpecialTimes())
		{
			for (const bool bConcealed : { false, true })
			{
				FDeathState S;
				S.DeathServerTime = T;
				S.bDiedConcealed = bConcealed;
				Out.Add(S);
			}
		}
		uint64 State = WireSweepSeed ^ 0xDEADull;
		for (int32 Index = 0; Index < SweepSeededCount; ++Index)
		{
			FDeathState S;
			S.DeathServerTime = FromBits(NextPattern(State));
			S.bDiedConcealed = (Index & 1) != 0;
			Out.Add(S);
		}
		return Out;
	}

	/** Field-by-field, bit-for-bit equality (Pose compared after the reader's closed-enum mapping). */
	bool SameAction(const FActionState& A, const FActionState& B)
	{
		const EActionPose WantPose = static_cast<uint8>(A.Pose) < GActionPoseCount ? A.Pose : EActionPose::None;
		return WantPose == B.Pose && A.Seq == B.Seq && A.Variant == B.Variant && ToBits(A.StartServerTime) == ToBits(B.StartServerTime)
			&& ToBits(A.ContactServerTime) == ToBits(B.ContactServerTime) && A.WarpPoint == B.WarpPoint;
	}

	/** Runs the FActionState sweep into Ctx: width, return value, round trip. */
	void CheckActionSweep(FStealthCheckContext& Ctx, const TArray<FActionState>& Sweep)
	{
		int32 WrongWidth = 0;
		int32 WrongReturn = 0;
		int32 WrongTrip = 0;
		for (const FActionState& Value : Sweep)
		{
			FActionState Write = Value;
			FNetBitWriter Writer(nullptr, 1024);
			bool bOk = false;
			WrongReturn += Write.NetSerialize(Writer, nullptr, bOk) && bOk ? 0 : 1;
			WrongWidth += Writer.GetNumBits() == FActionState::WireBits ? 0 : 1;
			FNetBitReader Reader(nullptr, Writer.GetData(), Writer.GetNumBits());
			FActionState Read;
			Read.NetSerialize(Reader, nullptr, bOk);
			WrongTrip += SameAction(Value, Read) && !Reader.IsError() ? 0 : 1;
		}
		Ctx.TestEqual(FString::Printf(TEXT("FActionState: %d values of %d are not exactly 208 bits"), WrongWidth, Sweep.Num()), WrongWidth, 0);
		Ctx.TestEqual(TEXT("FActionState: NetSerialize returns true with success for every value"), WrongReturn, 0);
		Ctx.TestEqual(TEXT("FActionState: every value round-trips bit for bit"), WrongTrip, 0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationWireWidthTest, "StealthGame.Assassination.WireWidth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationWireWidthTest::RunTest(const FString& Parameters)
{
	using namespace AssassinationWireWidthTestData;
	const TArray<FActionState> Actions = ActionSweep();
	TestTrue(TEXT("the FActionState sweep holds the fixed cases plus 1000 seeded values"), Actions.Num() > SweepSeededCount);

	// ---- AC1: FActionState, production serialiser ----
	{
		FStealthCheckContext Ctx;
		CheckActionSweep(Ctx, Actions);
		for (const FString& Failure : Ctx.GetFailures())
		{
			AddError(Failure);
		}
	}

	// ---- AC1: FDeathState ----
	{
		int32 WrongWidth = 0;
		int32 WrongReturn = 0;
		int32 WrongTrip = 0;
		const TArray<FDeathState> Deaths = DeathSweep();
		for (const FDeathState& Value : Deaths)
		{
			FDeathState Write = Value;
			FNetBitWriter Writer(nullptr, 256);
			bool bOk = false;
			WrongReturn += Write.NetSerialize(Writer, nullptr, bOk) && bOk ? 0 : 1;
			WrongWidth += Writer.GetNumBits() == FDeathState::WireBits ? 0 : 1;
			FNetBitReader Reader(nullptr, Writer.GetData(), Writer.GetNumBits());
			FDeathState Read;
			Read.bDiedConcealed = !Value.bDiedConcealed; // the read must overwrite the flag either way
			Read.NetSerialize(Reader, nullptr, bOk);
			WrongTrip += ToBits(Read.DeathServerTime) == ToBits(Value.DeathServerTime) && Read.bDiedConcealed == Value.bDiedConcealed && !Reader.IsError() ? 0 : 1;
		}
		TestEqual(FString::Printf(TEXT("FDeathState: values of %d not exactly 65 bits"), Deaths.Num()), WrongWidth, 0);
		TestEqual(TEXT("FDeathState: NetSerialize returns true with success"), WrongReturn, 0);
		TestEqual(TEXT("FDeathState: every value round-trips bit for bit"), WrongTrip, 0);
	}

	// ---- AC3: negative control, then the production serialiser restored ----
	{
		StealthEncounterWireTestSeams::SetActionStateSerializerOverride(&StealthEncounterWireTestSeams::SerializeWarpPointByMagnitude);
		FStealthNegativeControl Control(TEXT("FActionState WarpPoint packed by magnitude"));
		const FStealthNegativeControlResult Result = Control.Run([&Actions](FStealthCheckContext& Ctx) { CheckActionSweep(Ctx, Actions); });
		StealthEncounterWireTestSeams::SetActionStateSerializerOverride(nullptr);
		FStealthNegativeControl::Report(*this, Result);
		TestTrue(TEXT("the control was caught by the width assertion"), Result.bDetected && Result.InnerFailures.ContainsByPredicate([](const FString& F) { return F.Contains(TEXT("208 bits")); }));

		FStealthCheckContext Restored;
		CheckActionSweep(Restored, Actions);
		TestFalse(TEXT("with the production serialiser restored the sweep passes"), Restored.HasFailed());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS
