# Camera Tell Matrix review (story cs-024 AC2, AC-CS-23; ADVISORY, doc and code review)

Date: 2026-10-29 · Reviewer: self (unattended build batch; no independent review) · Story:
`production/epics/camera-system/story-024-presentation-include-check-tell-matrix.md`

## 1. The Baseline and Tell Matrix row (design review of Crowd)

`design/gdd/crowd-npc-behaviour.md`, Tell Matrix row **T17**:

> | T17 | Camera mode / camera toggle (1P or 3P, shoulder swap) | none: the camera is client-local and changes nothing on the pawn (Camera CS-R2) | n/a: not an advantage and not visible | base rate | Camera |

Finding: **PRESENT.** The row reads "no visible deviation" in substance ("none: the camera is client-local and changes nothing
on the pawn") and points to Camera as owner; the GDD's change log records it (line 526, "Camera CR 5 ... APPLIED: T17").

## 2. Crowd's virtual-look call site (code review)

`Source/StealthGameplay/Features/Crowd/Private/CrowdVirtualInput.cpp`: the virtual look calls
`StealthCrowdView::EvaluateViewEnvelope(Desired, Locomotion)` (line 93). That function is **Crowd's own stand-in**: it
normalises yaw and clamps pitch to SCL's `LookPitchLimitDeg` (+-20). It is not Camera's F1 envelope
(`StealthViewEnvelope::Step`, `Source/StealthGameplay/Public/View/Core/StealthViewEnvelope.h`, story cs-002), which clamps
the view yaw to +-70 relative to the body with the soft edge and the latched push, and the view pitch to -60 / +40.

Finding: **DEVIATION (advisory).** The name matches the GDD's ("Crowd's virtual look calls `EvaluateViewEnvelope`"), but the
envelope the crowd's virtual camera obeys is not the human one: an NPC's virtual camera is not yaw-limited relative to its
body and its pitch range is the replicated head clamp, not the local view range. The pawn-side output is the same either
way (control rotation feeds SCL's head look and the server clamps the head), so no tell is visible today; the gap is that
a future change to F1 (for example the edge push timing that drives SCL's turn-in-place) would not reach NPCs.

Recommendation (for the Crowd epic owner; not changed here, out of Camera's scope): make
`StealthCrowdView::EvaluateViewEnvelope` step `StealthViewEnvelope::Step` with `FViewEnvelopeParams::FromCamera` (the
function is pure, has no UObject or clock, and was written to be callable from the server, ADR-0016), keeping a state per
agent. Tracked as an open item in the batch report.

## Sign-off

- [ ] game-designer (advisory review) — pending (human)
