# OBJ-V3: the Objectives chain asset cooks with AlwaysCook and resolves on a cooked client (story oc-002, AC3 (b))

Date: 2026-10-28. Engine 5.8.4 (CL 58902052). Working tree: HEAD 2721e32 plus the uncommitted Objectives Chain batch 001-009
(the commit that carries this file).

## Cook

```
RunUAT BuildCookRun -project=<uproject> -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -IgnoreCookErrors -stagingdirectory=Saved/Staged/OcCook
BuildCookRun time: 86.37 s
BUILD SUCCESSFUL
AutomationTool exiting with ExitCode=0 (Success)
```

(`-IgnoreCookErrors`: the sample's broken CameraDirector_SandboxCharacter import, present before this project's work, as in
story tdc-020's `cook-and-stage.txt`.) The first attempt failed to BUILD the Development game: the existing test
`StealthGame.Assassination.WarpTargetsNotReplicated` read `UClass::ClassGeneratedBy`, which is editor-only data; it now tests
`CLASS_CompiledFromBlueprint` (same meaning, every configuration). No map or other asset references `DA_ObjectiveChain_Cacao`, so
its presence in the cooked pak comes from the `ObjectiveChain` primary asset type's `CookRule=AlwaysCook` (Config/DefaultGame.ini).

## Cooked client

```
Saved/Staged/OcCook/Windows/StealthGame.exe -ExecCmds="Stealth.Objectives.ResolveChain,quit" -nullrhi -unattended -nosound -windowed -log
LogInit: Build: ++UE5+Release-5.8-CL-58902052
LogTemp: Display: OBJECTIVES CHAIN RESOLVE: PASS id=ObjectiveChain:DA_ObjectiveChain_Cacao path=/Game/StealthGame/Objectives/DA_ObjectiveChain_Cacao.DA_ObjectiveChain_Cacao chains=1 rows=5 serial=1 cooked=1
exit=0
```

`cooked=1` is `FPlatformProperties::RequiresCookedData()`: the cooked Development client resolved
`FPrimaryAssetId("ObjectiveChain", DA_ObjectiveChain_Cacao)` through the Asset Manager and loaded the five OC-16 rows.

Result: OBJ-V3 PASS.
