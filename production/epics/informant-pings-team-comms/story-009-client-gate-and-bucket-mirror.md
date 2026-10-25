# Story 009: Client gate and bucket mirror

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (PNG-R3 gate paragraph, F1 client mirror, edge cases C-W32 and concealed)
**Requirement**: `TR-png-007` (also `TR-png-010` mirror and the client half of `TR-png-003`)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline (Technical Setup 2026-10-25b: `FTeamPingClientGate`, `IPingClientView`)
**ADR Decision Summary**: The client pre-check is the pure `FTeamPingClientGate::Evaluate(FTeamPingGateInputs) -> {Send, RefusedConcealed, RefusedSilent}` fed plain owner facts by `AStealthPlayerController` (phase, `PresentedOwnRole`, bound pawn, `IsOwnPawnConcealedForPing()`, bucket mirror) in the order phase, role, bound pawn, concealed, point rule, bucket; concealed is true for `Entering`, `Concealed`, `Exiting` and `Emerging` (fail-closed); `RefusedConcealed` sends nothing and increments `ConcealedRefusalSeq`. `IPingClientView` exposes `GetConcealedRefusalSeq()` and `GetBucketMirrorTokens()`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 overall; this story is plain C++ over injected facts)
**Engine Notes**: No engine API: the gate and mirror are headless. `IPingClientView` is a `UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))` in `Public/View/` implemented by `UTeamPingViewComponent`; neither the gate nor the interface may include a `Public/Server/` header, a Core or another Feature header (ADR-0007 Technical Setup 2026-10-25b).

**Control Manifest Rules (this layer)**:
- Required: the client gate reads the HUD's presented own role and the bound-pawn state, never the raw own `State`; concealed is fail-closed — source: ADR-0011, ADR-0017
- Required: `RefusedConcealed` sends nothing (no RPC, no token, mirror untouched, no pawn-command `End`) and increments `ConcealedRefusalSeq` — source: ADR-0011
- Forbidden: Never let Presentation read concealment state or the Pings service; it observes only `ConcealedRefusalSeq` through `PingView` — source: ADR-0007, ADR-0021

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (PNG-R3, F1, edge cases C-W32 and concealed), scoped to this story:*

- [ ] **AC1** (TR-png-007, TR-png-003 client half): **GIVEN** `FTeamPingClientGate::Evaluate(FTeamPingGateInputs)` **WHEN** driven table-wise **THEN** it checks in the order phase, presented role, bound pawn, concealed, point rule, bucket and returns `Send`, `RefusedConcealed` or `RefusedSilent`; `RefusedConcealed` is returned only when phase, role and bound pawn pass (concealed plus no ground returns `RefusedConcealed`, concealed plus wrong phase returns `RefusedSilent`); concealed is true for `Entering`, `Concealed`, `Exiting` and `Emerging` (fail-closed).
- [ ] **AC2** (TR-png-007, C-W32 client half): **GIVEN** raw own `State` `Informant` but `PresentedRole` `Active` (a victim in the strike window) **WHEN** evaluated **THEN** the gate returns `Send` (it reads `PresentedRole`, never raw `State`); from the own fall start until a body is bound (`bOwnPawnBound` false) it returns `RefusedSilent` and nothing is sent.
- [ ] **AC3** (TR-png-010 mirror): **GIVEN** the bucket mirror **WHEN** used **THEN** it is strict (sends only when tokens >= 1), advances on `EstimatedServerNow()` from its own send times, is reseeded on a HUD rebuild to a full bucket (`PingBucketSize` tokens) stamped at `EstimatedServerNow()` (a server-side drop caused by a mismatch with the server's bucket is silent and spends nothing), is left untouched by `RefusedConcealed`, and `GetBucketMirrorTokens()` is read-only.

---

## Implementation Notes

*Derived from ADR-0011 §9 (Technical Setup 2026-10-25b) and GDD PNG-R3:*

`FTeamPingGateInputs { ERoundPhase RoundPhase; EPresentedOwnRole PresentedRole; bool bOwnPawnBound; bool bConcealed; FTeamPingPointResult Point; float MirrorTokens; }`. `Evaluate` is a straight-line function in that order, no state. The mirror reuses the bucket class of story 004 in strict mode; it advances on `EstimatedServerNow()` (ADR-0002 §9) and is told of its own sends. `bOwnPawnBound` means a pawn is possessed and the own `FDeathState` is clear (false from the presented role change until a body is possessed). `IPingClientView::GetConcealedRefusalSeq()` is monotonic, +1 per `RefusedConcealed` press, never reset or decremented, compared by inequality, a plain non-replicated member of the owner's own component. Because a server drop in the strike window is silent, the mirror stays one token below the server's; it refills long before a body is bound again (informant downtime is at least `InformantRespawnDelaySec`), and a HUD rebuild reseeds it.

The controller side (collecting the inputs from `UPawnCommandClient::GetPresentedOwnRole()`, `FRoundPublicState`, Concealment's `IsOwnPawnConcealedForPing()`) is story 010 and 011.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 008: the point rule that produces `FTeamPingPointResult`.
- Story 010: `IA_Ping`, the controller collecting inputs and sending the RPC.
- Story 011: the concealed-press refusal as an end-to-end behaviour, including the HUD plate and tone.
- Story 012: the strike-window behaviour in a real match.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/View/Features/Pings/StealthTeamPingClientGate.h` (`FTeamPingGateInputs`, `FTeamPingClientGate`, `IPingClientView`)
- `Source/StealthGameplay/Private/Features/Pings/StealthTeamPingClientGate.cpp`
- `Source/StealthGameplay/Private/Tests/Pings/PingClientGateTest.cpp` (class `FPingsClientGateTest`, name `StealthGame.Pings.ClientGate`)
- `Source/StealthGameplay/Private/Tests/Pings/PingBucketMirrorTest.cpp` (class `FPingsBucketMirrorTest`, name `StealthGame.Pings.BucketMirror`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 gate order and results**
- Given: `FTeamPingClientGate::Evaluate(FTeamPingGateInputs)` driven table-wise.
- When: each input combination, in the order phase, presented role, bound pawn, concealed, point rule, bucket.
- Then: the first failing check decides; `RefusedConcealed` only if phase, role and pawn pass (concealed + no ground gives `RefusedConcealed`; concealed + wrong phase gives `RefusedSilent`); concealed is true for `Entering`, `Concealed`, `Exiting`, `Emerging`.
- Edge cases: an unknown or newly added bush state defaults to concealed (fail-closed).

**Test: AC2 presented role, not raw state**
- Given: raw `State` `Informant` but `PresentedRole` `Active` (a victim in the strike window); and the own fall start with `bOwnPawnBound` false.
- When: evaluate.
- Then: the first returns `Send`; the second returns `RefusedSilent` and nothing is sent.
- Edge cases: the bound pawn flips true again, then `Send` resumes.

**Test: AC3 bucket mirror**
- Given: the bucket mirror.
- When: send at tokens 0.99 and 1.0; advance time; `RefusedConcealed`; rebuild; call `GetBucketMirrorTokens()`.
- Then: strict (sends only at >= 1); advances on `EstimatedServerNow()` from its own send times; untouched by `RefusedConcealed`; reseeded to a full bucket on rebuild; the getter is const and read-only.
- Edge cases: the clock estimate steps backwards.

QL-STORY-READY: GAPS (revised) — AC3's reseed value was pinned as above on the qa-lead's proposal (the author should confirm "full bucket" at implementation).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Pings/PingClientGateTest.cpp` and `PingBucketMirrorTest.cpp` — must exist and pass (`/story-done` checks that they EXIST; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-004-ping-token-bucket-core.md`, `story-008-client-ping-point-rule.md`; cross-epic `production/epics/input-interaction-framework/story-071-context-swap-on-state-and-possession.md` (presented own role)
- Unlocks: `production/epics/informant-pings-team-comms/story-010-ia-ping-action-and-send-path.md`, `story-011-concealed-press-refusal.md`, `story-012-strike-window-drop.md`
