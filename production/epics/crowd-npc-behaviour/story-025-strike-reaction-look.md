# Story 025: Strike reaction look: the graybox market reads calm, not a stampede

> **Epic**: Crowd & NPC Behaviour
> **Status**: Ready
> **Layer**: Feature
> **Type**: Visual/Feel
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-001 = Server-only Crowd Director subsystem.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: GDD Visual/Audio Requirements and Game Feel (read those two sections only)
**ADR Decision Summary**: Strike reactions are glances, pauses, short reroutes and rare short panic runs and startle braces; nothing gathers, nothing flees the district, and no run lasts longer than 3 s.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Evidence is captured from a bystander client at 20 m in a PIE or standalone session on the graybox market; the retained clip and screenshots go to `production/qa/evidence/`.

**Why this story**: A reaction rule can pass every statistical test and still look wrong (a ring of onlookers, a stampede). The look is the product (coding standard: a parse check is not a run). Read GDD AC-CNB-19 and the Game Feel section.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: reactions follow story 024's rules unchanged; evidence retained in `production/qa/evidence/` (coding standard)
- Forbidden: tuning reactions to look better in a way that makes the distribution differ by outcome (AC-CNB-18 still binds) (ADR-0016)
- Guardrail: capture must be of the production path, not a special demo mode

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-19, scoped to this story:*

- [ ] (AC-CNB-19) GIVEN the graybox market and a strike in view at 10 m, WHEN recorded from a bystander at 20 m, THEN the clip shows glances and pauses but no gathering (no more than 3 NPCs stationary and facing the strike point within 5 m for more than 5 s, provisional QA thresholds), no flight from the district (no NPC leaves the market area after the strike) and no NPC run longer than 3 s, with at least 3 visible glance or pause reactions.
- [ ] Retained screenshots and the clip are in `production/qa/evidence/` with lead sign-off recorded in `production/qa/evidence/crowd-strike-reaction-look-evidence.md`.

---

## Implementation Notes

*Derived from GDD AC-CNB-19 and CNB-R8:*

- The numeric thresholds in the first criterion are QA tolerances added at review (the GDD says "gathering" and "flight" without numbers); raise them to the design owner if the team wants them in the GDD.
- Capture: strike at 10 m, record 30 s from a bystander at 20 m, repeat for three seeds; keep three screenshots (mid-reaction, after 5 s, after 15 s) and one clip.
- Sign-off is by the creative-director or art-director (the GDD's Visual/Audio lens); record the name and date in the evidence file.
- If the look fails but story 024 passes, the fix is a reaction-share tuning proposal routed through the tuning key change process, not a code special case.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 024: the reaction logic and its statistical tests
- Audio reactions: not in MVP scope for Crowd

---

## Files

- `production/qa/evidence/crowd-strike-reaction-look-evidence.md`
- `production/qa/evidence/<date>_crowd-strike-reaction-*.png` / clip

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25). AC-1 revised after the GAPS verdict (measurable thresholds added).*

- **AC-1**: `Crowd.StrikeReactionLook`
  - Setup: graybox market, 50 NPCs, a strike staged at 10 m, a bystander client at 20 m recording 30 s, three seeds
  - Verify: count NPCs stationary and facing the strike point within 5 m for more than 5 s; check for any NPC leaving the market area; time every NPC run; count glance and pause reactions
  - Pass condition: at most 3 gathered NPCs, none leaving the market, no run over 3 s, at least 3 visible glance or pause reactions
- **AC-2**: evidence retained
  - Setup: the three seeds' captures
  - Verify: screenshots and clip in `production/qa/evidence/`; evidence file with sign-off name and date
  - Pass condition: files exist and the sign-off line is filled in

---

## Test Evidence

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained screenshots in `production/qa/evidence/` plus sign-off in `production/qa/evidence/crowd-strike-reaction-look-evidence.md`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-024-strike-stimulus-and-reactions.md`
- Unlocks: None
