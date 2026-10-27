// Story asn-026 AC3 / AC-ASN-34 [Integration/ADVISORY] (ADR-0018 Performance Implications; control manifest guardrail
// "<= 0.1 ms total resolution for 10 Attacks in a frame with 50 pawns; <= 64 bytes of per-strike replicated payload"):
// StealthGame.Assassination.ResolverCost. The strike-resolver rig (FAttackResolverCore with the wire-write recorder) holds
// 50 pawns; each measured frame resolves 10 Attacks through FEncounterTelemetryProbe, the resolver UEncounterService
// registers, so the Stealth.Budget.Assassination scope and its cycle meter time exactly the production path. Three shapes
// (QA edge cases): 10 kills of distinct NPCs, 10 reversals (every victim braced first, outside the timed Attacks), and 10
// Attacks on one victim (one kill, nine NoContact on the falling body). Statistic (QA rewrite): the median over 100 frames
// after 30 warm-up frames, <= 0.1 ms per shape; the first (cold) frame is reported, not gated. Payload: per strike, the
// attacker's FActionState (208 bits = 26 bytes) plus its StrikeResult reply entry (fixed width, the same for every result)
// as each NetSerialize writes it, <= 64 bytes before property headers.
// This is the advisory smoke in a Development editor process; the evidence standard for any claim beyond it is a cooked
// Development standalone on 3060-class hardware (story Engine Notes).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "Server/EncounterTelemetryProbe.h"
#include "Tests/Assassination/EncounterTestRig.h"
#include "UObject/CoreNet.h"
#include "View/CommandReplyTypes.h"

namespace ResolverCostTestData
{
	using namespace StealthEncounterTest;

	enum class EShape : uint8 { DistinctKills, Reversals, OneVictim };
	const TCHAR* Name(EShape Shape) { return Shape == EShape::DistinctKills ? TEXT("10 kills") : (Shape == EShape::Reversals ? TEXT("10 reversals") : TEXT("10 on one victim")); }

	constexpr int32 PawnCount = 50;
	constexpr int32 AttacksPerFrame = 10;
	constexpr int32 WarmupFrames = 30;
	constexpr int32 MeasuredFrames = 100;
	/** The guardrail (control manifest): total resolution time of the frame's 10 Attacks. */
	constexpr double BudgetMs = 0.1;
	/** The guardrail: per-strike replicated payload before property headers. */
	constexpr int32 PayloadBudgetBytes = 64;
	constexpr double ReachCm = 150.0;

	struct FShapeResult
	{
		double ColdMs = 0.0;
		double MedianMs = 0.0;
		double MaxMs = 0.0;
		int32 Kills = 0;
		int32 Reversals = 0;
		int32 Whiffs = 0;
		int32 MaxPayloadBytes = 0;
	};

	int32 BytesOf(int32 Bits) { return (Bits + 7) / 8; }

	FShapeResult Run(EShape Shape)
	{
		FShapeResult Out;
		FRig R(TEXT("ResolverCost"), nullptr, /*bWithPawnState*/ true);
		TArray<APawn*> Attackers;
		TArray<APawn*> Victims;
		const FVector OneVictimAt(0.0, 20000.0, 0.0);
		for (int32 Index = 0; Index < AttacksPerFrame; ++Index)
		{
			if (Shape == EShape::OneVictim)
			{
				// Around the one victim, each facing it from ReachCm.
				const double Yaw = Index * 360.0 / AttacksPerFrame;
				const FVector At = OneVictimAt + FRotator(0.0, Yaw, 0.0).RotateVector(FVector(ReachCm, 0.0, 0.0));
				Attackers.Add(R.Spawn(At, 0, Yaw + 180.0));
			}
			else
			{
				Attackers.Add(R.Spawn(FVector(Index * 1000.0, 0.0, 0.0), 0));
				Victims.Add(R.Spawn(FVector(Index * 1000.0 + ReachCm, 0.0, 0.0)));
			}
		}
		if (Shape == EShape::OneVictim)
		{
			Victims.Add(R.Spawn(OneVictimAt));
		}
		while (R.Pawns.Pawns.Num() < PawnCount)
		{
			R.Spawn(FVector(R.Pawns.Pawns.Num() * 700.0, 8000.0, 0.0)); // the rest of the crowd, out of reach
		}

		FEncounterTelemetryProbe Probe;
		Probe.Bind(R.Core.Get(), &R.Defence, &R.Clock, R.Params.Public.DefenceCooldownSec, nullptr);
		FAssassinationCostMeter& Meter = FAssassinationCostMeter::Get();
		Meter.TakeMs();
		TArray<double> Samples;
		for (int32 Frame = 0; Frame < WarmupFrames + MeasuredFrames; ++Frame)
		{
			// Untimed: a fresh round (every role Ready, every defence state none), the recorders emptied, the braces up.
			R.Core->Reset();
			R.PawnState.Writes.Reset();
			R.Replies.Entries.Reset();
			R.Kills.Reset();
			R.Calls.Reset();
			R.Queue.Scheduled.Reset();
			R.Queue.Staged.Reset();
			R.Queue.VoidCodes.Reset();
			R.Seeds.Asked.Reset();
			R.Roster.Transitions.Reset();
			for (TPair<const APawn*, FFakeRoster::FMember>& Member : R.Roster.Members)
			{
				Member.Value.State = EParticipantState::Active; // a reversed attacker is Active again in the next round
			}
			R.Advance(3.0);
			if (Shape == EShape::Reversals)
			{
				for (APawn* Victim : Victims)
				{
					R.Command(Victim, ECommandKind::PrepareBegin);
				}
				R.Advance(0.2);
			}
			// Timed: the frame's 10 Attacks, in one tick, through the registered resolver.
			Meter.bEnabled = true;
			for (int32 Index = 0; Index < AttacksPerFrame; ++Index)
			{
				APawn* Victim = Shape == EShape::OneVictim ? Victims[0] : Victims[Index];
				Probe.Resolve(R.AttackItem(Attackers[Index], R.Facts(Victim).Location, Victim), R.NextKey());
			}
			Meter.bEnabled = false;
			const double Ms = Meter.TakeMs();
			if (Frame == 0)
			{
				Out.ColdMs = Ms;
			}
			if (Frame >= WarmupFrames)
			{
				Samples.Add(Ms);
			}
			if (Frame == WarmupFrames)
			{
				for (const FAttackResolution& Resolution : R.Core->GetResolutions())
				{
					Out.Kills += Resolution.Outcome == EAttackOutcome::Kill ? 1 : 0;
					Out.Reversals += Resolution.Outcome == EAttackOutcome::Reversal ? 1 : 0;
					Out.Whiffs += Resolution.Outcome == EAttackOutcome::Whiff ? 1 : 0;
					// The per-strike payload: the attacker's slot write plus its owner reply entry.
					const FRecordingPawnState::FWrite* Write = R.PawnState.Writes.FindByPredicate([&Resolution](const FRecordingPawnState::FWrite& W)
					{
						return !W.bDeath && W.Pawn == Resolution.Attacker && W.State.Pose == EActionPose::Strike;
					});
					FNetBitWriter ActionWriter(nullptr, 1024);
					FActionState Action = Write != nullptr ? Write->State : FActionState();
					bool bActionOk = true;
					Action.NetSerialize(ActionWriter, nullptr, bActionOk);
					FNetBitWriter ReplyWriter(nullptr, 1024);
					FCommandReply Reply = FCommandReply::MakeStrikeResult(Resolution.StrikeResult);
					bool bReplyOk = true;
					Reply.NetSerialize(ReplyWriter, nullptr, bReplyOk);
					Out.MaxPayloadBytes = FMath::Max(Out.MaxPayloadBytes, BytesOf(static_cast<int32>(ActionWriter.GetNumBits())) + BytesOf(static_cast<int32>(ReplyWriter.GetNumBits())));
				}
			}
		}
		Samples.Sort();
		Out.MedianMs = Samples.Num() > 0 ? 0.5 * (Samples[(Samples.Num() - 1) / 2] + Samples[Samples.Num() / 2]) : 0.0;
		Out.MaxMs = Samples.Num() > 0 ? Samples.Last() : 0.0;
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssassinationResolverCostTest, "StealthGame.Assassination.ResolverCost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssassinationResolverCostTest::RunTest(const FString& Parameters)
{
	using namespace ResolverCostTestData;
	for (const EShape Shape : { EShape::DistinctKills, EShape::Reversals, EShape::OneVictim })
	{
		// Arrange + Act
		const FShapeResult Result = Run(Shape);
		AddInfo(FString::Printf(TEXT("ResolverCost %s, %d pawns: median %.4f ms, max %.4f ms over %d frames (cold first frame %.4f ms); outcomes K%d R%d W%d; payload %d bytes per strike"),
			Name(Shape), PawnCount, Result.MedianMs, Result.MaxMs, MeasuredFrames, Result.ColdMs, Result.Kills, Result.Reversals, Result.Whiffs, Result.MaxPayloadBytes));

		// Assert: the shape resolved as intended, within the guardrails.
		switch (Shape)
		{
		case EShape::DistinctKills: TestEqual(TEXT("10 kills: ten Kill outcomes"), Result.Kills, AttacksPerFrame); break;
		case EShape::Reversals: TestEqual(TEXT("10 reversals: ten Reversal outcomes"), Result.Reversals, AttacksPerFrame); break;
		default: TestTrue(TEXT("10 on one victim: one kill, the rest find a falling body (no contact)"), Result.Kills == 1 && Result.Kills + Result.Whiffs == AttacksPerFrame); break;
		}
		TestTrue(*FString::Printf(TEXT("%s: median resolution of 10 Attacks with 50 pawns <= %.1f ms"), Name(Shape), BudgetMs), Result.MedianMs <= BudgetMs);
		TestTrue(*FString::Printf(TEXT("%s: per-strike payload <= %d bytes before property headers"), Name(Shape), PayloadBudgetBytes),
			Result.MaxPayloadBytes > 0 && Result.MaxPayloadBytes <= PayloadBudgetBytes);
	}
	return true;
}

#endif
