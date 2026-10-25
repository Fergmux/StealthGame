# Story 012: Strike-window drop

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (edge case C-W32 / S-I5 / D-W6, PNG-R3 gate paragraph; AC-PNG-18)
**Requirement**: `TR-png-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (outcome-blind `FActionState` at the queue slot, contact-time pending writes), with ADR-0011 §9 (the gate reads the presented role)
**ADR Decision Summary**: At an Attack's slot only the attacker's `FActionState` changes on the wire and every outcome-revealing write is staged at `ContactServerTime - StrikeNetLeadSec`; the victim's client presents the role change at its own fall start. A ping must therefore reveal nothing early: the client gate reads `PresentedOwnRole` and the bound-pawn state, the server drops presses from the resolving slot with no token, reply, echo, tone or plate.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0018 overall; MotionWarping is Beta in 5.8.4, not used by this story)
**Engine Notes**: The test drives presses on a fixed 0.05 s grid against a PIE strike fixture; timing assertions use queue slot order and `OnRoleChangePresented`, not wall clock. Run under the ADR-0008 net-emulation profile that the assassination epic's PIE tests use so the resolving slot and the victim's presentation are separated as in play.

**Control Manifest Rules (this layer)**:
- Required: the client gate reads `PresentedOwnState`/role and the bound-pawn state, never the raw own `State`; from the own fall start until a body is bound nothing is sent — source: ADR-0011
- Required: a strike-window press dropped server-side spends no token and gets no reply, echo, tone or plate — source: ADR-0011, ADR-0018
- Forbidden: Never send an owner-only reply that a victim receives before contact (it would announce the strike early) — source: ADR-0018, GDD ASN-R6

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (AC-PNG-18, edge case C-W32), scoped to this story:*

- [ ] **AC1** (AC-PNG-18 a, `[Integration]` `StealthGame.Pings.PIE.StrikeWindowDrop`): **GIVEN** an `Active` sender struck by an open assassination, one struck by a bush strike, and an informant killed (`InformantKilled`) **WHEN** each presses `IA_Ping` every 0.05 s from the strike's acceptance **THEN** presses before the resolving slot pass steps 1-3 (none is dropped for `State`, `Pawn` or `Concealed`); the first `PingBucketSize` are accepted and later ones are dropped only with reason `Bucket` (a 0.05 s press grid drains the 2-token default bucket in 0.1 s).
- [ ] **AC2** (AC-PNG-18 b): **GIVEN** the same runs **WHEN** presses arrive from the resolving slot to the sender's own fall start **THEN** they reach the server and are dropped at step 2 with no token spent, no reply, no echo, no tone, no plate, no cue event on the sender's side, and no slot change on the team part.
- [ ] **AC3** (AC-PNG-18 c): **GIVEN** the sender's own fall start (`OnRoleChangePresented`) **WHEN** presses continue until 1 s after contact and through the awaiting-takeover period until a body is bound **THEN** the client sends nothing.

---

## Implementation Notes

*Derived from the GDD edge case and ADR-0018 / ADR-0011 §9:*

Two windows (GDD): (a) strike accepted but not yet resolved, the sender is still `Active` with a bound pawn on the server, so the ping is accepted like any other; (b) from the resolving slot onward the server has written `Active -> Informant` and retired the binding (Roster R8; `InformantKilled` retired it for a killed informant; an exile releases the pawn to the crowd), so server step 2 drops it (`State` or `Pawn`) spending no token. The client keeps sending until its own fall start because its gate reads `PresentedOwnRole` (story 009). The client's bucket mirror is then one token below the server's, which is intended (story 009 AC3). This story adds no production logic beyond what stories 005, 009 and 010 provide; it is the integration proof, and the place where a mismatch (for example a reply sent on a drop, or the gate reading raw `State`) is caught. Use the assassination epic's strike fixtures (open assassination, bush strike) and the penalty epic's informant kill rather than building new ones.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Stories 005 and 009: the server drop reasons and the gate function (unit level).
- assassination-defence-reversal epic: strike resolution, `OnRoleChangePresented` and the victim's presentation (`production/epics/assassination-defence-reversal/story-017-no-warning-and-no-observer-cue.md`).
- npc-kill-penalty-informants epic: `InformantKilled` and the awaiting-takeover period.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Pings/StrikeWindowDropTest.cpp` (CQTest PIE, `NETWORK_TEST_CLASS(StrikeWindowDrop, "StealthGame.Pings.PIE")`)
- `Source/StealthGame/Private/StealthPlayerController.cpp` (only if the integration run shows the gate inputs are mis-sourced)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 pre-resolution presses**
- Given: a sender struck by an open assassination; one struck by a bush strike; an informant killed (`InformantKilled`).
- When: `IA_Ping` every 0.05 s from the strike acceptance up to the resolving slot.
- Then: no `State`/`Pawn`/`Concealed` drops; the first 2 accepted; the rest dropped with reason `Bucket`.
- Edge cases: the press on the tick of the resolving slot belongs to AC2; the 60 Hz step puts presses every 3 ticks.

**Test: AC2 resolving slot to own fall start (AC-PNG-18 b)**
- Given: the same three runs.
- When: presses from the resolving slot to the sender's own fall start.
- Then: they reach the server and are dropped at step 2, reason `State`; no token spent, reply, echo, tone, plate or sender-side cue event; no slot change on the team part.
- Edge cases: check the bucket before and after.

**Test: AC3 own fall start onward (AC-PNG-18 c)**
- Given: `OnRoleChangePresented` has fired.
- When: presses continue until 1 s after contact and through awaiting-takeover until a body is bound.
- Then: the client sends nothing.
- Edge cases: the first press after the body binds is evaluated normally.

QL-STORY-READY: GAPS (revised) — AC1 was corrected (bucket drops) as above on the qa-lead's proposal.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Pings/StrikeWindowDropTest.cpp` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-005-server-validation-core.md`, `story-009-client-gate-and-bucket-mirror.md`, `story-010-ia-ping-action-and-send-path.md`; cross-epic assassination-defence-reversal epic (strike fixtures), npc-kill-penalty-informants epic (`InformantKilled`)
- Unlocks: None
