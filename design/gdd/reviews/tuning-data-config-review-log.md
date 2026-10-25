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
