# ADR-0009: Version control and binary asset workflow

## Status
Accepted — 2026-10-25, by the user: **Git + Git LFS** chosen (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). Initialising the repository, the first commit and any push still need an explicit user instruction (CLAUDE.md); `_snapshots/` stays in force until then.

*Earlier status text (kept for its gating conditions):* Proposed — **BLOCKED ON USER DECISION.** No version control exists in this project. Choosing a system, initialising a repository and making the first commit are the user's decisions (CLAUDE.md: "No commits without user instruction"; architecture.md AQ-03). This ADR records the options, a recommendation and the policies that apply whichever is chosen. Nothing has been initialised, and no `.gitignore`/`.gitattributes` or other project file was changed by this ADR. Awaiting user acceptance.

## Date
2026-10-24

## Last Verified
2026-10-24 (local tool versions and project sizes measured this session; engine source-control plugins read from the 5.8.4 install)

## Decision Makers
Autonomous design pipeline (`/architecture-decision --review full`) records the options, validated by technical-director (TD-ADR) and the engine-specialist check. **The decision itself belongs to the user.**

Review record (2026-10-24): engine check **MINOR NOTES** (plugin facts confirmed; Git plugin probes but never uses LFS locking; editor "Make Writable" bypasses read-only; diff supported; no World Partition external-actor folders; `.uproject` CRLF; `assets/` holds `.glb` and `*.fbm/`). TD-ADR **CONCERNS** (6) → all revised: locking only with a locking-capable remote and a single-writer rule before that; Make Writable forbidden + pre-commit lock check + re-checkout step; remove the template's Godot/Unity ignore sections with a `git check-ignore` check; one-pattern-per-line `.gitattributes` with `*.glb`, `*.fbm/` and `text` rules; redirector rule; engine facts updated. Re-review of the revised text: not run (autonomous).

## Summary
The project (3.8 GB of `Content/`, 2,232 `.uasset`/`.umap` files, mostly binary and edited by both the user and AI agents through the Unreal MCP tools) has no version control; today's protection is manual `_snapshots/` copies. This ADR lays out Git + Git LFS (recommended), Perforce Helix Core and Unity Version Control, defines the binary-asset lock policy, ignore rules, commit conventions and the interim snapshot workaround, and leaves the choice to the user — every other ADR's acceptance waits on it (bootstrap order).

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.8 (installed 5.8.4, CL 58902052) |
| **Domain** | Core (tooling / process; editor source-control integration) |
| **Layer** | Foundation (process) |
| **Knowledge Risk** | LOW for the engine (source-control provider plugins are pre-cutoff); MEDIUM for third-party facts (hosting quotas, licence tiers and the community Git plugin change often and were not verified) |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `PLUGINS.md`; `docs/architecture/architecture.md` (Required ADRs bootstrap order, AQ-03); `design/gdd/game-concept.md` (TD-FEASIBILITY: "No version control on a 3.7 GB agent-edited binary project — Git + LFS recommended"); `production/first-playable-report.md` (known issue 11; snapshot use); `production/session-state/active.md` (snapshot workaround, resave gotchas); `.claude/docs/coding-standards.md` (commit conventions); engine plugins read 2026-10-24: `Engine/Plugins/Developer/{GitSourceControl,PerforceSourceControl,PlasticSourceControl}` |
| **Post-Cutoff APIs Used** | None |
| **Verification Required** | (1) The chosen editor provider plugin connects, shows status and (if locking is chosen) locks `.uasset`/`.umap` from the Content Browser in 5.8.4 [U]; (2) LFS pointer/smudge works for the 3.3 GB `Content/Characters` sample tree, including the `.uasset` files the editor must not resave (camera assets, active.md) [U]; (3) host storage/bandwidth quota fits ≥ 4 GB of LFS objects plus history [U — provider-specific]. |

Verified local facts [V] (measured 2026-10-24):
- `git version 2.52.0.windows.1` and `git-lfs/3.7.1` are installed (`C:\Program Files\Git\cmd`). No `.git` directory exists.
- Sizes: `Content/` 3.8 GB (`Content/Characters` 3.3 GB, `Content/MetaHumans` 338 MB, `Content/StealthGame` 58 MB); `assets/` 153 MB (FBX, GLB and extracted textures); `Source/` 254 KB; regenerable `Binaries/` 727 MB, `Intermediate/` 2.7 GB. 2,232 `.uasset`/`.umap` files; 6 files over 50 MB.
- A template `.gitignore` already exists with `Binaries/`, `DerivedDataCache/`, `Intermediate/`, `Saved/`, `*.exe`, `*.dll`, `*.log`, `*.zip`, and also `build/`, `bin/` (template rules). A template `.gitattributes` exists with LF rules for scripts only (no LFS rules).
- Engine source-control plugins in 5.8.4: **Git** 1.4 (`IsBetaVersion: true`, `GitSourceControl.uplugin:15`) — `UsesCheckout()` returns false and `UsesLocalReadOnlyState()` returns false (`GitSourceControlProvider.cpp:271-274, 286-289`); it probes git-lfs locking support (`GitSourceControlUtils.cpp:417-428`) but nothing reads that flag (`GitSourceControlProvider.h:21, 28`), so it gives **no editor-integrated locking**; it **does** support diff against the repository (`GitSourceControlProvider.cpp:306-309`). **Perforce** 1.0 and **Plastic SCM / Unity Version Control** 1.11.0 are not beta (`*.uplugin:15`).
- The editor can bypass a read-only file: with the Git provider a read-only package is not treated as "needs checkout" (`Editor/UnrealEd/Private/FileHelpers.cpp:1986`), and the save dialog offers **Make Writable** (`FileHelpers.cpp:2200-2247`). What MCP-driven saves do with a read-only file is [U].
- `Content/` has no World Partition `__ExternalActors__`/`__ExternalObjects__` folders today; `Config/` holds only `Default*.ini` (no per-user editor settings). `StealthGame.uproject` has CRLF line endings and no `text` rule exists for `.uproject`/`.uplugin`/`.ini`. `assets/` holds FBX, 8 `.glb` files and extension-less embedded textures in `*.fbm/` folders (FBX import side-effect); no `.blend` files.
- The template `.gitignore` contains Godot and Unity sections whose rules hit Unreal paths: `[Bb]uild/` (ignores `Build/` regardless of `core.ignorecase`), `[Ll]ogs/`, `[Tt]emp/`, and lower-case `build/`, `bin/` (Windows case-insensitive).
- CLAUDE.md's Technology Stack already names "Git with trunk-based development" (template default, never actioned).

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | None (the user's decision). |
| **Enables** | ADR-0007 and ADR-0008 acceptance (the module skeleton is the first commit; CI needs a repository); spikes S1/S2/S14; ADR-0001 acceptance; every coding epic. |
| **Blocks** | All coding (architecture.md AQ-03 "Blocks: all coding"); CI wiring in ADR-0008 (per-commit gate stays a local discipline until a repository exists). |
| **Ordering Note** | First in the acceptance order: ADR-0009 → ADR-0007 + ADR-0008 → spikes → ADR-0001 → ADR-0002 …. |

## Context

### Problem Statement
The project is a 3.8 GB, mostly-binary Unreal project edited by a human and by AI agents (editor MCP toolsets, Meshy/Blender pipelines). Without version control:
- a bad editor save is unrecoverable unless someone snapshotted first — this already happened once (resaving the sample's camera assets in 5.8 broke `CameraDirector` import; recovered only from `_snapshots/`, active.md);
- design and architecture documents (`design/`, `docs/`) have no history at all;
- the coding standards' "commits must reference the relevant design document or task ID" and ADR-0008's per-commit gate cannot exist;
- stories cannot be reviewed as diffs, and agents running in parallel worktrees (the studio template uses them) need a repository.
The choice involves cost, hosting and how the user wants to work, so it is not an architecture decision an agent may make; but its *shape* (what must be locked, ignored, stored as large files, and how commits are written) can be fixed now so that whichever system is chosen is set up correctly the first time.

### Constraints
- Single user today (plus AI agents on the same machine); possibly a small team later.
- Windows 11, UE 5.8.4 launcher build, VS 2022; git and git-lfs already installed.
- Unreal binary assets (`.uasset`, `.umap`) cannot be merged; concurrent edits to one asset lose work.
- Some sample assets must never be resaved by the 5.8 editor (camera assets, active.md); the tool must not force resaves (e.g. on checkout).
- Pipeline rule: this run may not init a repository, commit, or edit `.gitignore`/`.gitattributes` (outside `docs/`/`production/`).

### Requirements
- Full history of `Source/`, `Config/`, `*.uproject`, `Content/`, `design/`, `docs/`, `production/` (excluding gitignored session state), `tools/`, `assets/`, `.claude/`.
- Large-binary storage that does not bloat every clone (≥ 4 GB of binaries now).
- Exclusive locking for unmergeable assets once more than one writer (human or agent session) can touch the same asset.
- Editor integration good enough to see status/locks in the Content Browser.
- Commit conventions per coding standards (Conventional Commits, story/task ID in body).
- Works with agent worktrees (`.claude/worktrees/`) and the ADR-0008 gate.

## Decision

**Recommended (pending the user's choice): Git + Git LFS, trunk-based, with LFS file locking for unmergeable assets.** The policies in §2-§6 are written for Git + LFS and carry over to the alternatives as noted. Until the user decides, §6 (interim workaround) is in force.

### 1. Options (the user picks one)

| | **A. Git + Git LFS (recommended)** | **B. Perforce Helix Core** | **C. Unity Version Control (Plastic SCM)** | **D. Status quo (`_snapshots/` only)** |
|---|---|---|---|---|
| Setup now | Tools already installed [V]; `git init` + `git lfs install` + attributes; local-only repository possible immediately, remote later | Install server (local or hosted) and client; create depot/stream; typemap | Install client; cloud or local server; workspace | Nothing |
| Binary handling | LFS pointers; clone downloads only checked-out versions | Native; designed for large binaries | Native; designed for large binaries | Full copies per snapshot |
| Locking | `git lfs lock` with `lockable` attribute — **only once a remote whose LFS server supports the locking API exists**; a local-only repository cannot lock [U per host]; built-in UE Git plugin (Beta) does no locking [V] — editor locking needs the community Git LFS 2 plugin [U] or command-line discipline | Exclusive checkout (`+l` filetype), first-class in the editor plugin [V plugin not beta] | Smart locks, first-class in the editor plugin [V plugin 1.11.0 not beta] | None |
| Cost / hosting | Local repo free; remote LFS storage quotas vary by host and 3.8 GB exceeds typical free tiers [U] | Free tier for small teams exists but terms change [U]; server administration | Free tier exists but terms change [U]; cloud hosting | Free; disk |
| Fit with this project | Matches CLAUDE.md "Git, trunk-based", the template's hooks/`.gitattributes`, agent worktrees, `gh` tooling, UPGRADING.md template-merge flow | Industry standard for UE teams; weaker fit with template tooling (git-based hooks, worktrees, template upgrade by git merge) | Good binary story; weaker fit with git-based template tooling | Already failed once; no history, no diffs, no CI |
| Main risk | Lock discipline is manual unless the community plugin is adopted; LFS quota/bandwidth | Ops burden; template tooling assumes git | Ops/account; template tooling assumes git | Data loss |

**Why A is recommended:** zero install, the template's own tooling (hooks, worktrees, `UPGRADING.md` merge flow, `.gitattributes` already present) assumes git, the built-in editor plugin already shows status and diffs assets against the repository [V], and a local-only repository gives history and rollback today with no hosting decision. Locking is the weak point: until a locking-capable remote exists, the single-writer rule in §4 replaces it. **Choose B or C instead** if a team of several artists will join soon and editor-integrated exclusive checkout matters more than template integration.

### 2. Repository content and ignore rules (Git; equivalent ignore lists for B/C)
Tracked: everything under the project root except the ignore list. Before the first commit the setup story must review the template `.gitignore` for this project:
- **Ignore (already present):** `Binaries/`, `DerivedDataCache/`, `Intermediate/`, `Saved/`, `.vs/`, `*.VC.db`, `production/session-state/`, `production/session-logs/`, `.claude/settings.local.json`, `.claude/worktrees/`, `.claude/agent-memory/`.
- **Add:** `_snapshots/`, `_backup_pre_rebase/` (local safety copies; keep on disk, never commit), `Plugins/*/Binaries/`, `Plugins/*/Intermediate/`.
- **Keep commented** (must not be enabled): the template's `# *.uasset` / `# *.umap` lines.
- **Fix:** remove the template's **Godot and Unity sections entirely** — `[Bb]uild/` ignores an Unreal `Build/` folder (UAT/Gauntlet scripts, platform build settings, ADR-0008) regardless of `core.ignorecase`, and `[Ll]ogs/`, `[Tt]emp/` can hit Unreal paths; anchor or remove the generic `build/`, `bin/` rules (Windows is case-insensitive). Verify with `git check-ignore -v Build/Scripts/x.cs` (must print nothing) before the first commit. `*.zip`/`*.exe`/`*.dll` stay ignored (engine binaries are not committed; the project builds from source; a plugin that ships prebuilt DLLs would need an explicit exception).
- **`assets/`:** the `*.fbm/` folders (textures extracted by FBX import) are ignored or LFS-tracked — the setup story picks one; `.glb` files are LFS-tracked.
- `.gitignore`/`.gitattributes` are edited by the setup story, not by this ADR.

### 3. Large-file and lock attributes (Git LFS)
`.gitattributes` additions (setup story):
```
# Unreal binary assets — LFS + lockable (unmergeable)
*.uasset filter=lfs diff=lfs merge=lfs -text lockable
*.umap filter=lfs diff=lfs merge=lfs -text lockable
# Source art and media — LFS + lockable (unmergeable)
*.fbx filter=lfs diff=lfs merge=lfs -text lockable
*.glb filter=lfs diff=lfs merge=lfs -text lockable
*.blend filter=lfs diff=lfs merge=lfs -text lockable
*.psd filter=lfs diff=lfs merge=lfs -text lockable
*.tga filter=lfs diff=lfs merge=lfs -text lockable
*.exr filter=lfs diff=lfs merge=lfs -text lockable
*.hdr filter=lfs diff=lfs merge=lfs -text lockable
*.wav filter=lfs diff=lfs merge=lfs -text lockable
*.ogg filter=lfs diff=lfs merge=lfs -text lockable
*.mp4 filter=lfs diff=lfs merge=lfs -text lockable
*.png filter=lfs diff=lfs merge=lfs -text
*.jpg filter=lfs diff=lfs merge=lfs -text
*.jpeg filter=lfs diff=lfs merge=lfs -text
# Unreal text files stay in git (mergeable); normalise line endings
*.uproject text
*.uplugin text
*.ini text
```
Git reads one pattern per line: a line such as `*.fbx *.blend … filter=lfs` would treat `*.blend` as an attribute name and those files would never go to LFS. The existing LF rules for `*.sh`, `*.awk`, `.worktreeinclude`, `*.tsv`, `.claude/**/*.md` are kept. `*.cpp`, `*.h`, `*.cs`, `*.json`, `*.md` stay ordinary text.

### 4. Lock policy for `.uasset` / `.umap`
- **Before a locking-capable remote exists (including a local-only repository): single-writer rule.** Only one session (the user's editor or one agent session) edits `Content/` at a time; parallel agent worktrees may not save `.uasset`/`.umap` files. This is the interim policy for option A until §5's remote is chosen.
- **Once locking is available:** an unmergeable asset is locked by its editor *before* it is opened for editing and unlocked when the change is committed or abandoned. `lockable` makes unlocked files read-only on checkout, but this is **not** a hard stop: the editor's save dialog offers **Make Writable** [V], and files already present at `git init` stay writable until checked out again. Therefore: (1) **Make Writable is forbidden** for the user and for agents; (2) a **pre-commit check** compares every changed `.uasset`/`.umap` with `git lfs locks` and rejects changes to assets the committer does not hold; (3) the setup story re-checks-out the tree after adding `lockable` so the read-only state applies.
- **Agents:** an agent session that will save assets through the MCP toolsets locks (or, under the single-writer rule, claims) the explicit asset paths it will save first (the existing rule "save explicit paths, never mass-save" stays). Two concurrent agent sessions may not lock the same asset; the second session reports and waits.
- **Redirectors:** moving or renaming an asset leaves a redirector `.uasset` at the old path. The redirector is committed (and locked) together with the moved asset and every asset that references it; redirector fix-up is never run on sample folders (see the never-resave list), and elsewhere only as its own reviewed commit.
- **Never-resave list:** assets the 5.8 editor must not resave (the sample's camera assets; anything restored from a snapshot because a resave broke it) are listed in a checked-in `production/never-resave-assets.md` (created by the setup story) and are never locked for editing; a pre-commit check rejects a commit that modifies them.
- **Maps:** `.umap` files are locked per map; level work that would otherwise collide uses World Partition one-file-per-actor or sublevels (decided by the level ADRs, not here).
- **B/C mapping:** Perforce `+l` exclusive checkout on `//....uasset`/`//....umap` typemap; Unity VCS smart locks on the same extensions.

### 5. Workflow conventions
- **Trunk-based** (CLAUDE.md): one `main` branch; short-lived branches for stories and agent worktrees, merged by fast-forward or squash after the ADR-0008 per-commit gate passes.
- **Commits** follow `.claude/docs/coding-standards.md`: Conventional Commits (`feat:`, `fix:`, `chore:`, `docs:`, `test:`, `refactor:`), story/task ID in the body (`Story: EPIC-NNN-SNN`), design-doc or ADR reference.
- **Commits only on user instruction** (CLAUDE.md collaboration protocol) — agents prepare, the user (or an explicit user instruction) commits.
- **First commit** contents (setup story): the current tree as-is (prototype included, as evidence), after the ignore/attribute review above, tagged `baseline-2026-10-24` [suggested]. The module skeleton (ADR-0007) is the second commit.
- **Engine binaries are not versioned**; the engine is the launcher install pinned in `docs/engine-reference/unreal/VERSION.md`. A source-built engine (Dedicated Server, ADR-0024) gets its own decision.
- **Remote/backup:** the user chooses a host (or none, local-only plus an offline copy). Whatever the host, `git lfs` storage must be verified before the first push.

### 6. Interim workaround until the user decides (in force now)
The existing `_snapshots/` practice (active.md) continues and is made explicit:
- Before every batch of editor edits: copy `Content/StealthGame/` and `Source/` to `_snapshots/<YYYY-MM-DD>_<label>/`. **Extend it** to also copy `Config/`, `StealthGame.uproject` and any sample asset about to be touched (sample assets are never edited in place; they are duplicated or reparented with a snapshot first).
- `design/`, `docs/`, `production/` have no snapshot today; until VCS exists, ADR and GDD revisions rely on the decision log (`production/session-logs/decision-log.md`) and the "Change requests received" tables — an accepted, temporary gap.
- Never mass-save in the editor; save explicit paths only; editor autosave stays disabled (active.md).
- Snapshots are never deleted by agents (file deletion is an always-ask category); the user prunes them.

### Architecture Diagram

```
 (Recommended, pending user)                 (Interim, in force now)
 working tree ──git add──► index ──commit──► main (local)      Content/StealthGame, Source,
   │  *.uasset/*.umap: LFS pointer, lockable                   Config, .uproject, touched sample
   │  git lfs lock <path>  before edit (agents too)            assets ──copy──► _snapshots/<date>_<label>/
   │  read-only until locked                                   (before every editor edit batch)
   └─ pre-commit: ADR-0008 T0 lint + never-resave check
 main ──push (when a host is chosen)──► remote + LFS store
```

### Key Interfaces
- `.gitattributes` LFS/lockable patterns (§3) and `.gitignore` additions/fixes (§2) — applied by the setup story.
- `production/never-resave-assets.md` — checked-in list read by the pre-commit check.
- Commit message format — coding standards.
- Agent lock protocol: `git lfs lock <explicit paths>` → edit/save explicit paths → commit (on user instruction) → `git lfs unlock`.

### Implementation Guidelines
- No agent **may** initialise a repository, commit, push, or change `.gitignore`/`.gitattributes` without explicit user instruction.
- `.uasset` and `.umap` files **must** be stored via LFS (or the chosen system's binary storage) and **must** be lockable.
- Until a locking-capable remote exists, only **one** session **may** save `Content/` assets at a time.
- Once locking exists, an unmergeable asset **must** be locked before it is edited and unlocked after commit/abandon; agents **must** lock explicit paths before saving through MCP tools; **Make Writable must never** be used; the pre-commit lock check **must** pass.
- A redirector **must** be committed together with the moved asset and its referencers; redirector fix-up **must never** run on sample folders.
- Assets on the never-resave list **must never** be saved by the editor; a commit modifying one **must** be rejected.
- `Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Saved/`, `_snapshots/`, `_backup_pre_rebase/` **must never** be committed.
- The template's Godot and Unity ignore sections **must** be removed and `build/`/`bin/` anchored or removed before the first commit; `git check-ignore -v Build/Scripts/x.cs` **must** print nothing.
- `.gitattributes` **must** list one pattern per line.
- Commits **must** use Conventional Commits with a story/task ID and design-doc/ADR reference.
- Until VCS exists, every editor edit batch **must** be preceded by a `_snapshots/` copy including `Config/` and `StealthGame.uproject`.

## Alternatives Considered

### Alternative 1: Perforce Helix Core
- **Description**: Centralised VCS with exclusive checkout, streams and a first-class Unreal editor plugin.
- **Pros**: Industry standard for Unreal; native large-binary handling; exclusive locking in the editor [V plugin present, not beta]; scales to big teams.
- **Cons**: Needs a server (install, backups, licences/tier terms [U]); the studio template's tooling (git hooks, worktrees for parallel agents, `UPGRADING.md` git-merge flow, `gh`) assumes git; heavier for a single user.
- **Rejection Reason**: Not rejected — a valid user choice. Not recommended for the current single-user-plus-agents setup because it breaks template tooling; recommended instead if several artists join.

### Alternative 2: Unity Version Control (Plastic SCM)
- **Description**: Distributed/centralised hybrid with strong binary support and smart locks; Unreal plugin ships with the engine.
- **Pros**: Good binary handling and locking; editor plugin not beta [V]; GUI suited to artists.
- **Cons**: Account/hosting terms [U]; same git-tooling mismatch as Perforce.
- **Rejection Reason**: Not rejected — a valid user choice; not recommended for the same template-fit reason.

### Alternative 3: Plain Git without LFS
- **Description**: Commit binaries directly.
- **Pros**: Simplest.
- **Cons**: Every version of every 3.8 GB binary lives in every clone forever; no lock support; hosts reject files over their size limits.
- **Rejection Reason**: Unworkable for a 3.8 GB binary project.

### Alternative 4: Status quo (`_snapshots/` only)
- **Description**: Keep manual snapshots indefinitely.
- **Pros**: No decision or setup.
- **Cons**: No history for docs/design, partial coverage, no diffs, manual and easy to forget, no CI; already needed once to recover a broken asset.
- **Rejection Reason**: Acceptable only as the interim measure (§6).

## Consequences

### Positive
- Whichever system the user picks, the lock policy, ignore rules, attributes and commit conventions are already specified, so setup is one story.
- The never-resave list turns a known 5.8 gotcha into an enforced check.
- Unblocks the bootstrap order once decided (ADR-0007/0008 → spikes → ADR-0001).

### Negative
- Until the user decides, **all coding stays blocked** and the project runs on manual snapshots with no history for design documents.
- With Git + LFS, editor-integrated locking needs a community plugin [U] or command-line discipline (the built-in Beta Git plugin does not lock on checkout [V]).
- LFS hosting quota/cost for ≥ 4 GB is an open cost [U].

## Risks
- **Data loss before a decision** (bad resave, disk failure). Mitigation: extended snapshot rule (§6); recommend the user at least runs a local-only `git init` + LFS as soon as possible, even before choosing a host.
- **Lock discipline lapses** (an unlocked asset is saved via Make Writable, or before locking exists). Mitigation: single-writer rule until a locking remote exists; then Make Writable forbidden plus the pre-commit lock check; agents lock explicit paths first.
- **Ignore-rule mistakes** (template `build/` swallowing `Build/`; `*.uasset` accidentally uncommented). Mitigation: setup story reviews `.gitignore` against §2 and verifies `git status` shows `Content/**/*.uasset` as LFS-tracked before the first commit.
- **First LFS push exceeds host quota.** Mitigation: verify quota first; keep local-only until a host is chosen.
- **Sample assets resaved by the editor on checkout or redirector fix-up.** Mitigation: never-resave list + pre-commit check; no "fix up redirectors" or mass-save on sample folders.

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| game-concept.md | Technical Considerations / TD-FEASIBILITY: "No version control on a 3.7 GB agent-edited binary project — Git + LFS recommended (needs the user's go-ahead to commit)" | Options, recommendation, lock policy and interim workaround recorded; decision left to the user |
| (all GDDs) | Process prerequisite for every TR (architecture.md: ADR-009 covers no requirement text) | Enables the history, review and per-commit gates every TR's evidence relies on |
| coding-standards.md (project standard, not a GDD) | Commits reference the design document or task ID; Conventional Commits | §5 workflow conventions |

## Performance Implications
- **CPU / Memory / Load Time / Network (runtime)**: None — process decision.
- **Disk**: LFS local cache roughly doubles binary storage on the dev machine (working copy + LFS objects) [U exact]; snapshots grow until pruned by the user.
- **Workflow**: lock/unlock adds a step per asset edit.

## Migration Plan
On the user's decision (option A shown; B/C follow the same steps with their tools):
1. Review and fix `.gitignore` (§2; `git check-ignore` verification) and add LFS/lockable/text rules to `.gitattributes` (§3); create `production/never-resave-assets.md`.
2. `git init`, `git lfs install`, verify tracking with `git lfs ls-files` after staging; first commit of the current tree (user-instructed); suggested tag `baseline-2026-10-24`. Single-writer rule (§4) in force.
3. Optional remote: choose a host, verify LFS quota and locking-API support, push; then re-check-out the tree so `lockable` files become read-only, install the pre-commit lock check, and switch from the single-writer rule to locking.
4. Install/configure the editor provider (built-in Git plugin for status; community Git LFS plugin if editor locking is wanted [U]).
5. Wire the ADR-0008 per-commit gate as a pre-commit/pre-push hook and later CI.
6. Keep `_snapshots/` on disk until the user prunes them.

## Validation Criteria
1. `git status` on a fresh clone shows no `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, `_snapshots/` content; `git lfs ls-files` lists every `.uasset`/`.umap`.
2. (Once a locking-capable remote exists.) An unlocked `.uasset` is read-only in the working copy; `git lfs lock` makes it writable; a second lock attempt from another worktree fails; a commit changing an asset the committer has not locked is rejected by the pre-commit check.
3. The editor opens the project from a fresh clone and the prototype map plays (no missing assets).
4. A commit modifying an asset on the never-resave list is rejected.
5. The ADR-0008 per-commit gate runs from the hook.

## Related
- ADR-0007 (module skeleton = second commit), ADR-0008 (per-commit/nightly gates need a repository; `Build/` folder for UAT/Gauntlet scripts), ADR-0001 (acceptance waits on this), ADR-0024 (source-built engine for Dedicated Server needs its own VCS/engine-versioning decision).
- `docs/architecture/architecture.md` — Required ADRs bootstrap order; Open Question AQ-03.
- `design/gdd/game-concept.md` — Technical Considerations (TD-FEASIBILITY).
- `production/first-playable-report.md` — known issue 11; snapshot recovery.
- `production/session-state/active.md` — snapshot workaround and resave gotchas.
