# Story 074: AC 65 BraceProtectedOnlyOnConfirm

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: UI
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-007`, `TR-iif-012`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input
**Other ADRs**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: The HUD shows a player only what is theirs to know, timed by the server clock. View-models are plain `USTRUCT`s built by a headless C++ presenter that samples its carriers every frame by value, including the owner reply sink (ADR-0011 `ClientCommandReplies` entries consumed in order); widgets are dumb. ADR-0011 puts `FHoldStatus` on the owner's controller, flushed once at the end of the drain that produced it, and treats owner prediction as presentation only: the client never shows server-granted protection before the server confirms it.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0021 HIGH; ADR-0011 input layer HIGH)
**Engine Notes**: No post-cutoff API is introduced by the indicator itself. The N form relies on the ADR-0008 net-emulation profiles: PktLag 75 ms per direction (RTT about 150 ms) makes the pending window about 150-200 ms, roughly ten frames at 60 fps, which is why the capture must be frame-stepped (see QA gaps). `FHoldStatus` arrives in the owner-only `ClientCommandReplies` batch (ADR-0011 section 8); it is not a replicated property, so a RepNotify never drives the indicator (ADR-0021: no presentation depends on a RepNotify having fired).

**Control Manifest Rules (this layer)**:
- Required: Owner-only data (`FHoldStatus`, rejection codes, owner results, cooldown end) lives on the PlayerController carrier family — source: ADR-0011
- Required: A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step in one owner-only RPC per connection — source: ADR-0011
- Forbidden: Never deliver replies as replicated owner-only properties; never put owner-only data on the pawn — source: ADR-0011
- Required (Presentation Layer Rules): Presentation timing uses only `EstimatedServerNow()`; plate and card timelines are sampled from `now - t0` in C++; widgets are dumb; `FHudViewModel::Dump()` is deterministic — source: ADR-0021, ADR-0002

This story verifies that the brace indicator never claims protection the server has not granted. When the owner presses Q (Prepare), the indicator shows only a local "pending" outline emphasis; the Protected state (the inner stepped square plus the first ring segment) begins only when the server's `FHoldStatus` confirmation arrives. Predicting protection locally would give false safety: a Prepare can be rejected (cooldown, busy, an Attack with a lower key resolving first, story 058), and a player who believes it is protected when it is not would be misled by the UI. The AC's three screenshots cover the whole life of one brace under 75 ms per-direction latency: pending before confirmation, protected after, and cooling after the brace ends.

It depends on story 004 (the reply sink and the drain-end flush that delivers `FHoldStatus`). It relates to GDD UI Requirements item 2 (brace indicator), F4 (owner ring prediction and confirmation timeout; presentation only) and R12 (owner-only status), and to ADR-0021 section 4 and ADR-0011 sections 7 and 8. HUD widget code (the brace cluster in layer L1) belongs to the HUD epic; this story owns the IIF-side data (the `FHoldStatus` entry, the owner-local prediction input and the confirmation arrival) and the evidence.

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 65 (UI 2, UI, N `BraceProtectedOnlyOnConfirm`): **GIVEN** PktLag 75 ms per direction **WHEN** Q is pressed **THEN** the brace indicator shows only the pending outline until `FHoldStatus` arrives, and Protected (square plus first segment) only after. Screenshots: `production/qa/evidence/iif-brace-{pending,protected,cooling}.png`.

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 7, 8 and 9 (owner prediction), ADR-0021 Decision section 4, and GDD UI Requirements item 2:*

- `FHoldStatus {ClientCommandSeq, ActionType, Outcome in {Active, Completed, Cancelled}, ServerStartTime, ServerEndTime}` (no cancel reason) lives on the owner's `AStealthPlayerController` and arrives in the batched `ClientCommandReplies` RPC, flushed once at the end of the resolution step that produced it. It is sent on execution and again on every server-side end; a status whose `ClientCommandSeq` is older than the owner's current hold is ignored. Reply structs live in `StealthFoundation/Public/View`.
- Owner prediction is presentation only (ADR-0011 section 9): "the ring (F4) and prompt legality from owner-known data"; the client does not send `PrepareBegin` while its known cooldown runs. Prepare uses no predicted ring branch (story 079). The indicator's pending state is the local emphasis from the press; Protected starts on confirmation only; the brace's end (release, cap or cancel, identical; on a reversal at `FHoldStatus.ServerEndTime = ContactServerTime`, never on receipt, ASN-R23) removes the square with a 250 ms stepped exit, then Cooling shows the linear Bone Dim sweep and the seconds numeral counting the 60 s from acceptance (`DefenceCooldownSec`, owned by Assassination). There is no "Ready" sound.
- Indicator geometry per the GDD: 56 px, bottom-left, Bone, no signal hue; Ready is an empty outline; in Protected the inner stepped square appears at once and the outer ring fills in 4 stepped segments of `DefenceDurationSec` / 4 each (defaults 4.0 s, so 1.0 s each); it never shrinks, closes or flashes (no parry-window reading). Exact layout is the HUD UX spec's (`design/ux/hud.md`).
- Gap fixes from the qa-lead (author to confirm; the GDD AC stays the authority): the AC names `iif-brace-cooling.png` but has no THEN clause asserting the cooling state, so the manual check adds "after the brace ends the indicator shows Cooling until the cooldown ends"; the pending window at 75 ms per direction is only about 150-200 ms, so capture frame-stepped (or raise lag for the capture run only); the loss setting is not stated in the AC, so run the screenshot run at 0 % loss.
- Tuning keys: `DefenceDurationSec` 4.0 (range 3-5) and `DefenceCooldownSec` 60 are owned by Assassination; `AckSlackSec` 0.15 (IIF, audience All) governs when an unconfirmed ring hides (F4).
- Prototype disposition: the prototype's `AStealthHUD` drawing code and the defence state in `ServerSetDefence` (client-chosen hold state) are replaced, not ported; new code with no prototype counterpart. Content step: widget layout belongs to the HUD epic; before any Content step read `production/never-resave-assets.md`.
- Test seams: net emulation at PktLag 75 ms per direction with 0 % loss for the capture run, a frame-stepped capture of the owner client, and log timestamps showing `FHoldStatus` arrival between the pending and protected frames (the outgoing-RPC/incoming reply log from story 095).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-078-ring-monotonic-clamp.md`: the F4 ring clamp and timeout arithmetic (Logic).
- `production/epics/input-interaction-framework/story-079-ring-resync-and-late-confirm.md`: ring resync and late confirmation (Logic).
- `production/epics/input-interaction-framework/story-077-latency-icon.md`: the latency warning icon.
- `production/epics/input-interaction-framework/story-080-latency-budget.md`: the measured `FHoldStatus` arrival budget (AC 36).
- `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`: the reply sink and flush itself.
- The brace cluster widget and presenter: hud epic (stories not yet written).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/View/` (`FHoldStatus` and the owner-side data the presenter samples; IIF-owned data side)
- `Source/StealthGame/Public/StealthPlayerController.h` and `Source/StealthGame/Private/StealthPlayerController.cpp` (owner reply carrier consumed in order; shared with story 004)
- `production/qa/evidence/iif-brace-pending.png`
- `production/qa/evidence/iif-brace-protected.png`
- `production/qa/evidence/iif-brace-cooling.png`
- HUD widget code: hud epic, not this story

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- Gap 1: screenshot `iif-brace-cooling.png` is named but no THEN clause asserts the cooling state. Add: "AND after the brace ends the indicator shows Cooling until the cooldown ends".
- Gap 2: the pending window at 75 ms per direction is about 150-200 ms (roughly 10 frames at 60 fps); state the capture method (frame-stepped capture, or a raised lag only for the capture run).
- Gap 3: the loss setting is not stated (the profile default 1 % applies unless the AC says otherwise); state 0 % for the screenshot run.

QA-lead manual check sketch (unconfirmed): brace indicator pending, protected, cooling. Setup: N profile PktLag 75 ms per direction, 0 % loss; owner client; indicator visuals per the HUD UX spec (pending outline; Protected = square plus first segment). Verify: press Q; capture frame-stepped before `FHoldStatus` arrives, then after, then after End during cooldown. Pass condition: frame 1 shows the pending outline only and no Protected square; frame 2 (after `FHoldStatus`) shows Protected; frame 3 shows Cooling; three screenshots `production/qa/evidence/iif-brace-{pending,protected,cooling}.png` retained; log timestamps show `FHoldStatus` arrival between frames 1 and 2.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: a retained screenshot of each screen touched, in `production/qa/evidence/`: `production/qa/evidence/iif-brace-pending.png`, `production/qa/evidence/iif-brace-protected.png`, `production/qa/evidence/iif-brace-cooling.png`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`; cross-epic: hud (stories not yet written; the brace cluster widget and presenter), cross-epic: assassination-defence-reversal (stories not yet written; Prepare predicate, cooldown, `DefenceDurationSec`)
- Unlocks: None
