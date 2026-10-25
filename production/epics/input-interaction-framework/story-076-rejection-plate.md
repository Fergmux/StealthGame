# Story 076: AC 67 RejectionPlate

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: UI
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input
**Other ADRs**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: ADR-0021 fixes the HUD as UMG overlay layers at the HUD-R4 ZOrders, all hit-test-invisible, fed by plain-USTRUCT view-models that a headless C++ presenter builds from the owner's own carriers (the owner reply sink included); Presentation never reads a pawn's controller or PlayerState. ADR-0011 section 8 delivers every owner reply, rejections included, as entries of one batched reliable owner-only `ClientCommandReplies` RPC per connection per drain.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The plate uses no post-cutoff API of its own. It sits in ADR-0021's L1 persistent layer (`UStealthHudPersistentLayer`, added with `AddToPlayerScreen` at the HUD-R4 ZOrder, engine fact E1) and inherits ADR-0021 verification item UI-V3 (layer order). The reply it reacts to is an `ECommandRejectCode` entry of `FCommandReply` (ADR-0011 Key Interfaces); the ADR-0011 V-items (V1-V6) do not gate this story.

**Control Manifest Rules (this layer)**:
- Required: A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step in one owner-only RPC per connection; `Rejected_Latched` replies `NotNow` through the held-reply buffer — source: ADR-0011
- Required: Owner-only data (`FHoldStatus`, rejection codes, owner results, cooldown end) lives on the PlayerController carrier family — source: ADR-0011
- Forbidden: Never let a reply code depend on a target pawn's identity, team, concealment or brace state; never deliver replies as replicated owner-only properties — source: ADR-0011
- Forbidden: Never put owner-only data on the pawn — source: ADR-0011
- Guardrail: Command interface: at most one reply RPC per connection per drain — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 67 (UI 5, UI, P `RejectionPlate`): **GIVEN** a `NotNow` rejection repeated 3 times within 3 s, then a `Cooldown` rejection **WHEN** plates are observed **THEN** the "Not now" plate shows once; **AND** no plate shows for `Cooldown`. Screenshot: `production/qa/evidence/iif-reject-plate.png`.

---

## Implementation Notes

*Derived from ADR-0021 (Decision sections 1, 4, 9) and ADR-0011 (Decision section 8, Implementation Guidelines):*

**What this story builds.** The owner-only rejection plate of GDD UI Requirements item 5 (HUD element E15 in `design/ux/hud.md`): when a `ClientCommandReplies` batch carries a resolution-tier rejection (`NotNow`, `Busy`, `OutOfReach`), the presenter empties the interact ring and shows an opaque Obsidian plate with 20 px Bone text ("Not now", "Busy" or "Too far"), 400 ms in, 1.5 s hold, 600 ms out, at most once per `RejectPlateCooldownSec` (3.0 s) **per code**. A Prepare `Cooldown` rejection gets no plate, because the Cooling numeral of the brace indicator already says it; a `HideEnter` `Cooldown` is presented per the HUD spec (`design/ux/hud.md`), not here. The plate sits beside the brace indicator or in place of the prompt verb (anchors per HUD). Ingress-tier drops produce no reply and therefore no plate (GDD R6, ADR-0011 section 8: "Ingress tier: silent").

**Where it sits.** Rejection replies are produced by stories 004 (drain-end batching) and 029 (the code set). This story is the presentation end of that chain: it reads the owner reply sink only (ADR-0021 section 9: Presentation reads its own controller's owner data and the owner reply sink, never a pawn's PlayerState, Controller or Owner, and never includes a `Public/Server/` header). Reply codes are computed only from facts the owner already knows (GDD R6), so the plate adds no information about any other pawn. The "Not now" wording is the same for every cause of `NotNow` (stages 2a, 2b, 2e, `Rejected_Latched`), so the plate cannot reveal why.

**ADR rationale.** The per-code rate limit is a client-local presentation rule (a knob read from the client tuning snapshot, Audience `All`, Latch `Use`); it never feeds back to the server and adds no RPC. The plate lives in the HUD's persistent layer under ADR-0021's fixed layer order; it is hit-test-invisible and never activatable. Plates for a pawn struck in the same drain are presented no earlier than that pawn's `ContactServerTime` (ASN-R23, ADR-0018 held-reply rule): that timing belongs to the HUD and Assassination epics and is not part of this AC.

**Test seam.** The AC says the plate "shows once", which one screenshot cannot prove (qa-lead). The story must expose a plate-show counter (a development-only log line or presenter counter) so the three `NotNow` replies in 3 s can be seen to produce one show. The quiet owner-only 2D tone of UI Requirements item 5 is not asserted by this AC.

**Prototype.** New code, no prototype counterpart (the prototype has no rejection feedback).

**Tuning keys (defaults).** `RejectPlateCooldownSec` 3.0 (hard 1-10, safe 2-5; plate rate limit per code). The plate timings (400 ms in, 1.5 s hold, 600 ms out) are HUD art/UX constants.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`: the batched `ClientCommandReplies` flush that delivers the reply.
- `production/epics/input-interaction-framework/story-029-rejection-code-set.md`: which failure maps to which code.
- `production/epics/input-interaction-framework/story-072-informant-plate.md`: the informant plate ("Informants can only observe"), a separate rate limit and plate.
- `production/epics/input-interaction-framework/story-074-brace-protected-only-on-confirm.md`: the brace indicator states (pending, protected, cooling).
- `production/epics/input-interaction-framework/story-077-latency-icon.md`: the latency icon.

---

## Files

- `Source/StealthPresentation/Private/` — rejection-plate view-model and L1 widget wiring (exact file names per the HUD epic's layout; planned)
- `Source/StealthFoundation/Public/View/` — reads `FCommandReply` / `ECommandRejectCode` only (declared by story 001; no change expected)
- `production/qa/evidence/iif-reject-plate.png` — retained screenshot of the "Not now" plate

---

## QA Test Cases

**Manual verification steps (UI):**

- **AC-1**: GIVEN a `NotNow` rejection repeated 3 times within 3 s, then a `Cooldown` rejection, the "Not now" plate shows once and no plate shows for `Cooldown`.
  - Setup: PIE owner able to trigger `NotNow` on demand (for example Prepare while concealed) and a `Cooldown` rejection (Prepare during cooling); a plate-show counter available in the log.
  - Verify: trigger `NotNow` 3 times within 3 s and capture the plate on the first; then trigger one `Cooldown` rejection and watch the HUD for 1 s.
  - Pass condition: "Not now" plate counter = 1; `Cooldown` plate counter = 0; screenshot `production/qa/evidence/iif-reject-plate.png` retained showing the "Not now" plate.

*QL-STORY-READY: ADEQUATE (qa-lead). Interpretation: "shows once" needs a counter; a single screenshot cannot prove it.*

---

## Test Evidence

**Story Type**: UI
**Required evidence**:
- UI: a retained screenshot of each screen touched, in `production/qa/evidence/` — `production/qa/evidence/iif-reject-plate.png` (plus the plate-show counter output in the run log)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`; cross-epic: hud (stories not yet written; L1 persistent layer and plate widgets, `design/ux/hud.md` E15); cross-epic: tuning-data-config (stories not yet written; `RejectPlateCooldownSec` knob and client tuning snapshot)
- Unlocks: None
