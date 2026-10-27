#pragma once

#include "CoreMinimal.h"
#include "View/Core/StealthPawn.h"
#include "StealthTypes.h"
#include "View/NetPawnCommand.h"
#include "StealthCharacter.generated.h"

class UCameraComponent;
class AStealthBush;
class AStealthPlayerState;
class UInputAction;

/**
 * Prototype adapter over the production Shared Pawn (decision D53): AStealthCharacter now derives from
 * AStealthPawn (ADR-0010), so every body in L_Prototype is an AStealthPawn with UStealthMovementComponent, created by
 * UStealthPawnFactory. It keeps the prototype gameplay that has no production replacement yet (concealment,
 * defence, death presentation, appearance, the prototype input) until those systems' stories land; nothing new is
 * added here.
 *
 * The one pawn class used for BOTH players and NPCs (ADR-001 candidate: crowd identity parity).
 * The Game Animation Sample character Blueprint (CBP_SandboxCharacter; BP_CrowdCharacter is its child)
 * is reparented onto this class, so motion matching, traversal and cameras come from the sample while
 * the stealth rules live here. Nothing on this class differs between an NPC and a player except the
 * controller, which other clients cannot see except through the PlayerState link (see AStealthPlayerState).
 */
UCLASS()
class STEALTHGAME_API AStealthCharacter : public AStealthPawn
{
	GENERATED_BODY()

public:
	AStealthCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	/** Roster R9 EndPlay hook: ends this pawn's binding or tombstone (server). */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	/** Binds nothing gameplay-relevant (ADR-0011 section 9: gameplay input lives on AStealthPlayerController). */
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual bool CanJumpInternal_Implementation() const override;
	/** APawn's relevancy, unless a non-Shipping spike hook replaces it (THROWAWAY, IVP spike S9). */
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;
	/** Super, then a non-Shipping spike hook (THROWAWAY, IVP S10 re-run story-056: lost-pawn probe, ADR-0003 S10-R4). */
	virtual void OnActorChannelOpen(class FInBunch& InBunch, class UNetConnection* Connection) override;
	/** Super, then a non-Shipping spike hook (THROWAWAY, IVP S10 re-run story-056: pose warm-up, ADR-0003 S10-R5). */
	virtual void PostNetInit() override;

	// ---- Queries -------------------------------------------------------------------------

	/** Player state if this pawn is a player; null for NPCs. */
	AStealthPlayerState* GetStealthPlayerState() const;

	/** True when no player controls this pawn. */
	UFUNCTION(BlueprintPure, Category = "Stealth")
	bool IsNpc() const;

	/** Team index, or STEALTH_NO_TEAM for NPCs. */
	UFUNCTION(BlueprintPure, Category = "Stealth")
	uint8 GetTeam() const;

	/** A player in the informant role. */
	UFUNCTION(BlueprintPure, Category = "Stealth")
	bool IsInformant() const;

	/** A living, active (non-informant) player. */
	UFUNCTION(BlueprintPure, Category = "Stealth")
	bool IsActiveAgent() const;

	/** Moving faster than the run threshold. */
	UFUNCTION(BlueprintPure, Category = "Stealth")
	bool IsRunning() const;

	/** Doing parkour/traversal or airborne (jumps count as parkour). */
	UFUNCTION(BlueprintPure, Category = "Stealth")
	bool IsInTraversal() const;

	UFUNCTION(BlueprintPure, Category = "Stealth")
	bool IsDying() const { return bDying; }

	UFUNCTION(BlueprintPure, Category = "Stealth")
	bool IsPreparingDefence() const { return bPreparingDefence; }

	UFUNCTION(BlueprintPure, Category = "Stealth")
	bool IsConcealed() const { return bConcealed && !bRevealedUntilExit; }

	/** Held after a strike: movement speed is zero. */
	bool IsHeldInPlace() const;

	UFUNCTION(BlueprintPure, Category = "Stealth")
	bool IsFirstPerson() const { return bFirstPerson; }

	/** Locally predicted interaction progress (0..1) for the HUD; negative when not interacting. */
	float GetLocalInteractProgress() const;

	// ---- Server-side mutation (called by the game mode) ----------------------------------

	/** Server: plays the death (ragdoll) on every machine and schedules removal (the S10 harness scenarios' immediate kill). */
	void KillCharacter();

	/**
	 * Server, STAND-IN fall (until stories asn-018 / asn-020 own the strike clips and the client fall and linger): the
	 * prototype ragdoll on every machine at the death write's DeathServerTime. Unlike KillCharacter it neither unpossesses
	 * nor schedules a removal, and the server keeps the capsule's collision: the body sequence (story asn-019) releases the
	 * body from its controller at DeathServerTime + DeathFallSec and removes it through the round queue.
	 */
	void BeginDeathFallPresentation();

	/** Server: ends any defensive preparation. */
	void StopDefence();

	/** Server: begins the defensive preparation (rules already checked). */
	void StartDefence();

	/** Server: freezes movement for Duration seconds. */
	void HoldInPlace(float Duration);

	/** Server: bush overlap bookkeeping. */
	void SetInBush(AStealthBush* Bush, bool bInside);

	/** Server: a strike from concealment reveals the attacker until they leave the bush. */
	void RevealFromConcealment();

	/** Server: picks this character's appearance from the shared pool. */
	void SetAppearanceIndex(int32 NewIndex);

	/** Cancels a running traversal montage (used to stop informants from doing parkour). */
	void CancelTraversal();

	// ---- Client actions ------------------------------------------------------------------

	/** Switches between the sample's third-person camera rig and a head-mounted first-person camera. */
	UFUNCTION(BlueprintCallable, Category = "Stealth")
	void ToggleCameraMode();

	/**
	 * Owner prediction adapter (D53; ADR-0011 section 9 OnOwnerCommandPredicted): the owning client sent Kind, so the
	 * prototype's presentation-only predictions run (the predicted defence stance, the local interact progress). Nothing
	 * here sends anything: the command already went through the controller transport.
	 */
	void HandleOwnerCommandPredicted(ECommandKind Kind);

protected:
	/** Best visible target in front of this character within range, or null. */
	AStealthCharacter* FindAssassinationTarget() const;

	/** Whether the local viewer is on this pawn's team. */
	bool IsTeammateOfLocalViewer() const;

	/** Hides concealed enemies from this machine's viewer only (per-viewer concealment). */
	void UpdateConcealmentVisibility();

	void ApplyAppearance();
	/** The ragdoll; bDisableCapsule false keeps the capsule's collision (the server's body before its release). */
	void ApplyDeathPresentation(bool bDisableCapsule = true);
	void SetDefenceStance(bool bPrepared);

	/**
	 * The owner's predicted brace stance follows its replicated FActionState (story asn-016: the defence state is never
	 * replicated, so the Brace pose, which every client sees anyway, confirms or undoes the prediction).
	 */
	virtual void HandleActionStateChanged() override;

	UFUNCTION()
	void OnRep_AppearanceIndex();

	UFUNCTION()
	void OnRep_Dying();

	/** Head-mounted camera for first-person mode. Inactive in third person. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stealth")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	/** Socket/bone the first-person camera follows. */
	UPROPERTY(EditDefaultsOnly, Category = "Stealth|Camera")
	FName FirstPersonSocket = TEXT("head");

	UPROPERTY(ReplicatedUsing = OnRep_AppearanceIndex)
	int32 AppearanceIndex = INDEX_NONE;

	UPROPERTY(ReplicatedUsing = OnRep_Dying)
	bool bDying = false;

	/** Server only (story asn-016): the prototype brace tell is running. Clients read the replicated FActionState pose. */
	bool bPreparingDefence = false;

	/** In a bush. Replicated to all so each client can hide enemies locally (prototype; see header note). */
	UPROPERTY(Replicated)
	bool bConcealed = false;

	UPROPERTY(Replicated)
	bool bRevealedUntilExit = false;

	UPROPERTY(Replicated)
	double HeldUntilServerTime = 0.0;

	/** Server time the current defence began. */
	double DefenceActivationTime = 0.0;

	/** Bushes currently overlapping (server). */
	TArray<TWeakObjectPtr<AStealthBush>> OverlappingBushes;

	bool bFirstPerson = false;
	bool bLocalInteracting = false;
	double LocalInteractStartTime = 0.0;
	bool bHiddenForLocalViewer = false;
};
