# Story 027: Retire the prototype ping

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (PNG-R1: the ping marks a place and never the sender's own position; the epic's Prototype Disposition)
**Requirement**: `TR-png-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline (Migration Plan: replace, not port: `ServerPing` on the pawn and cooldown fields on the PlayerState), with ADR-0007 (nothing in the prototype is promoted)
**ADR Decision Summary**: The prototype's pawn RPCs (`ServerAssassinate`, `ServerSetDefence`, `ServerSetInteracting`, `ServerPing`), cooldown fields on the PlayerState and the sample's input binds are replaced by the controller-level command path and `ServerTeamPing`; nothing from the prototype is ported.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 overall; deletion plus a re-pointed bot command, no post-cutoff API)
**Engine Notes**: Deleting a UFUNCTION RPC changes the class's RPC table; rebuild the editor target and run the project test command (`commands.test`) after the removal. Prototype code is marked PROTOTYPE in `production/first-playable-report.md`.

**Control Manifest Rules (this layer)**:
- Required: no gameplay `Server`/`Client` UFUNCTION remains on `AStealthPawn` (the static scan rule covers pings) — source: ADR-0011
- Forbidden: Never port prototype ping code (its own-position ping, client RPC fan-out, cooldown on the PlayerState) — source: ADR-0007, ADR-0011

---

## Acceptance Criteria

*From the epic's Prototype Disposition and GDD PNG-R1, scoped to this story:*

- [ ] **AC1** (prototype removed): **GIVEN** the production path is in place **WHEN** the prototype is retired **THEN** `AStealthCharacter::OnPingPressed` with its T binding and `ServerPing`, `AStealthGameMode::HandlePing`, `AStealthPlayerController::ClientReceivePing`, the `UStealthSettings` `PingCooldown` and `PingLifetime` values, and `AStealthPlayerState::LastPingTime` are removed (grep finds none, with the files scanned stated) and the project builds.
- [ ] **AC2** (TR-png-001): **GIVEN** the prototype `StealthBot` "ping" command **WHEN** run in the networked smoke test **THEN** it calls `ServerTeamPing` from the bot's controller at an aim-derived ground point at least 100 cm from the sender's own position (never the sender's own position), and the ring appears in the ally's HUD model; **GIVEN** the aim has no ground hit **THEN** nothing is sent.
- [ ] **AC3** (HUD hint strings): **GIVEN** the prototype HUD hint strings **WHEN** grepped **THEN** none names the T key for ping (the `DrawPings` drawing and `FStealthPing` storage are retired by story 019 AC4, not here).

---

## Implementation Notes

*Derived from the epic's Prototype Disposition and ADR-0011 Migration Plan:*

Remove, after stories 006, 010 and 016 are in: `AStealthCharacter::OnPingPressed` and the `BindKey(EKeys::T, ...)` line (`StealthCharacter.cpp`), `ServerPing` (`StealthCharacter.h/.cpp`) and `LastLocalPingTime`, `AStealthGameMode::HandlePing` (`StealthGameMode.cpp`) and the `StealthPlayerState.cpp` `LastPingTime` initialiser and field, `ClientReceivePing` in `StealthPlayerController.cpp/.h`, and the `PingCooldown`/`PingLifetime` entries in `StealthSettings.h` (their production replacements are the tuning keys of story 001). Change the `[Bot] ... ping` action in `StealthGameMode.cpp` (which currently calls `HandlePing(Pawn, Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 300.f)`) to drive `ServerTeamPing` through the bot controller at a point from the ping-point rule (story 008) with an injected trace, so the networked smoke test (see `production/first-playable-report.md`) still covers a ping end to end. The `DrawPings` function, the prototype ring drawing and `FStealthPing`/`Pings` storage go with story 019 (they need the production painter first); AC3 here covers only the two prototype hint strings that name the T key ("INFORMANT: watch, ping sightings [T]" and "T ping" in `StealthHUD.cpp`), which switch to the binding label from `ST_Controls`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 019 AC4: removing `DrawPings`, the prototype ring drawing and `FStealthPing` storage.
- input-interaction-framework epic story-093 (static scan and prototype retirement for the other prototype RPCs).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGame/Private/StealthCharacter.cpp` and `Source/StealthGame/Public/StealthCharacter.h` (remove `OnPingPressed`, T bind, `ServerPing`, `LastLocalPingTime`)
- `Source/StealthGame/Private/StealthGameMode.cpp` and `Source/StealthGame/Public/StealthGameMode.h` (remove `HandlePing`; re-point the bot action)
- `Source/StealthGame/Private/StealthPlayerController.cpp` and `Source/StealthGame/Public/StealthPlayerController.h` (remove `ClientReceivePing`)
- `Source/StealthGame/Private/StealthPlayerState.cpp` and `Source/StealthGame/Public/StealthPlayerState.h` (remove `LastPingTime`)
- `Source/StealthGame/Public/StealthSettings.h` (remove `PingCooldown`, `PingLifetime`)
- `Source/StealthGame/Private/StealthHUD.cpp` (hint strings only)
- `Source/StealthGame/Private/Tests/PrototypePingRetiredTest.cpp` (class `FPingsPrototypePingRetiredTest`, name `StealthGame.Pings.PrototypePingRetired`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 prototype removed**
- Given: the production path in place.
- When: grep the symbols and build.
- Then: no `AStealthCharacter::OnPingPressed`, its T binding, `ServerPing`, `AStealthGameMode::HandlePing`, `AStealthPlayerController::ClientReceivePing`, `UStealthSettings` `PingCooldown`/`PingLifetime`, `AStealthPlayerState::LastPingTime`; the project builds.
- Edge cases: state the files scanned; the project build is the UBT step.

**Test: AC2 bot ping command**
- Given: the StealthBot "ping" command in the networked smoke test.
- When: run.
- Then: calls `ServerTeamPing` from the bot's controller at the aim-derived ground point (not the sender's own position; assert >= 100 cm away); the ring appears in the ally's HUD model.
- Edge cases: an aim with no ground hit sends nothing.

**Test: AC3 HUD hints**
- Given: the prototype HUD hint strings.
- When: grep for the T key naming ping.
- Then: none.
- Edge cases: `DrawPings` is retired by story 019 AC4, not here.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGame/Private/Tests/PrototypePingRetiredTest.cpp` plus the networked smoke run OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-006-server-team-ping-handler-and-host-parity.md`, `story-008-client-ping-point-rule.md`, `story-010-ia-ping-action-and-send-path.md`, `story-016-ring-model-and-age.md`
- Unlocks: None
