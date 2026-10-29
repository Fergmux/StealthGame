# Evidence: Camera System stories 007 and 024, 2026-10-29

Stories: `production/epics/camera-system/story-007-viewblock-channel-collision-profiles.md`,
`production/epics/camera-system/story-024-presentation-include-check-tell-matrix.md`.

## Build and gate

`StealthGameEditor.Build.cs` gained the `DataValidation` module (engine plugin, enabled by default): project files regenerated
(`Build.bat -projectfiles ... -game -rocket -progress` -> `Result: Succeeded`), editor build `Result: Succeeded`. Full
`StealthGame` suite: **597 tests, 597 passed** (`full-suite.txt`). `Config/DefaultEngine.ini` diff: the Pawn and CharacterMesh
profiles gain `(Channel="Camera",Response=ECR_Ignore)`; five new profiles StealthKit, StealthProp, StealthFoliage, StealthGlass,
StealthDecoration; no other line, no SecurityToken line. No Content asset touched (the sample camera assets and PC_Sandbox are
byte-identical, `git status` clean under `Content/`).

## What was built

| Story | Built | Tests |
|---|---|---|
| 007 | `StealthCollision::ViewBlock` (= ECC_GameTraceChannel2) and the profile expectation table (`StealthCollisionChannels.h`); the ini profiles; `UStealthCollisionProfileValidator` (UEditorValidatorBase, static meshes and actor Blueprints, reads only) with the Custom-preset rule | Camera.CollisionProfiles (AC1, AC2), Camera.CollisionProfileValidator (AC3) |
| 024 | the camera include / identity scan with its negative control; the Tell Matrix review | Camera.PresentationIncludeCheck; `production/qa/evidence/camera-tell-matrix-review.md` |

Found in testing and fixed: an override of a body's response turns its preset into `Custom` (the editor does the same), so the
first validator draft never saw a drift; the rule for Custom is now a Pawn-type body is held to the Pawn row and any other body
that blocks ViewBlock must block Camera (watched failing first: 3 assertions red, then green).

## Run-and-observe (3-player PIE, L_Prototype)

`StealthBot 1 watch2 120` (client 1's pawn stands 1.2 m from client 2's and faces it); captured all three windows, looked at,
none black: `cam-007-client1-pawn-near-3p.png`, `cam-007-client2-3p.png`, `cam-007-server-3p.png`. With pawns now ignoring the
Camera channel the sample 3P rig frames normally with another pawn beside the lens: no boom pull-in on the nearby pawn, no clip
into the own body. The 024 scan and review have nothing on screen (N/A).

## Code review (self)

Verdict: APPROVED WITH NOTES. One channel constant, one table read by both the ini guard and the validator; the validator never
saves; it is editor-only (StealthGameEditor). Notes: no kit / foliage asset uses the new profiles yet (Level Socket Contract and
Art epics assign them); the validator is proven on transient objects, not checked-in fixture assets (no Content written);
Crowd's virtual look uses its own envelope stand-in (advisory finding in the Tell Matrix review).
