# Evidence: Assassination & Defence Reversal stories 001-004 (2026-10-27)

Stories: `production/epics/assassination-defence-reversal/story-001..004`. Type: Logic (all four). Tier: workflow full
(rigor full). Build: `tools/dev/cycle.ps1 -Tests -NoLaunch` -> `Result: Succeeded`, 317 / 317 `StealthGame.` tests pass
(309 at batch start + 8 new). Full output: `full-suite.txt`. Stamp: `stamp.txt`.

## What was built

| Story | Built | Tests |
|---|---|---|
| 001 tuning keys and invariants | 13 new keys in `FAssassinationTuning_Public` (now 13 All keys) and a new `FAssassinationTuning_Server` (3 ServerOnly keys) on both tuning faces and `UStealthTuningData`; schema table regenerated (Stealth.Tuning.SchemaGen, WROTE), `TuningSchemaVersion` 3 -> 4 (fingerprint 0x1aa42c9205920326); invariants I-ASN-1..8, I-ASN-5w, W-ASN-9..11 as pure checks (`Server/StealthAssassinationInvariants.h`, `Private/Tuning/AssassinationInvariants.cpp`) registered in the TDC table with owner `Assassination`; the GDD table as fixture `Server/Testing/StealthAssassinationTuningKnobs.h`, pinned by SchemaComplete | `StealthGame.Assassination.TuningInvariants` |
| 002 F1 reach | `FStrikeReachCore` / `FStrikeReachParams` / `FStrikeReachInput` / `FStrikeReachResult` (`Features/Assassination/Public/Server/StrikeReachCore.h`), pure, standard and bush strike, returns X_eff and theta_T | `StealthGame.Assassination.ReachF1`, `StealthGame.Assassination.ReachBushStrike` |
| 003 defence state | `FDefenceStateCore` (stamp from activation, window by key, lockout, recovery, reset), `UDefenceStateComponent` (non-replicated, no tick, server FindOrAdd), `UAssassinationPreparePredicate` (Reject(Cooldown) iff F3), registered for `EActionType::Prepare` from `UStealthCompositionRoot` | `...CooldownFromActivation`, `...DefenceStateSurvivesPossession`, `...DisconnectKeepsTimers` |
| 004 wire structs | `EActionPose`, `EStrikeResult`, `EDefenceResult`, `StrikeVariant`, `FFixedWarpOffset`, `FActionState` (208 bits), `FDeathState` (65 bits), `FStrikeResult`, `FDefenceResult` in Foundation `Public/View/StealthEncounterWire.h`; SerializeBits-only NetSerialize, bitwise Identical; test seam serialiser for the negative control | `StealthGame.Assassination.WireWidth`, `StealthGame.Assassination.WireShape` |

Confirmed user rules honoured: cooldown 60 s from activation (`DefenceCooldownSec` default 60, stamped at the executed
Begin, asserted in CooldownFromActivation), protected window 4 s (`DefenceDurationSec` 4, CapEnd due at T_act + 4,
asserted in CooldownFromActivation and DisconnectKeepsTimers), reach about 2 m (`AssassinationRangeCm` 200).

## Gates watched failing

- WireWidth AC3: the magnitude-packed WarpPoint serialiser is installed through the test seam; the sweep reports widths
  other than 208 ("control detected" in the log); the production serialiser is restored and the sweep passes.
- TuningInvariants: an injected `CrowdReplacementMinDelaySec` of 9.2 turns I-ASN-5w into a warning (the injected inputs
  are really read); the safe-range lint asserts its exact finding set both ways.

## Decisions (logged with log_decision, 2026-10-27)

1. Cross-epic dependencies not Complete: proceed, risk accepted.
2. Keys in the existing `_Public` / new `_Server` sub-structs (not the story's per-owner headers): generator and tests reflect those files.
3. Externally owned keys (BushStrikeEmergeSec 0.35, InformantRespawnDelaySec 15, PawnRefAgreementCm 75) as pending owner constants (TD-TDC-I3 pattern), injectable through `FAssassinationInvariantInputs`, instead of a new `NotEvaluated` check value.
4. 1e-4 tolerance in the invariant comparisons (float keys widened).
5. Safe-range lint conflict pinned: `StrikeWarpYawLimitDeg` safe minimum 20 is rejected by I-ASN-8 at `StrikeBearingLimitDeg` 90 (60 + 20 < 90). GDD issue for game-designer.
6. Defence component created server-side on first defence write (no Core -> Feature include).
7. Protected-window record on `FDefenceStateCore` as the IIF hold-machinery stand-in.
8. Feature folder `Features/Assassination/{Public/Server,Private}`, core headers public.

## Run result

Run result: N/A — nothing player-observable changes: the registered Prepare predicate answers Allow until a Prepare(Begin)
is stamped, and no production path stamps yet (the in-slot resolver is stories 007-011); the wire structs are not yet
replicated properties (story 012).
