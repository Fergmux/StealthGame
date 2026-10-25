# Social Stealth Game — First Playable Build

## Purpose

Build a first playable prototype in Unreal Engine, using Blender for asset preparation. The game is an online, hardcore socialas stealth game inspired by Assassin's Creed Brotherhood multiplayer, with an Aztec-inspired tribal setting.

This document separates confirmed game rules from suggested prototype choices. Unchecked tasks describe work to complete; they do not indicate that any systems or assets already exist.

## Required tools and animation foundation

- [ ] Use **Meshy MCP** to create the initial character and environment assets. The spoken reference to “Meshy NPC” is interpreted as Meshy MCP.
- [ ] Use **Blender MCP** to prepare assets for Unreal: mesh cleanup, scale, pivots, materials, collision preparation, rigging or retarget preparation, and modular building assembly as needed.
- [ ] Use **Unreal MCP** to integrate the assets and build the playable game systems, maps, animation setup, and tests supported by the connection.
- [ ] Use Epic Games' **Game Animation Sample**, referred to as **GAS** in this document, as the foundation for motion matching, locomotion, and basic parkour/traversal. This is the confirmed sample; do not substitute the Animation Starter Pack. GAS here does not mean Gameplay Ability System.
- [ ] Reuse the sample's included animations, character setup, and traversal examples where suitable. Inspect preloaded project assets before generating replacements.
- [ ] Verify the installed Unreal version, sample compatibility, asset availability, and each MCP connection before implementation. Report unavailable capabilities and any manual setup needed; do not claim a tool was used when it was not.
- [ ] Preserve the existing project and build within it where practical. Inspect its current content before changing it.

Epic's documentation describes the Game Animation Sample's motion matching, locomotion, ledge scaling, and vaulting examples. The sample is an animation foundation; multiplayer game rules and reliable networked traversal still need implementation and validation.

Reference: [Epic Games — Game Animation Sample](https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine).

## Confirmed game rules

### Teams and victory

- Two teams of five compete online on the same map.
- Each team progresses through two preliminary objective stages before unlocking a final objective.
- Teams have separate preliminary objectives that are similar in structure but not identical. Completing one team's preliminary stage does not complete the other team's stage.
- The teams race to complete the final objective. The first team to finish wins.
- A team can also win by eliminating every active player on the opposing team.
- Example only: locate information about the king, acquire poison or prepare a trap, then kill the king. These are illustrative objectives, not a fixed mission requirement.

### Crowd blending and identity

- Populate the map with NPCs drawn from a fixed pool of character models.
- Players use that same pool. Multiple NPCs must match player appearances; the entire crowd must not use a single identical model.
- Enemy players are visually indistinguishable from NPCs. Do not add enemy nameplates, outlines, team colours, or automatic player-identification markers.
- Teammates have markers above their heads, visible to their own team.
- Players identify enemies through behaviour: objective interactions, assassination animations, running or parkour, and other suspicious actions.
- The central loop is to blend into the crowd, complete objectives covertly, and spot opponents who give themselves away.

### Assassinations and defence

- A successful assassination eliminates an active enemy unless the defender successfully reverses it.
- A player can hold a button to prepare for an incoming assassination.
- Preparation lasts approximately **three to five seconds**; the exact duration is undecided.
- The defensive move is available **once per minute**.
- An assassination against a prepared defender is reversed in the defender's favour.
- Defensive preparation cannot be used while running or performing parkour.
- There is no confirmed general melee combat, dodge, or health system. Do not expand this prototype into a conventional combat game.

### Killing NPCs

- Assassinating an NPC visibly exposes the action through the assassination animation itself.
- It does not create an additional reveal marker.
- A player has a limit of three NPC assassinations before being removed from active play and becoming an informant.
- Display a short message explaining the removal. “Arrested” or “exiled” are candidate wording; final wording is undecided.
- The exact threshold interpretation—removal on the third NPC kill or on exceeding three—needs confirmation. The proposed prototype default below uses removal on the third.

### Informants after elimination

- An assassinated player remains in the world as an NPC-like watcher or informant.
- They can walk around, observe enemies, and communicate information to teammates.
- They cannot complete objectives, use parkour, or otherwise directly advance their team's progress.
- Players removed for excessive NPC kills enter the same informant state.
- Informants do not count as surviving active players for the team elimination win condition.
- No revival or return to active play was requested.

### Hiding

- Include bushes or similar designated hiding places that players can remain in.
- A player inside a bush is completely invisible to enemies. This is an explicit visibility mechanic, not just visual cover from foliage.
- Rules for attacking from hiding, entering or leaving concealment, and informant visibility still need definition.

### Cameras and movement

- Support both first-person and third-person cameras with a player-controlled toggle.
- Both cameras use the same character, movement permissions, and abilities.
- Active players can use parkour. Informants cannot.
- Use Game Animation Sample motion matching and its available traversal assets as the starting point.
- Evaluate third-person corner peeking during playtests because it changes how safely players can observe enemies.

### Procedural maps

- Create a reusable set of modular building assets.
- Arrange these assets pseudorandomly for each match so layouts are not predictable.
- Place objectives at selected generated locations.
- Every participant in a match must see the same layout and objective placement.

## Ordered implementation checklist

### 1. Inspect the project and prove the toolchain

- [ ] Inspect the existing Unreal project, enabled plugins, sample content, and available assets.
- [ ] Verify Meshy MCP, Blender MCP, and Unreal MCP connectivity and supported operations.
- [ ] Locate or integrate a compatible Game Animation Sample foundation.
- [ ] Document missing dependencies before relying on them.
- [ ] Establish a minimal multiplayer test map before building the full environment.

**Done when:** the project opens, a character moves with the intended animation foundation, and multiple clients can join the same test session.

### 2. Build the multiplayer round framework

- [ ] Support two teams with capacity for five players each.
- [ ] Implement active, informant, and round-ended states.
- [ ] Keep team assignments, eliminations, objective progress, NPC-kill counts, defence timing, and victory decisions authoritative on the server.
- [ ] Replicate the relevant state consistently to clients.
- [ ] Implement objective-completion and opposing-team-elimination victories.
- [ ] Provide a basic way to start and reset a round for testing.

**Done when:** two clients can test the complete round lifecycle, and the session supports ten player slots. A full ten-player test remains a separate acceptance check.

### 3. Implement movement, cameras, and traversal

- [ ] Integrate Game Animation Sample locomotion and motion matching.
- [ ] Add walking, running, and a small set of reliable traversal actions using available sample content, such as vaulting and ledge traversal.
- [ ] Add the first/third-person toggle and suitable camera collision handling.
- [ ] Prevent running or traversal from bypassing defensive-state restrictions.
- [ ] Restrict informants to their permitted movement.
- [ ] Validate movement and traversal across the network; sample behaviour must not be assumed multiplayer-ready.

**Done when:** both camera views work, traversal is visible consistently to other clients, and state restrictions hold.

### 4. Create the shared character pool and crowd

- [ ] Generate a small Aztec-inspired character pool through Meshy MCP.
- [ ] Prepare the assets through Blender MCP and integrate them through Unreal MCP.
- [ ] Make the characters compatible with the chosen animation setup through suitable rigging or retargeting.
- [ ] Assign the same model pool to players and NPCs, with repeated appearances throughout the crowd.
- [ ] Implement basic NPC walking, idle behaviour, and navigable crowd routes.
- [ ] Match ordinary player walking and NPC presentation closely enough for blending to work.
- [ ] Add teammate-only overhead markers without revealing enemy identities.

**Done when:** an enemy's model, UI, or ordinary idle/walk presentation does not automatically identify them as a player.

### 5. Implement assassination, reversal, and informants

- [ ] Implement assassination targeting, range checks, and networked animation playback.
- [ ] Implement held defensive preparation, maximum duration, cooldown, and reversal resolution.
- [ ] Prevent defence during running or parkour, including incompatible state transitions while defence is held.
- [ ] Track NPC kills and apply the configured removal threshold.
- [ ] Convert eliminated or removed players to informants and disable prohibited interactions.
- [ ] Allow informants to continue observing and communicating.
- [ ] Resolve competing elimination and victory events consistently on the server.

**Done when:** normal assassinations, reversals, NPC penalties, informant restrictions, and team elimination wins work across clients.

### 6. Build a complete objective chain

- [ ] Create a reusable objective definition with location, interaction requirements, team eligibility, stage, and completion state.
- [ ] Implement two separate preliminary stages per team, followed by a final objective.
- [ ] Prevent early access to locked stages and prevent one team's preliminary completion from advancing the other team.
- [ ] Give players clear team objective progress without identifying enemy players.
- [ ] Build at least one complete example mission chain for end-to-end testing.

**Done when:** either team can win by progressing through its own preliminary stages and completing the final objective.

### 7. Build modular map generation

- [ ] Create a small building kit through Meshy MCP and Blender MCP, then import and assemble it through Unreal MCP.
- [ ] Include useful streets, crowd spaces, hiding spots, and traversal opportunities.
- [ ] Generate layouts using a server-selected seed and replicate or otherwise synchronise the resulting layout.
- [ ] Place valid team spawns and objective sites without intersecting geometry.
- [ ] Validate navigation, objective reachability, and usable routes for both teams.
- [ ] Ensure the generated layout supports NPC navigation and the implemented traversal actions.
- [ ] Reject or repair invalid layouts before beginning the round.

**Done when:** several different seeds produce playable layouts that agree across clients and allow both teams to finish their objective chains.

### 8. Add bush concealment

- [ ] Implement designated hiding volumes associated with bushes.
- [ ] Hide a concealed player's visible character and identifying attachments from opponents.
- [ ] Preserve friendly visibility or markers according to the documented prototype choice.
- [ ] Restore enemy visibility on exit and handle elimination or round reset cleanly.
- [ ] Validate concealment separately for each viewer; do not hide a character globally if teammates still need to see them.

**Done when:** opponents cannot see a concealed player in either camera mode, and exiting restores visibility reliably.

### 9. Validate and deliver the first playable

- [ ] Run a complete match ending through final-objective completion.
- [ ] Run a complete match ending through elimination of all active opponents.
- [ ] Verify a reversal, defence expiry and cooldown, and blocked defence while running or doing parkour.
- [ ] Verify NPC-kill removal and all informant interaction restrictions.
- [ ] Verify teammate markers, enemy anonymity, and concealment from different clients.
- [ ] Test different procedural seeds for shared geometry, reachable objectives, and navigable crowds.
- [ ] Test a ten-player session where available. Clearly distinguish verified two-client behaviour from untested full-capacity behaviour.
- [ ] Record assets produced by each MCP, changed project content, controls, test results, known issues, and how to launch the prototype.

**Done when:** the prototype is playable from round start to a valid win, with limitations documented honestly.

## Suggested first-draft defaults — not confirmed design decisions

Use these as configurable prototype values if implementation needs to proceed before further design discussion. Keep them distinct from confirmed requirements.

| Setting | Suggested starting choice |
| --- | --- |
| Defence duration | 4 seconds; tune within the requested 3–5 second range |
| Defence cooldown | 60 seconds from activation, including unused attempts |
| NPC-kill removal | On the third NPC assassination in a round |
| Objective count | One objective in each preliminary stage, then one shared final objective |
| Informant actions | Walk and observe; no attacks, defence, objectives, running, parkour, or physical blocking |
| Informant communication | Existing team voice/chat or external voice for initial tests; built-in voice is outside the first draft unless required |
| Bush behaviour | Concealment while fully inside; restored visibility on exit; attacking breaks concealment before the attack becomes visible |
| Friendly concealment visibility | Teammates retain their friendly overhead markers |
| Map scope | One compact environment assembled from a small modular kit |
| Crowd and character counts | Exposed tuning values, chosen after an initial performance and blending test |
| Match timer | No time-limit victory rule until specified |

## Open design decisions

- Is the final objective one shared target or two equivalent team-specific targets?
- How many tasks belong to each preliminary stage, and are alternative tasks allowed?
- Does successful reversal eliminate the attacker immediately, and what happens if both players are prepared?
- Does the defence cooldown begin on activation or when the defensive window ends?
- Is the NPC-kill penalty applied on the third kill or when the player exceeds three?
- Can informants be targeted, see concealed opponents, or pass through other characters?
- What exactly breaks bush concealment, and can concealed players be assassinated if their position is inferred?
- What are the desired match duration, crowd density, assassination range, and interaction times?
- What happens on disconnects, simultaneous final completions, or simultaneous team eliminations?

## Scope of the first draft

Prioritise a playable online social stealth loop, a small coherent asset kit, and clear evidence that the rules work. Advanced matchmaking, progression, cosmetics, extensive combat, elaborate parkour, and production-scale procedural generation are not required for this first draft.
