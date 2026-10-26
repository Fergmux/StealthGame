#pragma once

// Shared rig of the Round Exit B Logic tests (stories rmf-018..028, D53 integration batch): FRoundMatchCore over the
// recording strict fakes (story rmf-003), a host-sink spy, the fake clock (rmf-001) and a recording Event Log writer.
// Fixture literals come from Server/Testing/StealthRoundTestConstants.h (rmf-005); no fixture code string is quoted.

#include "CoreMinimal.h"

#if WITH_STEALTH_TEST_SEAMS

#include "Server/Core/Round/StealthRoundMatchCore.h"
#include "Server/Testing/StealthFakeClock.h"
#include "Server/Testing/StealthQueueTestRig.h"
#include "Server/Testing/StealthRoundFakeGateways.h"
#include "Server/Testing/StealthRoundTestConstants.h"

struct FRoundExitBRig
{
	/** The participant id of the host in every fixture (the first registrant). */
	static constexpr uint32 HostIdValue = 1;
	static constexpr double ClockStart = 100.0;

	FRoundFakeCallLog Log;
	FRoundFakeGatewaySet Fakes{ Log };
	FRoundFakeHostSink Host;
	FFakeStealthClock Clock{ ClockStart };
	FStealthRecordingLogWriter EventLog;
	TUniquePtr<FRoundMatchCore> Core;

	static FRoundRuleParams DefaultParams()
	{
		FRoundRuleParams Params;
		Params.TeamSize = RoundTestFixtures::FixtureTeamSize;
		Params.MinPlayersPerTeamCount = 1;
		Params.MaxTeamImbalanceCount = 1;
		Params.EndedMinDwellSec = RoundTestFixtures::EndedMinDwellSec;
		return Params;
	}

	explicit FRoundExitBRig(const FRoundRuleParams& Params = DefaultParams())
	{
		Core = MakeUnique<FRoundMatchCore>(Fakes.AsGateways(), &Host, &EventLog, &Clock);
		Core->SetParams(Params);
	}

	static FRoundCommandCaller HostCaller() { return FRoundCommandCaller::ServerAsHost(FParticipantId(HostIdValue)); }

	/** BeginMatch + Unknown -> Lobby (one admission push). */
	void OpenLobby()
	{
		Fakes.Roster.Script(TEXT("BeginMatch"), static_cast<int64>(ERosterResult::Applied));
		Fakes.Session.Script(TEXT("SetAdmissionMode"));
		Core->OpenMatch();
	}

	/** Sets the layout and marks every member Connected. */
	void SetLayout(int32 Team0, int32 Team1)
	{
		Fakes.Roster.SetLayout(Team0, Team1, HostIdValue);
		for (const FRoundRosterMember& Member : Fakes.Roster.Members)
		{
			Fakes.Session.ConnectionStates.Add(Member.Participant.Value, ERoundConnectionState::Connected);
		}
	}

	/** Scripts the gateway answers of one Start that reaches step 8 (Travel answers Accepted or Refused). */
	void ScriptStartToTravel(ERoundTravelRequestResult Travel = ERoundTravelRequestResult::Accepted)
	{
		Fakes.Tuning.Script(TEXT("PreviewPreset"), static_cast<int64>(ERoundPresetPreview::Unavailable));
		Fakes.Session.Script(TEXT("CanRequestRoundTravel"), 1);
		Fakes.Session.Script(TEXT("SetAdmissionMode"), 0, 2);
		Fakes.Roster.Script(TEXT("SetTeamsLocked"), static_cast<int64>(ERosterResult::Applied), 2);
		Fakes.Roster.Script(TEXT("ResetAll"), static_cast<int64>(ERosterResult::Applied));
		Fakes.Session.Script(TEXT("RequestRoundTravel"), static_cast<int64>(Travel));
	}

	/** Drives a valid 2v2 match from Lobby to Phase (Lobby, Preparing, Active, Ended or Reset). */
	void DriveTo(ERoundPhase Phase)
	{
		if (Phase == ERoundPhase::Lobby)
		{
			return;
		}
		ScriptStartToTravel();
		Core->Start(HostCaller());
		Fakes.Session.Script(TEXT("SetAdmissionMode"));
		Core->NotifyRoundTravelAccepted();
		if (Phase == ERoundPhase::Preparing)
		{
			return;
		}
		Fakes.Session.Script(TEXT("SetAdmissionMode"));
		Core->EnterActive();
		if (Phase == ERoundPhase::Active)
		{
			return;
		}
		Fakes.Session.Script(TEXT("SetAdmissionMode"));
		Core->EndRound(0, 1);
		if (Phase == ERoundPhase::Ended)
		{
			return;
		}
		Clock.AdvanceSeconds(RoundTestFixtures::EndedMinDwellSec + 1.0);
		Fakes.Session.Script(TEXT("SetAdmissionMode"));
		Core->ExecuteHostCommand(HostCaller(), ERoundHostCommand::Reset);
	}

	/** The current call count (a mark for CallsSince). */
	int32 Mark() const { return Log.Calls.Num(); }

	/** True for a side-effect-free query (the call-order spy rule: only mutating calls are compared). */
	static bool IsQuery(FName Call)
	{
		static const FName Queries[] = { TEXT("CanRequestRoundTravel"), TEXT("GetConnectionState"), TEXT("GetRosteredParticipants"),
			TEXT("PreviewPreset"), TEXT("GetEpochToken"), TEXT("GetServerSubSeed"), TEXT("GetUnboundActiveParticipants"), TEXT("GetAssignedAppearance") };
		for (const FName& Query : Queries)
		{
			if (Call == Query)
			{
				return true;
			}
		}
		return false;
	}

	/** Calls recorded after Mark, optionally the mutating ones only. */
	TArray<FRoundFakeCall> CallsSince(int32 InMark, bool bMutatingOnly = true) const
	{
		TArray<FRoundFakeCall> Out;
		for (int32 Index = InMark; Index < Log.Calls.Num(); ++Index)
		{
			if (!bMutatingOnly || !IsQuery(Log.Calls[Index].Call))
			{
				Out.Add(Log.Calls[Index]);
			}
		}
		return Out;
	}

	/** Number of recorded calls named Call after Mark. */
	int32 CountSince(int32 InMark, FName Call) const
	{
		int32 Count = 0;
		for (int32 Index = InMark; Index < Log.Calls.Num(); ++Index)
		{
			Count += Log.Calls[Index].Call == Call ? 1 : 0;
		}
		return Count;
	}
};

#endif // WITH_STEALTH_TEST_SEAMS
