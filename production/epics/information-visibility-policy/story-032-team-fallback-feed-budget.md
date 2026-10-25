# Story 032: The teammate fallback position feed costs 13 B per entry and at most 104 B/s per client

> **Epic**: Information Visibility Policy
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

When a teammate's pawn has no open, acknowledged actor channel on a connection (travel re-push, a takeover in flight, initial channel latency, any cull), the viewer's marker needs another source for that ally's position. The Visibility Policy builds a small fallback feed on the viewer's own C-TeamView, sent to teammate connections only, and this story proves its cost and behaviour: 13 bytes per entry, bounded by `N_u * 13 * h`, a `Seq` that advances on every feed tick, a stop within one period of the `PawnRef` resolving, and silence during travel. A feed that varied with anything but the unresolved count would be a side channel, so the measured bytes are a gate. It must not wait for the cull: the per-connection trigger is an injected channel-state provider.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-009` (Teammate fallback position feed on C-TeamView)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (section 7); ADR-0001: Crowd identity rules (section 5, C-TeamView)
**ADR Decision Summary**: A Visibility Policy-owned C-TeamView part, `UTeamPositionFeedComponent` (derived from `UTeamViewComponentBase`), on each player's controller carries entries `{PublicHandle, Pos, Seq}`; an entry is added in the same server frame in which a connection's channel for the teammate goes missing (or is not yet acknowledged) and removed one feed update after the channel's open is acknowledged; bandwidth is `B_feed = N_u * 13 B * h` at most 104 B/s, and only teammate connections ever receive it.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `UChannel::OpenAcked` as the "open acknowledged" signal is [U] in ADR-0003 section 7 and is verified against the 5.8.4 source as part of this story. `FindActorChannelRef == null` is [V] (E5). Under change-only replication a stationary ally's entry only changes because `Seq` advances (user decision 2026-10-25, Markers OQ14). The T2 measurement is in-process PIE under the legacy driver.

**Control Manifest Rules (this layer)**:
- Required: **The teammate fallback feed is added in the same frame a teammate's channel goes missing, to teammate connections only; every entry carries a `uint8 Seq` advanced on every feed tick** — source: ADR-0003
- Required: **Team-audience data travels only in a `UTeamViewComponentBase` subclass on the receiving teammate's own PlayerController; owner-audience data only on the owner's PlayerController** — source: ADR-0001
- Forbidden: **Never skip `FActionState.StartServerTime` in an `FActionState` write, and never send a feed entry without advancing its `Seq`** — source: ADR-0003, ADR-0017, ADR-0018
- Guardrail: **Replication**: fallback feed ≤ 104 B/s per client — source: ADR-0003

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md` (AC-IVP-16), scoped to this story:*

- [ ] **AC-IVP-16** [Unit][Int] `StealthGame.Visibility.TeamFallbackFeedBudget`: GIVEN 0-4 unresolved entries at 2 Hz, WHEN the feed runs, THEN each entry is 13 B, measured bytes/s ≤ `N_u·13·h` (≤ 104 B/s), `Seq` advances by one per feed tick per entry, the feed stops within one period of the `PawnRef` resolving, and nothing is sent during travel. (F6) Clarification (story-level, QL-STORY-READY): the measurement point is serialised feed-entry payload bytes per second on the feed component (s = 13 B per entry as defined by F6, times entries times feed ticks), not whole-connection bytes.

---

## Implementation Notes

*Derived from ADR-0003 section 7 and Key Interfaces, ADR-0001 section 5, ADR-0007 section 5, ADR-0008 sections 1-4:*

- Structure: a plain-C++ core (`FTeamPositionFeedCore`, no `UWorld` dependency, ADR-0007 section 5) holds the trigger logic, the `Seq` counter, the 10 cm quantisation of the teammate's feet (skeletal mesh world location) to `int16 x 3`, and the add/remove timing; a thin `UTeamPositionFeedComponent : UTeamViewComponentBase` (`UPROPERTY(Replicated) TArray<FTeamPositionFeedEntry> Entries`, at most 4) replicates it. `FTeamPositionFeedEntry` is `{PublicHandle, Pos, Seq}`, 13 B as serialised (F6); the wire struct is client-safe and sits under `Public/View/`, the builder under `Public/Server/`.
- Trigger per connection (ADR-0003 section 7): during `Active`, a teammate's pawn exists on the server and that connection has no open actor channel for it, or one whose open is not yet acknowledged. The channel state comes from an injected provider interface (so the T1 core and the T2 test do not wait for the cull); the production provider uses `FindActorChannelRef` and the open-acknowledged signal. The entry is added in the same server frame in which the channel goes missing (or the `PawnRef` is re-pushed unresolved) and removed one feed update after the open is acknowledged.
- Rate `TeamPositionFeedHz` (safe 1.5-4, test at 2 Hz) is a tuning value (TDC), never a literal in code. `Seq` is a wrapping `uint8` advanced on every feed tick for every entry, so a stationary ally's entry still changes under change-only replication.
- Only teammate connections ever receive the feed: it rides the receiving teammate's own PlayerController. Nothing is sent during travel (the feed core is quiet while the phase is not `Active`).
- Wire overhead is out of scope by design (budget it elsewhere): the measurement is payload bytes on the feed component, `N_u * 13 * h`. Test each `N_u = 0 to 4`; at `N_u = 4`, `h = 4` the bound is 208 B/s so the 104 B/s ceiling is checked at the 2 Hz default (4 x 13 x 2).
- Verify the `UChannel::OpenAcked` [U] signal in the 5.8.4 source as part of this story and record the finding (file and lines) in the story's evidence note; if the signal is not usable, escalate to ADR-0003 for an amendment rather than polling a substitute.
- T1 core test: `StealthGame.Visibility.TeamFallbackFeedBudget` (class `FVisibilityTeamFallbackFeedBudgetTest`). T2 measurement: `NETWORK_TEST_CLASS(TeamFallbackFeedBudget, "StealthGame.Visibility.PIE")` with `WithGameMode(AStealthGameMode)`, bound by server time and frames, recording the active replication system.
- Prototype: Replaces: none (new). Promotes: nothing. Reference only: `Source/StealthGame/Private/StealthHUD.cpp:171-174` (the prototype marker world iteration, replaced by teammate-markers).
- Cross-epic: needs participant-state-roster's `UTeamViewComponentBase` C-TeamView base and `PawnRef` pushes; tuning-data-config Exit A (module skeleton, harness) and its `TeamPositionFeedHz` key.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-033: teammate marker behaviour on concealed allies (consumes this feed)
- story-021: team view never reaching opponents (consumes this component)
- story-024: the cull whose channel closes will later trigger the feed
- teammate-markers epic: marker widgets and Markers' staleness rule; participant-state-roster epic: the C-TeamView base and `PawnRef`

---

## Files

*The code and test files this story creates or changes (planned):*

- `Source/StealthFoundation/Public/View/StealthTeamPositionFeed.h` (`FTeamPositionFeedEntry`, `UTeamPositionFeedComponent`)
- `Source/StealthFoundation/Public/Server/StealthTeamPositionFeedBuilder.h` (core, channel-state provider interface)
- `Source/StealthFoundation/Private/Visibility/StealthTeamPositionFeed.cpp`
- `Source/StealthFoundation/Private/Tests/Visibility/TeamFallbackFeedBudgetTest.cpp` (T1, `FVisibilityTeamFallbackFeedBudgetTest`, `StealthGame.Visibility.TeamFallbackFeedBudget`)
- `Source/StealthTestHarness/Private/Tests/PIE/TeamFallbackFeedBudgetTest.cpp` (T2 measurement, `NETWORK_TEST_CLASS(TeamFallbackFeedBudget, "StealthGame.Visibility.PIE")`)

*Note: the `StealthFoundation` and `StealthTestHarness` module folders are created by tuning-data-config Exit A (they do not exist yet).*

---

## QA Test Cases

- **AC-1**: Team fallback feed budget (AC-IVP-16)
  - Given: 0 to 4 unresolved entries at 2 Hz, with an injected channel-state provider.
  - When: the feed runs.
  - Then: each entry is 13 B as serialised feed-entry payload on the feed component; payload bytes per second <= `N_u*13*h` (104 B/s at most); `Seq` advances by 1 per feed tick per entry; the feed stops within one period of `PawnRef` resolving; nothing is sent during travel.
  - Edge cases: wire overhead is out of scope by design (budget it elsewhere); test each `N_u` = 0 to 4; the `UChannel::OpenAcked` signal check in 5.8.4 source stays part of the story.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Visibility/TeamFallbackFeedBudgetTest.cpp` (the studio docs' `tests/unit/visibility/`) — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: tuning-data-config Exit A (module skeleton, harness, include lint); participant-state-roster epic (the `UTeamViewComponentBase` C-TeamView base and `PawnRef` pushes)
- Unlocks: `production/epics/information-visibility-policy/story-033-teammate-marker-concealed-ally.md`, `production/epics/information-visibility-policy/story-021-team-view-never-to-opponents.md`
