#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"

#if WITH_STEALTH_TEST_SEAMS

#include "Server/StealthPenaltySeams.h"
#include "UObject/WeakObjectPtrTemplates.h"

class APawn;

/**
 * Test seams of the composition root's respawn gateway (FStealthPenaltyRespawnGateway; NPC-Kill Penalty story nkp-014).
 * Compiled only with WITH_STEALTH_TEST_SEAMS (never Shipping); every switch is off by default, so a production world never
 * sees them. Server only.
 *
 * 1. The HARNESS-ONLY crowd-less respawn. A production respawn never spawns a pawn (ADR-0019 section 4, PEN-R9/R10): with
 *    no eligible unseen crowd pawn the informant keeps waiting, whether or not the world has a crowd. The PIE harness rounds
 *    that run with NpcCount 0 (Assassination's OwnerResultShape, PossessionThroughFall, KillPlateTellsOneThing,
 *    CulledVictimOpensFalling, OneBodySequence, StrikeClipParity) have no crowd pawn to take over and still need a respawn,
 *    so they switch SetCrowdlessTeamStartRespawn on for their run: then a pick that finds no eligible unseen crowd pawn (the
 *    prototype's replacement NPC may exist and be seen) names a reserved key and its takeover spawns the participant's pawn
 *    at its team start
 *    (AStealthGameMode::SpawnHarnessCrowdlessRespawnPawn). Every such spawn is recorded as a HarnessSpawn probe event.
 *
 * 2. The respawn probe (StealthGame.Penalty.PIE.OneRespawnPath): while on, every gateway call of the commit (ADR-0019
 *    section 4.1 steps 2-6) appends one event, in call order, with the server frame and server time: the pick's status,
 *    level and RequestSeq; the pre-check and release answers; the takeover's answer with the stand-in LSC sight query
 *    re-run at the commit tick; the card move. Steps of one attempt run in one call stack, so an attempt is the run of events
 *    from one Pick to the next.
 *
 * 3. Release fault injection: the next N TryReleaseNpc calls answer false before touching the crowd (a failed release; the
 *    commit excludes the pawn and redraws next frame with no card move).
 *
 * Usage (a PIE test, BEFORE_ALL / AFTER_ALL):
 *   StealthPenaltyHarnessSeams::SetCrowdlessTeamStartRespawn(true);   // crowd-less harness round only
 *   StealthPenaltyHarnessSeams::SetRespawnProbe(true);                 // clears and starts recording
 *   ... GetRespawnProbeEvents() ...
 *   StealthPenaltyHarnessSeams::SetRespawnProbe(false); SetCrowdlessTeamStartRespawn(false);
 */
namespace StealthPenaltyHarnessSeams
{
	/** Turns the harness-only crowd-less team-start respawn on or off (off by default). */
	STEALTHGAME_API void SetCrowdlessTeamStartRespawn(bool bOn);
	/** True while the harness-only crowd-less team-start respawn is on. */
	STEALTHGAME_API bool IsCrowdlessTeamStartRespawn();

	/** One respawn-gateway call (story nkp-014's probe). */
	enum class ERespawnProbeStep : uint8
	{
		Pick,
		PreCheck,
		Release,
		Takeover,
		Card,
		/** The harness-only crowd-less path spawned a pawn (never in production). */
		HarnessSpawn,
	};

	/** One recorded call. Fields a step does not use keep their defaults. */
	struct FRespawnProbeEvent
	{
		ERespawnProbeStep Step = ERespawnProbeStep::Pick;
		/** Takeover, Card, HarnessSpawn: the participant. */
		FParticipantId Participant;
		/** Pick: the request's RespawnRequestSeq. */
		uint32 RequestSeq = 0;
		/** Pick: the team asked for. */
		ERosterTeam Team = ERosterTeam::None;
		/** Pick: the answer. */
		EPenaltyPickStatus PickStatus = EPenaltyPickStatus::None;
		/** Pick: the widening level used. */
		uint8 WideningLevel = 0;
		/** PreCheck, Release: the answer. */
		bool bOk = false;
		/** Takeover: the answer. */
		EPenaltyTakeoverResult TakeoverResult = EPenaltyTakeoverResult::Rejected_Stale;
		/** Takeover: the stand-in LSC sight query re-run at the commit tick (no live opposing participant sees the pawn). */
		bool bUnseenAtCommit = false;
		/** The pawn named by the call (Pick: the picked pawn; HarnessSpawn: the spawned pawn). */
		TWeakObjectPtr<APawn> Pawn;
		/** The pawn's location at the call. */
		FVector Location = FVector::ZeroVector;
		/** GFrameCounter at the call. */
		uint64 Frame = 0;
		/** The server world time at the call. */
		double ServerTime = 0.0;
	};

	/** Starts (clearing every event and injection) or stops the probe. */
	STEALTHGAME_API void SetRespawnProbe(bool bOn);
	/** True while the probe records. */
	STEALTHGAME_API bool IsRespawnProbeOn();
	/** Every event recorded since the probe was last started. */
	STEALTHGAME_API const TArray<FRespawnProbeEvent>& GetRespawnProbeEvents();
	/** Appends one event (the gateway's call; ignored while the probe is off, except HarnessSpawn, always counted). */
	STEALTHGAME_API void RecordRespawnProbeEvent(const FRespawnProbeEvent& Event);
	/** The number of harness-only spawns since the process started (production rounds must leave it unchanged). */
	STEALTHGAME_API int32 GetHarnessSpawnCount();

	/** The next Count TryReleaseNpc calls answer false without touching the crowd. */
	STEALTHGAME_API void InjectReleaseFailures(int32 Count);
	/** Consumes one injected release failure; true when one was pending. */
	STEALTHGAME_API bool ConsumeInjectedReleaseFailure();
}

#endif // WITH_STEALTH_TEST_SEAMS
