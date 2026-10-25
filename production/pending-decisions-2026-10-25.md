# Pending Decisions: Prioritised (2026-10-25)

Consolidated by the design-pipeline agent at the end of the unattended run. The full wording of each item is under "Pending user decisions" in `production/session-state/active.md`, and the rationale is in `production/session-logs/decision-log.md`.

**Fastest path forward:**
1. Choose version control (A1).
2. Approve the config batch (A4).
3. Answer B5 and C12–C13.

That unblocks the spikes (S1 net-emulation traversal, S2 50-character performance, S14) and ADR acceptance.

## A. Blockers

1. **Version control.** Nothing is in version control yet; the only history is the `_snapshots/` copies. Recommended: **Git + LFS**. ADR-0009 blocks the acceptance order: ADR-0009 → 0007/0008 → spikes → ADR-0001.
2. **Accept the 23 ADRs** (`docs/architecture/adr-0001` … `0023`). All are Proposed, and ADR-0001 also needs spikes S1, S2 and S14 to pass. The control manifest stays DRAFT until then.
3. **Performance on target hardware.** All numbers so far are from an RTX 5090. A capture on RTX 3060-class hardware is a condition for accepting ADR-0013 and ADR-0022.
4. **Config batch** (recommended: approve as one batch):
   - Pin Iris off.
   - Disable the Mover and NetworkPrediction plugins.
   - Set `NetClientTicksPerSecond` to about 2400. Without it the host updates only about 3 of 9 clients per frame.
   - Pin the `GameNetworkManager` movement values.
   - Lock the initial navmesh build, and remove the saved navmesh actors from layout maps.
   - Add a `ViewBlock` trace channel.
   - Rendering: `r.Substrate=0`, `r.RayTracing=0`, manual screen percentage, a 1000 MB texture streaming pool.
   - Enable the CommonUI plugin and its settings, plus the StateTree and GameplayStateTree plugins.
   - Optional: the listen-server tick clamp flag.

## B. Autonomous decisions to confirm or overrule

5. **Wrong-target tally.** The HUD shows "Wrong targets: n" instead of a strike counter, and the true count stays on the server, so an informant kill is never revealed. This changes the concept's wording. Recommended: confirm.
6. **Informant respawn.** A killed informant takes over an unseen crowd pawn near their team's spawn, instead of appearing at the spawn point.
7. **Rival progress signal.** On: the other team's finished preliminary stages (0–2), delayed and released on a shared 30 s schedule, never the final step.
8. **Camera pitch.** −60°/+40° in both camera modes. The default camera mode is revisited if first person reads more than 15% better in playtests.
9. **Strike reach on a fleeing runner.** Reduced from about 193 cm to 152 cm, so bystanders can't spot a victim freezing early.
10. **Concept-level items:**
    - build-order re-scope (instead of cuts)
    - pillar rewordings
    - kill confirmation (player or NPC)
    - informant pings plus team chat
    - "Held Breath" visual direction
    - working title "Smoking Mirror"
11. **Others:**
    - a balanced appearance deck replaces "3 duplicates per player" (already implemented in the prototype)
    - Lobby UI moves to Vertical Slice
    - Session rule 16 is rewritten for 5.8.4 seamless travel, which replaces the PlayerController and PlayerState

## C. Scope options (none applied)

12. **In-game informant ping in MVP.** The creative director recommends yes. Without it, playtests need external voice chat.
13. **Text-only end-of-round summary in MVP.** The creative director recommends yes.
14. **Smaller additions:**
    - a teammate-reconnecting indicator
    - name labels over the bodies when the round ends
    - a "walk slower" key
    - a sit/browse player action
    - the counter-blow facing an attacker from behind
    - a camera tap/hold option and a left-handed shoulder default
    - an option to turn off the strike hint
    - an audio bed while a killed informant waits to respawn
15. **Design-theory options:** NPCs use the brace pose too; cap how long players can run; restrict who can tend the final objective.
16. **Process and data:**
    - a headless server for evidence playtests
    - whether shipping tuning presets is allowed
    - Tuning Data as an ADR instead of a GDD
    - moving the hold-to-show-names key to Vertical Slice
    - Teammate Markers effort raised from S to M
    - derived kit assets before the art bible's production gate
    - NPC count raised from ~40 to ~50

## D. Schema and data changes (none applied)

17. **Timing fields:**
    - a carrier for the informant respawn `T_due`
    - `bDiedConcealed` and `StartServerTime`
    - `ContactServerTime`
    - `RetireAtServerTime`, `bPawnBound`, and a sequence number on the marker feed
18. **Rule and protocol additions:**
    - an `InformantKilled` reason value
    - the strike limit's hard minimum raised from 1 to 2
    - a "world-ready" step in round-start acknowledgement
    - touchable-prop records and pre-baked routes in the layout
    - `LayoutExactRefineMaxCount`
    - Crowd motion-profile and StateTree keys
    - a client input-device field
19. **Clean-ups:**
    - remove the per-objective hold-time override (it leaks timing)
    - retire a duplicate latency-warning key
    - six key renames

## E. Items the unattended run could not do

20. **Files outside the design agent's write scope:**
    - `project.yaml`: test command `RunTests ^StealthGame.`; add the proposed art budgets (≤1500 draw calls, 1 GB texture pool, 3 GB package).
    - `.claude/rules/test-standards.md`: an exemption for the engine's test class names.
    - `docs/engine-reference`: a dead input CVar name, and `DOREPLIFETIME` wrongly listed as deprecated.
    - (`CLAUDE.md` was already fixed during `/setup-engine`; no action needed.)
21. **Deletions (your call):**
    - the unused asset `/Game/StealthGame/Characters/BP_StealthCharacter`
    - stray helper files: `_hud_cd_fixes.py`, `_adr_tmp/`, the sub-agent brief, the registry staging folder
22. **Needs a person:**
    - measure the mannequin's stature and bone chains in 5.8.4
    - a sensitivity read on the Mexica-inspired visual direction before the vertical slice
23. **Later sessions:**
    - independent second reviews of the 18 GDDs
    - refresh the Data Flow and API sections of `architecture.md`
    - three low-priority ADR fixes
    - `/test-setup` and `/ux-design` (needed before `/gate-check pre-production`)
    - the prototype identity leak (`APawn::PlayerState` / `bConcealed` replicate to all; ADR-0001)
