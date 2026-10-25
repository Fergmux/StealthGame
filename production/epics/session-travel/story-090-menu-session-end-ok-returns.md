# Story 090: After the host leaves, every client sees 'The session has ended.' and OK returns to the menu with the address kept

> **Epic**: Session & Travel
> **Status**: Ready
> **Layer**: Foundation
> **Type**: UI
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/session-travel.md`
**Requirement**: `TR-session-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-session-001 = Listen-server direct join; ordered admission gates at PreLogin; capacity 2 x TeamSize)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: AGameModeBase with no match-state machine; four game-instance stores (Session, Roster, Round, Tuning preset) are the only authoritative match state; engine seamless ServerTravel with ?Seed=/?Preset=/?Round= server options and a re-bind of the replaced PlayerController and PlayerState on arrival; Session's single server-requested round acknowledgement; admission gates in PreLogin; Session's own clamped silence clock with engine timeouts as a backstop; rejoin and supersede that re-attach the retained PlayerState; default login pawn disabled; no host migration.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Uses only long-standing AGameModeBase, APlayerController and UNetDriver hooks, each re-read in the 5.8.4 source, so residual risk is behavioural (spikes S3, S4, S5), not API names. Seamless travel is disabled in single-process PIE unless net.AllowPIESeamlessTravel=1 (E7), so every travel acceptance test runs multi-process standalone. DispatchPostLogin is deprecated since 5.6: override PostLogin/OnPostLogin. The listen host never passes PreLogin (E2). Use UE_LOGF for logging.

**Why this story**: This story delivers `menu-session-end-ok-returns` as one buildable behaviour: after the host leaves, every client sees 'The session has ended.' and OK returns to the menu with the address kept. It is traced to the GDD criterion below and to TR-session-001. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): UI Requirements. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: Session status text is a display-only layer in the HUD's L4 (ZOrder 60); controls (Cancel, OK) live in a Session-owned modal pushed at ZOrder 100 through the menu root (GDD UI Requirements, D15, ADR-0021 L4 split); every Session RPC is owner-only (ADR-0005)
- Forbidden: No in-round element may show joins, leaves, connection states, ping values or counts of other participants, except the teammate-only reconnecting lines; Session never replicates connection data of other participants (GDD rule 24; ADR-0001, ADR-0005)
- Guardrail: Status text appears within 1 RTT plus one frame of the gate decision; every waiting state shows exactly one plain status line and ends in bounded time (GDD Game Feel)

---

## Acceptance Criteria

*From design/ux/main-menu.md, Acceptance Criteria (the GDD's UI Requirements name the Host and Join menu; the UX spec holds its criteria), scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] **Session end**: after the host leaves, "The session has ended." with OK appears on every client, OK returns to this screen, and the address field still holds the last address

---

## Implementation Notes

*Derived from ADR-0005 Implementation Guidelines and the GDD rules named above:*

- Screen spec: design/ux/main-menu.md (controls M3-M12). Status overlays are Session-owned and display-only (story 083); this story does not own Settings, Quit, accessibility, resolution, input-parity or performance criteria, which have no epic yet. A retained screenshot of the state is the evidence.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None: the prototype has no menu, lobby panel or status overlay (AStealthHUD draws only in-round debug text).

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- main-menu Settings, Quit, accessibility, resolution and input-parity criteria (no epic owns them yet)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/StealthMainMenu.cpp`
- `production/qa/evidence/menu-session-end-ok-returns-evidence.md`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Manual verification steps:**

- **AC-1**: **Session end**: after the host leaves, "The session has ended." with OK appears on every client, OK returns to this screen, and the address field still holds the last address
  - Setup: Host and at least one client in a session; the client has a stored address.
  - Verify: The host leaves; read each client's screen; press OK.
  - Pass condition: 'The session has ended.' with OK appears on every client. OK returns to the main menu and the address field still holds the last address. Screenshot retained.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: UI
**Required evidence**:
- UI: a retained screenshot of each screen touched in `production/qa/evidence/` and the sign-off record `production/qa/evidence/menu-session-end-ok-returns-evidence.md`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/session-travel/story-067-host-loss-detection-and-session-end.md`, `production/epics/session-travel/story-083-status-overlays-display-only.md`
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), test harness and CI must exist before the files below can be created at those paths. Until it lands, write the code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives.
- Unlocks: None
