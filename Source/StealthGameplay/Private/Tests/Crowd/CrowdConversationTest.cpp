// Story cnb-015 (production/epics/crowd-npc-behaviour/story-015-conversation-and-turn-in-place.md): conversation (C3) and
// turn-in-place (C6), partner-blind (GDD CNB-R5 C3, C6, CNB-R6 G5, AC-CNB-17; ADR-0001 identity blindness).
//   PartnerBlind      AC-CNB-17a: an NPC comes to rest within 150 cm of a resting partner 1,236 times with a scripted
//                     player-like pawn and 1,236 times with an NPC in the same spot (same facts): the face-toward rates
//                     differ by at most 5 points (TOST equivalence at alpha 0.05). The decision is one counter-based draw
//                     that never reads the neighbour's kind, so the paired trials decide identically.
//   FormationRules    AC-CNB-17b: a conversation forms only while both face each other within 60 degrees; an unanswered
//                     initiator turns away after U(2, 5) s for either partner kind; no NPC starts a leg toward a
//                     stationary pawn to start one (the decision only turns the head).
//   LengthsAndLook    over >= 100 conversations of the reference scene: lengths in U(6, 20) s and the mean partner-look
//                     share (G5) in [0.60, 0.80].
//   TurnInPlace       C6: idle gaze beyond 70 degrees for 0.5 s re-centres the body until the head yaw is at most
//                     TurnInPlaceExitFrac x LookYawLimitDeg (the SCL model every pawn shares).
// The story names a harness test (CrowdConversationBlindTest in StealthTestHarness) for AC-CNB-17a: 2,472 PIE trials
// are impractical per commit, so the trials run headless here against the same public facts (story deviation).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/Crowd/CrowdSimWorld.h"

namespace CrowdConversationTestData
{
	struct FConverseRecorder final : public ICrowdAgentObserver
	{
		uint32 Subject = MAX_uint32;
		int32 Decisions = 0;
		int32 Faced = 0;
		bool bReplyBranch = false;
		double DecisionTime = -1.0;
		/** The decision that turned toward the neighbour (a later rest may decide again): the turn-away clock's start. */
		double TurnDecisionTime = -1.0;
		double TurnAwayAfter = -1.0;
		int32 LegsAfterDecision = 0;
		virtual void OnConverseDecision(uint32 Ordinal, double Now, uint64, bool bReply, bool bDecided) override
		{
			if (Ordinal == Subject && Decisions == 0)
			{
				++Decisions;
				Faced += bDecided ? 1 : 0;
				bReplyBranch = bReply;
				DecisionTime = Now;
			}
			if (Ordinal == Subject && bDecided && TurnDecisionTime < 0.0)
			{
				TurnDecisionTime = Now;
			}
		}
		virtual void OnConversationEnded(uint32 Ordinal, double Now, uint64, double, bool bTurnedAway) override
		{
			if (Ordinal == Subject && bTurnedAway && TurnAwayAfter < 0.0 && TurnDecisionTime >= 0.0)
			{
				TurnAwayAfter = Now - TurnDecisionTime;
			}
		}
		virtual void OnLegStarted(uint32 Ordinal, double Now, float, double) override
		{
			LegsAfterDecision += Ordinal == Subject && DecisionTime >= 0.0 && TurnAwayAfter < 0.0 ? 1 : 0;
		}
	};

	/**
	 * One trial: the subject NPC (ordinal Trial * 4) walks 6 m and comes to rest 110 cm from a partner that stands facing
	 * away; bNpcPartner chooses an NPC in a long pause or a scripted pawn with the same facts.
	 */
	FConverseRecorder RunTrial(int32 Trial, bool bNpcPartner)
	{
		using namespace StealthCrowdSim;
		FConfig Config;
		Config.NpcCount = 2;
		Config.FirstNpcOrdinal = static_cast<uint32>(Trial) * 4;
		FWorld World(Config);
		FConverseRecorder Recorder;
		World.SetObserver(&Recorder);
		const FVector PartnerAt(700.0, 0.0, 0.0);
		const FVector RestAt(590.0, 0.0, 0.0);
		bool bGoal = false;
		World.SetRouteFixture([&bGoal, RestAt](uint32 Ordinal, const FVector&, FVector& OutGoal)
		{
			if (bGoal)
			{
				return false;
			}
			bGoal = true;
			OutGoal = RestAt;
			return true;
		});
		FNpc& Subject = World.SpawnNpcAt(FVector(0.0, 0.0, 0.0), 0.0f);
		Recorder.Subject = Subject.Body.Ordinal;
		FCrowdFirstActivity Walk;
		Walk.LegCm = 590.0;
		World.OverrideFirstActivity(Subject.Body.Ordinal, Walk);
		// A view the partner's long pause faces (so the resting partner holds its contextual look and presents the same
		// still, facing-away facts as the scripted pawn: no glance turns it toward the newcomer).
		World.AddContext({ FVector(1000.0, 0.0, 0.0), false });
		// The partner faces away (+X), so the subject's choice is the initiator's draw, not a reply.
		if (bNpcPartner)
		{
			FNpc& Partner = World.SpawnNpcAt(PartnerAt, 0.0f);
			FCrowdFirstActivity Rest;
			Rest.bPause = true;
			Rest.PauseSec = 60.0;
			World.OverrideFirstActivity(Partner.Body.Ordinal, Rest);
		}
		else
		{
			World.AddScripted(PartnerAt, 0.0f);
		}
		World.StartRound();
		for (int32 Frame = 0; Frame < 60 * 16 && !(Recorder.Decisions > 0 && (Recorder.TurnAwayAfter >= 0.0 || World.GetNow() - (Recorder.TurnDecisionTime >= 0.0 ? Recorder.TurnDecisionTime : Recorder.DecisionTime) > 6.0)); ++Frame)
		{
			World.Step();
		}
		return Recorder;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdConversationPartnerBlindTest, "StealthGame.Crowd.Conversation.PartnerBlind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdConversationPartnerBlindTest::RunTest(const FString& Parameters)
{
	using namespace CrowdConversationTestData;
	constexpr int32 Trials = 1236;
	int32 FacedPlayer = 0, FacedNpc = 0, DecidedPlayer = 0, DecidedNpc = 0, Mismatch = 0, Replies = 0;
	TArray<double> TurnAways;
	int32 LegsTowardPartner = 0;
	for (int32 Trial = 0; Trial < Trials; ++Trial)
	{
		const FConverseRecorder WithPlayer = RunTrial(Trial, false);
		const FConverseRecorder WithNpc = RunTrial(Trial, true);
		DecidedPlayer += WithPlayer.Decisions;
		DecidedNpc += WithNpc.Decisions;
		FacedPlayer += WithPlayer.Faced;
		FacedNpc += WithNpc.Faced;
		Replies += (WithPlayer.bReplyBranch ? 1 : 0) + (WithNpc.bReplyBranch ? 1 : 0);
		Mismatch += WithPlayer.Faced != WithNpc.Faced ? 1 : 0;
		if (WithPlayer.TurnAwayAfter >= 0.0)
		{
			TurnAways.Add(WithPlayer.TurnAwayAfter);
		}
		LegsTowardPartner += WithPlayer.LegsAfterDecision + WithNpc.LegsAfterDecision;
	}
	const double RatePlayer = DecidedPlayer > 0 ? static_cast<double>(FacedPlayer) / DecidedPlayer : 0.0;
	const double RateNpc = DecidedNpc > 0 ? static_cast<double>(FacedNpc) / DecidedNpc : 0.0;
	// TOST at alpha 0.05 with a 5-point margin: the 90 % interval of the difference lies inside (-0.05, 0.05).
	const double Se = FMath::Sqrt(RatePlayer * (1.0 - RatePlayer) / FMath::Max(DecidedPlayer, 1) + RateNpc * (1.0 - RateNpc) / FMath::Max(DecidedNpc, 1));
	const double Diff = RatePlayer - RateNpc;
	const bool bEquivalent = Diff - 1.645 * Se > -0.05 && Diff + 1.645 * Se < 0.05;
	double MinTurn = 1e9, MaxTurn = 0.0;
	for (const double Turn : TurnAways)
	{
		MinTurn = FMath::Min(MinTurn, Turn);
		MaxTurn = FMath::Max(MaxTurn, Turn);
	}
	AddInfo(FString::Printf(TEXT("CONVERSE face-toward: player-like %.4f (%d/%d), NPC %.4f (%d/%d), diff %.4f, 90 %% CI +-%.4f, paired mismatches %d, reply-branch decisions %d; turn-away after %.2f-%.2f s (%d)"),
		RatePlayer, FacedPlayer, DecidedPlayer, RateNpc, FacedNpc, DecidedNpc, Diff, 1.645 * Se, Mismatch, Replies, MinTurn, MaxTurn, TurnAways.Num()));
	TestTrue(TEXT("every trial reached a decision"), DecidedPlayer == Trials && DecidedNpc == Trials);
	TestTrue(TEXT("the face-toward rates are equivalent within 5 points (TOST, AC-CNB-17a)"), bEquivalent);
	TestEqual(TEXT("the paired trials decide identically (no kind input)"), Mismatch, 0);
	TestTrue(TEXT("the rate is CrowdConverseStartPct"), FMath::Abs(RatePlayer * 100.0 - FCrowdTuning_Server().CrowdConverseStartPct) < 4.0);
	TestTrue(TEXT("an unanswered initiator turns away after U(2, 5) s (AC-CNB-17b)"), TurnAways.Num() > 300 && MinTurn >= 2.0 - 1e-6 && MaxTurn <= 5.0 + 1.0 / 60.0 + 1e-6);
	TestEqual(TEXT("no leg starts between the decision and the turn-away (never walking toward a stationary pawn)"), LegsTowardPartner, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdConversationLengthsAndLookTest, "StealthGame.Crowd.Conversation.LengthsAndLook",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdConversationLengthsAndLookTest::RunTest(const FString& Parameters)
{
	using namespace StealthCrowdSim;
	struct FRecorder final : public ICrowdAgentObserver
	{
		FWorld* World = nullptr;
		TArray<double> Lengths;
		TArray<double> Shares;
		int32 FormedNotMutual = 0;
		virtual void OnConversationFormed(uint32 Ordinal, double Now, uint64 PartnerKey, double LengthSec) override
		{
			Lengths.Add(LengthSec);
			// Formation needs both facing each other within CrowdConverseFaceDeg (the public body yaw).
			const FNpc* Self = World->FindNpc(Ordinal);
			const FCrowdPerceivedPawn* Partner = World->GetHash().Find(PartnerKey);
			if (Self != nullptr && Partner != nullptr)
			{
				const float Face = World->GetCrowdTuning().CrowdConverseFaceDeg;
				const float SelfToPartner = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Partner->Location.Y - Self->Body.Location.Y, Partner->Location.X - Self->Body.Location.X)));
				const float PartnerToSelf = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Self->Body.Location.Y - Partner->Location.Y, Self->Body.Location.X - Partner->Location.X)));
				const bool bMutual = FMath::Abs(StealthLocomotion::DeltaYawDeg(Self->Body.Motion.FacingYawDeg, SelfToPartner)) <= Face + 1.0f
					&& FMath::Abs(StealthLocomotion::DeltaYawDeg(Partner->FacingYawDeg, PartnerToSelf)) <= Face + 1.0f;
				FormedNotMutual += bMutual ? 0 : 1;
			}
		}
		virtual void OnConversationEnded(uint32 Ordinal, double Now, uint64 PartnerKey, double PartnerLookShare, bool bTurnedAway) override
		{
			if (PartnerLookShare > 0.0)
			{
				Shares.Add(PartnerLookShare);
			}
		}
	};
	FConfig Config;
	// A denser square so resting neighbours are common (the reference crowd on a smaller plaza).
	Config.HalfSizeCm = 1500.0;
	Config.NodeSpacingCm = 300.0;
	FWorld World(Config);
	FRecorder Recorder;
	Recorder.World = &World;
	World.SetObserver(&Recorder);
	World.SpawnNpcs();
	World.StartRound();
	World.RunUntil(30.0 * 60.0);
	double MinLength = 1e9, MaxLength = 0.0, ShareSum = 0.0;
	for (const double Length : Recorder.Lengths)
	{
		MinLength = FMath::Min(MinLength, Length);
		MaxLength = FMath::Max(MaxLength, Length);
	}
	for (const double Share : Recorder.Shares)
	{
		ShareSum += Share;
	}
	const double MeanShare = Recorder.Shares.Num() > 0 ? ShareSum / Recorder.Shares.Num() : 0.0;
	AddInfo(FString::Printf(TEXT("CONVERSATIONS %d formed, lengths %.2f-%.2f s, %d with a look share, mean partner-look share %.3f, formed without mutual facing %d"),
		Recorder.Lengths.Num(), MinLength, MaxLength, Recorder.Shares.Num(), MeanShare, Recorder.FormedNotMutual));
	TestTrue(TEXT(">= 100 conversations"), Recorder.Lengths.Num() >= 100);
	TestTrue(TEXT("lengths in U(6, 20) s"), MinLength >= Config.Tuning.CrowdConversationMinSec && MaxLength <= Config.Tuning.CrowdConversationMaxSec);
	TestTrue(TEXT("the mean partner-look share lies in [0.60, 0.80] (G5)"), MeanShare >= 0.60 && MeanShare <= 0.80);
	TestEqual(TEXT("a conversation forms only while both face each other (AC-CNB-17b)"), Recorder.FormedNotMutual, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdTurnInPlaceTest, "StealthGame.Crowd.Conversation.TurnInPlace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrowdTurnInPlaceTest::RunTest(const FString& Parameters)
{
	const FLocomotionTuning_Public Locomotion;
	FStealthMotionState State;
	State.FacingYawDeg = 0.0f;
	FStealthMoveInput Input;
	Input.bHasLookTarget = true;
	Input.LookTargetYawDeg = 110.0f; // idle gaze 110 degrees off the body (a partner behind the shoulder)
	double TurnStarted = -1.0;
	for (int32 Frame = 0; Frame < 60 * 4; ++Frame)
	{
		const float Before = State.FacingYawDeg;
		FStealthLocomotionModel::Step(Locomotion, Input, 1.0f / 60.0f, State);
		if (TurnStarted < 0.0 && State.FacingYawDeg != Before)
		{
			TurnStarted = Frame / 60.0;
		}
	}
	const float HeadYaw = FMath::Abs(StealthLocomotion::DeltaYawDeg(State.FacingYawDeg, Input.LookTargetYawDeg));
	AddInfo(FString::Printf(TEXT("TURNINPLACE re-centre started after %.2f s, final head yaw %.1f deg (exit %.1f)"), TurnStarted, HeadYaw, Locomotion.TurnInPlaceExitFrac * Locomotion.LookYawLimitDeg));
	TestTrue(TEXT("the body re-centres after about 0.5 s beyond 70 degrees (C6)"), TurnStarted >= Locomotion.TurnInPlaceDelaySec - 1.0 / 60.0 && TurnStarted <= Locomotion.TurnInPlaceDelaySec + 0.1);
	TestTrue(TEXT("until the head yaw is at most TurnInPlaceExitFrac x LookYawLimitDeg"), HeadYaw <= Locomotion.TurnInPlaceExitFrac * Locomotion.LookYawLimitDeg + 0.5f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
