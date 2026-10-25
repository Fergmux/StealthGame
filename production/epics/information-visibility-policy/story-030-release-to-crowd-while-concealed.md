# Story 030: A concealed player who disconnects stays culled and becomes culled for former allies at one boundary

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: spike story-004 (S9) has not reported; needs ReleaseToCrowd/ClearBinding events from participant-state-roster.
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

A concealed player pawn is culled for non-allies but stays relevant to the owner and allies. If the owner disconnects and the grace period expires (or an informant disconnects), the pawn is released to the crowd and turns into a concealed NPC, which is culled for every connection. Allies must therefore lose the pawn as well, and they must lose it on the same global boundary grid as any other cull, with their teammate marker entry already gone, so the moment of release is not readable from the ally side or the opponent side. This protects Pillar 1 (a disconnect must not reveal which pawn was a player). The story depends on the S9 cull path and on the Roster's `ReleaseToCrowd` and `ClearBinding` events.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-004` (Concealment hybrid: quantised server cull for non-allies + client presentation hide); also `TR-ivp-005` (Relevancy varies by concealment and relation only)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (section 5.5, "Relation changes while culled"); ADR-0001: Crowd identity rules (section 5, C-TeamView)
**ADR Decision Summary**: A culled player pawn can stop being bound (`ReleaseToCrowd` at grace expiry or an informant's disconnect turns it into a concealed NPC). The Roster's binding events (`ReleaseToCrowd`, `ClearBinding`) notify the Policy, which re-evaluates that pawn's denied connections and closes the newly denied ones at the next grid boundary through the same cull tick.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: E5 (`FindActorChannelRef` + `UActorChannel::Close(EChannelCloseReason::Relevancy)`) [V]; E10 (a closed channel stays closed while the override returns false) [V], in-flight cull/un-cull race [U, S10]; E14 (per-pawn update boundaries are not a common grid, hence the global grid); E15 (client throttle). The cull path is gated on spike S9 (story-004).

**Control Manifest Rules (this layer)**:
- Required: **The concealment cull lands on the global boundary grid and closes the channel explicitly per denied connection in the cull tick**, with no client acknowledgement wait; un-cull happens in the same server step as the state change — source: ADR-0003
- Required: **Team-audience data travels only in a `UTeamViewComponentBase` subclass on the receiving teammate's own PlayerController; owner-audience data only on the owner's PlayerController** — source: ADR-0001
- Forbidden: **Never rely on the relevancy override alone to cull** — the channel closes 1-6 s late with jitter (a timing channel) — source: ADR-0003
- Guardrail: **Identity policy**: C-TeamView pushes change-only, ≤ 1 per frame, within the 50 KB/s per-client budget — source: ADR-0001

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md` (AC-IVP-32), scoped to this story:*

- [ ] **AC-IVP-32** [PIE-3] `StealthGame.Visibility.ReleaseToCrowdWhileConcealed`: GIVEN a concealed player who disconnects, WHEN `ReleaseToCrowd` runs, THEN the pawn stays culled for non-allies, becomes culled for former allies at the same boundary with their marker entry already gone, and never un-culls before release. (Edge Cases) Clarification (story-level, QL-STORY-READY): "same boundary" means the former allies' cull lands at the first global grid boundary at or after the `ReleaseToCrowd` step, and their C-TeamView marker entry for that pawn is removed before that boundary.

---

## Implementation Notes

*Derived from ADR-0003 section 5.5 ("Relation changes while culled"), ADR-0001 section 5 and ADR-0008 sections 2-3:*

- The Policy subscribes to the Roster's binding events (`ReleaseToCrowd`, `ClearBinding`) through a Foundation-declared, consumer-owned interface (ADR-0007: lower layers declare the interfaces they consume; registrations are weak and cleared in `Deinitialize`/`EndPlay`). On the event the Policy treats the pawn as an unbound (NPC-row) subject, re-evaluates the pawn's denied connections and, for each newly denied former ally, closes the channel at the next grid boundary through the same `FConcealmentCullTickFunction` (`TG_PostUpdateWork`) that story-024 builds. No second cull path.
- The ally's C-TeamView teammate entry (`PawnRef`, `IsConcealed`) for that pawn is removed by the Roster push step (the only writer); the assertion is on ordering by server step: marker entry removed before the boundary at which the ally's cull lands.
- The pawn must stay culled for non-allies for the whole window and must never un-cull before release (assert the non-ally view across the entire window, not only at the endpoints). A concealed NPC is culled for every connection (R7.1).
- Pawn parity: the released pawn becomes an NPC-row subject; nothing replicated differs from any other concealed NPC (no `COND_OwnerOnly` change visible to non-owners, ADR-0001 4.2).
- Negative control (added by the story, ADR-0008 section 3): a harness-only variant that keeps the ally's marker entry past the boundary (or delays the ally's cull past the boundary) must be detected by the suite; the suite passes only if the control is detected. The control type lives only in `StealthTestHarness`.
- T2 test: `NETWORK_TEST_CLASS(ReleaseToCrowdWhileConcealed, "StealthGame.Visibility.PIE")` with `WithGameMode(AStealthGameMode)`, a fixture map, at least 3 connections (owner, ally, opponent). Bound by server time and frames, never wall clock; assert and record the active replication system (legacy). The disconnect and `ReleaseToCrowd` are driven by a harness caller until participant-state-roster and session-travel deliver the real events.
- Spike gate: story-004 (S9) must report first (the cull tick and explicit close are specified from S9's result).
- Prototype: Replaces: `bConcealed`/`bRevealedUntilExit` replication to all clients and the client-side `UpdateConcealmentVisibility` toggle (`Source/StealthGame/Private/StealthCharacter.cpp:63-64`), through story-024. Promotes: nothing. Reference only: `Source/StealthGame/Private/Spikes/StealthIdentitySpike.*`.
- Cross-epic: needs participant-state-roster's `ReleaseToCrowd` and `ClearBinding` events and its C-TeamView push step; session-travel's disconnect and grace-expiry path (or a harness caller standing in for it).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-004: spike S9, cull latency and explicit channel close
- story-024: the cull record, cull tick and audience (this story reuses them)
- story-025: cull timing and the global grid on the normal concealment path
- story-021: team view never reaching opponents across takeover, disconnect and rejoin
- participant-state-roster epic: the binding events and the C-TeamView push step; session-travel epic: grace expiry

---

## Files

*The code and test files this story creates or changes (planned):*

- `Source/StealthFoundation/Public/Server/StealthVisibilityPolicy.h` (binding-event handler, extends story-024)
- `Source/StealthFoundation/Private/Visibility/StealthVisibilityPolicy.cpp` (re-evaluation of denied connections on `ReleaseToCrowd`/`ClearBinding`)
- `Source/StealthTestHarness/Private/Tests/PIE/ReleaseToCrowdWhileConcealedTest.cpp` (T2, `NETWORK_TEST_CLASS(ReleaseToCrowdWhileConcealed, "StealthGame.Visibility.PIE")`, plus the harness-only negative-control variant)

*Note: the `StealthFoundation` and `StealthTestHarness` module folders are created by tuning-data-config Exit A (they do not exist yet).*

---

## QA Test Cases

- **AC-1**: Release to crowd while concealed (AC-IVP-32)
  - Given: a concealed player pawn whose owner disconnects.
  - When: `ReleaseToCrowd` runs.
  - Then: the pawn stays culled for non-allies; former allies' cull lands at the first global grid boundary at or after the `ReleaseToCrowd` step; their C-TeamView marker entry for that pawn is removed before that boundary; the pawn never un-culls before release.
  - Edge cases: assert marker removal ordering by server step; include the non-ally view for the whole window; negative control (added): a variant that keeps the ally marker entry past the boundary must be detected.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/PIE/ReleaseToCrowdWhileConcealedTest.cpp` (T2 test, must exist and pass) OR a documented playtest or spike report

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-004-spike-s9-cull-latency-and-explicit-channel-close.md`, `production/epics/information-visibility-policy/story-024-concealment-cull-audience.md`; participant-state-roster epic (`ReleaseToCrowd` and `ClearBinding` events, C-TeamView push step); session-travel epic (disconnect and grace expiry)
- Unlocks: None
