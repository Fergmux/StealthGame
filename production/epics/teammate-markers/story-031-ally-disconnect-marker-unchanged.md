# Story 031: Ally disconnect leaves the marker unchanged

> **Epic**: Teammate Markers
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

When an ally disconnects, the pawn stays bound and AI-held during the grace period (Round R17), so the marker stays filled and unchanged: shape, fill and opacity never encode connection, and there is no "reconnecting" marker state. The only change is the team-only `bReconnecting` bit (user decision 2026-10-25, MVP item 10), which gives the focused label the "· reconnecting" suffix and the HUD its feed lines. The flag clears only on a Roster event (a completed rejoin, the grace-expiry conversion of an `Active` ally, a host kick, record removal in the lobby, round end; ruling D23), so the suffix persists for an informant ally that stays away and for an `Active` ally killed while disconnected. At grace expiry (`ReleaseToCrowd`) the marker holds `TeammateMarkerRetireHoldSec` and fades out, never returning over the released pawn. This story verifies the whole sequence on a real match and carries the Markers half of MVP item 10 (about 0.5 d).

**GDD**: `design/gdd/teammate-markers.md` (TM-R11 reconnecting suffix, Edge Cases connection, travel and phase; AC-TMK-26)
**Requirement**: `TR-tmk-???` (TM-R11 and MVP item 10 have no registry entry; the registry should gain one)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (reconnecting lines and label suffix come only from the teammate `bReconnecting` bit, HUD-R10a)
**Also governed by**: ADR-0001 (the one Team-audience connection bit); ADR-0005 (Roster flag clear rules)
**ADR Decision Summary**: Team feed reconnecting and back lines come only from the teammate `bReconnecting` bit; the marker label adds "· reconnecting" and the marker shape never changes.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Drive the disconnect with the host's connection-drop test hook and the grace timers from the Round and Session test seams (fake clock); the grace length `G_eff` is read from the Round tuning snapshot.

**Control Manifest Rules (this layer)**:
- Required: Team feed reconnecting/back lines come only from the teammate `bReconnecting` bit (HUD-R10a); the marker label adds "· reconnecting" and the marker shape never changes — source: ADR-0021
- Forbidden: Never encode connection state in the marker's shape, fill or opacity; the label never reads "back" or "disconnected" — source: GDD TM-R11, ADR-0021
- Guardrail: No connection state reaches non-owners (Session rule 24)

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-26 and the Edge Cases), scoped to this story:*

- [ ] **AC1** (AC-TMK-26 a, [PIE-2][Net] `DisconnectNoState`): **GIVEN** an ally who disconnects mid-round **WHEN** grace runs and then expires **THEN** during grace the marker is unchanged (filled, no "reconnecting" marker state, no flicker: fill and alpha constant every frame) while the focused label reads "{Name} · reconnecting" and returns to "{Name}" on a reclaim, and at expiry it holds `TeammateMarkerRetireHoldSec` then fades out and never returns over the released pawn.
- [ ] **AC2** (AC-TMK-26 b-d): **GIVEN** (b) an informant ally that stays disconnected past `G_eff`, (c) an `Active` ally killed while disconnected, (d) a superseding rejoin still loading at `G_eff` **THEN** where a marker exists the label keeps the suffix until the Roster clears the flag, whatever `ReconnectEndCause`, and the label only ever reads "{Name}" or "{Name} · reconnecting".
- [ ] **AC3** (Edge Cases, derived): **GIVEN** the `ReleaseToCrowd` boundary **THEN** `IsMarkedAlly` stays true for the 0.25 s hold over the released pawn (accepted harmless, allies-only) and a concealed ally's marker freezes at its last anchor position.

---

## Implementation Notes

*Derived from TM-R11 and the Edge Cases:*

Test matrix on a 2-client PIE (viewer plus the ally who drops, a third ally for the informant case): record per frame the marker fill, alpha and drawn flag, the focused label text, and `IsMarkedAlly` for the ally's pawn. Cases: (a) disconnect then reclaim inside grace; (a') disconnect then grace expiry; (b) informant ally gone past `G_eff` (no grace of its own, so nothing is released and the suffix persists until a rejoin or the round ends); (c) an `Active` ally struck while disconnected; (d) a rejoin that is superseded and still loading at `G_eff`. The suffix is a function of `bReconnecting` alone and does not depend on `ReconnectEndCause`; the label exists only while a marker does. The marker itself never changes with the flag (story 013 already asserts this at the model level).

For the visible half, retain a screenshot of the focused label with the suffix once story 022 exists (that screenshot is story 022's evidence); this story's evidence is the per-frame dumps.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-013-label-focus-and-reconnecting-text.md`: the label text logic.
- `story-022-name-label-plate-rendering.md`: drawing the plate.
- `participant-state-roster/story-051-pie-teammate-reconnecting-flag.md` and `story-054-disconnect-grace-timing.md`: the Roster flag and grace behaviour.
- the hud epic: the feed lines (HUD-R10a, AC-HUD-28).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Markers/DisconnectNoStateTest.cpp` (CQTest, `StealthGame.Markers.PIE.DisconnectNoState.*`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: an ally disconnecting mid-round. When: grace runs, then expires; on a separate run it is reclaimed. Then: during grace the marker stays filled with no flicker and the focused label reads "{Name} · reconnecting", returning to "{Name}" on reclaim; at expiry it holds `TeammateMarkerRetireHoldSec`, fades out and never returns over the released pawn. Edge cases: reclaim on the last grace frame.
- **AC2**: Given: (b) an informant ally disconnected past `G_eff`, (c) an `Active` ally killed while disconnected, (d) a superseding rejoin still loading at `G_eff`. When: each runs. Then: where a marker exists, the label keeps the suffix until the Roster clears the flag, whatever `ReconnectEndCause`, and reads only "{Name}" or "{Name} · reconnecting". Edge cases: no marker exists, so no label.
- **AC3**: Given: the `ReleaseToCrowd` boundary and a concealed disconnected ally. When: the release occurs. Then: `IsMarkedAlly` stays true for the 0.25 s hold over the released pawn, and the concealed ally's marker stays at its last anchor position. Edge cases: the hold is cut short by `Reset`.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the CQTest `DisconnectNoStateTest.cpp` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-006-lifecycle-retire-hold-takeover-direct-switch.md`, `story-013-label-focus-and-reconnecting-text.md`, `story-016-presenter-local-controller-only.md`; cross-epic `production/epics/participant-state-roster/story-051-pie-teammate-reconnecting-flag.md`, `story-054-disconnect-grace-timing.md`
- Unlocks: None
