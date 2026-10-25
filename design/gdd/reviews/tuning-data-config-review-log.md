# Review Log: Tuning Data & Config (design/gdd/tuning-data-config.md)

## Review — 2026-10-24 — Verdict: MAJOR REVISION NEEDED
Review mode: full · Workflow: full
Scope signal: L (multi-system integration with Session & Travel and Round, 4 formulas plus invariants, several ADR candidates; producer should verify before sprint planning)
Specialists: game-designer, systems-designer, qa-lead, network-programmer, unreal-specialist, security-engineer; senior: creative-director
Blocking items: 19 | Recommended: 22
Summary: The defaults match the locked facts and the core idea (server-owned, validated, frozen, hashed tuning) is sound, but TDC duplicated mechanisms that Session & Travel already owns: a second client acknowledgement, a preset carrier and a timeout. The duplicates contradicted each other. The full TuningHash was exposed to clients, and the round-2+ preset source was undefined. Three design faults were also found: a post-freeze join rule that breaks Pillar 1, a self-disabling I-4 threshold, and an underspecified hash encoding. About 14 acceptance criteria were untestable or missing. The creative-director adjudicated: one acknowledgement owned by Session & Travel carrying a client-recomputed ClientHash; TuningHash not replicated; Shipping always uses Default (enforced in code; whether presets ship is a pending user decision); no join after Frozen, with rejoin keeping Session & Travel pawn continuity.
Prior verdict resolved: First review
Findings:
- [BLOCKING] Core Rules CR-9: second ack mechanism duplicates and contradicts Session & Travel rule 11.6 (timeouts, disconnect reasons) [network-programmer, creative-director]
- [BLOCKING] Core Rules CR-7: full TuningHash replicated via S&T FRoundReadiness; hiding rationale incoherent since PresetId/ClientHash are public [network-programmer, security-engineer, systems-designer, game-designer]
- [BLOCKING] Core Rules CR-4: round-2+ preset source undefined; ServerTravel re-runs init and loses ?Tuning=; ?Tuning= vs ?Preset= [network-programmer, unreal-specialist, systems-designer]
- [BLOCKING] Edge Cases: "half or more mismatch aborts the round" enables griefing by quorum and makes AC-10b unreachable [security-engineer, qa-lead, systems-designer]
- [BLOCKING] Core Rules CR-9: ack timer starts at a client-reported signal, so Preparing can be held without bound [security-engineer]
- [BLOCKING] Core Rules CR-9: first-ack-only contradicts resend-then-second-mismatch; resend of an unchanged replicated property impossible; epoch encoding missing [security-engineer, network-programmer]
- [BLOCKING] Core Rules CR-2: "public matches use Default" is only a data flag; Shipping not enforced in code [security-engineer]
- [BLOCKING] Core Rules CR-8 / Edge Cases: post-freeze late join breaks Pillar 1 (spawn without a death); conflicts with S&T rejoin pawn continuity [game-designer, network-programmer]
- [BLOCKING] Formulas I-4: BlindGuessWarnPct is a preset key, so a preset can disable its own gate [game-designer, systems-designer]
- [BLOCKING] Formulas F-3: off-by-one in the strike-limit description; gate not grounded [game-designer, systems-designer]
- [BLOCKING] Canonical Tunables: NpcCount 0-5 is always rejected by the invariants [systems-designer]
- [BLOCKING] Formulas F-2: byte layout underspecified (len, SchemaVersion, type tags, enums, sort); example contradicts AC-5 golden vector [systems-designer, qa-lead]
- [BLOCKING] Core Rules CR-12: hot-reload live set and hash undefined for Round-latched keys; PIE re-ack demotes Ready [systems-designer, network-programmer]
- [BLOCKING] Core Rules CR-15: suffix rule fails the GDD's own keys; Npc* ban conflicts with migrating keys [qa-lead, unreal-specialist]
- [BLOCKING] Core Rules CR-3: no home for range-shaped values (NpcIdleTimeRange FVector2D) [unreal-specialist]
- [BLOCKING] Cross-References: prototype mapping not exhaustive [unreal-specialist]
- [BLOCKING] Acceptance Criteria: shipping symbol scans and "asset changed on disk" tests cannot work [unreal-specialist, qa-lead]
- [BLOCKING] Core Rules CR-9: ack bookkeeping on PlayerController/connection survives seamless travel [unreal-specialist]
- [BLOCKING] Acceptance Criteria: missing ACs (CR-15 lint, safe-range warning, always-cook, CR-10, CI zero-clamp); AC-2 targets shipping; AC-3/AC-6 bundled; AC-9 coverage; AC-17 ambiguous; AC-14a wall clock; harness unnamed [qa-lead]
- [RECOMMENDED] Player Fantasy: "every defeat leads back to a choice" overclaims [game-designer]
- [RECOMMENDED] Canonical Tunables: AssassinationRangeCm max 400 is a reach-mismatch tell (300); DefenceDurationSec safe range narrower than locked 3-5 [game-designer]
- [RECOMMENDED] Canonical Tunables: NpcCount 100 maximum exceeds the unvalidated perf budget; ObjectiveInteractSec 1.0 s may be too short to read [game-designer]
- [RECOMMENDED] Formulas: I-1 justification; F-3 output ranges after I-2; I-4 minimum-NpcCount table; FallbackLevel when Default requested; SchemaVersion bump rule; F-4 about 50 bytes [systems-designer, game-designer, network-programmer]
- [RECOMMENDED] Core Rules: atomic struct and client recompute; world-scoped accessor; PostEditChange debounce; catalog asset with soft refs; class Config flags [unreal-specialist]
- [RECOMMENDED] Edge Cases: pawn count minus public NpcCount leaks the human count (Round/Crowd invariant); identity-tell review for crowd-timing keys; dev builds internal only [security-engineer]
- [RECOMMENDED] Dependencies: HUD is #18 not #19; Participant State & Roster missing from downstream; Round->TDC call direction [main review, game-designer]
- [RECOMMENDED] Acceptance Criteria: rewrites for AC-4/15/16/19/20/21/24/25 [qa-lead]
Specialist disagreements (adjudicated by creative-director): ack ownership (game-designer: defer hash and cut kicks; network: one S&T ack with ClientHash; security: kick mismatchers) -> network + security; presets (game-designer: MVP Default only; security: enforce in code) -> enforce in code, cut decision to user; late join (game-designer: none after Frozen; network: rejoin continuity) -> both.
Revisions: applied by the reviewer in the same session (autonomous pipeline); independent re-review required.
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 59e3b7568f8117f08f140d944a46e1b6aa21aa26
Reviewed-Content-Hash: design/registry/entities.yaml dfeacb02917f751cb1dfd88cf490c7ec3d8d2d0f
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/session-travel.md 9e742c64a4a79cf931dd42fba35efb5dcb6a0d46
Reviewed-Content-Hash: design/gdd/participant-state-roster.md 53860f46744dab69a01d011f94581f60e6b60b3c
Reviewed-Content-Hash: design/gdd/input-interaction-framework.md fb21f33023d9c93a70c9432a202fe835eaeb8f8b
Reviewed-Content-Hash: design/gdd/character-pool-appearance.md 0726280799c1a0eaca75c36595b7684c6e213cdb
Reviewed-Content-Hash: design/gdd/level-socket-contract.md 587d95c70981b40e4986bd5b7257c20fc92c6ac3
Reviewed-Content-Hash: design/gdd/game-concept.md 8dccb622e17a7f1dc58349dd2baa54d52fc36d32
Reviewed-Content-Hash: design/gdd/systems-index.md 2adb098db430a1b7d1c77e1238171c3948bf08eb

## Review — 2026-10-25 — Verdict: NEEDS REVISION
Review mode: lean · Workflow: full
Scope signal: L (multi-system integration with Session & Travel and Round, 4 formulas plus about 60 owner invariants, ADR-0004; producer should verify before sprint planning; systems-index still says S)
Specialists: none (lean, single-session second-round review)
Blocking items: 6 (all revised in the GDD in this pass; a further independent re-review is needed to confirm) | Recommended: 6
Summary: The second round found the first-round blockers resolved: one acknowledgement owned by Session & Travel, TuningHash not replicated, the preset store, rejoin-only admission, I-4 as a schema constant, the byte layout and hard-range tables all hold, and every canonical default, range, audience and latch matches the registry and the owner GDDs. The new defects are all in the layer added since: the schema cannot express the registered disjoint hard ranges (`0 or 3 to 600`, `0 or 0.25 to 5`), several acceptance criteria cannot pass as written (AC-4 fixture, AC-17 flag on a `HostSelectable` preset, AC-30 I-ST-1 example, AC-15 allowlist rejecting registered `Team*` keys, duplicate AC-30 id), and the Session deadline wording and downstream dependency table are stale.
Prior verdict resolved: Yes (19 of 19 first-round blockers verified resolved against the GDD text and the current Session & Travel and Round GDDs)
Findings:
- [BLOCKING] Formulas F-1 / Core Rules CR-3: no rule for disjoint hard ranges; `EndedAutoResetSec` (0 or 3-600, Round) and `SessionHeartbeatIntervalSec` (0 or 0.25-5, Session) cannot be clamped by "clamp to [HardMin, HardMax]". REVISED: schema rows may declare a sentinel (value outside the continuous range, valid by itself); F-1, CR-3, the F-1 variable table, the example and AC-4 updated.
- [BLOCKING] Acceptance Criteria AC-4: the fixture (`NpcCount` 0 clamps to 6 with `TeamSize` 5) fails I-2, so the whole preset is rejected and the stated effective values never exist. REVISED: AC-4 is scoped to the F-1 stage in isolation; whole-preset outcomes stay with AC-6a / AC-17.
- [BLOCKING] Acceptance Criteria AC-17: the `HostSelectable` bullet (`TeamSize` 1, `NpcCount` 21/22) is specified with `NpcKillStrikeLimit` 1 plus `bTestAllowsStrikeLimitOne`, but that flag is rejected on `HostSelectable` presets, so the bullet could never show I-3. REVISED: that bullet uses `NpcKillStrikeLimit` 2 (I-4 result 0.085 added).
- [BLOCKING] Acceptance Criteria AC-30 (owner invariants): the I-ST-1 example (`SessionHardTimeoutSec` 4, Soft 3) clamps Hard to its minimum 5 and passes, and raising Soft alone also breaks I-CNB-1 at `CrowdIdleMinSec` 4, violating "breaks only that row". REVISED: example is Soft 4, Hard 5, `CrowdIdleMinSec` 6. A second criterion also carried id AC-30 (FrozenImmutable); renumbered AC-32.
- [BLOCKING] Acceptance Criteria AC-15 / CR-15.2: the `^(Npc|Player|Bot|Human|Team|Viewer)` scan with a three-key allowlist fails on registered keys `TeamPositionFeedHz`, `TeamPositionFeedGridCm`, `TeamZoneRadiusCm` and the seven `Teammate*` keys (none is a per-team variant). REVISED: allowlist extended in AC-15 and CR-15.2.
- [BLOCKING] Core Rules CR-9: describes the acknowledgement deadline as `TravelLoadTimeoutSec` from travel start; Session & Travel F7 is now `max(t_travel + TravelLoadTimeoutSec, t_ready + TravelAckGraceSec)`. REVISED (CR-9 and the Tuning Knobs note).
- [RECOMMENDED] Core Rules CR-12: the PIE advisory re-check had no channel (Session re-requests on a `Revision` change only from participants not yet `Connected`). REVISED: stated as an editor-only in-process comparison with no RPC.
- [RECOMMENDED] Formulas F-3: stated minima of BlindGuessSuccess (0.0099) and ExpectedBlindKills (0.01) are only reachable with `L` = 1; with the hard minimum 2 they are 0.0197 and 0.02. REVISED.
- [RECOMMENDED] Dependencies: Round key count read 14 (17 registered); Teammate Markers listed Soft while its GDD lists TDC Hard; Informant Pings (Hard) missing from the bidirectionality sentence; GDDs 24, 26, 27, 28 do not exist yet. REVISED (counts, Hard, forward-reference note).
- [RECOMMENDED] Core Rules CR-2: every non-Default preset is still cooked into Shipping paks although Shipping can only run `Default`; consider excluding them from the Shipping cook (ADR-0004 decision; not applied).
- [RECOMMENDED] Formulas I-4: breach rejects `Default`, but the schema-defaults rung equals `Default`, so a `Default` that fails I-4 can only end in `Failed`; AC-29 (CI) is the real guard (acceptable; note for ADR-0004).
- [RECOMMENDED] Formulas: the F-1 `HardMin` exception for `bTestAllowsStrikeLimitOne` was only in prose elsewhere. REVISED (F-1 variable table and AC-4).
Cross-doc follow-ups (main session):
- systems-index.md: TDC status In Review -> Needs Revision (this review is NEEDS REVISION, not an approval); effort S is stale for an L-scope GDD (about 60 owner invariants, 4 formulas, 150 public keys).
- round-match-framework.md acceptance criterion 7 still says "exactly half of the clients mismatching aborts; one fewer does not" for `TuningSnapshotFault`/mismatch; TDC CR-9 and Session rule 11.6 have no quorum abort (a mismatching client is only disconnected). Round's `EndedAutoResetSec` row ("0, or 3 to 600") should say it is a sentinel key per TDC CR-3/F-1; Session's `SessionHeartbeatIntervalSec` likewise.
- Stale `NpcKillStrikeLimit` range "1-5" after the 2026-10-25 hard minimum 2: hud.md F-table (line 276), npc-kill-penalty-informants.md F tables (lines 219, 275), assassination-defence-reversal.md (line 245).
- Stale `NpcCount` ranges in consumer formula tables: concealment-bushes.md (n_N 20-60), information-visibility-policy.md (n_N 20-60), crowd-npc-behaviour.md (35-60), objectives-chain.md (32-60) versus TDC hard 6-100, safe 35-50.
- design/registry/entities.yaml `tuning_derived_indicators` note says I-4 "warns when 100 * BlindGuessSuccess > BlindGuessWarnPct"; TDC I-4 rejects (for Default and HostSelectable presets) when it is >= the threshold. Add `sentinel` handling to the registry entries for `EndedAutoResetSec` and `SessionHeartbeatIntervalSec`.
- teammate-markers.md and informant-pings-team-comms.md list TDC Hard (TDC now agrees); Telemetry (26), Anti-Cheat (27), Dedicated Server (28), Settings (24) GDDs do not exist; TDC rows for them are forward references.
Revisions: applied by the reviewer in the same session (autonomous pipeline); one independent re-review is required before Approved.
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 63db04c802c6db06eb0ba3dac93f65fb9fa98068
Reviewed-Content-Hash: design/registry/entities.yaml 28f7a489149e3fba702986d089b7799719796b39
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/session-travel.md f6e4a60b76f7e3e99045662d1636bb3e226926f1
Reviewed-Content-Hash: design/gdd/round-match-framework.md 97e464757d41252679f8163bd3f90f68d509d23b
Reviewed-Content-Hash: design/gdd/participant-state-roster.md 11106ca55e071c8e8f1ab67926f678d5c3ecf306
Reviewed-Content-Hash: design/gdd/input-interaction-framework.md ed42829b7c7aea9557521fb4c45d124224835244
Reviewed-Content-Hash: design/gdd/character-pool-appearance.md d7835d0ffad2ed35212543f583ba4be4cd81e001
Reviewed-Content-Hash: design/gdd/level-socket-contract.md 170946af3b310140bd789eda4906874349fc59f9
Reviewed-Content-Hash: design/gdd/game-concept.md 6dbca7dc538ed44d89a5f009dbc8eda18782a2c8
Reviewed-Content-Hash: design/gdd/systems-index.md b195f89efa215d57e7630933f75bbbc02bdd9ba8
Reviewed-Content-Hash: design/gdd/hud.md 224977e0b36ac631f3ab286857b5b707178bc4dd
Reviewed-Content-Hash: design/gdd/teammate-markers.md 24083d13eef70e3b1d1b20c9905a338859258db5
Reviewed-Content-Hash: design/gdd/informant-pings-team-comms.md 6f5397e9f8ef2526f8d2c6c107100ba7a2db673b
