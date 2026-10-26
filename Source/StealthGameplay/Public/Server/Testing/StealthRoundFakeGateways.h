#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

#if WITH_STEALTH_TEST_SEAMS

#include "Server/Core/Round/StealthCrowdControl.h"
#include "Server/Core/Round/StealthRoundGateways.h"
#include "Server/Core/Round/StealthRoundTypes.h"

/**
 * Recording, strict fakes of Round's six gateways (GDD "Test seams (binding on implementation)" item 3; story
 * rmf-003, RMF-003 ruling 2026-10-25). Compiled only under WITH_STEALTH_TEST_SEAMS.
 *
 * Every fake appends to ONE shared, test-owned call-order log (FRoundFakeCallLog), so interleaved calls across
 * gateways are ordered by a single sequence, and keeps its own list too. Fakes are strict: every call consumes a
 * scripted answer queued with Script(); a call with nothing scripted is recorded as unexpected, answers a
 * neutral value, and FRoundFakeCallLog::Unexpected makes the test fail. Fakes record only what a call carried
 * (ids, enums, counts) and never a secret, a sub-seed, mailbox order or Event Log content: GetServerSubSeed
 * answers the scripted value without recording it.
 *
 * Usage:
 *   FRoundFakeCallLog Log;
 *   FRoundFakeGatewaySet Fakes(Log);
 *   Fakes.Session.Script(TEXT("RequestRoundTravel"), static_cast<int64>(ERoundTravelRequestResult::Accepted));
 *   FMySystemUnderTest Sut(Fakes.AsGateways());   // dependency injection, no singleton
 *   ... act ...
 *   TestTrue(TEXT("no unexpected call"), Log.Unexpected.IsEmpty());
 */

/** Which gateway a call went to. */
enum class ERoundFakeGateway : uint8
{
	Session,
	Roster,
	Tuning,
	CharacterPool,
	LevelSocket,
	Crowd,
};

/** One recorded call. */
struct FRoundFakeCall
{
	ERoundFakeGateway Gateway = ERoundFakeGateway::Session;
	FName Call;
	/** A summary of the arguments: ids, enums and counts only. */
	TArray<int64> Args;

	bool operator==(const FRoundFakeCall& Other) const { return Gateway == Other.Gateway && Call == Other.Call && Args == Other.Args; }
};

/** The shared, test-owned call-order log. */
struct FRoundFakeCallLog
{
	TArray<FRoundFakeCall> Calls;
	/** Calls that found no scripted answer (strict fakes): must be empty at the end of a test. */
	TArray<FRoundFakeCall> Unexpected;
};

/** Shared recording and scripting for every fake. */
class FRoundFakeGatewayBase
{
public:
	FRoundFakeGatewayBase(FRoundFakeCallLog& InLog, ERoundFakeGateway InGateway) : Log(InLog), Gateway(InGateway) {}

	/** Queues one scripted answer for Call (void calls script any value). Answers are consumed FIFO per call. */
	void Script(FName Call, int64 Answer = 0, int32 Times = 1)
	{
		TArray<int64>& Queue = Scripted.FindOrAdd(Call);
		for (int32 Index = 0; Index < Times; ++Index)
		{
			Queue.Add(Answer);
		}
	}

	/** This fake's own calls, in order (appended by const queries too). */
	mutable TArray<FRoundFakeCall> OwnCalls;

protected:
	/**
	 * Records the call (unless bRecord is false) and returns the scripted answer, or 0 and an unexpected entry.
	 * bScripted false: a state-answered query (the caller supplies the answer); nothing is consumed or flagged.
	 */
	int64 Record(FName Call, TArray<int64> Args, bool bRecord = true, bool bScripted = true) const
	{
		FRoundFakeCall Entry{ Gateway, Call, MoveTemp(Args) };
		if (bRecord)
		{
			Log.Calls.Add(Entry);
			OwnCalls.Add(Entry);
		}
		if (!bScripted)
		{
			return 0;
		}
		TArray<int64>* Queue = Scripted.Find(Call);
		if (Queue == nullptr || Queue->IsEmpty())
		{
			Log.Unexpected.Add(Entry);
			return 0;
		}
		const int64 Answer = (*Queue)[0];
		Queue->RemoveAt(0);
		return Answer;
	}

	FRoundFakeCallLog& Log;
	ERoundFakeGateway Gateway;
	mutable TMap<FName, TArray<int64>> Scripted;
};

class FRoundFakeSessionGateway : public FRoundFakeGatewayBase, public IRoundSessionGateway
{
public:
	explicit FRoundFakeSessionGateway(FRoundFakeCallLog& InLog) : FRoundFakeGatewayBase(InLog, ERoundFakeGateway::Session) {}

	virtual bool CanRequestRoundTravel() const override { return Record(TEXT("CanRequestRoundTravel"), {}) != 0; }
	virtual ERoundTravelRequestResult RequestRoundTravel(FName MapId, FName PresetId, uint32 RoundIndex) override
	{
		return static_cast<ERoundTravelRequestResult>(Record(TEXT("RequestRoundTravel"), { RoundIndex }));
	}
	virtual void RequestLobbyTravel() override { Record(TEXT("RequestLobbyTravel"), {}); }
	virtual void SetAdmissionMode(ERoundAdmissionMode Mode) override { Record(TEXT("SetAdmissionMode"), { static_cast<int64>(Mode) }); }
	virtual ERoundConnectionState GetConnectionState(FParticipantId Participant) const override
	{
		// A state set in ConnectionStates answers without a script (recorded all the same); otherwise strict.
		if (const ERoundConnectionState* Known = ConnectionStates.Find(Participant.Value))
		{
			Record(TEXT("GetConnectionState"), { Participant.Value }, /*bRecord*/ true, /*bScripted*/ false);
			return *Known;
		}
		return static_cast<ERoundConnectionState>(Record(TEXT("GetConnectionState"), { Participant.Value }));
	}

	/** Per-participant connection states answered by GetConnectionState without a script (D53 batch amendment). */
	TMap<uint32, ERoundConnectionState> ConnectionStates;
	virtual void KickParticipant(FParticipantId Participant) override { Record(TEXT("KickParticipant"), { Participant.Value }); }
	virtual uint64 GetServerSubSeed(ESeedDomain Domain, uint32 Attempt) const override
	{
		// Recorded without arguments: a sub-seed request names no secret, but its answer is never logged.
		return static_cast<uint64>(Record(TEXT("GetServerSubSeed"), { static_cast<int64>(Domain) }));
	}
};

class FRoundFakeRosterGateway : public FRoundFakeGatewayBase, public IRoundRosterGateway
{
public:
	explicit FRoundFakeRosterGateway(FRoundFakeCallLog& InLog) : FRoundFakeGatewayBase(InLog, ERoundFakeGateway::Roster) {}

	virtual ERosterResult BeginMatch(int32 TeamSize) override { return Result(TEXT("BeginMatch"), { TeamSize }); }
	virtual ERosterResult SetTeamSize(int32 TeamSize) override { return Result(TEXT("SetTeamSize"), { TeamSize }); }
	virtual ERosterResult SetTeamsLocked(bool bLocked) override { return Result(TEXT("SetTeamsLocked"), { bLocked ? 1 : 0 }); }
	virtual ERosterResult AssignTeams(TConstArrayView<FRoundTeamAssignment> Assignments) override
	{
		TArray<int64> Args;
		for (const FRoundTeamAssignment& A : Assignments)
		{
			Args.Add(A.Participant.Value);
			Args.Add(static_cast<int64>(A.Team));
		}
		const ERosterResult Answer = Result(TEXT("AssignTeams"), MoveTemp(Args));
		// An Applied batch is reflected in Members, so a following count query sees the new layout.
		if (Answer == ERosterResult::Applied)
		{
			for (const FRoundTeamAssignment& A : Assignments)
			{
				if (FRoundRosterMember* Member = Members.FindByPredicate([&A](const FRoundRosterMember& M) { return M.Participant == A.Participant; }))
				{
					Member->Team = A.Team;
				}
			}
		}
		return Answer;
	}
	virtual ERosterResult RemoveParticipant(FParticipantId Participant) override
	{
		const ERosterResult Answer = Result(TEXT("RemoveParticipant"), { Participant.Value });
		if (Answer == ERosterResult::Applied)
		{
			Members.RemoveAll([Participant](const FRoundRosterMember& M) { return M.Participant == Participant; });
		}
		return Answer;
	}
	virtual ERosterResult ResetAll(uint32 RoundIndex) override { return Result(TEXT("ResetAll"), { RoundIndex }); }
	virtual ERosterResult EndRoundAll() override { return Result(TEXT("EndRoundAll"), {}); }
	virtual ERosterResult RequestTransition(FParticipantId Participant, EParticipantState NewState, ERosterReason Reason,
		const FEpochToken& ExpectedEpoch, TOptional<uint32> ExpectedConnectionGeneration) override
	{
		return Result(TEXT("RequestTransition"), { Participant.Value, static_cast<int64>(NewState), static_cast<int64>(Reason),
			ExpectedEpoch.RoundEpoch, ExpectedConnectionGeneration.IsSet() ? static_cast<int64>(ExpectedConnectionGeneration.GetValue()) : -1 });
	}
	virtual ERosterResult BindPawn(FParticipantId Participant, const FPawnHandle& Pawn) override { return Result(TEXT("BindPawn"), { Participant.Value }); }
	virtual ERosterResult ReleaseToCrowd(const FPawnHandle& Pawn) override { return Result(TEXT("ReleaseToCrowd"), {}); }
	virtual void GetUnboundActiveParticipants(TArray<FParticipantId>& OutParticipants) const override
	{
		const int64 Count = Record(TEXT("GetUnboundActiveParticipants"), {});
		OutParticipants.Reset();
		for (int64 Index = 0; Index < Count; ++Index)
		{
			OutParticipants.Add(FParticipantId(static_cast<uint32>(Index)));
		}
	}
	virtual ERosterResult RecordAppearance(FParticipantId Participant, uint16 AppearanceId) override
	{
		return Result(TEXT("RecordAppearance"), { Participant.Value, AppearanceId });
	}
	virtual FEpochToken GetEpochToken() const override
	{
		return FEpochToken(0, static_cast<uint32>(Record(TEXT("GetEpochToken"), {})));
	}

	/** Answers Members (a layout the test sets) without a script; the query is recorded (D53 batch amendment). */
	virtual void GetRosteredParticipants(TArray<FRoundRosterMember>& OutMembers) const override
	{
		Record(TEXT("GetRosteredParticipants"), {}, /*bRecord*/ true, /*bScripted*/ false);
		OutMembers = Members;
	}

	/** The rostered layout GetRosteredParticipants answers (tests set it; Applied AssignTeams/RemoveParticipant update it). */
	TArray<FRoundRosterMember> Members;

	/** Sets Members to Team0 members on team 0 then Team1 on team 1, ids FirstId, FirstId + 1, ... */
	void SetLayout(int32 Team0, int32 Team1, uint32 FirstId = 1)
	{
		Members.Reset();
		uint32 Next = FirstId;
		for (int32 Index = 0; Index < Team0; ++Index) { Members.Add({ FParticipantId(Next++), ERosterTeam::Team0 }); }
		for (int32 Index = 0; Index < Team1; ++Index) { Members.Add({ FParticipantId(Next++), ERosterTeam::Team1 }); }
	}

	/** Members on Team. */
	int32 CountOn(ERosterTeam Team) const
	{
		return Members.FilterByPredicate([Team](const FRoundRosterMember& M) { return M.Team == Team; }).Num();
	}

private:
	ERosterResult Result(FName Call, TArray<int64> Args) { return static_cast<ERosterResult>(Record(Call, MoveTemp(Args))); }
};

class FRoundFakeTuningGateway : public FRoundFakeGatewayBase, public IRoundTuningGateway
{
public:
	explicit FRoundFakeTuningGateway(FRoundFakeCallLog& InLog) : FRoundFakeGatewayBase(InLog, ERoundFakeGateway::Tuning) {}

	virtual void SignalPreparingEntered() override { Record(TEXT("SignalPreparingEntered"), {}); }
	virtual ERoundPresetPreview PreviewPreset(FName PresetId) const override { return static_cast<ERoundPresetPreview>(Record(TEXT("PreviewPreset"), {})); }
};

class FRoundFakeCharacterPoolGateway : public FRoundFakeGatewayBase, public IRoundCharacterPoolGateway
{
public:
	explicit FRoundFakeCharacterPoolGateway(FRoundFakeCallLog& InLog) : FRoundFakeGatewayBase(InLog, ERoundFakeGateway::CharacterPool) {}

	virtual ERoundAllocateResult AllocateAppearances(TConstArrayView<FParticipantId> Players, int32 NpcCount) override
	{
		return static_cast<ERoundAllocateResult>(Record(TEXT("AllocateAppearances"), { Players.Num(), NpcCount }));
	}
	virtual uint16 GetAssignedAppearance(FParticipantId Participant) const override
	{
		return static_cast<uint16>(Record(TEXT("GetAssignedAppearance"), { Participant.Value }));
	}
	virtual void DiscardDeal() override { Record(TEXT("DiscardDeal"), {}); }
};

class FRoundFakeLevelSocketGateway : public FRoundFakeGatewayBase, public IRoundLevelSocketGateway
{
public:
	explicit FRoundFakeLevelSocketGateway(FRoundFakeCallLog& InLog) : FRoundFakeGatewayBase(InLog, ERoundFakeGateway::LevelSocket) {}

	virtual void RequestLayout(int32 Seed, uint64 TuningHash) override { Record(TEXT("RequestLayout"), { Seed }); }
	virtual ERoundPlacementResult RequestStartPlacement(TConstArrayView<FParticipantId> Team0, TConstArrayView<FParticipantId> Team1, int32 NpcCount) override
	{
		return static_cast<ERoundPlacementResult>(Record(TEXT("RequestStartPlacement"), { Team0.Num(), Team1.Num(), NpcCount }));
	}
};

class FRoundFakeCrowdControl : public FRoundFakeGatewayBase, public ICrowdControl
{
public:
	explicit FRoundFakeCrowdControl(FRoundFakeCallLog& InLog) : FRoundFakeGatewayBase(InLog, ERoundFakeGateway::Crowd) {}

	virtual void Halt() override { Record(TEXT("Halt"), {}); }
	virtual void DriveBoundPawn(AStealthPawn* Pawn) override { Record(TEXT("DriveBoundPawn"), { Pawn != nullptr ? 1 : 0 }); }
	virtual void ReleaseControl(AStealthPawn* Pawn) override { Record(TEXT("ReleaseControl"), { Pawn != nullptr ? 1 : 0 }); }
	virtual void AdoptReleasedPawn(AStealthPawn* Pawn, const FCrowdAdoptParams& Params) override
	{
		Record(TEXT("AdoptReleasedPawn"), { Pawn != nullptr ? 1 : 0, Params.bExitAtNextScheduledSlot ? 1 : 0 });
	}
};

/** Records every host-sink report (the host-only refusal channel, Round R5/R22). */
class FRoundFakeHostSink : public IRoundHostSink
{
public:
	TArray<FStartRefusal> StartRefusals;
	struct FCommandRefusal
	{
		ERoundHostCommand Command;
		ERoundCommandResult Result;
		uint8 Detail;
	};
	TArray<FCommandRefusal> CommandRefusals;

	virtual void OnStartRefused(const FStartRefusal& Refusal) override { StartRefusals.Add(Refusal); }
	virtual void OnCommandRefused(ERoundHostCommand Command, ERoundCommandResult Result, uint8 Detail) override { CommandRefusals.Add({ Command, Result, Detail }); }
};

/** All six fakes on one shared log, with the FRoundGateways bundle a plain Round core receives. */
class FRoundFakeGatewaySet
{
public:
	explicit FRoundFakeGatewaySet(FRoundFakeCallLog& InLog)
		: Session(InLog), Roster(InLog), Tuning(InLog), CharacterPool(InLog), LevelSocket(InLog), Crowd(InLog)
	{
	}

	FRoundGateways AsGateways()
	{
		FRoundGateways Gateways;
		Gateways.Session = &Session;
		Gateways.Roster = &Roster;
		Gateways.Tuning = &Tuning;
		Gateways.CharacterPool = &CharacterPool;
		Gateways.LevelSocket = &LevelSocket;
		Gateways.Crowd = &Crowd;
		return Gateways;
	}

	FRoundFakeSessionGateway Session;
	FRoundFakeRosterGateway Roster;
	FRoundFakeTuningGateway Tuning;
	FRoundFakeCharacterPoolGateway CharacterPool;
	FRoundFakeLevelSocketGateway LevelSocket;
	FRoundFakeCrowdControl Crowd;
};

#endif // WITH_STEALTH_TEST_SEAMS
