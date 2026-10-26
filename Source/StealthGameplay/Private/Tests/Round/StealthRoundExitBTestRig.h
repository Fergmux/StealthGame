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
		Params.NpcCount = RoundTestFixtures::PreparingRigNpcCount;
		return Params;
	}

	explicit FRoundExitBRig(const FRoundRuleParams& Params = DefaultParams())
	{
		Fakes.Spawner.Clock = &Clock;
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
		// R6 P2 at Preparing entry (the Preparing batch): "Preparing entered" reaches TDC.
		Fakes.Tuning.Script(TEXT("SignalPreparingEntered"));
	}

	/** Scripts the forced Active-entry step (EnterActive): an empty unbound set and the one Crowd start. */
	void ScriptActiveEntry()
	{
		Fakes.Session.Script(TEXT("SetAdmissionMode"));
		Fakes.Roster.Script(TEXT("GetUnboundActiveParticipants"), 0);
		Fakes.Crowd.Script(TEXT("StartRound"));
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
		ScriptActiveEntry();
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

	// --- The Preparing sequence (stories rmf-031..042) -------------------------------------------------------------

	/** Start accepted and travel accepted with Seed: Preparing, "Preparing entered" signalled. */
	void EnterPreparing(int32 Seed = RoundTestFixtures::PreparingRigSeed)
	{
		ScriptStartToTravel();
		Core->Start(HostCaller());
		Fakes.Session.Script(TEXT("SetAdmissionMode"));
		Core->NotifyRoundTravelAccepted(Seed);
	}

	/** Scripts the strict calls of P3 (layout request, the deal) and P6 (placement, appearances, sub-seed, epoch). */
	void ScriptPreparingSequence()
	{
		const int32 Players = Fakes.Roster.Members.Num();
		Fakes.LevelSocket.Script(TEXT("RequestLayout"));
		Fakes.CharacterPool.Script(TEXT("AllocateAppearances"), static_cast<int64>(ERoundAllocateResult::Ok));
		Fakes.LevelSocket.Script(TEXT("RequestStartPlacement"), static_cast<int64>(ERoundPlacementResult::Ok));
		Fakes.CharacterPool.Script(TEXT("GetAssignedAppearance"), 0, Players);
		Fakes.Roster.Script(TEXT("RecordAppearance"), static_cast<int64>(ERosterResult::Applied), Players);
		Fakes.Session.Script(TEXT("GetServerSubSeed"), static_cast<int64>(RoundTestFixtures::PreparingRigSpawnSubSeed));
		Fakes.Roster.Script(TEXT("GetEpochToken"), RoundTestFixtures::FixtureRoundEpoch);
	}

	/** An acknowledgement from Participant under this Preparing's nonce and its current generation. */
	FRoundTravelAck MakeAck(FParticipantId Participant) const
	{
		FRoundTravelAck Ack;
		Ack.Participant = Participant;
		Ack.Epoch = Core->GetPreparing().GetAckEpoch();
		const uint32* Generation = Fakes.Session.Generations.Find(Participant.Value);
		Ack.ConnectionGeneration = Generation != nullptr ? *Generation : 0;
		return Ack;
	}

	/** Every Connected member acknowledges (travel complete, or the world-ready stage); Except is skipped. */
	void AckAll(bool bWorldReady, FParticipantId Except = FParticipantId())
	{
		for (const FRoundRosterMember& Member : Fakes.Roster.Members)
		{
			const ERoundConnectionState* State = Fakes.Session.ConnectionStates.Find(Member.Participant.Value);
			if (Member.Participant == Except || State == nullptr || *State != ERoundConnectionState::Connected)
			{
				continue;
			}
			if (bWorldReady)
			{
				Core->NotifyParticipantWorldReady(MakeAck(Member.Participant));
			}
			else
			{
				Core->NotifyParticipantTravelComplete(MakeAck(Member.Participant));
			}
		}
	}

	/** One server frame: the clock advances (time and tick), then the Round tick. */
	void Frame(double Seconds = RoundTestFixtures::RigFrameSec)
	{
		Clock.Advance(Seconds, 1);
		Core->Tick();
	}

	/** From Preparing (EnterPreparing done): tuning, layout, acks, P6 in one frame, world-ready, P7 and Active entry. */
	void RunPreparingToActive()
	{
		ScriptPreparingSequence();
		Core->NotifyTuningReady(RoundTestFixtures::PreparingRigTuningHash);
		Core->NotifyLayoutReady(1, Core->GetPreparing().GetSeed());
		AckAll(/*bWorldReady*/ false);
		Frame(); // P5 satisfied: P6 spawns every pawn
		AckAll(/*bWorldReady*/ true);
		Frame(); // P7: ActiveAtServerTime = Now + StartLeadSec
		ScriptActiveEntry();
		Frame(Core->GetParams().Preparing.StartLeadSec); // the first tick at or after ActiveAtServerTime
	}

	/** The current call count (a mark for CallsSince). */
	int32 Mark() const { return Log.Calls.Num(); }

	/** True for a side-effect-free query (the call-order spy rule: only mutating calls are compared). */
	static bool IsQuery(FName Call)
	{
		static const FName Queries[] = { TEXT("CanRequestRoundTravel"), TEXT("GetConnectionState"), TEXT("GetRosteredParticipants"),
			TEXT("PreviewPreset"), TEXT("GetEpochToken"), TEXT("GetServerSubSeed"), TEXT("GetUnboundActiveParticipants"), TEXT("GetAssignedAppearance"),
			TEXT("GetConnectionGeneration"), TEXT("GetPlacementZones") };
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
