#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "Server/Core/Round/StealthRoundGateways.h"
#include "UObject/Object.h"

#include "StealthRoundRosterGateway.generated.h"

class URosterStore;

/**
 * The production Round -> Roster gateway (RMF-003 ruling: each producing system implements its gateway as a thin
 * adapter over its own facade). Forwards IRoundRosterGateway to URosterStore; adds no rule. Pawn handles from the
 * queue vocabulary (FPawnHandle) are not mapped yet: BindPawn/ReleaseToCrowd answer Rejected_PawnInUse until the
 * pawn factory story (SCL-006) routes binding through URosterStore directly.
 */
UCLASS(NotBlueprintType, NotBlueprintable)
class STEALTHGAMEPLAY_API URoundRosterStoreGateway : public UObject, public IRoundRosterGateway
{
	GENERATED_BODY()

public:
	/** The store this gateway forwards to (held weakly; a dead store answers Rejected_NotAuthority). */
	void Bind(URosterStore* InStore);

	virtual ERosterResult BeginMatch(int32 TeamSize) override;
	virtual ERosterResult SetTeamSize(int32 TeamSize) override;
	virtual ERosterResult SetTeamsLocked(bool bLocked) override;
	virtual ERosterResult AssignTeams(TConstArrayView<FRoundTeamAssignment> Assignments) override;
	virtual ERosterResult RemoveParticipant(FParticipantId Participant) override;
	virtual ERosterResult ResetAll(uint32 RoundIndex) override;
	virtual ERosterResult EndRoundAll() override;
	virtual ERosterResult RequestTransition(FParticipantId Participant, EParticipantState NewState, ERosterReason Reason,
		const FEpochToken& ExpectedEpoch, TOptional<uint32> ExpectedConnectionGeneration) override;
	virtual ERosterResult BindPawn(FParticipantId Participant, const FPawnHandle& Pawn) override;
	virtual ERosterResult ReleaseToCrowd(const FPawnHandle& Pawn) override;
	virtual void GetUnboundActiveParticipants(TArray<FParticipantId>& OutParticipants) const override;
	virtual ERosterResult RecordAppearance(FParticipantId Participant, uint16 AppearanceId) override;
	virtual FEpochToken GetEpochToken() const override;
	virtual void GetRosteredParticipants(TArray<FRoundRosterMember>& OutMembers) const override;

private:
	TWeakObjectPtr<URosterStore> Store;
};
