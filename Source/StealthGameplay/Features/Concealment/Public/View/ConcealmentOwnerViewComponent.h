#pragma once

#include "CoreMinimal.h"
#include "View/StealthRosterTeamView.h"

#include "ConcealmentOwnerViewComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnOwnerConcealedChanged, bool /*bIsConcealed*/);

/**
 * UConcealmentOwnerViewComponent (ADR-0017 section 1; GDD CON-R2; story con-023): the owner's own concealed flag, a
 * replicated component on the PlayerController (the ADR-0001 owner-data family: the controller is relevant only to its
 * owner, so the audience is Owner). One property, push-based: bIsConcealed. Its only writer is the server's Concealment
 * core through the composition root's flag adapter (set at the commit, cleared at the crossing, or at a killed-concealed
 * body's removal / the participant's rebind). BushId is never replicated: an owner derives it from its position and the
 * Manifest. The IVP allowlist has exactly this one row for the component.
 *
 * Usage (owner client): Controller->FindComponentByClass<UConcealmentOwnerViewComponent>()->IsConcealed()
 */
UCLASS()
class STEALTHGAMEPLAY_API UConcealmentOwnerViewComponent : public UTeamViewComponentBase
{
	GENERATED_BODY()

public:
	UConcealmentOwnerViewComponent();

	/** The default-subobject name on the PlayerController. */
	static const FName ComponentName;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: writes the flag (change-only). True when it changed. */
	bool SetConcealed(bool bConcealed);

	/** The owner's concealed flag (the owner client and the server). */
	bool IsConcealed() const { return bIsConcealed; }

	/** Server writes since construction (observation). */
	int32 GetServerWriteCount() const { return ServerWriteCount; }

	/** Raised on the owner client when the flag changes (and on the listen host for its own controller). */
	FOnOwnerConcealedChanged OnConcealedChanged;

protected:
	UFUNCTION()
	void OnRep_IsConcealed();

private:
	UPROPERTY(ReplicatedUsing = OnRep_IsConcealed)
	bool bIsConcealed = false;

	int32 ServerWriteCount = 0;
};
