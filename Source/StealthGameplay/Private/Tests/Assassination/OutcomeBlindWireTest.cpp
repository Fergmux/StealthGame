// Stories asn-012 AC1 / AC2 (AC-ASN-20 slot window, ADR-0008 section 3 negative control) and asn-013 AC1 (AC-ASN-20
// death-write clause): StealthGame.Assassination.OutcomeBlindWire. A kill, a whiff and a reversal by the same attacker at
// the same open-spot WorldPoint, scripted at the same slot server time on the real round queue (FRoundEncounterRig with
// the core's production writes), every other pawn stationary. The server's writes are recorded per tick from the slot:
//   slot tick       exactly one write in all three runs, the attacker's FActionState {Strike, Variant, Start, Contact,
//                   WarpPoint}, bit-identical across the runs (208 bits by NetSerialize, same bytes): no field encodes the
//                   outcome; nothing on any other pawn up to (excluding) the death-write tick;
//   death-write tick FDeathState on the victim (kill) or on the attacker plus Counter on the defender (reversal), at
//                   ContactServerTime - StrikeNetLeadSec (0.15 s) +- 1 tick, DeathServerTime = ContactServerTime; the
//                   whiff writes neither, ever (checked to the window's end + 0.5 s).
// Negative control (AC2): with the WITH_STEALTH_TEST_SEAMS seam that writes the outcome into Variant, the same comparison
// fails. Deviation (logged): the byte capture on a bystander connection under LeakNetProfile (IVP story 008 harness) and
// the StealthLeak2 presentation clause are not built; the server write set is the capture's source, and the client half
// is the PIE test StealthGame.Assassination.PIE.OutcomeBlindWire.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Serialization/BitWriter.h"
#include "Tests/Assassination/EncounterRoundRig.h"

namespace OutcomeBlindWireTestData
{
	using namespace StealthEncounterTest;

	enum class ERun : uint8
	{
		Kill,
		Whiff,
		Reversal,
	};

	const TCHAR* Name(ERun Run)
	{
		return Run == ERun::Kill ? TEXT("kill") : (Run == ERun::Whiff ? TEXT("whiff") : TEXT("reversal"));
	}

	/** The fixture geometry: the attacker at the origin facing +X, the WorldPoint 150 cm ahead. */
	const FVector WorldPoint(150.0, 0.0, 0.0);
	/** The whiff run's target pawn stands well outside the open-spot radius (110 cm), so nobody is at the WorldPoint. */
	const FVector WhiffTargetAt(150.0, 400.0, 0.0);
	const FVector BystanderAt(2000.0, 0.0, 0.0);
	/** Ticks drained after the slot: past the death write (0.25 s) plus 0.5 s. */
	constexpr int32 WindowTicks = 45;

	struct FCapture
	{
		bool bValid = false;
		/** The writes of the slot drain. */
		TArray<FRecordingPawnState::FWrite> Slot;
		/** The writes of the following drains, with their tick offset from the slot (1 = the next tick). */
		TArray<TPair<int32, FRecordingPawnState::FWrite>> After;
		double SlotServerTime = 0.0;
		double ContactServerTime = 0.0;
		EAttackOutcome Outcome = EAttackOutcome::Whiff;
		APawn* Attacker = nullptr;
		APawn* Target = nullptr;
		/** The slot write as serialised (NetSerialize), and its bit count. */
		TArray<uint8> Bytes;
		int64 Bits = 0;
	};

	FCapture Run(ERun Kind)
	{
		FCapture Out;
		FRoundEncounterRig R(TEXT("OutcomeBlindWireWorld"), /*bProductionWrites*/ true);
		R.W.ToActive(RoundTestFixtures::Layout2v2.Team0, RoundTestFixtures::Layout2v2.Team1);
		APawn* A = R.Spawn(FVector::ZeroVector, 1);
		APawn* Target = R.Spawn(Kind == ERun::Whiff ? WhiffTargetAt : WorldPoint);
		APawn* Bystander = R.Spawn(BystanderAt);
		// The same pre-slot drain in every run: the reversal's defender braces; the other runs brace the far bystander.
		R.Drain({ R.Command(Kind == ERun::Reversal ? Target : Bystander, ECommandKind::PrepareBegin) });
		R.PawnState.Writes.Reset();

		R.Drain({ R.Command(A, ECommandKind::Attack, WorldPoint) });
		if (R.Core->GetResolutions().Num() != 1)
		{
			return Out;
		}
		const FAttackResolution& Resolution = R.Core->GetResolutions()[0];
		Out.Outcome = Resolution.Outcome;
		Out.SlotServerTime = Resolution.ExecServerTime;
		Out.ContactServerTime = Resolution.ContactServerTime;
		Out.Attacker = A;
		Out.Target = Target;
		Out.Slot = R.PawnState.Writes;
		for (int32 Tick = 1; Tick <= WindowTicks; ++Tick)
		{
			R.PawnState.Writes.Reset();
			R.Tick();
			for (const FRecordingPawnState::FWrite& Write : R.PawnState.Writes)
			{
				Out.After.Add({ Tick, Write });
			}
		}
		if (Out.Slot.Num() > 0)
		{
			FActionState Written = Out.Slot[0].State;
			FBitWriter Writer(256, true);
			bool bSuccess = false;
			Written.NetSerialize(Writer, nullptr, bSuccess);
			Out.Bits = Writer.GetNumBits();
			Out.Bytes = *Writer.GetBuffer();
		}
		Out.bValid = true;
		return Out;
	}

	/** True when two runs' slot writes are the same write (every field, by NetSerialize bytes and Identical). */
	bool SameSlotWrite(const FCapture& A, const FCapture& B)
	{
		return A.Slot.Num() == 1 && B.Slot.Num() == 1 && A.Slot[0].State.Identical(&B.Slot[0].State, 0) && A.Bits == B.Bits && A.Bytes == B.Bytes;
	}

	/** Restores the negative-control seam whatever the test does. */
	struct FSeamGuard
	{
		explicit FSeamGuard(bool bOn) { StealthEncounterTestSeams::bWriteOutcomeIntoVariant = bOn; }
		~FSeamGuard() { StealthEncounterTestSeams::bWriteOutcomeIntoVariant = false; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationOutcomeBlindWireTest, "StealthGame.Assassination.OutcomeBlindWire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationOutcomeBlindWireTest::RunTest(const FString& Parameters)
{
	using namespace OutcomeBlindWireTestData;
	const FEncounterParams Params = FEncounterParams::Defaults();
	const double Lead = Params.Server.StrikeNetLeadSec;
	const double Tick = RoundTestFixtures::RigFrameSec;

	FCapture Runs[3];
	for (const ERun Kind : { ERun::Kill, ERun::Whiff, ERun::Reversal })
	{
		FCapture& C = Runs[static_cast<int32>(Kind)];
		C = Run(Kind);
		const TCHAR* N = Name(Kind);
		if (!TestTrue(FString::Printf(TEXT("%s: one executed Attack"), N), C.bValid))
		{
			return false;
		}
		const EAttackOutcome Expected = Kind == ERun::Kill ? EAttackOutcome::Kill : (Kind == ERun::Whiff ? EAttackOutcome::Whiff : EAttackOutcome::Reversal);
		TestTrue(FString::Printf(TEXT("%s: the outcome is the scripted one"), N), C.Outcome == Expected);

		// Slot tick: exactly the attacker's FActionState.
		TestEqual(FString::Printf(TEXT("%s: exactly one write in the slot tick"), N), C.Slot.Num(), 1);
		const bool bSlotOk = C.Slot.Num() == 1 && !C.Slot[0].bDeath && C.Slot[0].Pawn == C.Attacker && C.Slot[0].State.Pose == EActionPose::Strike;
		TestTrue(FString::Printf(TEXT("%s: it is the attacker's FActionState (Strike)"), N), bSlotOk);
		TestEqual(FString::Printf(TEXT("%s: 208 bits on the wire"), N), C.Bits, static_cast<int64>(FActionState::WireBits));
		if (bSlotOk)
		{
			const FActionState& S = C.Slot[0].State;
			TestTrue(FString::Printf(TEXT("%s: StartServerTime is the slot time"), N), S.StartServerTime == C.SlotServerTime);
			TestTrue(FString::Printf(TEXT("%s: ContactServerTime = slot + StrikeContactSec"), N), FMath::IsNearlyEqual(S.ContactServerTime, C.SlotServerTime + Params.Public.StrikeContactSec, 1e-9));
		}

		// Up to (excluding) the death-write tick: nothing on any pawn other than the attacker, no death write anywhere.
		const double DeathWriteTime = C.ContactServerTime - Lead;
		int32 EarlyOthers = 0;
		int32 EarlyDeaths = 0;
		TArray<TPair<int32, FRecordingPawnState::FWrite>> Outcome;
		for (const TPair<int32, FRecordingPawnState::FWrite>& Entry : C.After)
		{
			const bool bOutcomeWrite = Entry.Value.bDeath || Entry.Value.State.Pose == EActionPose::Counter;
			if (Entry.Value.ServerTime + 0.5 * Tick < DeathWriteTime - Tick)
			{
				EarlyOthers += Entry.Value.Pawn != C.Attacker ? 1 : 0;
				EarlyDeaths += bOutcomeWrite ? 1 : 0;
			}
			if (bOutcomeWrite)
			{
				Outcome.Add(Entry);
			}
		}
		TestEqual(FString::Printf(TEXT("%s: no write on another pawn before the death-write tick"), N), EarlyOthers, 0);
		TestEqual(FString::Printf(TEXT("%s: no outcome write before the death-write tick"), N), EarlyDeaths, 0);

		// The death-write tick.
		if (Kind == ERun::Whiff)
		{
			TestEqual(TEXT("whiff: FDeathState, Disabled and Counter are never written"), Outcome.Num(), 0);
			continue;
		}
		APawn* Fallen = Kind == ERun::Kill ? C.Target : C.Attacker;
		const TPair<int32, FRecordingPawnState::FWrite>* Death = Outcome.FindByPredicate([Fallen](const TPair<int32, FRecordingPawnState::FWrite>& E) { return E.Value.bDeath && E.Value.Pawn == Fallen; });
		if (TestNotNull(FString::Printf(TEXT("%s: FDeathState written on the %s"), N, Kind == ERun::Kill ? TEXT("victim") : TEXT("attacker")), Death))
		{
			AddInfo(FString::Printf(TEXT("OUTCOMEBLIND %s: slot %.4f, contact %.4f, death write applied %.4f (T_dw %.4f), tick +%d"), N, C.SlotServerTime, C.ContactServerTime,
				Death->Value.ServerTime, DeathWriteTime, Death->Key));
			TestTrue(FString::Printf(TEXT("%s: at ContactServerTime - 0.15 s +- 1 tick"), N), FMath::Abs(Death->Value.ServerTime - DeathWriteTime) <= Tick + 1e-9);
			TestTrue(FString::Printf(TEXT("%s: DeathServerTime = ContactServerTime"), N), Death->Value.Death.DeathServerTime == C.ContactServerTime);
		}
		const int32 OutcomeCount = Outcome.Num();
		if (Kind == ERun::Kill)
		{
			TestEqual(TEXT("kill: one outcome write (the victim's death)"), OutcomeCount, 1);
		}
		else
		{
			const TPair<int32, FRecordingPawnState::FWrite>* Counter = Outcome.FindByPredicate([&C](const TPair<int32, FRecordingPawnState::FWrite>& E) { return !E.Value.bDeath && E.Value.Pawn == C.Target; });
			TestTrue(TEXT("reversal: Counter on the defender in the death-write tick"), Counter != nullptr && Death != nullptr && Counter->Key == Death->Key
				&& Counter->Value.State.ContactServerTime == C.ContactServerTime && Counter->Value.State.Variant == 0 && Counter->Value.State.WarpPoint == FFixedWarpOffset());
			TestEqual(TEXT("reversal: two outcome writes (the attacker's death, the defender's Counter)"), OutcomeCount, 2);
			TestTrue(TEXT("reversal: the defender is never written dead (never Disabled)"), !Outcome.ContainsByPredicate([&C](const TPair<int32, FRecordingPawnState::FWrite>& E) { return E.Value.bDeath && E.Value.Pawn == C.Target; }));
		}
	}

	// Across the runs: one write, identical in every field and in its bytes.
	TestTrue(TEXT("kill and whiff slot writes are identical (values and bytes)"), SameSlotWrite(Runs[0], Runs[1]));
	TestTrue(TEXT("kill and reversal slot writes are identical (values and bytes)"), SameSlotWrite(Runs[0], Runs[2]));

	// AC2 negative control: a build that writes the outcome into Variant fails the same comparison.
	{
		FSeamGuard Seam(true);
		const FCapture Kill = Run(ERun::Kill);
		const FCapture Whiff = Run(ERun::Whiff);
		const FCapture Reversal = Run(ERun::Reversal);
		const bool bDetected = !SameSlotWrite(Kill, Whiff) && !SameSlotWrite(Kill, Reversal);
		AddInfo(FString::Printf(TEXT("OUTCOMEBLIND negative control (outcome in Variant): variants kill %d whiff %d reversal %d -> detected %d"),
			Kill.Slot.Num() == 1 ? Kill.Slot[0].State.Variant : -1, Whiff.Slot.Num() == 1 ? Whiff.Slot[0].State.Variant : -1,
			Reversal.Slot.Num() == 1 ? Reversal.Slot[0].State.Variant : -1, bDetected ? 1 : 0));
		TestTrue(TEXT("negative control: the outcome-in-Variant seam build fails the comparison"), bDetected);
	}
	TestTrue(TEXT("the production build passes after the seam run (seam restored)"), SameSlotWrite(Run(ERun::Kill), Run(ERun::Whiff)));
	return true;
}

#endif
