# Evidence: Assassination & Defence Reversal stories 005-011 (2026-10-27)

Epic: `production/epics/assassination-defence-reversal/EPIC.md`. Stories 005-011, all **Complete** (Logic / Integration;
no player-visible change: the resolver is built and tested but not yet registered in play, stories 012-013).

## What was built

The strike resolver core, plain C++ behind injected seams, in `Source/StealthGameplay/Features/Assassination/`:

- `Server/EncounterTypes.h` (+ `Private/EncounterTypes.cpp`): outcome enums, `FOnPawnKilled`, `FEncounterParams` (frozen
  tuning + foreign keys), pawn facts, the consumer-owned seams, the reply and resolution records.
- `Server/VictimResolverCore.h` (+ .cpp): ASN-R8 rules 1-4 and the candidate filter (asn-005), rule 2 with the
  BushSpotPick counter-mode secret-stream draw (asn-006).
- `Server/BushSpotQuery.h`: `IBushSpotQuery`, the Concealment seam (Concealment not built: stub harness in tests).
- `Server/StrikeRoleTable.h` (+ .cpp): the per-pawn strike-role table (lazy expiry on the queue clock) and
  `FBracedAtAttackCore` (F2 key compare, asn-009).
- `Server/AttackResolverCore.h` (+ .cpp): `FAttackResolverCore`, the `IQueueResolver` for PawnCommand (Attack,
  PrepareBegin/End, slot gate for the rest) and HoldTimer (the Prepare hold stand-in); the in-slot effect order of
  ADR-0018 section 2.1 (asn-007), the lockout for every outcome (asn-008), same-drain ordering and the held NotNow after
  the latch (asn-009), the reversal / counter / chained strikes (asn-010); DeathWrite / CounterPose staged in the slot.
- `Server/EncounterService.h` (+ .cpp): `UEncounterService` thin facade and its production adapters (queue, defence
  component, world pawn query, Roster store, action hub, SCL stance, CanViewerKnow stand-in); `RegisterOn(queue)`.
- `Server/AssassinationAttackPredicate.h` (+ `Private/AttackPredicate.cpp`): Attack's CanBegin = Allow, registered in
  `UStealthCompositionRoot` (asn-008).
- Fix: `StealthFoundation/Private/Tuning/AssassinationInvariants.cpp` anonymous `Rows` -> `AssassinationInvariantRows`
  (unity-build C4459 clash, latent since asn-001).

## Tests (all pass; `full-suite.txt`: 337 / 337)

New (21): `StealthGame.Assassination.` VictimResolutionOrder, AlliesNeverVictims, DeadBodyIsNoContact (005);
BlindBushStrikeIsACommitment, BushVictimHash (006); OutcomeInSlot, KillEventMatrix, ReversalCanEndTheRound,
EncounterServiceSmoke (007 + facade); NoTargetDependentRejection, LockoutForEveryOutcome, MovingVictims (008);
BracedAtAttackKeyOrder, SameDrainOrdering (009); ReversalEliminatesAttacker, CounterIsNotProtected, StrikerStruckMidLunge
(010); NpcBraceSamePath, NpcsNeverStrike, PlantedBrace (011). Rigs: `EncounterTestRig.h` (fakes, direct-call stub-queue
harness), `EncounterRoundRig.h` (real FResolutionQueueCore + real FRoundMatchCore via FRoundWinQueueRig).

Measured: BushVictimHash O1 493 / O2 507 of 1,000, one draw per strike; MovingVictims runner X_eff 238.25 (<= 240);
NpcsNeverStrike 50 NPCs x 10 min: 1,800,100 Move calls, 232 discrete commands, 0 Attack; PlantedBrace 4 s: 0.0000 cm,
0.0000 deg, look target error 0.

Watched failing: SameDrainOrdering case (b) failed when the Round rig bound no visibility seam (default-deny made A2's
PawnRef a spot strike that killed the bystander); it passes with the seam bound.

Run history: run 1 build error (missing include in PlantedBraceTest); run 2 336/337 (SameDrainOrdering, above); runs 3
and 4 336/337 with `StealthGame.Locomotion.PIE.LocomotionPie.ReleaseBaselineStopUnderLag` failing (owner/server stop
under 100 ms PktLag on L_Prototype with the live crowd; it passed in run 2, passed twice in isolation and in run 5, and
nothing in this batch runs on its path besides the always-Allow Attack predicate registration): a timing-sensitive PIE
test, flagged for the owner (SCL-011) as intermittent; run 5 (final) 337/337.

Run result: N/A for every story (server logic not wired into play; stories 012-013 wire it and run-and-observe).

## Decisions (logged to production/session-logs/decision-log.md)

Proceed-anyway on cross-epic seams (005-011); feature-folder placement; opaque APawn identity + fact seam, spawn-serial
stand-in; horizontal PawnRef agreement and the honoured-then-terminal Dead ref; BushSpotPick keying and one-draw rule;
lazy Striking/Countering expiry; Prepare hold stand-in (IIF 042-050); DeathWrite/CounterPose staging in the slot now;
Countering from the slot; headless instead of PIE for 011; Concealment stand-in adapter deferred to the wiring;
CanViewerKnow default-deny + world stand-in.
