#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "DefenceOwnerComponent.generated.h"

class AController;
class APawn;

/**
 * UDefenceOwnerComponent: the ONLY defence datum a client ever holds (GDD ASN-R18, UI-ASN-3; ADR-0018 section 1.5; story
 * asn-016). A default subobject of AStealthPlayerController, replicated COND_OwnerOnly through the controller's
 * registered subobject list, so only the owning connection receives it (a PlayerController never replicates to another
 * connection at all). It is not a pawn property, so pawn byte parity is unaffected; the pawn-side UDefenceStateComponent
 * has no replicated property (T_act and LockoutEnd never reach a client).
 *
 *   CooldownEndServerTime  T_act + DefenceCooldownSec of the owner's current pawn, -1 when the pawn never braced (or after
 *                          Round's Reset). Written on the server only: at an executed Prepare(Begin) (the resolver's slot,
 *                          FEncounterOwnerCooldownPublisher, the same stamp as the server's Cooldown verdict) and on a
 *                          re-pawn (SyncFromPawn; PawnRecovery through AdoptForRecovery). A re-press inside the window or
 *                          the cooldown is voided Cooldown in its slot and writes nothing.
 *
 * The HUD reads GetCooldownRemaining(ServerNow) for the "Defence: recovering Ns" line (story asn-024 owns the brace
 * indicator). The component decides nothing on the server: the server's verdict is the stamp.
 *
 * Usage (server):
 *   UDefenceOwnerComponent::Find(Pawn->GetController())->SetCooldownEndServerTime(TAct + Cooldown);
 *   OnPossess:   DefenceOwner->SyncFromPawn(NewPawn, DefenceCooldownSec);
 * Usage (owner client):
 *   const double Remaining = DefenceOwner->GetCooldownRemaining(GameState->GetServerWorldTimeSeconds());
 */
UCLASS(NotBlueprintable, NotBlueprintType, ClassGroup = (Stealth))
class STEALTHGAMEPLAY_API UDefenceOwnerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDefenceOwnerComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The component on Controller (null for none, or an AI controller). */
	static UDefenceOwnerComponent* Find(const AController* Controller);

	/** The owner's cooldown end in server time; negative when none. */
	double GetCooldownEndServerTime() const { return CooldownEndServerTime; }
	/** Seconds until the cooldown ends at NowServerTime (0 when none or over). */
	double GetCooldownRemaining(double NowServerTime) const;

	/** Server: writes the value (no write when unchanged). Returns true when it changed. */
	bool SetCooldownEndServerTime(double InCooldownEndServerTime);
	/** Server: back to none (Round Reset). */
	void ClearCooldownEnd() { SetCooldownEndServerTime(-1.0); }

	/**
	 * Server, on a re-pawn (OnPossess): copies the new pawn's stamp end (T_act + CooldownSec), or none when the pawn never
	 * braced. A pawn that PawnRecovery handed over carries the adopted stamp (AdoptForRecovery below).
	 */
	void SyncFromPawn(const APawn* Pawn, double CooldownSec);

	/**
	 * Server, PawnRecovery (ASN-R18; the SCL / Roster recovery path calls it before possessing NewPawn): the new pawn adopts
	 * the lost pawn's stamp, or stamps NowServerTime when it is unknown (never a free brace), and Controller's owner value is
	 * copied from it (lost end known: that end; unknown: Now + CooldownSec).
	 */
	static void AdoptForRecovery(AController* Controller, APawn* NewPawn, const APawn* LostPawn, double NowServerTime, double CooldownSec);

	/** Server writes so far (tests: a re-press must not write). */
	int32 GetWriteCount() const { return WriteCount; }

private:
	/** T_act + DefenceCooldownSec, or -1 (none). Owner only. */
	UPROPERTY(Replicated)
	double CooldownEndServerTime = -1.0;

	int32 WriteCount = 0;
};
