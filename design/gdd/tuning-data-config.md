# Tuning Data & Config

> **Status**: Reviewed - MAJOR REVISION NEEDED 2026-10-24 (all blocking revisions applied by the reviewer; independent re-review required before Approved); reconciled 2026-10-24; ADR-synced 2026-10-24; user scope decisions 2026-10-25 registered (Informant Pings owner keys and I-PNG-1; Crowd `CrowdFullPacePct` default 55 with the walk-slower modifier); user decisions applied 2026-10-25 (`NpcCount` default 50; `NpcKillStrikeLimit` hard minimum 2 with a test-preset flag; new keys `LayoutExactRefineMaxCount`, `CrowdMotionProfileAsset`, `CrowdFullStateTreeAsset`, `CrowdRestrictedStateTreeAsset`; `LatencyWarnRttSec` retired; CR-10 renames applied; Shipping presets Default-only confirmed; stays a GDD — OQ-1, OQ-6, OQ-12 closed)
> **Design Review**: MAJOR REVISION NEEDED 2026-10-24 (27 revisions applied: 19 blocking, 8 recommended groups; log: design/gdd/reviews/tuning-data-config-review-log.md)
> **Author**: design pipeline (autonomous run) + specialist agents (creative-director, systems-designer, ue-replication-specialist, engine-programmer, qa-lead)
> **Last Updated**: 2026-10-24
> **Last Verified**: 2026-10-24
> **Implements Pillar**: Pillar 1 (Hide in Plain Sight: no tuning-induced tell); supports Pillar 2 (every advantage is a tuned, readable deviation)
> **Creative Director Review (CD-GDD-ALIGN)**: CONCERNS then REVISED 2026-10-24 (two concerns addressed: non-Ready participants have no observable pawn, CR-8; blind-guess breach rejects public presets, I-4; host wording narrowed in Edge Cases; revised sections not re-gated)

## Summary

Tuning Data & Config is the single convention that makes every gameplay number in Smoking Mirror a designer-editable data value that the server loads, validates, freezes and stamps with a hash for each round, and that clients can only receive, never supply. It exists so that no match is decided by a locally modified file and no tuned value can make one player differ from an NPC (Pillar 1); players never see it, they feel only that all ten of them play by the same numbers.

> **Quick reference** — Layer: `Foundation` · Priority: `MVP` · Key deps: `None` (downstream: Round & Match, Input & Interaction, Character Pool, Level Socket Contract, Shared Character, Crowd, Assassination, NPC-Kill Penalty, Objectives, Generator)

## Overview

Tuning Data & Config (TDC) is the Foundation-layer data convention for the whole game. It defines (1) where tunable values live: cooked data assets of one class (working name `UStealthTuningData`), composed of per-owner-system sub-structs and edited in the Unreal editor without recompiling; (2) how the server loads them: it selects a preset, validates every key against a hard range, rejects incoherent presets, freezes the result when a round enters `Preparing`, and stamps it with a `TuningHash`; (3) how values reach clients: only keys flagged client-visible are replicated, in one snapshot identical for every viewer, and clients act only on that snapshot, never on local files, ini overrides or console variables; and (4) how every downstream GDD registers its knobs, so each tunable has one owner, a default, a hard range, a safe range and an audience.

It is deliberately small: it owns a schema, a pipeline and a convention, not gameplay behaviour. The eight canonical keys the user left open (`TeamSize`, `NpcCount`, `AssassinationRangeCm`, `DefenceDurationSec`, `DefenceCooldownSec`, `NpcKillStrikeLimit`, `ObjectiveInteractSec`, `TargetRoundMinutes`) are tabulated here with defaults and ranges; their meaning belongs to their owner systems. The concept's PR-SCOPE suggestion to make this an ADR plus knob registry was declined by the user 2026-10-25: Tuning Data stays a GDD, and ADR-0004 covers the implementation (OQ-1 closed).

## Player Fantasy

*Framing: indirect (infrastructure the player never touches). Shaped by creative-director, framings A and C combined.*

**The mirror does not lie.** Every citizen, rival and informant in the city lives by the same numbers, and no one can bend them. When a stranger braces a beat early, or lingers three seconds too long at a shrine, that difference is something they chose, not something the machine gave them. You read people; you never have to read the system.

The anchor is the Core Fantasy's moment of certainty: *they turned their head one beat too late, and you are right.* This system is why you can trust that certainty: the stranger's defence, cooldown and interaction time are exactly yours, so the deviation you spotted can only be behaviour. The victim's side matters equally (Pillar 3): when the knife finds you, it is because someone watched you longer than you watched them, never because their reach was longer, their timings different or their copy of the game different. Whatever beat you, it was never the numbers.

Pillar link: Pillar 1 ("until someone commits, an enemy player cannot be told apart from an NPC by model, animation, audio, UI, or network presentation") and Pillar 2 ("anything that helps a player must be a visible deviation from that baseline"). Phrase to avoid in any player-facing text: "level playing field" (sameness is the background, not the selling point).

## Detailed Design

> Specialist input folded in: systems-designer (ranges, invariants, edge cases), ue-replication-specialist (snapshot placement, ack, hot-reload), engine-programmer (data asset feasibility); revised after the 2026-10-24 design review (game-designer, systems-designer, qa-lead, network-programmer, unreal-specialist, security-engineer, creative-director), which moved the per-client acknowledgement into Session & Travel and made Shipping use `Default` only. `docs/engine-reference/unreal/` is silent on data assets, metadata, cooking and asset-edit delegates, so every engine statement here is general UE knowledge, **unverified against 5.8**, and is an ADR check item rather than a commitment.

### Core Rules

**CR-1 Everything gameplay-relevant is a tuning key.** Any number that changes simulation outcome, or that another player can observe (timing, range, count, speed, limit), is a tuning key owned by exactly one system. A hardcoded gameplay number is a defect (coding standard). Engine constants, asset references and localisable text are not tuning.

**CR-2 Authoring.** Tuning lives in cooked data assets of one class composed of per-owner-system sub-structs. The field name is the canonical key. Designers edit assets in the editor with no recompile. Several instances are *presets*: `Default` (shipped baseline, equal to every owner's Default column), plus optional `Playtest` and `Solo` (for example `TeamSize` 1 for bot testing). A catalog asset (working name `UStealthTuningCatalog`) lists presets by soft reference; each entry carries a `HostSelectable` flag. The catalog is cooked with the game so every preset ships even though none is hard-referenced (catalog with soft references versus Asset Manager always-cook registration is an ADR choice; engine behaviour unverified on 5.8, AC-27). **Shipping rule (enforced in code, not by data)**: a Shipping build always resolves `Default`; the `?Tuning=` option and `stealth.tuning.preset` are not compiled into Shipping, so a mis-flagged `HostSelectable` entry cannot alter a shipped match. `HostSelectable` is honoured only in non-Shipping builds (Development and Test, internal playtests). The user confirmed 2026-10-25 that Shipping uses `Default` only (OQ-6 closed); host-selectable presets stay a non-Shipping (internal playtest) tool.

**CR-3 The schema owns hard ranges, in code.** Every key has a C++ schema row: key, type, default, hard min, hard max, audience, latch class (CR-11). Editor metadata (`ClampMin/ClampMax/UIMin/UIMax`, audience tag) is a convenience mirror only, because metadata is stripped from cooked builds and does not constrain values loaded from disk. Allowed field types: `bool`, `int32`, `float`, enums (no arrays, maps, vectors or doubles; a double would change the bits the hash covers). Enums have explicitly declared integer values and are stored and hashed by that value. A range-shaped value (a min/max pair, for example the prototype's `NpcIdleTimeRange` `FVector2D`) becomes two scalar keys (for example `CrowdIdleMinSec`, `CrowdIdleMaxSec`) plus invariant I-5. Widening a hard range is a code change plus a GDD change; that friction is intentional: designers tune freely inside the hard range, and the hard range itself is a deliberate safety contract whose source table is an ADR choice (OQ-2).

**CR-4 Server authority and preset source.** Only the server (the listen-server host in the prototype) chooses, loads, validates and freezes tuning. The preset *name* lives in a server-side **preset store** owned by TDC and scoped to the game instance (working name: a `UGameInstanceSubsystem`), because `ServerTravel` creates a new GameMode and re-runs map initialisation, so anything parsed "once at init" would be lost for round 2 and later. The store is written once, when the host opens the session, from the host's own launch option `?Tuning=<PresetName>` (non-Shipping builds only, CR-2; absent means `Default`), or later by the non-Shipping `stealth.tuning.preset` command (CR-13). Round passes the stored `PresetId` to Session & Travel's `RequestRoundTravel`, which carries it server-side in the travel URL as `?Preset=` (Session & Travel rule 11.3). At each `Preparing`, TDC calls `ResolveRoundPreset(Name)` on the stored name: it matches case-insensitively against the catalog, rechecks `HostSelectable` and the Shipping rule, and selects `Default` with a warning for anything else (unknown, padded, or containing path or separator characters). If the travel-URL value disagrees with the store, the store wins and a warning is logged. Per-client login options are never read for it. Clients never supply a tuned value, a preset name or a range; the only client-to-server tuning datum is the `ClientHash` the client recomputes from what it *received*, carried inside Session & Travel's round acknowledgement (CR-9).

**CR-5 Global and uniform.** One value set per round, identical for every participant, team, viewer class and NPC. Per-player, per-team, per-viewer and NPC-versus-player variants (no `Npc*` twin of an observable duration or range) are forbidden: they would be a tell and would break team-blindness. A handicap system, if ever wanted, is a new GDD.

**CR-6 Validation ladder (server, at load).** (a) NaN or infinity in a float, or an enum value outside its declared set: the field takes its schema default, warning logged (bools and ints need no finiteness check). (b) Out of hard range: clamped to the nearest bound, warning naming key, authored value and clamped value. (c) Cross-field invariants I-1 to I-4 (Formulas) are evaluated on the clamped set; if any fails, the **whole preset is rejected** (never partly repaired: a silent repair hides designer intent). Fallback ladder: requested preset, then `Default`, then schema defaults (the C++ initialisers, pinned equal to the GDD defaults by test), then `Failed`: the round does not start, Round returns to `Lobby` and logs `TuningLoadFailed`. If the requested preset *is* `Default`, the repeated rung is skipped: a failing `Default` goes straight to schema defaults (`FallbackLevel` 2, `PresetId` `SchemaDefaults`). `FallbackLevel` (0 requested, 1 Default, 2 schema defaults) and a `ValidationReport` (clamps, rejections, warnings) are recorded. A catalog preset that needs any clamp is a CI failure; the runtime clamp is only a safety net.

**CR-7 Freeze and stamp.** On entering `Preparing`, Round calls TDC's `BeginResolve()` (TDC never calls Round; the dependency is one-way) and the server runs CR-6, freezes the result into a **ValidatedTuning** object that all server systems read (never the raw asset), computes `TuningHash` (all keys) and `ClientHash` (public keys) per F-2, and raises `OnTuningReady(TuningHash)`. The set is immutable until `Reset` (the only exception is the editor-only PIE hot-reload of CR-12). The next round re-runs the pipeline, so the preset may change between rounds, never within one. Round writes `TuningHash`, `PresetId`, `SchemaVersion`, `FallbackLevel`, `Revision` and the warnings into the header of the server-only Round Event Log. `TuningHash` is **not replicated** to any client because no client consumes it. This is not a secrecy rule: `PresetId` is public and every preset ships in every client's paks, so preset identity is not confidential. Clients read `TuningHash` only from the Round Event Log after `Ended`, under the Visibility Policy. The only hash a client handles is `ClientHash`, which it recomputes itself (CR-9).

**CR-8 Audience and replication.** Each key is `ServerOnly` (the default; an unspecified key is never `All`) or `All`. An owner table may label an `All` key `Client` when no server code reads it (a client-only consumer: presentation, owner prediction, animation): `Client` is the same audience as `All` (public struct, replicated in the snapshot, covered by `ClientHash`); the label only records that the server has no consumer (reconciliation ruling 2026-10-24). A row an owner marks as a user setting (CR-14) is not a preset field: its default and clamp bounds are client-local settings metadata, never replicated or hashed. Audience is structural: the schema is split into a *public* struct and a *server-only* struct and only the public struct replicates, so a mis-tagged field cannot leak (a reflection test ties struct membership to the schema row's audience, AC-7). Every `All` key must name a client consumer (an input, animation, HUD or prediction need). The snapshot is one struct on the GameState (not PlayerState: no per-player hook) holding the public values, `SchemaVersion`, `PresetId`, `RoundIndex` (the epoch source, so the client learns the epoch from the same struct), `Revision` and a `bValid` flag. It replicates with ordinary property replication plus RepNotify (not initial-only: that cannot carry a hot-reload revision and is unsafe for rejoiners) at full float precision (no quantisation), and it must arrive as **one atomic unit** (default per-field delta replication could deliver a partial set; atomic struct or custom net serialisation, engine detail unverified on 5.8 including under Iris -> ADR). It carries no transmitted hash: the client recomputes `ClientHash` from the fields it received (CR-9). The snapshot is written only after the freeze, so in a new GameState `bValid` is false until then, and values from a previous GameState are invalid from travel start. Until `bValid`, client tuning reads return an explicit *invalid* result and the client holds its participant in a loading state; it never falls back to local defaults or assets. **Pillar-1 rule for the loading state**: a participant entering a round for the first time has no pawn observable by other clients until it is tuning-`Ready` (no frozen, idle or popping-in figure). Round owns pawn release and releases first-entry pawns at a common moment (change request), so per-client readiness timing is never visible. No participant joins after `Frozen`: Session & Travel admission is rejoin-only in-round, and a pawn appearing without a preceding death would identify a human. A **rejoining** participant keeps pawn continuity per Session & Travel rule 22: its pawn is not hidden or re-spawned for tuning reasons, and its client issues no commands until its snapshot for the current epoch is valid. The loading state is visible only on that participant's own screen. `ServerOnly` means not replicated, not secret: every preset ships in every client's paks, so no key may be treated as confidential.

**CR-9 Acknowledgement (one acknowledgement, owned by Session & Travel).** TDC has **no RPC, timer or kick rule of its own**. The per-client tuning check rides Session & Travel's single round acknowledgement (rule 11.6). When a client holds a valid snapshot for the current epoch and its PlayerController exists (either may arrive first; seamless travel **replaces** the PlayerController and PlayerState in UE 5.8.4, so "exists" means the new controller, which exists before Session sends the acknowledgement request; ADR-0005 E5, R-6), it recomputes `ClientHash` (F-2) from the values it received, and that value is the tuning field of the acknowledgement echo. It replaces the full `TuningHash` that Session & Travel currently names (change request). The server compares it with its own `ClientHash` for the same epoch (`Epoch` = `RoundIndex` plus `Revision`, both from the snapshot). Session & Travel owns the rest: the resend mechanism (an unchanged replicated property is never re-sent, so its resend must be explicit), the absolute deadline `TravelLoadTimeoutSec` (measured by the server from travel start, so a client cannot delay its own timer and a rejoin does not reset a cumulative wait), the disconnect reason (`ParamsMismatch`, then Roster `Disconnect`), duplicate and stale handling, and rate limiting. TDC adds no quorum rule and **never aborts a round because of client reports**, so colluding clients cannot cancel a round through tuning. Server-side tuning-ready bookkeeping is keyed by `ParticipantId` (never by connection, PlayerController or PlayerState, which are replaced at seamless travel and on rejoin), is never replicated (a replicated "acked" flag would publish the human roster), and is cleared at every new epoch. A participant is tuning-`Ready` when Session reports its acknowledgement accepted (`OnParticipantTravelComplete`). **Server self-check**: after freezing, the server recomputes `ClientHash` from the serialised public struct it is about to replicate and compares it with the hash of the validated set; a difference raises `TuningSnapshotFault`, the pipeline enters `Failed` and Round returns to `Lobby`. This is the only server-fault path. A correct hash proves only that the public data was delivered intact. It is a readiness handshake, not a security control: it never proves the client binary is unmodified, and Anti-Cheat must not consume it. The listen-server host is marked ready locally through the same accessor path (it receives no RepNotify). Session's acknowledgement is server-requested (`ClientRequestRoundAck` / `ServerRoundAck`) and also carries Character Pool's `PoolHash` beside `ClientHash`; a mismatch of either takes Session's `ParamsMismatch` path (Session & Travel rule 11.6).

**CR-10 One access path.** All consumers read through a **world-scoped** tuning accessor (a world or game-instance subsystem resolved from a world context, so multi-client PIE reads the right world; engine detail -> ADR) with two faces. Server code uses `GetServerTuning()` (all keys, valid only after freeze; no pawn, controller, team or viewer parameter, so per-participant values cannot be requested, CR-5). Client code uses `GetClientTuning()` (public keys only, from the replicated snapshot only). The client face is typed to the public struct, so a server-only key cannot be expressed in client C++ (compile-time guarantee, AC-28). Any reflective or Blueprint path that still asks for one returns *invalid* and logs (an `ensure` in development, never a `check` that aborts). The host's client-side code uses the snapshot path too. Client code holds no reference to the tuning asset class (code-review checklist plus an automated reference check). A hard client/server code boundary (a server-only module) is a Dedicated Server stage option, not an MVP requirement.

**CR-11 Latching.** Each key has a latch class: `Round` (applied at `Preparing`, never changes within a round: `TeamSize`, `NpcCount`, `NpcKillStrikeLimit`, `TargetRoundMinutes`), `Action` (captured when the action starts, so a mid-action change never retimes it: durations and cooldowns), `Use` (read at point of use: ranges, thresholds), or `Session` (read once when the session opens and not refrozen per round: Session & Travel's admission and liveness keys, which act before and between rounds; they are validated by CR-6 at session open, a preset change between rounds does not alter them, and a cross-field invariant that mixes `Session` and round keys is evaluated at each `Preparing` against the session's values; added at the 2026-10-24 reconciliation).

**CR-12 Hot-reload is PIE/editor only.** In a PIE world of an editor build, a committed property edit on a preset in use triggers re-validation (committed value changes only, not every interactive slider tick, undo step or reimport; debounced and routed to the PIE server world's subsystem; engine detail -> ADR). The **live set** after an edit is the frozen `Round`-latched values plus the edited `Action` and `Use` values; CR-6 and F-2 run on that live set, so the snapshot never carries an edited `Round` value before `Reset` and the HUD and server agree. Edits to `Round`-latched keys form a *pending* set that applies at the next `Reset` and is re-validated at the next `Preparing`; a pending edit that would fail validation logs a warning at edit time, and at `Preparing` the CR-6 ladder applies. If the live set passes, it is published as a new frozen set with `Revision` + 1, but only if the live-set hash changed. The snapshot re-replicates, clients recompute `ClientHash` under the new epoch, and `OnTuningChanged(Revision)` fires. A PIE re-check is advisory: a mismatch logs an error, but it never demotes a client from `Ready` and never removes a pawn. `Action` values already captured by in-flight actions, on server and client, are never retimed. A failing edit is rejected and the previous set stays live. The round is flagged `HotReloaded` in the log header so telemetry and evidence exclude it. In packaged, standalone-server and shipping builds the hook is compiled out (`WITH_EDITOR` guard): assets are read once per round and never watched. Consumers hold the validated copy, so an edit can only reach live state through CR-6.

**CR-13 No back doors.** Tuning fields are never `Config` properties, and the owning classes carry no `Config` or per-object-config class flags (no ini, `Saved/Config` or command-line override). No console variable overrides a tuning value in shipping. Non-shipping builds may offer `stealth.tuning.dump` (preset, hashes, fallback level, values) and a server-side `stealth.tuning.preset <name>` taking effect at the next `Preparing` through `ResolveRoundPreset` and CR-6. Both are compiled out of Shipping (the preset command also out of Test), and their code carries a unique sentinel marker literal for the AC-22 binary scan. Non-shipping builds (Development, Test) are internal only and are never distributed outside the team. The prototype `UStealthSettings` (`Config = Game`, read from `DefaultGame.ini` and locally overridable from `Saved/Config`) is a live back door until migrated: removing its gameplay fields is a gate before any external playtest (Cross-References, migration).

**CR-14 Client-local settings are not tuning.** Preferences affecting only the local player's presentation (sensitivity, FOV, volume, UI scale, accessibility; Settings & Accessibility) are not tuning keys and never reach the server. If such a preference could change what others see or any simulation outcome it is a Pillar-1 defect, not a setting.

**CR-15 Knob Registry Convention (how a downstream GDD registers).** Every GDD that owns a tunable:
1. lists it in its own **Tuning Knobs** section as a table with columns `Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Unit | Effect / breaks if too high or low`;
2. uses a globally unique `PascalCase` key ending in a unit or kind suffix from {`Sec`, `Ms`, `Minutes`, `Cm`, `CmPerSec`, `PerSec`, `PerMin`, `Hz`, `Deg`, `Pct`, `Mult`, `Frac`, `Gain`, `Sigma`, `Exp`, `Count`, `Size`, `Limit`, `Depth`, `Asset`} (`Asset` = a soft object reference to a data or behaviour asset, added 2026-10-25 for the Crowd asset keys; list extended at the 2026-10-24 reconciliation to cover the registered owner tables); enums end in `Mode`; booleans start with `b`; `Min`/`Max` name either half of a range pair (`CrowdIdleMinSec`/`CrowdIdleMaxSec`, covered by I-5) or a one-sided bound (`CrowdMaxIdleSec`), and never mean minutes; one quantity never has two names. Keys about crowd or NPC behaviour use the `Crowd` prefix, not `Npc` (AC-15 allowlists only `NpcCount`, `NpcKillStrikeLimit` and `TeamSize`);
3. is the single owner; another GDD using the value references the owner's key and never restates or alters it (a "(default N)" mention in prose is allowed);
4. states the audience (`ServerOnly` unless a client needs it, with the named consumer) ;
5. separates *hard range* (schema clamp) from *safe range* (recommended; leaving it logs a warning, never clamps);
6. sends any cross-field constraint to this GDD as a change request, to be added to the invariant list;
7. is mirrored by the orchestrator into `design/registry/entities.yaml` constants with `source:` = the owner GDD;
8. reviews, in its own GDD, any key that times NPC replacement, informant respawn, crowd sizing or another crowd-versus-player event as a possible identity tell (Pillar 1), and states the result in its knob row.
The aggregate list is therefore the union of the owner tables; the table below covers the eight keys fixed before the other GDDs existed plus this system's own constant. Implementation enforces the convention with a schema test (AC-3a) and a design-doc lint (AC-3b).

### Canonical Tunables

Definitions of meaning live in the owner GDDs; this table fixes name, default and ranges.

| Key | Type | Owner system | Default | Hard range (clamped) | Safe range (warn outside) | Audience | Latch | Meaning |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| `TeamSize` | int | Round & Match Framework | 5 | 1 to 5 | 2 to 5 | All | Round | players per team (a real match is 5; 1 to 4 for tests) |
| `NpcCount` | int | Crowd & NPC Behaviour | 50 (user-locked ~40 changed to 50 by the user 2026-10-25) | 6 to 100 | 35 to 50 | ServerOnly | Round | NPC characters in the crowd (the hard minimum 6 rests on I-2 alone; the safe minimum 35 is the Level Socket Contract D6 bound at the defaults, I-LSC-1; the default 50 gives 60 characters with 10 players, above the ~50-character performance evidence so far, so it is provisional pending spike R3 at 60 characters; above 50 is outside the safe range) |
| `AssassinationRangeCm` | float | Assassination & Defence Reversal | 200 | 100 to 250 | 150 to 250 | All | Use | strike reach in cm (about 2 m); capped at 2.5 m (lowered from 3 m at the 2026-10-24 reconciliation, Assassination request) because a longer reach than the strike animation shows would itself be a tell (Assassination owns the reach-versus-animation check, I-ASN-1) |
| `DefenceDurationSec` | float | Assassination & Defence Reversal | 4.0 | 3.0 to 5.0 | 3.0 to 5.0 | All | Action | hold-to-prepare time (safe range equals the locked tuning range 3 to 5 s) |
| `DefenceCooldownSec` | float | Assassination & Defence Reversal | 60 | 30 to 120 | 45 to 75 | All | Action | cooldown from activation, used or unused |
| `NpcKillStrikeLimit` | int | NPC-Kill Penalty & Informants | 3 | 2 to 5 (1 only in a non-selectable test preset with `bTestAllowsStrikeLimitOne`; user decision 2026-10-25) | 2 to 4 | All | Round | removal on the Nth NPC kill |
| `ObjectiveInteractSec` | float | Objectives Chain | 3.0 | 2.0 to 6.0 | 2.5 to 4.5 | All | Action | hold-to-interact time at an objective (hard range raised from 1.0-10.0 at the 2026-10-24 reconciliation, Objectives OC-14: a tend shorter than 2 s reads like an ambient touch, OQ-9 answered) |
| `TargetRoundMinutes` | float | Round & Match Framework | 12 | 5 to 30 | 8 to 15 | ServerOnly | Round | **non-binding** design target; no timer exists; logged only |
| `BlindGuessWarnPct` | float | Tuning Data & Config | 50 | **schema constant** (code; not a preset field, not hashed) | -- | ServerOnly | -- | I-4 threshold on `100 * BlindGuessSuccess` (provisional pending playtest); a constant so no preset can loosen its own gate |

`TuningAckTimeoutSec` (previously listed here) is **retired** (2026-10-24 review): TDC no longer has its own acknowledgement, and Session & Travel's absolute `TravelLoadTimeoutSec` bounds the wait (CR-9).

Audience rationale: `All` keys are those a client needs for its own presentation or input feedback (interaction and defence progress timing and animation, HUD limits, team size); being uniform for everybody they carry no identity information. `NpcCount` stays `ServerOnly` (clients need no count; it is still derivable from the shipped presets, so Round and Crowd must keep the total pawn count from revealing the human count, Edge Cases); `TargetRoundMinutes` has no client use.

### Registered Owner Keys (reconciliation 2026-10-24)

Registration index per CR-15. The owner's Tuning Knobs table is authoritative for every name, default, range, audience and latch; this table only records that the owner registered (counts are key names in the owner table at the 2026-10-24 reconciliation, canonical keys counted once under their owner). Owner cross-field constraints are listed under Formulas, "Owner invariants".

| Owner GDD (index #) | Keys | Audience | Latch | Owner invariants | Notes |
| ---- | ---- | ---- | ---- | ---- | ---- |
| Round & Match Framework (6) | 17 (incl. `TeamSize`, `TargetRoundMinutes`; `RevealPawnWaitMaxSec` added at the 2026-10-24 cross-review, S-B4) | `TeamSize`, `RevealPawnWaitMaxSec` All; rest ServerOnly | Round, Action, Use | I-RMF-1, I-RMF-2, W-RMF-3, W-RMF-4 | `DisconnectGraceSec` hard range 3-120 (supersedes Roster's 10-120); F6's `PreparingWatchdogEff` is computed at runtime, not a preset invariant |
| Session & Travel (5) | 13 (sub-struct `Session`; recounted at the 2026-10-24 cross-review, C-W16) | ServerOnly; `HostLossTimeoutSec`, `ClientAutoRejoinIntervalSec` All, delivered in Session's `SessionClientParams` at join (not in the round snapshot) | Session | I-ST-1 to I-ST-3 | F4 adequacy is checked offline against measured RTT, not at preset load |
| Participant State & Roster (2) | 2 (`AppearanceHistoryDepth` log-only, `RosterTransitionLogDepth` dev-only) | ServerOnly | Round | -- | Roster table converted to CR-15 columns at this reconciliation |
| Input & Interaction Framework (3) | 25 (`LatencyWarnRttSec` retired 2026-10-25, user decision: HUD's `HudLatencyWarnRttSec` is the one key) | mixed | Use (mostly) | I-IIF-1, I-IIF-2 | `PrepareLatencyForgivenessSec` retired; `InteractCancelRangeCm` replaced by `InteractCancelHysteresisCm`; owns `InteractRangeCm` (prototype `ObjectiveInteractRange`) |
| Character Pool & Appearance (4) | 3 (`DeckCopiesFloorCount`, `DeckActiveAppearancesMinCount`, `DeckActiveAppearancesMaxCount`) | ServerOnly | Round | I-3 (rewritten), I-5 (range pair) | `PoolResidentTimeoutSec` retired; `AppearancePalette` is pool content, not a key |
| Level Socket Contract (8) | about 79 (incl. `LayoutExactRefineMaxCount`, int, default 2,000, hard 200-20,000, ServerOnly, Round; added 2026-10-25 by user decision for ADR-0014 §5.2; excluding the two SCL traversal heights it references) | ServerOnly | Round | I-LSC-1 to I-LSC-4 | retired: `InformantRespawnSocketsMinCount`, `EmergenceStarvationSec`, `ZoneSeparationEuclidCm`; its table states Audience and Latch once in the preamble (ServerOnly, Round) instead of per row, accepted |
| Shared Character & Locomotion (9) | 51 (recounted 2026-10-24, C-W16) | `Client` (= All) and ServerOnly | Round, Use | I-SCL-1 to I-SCL-14 | `InformantMaxSpeedCmPerSec` and `FootstepStrideWalkCm` retired; owns `TraversalVaultHeightCm`/`TraversalClimbHeightCm` (LSC references them) |
| Camera System (10) | 48 (6 marked * are CR-14 user settings) | `Client` (= All) | Use, Round | I-CS-1 to I-CS-5 | new at its review: `ViewPitchDownLimitDeg`, `ViewPitchUpLimitDeg`, `EdgePushLatchSec`, `CameraEdgeSoftMinGain`, `EyeMaskBeyondHz` |
| Information Visibility Policy (7) | 4 (`ConcealCullDelayMs`, `TeamPositionFeedHz`, `TeamPositionFeedGridCm`, `bConcealServerCull`) | ServerOnly | Round | -- | its test constants (`LeakAucMax`, `HidePosteriorFactor` and others) are harness configuration, not keys |
| Crowd & NPC Behaviour (11) | 53 (incl. `NpcCount`; key names, paired rows counted per name; recounted 2026-10-24, C-W16; plus the three soft-reference asset keys `CrowdMotionProfileAsset` (`UCrowdMotionProfileData`), `CrowdFullStateTreeAsset` (`ST_CrowdFull`) and `CrowdRestrictedStateTreeAsset` (`ST_CrowdRestricted`) added 2026-10-25 by user decision for ADR-0016 §3, §5) | ServerOnly | Round, Action, Use | I-CNB-1 to I-CNB-5; offline O-CNB-6, O-CNB-7 | `CrowdHeadHoldIntervalSec` replaces `CrowdHeadHoldPct`; prototype `NpcIdleTimeRange` is `CrowdIdleMinSec`/`CrowdIdleMaxSec` |
| Concealment (Bushes) (12) | 11 | All (transition times), ServerOnly | Round, Use | I-CON-a to I-CON-e (F3), I-ASN-3 (= F3 f) | hard minimum 0.30 s for `BushExitCrossSec` and `BushStrikeEmergeSec` |
| Assassination & Defence Reversal (13) | 16 (incl. three canonical) | All and ServerOnly | Use, Action, Round | I-ASN-1 to I-ASN-8, W-ASN-9 | retired: `AttackerRecoverySec`, `BodyLifetimeSec`, `BlindStrikeRadiusCm`, `AssassinationConeHalfAngleDeg`, `AssassinationLagToleranceMult` (prototype mapping) |
| NPC-Kill Penalty & Informants (14) | 5 (incl. `NpcKillStrikeLimit`) | All (owner presentation), ServerOnly | Round, Action | I-NKP-1, W-NKP-2 to W-NKP-5 | `NpcKillStrikeLimit` hard minimum raised to 2 by user decision 2026-10-25 (OQ-12 closed; `L = 1` only with the test-preset flag); `ExileBeatSec` safe range 0.75-1.2 (bounded by I-HUD-3 and I-HUD-5; cross-review C-W2) |
| Objectives Chain (15) | 11 (incl. `ObjectiveInteractSec`) | All (owner ring), ServerOnly | Action, Round, Use | I-OC1 (warning), I-OC2, I-OC3 | |
| Procedural City Generator (16) | 32 (`CityGen*`; recounted 2026-10-24, C-W16) | ServerOnly | Round | I-PCG-1 to I-PCG-4 | does not use `TargetRoundMinutes` (OQ-7 answered); its table states Owner, Audience and Latch once in the preamble, accepted (as for LSC) |
| Teammate Markers (17) | 7 (1 user setting) | `Client` (= All) | Use | I-TM-1 | `TeammateMarkerEdgeInsetPct` replaced by HUD's `HudSafeMarginPct`; `TeammateMarkerDeathHoldCapSec` removed |
| Informant Pings & Team Communication (20; MVP since 2026-10-25) | 5 (`PingLifetimeSec`, `PingBucketSize`, `PingBucketPerSec`, `PingRangeCm`, `PingRangeSlackCm`; owner name `InformantPings`) | `PingLifetimeSec` Client; `PingBucketSize`, `PingBucketPerSec`, `PingRangeCm` All (the client mirrors the bucket and range); `PingRangeSlackCm` ServerOnly | Use, Round | I-PNG-1 (warn) | registered by user decision 2026-10-25 (system moved to MVP) |
| HUD (18) | 21 | `Client` (= All) | Use | I-HUD-1 to I-HUD-7 | owns `HudLatencyWarnRttSec` (IIF UI-7 threshold); `HudReadHoldMult` (renamed from `HudReadHoldScale` by the HUD owner at reconciliation) is a CR-14-adjacent multiplier tied to text scale (VS) |
| Tuning Data & Config (1) | `BlindGuessWarnPct` (schema constant), catalog `HostSelectable` flags | ServerOnly | -- | I-1 to I-5 | |

**Names outside the CR-15.2 suffix list** (doc-lint findings at the reconciliation): **renames APPLIED everywhere 2026-10-25 (user decision 2026-10-25)** — `ConcealServerCullEnabled` -> `bConcealServerCull` (IVP/Concealment), `EyeMaskHideFails` -> `EyeMaskHideFailCount` (Camera), `StickDeadZone` / `StickOuterZone` -> `StickDeadZoneFrac` / `StickOuterZoneFrac` (Camera), `LayoutSliceWorkUnitsMax` -> `LayoutSliceWorkUnitsLimit` (LSC), `RespawnPoolMinExpected` -> `RespawnPoolMinExpectedCount` (LSC), ~~`HudReadHoldScale`~~ (HUD: renamed `HudReadHoldMult`, done 2026-10-24). AC-3a no longer fails on these. **One quantity, two names**: LSC's `ReachDetourMult` is held equal to `DetourMaxMult` (I-LSC-4); if the two can never differ, LSC should retire one (CR-15.3).

### States and Transitions

Server pipeline (one instance per round, held by the game-instance-scoped TDC subsystem, not by the GameMode, which travel recreates):

| State | Meaning | Enters on | Leaves to |
| ---- | ---- | ---- | ---- |
| `Idle` | no frozen set (Lobby / Reset) | process start, `Reset` | `Resolving` when Round calls `BeginResolve()` on entering `Preparing` |
| `Resolving` | stored preset name resolved (`ResolveRoundPreset`), asset loaded | `BeginResolve()` | `Validating`; or `Failed` if no rung of the ladder loads |
| `Validating` | CR-6 clamps and invariants | asset loaded | `Frozen`, or fall one rung and re-enter `Validating`, or `Failed` |
| `Frozen` | immutable, stamped, self-checked (CR-9), snapshot published, `OnTuningReady` raised | validation and self-check passed | `Idle` on `Reset`; stays `Frozen` with `Revision` + 1 on a passing PIE hot-reload publish (editor only, CR-12); `Failed` on `TuningSnapshotFault` |
| `Failed` | no valid set at any rung, or server self-check failed | ladder exhausted or `TuningSnapshotFault` | `Idle`; Round returns to `Lobby` and logs `TuningLoadFailed` or `TuningSnapshotFault` |

Per-client (client and listen host):

| State | Meaning | Leaves to |
| ---- | ---- | ---- |
| `NoSnapshot` | `bValid` false or snapshot from an old epoch; reads return *invalid*; participant held in loading | `SnapshotReceived` on RepNotify with `bValid` for the current `RoundIndex` |
| `SnapshotReceived` | values usable; `ClientHash` recomputed and handed to Session & Travel's acknowledgement once the PlayerController exists | `Ready` when Session reports the acknowledgement accepted; disconnected per Session & Travel's mismatch or deadline rules |
| `Ready` | normal play | `NoSnapshot` on travel start (new epoch). A PIE `Revision` change triggers an advisory re-check and never leaves `Ready` (CR-12) |

### Interactions with Other Systems

All interface names for not-yet-written GDDs are **provisional**. Call direction is one-way: Round and Session call TDC or read its outputs; TDC never calls them.

| System | Flows into TDC | Flows out of TDC | Interface owner |
| ---- | ---- | ---- | ---- |
| Round & Match Framework | `BeginResolve()` on entering `Preparing`; `Reset`; stored `PresetId` request for travel | `OnTuningReady(TuningHash)`, header fields for the Round Event Log, `Failed` result with reason | Round (phase), TDC (events, accessor) |
| Session & Travel | host-open launch option `?Tuning=` (into the preset store); acknowledgement echo carrying the client's `ClientHash`; `OnParticipantTravelComplete` (= tuning-`Ready`) | `SchemaVersion` (exposed to Session as `TuningSchemaVersion`) for the join gate; server `ClientHash` per epoch for the comparison; `PresetId` for the travel URL `?Preset=` | Session & Travel (acknowledgement, deadline, mismatch handling); TDC (hash, preset store) |
| Participant State & Roster | -- | `TeamSize`, read by Round or Session and injected into `BeginMatch` (Roster reads no config) | Roster |
| Information Visibility Policy | audience vocabulary | public/server-only split; not a subject Fact, so `CanViewerKnow` is not consulted | Visibility Policy (vocabulary only) |
| Every owner GDD | its knob table, schema row request | `GetServerTuning()`, `GetClientTuning()`, `OnTuningChanged` (PIE) | TDC |
| Telemetry, Anti-Cheat (later) | -- | `TuningHash`, `PresetId`, audience flags (after `Ended`; the acknowledgement is not an anti-cheat signal) | TDC |

## Formulas

TDC defines three numeric rules (validation, canonical hash, derived design indicators), five invariants and a size budget.

**F-1 Validate**

`Effective(k) = Default(k)` if `v` is a non-finite float or an enum value outside its declared set; otherwise `Effective(k) = clamp(v, HardMin(k), HardMax(k))` (bools pass unchanged)

| Variable | Type | Range | Source | Description |
| ---- | ---- | ---- | ---- | ---- |
| `v` | bool / int32 / float / enum | any | data file (authored preset) | authored value of key `k` |
| `Default(k)` | same as `k` | inside hard range | schema constant (pinned to GDD default) | fallback for non-finite or undeclared input |
| `HardMin(k)`, `HardMax(k)` | int / float | per key (Canonical Tunables) | schema constant (code) | enforced bounds |

**Output Range**: `HardMin(k)` to `HardMax(k)`. NaN and infinity can only occur in float fields; the finite test runs **before** clamping (clamp of NaN is NaN). +Infinity and -Infinity take the default, not a bound. A value exactly on a bound is accepted with no warning.
**Example**: `DefenceDurationSec` authored `7.5` gives `5.0` (warning); `ObjectiveInteractSec` authored NaN or -Infinity gives `3.0`; `AssassinationRangeCm` authored `50` gives `100`; `NpcKillStrikeLimit` authored `0` gives `2` (hard minimum 2 since 2026-10-25) and `9` gives `5`; `NpcCount` authored `0` gives `6`; `NpcKillStrikeLimit` authored `3` stays `3`.

**F-2 Canonical hash**

`H = Hash64( u32le(SchemaVersion) || for each key k, sorted by byte-wise (memcmp, case-sensitive) comparison of its ASCII name: u8(len(k)) || ascii(k) || u8(typeTag(k)) || canon(Effective(k)) )`, computed once over all keys (`TuningHash`) and once over the public keys only (`ClientHash`). Schema constants that are not preset fields (for example `BlindGuessWarnPct`) are not hashed.

| Variable | Type | Range | Source | Description |
| ---- | ---- | ---- | ---- | ---- |
| `SchemaVersion` | int32, encoded as 4 bytes little-endian | 1 to 2^31-1 | constant; bumped when any key is added, removed, renamed or re-typed, when a key's audience or latch class changes, or when an enum's declared values change | incompatibility marker (exposed to Session & Travel as `TuningSchemaVersion`) |
| `k` | ASCII string, 1 to 255 bytes | unique across the game | schema | sort key; `len(k)` encoded as one byte |
| `typeTag` | uint8 | bool = 0, int = 1, float = 2, enum = 3 | schema | removes cross-type ambiguity |
| `canon(x)` | bytes | 1, 4 or 8 | calculated | bool: 1 byte (0 or 1); int32 and enum: widened to int64, 8 bytes little-endian (enum = its declared value); float: 4 bytes IEEE-754 little-endian with `-0.0` mapped to `+0.0` |
| `Hash64` | uint64 | 0 to 2^64-1 | constant (algorithm named in the ADR; a stable 64-bit non-cryptographic hash such as FNV-1a-64, not the engine's `GetTypeHash`) | not a security primitive |

**Output Range**: 64-bit value; equal inputs on any platform give equal outputs; any key change changes the value (collision odds negligible for this use). The `-0.0` rule matters only to the hash function itself (every float hard minimum is above zero, so F-1 never emits `-0.0`); it is tested on the hash function directly (AC-5).
**Example**: key `TeamSize`, int 5, encodes as `08 | 54 65 61 6D 53 69 7A 65 | 01 | 05 00 00 00 00 00 00 00`; key `DefenceDurationSec` (18 bytes), float 4.0, encodes as `12 | "DefenceDurationSec" | 02 | 00 00 80 40`. The golden test vector is a **committed fixture** (a fixed `SchemaVersion` and one key of each type tag, AC-5), not the `Default` preset, which changes on every retune; `Default` is checked only for equal hashes across the CI platform matrix.

**F-3 Derived design indicators** (computed at validation in double precision; used for invariants and warnings, not gameplay)

`CrowdRatio = NpcCount / (2 * TeamSize)` · `DutyCycle = DefenceDurationSec / DefenceCooldownSec` · `p = TeamSize / (TeamSize + NpcCount)` · `BlindGuessSuccess = 1 - (1 - p)^L` · `ExpectedBlindKills = L * p / (1 - p)` · `MissToHit = NpcCount / TeamSize`, with `L = NpcKillStrikeLimit`

| Variable | Type | Range | Source | Description |
| ---- | ---- | ---- | ---- | ---- |
| `NpcCount`, `TeamSize` | int | per hard range | data file | as in Canonical Tunables |
| `L` (`NpcKillStrikeLimit`) | int | 2 to 5 (1 only with the test-preset flag) | data file | number of NPC kills that removes a player: a blind guesser survives `L - 1` misses and the `L`-th miss removes them, so they get at most `L` attempts that can miss |
| `DefenceDurationSec`, `DefenceCooldownSec` | float | per hard range | data file | as in Canonical Tunables |
| `CrowdRatio` | double | 1.0 to 50 after validation (I-2) | calculated | NPCs per player |
| `DutyCycle` | double | 0.025 to 0.125 after validation (I-1) | calculated | share of time a defender can be prepared |
| `p` | double | 1/101 to 1/3 after validation (I-2 caps it at 1/3) | calculated | chance a random non-teammate target is an enemy player (teammates carry markers, so a blind guesser skips them); a no-information floor, not a model of real targeting, which reads tells |
| `BlindGuessSuccess` | double | 0.0099 to 0.868 after validation | calculated | chance a player who guesses uniformly at random lands at least one enemy kill before the `L`-th miss removes them |
| `ExpectedBlindKills` | double | 0.01 to 2.5 after validation | calculated | expected enemy kills a uniform blind guesser makes before removal (negative binomial mean; `1 - p >= 2/3`, so no division by zero) |
| `MissToHit` | double | 2 to 100 after validation (I-2) | calculated | expected NPC kills per enemy kill for a blind guesser (informational) |

**Output Range**: as in the table. Ignored on purpose (they make blind guessing worse, so the floor is conservative): the exposure cost of a visible miss, and reversal by a prepared defender. Ignoring reversals makes `BlindGuessSuccess` and `ExpectedBlindKills` about 7 to 9 percent optimistic for the guesser at the defaults (Assassination F5), which is the safe direction for I-4.
**Example (defaults)**: `CrowdRatio = 50 / 10 = 5.0`; `DutyCycle = 4 / 60 = 0.0667`; `p = 5 / 55 = 0.0909`; `BlindGuessSuccess = 1 - 0.9091^3 = 0.249`; `ExpectedBlindKills = 3 * 0.0909 / 0.9091 = 0.300`; `MissToHit = 10` (recomputed for `NpcCount` 50, 2026-10-25). **Example (rejected in a public preset)**: `NpcCount` 15, `TeamSize` 5, `L` 3 gives `p = 0.25`, `BlindGuessSuccess = 1 - 0.75^3 = 0.578` (above the 50 percent threshold), `ExpectedBlindKills = 1.0`.

**Invariants** (evaluated on clamped values; failure rejects the whole preset, CR-6):
- **I-1** `DefenceCooldownSec >= 8 * DefenceDurationSec` (float comparison; `8 *` is exact). Rationale, Pillar 3: being unprepared must be the normal state, so a defender can be prepared at most 12.5 percent of the time. Inside the safe ranges it never fires (8 x 5.0 = 40 is below the safe cooldown minimum of 45); it guards hard-range corners (for example duration 5 with cooldown 30 to 39).
- **I-2** `NpcCount >= 2 * TeamSize` (`CrowdRatio >= 1.0`; fewer NPCs than players makes the crowd a minority and breaks Pillar 1).
- **I-3** `2 * TeamSize + NpcCount >= DeckCopiesFloorCount * DeckActiveAppearancesMinCount` (Character Pool's deck floor, 4 x 6 = 24 at the defaults; replaces the provisional `>= 12`, reconciliation 2026-10-24). Scoped like I-4: a breach rejects `Default` and `HostSelectable` presets and is only a warning in non-selectable test presets (the deck then runs under Character Pool's `Warn` floor policy). At `TeamSize` 5 it needs `NpcCount >= 14`, which I-LSC-1 already exceeds; it binds for small test teams (for example `TeamSize` 1 needs `NpcCount >= 22` in a selectable preset).
- **I-4** (catalog presets flagged `HostSelectable`, plus `Default`): `100 * BlindGuessSuccess < BlindGuessWarnPct` (schema constant 50, provisional pending playtest; whether to gate relative to `Default` instead is OQ-3). A breach in such a preset is a CI failure and, at runtime, rejects the preset (CR-6 ladder), because Pillar 2 forbids blind guessing becoming a reliable alternative to reading tells in public matches. In non-selectable test presets (for example `Solo`) a breach is only a warning. Lowest `NpcCount` passing I-4 at `TeamSize` 5: `L` 1 -> 10 (I-2 binds), 2 -> 13, 3 -> 20, 4 -> 27, 5 -> 34. Within the safe ranges (`L` 2 to 4, `NpcCount` 35 to 50) I-4 always passes; `L` 5 is outside the safe range and with `NpcCount` 30 to 33 is rejected.
- **I-5** For every registered range pair (`...MinSec`/`...MaxSec` and similar): `Min <= Max`.
- **Owner invariants** (registered at the 2026-10-24 reconciliation per CR-15.6). Each owner GDD defines the meaning and rationale; TDC evaluates them in the CR-6 ladder on the clamped set, together with I-1 to I-5. "Reject" fails the whole preset; "warn" logs and keeps it. A constraint that needs a measured or layout-time quantity is not a preset invariant: it is marked **offline** and checked by its owner's CI or load-time rule instead.

| ID | Owner | Constraint (clamped values) | On failure |
| ---- | ---- | ---- | ---- |
| I-RMF-1 | Round | `MinPlayersPerTeamCount <= TeamSize` | reject |
| I-RMF-2 | Round | `EndedAutoResetSec = 0` or `EndedAutoResetSec >= EndedMinDwellSec` | reject |
| W-RMF-3 | Round | `DisconnectGraceSec <= RejoinWindowSec - 30` (otherwise F4's `G_eff` clamp applies) | warn |
| W-RMF-4 | Round | `DisconnectGraceSec >= 10` outside the `Solo` preset | warn |
| I-ST-1 | Session | `SessionHardTimeoutSec >= SessionSoftTimeoutSec + 2` | reject |
| I-ST-2 | Session | `SessionHeartbeatIntervalSec = 0` or `0.25 <= SessionHeartbeatIntervalSec <= SessionSoftTimeoutSec / 3` | reject |
| I-ST-3 | Session | `ShutdownFlushSec >= 0.3` | reject |
| I-IIF-1 | IIF | `TransportBucketSize >= 2 * (sum of the per-pawn Bucket*Size keys)` | reject |
| I-IIF-2 | IIF | `TransportBucketPerSec >= 2 * (sum of the per-pawn Bucket*PerSec keys)` | reject |
| I-LSC-1 | LSC | D6 bound: `NpcCount >= max(ceil(2 * (ceil(T(1 - s)/s) + W_min - 1) / q), ceil(LayoutWalkableCellsMinCount / WalkableCellsPerPawnMaxCount) - 2T)` with the symbols of LSC's D6 formula (35 at the defaults) | reject |
| I-LSC-2 | LSC | `(1 + LayoutMaxReseedAttemptsCount) * LayoutAttemptTimeoutSec <= LayoutTotalBudgetSec` | reject |
| I-LSC-3 | LSC | `EmergenceWitnessRangeCm >= SightlineMaxCm` | reject |
| I-LSC-4 | LSC | `ReachDetourMult = DetourMaxMult` | reject |
| (layout) | LSC | each candidate zone has at least `TeamSize + 1` start cells: a layout predicate (no key holds the count), checked by LSC validation | offline |
| I-SCL-1 to I-SCL-9, I-SCL-12, I-SCL-13 | SCL | as listed in SCL Tuning Knobs (speed ordering, traversal heights, animation-tier hysteresis, stop time, pace floor, turn radius, starvation catch-up, smoothing versus net rate, starvation versus `SessionSoftTimeoutSec`, cadence ordering) | reject |
| I-SCL-10, I-SCL-11, I-SCL-14 | SCL | listen-host frame time ≤ 1/60 s at the profiled load (harness frame-time capture; restated 2026-10-25, not a `NetServerMaxTickRate` startup check), the F8 animation budget at the measured per-tier cost (performance CI, warning), traversal reach versus authored clip length (content validator) | offline |
| I-SCL-15 | SCL (ADR-0010 §6) | engine movement-network configuration versus SCL keys, checked by a boot test (the engine reads these from the process-wide config, so they are pinned in project config, never set by tuning at run time): `MaxClientForcedUpdateDuration >= InputStarvationSec + max(WalkStopSec, RunStopSec) + 0.25` and `MaxMoveDeltaTime <= MAXCLIENTUPDATEINTERVAL = InputStarvationSec` | reject at boot (offline) |
| I-CS-1 to I-CS-5 | Camera | as listed in Camera Tuning Knobs (I-CS-1 reads IIF's `CameraToggleLockoutSec`; I-CS-4 reads Concealment's bush dome inner radius, a fixed asset fact of 90 cm, as a schema constant) | reject |
| I-CNB-1 | Crowd | `CrowdIdleMinSec >= SessionSoftTimeoutSec + 1` | reject |
| I-CNB-2 | Crowd | `CrowdMaxIdleSec >= max(CrowdIdleMaxSec, CrowdConversationMaxSec)` | reject |
| I-CNB-3 | Crowd | `(60 / CrowdAmbientCycleIntervalSec) * (1 - EmergenceExitCancelMaxPct / 100) >= AmbientEmergencePerSocketMinPerMin` | reject |
| I-CNB-4 | Crowd | `2 * 60 / CrowdAmbientCycleIntervalSec <= 60 / EmergenceSocketCooldownSec` | reject |
| I-CNB-5 | Crowd | F2 compensation validity: `(Lambda - r_N,j) * CrowdTellChainGapSec / 60 <= 0.5` for every tell j (rates from the Crowd interval keys) | reject |
| O-CNB-6 | Crowd | `60 / CrowdBushIntervalSec >= r_N,req` (IVP F5 at `HidePosteriorFactor` with the measured player rate) | offline (CI against the latest measured rate) |
| O-CNB-7 | Crowd | F1 `max_t rho_t / mean rho <= 1.5` | offline (Crowd, at layout load) |
| I-CON-a to I-CON-e | Concealment | F3 (a)-(e): enter duration covers swap plus `ConcealCullDelayMs`; exit crossing and bush-strike emergence at least `T_uncull` (from IVP's `LeakNetProfile` constants and `PawnNetUpdateHz`, 0.25 s at the defaults); calm-gait entry warp against `WalkSpeedCmPerSec`; swap before the end and crossing before the end | reject |
| I-ASN-1 | Assassination | `AssassinationRangeCm - r_c - StrikeContactGapCm <= StrikeWarpLimitCm` (r_c = SCL capsule radius) | reject |
| I-ASN-2 | Assassination | `StrikeContactSec + 0.4 <= CounterDurationSec <= AttackAttemptLockoutSec` | reject |
| I-ASN-3 | Assassination / Concealment | `StrikeContactSec + BushStrikeEmergeSec <= 1.2` (Concealment F3 f) | reject |
| I-ASN-4 | Assassination | `StrikeNetLeadSec <= StrikeContactSec / 2` | reject |
| I-ASN-5 | Assassination | `DeathFallSec >= 1.1` | reject |
| I-ASN-5w | Assassination | `DeathFallSec + BodyLingerSec + BodyRemovalGraceSec < CrowdReplacementMinDelaySec` | warn |
| I-ASN-6 | Assassination | `AttackAttemptLockoutSec >= StrikeContactSec + 1.1` | warn |
| I-ASN-7 | Assassination | `StrikeWarpLimitCm / StrikeContactSec <= 600` | warn |
| I-ASN-8 | Assassination | `StrikeBearingLimitDeg <= 60 + StrikeWarpYawLimitDeg` | reject |
| W-ASN-10 | Assassination (ADR-0018 §3) | `AttackAttemptLockoutSec > StrikeContactSec + BushStrikeEmergeSec` (strict; protects the uniqueness of the strike-to-outcome match) | warn |
| W-ASN-11 | Assassination / Penalty (ADR-0017) | `DeathFallSec + BodyLingerSec + BodyRemovalGraceSec < InformantRespawnDelaySec` (a killed body is removed before its informant respawns) | warn |
| W-ASN-9 | Assassination | `OpenSpotToleranceCm = PawnRefAgreementCm` | warn |
| I-NKP-1 | Penalty | `NpcKillStrikeLimit != 1` | reachable only in a preset with `bTestAllowsStrikeLimitOne` (hard minimum 2 otherwise, 2026-10-25); warn; reject in `Default` and `HostSelectable` presets |
| W-NKP-2 | Penalty | `InformantRespawnDelaySec + 2 * RespawnStarvationSec <= 40` | warn |
| W-NKP-3 | Penalty | `StrikeOutReleaseGraceSec >= AttackAttemptLockoutSec` | warn |
| W-NKP-4 | Penalty | `1.2 + DeathFallSec + NoPawnFadeSec <= InformantRespawnDelaySec` | warn |
| W-NKP-5 | Penalty | `ExileCardTimeoutSec >= StrikeContactSec + BushStrikeEmergeSec + ExileBeatSec + 0.3` (widened at the 2026-10-24 cross-review, S-W1: the card of a bush-strike exile is due up to `StrikeContactSec + BushStrikeEmergeSec` after the state arrives) | warn |
| I-OC1 | Objectives | `ObjectiveInteractSec >= CrowdAmbientTouchMaxSec + 0.5` | warn (owner AC-OC-11) |
| I-OC2 | Objectives | `RivalSignalDelayMaxSec >= RivalSignalDelayMinSec + 30`; and `W_eff = RivalSignalDelayMaxSec - RivalSignalDelayMinSec + RivalSignalReleaseQuantumSec` warns below 163 s and rejects below 100 s | reject / warn (TDC never repairs a value to satisfy an invariant, CR-6, so the owner's "clamp Max up" is applied as a rejection; reconciliation ruling) |
| I-OC3 | Objectives | `TendCueDelaySec + 0.25 >= CrowdDecoyEarlyReleaseMinSec` (Crowd key, registered at reconciliation) and `TendCueDelaySec <= ObjectiveInteractSec - 1.0` | reject |
| I-PCG-1 | Generator | `CityGenGridSideMinCount <= CityGenGridSideMaxCount` (an I-5 range pair) | reject |
| I-PCG-2 | Generator | `CityGenWitnessCellsTargetCount <= EmergenceWitnessCellsMaxCount` | reject |
| I-PCG-3 | Generator | `CityGenSkeletonFairnessMaxMult < ChainFairnessMaxMult` | reject |
| I-PCG-4 | Generator | `CityGenFacadeIdenticalRunMaxCount <= 6` | reject |
| I-TM-1 | Teammate Markers | `TeammateMarkerFeedStaleSec >= 3 / TeamPositionFeedHz` when `TeamPositionFeedHz >= 0.6` | reject |
| I-HUD-1, I-HUD-2 | HUD | `HudResultPlateMaxHoldBackSec` and `HudOwnRemovalHoldCapSec` each `>= StrikeContactSec + BushStrikeEmergeSec + 1/60` | reject |
| I-HUD-3 | HUD | `HudRemovalCardDelaySec = ExileBeatSec` | reject (raised from a warning at HUD's design review) |
| I-HUD-4 | HUD | `HudFeedMinDwellSec <= HudFeedLineLifeSec` (reject); `HudReplayWindowSec >= 0.5` (warn) | reject / warn |
| I-HUD-5 | HUD | `ExileBeatSec >= HudResultPlateInSec + 0.5` | reject |
| I-HUD-6 | HUD | `HudRemovalCardDelaySec + HudRemovalCardInSec <= DeathFallSec + NoPawnFadeSec` | reject |
| I-HUD-7 | HUD | `HudBannerMinPlateSec <= HudBannerMaxDelaySec` | reject |
| I-PNG-1 | Informant Pings | `PingBucketPerSec <= 0.5` when `PingLifetimeSec >= 6` (bounds the cadence of one sender's rings; user decision 2026-10-25) | warn |

  A preset that fails an owner invariant falls down the CR-6 ladder exactly like one that fails I-1 to I-5. Invariants may span owners and audiences (for example I-TM-1 reads a ServerOnly and a public key): the validator is server-side and sees every key.
- **Warnings (not rejections)**: any value outside its safe range (the value is kept); I-4 breach in a non-selectable preset.

**F-4 Snapshot size budget** (constraint, not a formula): public snapshot at most **2,048 bytes** per client per revision (raised from 256 at the 2026-10-24 reconciliation: the registered owner tables hold about 150 public `All`/`Client` keys, mostly presentation timings, about 600 bytes of values at 4 bytes each). Header fields: `SchemaVersion` (4), `RoundIndex` (4), `Revision` (4), `PresetId` (a catalog name of at most 16 bytes, or an index), `bValid` (1). Atomic delivery of a struct this size is an ADR check item (OQ-2). It is sent at travel and on each PIE revision; `ClientHash` adds 8 bytes to Session & Travel's acknowledgement once per epoch. Both are one-off messages: 2,048 bytes is 4 percent of a single second of the 50 KB/s per-client budget (51,200 bytes) and nothing recurs per tick.

## Edge Cases

*Acknowledgement, deadline and disconnect outcomes below follow Session & Travel's rules (CR-9); TDC states only what its own data does.*

- **If a client disconnects before Session accepts its acknowledgement**: its epoch-keyed tuning-ready entry is dropped. Session & Travel's rejoin rules apply (rule 21). A rejoin within the window receives the current snapshot and acknowledges against the same epoch; Session's absolute deadline is not reset by the rejoin.
- **If a client's `ClientHash` does not match the server's**: Session & Travel's mismatch path applies (explicit resend once, then `ParamsMismatch`, then Roster `Disconnect`). TDC never aborts a round on client reports, so any number of mismatching or colluding clients only gets themselves disconnected.
- **If the server self-check fails (CR-9)**: `TuningSnapshotFault` is logged, the pipeline enters `Failed`, Round returns to `Lobby`, and no client is disconnected.
- **If an acknowledgement carries a stale epoch (older `RoundIndex` or `Revision`) or is a duplicate**: Session & Travel's acknowledgement handling ignores and counts it; it never satisfies the current wait. A client sends its echo only after it holds the current epoch's snapshot, so no early echo needs queueing.
- **If the snapshot arrives before the PlayerController, or the reverse**: the client computes `ClientHash` when both exist; arrival order does not matter. After seamless travel the PlayerController already exists, so only the new snapshot (new `RoundIndex`) is awaited.
- **If the listen host is also a client**: its tuning-`Ready` is local and immediate through the accessor path. It never exercises replication, so a loopback test with a real remote client covers serialisation (AC-12b).
- **If anyone tries to join after the freeze**: Session & Travel admission refuses it (in-round admission is rejoin-only). TDC defines no post-freeze join, because a pawn appearing without a preceding death would identify a human (Pillar 1).
- **If a participant rejoins mid-round**: pawn continuity follows Session & Travel rule 22. The pawn is not hidden or re-spawned for tuning reasons, and the rejoining client issues no commands until its snapshot for the current epoch is valid. Because `NpcCount` is derivable from the shipped presets, the total pawn count must not change through any disconnect or rejoin, or pawns minus `NpcCount` would reveal the human count (change request to Round and Crowd).
- **If the stored preset name changes between rounds (non-Shipping only)**: it is re-resolved at the next `Preparing` (`ResolveRoundPreset`), with `Revision` 0, a new hash and new latches. A `stealth.tuning.preset` command issued mid-round changes nothing until that next `Preparing`.
- **If the travel URL `?Preset=` disagrees with the preset store**: the store wins and a warning is logged.
- **If the host leaves**: no host migration exists in the MVP. The session ends, the round aborts with no result, and no tuning state persists.
- **If `?Tuning=` is unknown, malformed (padding, path or separator characters), non-`HostSelectable`, or the build is Shipping**: `Default` is used and a warning is logged. If the option is absent, `Default` is used with no warning.
- **If `Default` itself is requested and fails to load or validate**: the repeated rung is skipped, schema defaults apply, `FallbackLevel` is 2 and `PresetId` is `SchemaDefaults`.
- **If any other requested preset fails to load or fails an invariant**: the ladder falls to `Default`, then to schema defaults, and records `FallbackLevel`. If all rungs fail, the pipeline is `Failed` and the round does not start.
- **If a preset contains an out-of-range, non-finite or undeclared-enum value**: F-1 clamps it or applies the default, and the warning enters the log header. The same preset in the catalog is a CI failure (AC-29).
- **If a value is inside its hard range but outside its safe range**: the value is kept, a warning is logged, and the preset is accepted (AC-26).
- **If a range pair is authored with Min greater than Max**: I-5 fails and the whole preset is rejected.
- **If a hot-reload edit arrives mid-round (PIE)**: the live-set rule of CR-12 applies. Edits to `Round`-latched keys are pending until `Reset`, in-flight actions keep their latched durations, `Revision` increments only if the live-set hash changes, no client leaves `Ready`, and the round is flagged `HotReloaded`. A failing edit changes nothing live.
- **If a designer drags a slider in PIE**: only committed value changes trigger re-validation, so `Revision` does not climb with each interactive tick.
- **If a hot-reload hook could be triggered in a packaged or shipping build**: it cannot, because the code is compiled out (`WITH_EDITOR`). This is verified by a static build-flag test and by the absence of the sentinel literal (AC-14c, AC-22).
- **If a client's local tuning asset, ini or cvar is modified**: there is no gameplay effect, because the client acts only on the replicated snapshot (AC-8), and no tell is created.
- **If client code requests a `ServerOnly` key**: the C++ client face cannot express it, so it does not compile. A reflective or Blueprint request returns *invalid* and logs (an `ensure` in development), and never returns a local default.
- **If two owner GDDs register the same key name, or one quantity under two names**: the doc lint (AC-3b) and the schema test fail. It is resolved by a change request, never by suffixing.
- **If a downstream system proposes an observable NPC-only or player-only variant of a duration or range**: CR-5 rejects it.
- **If `TeamSize` is less than 5 or fewer humans join than `2 * TeamSize`**: tuning is unchanged. Round owns the fill rule, and the F-3 indicators use the configured `TeamSize`.
- **If the host chooses a non-default preset (non-Shipping)**: it is accepted only when the entry is `HostSelectable`, and every client sees the same values (no tell). In Shipping the choice is impossible (CR-2).
- **If the listen host inspects `ServerOnly` data or its own files**: tuning data carries no identity information (every value is global and every preset ships in every client's paks), so this adds no Pillar-1 leak by itself. The host's broader server access (for example the pawn-to-participant binding) is a prototype limitation of the listen server, owned by the Visibility Policy and removed by the Alpha dedicated server. It is stated here, not hidden. A malicious host's modified `Default` is detectable only after `Ended`, by comparing the logged `TuningHash` with the published golden hash of the shipped `Default`; this is accepted for the prototype.
- **If seamless travel recreates the GameState**: the previous snapshot is discarded and is invalid from travel start, and the new snapshot carries the new `RoundIndex`. PlayerController and PlayerState persist, so no tuning state is stored on them. A stale acknowledgement from the old epoch is ignored.
- **If PIE runs several clients in one process**: they share one asset object, so "the client never reads its asset" cannot be proven there. AC-8 uses separate processes or a code-level check, and the world-scoped accessor (CR-10) keeps each PIE world's reads separate.
- **If a preset tries to set `BlindGuessWarnPct`**: it cannot, because the threshold is a schema constant and not a preset field.
- **If an owner invariant mixes `Session`-latched and round keys (for example I-CNB-1 with `SessionSoftTimeoutSec`)**: it is evaluated at every `Preparing` against the values the session opened with; a round preset that fails it falls down the CR-6 ladder, and the session keys are never changed mid-session to make it pass.
- **If an owner constraint needs a measured or layout-time value (the offline rows of the owner-invariant table)**: TDC does not evaluate it at preset load; its owner's CI or load-time check enforces it, and a failure there never edits a preset.
- **If an owner asks for an invariant to clamp or repair a value (for example Objectives I-OC2)**: TDC applies it as a rejection or warning instead. Only F-1 clamps, and only to a key's own hard range (CR-6).

## Dependencies

TDC is a Foundation system with **no upstream GDD dependencies**; it needs only engine facilities (data assets, reflection, property replication) and the coding-standard rule that gameplay values be data-driven. Round and Session call into TDC (`BeginResolve`, `Reset`, preset store, hash comparison), and TDC never calls them, so there is no dependency cycle. The per-client delivery check is **delegated** to Session & Travel's acknowledgement (CR-9), which is an interface delegation, not a TDC data dependency: TDC's rules and tests hold with Session stubbed. Interface names for GDDs not yet written are **provisional**.

### Upstream (TDC depends on)

| System | Data interface | Hard/Soft |
| ---- | ---- | ---- |
| None (Foundation) | -- | -- |

### Downstream (these depend on TDC)

Hard = the dependent cannot be finished or run without a TDC key or the accessor. All read through `GetServerTuning()` / `GetClientTuning()` and register keys per CR-15.

| Dependent system (index #) | Takes from TDC | Keys it owns (its Tuning Knobs table is authoritative) | Hard/Soft |
| ---- | ---- | ---- | ---- |
| Participant State & Roster (2) | `TeamSize`, injected into `BeginMatch` by Round or Session (the Roster reads no config) | none | Soft (indirect) |
| Round & Match Framework (6) | `BeginResolve()` at `Preparing`; `OnTuningReady`; `Failed`; log header fields; stored `PresetId` for travel; `PreviewPreset(PresetName)` dry run (provisional) | `TeamSize`, `TargetRoundMinutes` and 14 Round keys | Hard |
| Input & Interaction Framework (3) | interaction thresholds, hold times | own keys (registered, see Registered Owner Keys) | Hard |
| Character Pool & Appearance (4) | appearance-deck parameters at round start | `DeckCopiesFloorCount`, `DeckActiveAppearancesMinCount`, `DeckActiveAppearancesMaxCount` (I-3, I-5) | Hard |
| Level Socket Contract (8) | layout and validation limits | own keys (registered, see Registered Owner Keys) | Hard |
| Shared Character & Locomotion (9) | speed tiers, traversal limits, restrictions | own keys (registered, see Registered Owner Keys) | Hard |
| Crowd & NPC Behaviour (11) | population and density | `NpcCount` plus behaviour keys | Hard |
| Assassination & Defence Reversal (13) | range, defence duration, cooldown, strike and body timings | `AssassinationRangeCm`, `DefenceDurationSec`, `DefenceCooldownSec` and 13 Assassination keys | Hard |
| Camera System (10) | view, rig and eye-mask presentation values (client-only consumer) | 48 Camera keys (`Client`) | Hard |
| Concealment (Bushes) (12) | transition timings, spot tolerance, re-entry cooldown | 11 Concealment keys | Hard |
| NPC-Kill Penalty & Informants (14) | strike limit, informant respawn delay | `NpcKillStrikeLimit`, `InformantRespawnDelaySec`, `ExileBeatSec`, `StrikeOutReleaseGraceSec`, `ExileCardTimeoutSec` | Hard |
| Objectives Chain (15) | interaction time, stage structure, rival-signal timing | `ObjectiveInteractSec` and 10 Objectives keys | Hard |
| Procedural City Generator (16) | size limits, retry budget | own keys (registered, see Registered Owner Keys) | Hard |
| HUD (18) | public values for owner-only displays (strike limit, defence timing); its own presentation timings | 21 HUD keys (`Client`) | Hard |
| Teammate Markers (17) | marker presentation values (client-only consumer) | 7 Teammate Markers keys (`Client`) | Soft |
| Informant Pings & Team Communication (20) | ping lifetime, rate budget and range | 5 `InformantPings` keys | Hard |
| Telemetry & Analytics (26) | `TuningHash`, `PresetId` as dimensions | none | Soft |
| Anti-Cheat & Info-Leak Security (27) | audience flags as leak-audit input | none | Soft |
| Dedicated Server (28) | data assets cooked into the server target | none | Soft |
| Session & Travel (5) | `SchemaVersion` (as `TuningSchemaVersion`) for the join gate; server `ClientHash` per epoch for its single acknowledgement; `PresetId` from the preset store for the `?Preset=` travel option | 13 `Session`-latched keys (CR-11) | Soft |
| Information Visibility Policy (7) | audience vocabulary (`ServerOnly | All` subset) | 4 IVP keys (`ConcealCullDelayMs`, `TeamPositionFeedHz`, `TeamPositionFeedGridCm`, `bConcealServerCull`) | Soft (vocabulary and its own keys) |
| Settings & Accessibility (24) | boundary rule CR-14 | none | Soft |

**Bidirectionality**: the thirteen hard dependents (index rows 3, 4, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 18; Camera, Concealment and HUD added at the 2026-10-24 reconciliation) list TDC in the index's Depends On column and must list it in their own Dependencies sections and register keys via CR-15. Session & Travel and Participant State & Roster already name TDC in their own Dependencies (verified 2026-10-24). The soft consumers (Roster, Teammate Markers, Telemetry, Anti-Cheat, Dedicated Server, Session & Travel, Visibility Policy, Settings) consume TDC without being listed as its dependents in the index; change requests are in Cross-References.

## Tuning Knobs

TDC's own knobs are few; the canonical keys of other systems are tabulated under Detailed Design and are tuned in their owner GDDs.

| Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Unit | Effect / what breaks |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| `BlindGuessWarnPct` | float (schema constant, code) | TDC | 50 | -- (constant; changed only by a code and GDD change) | 40 to 70 recommended | ServerOnly | -- | percent | I-4 threshold on `100 * BlindGuessSuccess` for `Default` and `HostSelectable` presets. Too low: legitimate dense-crowd presets are rejected. Too high: presets where uniform blind guessing reliably pays pass CI. Not a preset field, so no preset can loosen its own gate. Provisional pending playtest (OQ-3). |
| Catalog `bTestAllowsStrikeLimitOne` flag | bool per preset | TDC | false everywhere | -- | -- | ServerOnly | Round | flag | **Added 2026-10-25 (user decision, Penalty OQ-NKP-11):** lowers `NpcKillStrikeLimit`'s hard minimum from 2 to 1 for that preset only; rejected on `Default` and on any `HostSelectable` preset; compiled out of Shipping. |
| Catalog `HostSelectable` flags | bool per preset | TDC | `Default` true; others false | -- | -- | ServerOnly | Round | flag | Which presets a host may choose **in non-Shipping builds**. Shipping always uses `Default` regardless of the flag (CR-2). |

`TuningAckTimeoutSec` is retired (CR-9); the acknowledgement wait is bounded by Session & Travel's `TravelLoadTimeoutSec`.

Interactions: `DefenceCooldownSec` and `DefenceDurationSec` are coupled by I-1; `NpcCount`, `TeamSize` and `NpcKillStrikeLimit` jointly drive `BlindGuessSuccess` (raising the strike limit or lowering `NpcCount` makes blind guessing safer, which weakens Pillar 1 reading; the I-4 table under Formulas gives the lowest passing `NpcCount` per strike limit, and the safe ranges are mutually consistent); `ObjectiveInteractSec` plus layout size set round length against the non-binding `TargetRoundMinutes`. Round length is not validated against the target (a playtest metric, not a rule); the Generator does not read it either (OQ-7). Cross-owner couplings registered at the 2026-10-24 reconciliation are the owner invariants under Formulas; the densest are the strike-timing chain (Assassination, Concealment, HUD, Teammate Markers: I-ASN-3, I-HUD-1/2), the crowd timing set (Crowd, Session, LSC: I-CNB-1 to I-CNB-4) and the respawn timing set (Penalty, LSC, Camera: W-NKP-2, W-NKP-4).

## Visual/Audio Requirements

None. TDC produces no visuals or audio. The only presentation-adjacent rule is that no tuning value may cause a visual or audio difference between a player-driven and an NPC-driven pawn (CR-5). art-director not consulted: Core/Foundation category, no player-facing content (optional section per skill rules).

## Game Feel

Not applicable as a direct requirement (Foundation category). Feel targets (responsiveness, weight, timing) live in each owner GDD; TDC's contribution to feel is iteration speed: a designer must be able to change a value in a PIE session and feel it within seconds (CR-12), and tuned `Action` values must never be retimed mid-action (CR-11).

## UI Requirements

No player-facing UI. Non-shipping tooling only: `stealth.tuning.dump` output and an optional development overlay showing the short `ClientHash`, preset id, `Revision` and `FallbackLevel`; it must be compiled out of shipping. Vertical Slice (provisional): the lobby displays the active preset name to all players (Lobby UI GDD owns it). No UX spec is required, so no UX flag.

## Cross-References

| This document references | Target | Element referenced | Nature |
| ---- | ---- | ---- | ---- |
| `OnPawnKilled`, strike rules | design/gdd/npc-kill-penalty-informants.md | `NpcKillStrikeLimit` meaning; I-NKP-1, W-NKP-2 to 5 | Value ownership |
| Assassination range, defence | design/gdd/assassination-defence-reversal.md | `AssassinationRangeCm`, `DefenceDurationSec`, `DefenceCooldownSec`; I-ASN-1 to 8 | Value ownership |
| Round phases, event log | design/gdd/round-match-framework.md | `Preparing` wait, log header, `TeamSize`, `TargetRoundMinutes` | Rule dependency |
| Objective interaction | design/gdd/objectives-chain.md | `ObjectiveInteractSec` (hard 2.0-6.0, OC-14); I-OC1 to 3 | Value ownership |
| Crowd population | design/gdd/crowd-npc-behaviour.md | `NpcCount`; I-CNB-1 to 5 | Value ownership |
| Appearance deck | design/gdd/character-pool-appearance.md | `DeckCopiesFloorCount`, `DeckActiveAppearancesMinCount`, `DeckActiveAppearancesMaxCount` for I-3 and I-5 | Rule dependency |
| Audience vocabulary | design/gdd/information-visibility-policy.md | `ServerOnly | All` | Vocabulary |
| Every owner table | every design/gdd/*.md with a Tuning Knobs section | registered keys and owner invariants | Registry (Registered Owner Keys; Formulas, owner invariants) |
| Join gate, travel, acknowledgement | design/gdd/session-travel.md (rules 7.2, 11.3-11.7, 21, 22) | `TuningSchemaVersion`, `?Preset=`, `FRoundReadiness`, single acknowledgement, `TravelLoadTimeoutSec`, rejoin pawn continuity | Interface (CR-4, CR-8, CR-9; change requests below) |
| Participant capacity | design/gdd/participant-state-roster.md | `TeamSize` injected into `BeginMatch` | Value consumer |
| Game concept | design/gdd/game-concept.md | Pillars 1 and 2; locked defaults | Pillar and value source |

**Change requests for other GDDs** (not applied; parallel mode; updated by the 2026-10-24 review):
1. **Every hard dependent**: add "Tuning Data & Config" to Dependencies; use the CR-15 Tuning Knobs columns including Latch; name crowd keys `Crowd*`, not `Npc*`; review crowd-timing keys as identity tells (CR-15.8).
2. **Session & Travel** (written; these conflict with it today):
   - (a) In `FRoundReadiness` and the rule 11.6 echo, replace the full `TuningHash` with the client-recomputed `ClientHash`. `TuningHash` is never replicated (CR-7).
   - (b) Define an explicit resend mechanism (client RPC or a nonce field), because an unchanged replicated property is never re-sent.
   - (c) Treat a `ClientHash` mismatch as `ParamsMismatch`.
   - (d) Take `PresetId` from TDC's preset store for the `?Preset=` travel option, as a server-side carrier only. TDC re-validates it at `Preparing`.
   - (e) Read `TuningSchemaVersion` as TDC's `SchemaVersion`.
   - (f) Keep in-round admission rejoin-only. Keep `TravelLoadTimeoutSec` absolute from server travel start, and do not reset it on rejoin.
   - (g) Key acknowledgement bookkeeping by `ParticipantId` and reset it each epoch.
3. **Round & Match Framework**:
   - Call `BeginResolve()` on entering `Preparing`.
   - Wait for `OnTuningReady` plus every participant's `OnParticipantTravelComplete`; there is no separate TDC ready gate.
   - Release first-entry pawns only after tuning-`Ready`, at one common moment.
   - Handle `Failed` (`TuningLoadFailed`, `TuningSnapshotFault`) by returning to `Lobby`.
   - Write the log header fields.
   - Never broadcast pre-round disconnects to clients.
   - With Crowd, keep the total pawn count constant through disconnect and rejoin, so pawns minus the derivable `NpcCount` never reveals the human count.
4. **Character Pool & Appearance**: *(answered 2026-10-24: I-3 now uses `DeckCopiesFloorCount` x `DeckActiveAppearancesMinCount`; `AppearancePalette` is CPA pool content, not a key.)*
5. **Assassination & Defence Reversal**: *(answered 2026-10-24: `Action` latch confirmed; the reach check is I-ASN-1 and the hard maximum is now 250.)*
6. **Information Visibility Policy**: accept the `ServerOnly | All` audience subset for tuning.
7. **Objectives Chain**: *(answered 2026-10-24: hard range raised to 2.0-6.0, OC-14.)*
8. **Crowd & NPC Behaviour**: split `NpcIdleTimeRange` into `CrowdIdleMinSec`/`CrowdIdleMaxSec` (I-5); use `Crowd*` names; co-own the constant-pawn-count rule with Round.
9. **NPC-Kill Penalty & Informants, Shared Character & Locomotion, Input & Interaction, Informant Pings (VS)**: register the migrated prototype keys assigned to them below. *(Penalty, SCL and IIF registered by 2026-10-24; Informant Pings is VS.)*
10. **Rename requests (doc lint, 2026-10-24 reconciliation) — APPLIED 2026-10-25 (user decision 2026-10-25)**: IVP/Concealment `ConcealServerCullEnabled` -> `bConcealServerCull`; Camera `EyeMaskHideFails` -> `EyeMaskHideFailCount`, `StickDeadZone`/`StickOuterZone` -> `StickDeadZoneFrac`/`StickOuterZoneFrac`; LSC `LayoutSliceWorkUnitsMax` -> `LayoutSliceWorkUnitsLimit`, `RespawnPoolMinExpected` -> `RespawnPoolMinExpectedCount` (renamed in every GDD, ADR, the entity registry, the architecture registry and the control manifest). Still open: retire one of `ReachDetourMult`/`DetourMaxMult` if they can never differ (CR-15.3).
11. **Crowd & NPC Behaviour**: *(done 2026-10-24: `CrowdDecoyEarlyReleaseMinSec`/`MaxSec` registered; I-OC3 reads the key.)*
12. **Objectives Chain**: I-OC2 is enforced as reject/warn, not as a clamp of `RivalSignalDelayMaxSec` (CR-6); restate OC Tuning Knobs accordingly.

**Prototype mapping** (exhaustive). `Source/StealthGame/Public/StealthSettings.h` is a `UDeveloperSettings` with `Config = Game`, reading `Config/DefaultGame.ini`; a packaged client can override it locally. It is superseded, not extended. Proposed key names are provisional; each owner GDD decides the final name and range.

| Prototype field | Becomes | Owner (provisional) |
| ---- | ---- | ---- |
| `MaxPlayersPerTeam` | `TeamSize` | Round |
| `MinPlayersToStart` | `PlayersToStartCount` | Round / Session |
| `bAutoStartRound` | `bAutoStartRound` (non-Shipping test convenience or Round rule; owner decides) | Round |
| `RoundStartDelay`, `RoundRestartDelay` | `RoundStartDelaySec`, `RoundRestartDelaySec` | Round |
| `InformantRespawnDelay` | `InformantRespawnDelaySec` (identity-tell review, CR-15.8) | NPC-Kill Penalty & Informants |
| `DefenceDuration`, `DefenceCooldown` | `DefenceDurationSec`, `DefenceCooldownSec` | Assassination |
| `AssassinationRange` | `AssassinationRangeCm` | Assassination |
| `AssassinationServerTolerance` | **dropped** (no lag compensation; Assassination resolves on server positions) | Assassination |
| `AssassinationConeHalfAngle` | **dropped** (targeting is the camera ray plus `PawnRef`; `StrikeWarpYawLimitDeg` is a presentation cap) | Assassination |
| `BlindStrikeRadius` | `OpenSpotToleranceCm` (open ground) and `BushSpotToleranceCm` (bush) | Assassination, Concealment |
| `AttackerRecoveryTime` | `AttackAttemptLockoutSec` | Assassination |
| `BodyLifetime` | `BodyLingerSec` plus `DeathFallSec` (identity-tell review passed in Assassination) | Assassination |
| `NpcKillLimit` | `NpcKillStrikeLimit` | NPC-Kill Penalty & Informants |
| `RunSpeedThreshold` | `RunSpeedThresholdCmPerSec` | Shared Character & Locomotion |
| `InformantMaxSpeed` | **retired** (informants use the Walk gait and `WalkSpeedCmPerSec`; a separate top speed would be a tell) | Shared Character & Locomotion |
| `ObjectiveInteractTime` | `ObjectiveInteractSec` | Objectives Chain |
| `ObjectiveInteractRange` | `InteractRangeCm` (one quantity, one name) | Input & Interaction |
| `NpcCount` | `NpcCount` | Crowd |
| `CrowdRadius` | `CrowdRadiusCm` (may be superseded by Level Socket `CrowdRoute`) | Crowd |
| `NpcIdleTimeRange` (`FVector2D`) | `CrowdIdleMinSec`, `CrowdIdleMaxSec` + I-5 | Crowd |
| `NpcReplacementDelay` | `CrowdReplacementMinDelaySec`, `CrowdReplacementMaxDelaySec` (identity-tell review passed in Crowd) | Crowd |
| `PingLifetime`, `PingCooldown` | `PingLifetimeSec`, `PingCooldownSec` | Informant Pings (Vertical Slice) |
| `CharacterClass`, `AppearanceMaterial`, `AppearanceColorParameters`, `TeamNames` | not tuning: content references and localised text stay in a settings/content object | -- |
| `AppearancePalette` | not a scalar key: pool content (swatches in the static material instances), owner confirmed | Character Pool & Appearance |

Some prototype clamps are looser than the hard ranges here: cooldown minimum 1 s versus 30, range minimum 50 versus 100, interaction minimum 0.5 versus 2.0, `NpcCount` maximum 200 versus 100. Fast test presets therefore cannot go below the hard minima without a code change. This friction is accepted on purpose (CR-3) and revisited if playtesting needs it (OQ-10). Migration order: add the asset and schema with current values; add the accessor; convert call sites; remove the migrated properties and the ini section. The removal is a gate before any external playtest (CR-13).

**Change requests received (reconciliation 2026-10-24)**

| From | Request | Disposition |
| ---- | ---- | ---- |
| Assassination CR-2, CR-18 | register 16 keys incl. `StrikeBearingLimitDeg` | APPLIED (Registered Owner Keys) |
| Assassination CR-2, CR-18 | invariants I-ASN-1 (rewritten with r_c) to I-ASN-8, warnings I-ASN-5w/6/7 and the spot-tolerance parity warning | APPLIED (owner invariants) |
| Assassination CR-2 | lower `AssassinationRangeCm` hard maximum to 250 | APPLIED (Canonical Tunables) |
| Assassination CR-2 | annotate F-3: reversals ignored, about 7-8 % optimistic | APPLIED (F-3) |
| Assassination CR-2 | retire prototype keys as mapped | APPLIED (prototype mapping) |
| Assassination CR-18 | `StrikeWarpLimitCm` hard 150-240; `StrikeNetLeadSec` safe 0.10-0.15 | ALREADY DONE (owner table is authoritative) |
| Camera CR-8 | add Camera as a dependent; register 5 new keys; I-CS-4, I-CS-5 | APPLIED (Downstream table; registry; I-CS-1 to 5) |
| Camera, HUD, Teammate Markers, SCL | audience `Client` | APPLIED as a ruling: `Client` = `All` with a client-only consumer (CR-8) |
| Character Pool CR-5 (a)-(e), CR-15 | I-3 rewritten on deck keys and scoped like I-4; `NpcCount` hard minimum rests on I-2; AC-17 updated; Cross-References and Dependencies rows; `AppearancePalette` owner confirmed; register 3 deck keys | APPLIED |
| Concealment CR-16 | register `BushExitDepenetrationCapCm` and the Concealment keys; F3 (a)-(f) invariants; hard minimum 0.30 s for the crossing keys | APPLIED (I-CON-a to e, I-ASN-3; minimums already in the owner table) |
| Crowd CR-12, CR-19 | register Crowd keys and renames; cross-field constraints | APPLIED: I-CNB-1 to 5 as preset invariants; the measured-rate and layout-time constraints as offline checks (O-CNB-6, O-CNB-7) |
| HUD CR-HUD-4 (incl. its design-review additions) | dependents row HUD becomes Hard; register 21 keys; I-HUD-1 to 7 (I-HUD-3 now rejects) | APPLIED; rename request for `HudReadHoldScale` (suffix) — done by HUD (`HudReadHoldMult`) |
| IIF CR-10 | migration `ObjectiveInteractRange` -> `InteractRangeCm`; transport bucket invariants; retired and replaced keys noted | APPLIED |
| LSC CR-4 | register keys, new keys and retirements; D6 `NpcCount` bound; budget, witness range and detour invariants | APPLIED (I-LSC-1 to 4; `NpcCount` safe minimum 30 -> 35 so safe ranges stay mutually consistent) |
| LSC CR-4 | start cells >= `TeamSize + 1` | APPLIED as an offline layout predicate (no key holds the count) |
| Penalty CR-11 | `NpcKillStrikeLimit = 1` warning that rejects `Default`/`HostSelectable`; four cross-field warnings; register 4 keys | APPLIED (I-NKP-1, W-NKP-2 to 5) |
| Penalty CR-11 (pending part) | raise the `NpcKillStrikeLimit` hard minimum to 2, `L = 1` behind a dev flag | APPLIED 2026-10-25 (user decision; canonical row, F-1, I-NKP-1, catalog flag `bTestAllowsStrikeLimitOne`; OQ-12 closed) |
| Objectives CR-2 | `ObjectiveInteractSec` hard 2.0-6.0, safe 2.5-4.5; I-OC1; register keys | APPLIED (OQ-9 closed; I-OC1 as a warning per the owner's AC-OC-11) |
| Objectives Tuning Knobs | I-OC2 "clamp Max up" | DECLINED as a clamp (CR-6: invariants never repair); applied as reject/warn; change request 12 back to Objectives |
| Generator CR-3 | register `CityGen*` keys (ServerOnly, Round); four invariants; OQ-7 answer | APPLIED (I-PCG-1 to 4; OQ-7 closed) |
| SCL CR-2 | retire `InformantMaxSpeedCmPerSec`; register keys incl. the review's new keys; I-SCL-1 to 12 (now to 14) | APPLIED (three are offline checks) |
| Teammate Markers CR-5 | invariant on `TeammateMarkerFeedStaleSec`; soft-consumer list; register `Client` keys | APPLIED (I-TM-1) |
| User decision 2026-10-25 (scope; Informant Pings to MVP) | register the five `InformantPings` keys and I-PNG-1 | APPLIED (Registered Owner Keys row, Owner invariants row, Downstream row) |
| User decision 2026-10-25 (scope; Crowd OQ3) | `CrowdFullPacePct` default 100 → 55 now that the keyboard walk-slower modifier exists (IIF R15a) | ALREADY DONE in the owner's table (Crowd Tuning Knobs); no TDC rule changes (the key count is unchanged) |
| Round CR-4, CR-14 | register Round keys; `DisconnectGraceSec` hard 3-120; `G_eff` clamp warning; invariants on `MinPlayersPerTeamCount` and `EndedAutoResetSec`; `NpcCount >= max(2 x TeamSize, 24 - P)` | APPLIED (I-RMF-1/2, W-RMF-3/4); the `NpcCount` rule is I-2 plus the rewritten I-3 |
| Round CR-4 | F6 computed watchdog replaces a watchdog invariant | ALREADY DONE (TDC had no watchdog invariant; recorded as runtime, not a preset rule) |
| Round CR-4 | dry-run `PreviewPreset(PresetName)` for R5 check 5 | APPLIED (Downstream row, provisional interface; runs CR-6 without freezing or publishing) |
| Round CR-4, CR-14 | add `SeqOrder` and `TieRank` sub-seed domains | REDIRECTED: sub-seed domains are Session & Travel's F5 (Round's own text says so), not TDC |
| Session CR-3 | latch class `Session`; register 12 keys; invariants Hard >= Soft + 2, heartbeat set, `ShutdownFlushSec >= 0.3`; F4 adequacy; note `PoolHash` and server-requested acknowledgement in CR-9 | APPLIED (CR-11, CR-9, I-ST-1 to 3); F4 adequacy offline (measured inputs) |
| Roster OQ15 | `TeamSize` owner Round; register `DisconnectGraceSec` 10-120; allow a `TeamSize` 1 test preset | ALREADY DONE / SUPERSEDED (Round owns `DisconnectGraceSec` with hard 3-120; `Solo` preset exists, CR-2) |
| Doc lint (this pass) | suffix list and `Min`/`Max` usage vs registered names | APPLIED (CR-15.2 extended; five rename requests, change request 10) |
| Registered public keys | snapshot size | APPLIED as a ruling: F-4 budget 256 -> 2,048 bytes (AC-19) |
| Cross-review 2026-10-24 S-W1, S-B4, C-W2, C-W16 | Widen W-NKP-5; register Round's `RevealPawnWaitMaxSec`; note the `ExileBeatSec` bound; correct stale key counts | APPLIED (cross-review fixes 2026-10-24): W-NKP-5 row; Round row 17 keys with the new `All` key; Penalty row note; Session 13, SCL 51, Crowd 50, Generator 32 (preamble form accepted) |
| ADR-0004 sync 2026-10-24 | AC test root follows ADR-0007 (`StealthFoundation` runtime-core, `StealthGameEditor` metadata tests); client struct named `FStealthClientTuning` (not `FStealthPublicTuning`) | APPLIED (AC preamble); struct name: no occurrence in this GDD (ADR-0007's text is the ADR owner's) |
| ADR-0005 sync 2026-10-24 | CR-9 "after seamless travel the PlayerController already exists" holds only as "a new controller exists before the acknowledgement request"; PlayerController/PlayerState do not persist | APPLIED (CR-9, two sentences) |
| ADR-0010 sync 2026-10-24 | Invariants `MaxClientForcedUpdateDuration ≥ InputStarvationSec + max(WalkStopSec, RunStopSec) + 0.25` and `MaxMoveDeltaTime ≤ MAXCLIENTUPDATEINTERVAL = InputStarvationSec`, boot-tested against pinned engine config | APPLIED (I-SCL-15, offline boot check; no new key; the config pin stays an active.md item) |
| ADR-0017 sync 2026-10-24 | Cross-field warning `DeathFallSec + BodyLingerSec + BodyRemovalGraceSec < InformantRespawnDelaySec` | APPLIED (W-ASN-11, warn) |
| ADR-0018 sync 2026-10-24 | Cross-field warning `AttackAttemptLockoutSec > StrikeContactSec + BushStrikeEmergeSec` (strict) | APPLIED (W-ASN-10, warn) |
| ADR-0016 sync 2026-10-24 | Register a `UCrowdMotionProfileData` soft-ref key and the StateTree asset keys in the Crowd ServerOnly sub-struct | APPLIED 2026-10-25 (user decision): `CrowdMotionProfileAsset`, `CrowdFullStateTreeAsset`, `CrowdRestrictedStateTreeAsset` registered (Crowd row above; Crowd Tuning Knobs) |
| ADR-0014 sync 2026-10-24 | New LSC key `LayoutExactRefineMaxCount` | APPLIED 2026-10-25 (user decision; LSC row above, LSC Tuning Knobs) |
| Architecture review 2026-10-25 (SCL I-SCL-10 restatement) | I-SCL-10 is a listen-host frame-time check, not an engine tick-rate config check | APPLIED (invariant table row wording; no key change) |

## Acceptance Criteria

Tests are named `StealthGame.Tuning.<Scenario>`. Runtime-core tests live in `Source/StealthFoundation/Private/Tests/`, metadata and lint tests in `Source/StealthGameEditor/Private/Tests/`, and multi-process tests in `Source/StealthTestHarness/Private/` (module layout of ADR-0007/0008; test names unchanged).

**Test seams** (required by these ACs):
- an injectable asset loader that can return a missing or corrupted preset;
- an injectable clock;
- a log-capture hook asserting warnings by key;
- a test hook that holds back snapshot replication or forces arrival order;
- a Round/Session stub for criteria whose owner GDD is unwritten.

**Test environments**:
- "Editor" means an editor automation test.
- "Dev build" means a packaged Development build. Shipping builds cannot run automation, so Shipping-only properties are checked by build-flag tests plus a binary scan for a sentinel literal.
- "Multi-process" means separate client processes driven by a multi-process harness (Gauntlet or a custom launcher, chosen by ADR, OQ-11). Single-process PIE shares one asset object and does not support seamless travel.

**Core rules**
- **AC-1 (CR-2, CR-4)**: GIVEN a Dev build, WHEN the server starts with no `?Tuning=` option, THEN the active preset is `Default`, `FallbackLevel` is 0, `PresetId` is `Default` and no warning is logged. (`StealthGame.Tuning.DefaultPreset`)
- **AC-2a (CR-4)**: GIVEN a Dev build with test catalog entry `Playtest` not `HostSelectable`, WHEN the host opens with `?Tuning=Playtest`, THEN `Default` is active and one warning names the option. (`StealthGame.Tuning.PresetNotSelectable`)
- **AC-2b (CR-4)**: GIVEN `Playtest` is `HostSelectable`, WHEN the host opens with `?Tuning=playtest`, THEN `Playtest` is active. (`StealthGame.Tuning.PresetCaseInsensitive`)
- **AC-2c (CR-4)**: GIVEN `Playtest` is `HostSelectable`, WHEN the host opens with each of `" Playtest"`, `"Playtest/../x"` and `"Playtest;"`, THEN `Default` is active plus one warning each. (`StealthGame.Tuning.PresetMalformed`)
- **AC-2d (CR-4)**: GIVEN the server runs `Default`, WHEN a client logs in with `?Tuning=Playtest` in its own login options, THEN the active preset is unchanged. (`StealthGame.Tuning.ClientOptionIgnored`)
- **AC-2e (CR-4)**: GIVEN the store holds `Playtest`, WHEN two `ServerTravel` round changes occur (multi-process), THEN both rounds resolve `Playtest`. AND WHEN the travel URL is forced to `?Preset=Default`, THEN the store still wins and a warning is logged. (`StealthGame.Tuning.PresetSurvivesTravel`)
- **AC-3a (CR-3)**: GIVEN the compiled schema, WHEN the schema test runs in the editor, THEN all of the following hold:
  - every row has a default, hard min/max, audience and latch class, and each default lies inside its hard range;
  - key names are unique, and every key ends with a CR-15 suffix (booleans start with `b`);
  - the metadata mirrors equal the schema;
  - the `Default` preset and the C++ initialisers both equal the Default column of Canonical Tunables;
  - reflection finds only `bool`, `int32`, `float` and enum fields, no `Config` property and no `Config` or per-object-config class flag.
  (`StealthGame.Tuning.SchemaComplete`)
- **AC-3b (CR-15)**: GIVEN `design/gdd/*.md`, WHEN the doc lint runs in CI, THEN all of the following hold:
  - every Tuning Knobs table has the CR-15 columns;
  - every key appears as owned in exactly one GDD;
  - every owned key matches a schema row and a `design/registry/entities.yaml` constant.
  AND a fixture GDD with a deliberately duplicated key fails the lint. (`StealthGame.Tuning.KnobRegistryLint`, CI script)
- **AC-4 (CR-6, F-1)**: GIVEN a preset with `ObjectiveInteractSec` NaN, `DefenceDurationSec` 7.5, `AssassinationRangeCm` 50, `NpcKillStrikeLimit` 0 and `NpcCount` 0, WHEN validated, THEN the effective values are 3.0, 5.0, 100, 2 and 6, and the captured log holds one warning per key naming its authored and effective value. AND +Infinity and -Infinity each yield 3.0, AND `NpcKillStrikeLimit` 9 yields 5, AND values exactly on a bound produce no warning. (`StealthGame.Tuning.ValidateClampAndNan`)
- **AC-5 (CR-7, F-2)**: GIVEN the committed fixture (fixed `SchemaVersion` and one key of each type tag), WHEN hashed on every CI platform, THEN the result equals the committed golden value. AND all of the following hold:
  - permuting the input key order gives the same hash;
  - a one-bit change to any field changes it;
  - the hash function maps `-0.0` and `+0.0` equally;
  - changing a server-only key changes `TuningHash` but not `ClientHash`, and changing a public key changes both;
  - `Default` hashes equally on all CI platforms.
  (`StealthGame.Tuning.HashStableAndSensitive`)
- **AC-6a (CR-6)**: GIVEN a requested preset violating I-1 (`DefenceDurationSec` 5, `DefenceCooldownSec` 35), WHEN resolved, THEN the whole preset is rejected, `Default` is active and `FallbackLevel` is 1. (`StealthGame.Tuning.FallbackToDefault`)
- **AC-6b (CR-6)**: GIVEN the injectable loader returns a corrupted `Default` and the requested preset fails, THEN schema defaults apply with `FallbackLevel` 2. AND GIVEN `Default` itself is requested and corrupted, THEN `FallbackLevel` is 2 and `PresetId` is `SchemaDefaults`. (`StealthGame.Tuning.FallbackToSchema`)
- **AC-6c (CR-6)**: GIVEN every rung fails, WHEN resolved against a Round stub, THEN the pipeline is `Failed`, the stub receives the `Failed` result and `TuningLoadFailed` is logged. (`StealthGame.Tuning.FallbackFailed`)
- **AC-7 (CR-8)**: GIVEN a 2-client round using a non-`Default` test preset, WHEN `Preparing` completes, THEN all of the following hold:
  - each client's snapshot contains exactly the public keys plus `SchemaVersion`, `PresetId`, `RoundIndex`, `Revision` and `bValid`;
  - reflection shows that struct membership equals the schema audience and that no server-only key is in the replicated struct;
  - the snapshot is on the GameState and on no PlayerState;
  - no field named `TuningHash` is replicated;
  - each client's recomputed `ClientHash` equals the server's.
  (`StealthGame.Tuning.SnapshotPublicOnly`)
- **AC-8 (CR-4, CR-10, CR-13)**: GIVEN a multi-process client launched with a locally modified tuning asset, a modified ini and a `set` console override (all with values outside the hard ranges), WHEN it plays a round against an unmodified Dev-build server running a non-`Default` preset, THEN every client-side `GetClientTuning()` read equals the server snapshot. A companion static check (`StealthGame.Tuning.NoClientAssetReference`) asserts that client-side code holds no reference to the tuning asset class. (`StealthGame.Tuning.LocalTamperIgnored`)
- **AC-9 (CR-8)**: GIVEN snapshot replication to client A is held back by the test hook, WHEN client B samples its replicated actors every tick from A's connect until the server marks A tuning-`Ready`, THEN B sees zero pawns bound to A, and A's `GetClientTuning()` returns *invalid* and no default value throughout. A's pawn first appears on B no earlier than the common release tick of the round. AND a rejected acknowledgement on Session's deadline or mismatch path (stub) never produces a pawn for A. (`StealthGame.Tuning.NoPawnBeforeReady`)
- **AC-10 (CR-9)**: GIVEN `Preparing` with a Session stub, WHEN each client holds the snapshot, THEN each client hands Session exactly one `ClientHash` per epoch, equal to the server's. The value is recomputed from the received fields: the test corrupts one received field through the hook and asserts the handed hash differs. AND the snapshot-first and controller-first orders (forced by the hook) both produce exactly one hand-off. (`StealthGame.Tuning.ClientHashHandoff`)
- **AC-11 (CR-9)**: GIVEN a `ClientHash` for a stale epoch (old `RoundIndex` or `Revision`), WHEN the server compares it, THEN it is reported as not matching the current epoch and never marks the participant tuning-`Ready`. AND no replicated property on any PlayerState or GameState reveals any participant's tuning-ready state (reflection). (`StealthGame.Tuning.StaleEpochIgnored`)
- **AC-12a (CR-9, CR-10)**: GIVEN a listen-server host, WHEN `Preparing` runs, THEN the host is tuning-`Ready` through the accessor path without any RPC and reads public values identical to the snapshot. (`StealthGame.Tuning.HostPath`)
- **AC-12b (CR-8)**: GIVEN a listen host and one remote multi-process client, WHEN the snapshot replicates, THEN the remote client's recomputed `ClientHash` equals the host's. This loopback test exercises serialisation, which the host path never does. (`StealthGame.Tuning.RemoteLoopbackHash`)
- **AC-12c (CR-9)**: GIVEN the server self-check is forced to see a differing serialised struct (test hook), WHEN freezing, THEN `TuningSnapshotFault` is logged, the pipeline is `Failed`, the Round stub receives `Failed`, and no client is disconnected. (`StealthGame.Tuning.SelfCheckFault`)
- **AC-13 (CR-8, Edge rejoin)**: GIVEN a multi-process client that disconnects during `Active` and rejoins within Session's window, WHEN it reconnects, THEN all of the following hold:
  - it receives the current snapshot with the round's original `RoundIndex` and `ClientHash`;
  - no command from it is accepted before its snapshot is valid;
  - its pawn is neither despawned nor respawned for tuning reasons;
  - the other clients' `Active` tick count and state are unchanged against a control run.
  (`StealthGame.Tuning.RejoinSnapshot`)
- **AC-14a (CR-11, CR-12)**: GIVEN PIE with 3 worlds and an injected clock, with a defence started at t=0 and `DefenceDurationSec` 4.0, WHEN a designer commits 3.0 at t=1, THEN all of the following hold:
  - the in-flight defence ends at t=4.0 and the next lasts 3.0;
  - `Revision` rises by exactly 1;
  - clients recompute `ClientHash` under the new epoch and stay `Ready`;
  - the log is flagged `HotReloaded`;
  - a committed `NpcCount` edit leaves the live value and the snapshot unchanged until `Reset`, and `Revision` does not rise for it.
  (`StealthGame.Tuning.HotReloadPie`)
- **AC-14b (CR-12)**: GIVEN PIE, WHEN a committed edit violates I-1, THEN it is rejected, and the previous frozen set and `Revision` stay live. AND 20 interactive (non-committed) slider ticks raise `Revision` by 0. (`StealthGame.Tuning.HotReloadRejected`)
- **AC-14c (CR-12)**: GIVEN the build configuration, WHEN a static build-flag test runs in a Dev build, THEN the hot-reload code is compiled only under `WITH_EDITOR`, and no asset-change subscription exists when `WITH_EDITOR` is 0. AND the Shipping binary scan of AC-22 finds no hot-reload sentinel. (`StealthGame.Tuning.HotReloadAbsentPackaged`)
- **AC-15 (CR-5)**: GIVEN the schema, WHEN reflected, THEN no key name matches `^(Npc|Player|Bot|Human|Team|Viewer)`, except the allowlist `NpcCount`, `NpcKillStrikeLimit`, `TeamSize`. Owner-GDD review for observable variants remains a milestone review step. (`StealthGame.Tuning.NoVariantKeys`)
- **AC-16 (CR-7)**: GIVEN a round in `Active`, WHEN the replicated state of the GameState and every PlayerState is reflected, THEN no `TuningHash` value is present. AND WHEN the round reaches `Ended`, THEN the Round Event Log header (stub until the Round GDD) contains `TuningHash`. (`StealthGame.Tuning.FullHashNotReplicated`; provisional on Round)

**Formulas and invariants**
- **AC-17 (F-3, I-1, I-2, I-3, I-5)**: GIVEN a test schema fixture holding only I-1 to I-5 (owner invariants are covered by AC-30) and non-`HostSelectable` test presets (except where stated) with `NpcKillStrikeLimit` 1 and `bTestAllowsStrikeLimitOne` (so I-4 cannot interfere), WHEN validated, THEN each of the following holds:
  - `DefenceDurationSec` 5 with `DefenceCooldownSec` 40 passes, and with 39.9 is rejected;
  - `TeamSize` 5 with `NpcCount` 10 passes with an I-3 warning (20 < 24), and with 9 is rejected (I-2);
  - `TeamSize` 1 with `NpcCount` 10 passes with an I-3 warning (12 < 24);
  - `TeamSize` 4 with `NpcCount` 8 passes with an I-3 warning (I-2 tie, 16 < 24), and with 7 is rejected (I-2);
  - a `HostSelectable` preset with `TeamSize` 1 and `NpcCount` 21 is rejected (I-3, 23 < 24), and with 22 passes;
  - a range pair with Min > Max is rejected (I-5);
  - each rejection rejects the whole preset at `FallbackLevel` 1;
  - authored `DefenceDurationSec` 7.5 clamps to 5 and passes with cooldown 40.
  (`StealthGame.Tuning.InvariantBoundaries`)
- **AC-18 (F-3, I-4)**: GIVEN defaults, WHEN the indicators are computed, THEN `CrowdRatio` is 5.0, `DutyCycle` is within 0.0001 of 0.0667, `BlindGuessSuccess` is within 0.001 of 0.249 and `ExpectedBlindKills` is within 0.001 of 0.300. AND GIVEN `NpcCount` 15 and `NpcKillStrikeLimit` 3, THEN `BlindGuessSuccess` is within 0.001 of 0.578, a non-selectable preset is accepted with a warning, and a `HostSelectable` preset or `Default` is rejected. AND GIVEN `TeamSize` 5 and `NpcKillStrikeLimit` 3 in a `HostSelectable` preset validated against the AC-17 test schema fixture (I-1 to I-5 only; with the full schema I-LSC-1 rejects below 35), THEN `NpcCount` 20 passes and 19 is rejected. AND no preset field named `BlindGuessWarnPct` exists (reflection). (`StealthGame.Tuning.DerivedIndicators`)
- **AC-19 (F-4)**: GIVEN the public snapshot struct with a maximum-length `PresetId`, WHEN serialised through the replication path in an automation test, THEN it is at most 16,384 bits (2,048 bytes) with every registered public key present. A net-profile capture of a 10-client join is advisory evidence only. (`StealthGame.Tuning.SnapshotSizeBudget`)
- **AC-30 (owner invariants, reconciliation 2026-10-24)**: GIVEN the full schema and the `Default` preset, WHEN validated, THEN every owner invariant in the Formulas table passes and no warning is raised. AND GIVEN, for each reject-class row, a fixture preset that breaks only that row (for example `StrikeNetLeadSec` 0.25 with `StrikeContactSec` 0.40 for I-ASN-4; `SessionHardTimeoutSec` 4 with `SessionSoftTimeoutSec` 3 for I-ST-1), THEN the preset is rejected and the report names exactly that invariant ID; for each warn-class row, the preset is accepted with exactly one warning naming the ID; for I-NKP-1, `NpcKillStrikeLimit` 1 warns in a non-selectable preset with `bTestAllowsStrikeLimitOne`, is clamped to 2 with a warning in one without the flag, and is rejected in a `HostSelectable` one (the flag itself is rejected there). AND no offline row is evaluated at preset load. (`StealthGame.Tuning.OwnerInvariants`)

**Validation, tooling and cross-system**
- **AC-20 (Round, CR-7)**: GIVEN a completed round against the Round stub, WHEN the server-only Round Event Log header is read, THEN it contains `TuningHash`, `PresetId`, `SchemaVersion`, `FallbackLevel`, `Revision`, the validation warnings and the `HotReloaded` flag. (`StealthGame.Tuning.RoundLogHeader`; provisional on Round; client readability before `Ended` is covered by AC-16)
- **AC-21 (Pillar 1, CR-5, CR-10)**: GIVEN the accessor API, WHEN reflected or compiled, THEN `GetServerTuning()` and `GetClientTuning()` take no pawn, controller, team or viewer parameter, so no per-participant value can be requested. (`StealthGame.Tuning.PlayerNpcParity`)
- **AC-22 (CR-2, CR-13)**: GIVEN the packaged Shipping binary, WHEN it is scanned in CI for the unique sentinel literals of the hot-reload, preset-option and console-command code (as ASCII and UTF-16LE), THEN none is present. AND a Shipping build launched with `?Tuning=Playtest`, where `Playtest` is `HostSelectable`, logs `PresetId` `Default` in its server log. (`StealthGame.Tuning.NoShippingBackDoor`, CI script)
- **AC-23 (Edge, seamless travel)**: GIVEN a multi-process `ServerTravel` between rounds, WHEN the new GameState replicates, THEN all of the following hold:
  - the old snapshot reads as *invalid* from travel start;
  - the new snapshot carries the new `RoundIndex`;
  - a `ClientHash` for the old epoch never marks a participant `Ready`;
  - server tuning-ready bookkeeping is empty at the start of the new epoch.
  (`StealthGame.Tuning.TravelEpoch`)
- **AC-24 (CR-14)**: GIVEN a client, WHEN the player changes sensitivity, FOV, volume and UI scale, THEN the client sends zero reliable RPCs for them, and a CRC of that player's replicated Pawn and PlayerState properties, taken on a second client, is identical before and after. (`StealthGame.Tuning.LocalSettingsNotTuning`)
- **AC-25 (CR-1)**: GIVEN the gameplay C++ modules, WHEN the CI literal scan runs, THEN it fails on any numeric literal in gameplay timing, range or count code that is not on a committed allowlist file, where each allowlist entry carries a reason (engine constant, unit conversion, test). (`StealthGame.Tuning.NoHardcodedGameplay`, CI script)
- **AC-26 (CR-15.5)**: GIVEN a non-selectable preset with `NpcCount` 60 (inside the hard range, outside the safe range), WHEN validated, THEN the value stays 60, one safe-range warning names the key and the preset is accepted. (`StealthGame.Tuning.SafeRangeWarnOnly`)
- **AC-27 (CR-2)**: GIVEN a cooked Dev build in which `Playtest` is referenced only by the catalog, WHEN the host opens with `?Tuning=Playtest` (`HostSelectable`), THEN `Playtest` loads with `FallbackLevel` 0. (`StealthGame.Tuning.PresetsCooked`)
- **AC-28 (CR-10)**: GIVEN the client accessor, WHEN a compile-fail test tries to read a server-only key through `GetClientTuning()` in C++, THEN compilation fails. AND a reflective request for a server-only key on a client returns *invalid* and logs, asserted with an expected-error hook and without aborting. (`StealthGame.Tuning.ServerKeyNotOnClient`)
- **AC-29 (CR-6)**: GIVEN every catalog preset, WHEN validated in CI, THEN each reports zero clamps and zero rejections, and each `HostSelectable` preset and `Default` also reports zero I-4 warnings. (`StealthGame.Tuning.CatalogClean`)
- **AC-30 (CR-7)**: GIVEN a frozen set, WHEN any server system attempts to modify it outside the PIE hot-reload path, THEN the API offers no mutator (const access, compile-level), and the frozen hash is unchanged at `Ended`. (`StealthGame.Tuning.FrozenImmutable`)
- **AC-31 (Edge, preset change)**: GIVEN a non-Shipping session whose stored preset changes via `stealth.tuning.preset` during `Active`, WHEN the round continues, THEN live values are unchanged. AND at the next `Preparing` the new preset is active with `Revision` 0 and a different `TuningHash`. (`StealthGame.Tuning.PresetChangeNextRound`)

**QA notes (qa-lead reviewed; revised 2026-10-24)**:
- AC-2e, AC-8, AC-12b, AC-13 and AC-23 need the multi-process harness (OQ-11).
- AC-9 and AC-13 also depend on Round and Session behaviour and run against stubs until those GDDs land.
- AC-16 and AC-20 are provisional on Round.
- AC-3b, AC-22, AC-25 and AC-29 are CI scripts, not automation tests.
- The project CLAUDE.md still imports the Godot reference while the Unreal test root is `Source/<Module>/Private/Tests/` (existing prototype tests live in `Source/StealthGame/Private/Tests`); test files belong there.

## Open Questions

| # | Question | Owner | Target |
| ---- | ---- | ---- | ---- |
| OQ-1 | **CLOSED — the user decided 2026-10-25: Tuning Data stays a GDD; ADR-0004 covers the implementation** (PR-SCOPE adjustment 6 declined). | user | closed 2026-10-25 |
| OQ-2 | ADR candidates. The engine statements behind them are unverified against UE 5.8 (the reference docs have no coverage of data assets or cooking). | technical-director | architecture phase |
| | • hash algorithm (F-2 fixes the encoding) | | |
| | • schema table generation (X-macro versus reflection plus metadata), and the source of the hard-range table | | |
| | • the public/server-only struct split, and atomic replication of the snapshot (atomic struct or custom net serialisation; Iris behaviour) | | |
| | • one root asset versus several per-domain assets | | |
| | • catalog with soft references versus Asset Manager always-cook | | |
| | • the world-scoped accessor subsystem, and the game-instance preset store | | |
| | • PIE edit-hook debounce and routing | | |
| OQ-3 | The I-4 threshold is provisional (I-3 was replaced with Character Pool's deck keys at the 2026-10-24 reconciliation). Calibrate `BlindGuessWarnPct` from playtest, and decide whether I-4 should instead gate relative to `Default` (for example at most 1.25 times Default's `BlindGuessSuccess`) or on `ExpectedBlindKills`. | systems-designer | after Character Pool GDD and first playtest |
| OQ-4 | **Answered (2026-10-24 review)**: there is no join after `Frozen` (Session & Travel admission is rejoin-only in-round). A rejoin keeps pawn continuity (Session & Travel rule 22) and acknowledges under Session's absolute deadline. | -- | closed |
| OQ-5 | Should `ServerOnly` keys that are truly sensitive (none today) get a server-only cook? Not needed at MVP; revisit at the Dedicated Server stage. | security-engineer | Alpha |
| OQ-6 | **CLOSED — the user decided 2026-10-25: host-selectable presets never ship (Shipping uses `Default` only); presets remain a non-Shipping playtest tool, so the lobby has no preset display in MVP.** Original question: Shipping builds now always use `Default`, enforced in code, and presets work only in non-Shipping builds. Should host-selected presets ever ship (private matches, custom games), and should the lobby show the active preset to every player before a match? Until decided, the listen host's broader data access stays a stated prototype limitation. | user / Lobby UI | closed 2026-10-25 |
| OQ-7 | **Answered (2026-10-24 reconciliation)**: the Generator does not use `TargetRoundMinutes` (Generator change request 3); Round logs it only (F7). | -- | closed |
| OQ-8 | Migration of `UStealthSettings` fields: each mapped key in the Cross-References table needs its owner GDD to register it (name, range, audience, latch). | owner GDD authors | per GDD |
| OQ-9 | **Answered (2026-10-24 reconciliation)**: hard range 2.0-6.0, safe 2.5-4.5 (Objectives OC-14); fast tests use the non-Shipping `DebugObjectiveCompletion`. | -- | closed |
| OQ-10 | `NpcCount` hard maximum 100 versus the performance budget (validated so far only for about 50 characters); and whether non-Shipping test presets need a widened-range tier below the hard minima (for example a 1 s cooldown). | performance-analyst / systems-designer | after the 60-character spike (R3; `NpcCount` 50 default gives 60 characters at 5v5) |
| OQ-11 | Which multi-process test harness is used (Gauntlet or a custom launcher), and which CI job runs it. | qa-lead / devops-engineer | before the first networked story |
| OQ-12 | **CLOSED — approved by the user 2026-10-25 and applied (canonical row, F-1 example, I-NKP-1, catalog flag `bTestAllowsStrikeLimitOne`, AC-4/17/30).** Original question (Penalty CR-11 / OQ-NKP-11): raise the `NpcKillStrikeLimit` hard minimum from 1 to 2 and allow `L = 1` only behind a dev/test flag, so clamping an authored 0 can never reach `L = 1`. Until decided, I-NKP-1 warns on `L = 1` and rejects it in `Default` and `HostSelectable` presets. Recorded in `production/session-state/active.md`. | user | before the first external playtest |
