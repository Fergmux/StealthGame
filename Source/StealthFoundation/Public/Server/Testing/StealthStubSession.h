#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

#if WITH_STEALTH_TEST_SEAMS

/**
 * Test-only Session & Travel stub for the TDC acceptance tests (ADR-0008 section 4; story tdc-052). It
 * stands in for Session's single round acknowledgement (CR-9, Session rule 11.6) until the Session &
 * Travel epic lands. Not a production API. Compiled only under WITH_STEALTH_TEST_SEAMS.
 *
 * Participants are keyed by ParticipantId (never by controller or PlayerState, CR-9). Every
 * acknowledgement is recorded in order; a second acknowledgement from the same participant is recorded as
 * a second one, never merged. An acknowledgement is accepted when its ClientHash equals the expected hash
 * the test set for the epoch; a deadline records a rejected acknowledgement. AcceptCommand(participant)
 * returns false until that participant's tuning is valid (an accepted acknowledgement) and true afterwards.
 * RejoinWindowSec belongs to the Session GDD and is not in the TDC inputs: the test supplies it.
 */

/** One recorded acknowledgement. */
struct FStealthStubAck
{
	int32 ParticipantId = -1;
	uint64 ClientHash = 0;
	bool bAccepted = false;
	/** True when the record is the deadline path (no echo arrived in time). */
	bool bDeadline = false;

	bool operator==(const FStealthStubAck& Other) const
	{
		return ParticipantId == Other.ParticipantId && ClientHash == Other.ClientHash && bAccepted == Other.bAccepted && bDeadline == Other.bDeadline;
	}
};

class FStubSession
{
public:
	/** The server's ClientHash for the current epoch; echoes are compared with it. */
	void SetExpectedClientHash(uint64 InHash) { ExpectedClientHash = InHash; }

	/** Records one acknowledgement echo; returns whether it was accepted. An accepted echo makes the participant's tuning valid. */
	bool Acknowledge(int32 ParticipantId, uint64 ClientHash)
	{
		FStealthStubAck& Ack = Acks.AddDefaulted_GetRef();
		Ack.ParticipantId = ParticipantId;
		Ack.ClientHash = ClientHash;
		Ack.bAccepted = ClientHash == ExpectedClientHash;
		if (Ack.bAccepted)
		{
			ValidParticipants.Add(ParticipantId);
		}
		return Ack.bAccepted;
	}

	/** The deadline path: records a rejected acknowledgement for a participant that never echoed. */
	void Deadline(int32 ParticipantId)
	{
		FStealthStubAck& Ack = Acks.AddDefaulted_GetRef();
		Ack.ParticipantId = ParticipantId;
		Ack.bDeadline = true;
	}

	/** True once the participant has an accepted acknowledgement for this epoch. */
	bool IsAccepted(int32 ParticipantId) const { return ValidParticipants.Contains(ParticipantId); }

	/** The command gate: false until the participant's tuning is valid, true afterwards. */
	bool AcceptCommand(int32 ParticipantId) const { return IsAccepted(ParticipantId); }

	/** A new epoch clears every participant's tuning-ready state (CR-9); the record of acknowledgements stays. */
	void BeginEpoch(uint64 InExpectedClientHash)
	{
		ExpectedClientHash = InExpectedClientHash;
		ValidParticipants.Reset();
	}

	/** Session's rejoin window, supplied by the test. */
	double RejoinWindowSec = 0.0;

	/** Every acknowledgement, in order. */
	const TArray<FStealthStubAck>& GetAcks() const { return Acks; }

private:
	uint64 ExpectedClientHash = 0;
	TSet<int32> ValidParticipants;
	TArray<FStealthStubAck> Acks;
};

#endif // WITH_STEALTH_TEST_SEAMS
