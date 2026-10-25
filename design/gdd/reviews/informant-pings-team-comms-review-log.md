# Review Log: Informant Pings & Team Communication

## Review — 2026-10-25 — Verdict: NEEDS REVISION
Review mode: lean · Workflow: full
Scope signal: L (11 dependencies, 4 formulas, ADR amendments already written; producer should verify)
Specialists: none (lean); the review was run by one independent reviewer (sonnet) that did not author the GDD
Blocking items: 4 | Recommended: 9
Summary: All 8 sections present and substantive; no Pillar-1 leak found (Team-audience C-TeamView part, never on opponent controllers). Blocking: validation used a Layout Manifest extent that does not exist; the part minted a `RoundEpoch` the Roster owns; the ping-point rule and range origin were inconsistent with Camera CS-R8 and the 3P boom; AC-PNG-04 was non-deterministic because tokens were spent before the range check.
Prior verdict resolved: First review
Findings:
- [BLOCKING] Core Rules PNG-R5: step 4 used a nonexistent `FLayoutManifest` extent — FIXED (extent check removed; range alone bounds the point; LSC dependency removed)
- [BLOCKING] Core Rules PNG-R6/R10: `RoundEpoch` in `FTeamPingView` — FIXED (dropped; client reads the Roster's)
- [BLOCKING] Core Rules PNG-R3, F2: range origin and ground-point rule — FIXED (own rule: `ViewBlock` aim-ray hit, 300 cm drop to walkable ground; range from the eye pivot on both machines)
- [BLOCKING] Acceptance Criteria AC-PNG-04: token spent before range check — FIXED (validation reordered, failures spend nothing; full bucket per case)
- [RECOMMENDED] Formulas F1: client mirror skew at the refill boundary — FIXED (server tolerance ε = 0.05 token; AC-PNG-05 boundary case)
- [RECOMMENDED] Core Rules / Tuning Knobs: quasi-tracking by re-marking a walker — NOT APPLIED (CD-GDD-ALIGN finding 1 ruled manual shadowing intended informant work; OQ7 measures it)
- [RECOMMENDED] Tuning Knobs `PingBucketSize` rationale contradicted one slot per sender — FIXED
- [RECOMMENDED] Formulas F4 / AC-PNG-11: per-element delta unspecified, no headroom — FIXED (fixed-size slot array; limit 80 B/s)
- [RECOMMENDED] Dependencies / cross-docs: Round row, HUD-R4 L0 list, HUD-R17, art bible "(informant)" wording — FIXED (all four documents updated)
- [RECOMMENDED] Core Rules PNG-R4: unused `ClientPingSeq` — FIXED (payload is the point only; AC-PNG-02 asserts it)
- [RECOMMENDED] Core Rules PNG-R7 / Visual: edge-ring label and overlap; "not shown before" — FIXED (no label on edge rings, newest on top; last shown `PingSeq` per sender)
- [RECOMMENDED] Acceptance Criteria AC-PNG-13, AC-PNG-10, Game Feel latency — FIXED (40 % placeholder target; blind sort; AC-PNG-14 added)
- [RECOMMENDED] Acceptance Criteria AC-PNG-01: listen host on the opposing team — FIXED
- [NICE-TO-HAVE] Edge Cases: informant killed with a live ring; bucket across reconnect — FIXED; Ping key discoverability — noted in OQ3; Session "pings" wording — FIXED (latency pings)
Receipt: not stamped. The revisions above were applied after the review, so stamping the current bytes would make the next run treat the revised document as the one that failed. The re-review must run in full (`/design-review design/gdd/informant-pings-team-comms.md`, fresh session).
