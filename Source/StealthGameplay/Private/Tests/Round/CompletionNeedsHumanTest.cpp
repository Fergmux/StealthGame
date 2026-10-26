// Story rmf-049 (production/epics/round-match-framework/story-049-completion-needs-human.md), GDD AC 20 / R13, R15, F2:
// a final completion by a human, Connected, Active completer gives ObjectiveWin (one WinLatched); a completer that is
// AI-driven, Disconnected, an Informant, or disconnected in the same tick gives Voided(FinalCompletion, code) with a
// distinct code per case and no outcome. The shared validity function is FRoundMatchCore::ValidateFinalCompleter
// (StealthRoundWin::ValidateCompleter); the stub Objectives resolver calls SubmitFinalCompletion in its slot. The
// same-tick case fixes the stated order (the disconnect first, earlier in the tick), as the story's Open item asks.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

#include "StealthRoundEndRig.h"

namespace CompletionNeedsHumanTestData
{
	enum class ECase : uint8
	{
		Human,
		AiDriven,
		Disconnected,
		Informant,
		DisconnectedSameTick,
	};

	/** The completer: participant 1 on team 0 of a 2v2 round. */
	constexpr uint32 Completer = 1;

	/** The Voided entry the case logged (the queue writes it for the voided completion slot: D0 HoldTimer, D1 the validity code), or null. */
	const FRoundLogEntry* FindCompletionVoid(const FRoundEndRig& R)
	{
		return R.Rig.EventLog.Entries.FindByPredicate([](const FRoundLogEntry& E)
		{
			return E.Type == ERoundLogType::Voided && E.Data[0] == static_cast<uint32>(EQueueItemType::HoldTimer);
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundCompletionNeedsHumanTest, "StealthGame.Round.CompletionNeedsHuman",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundCompletionNeedsHumanTest::RunTest(const FString& Parameters)
{
	using namespace CompletionNeedsHumanTestData;
	TSet<uint32> VoidCodes;
	for (const ECase Case : { ECase::Human, ECase::AiDriven, ECase::Disconnected, ECase::Informant, ECase::DisconnectedSameTick })
	{
		// Arrange.
		FRoundEndRig R;
		R.ToActive(2, 2);
		R.ScriptRoundEnd();
		FRoundCompleterControl Control;
		switch (Case)
		{
		case ECase::AiDriven:
			// An Unresponsive hand-over: the participant is still Connected, the bound pawn is AI-driven (R17.1).
			R.Rig.Core->OnParticipantUnresponsive(FParticipantId(Completer), R.Rig.Clock.GetServerTimeSeconds());
			break;
		case ECase::Disconnected:
			R.Disconnect(Completer);
			R.Frame();
			break;
		case ECase::Informant:
			R.Rig.Fakes.Roster.SetState(FParticipantId(Completer), EParticipantState::Informant);
			break;
		default:
			break;
		}
		const int32 Latched = R.CountLogged(ERoundLogType::WinLatched);

		// Act: the completion item; for the same-tick case the disconnect lands earlier in the completion's tick.
		R.Queue->Submit(FMailboxId::ServerLocal(), R.Completion(1, Completer, 0, Control));
		R.Rig.Clock.Advance(RoundTestFixtures::RigFrameSec, 1);
		if (Case == ECase::DisconnectedSameTick)
		{
			R.Disconnect(Completer);
		}
		R.Rig.Core->Tick();
		R.Queue->Drain();

		// Assert.
		const TCHAR* Name = Case == ECase::Human ? TEXT("human") : Case == ECase::AiDriven ? TEXT("AI-driven") : Case == ECase::Disconnected ? TEXT("Disconnected")
			: Case == ECase::Informant ? TEXT("Informant") : TEXT("disconnected in the same tick");
		if (Case == ECase::Human)
		{
			TestTrue(TEXT("human, Connected, Active: ObjectiveWin"), R.Rig.Core->IsTerminated() && R.Rig.Core->GetLatch().GetOutcome().EndReason == ERoundEndReason::ObjectiveWin);
			TestEqual(TEXT("human: one WinLatched"), R.CountLogged(ERoundLogType::WinLatched) - Latched, 1);
			TestTrue(TEXT("human: no FinalCompletion void"), FindCompletionVoid(R) == nullptr);
		}
		else
		{
			const FRoundLogEntry* Void = FindCompletionVoid(R);
			TestNotNull(FString::Printf(TEXT("%s: Voided(FinalCompletion, code) logged"), Name), Void);
			if (Void != nullptr)
			{
				VoidCodes.Add(Void->Data[1]);
				TestTrue(FString::Printf(TEXT("%s: the code is a failure code"), Name), Void->Data[1] != static_cast<uint32>(ERoundCompletionValidity::Valid));
			}
			TestFalse(FString::Printf(TEXT("%s: no outcome"), Name), R.Rig.Core->IsTerminated());
			TestEqual(FString::Printf(TEXT("%s: no WinLatched"), Name), R.CountLogged(ERoundLogType::WinLatched) - Latched, 0);
			TestTrue(FString::Printf(TEXT("%s: the round stays Active"), Name), R.Rig.Core->GetPhase() == ERoundPhase::Active);
		}
	}
	TestEqual(TEXT("a distinct code per failing case"), VoidCodes.Num(), 4);
	TestTrue(TEXT("the AI-driven code"), VoidCodes.Contains(static_cast<uint32>(ERoundCompletionValidity::CompleterAiDriven)));
	TestTrue(TEXT("the Disconnected code"), VoidCodes.Contains(static_cast<uint32>(ERoundCompletionValidity::CompleterDisconnected)));
	TestTrue(TEXT("the Informant code"), VoidCodes.Contains(static_cast<uint32>(ERoundCompletionValidity::CompleterInformant)));
	TestTrue(TEXT("the same-tick code"), VoidCodes.Contains(static_cast<uint32>(ERoundCompletionValidity::CompleterDisconnectedThisTick)));

	// The control-epoch clause (R13): a hold whose pawn's control epoch changed since its Begin is not human-controlled.
	FRoundCompleterFacts Facts;
	Facts.bRostered = Facts.bOnTeam = Facts.bConnected = true;
	Facts.State = EParticipantState::Active;
	Facts.Control.BeginControlEpoch = 1;
	Facts.Control.CurrentControlEpoch = 2;
	TestTrue(TEXT("a control change since Begin: AI-driven"), StealthRoundWin::ValidateCompleter(Facts) == ERoundCompletionValidity::CompleterAiDriven);
	Facts.Control.CurrentControlEpoch = 1;
	Facts.Control.bHandOverPending = true;
	TestTrue(TEXT("a pending hand-over: AI-driven"), StealthRoundWin::ValidateCompleter(Facts) == ERoundCompletionValidity::CompleterAiDriven);
	Facts.Control.bHandOverPending = false;
	TestTrue(TEXT("unchanged epoch, no hand-over: Valid"), StealthRoundWin::ValidateCompleter(Facts) == ERoundCompletionValidity::Valid);
	return true;
}

#endif
