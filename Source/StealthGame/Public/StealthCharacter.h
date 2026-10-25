#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "StealthTypes.h"
#include "StealthCharacter.generated.h"

class UCameraComponent;
class AStealthBush;
class AStealthPlayerState;
class UInputAction;

/**
 * The one pawn class used for BOTH players and NPCs (ADR-001 candidate: crowd identity parity).
 * The Game Animation Sample character Blueprint (CBP_SandboxCharacter; BP_CrowdCharacter is its child)
 * is reparented onto this class, so motion matching, traversal and cameras come from the sample while
 * the stealth rules live here. Nothing on this class differs between an NPC and a player except the
 * controller, which other clients cannot see except through the PlayerState link (see AStealthPlayerState).
 */
UCLASS()
class STEALTHGAME_API AStealthCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AStealthCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual bool CanJumpInternal_Implementation() const override;

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

	/** Server: plays the death (ragdoll) on every machine and schedules removal. */
	void KillCharacter();

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

protected:
	// Input handlers (local player only).
	void OnAssassinatePressed();
	void OnDefencePressed();
	void OnDefenceReleased();
	void OnInteractPressed();
	void OnInteractReleased();
	void OnPingPressed();

	/** Best visible target in front of this character within range, or null. */
	AStealthCharacter* FindAssassinationTarget() const;

	/** Whether the local viewer is on this pawn's team. */
	bool IsTeammateOfLocalViewer() const;

	/** Hides concealed enemies from this machine's viewer only (per-viewer concealment). */
	void UpdateConcealmentVisibility();

	void ApplyAppearance();
	void ApplyDeathPresentation();
	void SetDefenceStance(bool bPrepared);

	UFUNCTION(Server, Reliable)
	void ServerAssassinate(AStealthCharacter* Target, FVector_NetQuantize StrikePoint);

	UFUNCTION(Server, Reliable)
	void ServerSetDefence(bool bWantsDefence);

	UFUNCTION(Server, Reliable)
	void ServerSetInteracting(bool bWantsInteract);

	UFUNCTION(Server, Reliable)
	void ServerPing(FVector_NetQuantize Location);

	UFUNCTION()
	void OnRep_AppearanceIndex();

	UFUNCTION()
	void OnRep_Dying();

	UFUNCTION()
	void OnRep_PreparingDefence();

	/** Head-mounted camera for first-person mode. Inactive in third person. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stealth")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	/** Enhanced Input action reused for objective interaction (the sample's IA_Interact, bound to E). */
	UPROPERTY(EditDefaultsOnly, Category = "Stealth|Input")
	TSoftObjectPtr<UInputAction> InteractAction;

	/** Socket/bone the first-person camera follows. */
	UPROPERTY(EditDefaultsOnly, Category = "Stealth|Camera")
	FName FirstPersonSocket = TEXT("head");

	UPROPERTY(ReplicatedUsing = OnRep_AppearanceIndex)
	int32 AppearanceIndex = INDEX_NONE;

	UPROPERTY(ReplicatedUsing = OnRep_Dying)
	bool bDying = false;

	UPROPERTY(ReplicatedUsing = OnRep_PreparingDefence)
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
	double LastLocalPingTime = -1000.0;
	bool bHiddenForLocalViewer = false;
};
