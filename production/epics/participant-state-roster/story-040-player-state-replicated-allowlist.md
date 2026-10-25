# Story 040: PlayerState replicates only the allowlist and engine defaults are disabled or constant

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules
**Also governed by**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: ADR-0001 section 4.3 makes `AStealthPlayerState` a closed public allowlist: it replicates to all clients exactly `PublicHandle` (constant for the match) and `PlayerNamePrivate` (carrying `DisplayName` only, frozen after Lobby), plus base `AActor` properties held constant. `AActor::Owner`, `PlayerId` and `UniqueId` are reset to `COND_OwnerOnly`; `Score`, `StartTime`, `bIsABot`, `bIsInactive`, `bIsSpectator`, `bOnlySpectator`, `bFromPreviousLevel` and `CompressedPing` are disabled, with the replicated-ping flag cleared. No Roster transition, connection event or team change ever writes a PlayerState byte. ADR-0005 keeps authoritative state in the game-instance stores precisely because seamless travel replaces PlayerStates.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0001 post-cutoff items: `APawn::RemoteViewPitch16` (5.6) and the `net.Iris.UseIrisReplication` default of 0 (legacy driver). Verification items: S2 (PlayerState trimming; PASS 2026-10-25 within the testable scope, travel and rejoin not testable), S1, S14. Spike amendment: `SetShouldUpdateReplicatedPing` is protected in 5.8.4, so the replicated-ping flag must be set inside the PlayerState class itself. `Score`, `PlayerId`, `PlayerNamePrivate` and `bShouldUpdateReplicatedPing` are private members, so they are reset or disabled by name; a misspelled name is a fatal startup error, not a compile error (E4/E17). ADR-0005 verification items S3, S4, S5 are not yet reported.

**Control Manifest Rules (this layer)**:
- Required: `AStealthPlayerState` replicates only `PublicHandle` and `PlayerNamePrivate`; it resets its own `Owner`, `PlayerId` and `UniqueId` to `COND_OwnerOnly` — source: ADR-0001
- Required: Every new replicated field, fact, audience, condition or replicated class ships with a Visibility Policy row and a signed-off allowlist row; the reflection audit fails the build otherwise — source: ADR-0001, ADR-0003
- Required: Every leak, parity, statistical and lint gate includes a negative control the suite detects — source: ADR-0008
- Forbidden: Never put team, state, connection, appearance or pawn data on a PlayerState; never give NPCs a PlayerState — source: ADR-0001
- Forbidden: Never change the in-round player array; never enter the engine spectator state — source: ADR-0001
- Forbidden: Never promote prototype code (`Source/StealthGame`) into a production module before its governing ADR is Accepted; promotion is a rewrite, not a file move — source: ADR-0007
- Guardrail: Identity policy: C-TeamView pushes change-only, at most 1 per frame, within the 50 KB/s per-client budget (PlayerState adds no per-event bytes) — source: ADR-0001

This story rewrites the game PlayerState so that its replicated surface is the closed C-Public allowlist and every engine-default property is disabled or held constant, and it proves that with a reflection audit plus a scripted disconnect, rejoin and elimination run. It exists because any PlayerState field that changes when someone is eliminated or disconnects is a remote kill or disconnect detector (Pillar 1). It realises GDD R6 (C-Public closed allowlist) and the Edge Case "any PlayerState field outside the allowlist would change on a player event".

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-44** [Int] `StealthGame.Roster.PlayerStateReplicatedAllowlist`: GIVEN the game PlayerState class, WHEN its lifetime replicated properties are enumerated by reflection after the replicated-props override, THEN:
  - the actively replicated set is exactly {`PublicHandle`, `DisplayName`};
  - every engine-default property in R6's list is disabled or flagged held-constant;
  - a scripted disconnect, rejoin and elimination change no allowlisted or held-constant value (R6 closed allowlist, Session rules 22/24).

---

## Implementation Notes

*Derived from ADR-0001 section 4.3 and Implementation Guidelines, ADR-0005 guideline 4 and GDD R6:*

- **The class.** `AStealthPlayerState : APlayerState` lives in the root module (`StealthGame`, ADR-0001 Key Interfaces), header under `Source/StealthGame/Public/View/` (ADR-0007: every public header sits in `View/` or `Server/`; this one is client-safe). `GetLifetimeReplicatedProps` calls `Super`, then applies, per ADR-0001 table 4.3: `DOREPLIFETIME(AStealthPlayerState, PublicHandle)` (`PublicHandle` is constant for the match); `Score` and `StartTime` disabled (`DISABLE_REPLICATED_PRIVATE_PROPERTY` for the private `Score`); `PlayerId` (private, by name with `ResetReplicatedLifetimeProperty`) and `UniqueId` reset to `COND_OwnerOnly`; `bIsABot`, `bIsInactive`, `bIsSpectator`, `bOnlySpectator`, `bFromPreviousLevel` disabled; `CompressedPing` disabled; `AActor::Owner` reset to `COND_OwnerOnly` (rejoin re-association calls `SetOwner`, E16). `PlayerNamePrivate` stays at the engine default and carries `DisplayName` only, frozen after Lobby. The base `AActor` properties (`bReplicateMovement`, `Role`/`RemoteRole`, `bHidden`, `bTearOff`, `bCanBeDamaged`, `Instigator`) are held constant and each is listed in the allowlist table.
- **Ping flag.** Call `SetShouldUpdateReplicatedPing(false)` inside the PlayerState class (constructor), not from outside: it is protected in 5.8.4 (S2 amendment).
- **Checked-in treatment table.** The AC says "disabled or flagged held-constant" but defines no flag. Resolve it with a checked-in table (a `constexpr` array in the test, mirrored from ADR-0001 table 4.3) mapping each R6 default property to its treatment: `Disabled`, `OwnerOnly` or `Constant`. "Constant" is verified by value comparison before and after the scripted run; no code-side flag is needed.
- **Enumerating the active set.** Read the CDO's replicated properties via `GetLifetimeReplicatedProps` after the override and treat a property as non-owner-visible when its condition is not `COND_Never` and not `COND_OwnerOnly`. Compare that set with {`PublicHandle`, `DisplayName` (engine `PlayerNamePrivate`)} plus the held-constant base `AActor` entries from the table.
- **Where the test lives.** The AC puts [Int] tests under `Source/StealthFoundation/Private/Tests/Roster/`, but Foundation cannot depend on the root module. The test therefore resolves the class by reflection (for example `LoadClass<APlayerState>` on `/Script/StealthGame.StealthPlayerState`) and never includes the game header. If this proves unworkable, relocate the test to the root module's `Private/Tests/` and record it.
- **Scripted run.** In an automation world, snapshot every allowlisted and held-constant value, then drive a disconnect (`NotifyConnectionChanged(Disconnected)`), a rejoin (reclaim) and an elimination (`RequestTransition(Informant, Assassinated)`) through the Roster core and any PlayerState deactivate/reactivate path that exists, then assert the snapshot is unchanged. `OnDeactivated` not destroying the PlayerState while admission is `RejoinOnly` and `ShouldBroadCastWelcomeMessage` returning false are ADR-0005 guidelines 4 and 4a owned by the Session & Travel work; assert on whatever exists and do not implement them here.
- **Visibility Policy rows.** `PublicHandle` is a new replicated field and needs its IVP row and signed-off allowlist row (manifest rule); confirm the row exists before closing.
- **Open point:** the AC says the active set is "exactly {`PublicHandle`, `DisplayName`}", but ADR-0001 also keeps constant base `AActor` properties and `COND_OwnerOnly` entries for `PlayerId`, `UniqueId` and `Owner` (the GDD says "disabled or held-constant", the ADR chose owner-only for the ids). The active-set definition above reconciles them; the design owner should confirm and the AC wording be aligned.
- **Open point:** prototype consumers of the removed fields (`PS->Team`, `LifeState`, `IsInformant()` in `Source/StealthGame/Private/StealthCharacter.cpp`, `StealthHUD.cpp`, `StealthGameMode.cpp`) will not compile once the fields go. Decide with the lead-programmer whether the prototype is isolated behind a build guard or its consumers are removed in the same change; the manifest says promotion is a rewrite, not a file move.
- **Prototype disposition:** replaces `Source/StealthGame/Public/StealthPlayerState.h` and `Source/StealthGame/Private/StealthPlayerState.cpp`, which replicate `Team` and `LifeState` to all clients (known issue 2 of `production/first-playable-report.md`) and `NpcKills`, `LastDefenceActivationTime` owner-only. `Source/StealthGame/Private/Spikes/StealthIdentitySpike.*` (its `ApplyPlayerStateTrims`) is reference only, never promoted.
- **Header naming (ADR-0007).** Stealth prefix, project-unique: `StealthPlayerState.h` (the prototype header of the same name is deleted in the same change, so no duplicate exists), `StealthRosterTypes.h` for `FPublicHandle` if needed.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 044 (The public player list is identical before and after eliminations and disconnects): PIE proof of the player-list widget and its screenshots.
- Story 045 (Opponents receive nothing but allowlisted movement updates after a kill of a player, NPC or informant): PIE proof that no PlayerState update reaches an opponent after a kill.
- Story 046 (Non-owned pawns carry null PlayerState, Controller and Owner for players and NPCs alike): owns the pawn pointer conditions (`AStealthPawn`).
- Story 039 (No Server/Client/Multicast RPC parameter contains a participant id or identity key): RPC parameter audit.
- Story 057 (Two client processes on one machine with distinct identity keys get distinct records) and Story 059 (A client rejoining within grace reclaims its record, pawn and informant status): multi-process rejoin and S4.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGame/Public/View/StealthPlayerState.h` (new; replaces `Source/StealthGame/Public/StealthPlayerState.h`)
- `Source/StealthGame/Private/StealthPlayerState.cpp` (rewritten; replaces the prototype implementation)
- `Source/StealthFoundation/Public/View/StealthRosterTypes.h` (`FPublicHandle` if not already present)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterPlayerStateAllowlistTest.cpp` (class `FRosterPlayerStateReplicatedAllowlistTest`, test name `StealthGame.Roster.PlayerStateReplicatedAllowlist`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

**Flagged gap (QL-STORY-READY: GAPS):** "flagged held-constant" has no defined flag or mechanism. R6 leaves each default property's treatment to the ADR. Needs a checked-in table of each default property to "disabled" or "constant", with "constant" checked by value comparison.

**Provisional spec (qa-lead, pending gap resolution):**

- **AC-1**: PlayerState replicates only the allowlist; engine defaults are disabled or constant.
  - Given: the game PlayerState CDO and the ADR-0001 treatment table.
  - When: lifetime replicated properties enumerated after the override (condition not COND_Never), then a scripted disconnect, rejoin and elimination runs.
  - Then: active set exactly {PublicHandle, DisplayName}. Each R6 default (PlayerId, Score, StartTime, bIsSpectator, bOnlySpectator, bFromPreviousLevel, bIsABot, bIsInactive, UniqueId, CompressedPing) disabled or constant. No allowlisted or constant value changes.
  - Edge cases: CompressedPing is set inside the PlayerState class.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterPlayerStateAllowlistTest.cpp` — automated test must exist and pass (OR a documented playtest; `qa.level` is full, so the automated test is required).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-001-record-schema-whitelist.md`
- Unlocks: `production/epics/participant-state-roster/story-044-pie-public-list-reveals-nothing.md`, `production/epics/participant-state-roster/story-045-pie-elimination-leak-diff.md`, `production/epics/participant-state-roster/story-046-pie-pawn-pointer-dump.md`
