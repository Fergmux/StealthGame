#pragma once

// Story asn-021 (production/epics/assassination-defence-reversal/story-021-role-change-dispatch-and-transient-grade.md):
// the owning client's own-role clock (ADR-0018 section 5.2) and the transient grade at the local contact frame (GDD
// VA-ASN-4, art bible 2.7).

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "Subsystems/LocalPlayerSubsystem.h"

#include "EncounterViewSubsystem.generated.h"

class APawn;
class AStealthPawn;

/** Why the locally possessed pawn's role changed (ASN-R23). */
UENUM()
enum class ERoleChangeReason : uint8
{
	/** An Active player struck and killed. */
	Assassinated,
	/** An Active player whose own strike was reversed by a braced defender. */
	ReversedAgainst,
	/** A player already an informant, killed. */
	InformantKilled,
};

/** Log name of a reason. */
STEALTHGAMEPLAY_API const TCHAR* LexRoleChangeReason(ERoleChangeReason Reason);

/** Which participant's camera a grade is for (VA-ASN-4: a body met a body; never a whiff, a brace, a bystander or an ally). */
UENUM()
enum class ETransientGradeCause : uint8
{
	/** The attacker of a kill or a reversal, at its FStrikeResult.ContactServerTime. */
	AttackerContact,
	/** A player victim (Active or informant), at its own DeathServerTime. */
	VictimDeath,
	/** A bound defender on a reversal, at its Counter's ContactServerTime. */
	DefenderCounter,
};

/** One ApplyTransientGrade call (observation for tests and the presentation log). */
struct FTransientGradeCall
{
	ETransientGradeCause Cause = ETransientGradeCause::AttackerContact;
	/** EstimatedServerNow() at the call. */
	double ServerNow = 0.0;
	/** The contact / death time the call answers. */
	double DueServerTime = 0.0;
	/** The world's global time dilation at the call (the grade never changes it). */
	float TimeDilation = 1.0f;
	/** Reduce screen effects was on. */
	bool bReduced = false;
};

/** One OnRoleChangePresented dispatch. */
struct FRoleChangeRecord
{
	ERoleChangeReason Reason = ERoleChangeReason::Assassinated;
	double ServerNow = 0.0;
	double DeathServerTime = 0.0;
	/** The dying pawn, as an opaque key (never dereferenced). */
	const APawn* PawnKey = nullptr;
};

/** OnRoleChangePresented(Reason, LocalTime): client-local, once per FDeathState of the locally possessed pawn. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnRoleChangePresented, ERoleChangeReason /*Reason*/, double /*LocalTime*/);

/**
 * UEncounterViewSubsystem (ADR-0018 section 5.2): LocalPlayer-scoped. UEncounterPresentationSubsystem::Refresh calls
 * DispatchRoleChange for the locally possessed pawn exactly once per FDeathState, at its DeathServerTime (or on arrival when
 * late); HUD, Camera, input-context swaps and owner audio subscribe to OnRoleChangePresented and to nothing else, buffering
 * any outcome-caused owner data that arrives earlier (ASN-R23).
 *
 * ApplyTransientGrade is a STAND-IN seam for Camera story 019's grade hook (not built): it records the call and drives
 * UEncounterGradeCameraModifier (a post-process beat on this player's camera only; never time dilation or shake). The
 * Reduce screen effects setting (art bible 7.6 rule 8) is the console variable Stealth.Accessibility.ReduceScreenEffects
 * until the settings menu exists.
 *
 * Usage (a HUD consumer):
 *   if (UEncounterViewSubsystem* View = LocalPlayer->GetSubsystem<UEncounterViewSubsystem>())
 *   {
 *       View->OnRoleChangePresented.AddUObject(this, &UMyHud::HandleRoleChange);
 *   }
 */
UCLASS()
class STEALTHGAMEPLAY_API UEncounterViewSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	/** The subsystem of the local player whose controller possesses Pawn on this machine; null for any other pawn. */
	static UEncounterViewSubsystem* FindForPawn(const APawn* Pawn);

	/** Reduce screen effects is on (Stealth.Accessibility.ReduceScreenEffects). */
	static bool IsReduceScreenEffects();

	/** The own-role clock (ADR-0018 section 5.2). */
	FOnRoleChangePresented OnRoleChangePresented;

	/** Called by the presentation once per FDeathState of this player's pawn, at its DeathServerTime (or on late arrival). */
	void DispatchRoleChange(const APawn* Pawn, ERoleChangeReason Reason, double ServerNow, double DeathServerTime);

	/**
	 * Camera's grade hook (STAND-IN, Camera story 019): starts the art bible 2.7 grade on this player's camera now. Callers
	 * call it on the local contact frame, never earlier, and only for the participants.
	 */
	void ApplyTransientGrade(ETransientGradeCause Cause, double ServerNow, double DueServerTime);

	/** The grade's blend weight at WorldTime (0 outside the beat). */
	float GetGradeWeight(double WorldTime) const;

	/** True while a grade beat is running at WorldTime. */
	bool IsGradeActive(double WorldTime) const { return GetGradeWeight(WorldTime) > 0.0f; }

	/** Every grade call and role change this session (observation). */
	const TArray<FTransientGradeCall>& GetGradeCalls() const { return GradeCalls; }
	const TArray<FRoleChangeRecord>& GetRoleChanges() const { return RoleChanges; }

	/**
	 * STAND-IN for the owner's C-TeamView own State (HUD / Roster owner part): whether this player's current pawn was
	 * possessed as an informant. Set by the game's player controller; unset reads false.
	 */
	void SetPossessedAsInformantQuery(TFunction<bool(const APawn*)> Query) { PossessedAsInformant = MoveTemp(Query); }
	bool WasPossessedAsInformant(const APawn* Pawn) const { return PossessedAsInformant ? PossessedAsInformant(Pawn) : false; }

private:
	TArray<FTransientGradeCall> GradeCalls;
	TArray<FRoleChangeRecord> RoleChanges;
	TFunction<bool(const APawn*)> PossessedAsInformant;
	/** World time the current beat started (-1: none), and whether it runs reduced. */
	double GradeStartWorldTime = -1.0;
	bool bGradeReduced = false;
};

/**
 * The transient grade's post-process beat (STAND-IN for Camera story 019's grade hooks): a camera modifier added to the
 * local player's camera manager on the first grade, reading the weight from the player's UEncounterViewSubsystem.
 */
UCLASS()
class STEALTHGAMEPLAY_API UEncounterGradeCameraModifier : public UCameraModifier
{
	GENERATED_BODY()

public:
	virtual void ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings) override;
};
