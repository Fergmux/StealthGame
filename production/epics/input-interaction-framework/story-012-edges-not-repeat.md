# Story 012: AC 4 EdgesNotRepeat

> **Epic**: Input & Interaction Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Held commands map `ETriggerEvent::Started` to Begin and both `Completed` and `Canceled` to End; instant commands map `Started`; server-meaningful actions are Boolean with only the default trigger, and no command ever auto-repeats while a key is held.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Input layer HIGH (5.8 Enhanced Input / Common UI unification). Engine fact E1: `ETriggerEvent` has `Started` (None to Ongoing/Triggered), `Triggered`, `Ongoing`, `Canceled` and `Completed`; with the default trigger a Boolean action fires `Triggered` every frame while held, which is exactly why the builder must bind `Started`, not `Triggered`. Verification item V1 (story 008) covers the edge behaviour on context changes; this story's sampling test injects synthetic events and does not depend on V1.

**Control Manifest Rules (this layer)**:
- Required: Gameplay input is bound on `AStealthPlayerController`; held commands map `Started` to Begin and both `Completed` and `Canceled` to End; server-meaningful actions are Boolean — source: ADR-0011
- Required: Ends are idempotent per Begin — source: ADR-0011
- Forbidden: Never use Hold/HoldAndRelease/Tap/Pulse/Chorded triggers for server-meaningful actions — source: ADR-0011
- Forbidden: Never debounce releases client-side — source: ADR-0011
- Guardrail: Command interface uplink at most 20 reliable RPCs per second per connection; honest play about 1-2 per second, so a held key must never generate per-frame traffic — source: ADR-0011

This story builds the client-side edge handling for held and instant commands: one Begin on the press edge, nothing while the key is held (including OS key-repeat events), one End on release for held kinds and nothing on release for instant kinds. It is the first behavioural use of the input assets from story 007 and is the foundation of the flush story (013). The release debounce is deliberately not here: "never debounce releases client-side" (Alternative 4 rejected); a release and re-press inside 0.1 s sends End then Begin, and the server merges them (story 044).

The qa-lead flags a systemic gap: the test needs the `AStealthPlayerController` input setup, a `UPawnCommandClient` shell and the `ServerPawnCommand` transport; no story in 001-031 other than 094 creates them. Per the plan's additions, story 012 now depends on story 094 (controller transport skeleton) for those, and uses a recording transport (not the real RPC) in the test.

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 4 (R4, Logic, U `EdgesNotRepeat`): **GIVEN** a key held 2 s **WHEN** input is sampled **THEN** exactly one Begin is sent and nothing repeats.

---

## Implementation Notes

*Derived from ADR-0011 Decision §9 ("Edges only (IIF R4)"), §2 and Implementation Guidelines:*

"Held commands bind `ETriggerEvent::Started` to `Begin` and **both** `Completed` and `Canceled` to `End` (E1); instant commands bind `Started`. Actions with server meaning are **Boolean** input actions (E12) with only the default (Down/Pressed) trigger ... `IA_Move` and `IA_Look` are axis actions and carry no edges. No command auto-repeats." The reason: the server owns every hold timer and the transport is one reliable RPC per discrete edge (at most 20 per second per connection), so repeating Begin would burn the transport bucket and change meaning. GDD R4: "After any cancellation, a held key needs a fresh press."

**Test design (QL-STORY-READY, to pin in the implementation):** the AC's "a key" is unspecified, the release behaviour (one End) is not stated, and the sampling layer is open. Rewrite proposed by the qa-lead: "For each of Interact, Prepare (held) and Attack, Hide, Traverse (instant): with the key held 2.0 s at 30, 60 and 144 Hz sampling, including OS key-repeat events, exactly one Begin (or one instant command) is sent, nothing repeats during the hold, and release sends exactly one End for the held kinds and nothing for the instant kinds." Setup: `UPawnCommandClient` on a stub controller with a recording transport and the IA assets of story 007; inject `Started`, then `Triggered` every sample, then `Completed`, with `IsRepeat` key-down events interleaved. Edge cases: release and re-press inside 0.1 s sends End then Begin (debounce is server-side); a key held across a context swap is story 013; two different keys held together send two independent Begins.

The `UPawnCommandClient` shell and the transport arrive via story 094. GDD rules: R4 (line 68), R16 (hold-only in MVP, no toggle). Prototype replaced: `StealthCharacter.cpp` `OnInteractPressed/Released`, `OnDefencePressed/Released`, `OnAssassinatePressed` (they sent held state from the client; this replaces them with edges). Tuning keys: none client-side (`ReleaseDebounceSec` is server-only, story 044).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-013-flush-sends-end.md`: End on context swap, menu, focus loss and unpossess (AC 5).
- `story-044-debounce-merge.md`: the server-side release debounce and merge (AC 19).
- `story-007-input-assets-and-trigger-lint.md`: the assets and trigger lint this story uses.
- `story-094-controller-transport-skeleton.md`: `UPawnCommandClient` and the `ServerPawnCommand` RPC.
- `story-016-no-toggle-setting-mvp.md`: no hold/toggle setting in MVP.

---

## Files

*The code and test files this story changed (written by /dev-story; /story-done checks scope against it).*

- `Source/StealthGameplay/Public/View/Input/PawnCommandClient.h`
- `Source/StealthGameplay/Private/View/Input/PawnCommandClient.cpp`
- `Source/StealthGameplay/Private/Tests/Input/EdgesNotRepeatTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- "A key" is unspecified, the release behaviour (one End) is not stated, and the sampling layer is open. No story in 001-031 creates AStealthPlayerController input setup, UPawnCommandClient or the ServerPawnCommand RPC that this test needs, so 012 implicitly owns them (the plan now adds a dependency on story 094 for them).
- QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "For each of Interact, Prepare (held) and Attack, Hide, Traverse (instant): with the key held 2.0 s at 30, 60 and 144 Hz sampling, including OS key-repeat events, exactly one Begin (or one instant command) is sent, nothing repeats during the hold, and release sends exactly one End for the held kinds and nothing for the instant kinds."
- Scenario sketch from the qa file (`EdgesNotRepeat`, AC 4): UPawnCommandClient on a stub controller with a recording transport; IA assets from story 007; key held 2.0 s (inject Started, then Triggered every sample, then Completed) at each rate with IsRepeat key-down events interleaved; transport records 1 Begin and 0 further commands during the hold; at release 1 End (held kinds) or none (instant). Edge cases: release and re-press inside 0.1 s sends End then Begin; two different keys held together send two independent Begins.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/EdgesNotRepeatTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [x] Created and passing (`StealthGame.Input.EdgesNotRepeat`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-007-input-assets-and-trigger-lint.md`, `production/epics/input-interaction-framework/story-094-controller-transport-skeleton.md`
- Unlocks: `production/epics/input-interaction-framework/story-013-flush-sends-end.md`

---

## Completion Notes
**Completed**: 2026-10-26 (IIF batch 13: command/reply types, core, hub, reply step, transport, seams, spikes V2/V3/V5, input ACs; D53 integration of the production input path)
**Criteria**: 1/1 passing (held actions bind exactly Started/Completed/Canceled and instant ones exactly Started on a real UEnhancedInputComponent; a key held 2 s at 30, 60 and 144 Hz with OS repeats sends one Begin and one End (held) or one command (instant); Seq strictly increasing; a terminal edge after an End is swallowed)
**Deviations**: None beyond the pinned qa-lead rewrite (kinds, rates, release behaviour).
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Input/EdgesNotRepeatTest.cpp`
Run result: N/A — edge counting on the command stream; observed in play through story iif-094 (one seq per press)
**Reviewed by**: lead-programmer focused review (sonnet; 3 medium + 6 low findings, all fixed except the documented limits) + self-review, IIF batch 13
