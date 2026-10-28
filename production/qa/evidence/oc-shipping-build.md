# Shipping build/link with the Objectives debug path compiled out (story oc-008, AC4 (c))

Date: 2026-10-28. Engine 5.8.4 (CL 58902052). Working tree: HEAD 2721e32 plus the uncommitted Objectives Chain batch 001-009
(the commit that carries this file).

```
Build.bat StealthGame Win64 Shipping -Project=<uproject> -WaitMutex
Building StealthGame...
[4/67] Compile [x64] ObjectivesInvariants.cpp
[10/67] Compile [x64] StealthObjectivesWiring.cpp
[42/67] Compile [x64] ObjectivesClaims.cpp
[43/67] Compile [x64] ObjectivesChainRegistry.cpp
[44/67] Compile [x64] ObjectivesInteractPredicate.cpp
[45/67] Compile [x64] ObjectivesCore.cpp
[48/67] Compile [x64] ObjectivesService.cpp
[66/67] Link [x64] StealthGame-Win64-Shipping.exe
Result: Succeeded
Total execution time: 51.98 seconds
exit=0
```

No error and no unresolved external: every declaration and definition of `DebugCompleteStage`, `ApplyDebugStage`,
`ApplyDebugStageCompletion` and the composition root's `SubmitDebugStage` / `OnObjectiveDebugStage` wiring is inside
`#if !UE_BUILD_SHIPPING` (source scan: `StealthGame.Objectives.DebugCompleteStage`), and Shipping links without them. Round's
`FRoundMatchCore::SubmitDebugObjectiveStage` compiles in every configuration and answers `Unavailable` in Shipping, as
`SubmitDebugItem` does; the queue drops a DebugItem in Shipping (ADR-0002).

Result: PASS.
