# Assassination PIE flakes in the full gate: root causes and fixes (2026-10-28)

Seen in the NKP 016-022 and OC 001-012 gates (`../npc-kill-penalty-informants-016-022-2026-10-27/evidence.md`,
`../objectives-chain-001-009-2026-10-28/evidence.md`, `../objectives-chain-010-012-2026-10-28/evidence.md`): each test passed alone,
then failed in some full-suite runs:

| Test | Symptom in the full gate |
|---|---|
| `Assassination.PIE.KillAtArmsLength` | "machine N: Death_Fall starts at ContactServerTime (+- one frame)": the fall 45-136 ms after contact (several runs) |
| `Assassination.PIE.StrikeClipParity` | "the five strikes share one variant, horizontal WarpPoint and contact time": four pawn targets at WarpPoint (1441,-48) against the empty spot's (1500,0) |
| `Assassination.PIE.KillPlateTellsOneThing` (found in this batch's first gate, section 4) | "four S results, P2's result and S's Counter were seen": S unbraced at the reversal press |
| `Input.PIE.ContextSwapOnStateAndPossession` (found in this batch's second gate, section 5) | "the owner's C-TeamView reported Informant": the bot's strike was NoContact |
| `OneBodySequence`, `NoObserverCue`, `PossessionThroughFall` | the engine ensure `TMatrix<T>::InverseFast(), trying to invert a NIL matrix` raised from `UnrealEditor-MotionWarping.dll` (once per process; whichever test runs fails) |

## 1. MotionWarping NIL-matrix ensure (product fix)

**Root cause.** `URootMotionModifier_SkewWarp::WarpTranslation` (UE 5.8.4, `RootMotionModifier_SkewWarp.cpp`) builds its working
space with `FRotationMatrix::MakeFromXZ(ToRootNormalized, ...)`, where `ToRootNormalized` is the normalised root translation still to
come in the window (`PreviousPosition -> EndTime`). When that remaining translation is (near) zero while the step's own root
translation is not (the engine's guards test the step and the whole window, never the remainder), the direction is the zero
vector, the matrix is all zeros and its three `InverseTransformVector` calls raise the ensure at Matrix.h line 468 and return NaN
(16 log lines per occurrence, as in the gate logs). A strike montage position that is moved back from the window's end (previous =
end, current earlier: a seek of a lunge that ran ahead of its server timeline, or a replayed move) is exactly that input.

**Watched failing** (`watch-fail-engine-skew.txt`): a temporary test fed the engine skew modifier the D54 strike montage
(`AM_ASN_Strike_D54Placeholder`, window [0, 0.9], previous 0.9, current 0.45) and raised the same ensure (same file and line); the
guarded modifier on the same state passes the step through (`StealthGame.Assassination.StrikeWarpGuard`, kept). The exact in-PIE
trigger was not caught in two diagnostic runs of all 27 Assassination PIE tests with the guard logging (`diagnostic-group-runs.txt`:
0 guard lines, 0 ensure lines), so the fix closes the whole class of degenerate inputs rather than one call site. The final gate's
run 5 then caught the in-PIE trigger (`full-suite-runs.md`): a forward step at 0.8627 -> 0.8815 of the [0, 0.9] window whose
remaining translation is 0 (the clip's root motion is spent before the window ends), logged by the guard on the striker's
autonomous proxy and the server and passed through, with no ensure.

**Fix** (`Source/StealthGameplay/Features/Assassination/Public/View/StealthStrikeSkewWarp.h`, `Private/StealthStrikeSkewWarp.cpp`;
`EncounterPresentation.cpp` adds it instead of the engine class): `UStealthStrikeSkewWarp` is the engine skew warp with one guard,
`FStrikeWarpGuard::IsDegenerate` (the engine's own conditions for reaching the skew, plus a remaining translation below
GetSafeNormal's tolerance): that step's root motion passes through unwarped, which is what the window's end does anyway, and the
modifier logs it once (`[StrikeWarp] ... degenerate skew step passed through unwarped`, with the positions, role and net mode). The
D54 placeholder import scale is not involved (the clip's window translation is 62.9 cm forward, scale 1).

## 2. KillAtArmsLength: the frame bound measured the wrong frame (test fix)

**Root cause.** "+- one frame" was `max(1/60, DeltaSeconds) + 2 ms` with `DeltaSeconds` read in the assert step, 1.5 s after contact.
In the full gate a load hitch on the contact frame (logged: the server's fall at 8.303 for contact 8.250, every presentation row of
that host frame at 8.303) put the fall one long frame after contact, which the bound read as late.

**Fix** (`Source/StealthTestHarness/Private/Tests/PIE/Assassination/KillAtArmsLengthPieTest.cpp`): every machine's estimated server
time is sampled once per engine frame (latent commands run after `GEngine->Tick`, sharing `GFrameCounter` with the presentation
rows); the fall must start on the first frame of that machine at or after contact (its row at or after contact, the machine's
previous sampled frame before it), however long that frame was. A second method, `RivalAt230cmFallsOnTheFirstFrameAtContactUnderAHitch`,
injects a 150 ms stall in the last frame before contact: the old bound FAILS on all three machines there and the new check passes
(`killatarmslength-filtered.txt`: frame steps 155 / 156 ms, the old bound reported as FAILS) - the deterministic watch-fail.

## 3. StrikeClipParity: the staging spot was layout-dependent (test fix)

**Root cause.** The anchor came from `MoveClearOfBushes` only; the spawn layout is drawn from OS entropy, and when the target spot
150 cm ahead encroached on an adobe piece the teleport's encroachment check pushed every pawn target to the same nearby spot
(144 cm at -1.9 degrees: WarpPoint (1441,-48)) while the empty-spot strike aimed at the exact point (1500,0). The pattern is the
KillAtArmsLength fix 32cb760.

**Fix** (`StrikeClipParityPieTest.cpp`): S's stand and the target spot sit on a clear, flat disc with nothing standing on it, away
from every bush (`StealthPIEOpenCircle::FindOpenCircle`, radius 75 + 60 cm margin); after the fix all five strikes share
WarpPoint (1500,0) (`strikeclipparity-filtered.txt`).

## 4. KillPlateTellsOneThing: the blind side-step before the brace (test fix, found in this batch's gate)

**Seen.** The first 5-run gate of this batch failed run 2 on `Assassination.PIE.KillPlateTellsOneThing` ("four S results, P2's
result and S's Counter were seen"): seed 1637189918 (12 adobe pieces, 6 garden spots), the reversal press logged S pose 0 (None,
not braced) and P2's strike was NoContact, so S never countered and steps 4-5 cascaded. Run 1 of the same binaries passed (S pose
2 = Brace at the press).

**Root cause.** Step 3a moved S 500 cm sideways blind (`TeleportTo(S - Right * 500)`) and pressed PrepareBegin on the same frame.
Where that spot lands depends on the layout (OS-entropy seed): the resolver voids PrepareBegin as NotNow while S is concealed in
a bush (`ResolvePrepareBegin`, CON-R7) or traversing, and an unbraced, out-of-place S cannot produce the reversal.

**Watched failing** (`killplate-watch-fail.txt`, temporary methods deleted after the run): the old step 3a with S landing in a
bush leaves S at pose 0 (concealed 1) 0.4 s after the press - FAIL; the new 3a/3b on the same landing moves S to a clear disc and
braces (pose 2) - pass.

**Fix** (`Source/StealthTestHarness/Private/Tests/PIE/Assassination/KillPlateTellsOneThingPieTest.cpp`): both of S's stands (steps
1-2 and 3-5) are `StealthPIEOpenCircle::FindOpenCircle` discs (radius 160 + 60 cm margin, > 900 cm from every bush, the second
>= 450 cm from the NPC's body); S settles 0.6 s, then braces, and P2 is placed only once S's pose is Brace. Filtered after the
fix: 3 of 3 pass on fresh layouts (`killplate-filtered.txt`).

## 5. Input.PIE.ContextSwapOnStateAndPossession: the bot's approach at its own height (test fix, found in this batch's gate)

**Seen.** The second gate attempt (after section 4) passed runs 1-3 (517/517 each) and failed run 4 on
`Input.PIE.ContextSwapOnStateAndPossession` ("the owner's C-TeamView reported Informant"): seed 1699684516, 50 NPCs, the host's
development bot (`strike_enemy`) struck 0.75 s after Active and the plate was NoContact, so the client never became an Informant.

**Root cause.** `strike_enemy` teleports the bot 120 cm from the target at the bot's **own** Z (and through whatever stands at
that spot); reach is pure geometry at the slot (range, vertical, warp, bearing; `FStrikeReachCore::Evaluate`). Where the two
team spawns sit on different heights, or a crowd NPC or an adobe piece is at the approach spot, the strike executes out of reach.

**Watched failing** (`contextswap-watch-fail.txt`, temporary method deleted after the run): with the host 80 cm above the client's
floor, the old step (2) reproduces the gate failure exactly - a NoContact plate and no Informant view.

**Fix** (`Source/StealthTestHarness/Private/Tests/Input/ContextSwapOnStateAndPossessionPieTest.cpp`): the round is crowd-less
(NpcCount 0; the test is about input contexts) and, before any input, both pawns stand on one clear, flat disc (radius 90 + 60 cm,
> 900 cm from every bush) 120 cm apart, the host facing the client, so the approach spot is the host's own stand. Filtered after the
fix: 3 of 3 pass on fresh layouts (`contextswap-filtered.txt`).

## Consecutive full-suite runs (zero flakes)

See `full-suite-runs.md`: five consecutive full-suite runs of the final binaries, after the section 4 fix (the first gate attempt's run 1 passed 517/517 and its run 2 found section 4; the second attempt's runs 1-3 passed and its run 4 found section 5; the count restarted each time).
