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

## Review — 2026-10-25 — Verdict: NEEDS REVISION
Review mode: lean · Workflow: full
Scope signal: L (10 dependencies, 4 formulas, 15 acceptance criteria; ADR amendments already written; producer should verify)
Specialists: none (lean); single-session re-review
Blocking items: 2 | Recommended: 6
Summary: All 8 sections present; every first-round finding is confirmed resolved in the text and no Pillar-1 leak was found (opponents hold no entry, slot or widget). Two new blockers: the ping-point rule says a hit in foliage drops to the ground under it, but ADR-0015/0017 make bush foliage ignore `ViewBlock`, so the ray passes through and the point lands beyond the bush (the rule and its edge case were false), and the third-person form did not follow CS-R8's eye-pivot re-trace; and no acceptance criterion exercised the ping-point rule, though the coverage line credited PNG-R3 to AC-PNG-06, which only aims at ground and sky.
Prior verdict resolved: Yes (4 of 4 blockers and all recommended items verified in the text; the quasi-tracking drop stays deliberately not applied, OQ7)
Findings:
- [BLOCKING] Core Rules PNG-R3 / Edge Cases: foliage "lands on the ground under it" contradicts the `ViewBlock` contract (foliage and pawns ignore it); third-person form did not re-trace from the eye pivot like CS-R8 — FIXED (world-query-only reuse of CS-R8's two-step form; point lands beyond foliage; 1 cm normal offset for the down trace; walkable test named; none within 300 cm sends nothing)
- [BLOCKING] Acceptance Criteria: PNG-R3's point rule, the pawn pass-through (no pawn can be named) and eye-pivot range in 3P were untested — FIXED (AC-PNG-15 added; coverage line updated)
- [RECOMMENDED] Core Rules PNG-R4 / Formulas F2: "10 cm quantised" for `FVector_NetQuantize10` is wrong (it keeps one decimal place of a cm, 0.1 cm) — FIXED in this GDD; IVP `TeamPing` row and ADR-0011 amendment still say "10 cm" (follow-up)
- [RECOMMENDED] Acceptance Criteria AC-PNG-05: asserts the exact refill boundary 11.90 s (0.95 token), not representable exactly in floating point — FIXED (11.91 accepted, 11.89 dropped)
- [RECOMMENDED] Game Feel / AC-PNG-14: the 110 ms p95 own-echo target is about 60 ms RTT plus one server frame, one replication interval and one client frame (about 104 ms by estimate); tight, no breakdown — NOT APPLIED (needs a measured replication cadence for the controller; owner: network-programmer at the first PIE capture)
- [RECOMMENDED] Formulas F4 / PNG-R6: the slot index per team member is unspecified (Roster team order vs `PublicHandle`); a programmer must pick — NOT APPLIED (carrier-level choice for ADR-0001 amendment; follow-up)
- [RECOMMENDED] Dependencies: Camera is one-directional although Pings now consume CS-R8's world query; Crowd (T13, R4) and Assassination (ASN-R6) are cited in Cross-References but not in Dependencies — NOT APPLIED (soft; Camera follow-up below)
- [RECOMMENDED] Acceptance Criteria ordering: AC-PNG-14 sits before AC-PNG-07 — NOT APPLIED (cosmetic)
- [NICE-TO-HAVE] none
Cross-doc follow-ups (main session):
- design/art/art-bible.md 7.x table row 9 (about line 2687) still reads "Informant viewers on the pinging team; never an active Ally unless design relays it", lists a "spatial audio tick" and a feed line "Sighting near [landmark]" as redundancy channels; the GDD ships rings to all allies (user decision 2026-10-25), a non-positional UI tick and no feed line (PNG-R7, R8). Align row 9 (3.4 at line 1211 is already aligned).
- design/gdd/information-visibility-policy.md R3 `TeamPing` row and docs/architecture/adr-0011-pawn-command-interface-input.md line 457: "10 cm quantised" should read 0.1 cm (`FVector_NetQuantize10`).
- design/gdd/camera-system.md CS-R8: add Pings as a third consumer, and expose the world-only query (the ViewBlock segments, without the pawn query) so Pings can reuse it without the pawn query; ADR-0015 §5 if it names consumers.
- docs/architecture/adr-0001 (feature part list): state the slot-index rule for `FTeamPingView.Entries[TeamSize]`.
- design/gdd/systems-index.md row 20: status stays In Review (a revision verdict is not an approval).
Receipt: not stamped. The revisions above were applied after the review, so stamping the current bytes would make the next run treat the revised document as the one that failed. The next review must run in full (`/design-review design/gdd/informant-pings-team-comms.md`, fresh session).
