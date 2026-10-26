#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "StealthRosterTeamView.generated.h"

class APawn;

/**
 * C-TeamView, Roster part (ADR-0001 sections 5-6 and Key Interfaces; GDD participant-state-roster R6; stories
 * psr-037 (types) and psr-062 (component and push step)).
 *
 * Client-safe wire types (no Server/ include, ADR-0007 rule (c)). Team data reaches a client only through these:
 * one replicated component per APlayerController, relevant only to its owner (COND_OwnerOnly), written only by the
 * Roster push step (FRosterPushStep) on change and at most once per server frame. Own-team data only: no opponent
 * entry and no opponent count ever appears here.
 *
 * Story psr-037 owns the fixed-width NetSerialize and the <= 96 B size budget; until it lands these structs use the
 * default property serializer (decision logged in the D53 integration batch).
 */

/** Why a teammate's bReconnecting flag was cleared (ruling D23). */
UENUM()
enum class EReconnectEndCause : uint8
{
	None = 0,
	Rejoined = 1,
	GraceExpired = 2,
	/** Removal, elimination, round end or a host kick. */
	Cleared = 3,
};

/** The owner block (R6): what the owning player knows about itself. */
USTRUCT()
struct STEALTHFOUNDATION_API FTeamViewOwnerPart
{
	GENERATED_BODY()

	/** The owner's random PublicHandle (never a ParticipantId). */
	UPROPERTY()
	uint32 Handle = 0;

	/** 0, 1, or 255 for none. */
	UPROPERTY()
	uint8 Team = 255;

	/** EParticipantState as uint8 (Active 0, Informant 1, RoundEnded 2). */
	UPROPERTY()
	uint8 State = 2;

	UPROPERTY()
	uint32 RoundEpoch = 0;

	/** Owner-view feed (story psr-038); 0 until it lands. */
	UPROPERTY()
	uint8 WrongTargetTally = 0;

	/** Owner-view feed (story psr-038); width placeholder for FClientCommandSeq (ADR-0011). */
	UPROPERTY()
	uint32 LastResultSeq = 0;

	/** Informant respawn due time, 0 = none (story psr-038). */
	UPROPERTY()
	double T_due = 0.0;

	bool operator==(const FTeamViewOwnerPart& O) const
	{
		return Handle == O.Handle && Team == O.Team && State == O.State && RoundEpoch == O.RoundEpoch && WrongTargetTally == O.WrongTargetTally
			&& LastResultSeq == O.LastResultSeq && T_due == O.T_due;
	}
	bool operator!=(const FTeamViewOwnerPart& O) const { return !(*this == O); }
};

/** One teammate (R6): at most TeamSize - 1 (four) per view. */
USTRUCT()
struct STEALTHFOUNDATION_API FTeamViewTeammateEntry
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 Handle = 0;

	/** EParticipantState as uint8. */
	UPROPERTY()
	uint8 State = 2;

	/** The teammate's live (or, in the retiring update, retired) pawn; null when none. Never an AStealthPawn type (Foundation). */
	UPROPERTY()
	TObjectPtr<APawn> PawnRef = nullptr;

	/** Owned by Concealment (later epic); false until it writes. */
	UPROPERTY()
	bool bIsConcealed = false;

	/** Non-zero only in the retiring update (manifest rule): seconds of server time. */
	UPROPERTY()
	double RetireAtServerTime = 0.0;

	/** Team-audience reconnect indicator (ruling D23): set on a Disconnected projection in phase Active, cleared only by an event. */
	UPROPERTY()
	bool bReconnecting = false;

	UPROPERTY()
	EReconnectEndCause ReconnectEndCause = EReconnectEndCause::None;

	bool operator==(const FTeamViewTeammateEntry& O) const
	{
		return Handle == O.Handle && State == O.State && PawnRef == O.PawnRef && bIsConcealed == O.bIsConcealed
			&& RetireAtServerTime == O.RetireAtServerTime && bReconnecting == O.bReconnecting && ReconnectEndCause == O.ReconnectEndCause;
	}
	bool operator!=(const FTeamViewTeammateEntry& O) const { return !(*this == O); }
};

/** The whole Roster view of one owner: the unit the push step writes and story psr-037 serializes. */
USTRUCT()
struct STEALTHFOUNDATION_API FTeamView
{
	GENERATED_BODY()

	UPROPERTY()
	FTeamViewOwnerPart Owner;

	/** Own-team teammates only, in ParticipantId order (at most four). */
	UPROPERTY()
	TArray<FTeamViewTeammateEntry> Teammates;

	bool operator==(const FTeamView& O) const { return Owner == O.Owner && Teammates == O.Teammates; }
	bool operator!=(const FTeamView& O) const { return !(*this == O); }
};

/** Base of every C-TeamView part (ADR-0001 Key Interfaces): owner-only, on an APlayerController. */
UCLASS(Abstract)
class STEALTHFOUNDATION_API UTeamViewComponentBase : public UActorComponent
{
	GENERATED_BODY()

public:
	UTeamViewComponentBase();
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTeamViewUpdated, const FTeamView& /*View*/);

/**
 * The Roster part of C-TeamView. Server: only FRosterPushStep calls ApplyView. Client: OnRep_View raises
 * OnViewUpdated for the HUD. Replicated owner-only and push-based (MarkDirty on every write).
 */
UCLASS()
class STEALTHFOUNDATION_API URosterTeamViewComponent : public UTeamViewComponentBase
{
	GENERATED_BODY()

public:
	URosterTeamViewComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server (push step only): writes the view and marks it dirty. Counts the write. */
	void ApplyView(const FTeamView& NewView);

	/** The current view (server: last written; client: last received). */
	const FTeamView& GetView() const { return View; }

	/** Server writes made through ApplyView (the push step's change-only, once-per-frame contract is tested on it). */
	int32 GetServerWriteCount() const { return ServerWriteCount; }

	/** Raised on the owning client when a new view arrives. */
	FOnTeamViewUpdated OnViewUpdated;

protected:
	UFUNCTION()
	void OnRep_View();

private:
	UPROPERTY(ReplicatedUsing = OnRep_View)
	FTeamView View;

	int32 ServerWriteCount = 0;
};
