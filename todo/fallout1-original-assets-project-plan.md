# Fallout 1 Original-Assets Runtime Plan

## Progress Update (April 17–18, 2026)

### Known open problems (bringup bugs)

| # | Problem | Status |
|---|---------|--------|
| 1 | Player sprite scrambled in FO1 mode (hmwarr not in CRITTERS.LST → critter 0 used) | **Fixed** — commit `d68790a` |
| 2 | Character editor crash (`karmaInit`/`genericReputationInit` return -1 on missing FO2-only files) | **Fixed** — commit `d68790a` |
| 3 | Combat AI crash: `aiInit()` fails on FO1 `ai.txt` (no `*_end` fields) → null `gAiPackets` → strlen crash in `_combatai_msg` | **Fixed** — commit `692cef5` |
| 4 | 5× "String not found" at startup — caliber type names 14–18 absent from FO1 `proto.msg` (cosmetic) | open |
| 5 | 2× "String not found" before `>intface_init` — interface indicator IDs 100–104 may differ in FO1 `intrface.msg` | open |
| 6 | "Only action is looking" — root cause TBD (cursor/action menu issue in FO1 mode) | open |
| 7 | Character creation UI: only "take" and "back" work on the selector | open |
| 8 | `Script Error: scripts\glowgen.int` — FO2 global script not applicable in FO1 mode | open |
| 9 | Unarmed hand/kick actions not shown in combat menu (unarmed.txt?) | skip for now |
| 10 | Exiting first map goes to empty map instead of world map | **In progress** — art shim deployed, needs test |
| 11 | Saving fails with "Error saving game" | **Fixed** — commit `4823bd3` |
| 12 | Pip-Boy fails with "Error" | **Fixed** — commit `62f4dae` |

### Completed implementation tasks

- `Task 1` variant/startup scaffolding
  - central `GameVariant` module
  - variant-aware startup title/map/movie/config selection
  - commit: `b5c4202`
- `Task 2` install-root/data-root path resolution
  - macOS `.app` install-root trimming and path anchoring in config loading
  - commit: `1e9b81d`
- startup alert logging improvement
  - "Could not find the master/critter datafile" now logs resolved paths before the popup
  - commit: `544eb92`
- FO1 archive backend bringup
  - imported `src/fo1db/*` and integrated FO1 backend into `dfile`
  - commit: `f264827`
- config policy alignment for FO1 mode
  - FO1 now uses `fallout2.cfg` / `f2_res.ini` (no FO1-specific config filenames)
  - commit: `cf6b3e7`
- FO1 startup stabilization at `proto_init`
  - added FO1 fallback perk name/description strings for missing FO2-only perk rows
  - commit: `a323a12`
- FO1 startup stabilization at worldmap init
  - missing `data\\worldmap.txt` now fails gracefully with debug output instead of crashing in subtile marking
  - commit: `34b3232`
- FO1 worldmap shim loading from game-folder mods
  - FO1 mode now falls back to `mods\\fo1_shims\\data\\worldmap.txt` (or `fo1_shims\\data\\worldmap.txt`) when `data\\worldmap.txt` is missing
  - parser-complete shim file added at `fo1_shims/data/worldmap.txt`
  - commit: `aa54707`
- FO1 worldmap shim upgraded to real data (FO1in2 reference)
  - `fo1_shims/data/worldmap.txt` replaced with the full FO1in2-derived worldmap dataset (4x5 tiles, FO1 terrain/chance/table topology)
  - FO1 map-reference parsing now gracefully skips `maps=` / `map_XX=` resolution when stock FO1 map lookup entries are unavailable, avoiding noisy `WorldMap Error: Couldn't find match for Map Index!` spam during bringup
  - worldmap tile-load popup paths now also emit debug log context before showing alerts
- FO1 map lookup shim import (FO1in2 reference)
  - added `fo1_shims/data/maps.txt` from FO1in2 so worldmap `maps=` / `map_XX=` entries resolve through normal CE map lookup flow
  - removed the temporary FO1 worldmap map-lookup skip path from runtime
  - verified clean startup through `wmWorldMap_init` with no map-index lookup spam and no tile-load popup
- FO1 city metadata shim import (FO1in2 reference)
  - added `fo1_shims/data/city.txt` from FO1in2 so FO1 area/town/entrance metadata is loaded from the shim mod
  - verified clean startup through `wmWorldMap_init` with FO1 `maps.txt` + `city.txt` + `worldmap.txt` active under `mods/fo1_shims`
  - no new area/map lookup parse errors observed during startup
- FO1 party metadata shim import (FO1in2 reference)
  - added `fo1_shims/data/party.txt` from FO1in2 to supply FO1-compatible party member option metadata
  - verified startup path remains clean through `wmWorldMap_init` and into main menu audio with `party.txt` active in `mods/fo1_shims`
  - no new `party` init parse/load errors observed in `debug.log`
- FO1 endgame death-table shim import (FO1in2 reference)
  - added `fo1_shims/data/enddeath.txt` from FO1in2 to provide FO1-compatible death-ending selection data
  - verified startup now reaches `>endgameDeathEndingInit` in FO1 mode without the previous `data\\enddeath.txt is missing` fallback log
  - startup continues normally into main menu audio with `enddeath.txt` active in `mods/fo1_shims`
- FO1 credits/quotes text shim import (FO1in2 reference)
  - added `fo1_shims/text/english/credits.txt` and `fo1_shims/text/english/quotes.txt` from FO1in2
  - placed under `text/english` to match localized path lookup used by `creditsOpen("credits.txt")` / `creditsOpen("quotes.txt")`
  - verified no startup regression with the new text shims active in `mods/fo1_shims`
- FO1 player art fix: alias TRIBAL critter slots to JUMPSUIT in `artInit()` when `hmwarr` absent
  - commit: `d68790a`
- FO1 character editor: treat missing `karmavar.txt`/`genrep.txt` as empty, not fatal
  - commit: `d68790a`
- FO1 combat AI: make `ai.txt` `*_end` fields optional, falling back to FO1-CE convention
  - commit: `692cef5`
- FO1 pip-boy: treat missing `holodisk.txt` as empty data, not fatal
  - commit: `62f4dae`
- FO1 save game: skip party member proto copy for protos that live only in the DAT archive
  - commit: `4823bd3`
- FO1 worldmap: deployed art shim (fo1_shims/art/intrface/) with:
  - FO1in2's extended INTRFACE.LST (508 entries, adds Twnmap/Wm_ city art at indices 469-492)
  - FO1in2's custom FO1 worldmap tiles (WRLDMP00-19, WRLDSPR0-2)
  - FO1in2's FO1 city town map images (TWNMAP00-11.FRM → indices 469-480)
  - FO1in2's FO1 worldmap city markers (Wm_00-11.FRM → indices 481-492)
  - FO2 worldmap UI FRMs (WMAPBOX, WMSCREEN, WMTABS, WMDIAL, etc. → extracted from FO2 master.dat)
  - Root cause: `wmInterfaceInit()` failed at FRM 136 (WMAPBOX.FRM) because FO1's INTRFACE.LST has < 136 entries and no worldmap art

## FO1in2 Findings

The FO1in2 project (`../fo1in2/`) provides the reference implementation. Key observations:

- FO1in2 provides a **custom INTRFACE.LST** in `mods/fo1_base/art/intrface/INTRFACE.LST` (508 entries) that extends FO2's standard list (468 entries) with FO1-specific city art at indices 469-492
- FO1in2 provides **FO1-specific worldmap tiles** (WRLDMP*.FRM) — different art from FO2's tiles, depicting FO1's California
- FO1in2 provides **FO1 city town maps** (TWNMAP00-11.FRM) and **worldmap city markers** (Wm_00-11.FRM)
- FO1in2 relies on FO2 master.dat for core worldmap UI FRMs (WMAPBOX.FRM, WMSCREEN.FRM, WMTABS.FRM, etc.)
- FO1in2's `worldmap.txt`, `city.txt`, `maps.txt`, `party.txt`, `enddeath.txt` are already in our shims
- **Plan remains sound**: the fo1_shims directory-based mod overlay approach correctly mirrors FO1in2's asset layering

Current status after FO1 DAT integration:

- FO1 run with explicit FO1 DAT paths no longer fails at master/critter archive open.
- Bringup now advances past `wmWorldMap_init` and `endgameDeathEndingInit`, reaches main-menu background audio load (`07desert.acm`), and resolves FO1 shim worldmap/map references without map-index parse spam.

### ASan Bringup Workflow (macOS, preferred)

Reason:

- ASan gives deterministic crash diagnostics in terminal/debug logs and avoids relying on macOS crash-reopen dialogs.

Build:

- `CC=clang CXX=clang++ cmake -S . -B out/build/local-asan-arm64 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON -DFALLOUT_VENDORED=OFF -DCMAKE_PREFIX_PATH=/opt/homebrew -DCMAKE_OSX_ARCHITECTURES=arm64`
- `cmake --build out/build/local-asan-arm64 -j 8`

Refresh test binary in FO1 folder:

- `cp ~/game/fallout2-ce/out/build/local-asan-arm64/Fallout\ II\ Community\ Edition.app/Contents/MacOS/Fallout\ II\ Community\ Edition /Users/klaas/Documents/f2stuff/Fallout1/Fallout\ II\ Community\ Edition.app/Contents/MacOS/Fallout\ II\ Community\ Edition`

Run (from FO1 folder):

- `cd /Users/klaas/Documents/f2stuff/Fallout1`
- `./Fallout\ II\ Community\ Edition.app/Contents/MacOS/Fallout\ II\ Community\ Edition --game=fo1 '[system]master_dat=MASTER.DAT' '[system]master_patches=DATA' '[system]critter_dat=CRITTER.DAT' '[system]critter_patches=DATA' '[debug]mode=log'`

Operational notes:

- keep using `fallout2.cfg` in the game folder (current policy)
- stop runs with `Ctrl-C` (or `kill <pid>`) to avoid stale processes and macOS reopen prompts
- `--scan-unimplemented` currently reports FO1 DATs as non-database (FO2-format validator path), so use normal startup runs + `debug.log` for FO1 bringup checkpoints

## Objective

Run original Fallout 1 content in this codebase using only original Fallout 1 assets:

- allowed:
  - this repo as the engine base
  - reference code from `../fallout1-ce`
  - original Fallout 1 install assets from `/Users/klaas/Documents/f2stuff/Fallout1`
- not allowed for the target end state:
  - Fallout 2 assets from `/Applications/Fallout2Codex`
  - `Fo1in2` runtime overlay/content as a dependency
  - `mods/fo1_base` or any other compatibility layer derived from FO2-shaped content

This is stricter than the earlier `fallout1-runtime-bringup` branch. That branch proved a hybrid approach can work, but it also proved that "boots with FO1 DAT support" is not the same as "runs stock Fallout 1 assets only".

## Executive Summary

The project is viable, but only if it is treated as a staged compatibility program, not a simple boot-option patch.

The prior branch already demonstrated three useful facts:

1. A `GameVariant` layer is the right shape.
2. Native Fallout 1 DAT support is required.
3. The first bringup became playable only after mounting `mods/fo1_base`, which means stock FO1 assets still do not satisfy current CE runtime assumptions.

The practical implication is:

- short term: recover the reusable runtime infrastructure from `fallout1-runtime-bringup`
- medium term: remove the dependency on `mods/fo1_base` by replacing FO2-shaped assumptions with true FO1-compatible loading and behavior
- long term: keep one codebase with explicit FO1/FO2 variant handling and separate validation for each

## Current Evidence

### Current `main` branch status

The current branch is still FO2-hardcoded in several key places:

- [src/main.cc](/Users/klaas/game/fallout2-ce/src/main.cc) hardcodes:
  - startup map `artemple.map`
  - intro credits movie `MOVIE_CREDITS`
  - new-game movie `MOVIE_ELDER`
- [src/game_config.cc](/Users/klaas/game/fallout2-ce/src/game_config.cc) hardcodes:
  - default config `fallout2.cfg`
  - `f2_res.ini` migration flow
- [src/game.cc](/Users/klaas/game/fallout2-ce/src/game.cc) hardcodes:
  - global vars bootstrap from `data\\vault13.gam`
  - optional loading of `f2_res.dat`
- [src/xfile.cc](/Users/klaas/game/fallout2-ce/src/xfile.cc) still uses the normal FO2-first xbase search order

### What the old `fallout1-runtime-bringup` branch already solved

The old branch added:

- runtime variant selection via `--game=fo1|fo2`
- variant-aware title/config/starting-map/movie gating
- macOS install-root resolution for `.app` inside a game folder
- Fallout 1 DAT support by importing an FO1 database backend from `../fallout1-ce`
- FO1-mode xbase changes so loose root files and mods are visible before DAT contents

Relevant old-branch files:

- `src/game_variant.cc`
- `src/fo1db/*`
- `src/dfile.cc`
- `src/xfile.cc`
- `todo/fallout1-bringup-log.md`

### What the old branch did not solve

The old branch also logged that it only became meaningfully viable after loading:

- `mods/mods_order.txt`
- `mods/fo1_base`

That is disqualifying for the target end state here.

The startup logs from that branch show:

- `xfileOpen: request path=mods/mods_order.txt`
- `Loading mod mods/fo1_base`

The same branch notes that `fo1_base` supplied compatibility content such as:

- `scripts/SCRIPTS.LST`
- `data/WORLDMAP.TXT`
- `data/Vault13.gam`
- `premade/player.gcd`
- `proto/*/*.LST`
- `art/*/*.LST`
- `text/english/game/*.msg`

That is the clearest evidence that current CE still expects a number of FO2-shaped content contracts that stock FO1 does not meet directly.

## Non-Negotiable Constraint

The current local Fallout 1 folder is not a pristine stock validation target anymore. It already contains:

- `mods/fo1_base`
- compatibility files under `mods/`
- an app bundle and debug artifacts

Therefore the first real task is to create or document a clean validation environment.

Current observed state of `/Users/klaas/Documents/f2stuff/Fallout1`:

- original FO1 core archives are present:
  - `MASTER.DAT`
  - `CRITTER.DAT`
- there is also a loose `DATA/` tree with at least:
  - `DATA/MAPS/*.edg`
  - `DATA/SCRIPTS/*.INT`
  - `DATA/TEXT/ENGLISH/...`
  - small loose `DATA/art/...` additions
- the folder still contains non-original local/runtime artifacts:
  - `Fallout II Community Edition.app`
  - `ce_startup.log`
  - `debug.log`
  - `mods/mods_order.txt`
  - high-resolution patch files such as `f1_res.dll`, `f1_res_Config.exe`, `ddraw.dll`
  - local configs including `fallout.cfg` and `f2_res.ini`

Practical implication:

- this folder is now clean enough to investigate FO1 content presence more reliably than before
- but it is still not a strict "untouched original install" baseline
- the bringup notes should distinguish:
  - original FO1 content
  - optional GOG/installer extras
  - local CE/runtime artifacts
  - high-resolution patch overlays

## Recommended Strategy

Do not try to merge `../fallout1-ce` wholesale into this repo.

Do not try to productize the old branch as-is.

Recommended strategy:

1. Reintroduce only the reusable runtime infrastructure from `fallout1-runtime-bringup`.
2. Establish a clean stock-FO1 validation harness.
3. Remove each remaining compatibility overlay dependency by replacing it with the smallest defensible mechanism:
   - direct use of stock FO1 assets where possible
   - small shipped or generated FO1 bridge assets where that avoids large engine forks
   - engine-side FO1 behavior only where data bridging is not sufficient
4. Keep every change behind an explicit `GameVariant::Fallout1` or equivalent capability flag.
5. Treat FO2 compatibility as the regression boundary for every phase.

### Code Organization Bias: Separate FO1 Modules, Thin Integration Points

The preferred end state is not just "FO1 works", but "FO1 support is easy to reason about and does not sprawl through the FO2 engine".

Preferred implementation style:

- keep shared CE subsystems intact where raw container/layout compatibility already exists
- move FO1-specific policy and behavior into dedicated `fo1_*` modules
- call those modules from a few narrow integration points at subsystem boundaries
- avoid deep scattered `if (isFallout1())` checks in generic gameplay loops unless the decision truly belongs there

Good shapes:

- variant-owned providers/helpers for:
  - bootstrap file selection
  - startup flow
  - worldmap metadata
  - Pip-Boy quest/holodisk metadata
  - timer/story-state rules
  - endgame slide selection
  - trait/perk rule tables
- shared parser/loader code with variant-owned policy hooks
- explicit subsystem-entry dispatch instead of low-level branching everywhere

Bad shapes:

- many tiny inline variant checks spread through combat/stat/item/map code
- forcing FO1 semantics through FO2-native metadata structures when the meaning is different
- whole-file forks of CE subsystems before targeted hook points are exhausted

### Preferred Balance: Bridge Assets vs Engine Forks

The target is not "no bridge assets under any circumstances". The target is:

- no FO2 asset dependency
- no large compatibility mod as a hidden runtime requirement
- as few FO1 bridge assets as possible

Decision bias:

- prefer stock FO1 assets if CE can already consume them
- prefer a minimal bridge asset if it is declarative, stable, and lets CE reuse existing infrastructure
- prefer engine work only when the mismatch is genuinely behavioral or ABI-level

Working examples:

- `data\\worldmap.txt`
  - acceptable as a shipped or generated FO1 bridge asset if derived from FO1 semantics and it avoids a large worldmap-engine fork
- `data\\party.txt`
  - acceptable as a minimal bootstrap bridge asset if it starts with mostly generic/default companion behavior and does not force heavy FO1-specific party code too early
- `credits.txt`
  - candidate if FO1 distribution lookup remains awkward; FO1 itself expects this file name, so a tiny shipped copy or extraction/import step may be lower risk than more path special-casing
- `quotes.txt`
  - same logic as `credits.txt`; treat as a distribution-path problem first and a bridge asset second
- a very small `text/english/game/*.msg` compatibility patch layer
  - candidate only for specific CE-required entries/files that stock FO1 does not provide in the expected place or numbering
  - should stay surgical; do not let this turn into a broad text replacement overlay
- script proc table differences
  - not a good bridge-asset candidate; this is runtime/ABI work
- savefile layout differences
  - not a bridge-asset problem; FO1 original save compatibility stays out of scope

## Non-Goals

These items should be treated as explicitly out of scope unless the project is deliberately expanded later:

- original FO1 savefile compatibility
- shipping `Fo1in2`, `mods/fo1_base`, or any large FO2-derived compatibility mod as a runtime dependency
- byte-identical FO1 engine internals where CE already has a safe shared runtime model
- broad save/load ABI unification between FO1 and FO2
- fixing every FO1-vs-FO2 gameplay difference before first playable bringup

Explicitly allowed if kept small and justified:

- a tiny set of FO1 bridge assets that let CE satisfy missing FO2-era data-driven surfaces without pulling in FO2 assets

This matters because several subsystems have enough drift that "could be made compatible later" should not quietly become "must be solved before boot".

## Pre-Start Decisions To Lock

Before implementation starts, these decisions should be made explicit so the bringup does not wander:

### 1. Variant selection model

Decide whether FO1 support is entered by:

- explicit `--game=fo1`
- install autodetection
- or autodetection with explicit override

Recommended:

- keep explicit `--game=fo1` during bringup
- consider autodetection only after the runtime is stable

### 2. Variant API shape

Decide whether code should branch on:

- a raw `GameVariant` enum everywhere
- or capability-style helpers such as:
  - `isFallout1()`
  - `getConfigBasename()`
  - `getStartupMapName()`
  - `supportsPartyTxt()`
  - `getMovieDescriptor(...)`

Recommended:

- use a small central variant/capability layer
- do not spread filename and content decisions across unrelated subsystems

### 3. Initial FO1 acceptance target

Decide what counts as "started successfully".

Recommended first success bar:

- boot from a pristine FO1 install
- show the main menu
- start a new game
- enter the first playable map from stock FO1 assets only

Do not set the first milestone to "worldmap/combat/party are all correct".

### 4. Save UX policy

The doc already treats FO1 save compatibility as unsupported, but the user-facing behavior should be defined early:

- detect unsupported FO1 save
- show a specific error
- do not attempt partial load

Recommended:

- make the error mention original FO1 saves directly
- keep the message distinct from generic corruption/unsupported-version failures

## Pre-Implementation Verification Tasks

These are one-time investigations that should be finished before the first code port starts, because they reduce thrash later.

### Stock FO1 asset inventory

Build a concrete inventory from a pristine FO1 install answering:

- does stock FO1 actually contain:
  - `data\\vault13.gam`
  - `data\\worldmap.txt`
  - `scripts\\scripts.lst`
  - `proto\\*\\*.lst`
  - `art\\*\\*.lst`
  - `credits.txt`
  - `quotes.txt`
  - premade `.gcd` files
- if yes:
  - where
  - in which archive or loose directory
  - with what path casing
- if no:
  - whether CE should generate/index data at runtime
  - or whether a true FO1-native code path should bypass the expectation entirely

This should produce a short stock-asset truth table in the bringup log.

Current findings from the local FO1 folder:

- not found as loose files:
  - `data\\vault13.gam`
  - `data\\worldmap.txt`
  - `scripts\\scripts.lst`
  - `credits.txt`
  - `quotes.txt`
  - premade `.gcd` files
  - loose proto/art `.lst` index files under `DATA/`
- found as loose files:
  - compiled scripts under `DATA/SCRIPTS/*.INT`
  - text resources under `DATA/TEXT/ENGLISH/...`
  - at least one additional text resource under `text/english/game/PERK.msg`
  - `.edg` map-side files under `DATA/MAPS/*.edg`
  - a small loose art overlay under `DATA/art/...`
- inconclusive from the current tooling:
  - the local `./.local-tools/fallout2-dat` utility cannot open stock FO1 DATs, which reinforces the need for a dedicated FO1 archive backend

Implication:

- several CE assumptions are definitely not satisfied by loose files alone in this install
- Phase 2 should assume real work is needed around archive access and/or FO1-native indexing/bootstrap behavior

Cross-check against `../fallout1-ce` source:

- FO1 itself also expects several of these same paths:
  - `data\\vault13.gam`
  - `fallout.cfg`
  - `f1_res.ini`
  - startup map `V13Ent.map`
  - `credits.txt`
  - `quotes.txt`
- FO1 source also depends on the same list-file families:
  - `proto\\<type>\\<type>.lst`
  - `art\\<type>\\<type>.lst`
  - `scripts\\scripts.lst`

Meaning:

- the presence of `.lst` dependencies is not, by itself, evidence of an FO2-only assumption
- the more precise question is:
  - can CE FO1 mode access those stock FO1 resources from FO1 archives correctly
  - and where does CE still impose FO2-era semantics after the files are found

Archive verification using a local FO1 DB-based inspector:

- confirmed present in stock FO1 archives:
  - `SCRIPTS\\SCRIPTS.LST`
  - `DATA\\VAULT13.GAM`
  - `PROTO\\ITEMS\\ITEMS.LST`
  - `ART\\INTRFACE\\INTRFACE.LST`
  - `ART\\CRITTERS\\CRITTERS.LST`
  - premade characters under `PREMADE\\*.GCD`, including `PREMADE\\PLAYER.GCD`
- confirmed absent from `MASTER.DAT`:
  - `DATA\\WORLDMAP.TXT`
  - `DATA\\PARTY.TXT`
  - `CREDITS.TXT`
  - `QUOTES.TXT`
- confirmed present in `MASTER.DAT` under `DATA\\*.TXT`:
  - `DATA\\AI.TXT`
  - `DATA\\BADWORDS.TXT`

Refined implication:

- `.lst` files and `vault13.gam` are not the missing-content problem; CE should be able to consume stock FO1 versions once FO1 archive access is correctly wired through normal file-open paths
- `worldmap.txt` and `party.txt` are different:
  - they appear to be real CE/FO2-era assumptions, not merely missing loose files
  - `../fallout1-ce` does not load either file
- `credits.txt` / `quotes.txt` need a separate distribution-path decision because FO1 source expects them, but they are not in `MASTER.DAT`

### Clean-vs-contaminated environment diff

Record the exact difference between:

- a pristine FO1 folder
- the currently contaminated local FO1 folder

That prevents later "works on my machine" confusion caused by leftover compatibility files.

### Old-branch delta classification

Before porting from `fallout1-runtime-bringup`, classify each old-branch change as one of:

- required runtime infrastructure
- useful but temporary bringup instrumentation
- unacceptable overlay dependency
- unrelated drift

This is a better pre-port filter than replaying commits mechanically.

## Architecture Guardrails

These constraints should keep the implementation from turning into scattered FO1 special cases.

### Centralize variant-owned content decisions

Keep these in one place rather than hardcoding them in multiple subsystems:

- config filename
- hi-res config filename
- startup map
- movie sequencing
- global-vars bootstrap source
- save root / save compatibility policy
- optional archive loading such as `f2_res.dat`

### Prefer capability checks over ad hoc `if (fo1)`

Recommended pattern:

- one high-level variant enum
- a small set of helper queries for content/rules differences

Avoid:

- duplicating file names, enum bounds, or proc-count assumptions in many subsystems

### Separate loader adaptation from rules adaptation

Keep these as separate workstreams:

- loader adaptation:
  - file resolution
  - archive support
  - enum/defaulting during deserialization
- rules adaptation:
  - combat behavior
  - worldmap semantics
  - party behavior
  - movie flow

That separation will make it much easier to know whether a failure is "cannot load content" or "loaded content but behaves wrong".

## Actual CE Startup Blocking Order

CE's initialization order matters for planning, because some FO1 mismatches are immediate boot blockers, not later gameplay work.

Observed order in [src/game.cc](/Users/klaas/game/fallout2-ce/src/game.cc):

1. `partyMembersInit()`
2. `gameMoviesInit()`
3. `protoInit()`
4. `scriptsInit()`
5. `gameLoadGlobalVars()`
6. `_scr_game_init()`
7. `wmWorldMap_init()`
8. later systems such as dialog/combat/automap

Implications:

- `data\\party.txt` is not merely a later party-feature mismatch
  - in current CE it can block startup before main menu/new-game bringup stabilizes
- `proto\\...\\*.lst` and `art\\...\\*.lst` are early boot blockers because `protoInit()` depends on them and `mapInit()` pulls in `artInit()`
- `scripts\\scripts.lst` is an early boot blocker because `scriptsInit()` counts and loads it before game-mode script setup
- `data\\vault13.gam` and CE's current `data\\worldmap.txt` expectation are also startup blockers in current sequencing, not deferred gameplay problems

Planning consequence:

- early FO1 work should be organized around actual init order, not just conceptual subsystem order
- `party.txt` deserves earlier treatment in the plan than a normal "later gameplay correctness" item

## Main Technical Gaps

### 0. Schema and binary-layout compatibility

This is the subtle part that sits underneath the pathing work.

After comparing `fallout1-ce` against this repo, the main pattern is:

- many core structs are still shape-compatible enough for a shared in-memory runtime
- but several serialized formats and enum spaces are not cleanly interchangeable
- therefore "load FO1 files into FO2 CE structs" is only safe if each format is audited field-by-field

Key findings:

- `CritterProtoData` is not identical:
  - FO1: flags, `baseStats[35]`, `bonusStats[35]`, `skills[18]`, `bodyType`, `experience`, `killType`
  - FO2 CE adds `damageType`
  - files:
    - [src/proto_types.h](/Users/klaas/game/fallout2-ce/src/proto_types.h)
    - `../fallout1-ce/src/game/proto_types.h`
- `SceneryProto*` semantics are not identical:
  - FO2 CE names and uses ladder/stairs data more explicitly
  - `SceneryProtoLadderData` and related object-side scenery data differ in meaning
- proto enum spaces are wider in FO2 CE:
  - more calibers
  - more kill types
  - more hardcoded proto ids
  - `SFALL_KILL_TYPE_COUNT` extension
- script proc surface is wider in FO2 CE:
  - FO1 has 24 script procs
  - FO2 CE has 28, including push/dropping/combat-start/combat-over
  - files:
    - [src/scripts.h](/Users/klaas/game/fallout2-ce/src/scripts.h)
    - `../fallout1-ce/src/game/scripts.h`
- game-movie enum space is completely different:
  - FO1 and FO2 do not share the same movie table shape
  - files:
    - [src/game_movie.h](/Users/klaas/game/fallout2-ce/src/game_movie.h)
    - `../fallout1-ce/src/game/gmovie.h`
- perk enum space is much wider in FO2 CE:
  - stats and skills are mostly stable
  - traits are basically the same set
  - perks are not
  - files:
    - [src/perk_defs.h](/Users/klaas/game/fallout2-ce/src/perk_defs.h)
    - `../fallout1-ce/src/game/perk_defs.h`
- object runtime layout is mostly compatible, but semantics drifted:
  - FO2 CE added clearer meanings like `scriptIndex`
  - ladder/misc/object flag semantics widened
  - object flags and proto flags gained more meanings
- animation enum count is not the main incompatibility:
  - base animation type space is broadly shared
  - the bigger difference is runtime behavior, capacity, and content expectations
  - FO2 CE animation engine has larger sequence/description pools and extra sequence kinds

Practical conclusion:

- the biggest risk is not C++ source compatibility
- the biggest risk is incorrect interpretation of serialized FO1 content using FO2-era assumptions

Recommended design rule:

- keep one canonical in-memory runtime model in CE
- add variant-aware loaders/upconverters for every on-disk format that differs
- never rely on "structs happen to be close enough" unless the read/write path was verified field-by-field

### 1. Product identity and startup flow

This is the easiest gap and should be done first.

Needed work:

- introduce a central `GameVariant` API
- make startup title variant-aware
- make config filename variant-aware:
  - FO1: `fallout.cfg`
  - FO2: `fallout2.cfg`
- make hi-res config filename variant-aware:
  - FO1: `f1_res.ini`
  - FO2: `f2_res.ini`
- make startup map variant-aware:
  - FO1: `V13Ent.map`
  - FO2: `artemple.map`
- gate intro/new-game movies by variant
- gate optional `f2_res.dat` loading by variant

Primary source hotspots:

- [src/main.cc](/Users/klaas/game/fallout2-ce/src/main.cc)
- [src/game_config.cc](/Users/klaas/game/fallout2-ce/src/game_config.cc)
- [src/game.cc](/Users/klaas/game/fallout2-ce/src/game.cc)
- [src/sfall_ini.cc](/Users/klaas/game/fallout2-ce/src/sfall_ini.cc)

### 2. Install-root and data-root resolution

The old branch found a real macOS-specific problem: launching from inside the `.app` bundle pointed config/data resolution at `Contents/MacOS` instead of the enclosing game folder.

Needed work:

- recover the old install-root resolution logic
- ensure relative config/data paths are resolved against the game folder, not the executable subfolder
- verify this on macOS specifically, since that is the local workflow here

Primary source hotspots:

- `fallout1-runtime-bringup:src/game_variant.cc`
- [src/game_config.cc](/Users/klaas/game/fallout2-ce/src/game_config.cc)
- [src/settings.cc](/Users/klaas/game/fallout2-ce/src/settings.cc)

### 3. Native FO1 archive support

This is mandatory. The old logs already showed stock FO1 `MASTER.DAT` was rejected by current CE's DAT parser until the FO1 backend was imported.

Needed work:

- recover the FO1 DB backend from the old branch
- keep FO2 DAT loading unchanged
- verify archive lookup behavior against actual stock FO1 DAT contents
- remove old bringup-only startup logging once stable

Primary source hotspots:

- `fallout1-runtime-bringup:src/fo1db/*`
- `fallout1-runtime-bringup:src/dfile.cc`
- [src/dfile.h](/Users/klaas/game/fallout2-ce/src/dfile.h)

### 4. Loader assumptions that currently depend on overlay content

This is the real project.

The old branch's dependency on `fo1_base` strongly suggests that CE still assumes one or more of the following contracts:

- FO2-style loose files exist where stock FO1 keeps data in DATs or in different locations
- file names exist but are loaded from different paths
- message/proto/script indexes differ in format or location
- startup bootstrap files are named differently or expected in the wrong place

Known CE assumptions to audit:

- [src/game.cc](/Users/klaas/game/fallout2-ce/src/game.cc): loads global vars from `data\\vault13.gam`
- [src/worldmap.cc](/Users/klaas/game/fallout2-ce/src/worldmap.cc): loads `data\\worldmap.txt`
- [src/scripts.cc](/Users/klaas/game/fallout2-ce/src/scripts.cc): loads `scripts\\scripts.lst`
- [src/proto.cc](/Users/klaas/game/fallout2-ce/src/proto.cc): loads `proto\\<type>\\<type>.lst`
- [src/party_member.cc](/Users/klaas/game/fallout2-ce/src/party_member.cc): loads `data\\party.txt`
- [src/main.cc](/Users/klaas/game/fallout2-ce/src/main.cc): opens `credits.txt` and `quotes.txt`

Expected outcome:

- each of these paths either resolves directly from stock FO1 assets
- or gets a variant-aware FO1 path/data source without importing replacement content from `Fo1in2`

Additional requirement:

- every affected loader must be audited for schema assumptions, not just path assumptions

Concrete examples:

- proto readers/writers must decide whether FO1 and FO2 share the same serialized field order
- script metadata readers must not assume FO2 proc-table width when consuming FO1-era content
- movie/gameflow code must use variant-specific symbolic movie ids instead of shared numeric ids

Refined interpretation after comparing with `fallout1-ce`:

- some CE path assumptions are genuinely FO1-native and probably should stay:
  - `data\\vault13.gam`
  - `proto\\...\\*.lst`
  - `art\\...\\*.lst`
  - `scripts\\scripts.lst`
- therefore these should not be prematurely classified as "generate replacements" problems
- first classify them as:
  - stock FO1 path/resource likely exists but CE cannot reach it yet in FO1 mode
  - stock FO1 path/resource is found, but CE interprets it with FO2-era semantics
  - stock FO1 resource genuinely does not exist in the user distribution and needs a fallback strategy

Confirmed examples after archive inspection:

- stock FO1 path/resource exists in archives:
  - `data\\vault13.gam`
  - `scripts\\scripts.lst`
  - `proto\\...\\*.lst`
  - `art\\...\\*.lst`
- CE-side resource assumption appears not to exist in stock FO1 archives:
  - `data\\worldmap.txt`
  - `data\\party.txt`
- stock FO1 distribution-path question remains open:
  - `credits.txt`
  - `quotes.txt`

### 5. Search order and overlay precedence

The old branch changed FO1-mode xbase ordering to prefer loose root files before DAT archives. That made mod overlays work, but it also blurred stock-vs-overlay validation.

Needed work:

- recover only the portion of xbase behavior that is necessary for stock FO1
- avoid silently depending on `mods/`
- decide whether FO1 mode should mount the install root `.` at all
- if root mounting remains necessary, test it first with a pristine install and no mods present

Primary source hotspots:

- [src/xfile.cc](/Users/klaas/game/fallout2-ce/src/xfile.cc)
- `fallout1-runtime-bringup:src/xfile.cc`

### 6. Gameplay and rules divergence

Once the stock assets load, FO1 still differs from FO2 in behavior. `Fo1in2` is still useful here as an inventory of mismatches, but not as runtime content.

Known likely differences:

- starting time and date
- endgame movie/credits sequencing
- world map travel timing
- some perk/item/stat behavior
- party handling
- FO1-specific encounter/world map behavior
- dude art/animation selection

Important nuance:

- some subsystems in this category also have startup-facing configuration surfaces in CE
- for example `partyMembersInit()` currently makes `data\\party.txt` an initialization-time concern, even though party behavior itself is a later gameplay topic

This work should be done only after the stock data path is stable enough to reach main menu, new game, and first map reliably, but startup-facing blockers inside these systems should be handled earlier.

Sources to mine:

- `../Fo1in2/ddraw.ini`
- `../Fo1in2/config/fo1_settings.ini`
- `../Fo1in2/Mapper/source/scripts/GlobalScripts/gl_fo1mechanics.ssl`
- `../fallout1-ce` for native behavior reference

### 7. Save/load separation

FO1 and FO2 must not share ambiguous save metadata or slot assumptions.

Needed work:

- separate save roots, or
- variant-tag save metadata and block cross-loads
- detect unsupported FO1 savefiles and show a relevant error

Important scope decision:

- FO1 savefile compatibility is not desired
- the goal is runtime support for starting and playing FO1 content from original assets
- if an original FO1 save is selected, CE should reject it cleanly rather than attempt compatibility

This is not the first blocker, but the rejection path should exist before calling the feature usable.

## Proposed Execution Plan

### Phase 0: Establish a clean validation harness

Goal:

- make "original assets only" testable and repeatable

Tasks:

- create a pristine FO1 test folder with no `mods/`, no `fo1_base`, and no borrowed FO2 files
- document the exact folder contents
- record expected original asset layout:
  - `MASTER.DAT`
  - `CRITTER.DAT`
  - `DATA/`
- define one canonical local launch workflow for FO1 bringup
- decide whether `--game=fo1` is required initially or only for debugging

Acceptance criteria:

- one documented stock FO1 test folder exists
- one documented "contaminated compatibility folder" exists for comparison only
- a short stock-asset truth table exists for the files CE currently expects
- the launch command and working directory are fixed and documented

### Phase 1: Recover the infrastructure spike from the old branch

Goal:

- restore the minimal runtime plumbing needed to boot stock FO1 assets at all

Tasks:

- port `GameVariant` support from `fallout1-runtime-bringup`
- port install-root resolution
- port config/hi-res filename selection
- port startup map/movie gating
- port FO1 DAT support from `src/fo1db/*` and associated `dfile` integration
- port only the minimum `game.cc` changes needed for variant-aware boot

Important rule:

- do not port any `mods/fo1_base` loading dependency as part of success criteria

Acceptance criteria:

- the engine launches in FO1 mode from a pristine FO1 folder
- stock FO1 `MASTER.DAT` and `CRITTER.DAT` open successfully
- main menu appears without relying on `mods/fo1_base`
- the run log is clean enough to distinguish missing-content failures from variant-selection/path failures
- the next failing init step is identified by subsystem name, not just by a generic startup failure

### Phase 2: Reach new game without compatibility overlays

Goal:

- make stock FO1 content load through character selection into new-game startup

Tasks:

- audit all file-open failures from startup through new game
- replace FO2-hardcoded bootstrap paths with variant-aware FO1 equivalents
- resolve missing startup assets and text resources without copying Fo1in2 content
- audit schema-sensitive readers in parallel with file-open failures
- verify whether:
  - `credits.txt`
  - `quotes.txt`
  are present elsewhere in the FO1 distribution, or need variant-specific handling
  - `worldmap.txt`
  - `party.txt`
  need true FO1-native code paths rather than archive lookups

Acceptance criteria:

- main menu works
- new game starts
- first playable map loads from stock FO1 assets only
- no known loader is still depending on an FO2-era struct width or enum count by accident
- all missing-file failures seen up to first-map load are classified as:
  - wrong path
  - wrong format expectation
  - true missing stock asset
  - runtime behavior dependency
- init-time blockers are distinguished from later gameplay mismatches

### Phase 3: Replace overlay-shaped contracts with FO1-native contracts

Goal:

- remove the need for FO2-shaped compatibility assumptions

Tasks:

- audit every file previously satisfied by `fo1_base`
- categorize each one:
  - stock FO1 file exists, wrong path handling in CE
  - stock FO1 file exists, wrong format expectation in CE
  - CE expects an FO2-only bootstrap artifact and needs a true FO1 code path
- create a second categorization for schema differences:
  - same bytes, different enum/value interpretation
  - extra FO2 field that needs defaulting when loading FO1
  - FO1-only behavior encoded outside the serialized format and needing runtime rules
- implement FO1-native path/format handling in CE
- avoid checking in converted content derived from `Fo1in2`

Suggested tracking table columns:

- path expected by CE
- stock FO1 source of truth
- current failure mode
- schema risk
- owner file/function
- fix type
- validation status

Acceptance criteria:

- the project can enumerate the complete set of former overlay dependencies
- each dependency is either removed or replaced with a native FO1 code path
- any remaining unresolved dependency is tracked as a specific blocker, not a vague "needs more FO1 compatibility"

### Phase 4: Gameplay correctness pass

Goal:

- make the runtime behave like Fallout 1, not merely load Fallout 1 assets

Tasks:

- compare gameplay behavior against `../fallout1-ce` and original Fallout 1
- audit time, perks, combat, world map, endgame, and party behavior
- use `Fo1in2` only as a mismatch inventory, not as a shipped dependency
- add focused diagnostics where runtime behavior is ambiguous

Acceptance criteria:

- early-game progression is playable
- obvious FO2 behavior leaks are triaged and tracked
- known FO1-vs-FO2 rules differences are documented
- the top remaining behavior mismatches are ranked by player impact

### Phase 5: Secondary UX and FO1 flavor restoration

Goal:

- restore FO1-specific informational and presentation behavior that is visible to players but not required for core progression

Tasks:

- Pip-Boy countdown / "time left" presentation
- Pip-Boy status wording/grouping and related non-blocking journal differences
- "Tell me about" or equivalent informational/help flows
- credits/quotes/menu and archive presentation polish
- other intentionally deferred FO1-specific UI/text flavor differences

Acceptance criteria:

- deferred FO1 flavor work is tracked as explicit items, not mixed into core-runtime blocking work
- secondary UX items have a ranked backlog and clear ownership

### Phase 6: Regression protection and packaging

Goal:

- make FO1 support maintainable without destabilizing FO2

Tasks:

- add startup logging for variant selection and key resources, then trim it down
- add variant-specific smoke-test workflows
- isolate save data between FO1 and FO2
- detect unsupported FO1 save files and display a clear message instead of attempting load
- decide whether to ship:
  - one binary with autodetection, or
  - two product targets from one source tree

Acceptance criteria:

- FO1 and FO2 both boot from their own stock folders
- save/load separation is safe
- unsupported FO1 saves fail fast and clearly
- the support model is documented

## Bringup Checkpoints

These are the concrete checkpoints worth using during implementation.

1. Variant selection works and logs the chosen mode clearly.
2. FO1 install root resolves correctly from the `.app` bundle working layout.
3. FO1 `MASTER.DAT` and `CRITTER.DAT` mount successfully.
4. The first post-archive init blocker is identified among:
   - `partyMembersInit`
   - `protoInit`
   - `scriptsInit`
   - `gameLoadGlobalVars`
   - `wmWorldMap_init`
5. Main menu renders using stock FO1 assets only.
6. New game path enters FO1 startup flow and lands on the first map.
7. First map scripts execute without crashing due to proc-table mismatch.
8. Worldmap transition works at least once in FO1 mode.
9. Early combat encounter works without obvious out-of-range enum/table issues.

These checkpoints are intentionally ordered so failures can be localized quickly.

## Open Questions To Resolve Early

These are not blockers to writing the plan, but they should be answered before implementation gets far:

- Does stock FO1 provide all `.lst` index files CE currently expects in `MASTER.DAT` / `CRITTER.DAT`, and can the imported FO1 DB backend expose them through CE's normal file-open paths?
- Can the imported FO1 DB backend expose confirmed stock resources like `SCRIPTS\\SCRIPTS.LST` and `DATA\\VAULT13.GAM` through CE's ordinary `fileOpen` / `xfileOpen` callers without overlay support?
- For missing FO2-era data-driven surfaces, which are best handled by tiny bridge assets instead of engine code?
  - likely first candidates: `data\\worldmap.txt`, `data\\party.txt`
  - likely non-candidates: script proc ABI, save/load layout
- Is `data\\party.txt` best handled by:
  - a tiny shipped/generated FO1 bridge file
  - a tiny built-in FO1 descriptor table
  - or a more explicit FO1-specific party code path?
- Should FO1 support ever write CE-side FO1-mode saves, or should FO1 mode initially be load-disabled / save-disabled entirely?
- Is variant autodetection worth supporting, or is explicit `--game=fo1` enough for the whole first implementation?
- Which behavior mismatches are treated as first-playable blockers versus later correctness work?
- Do we want to treat the current local FO1 folder as:
  - the content source of truth for investigation
  - but not the final acceptance environment because of HR-patch and CE-local artifacts?

## Asset Truth Table

This table records what is currently known from the local FO1 folder. It should be revised once archive contents can be enumerated with the FO1 backend.

| CE expectation | Loose file present in local FO1 folder? | Notes |
| --- | --- | --- |
| `MASTER.DAT` | Yes | Original FO1 core archive present |
| `CRITTER.DAT` | Yes | Original FO1 core archive present |
| `data\\vault13.gam` | No | Confirmed present in `MASTER.DAT` |
| `data\\worldmap.txt` | No | Confirmed absent from inspected `MASTER.DAT`; `../fallout1-ce` does not load this file, so this is a CE-side assumption |
| `scripts\\scripts.lst` | No | Confirmed present in `MASTER.DAT` |
| `proto\\*\\*.lst` | No | Confirmed present in stock archives; for example `PROTO\\ITEMS\\ITEMS.LST` exists |
| `art\\*\\*.lst` | No | Confirmed present in stock archives; for example `ART\\INTRFACE\\INTRFACE.LST` and `ART\\CRITTERS\\CRITTERS.LST` exist |
| compiled `DATA/SCRIPTS/*.INT` | Yes | Loose compiled scripts are present |
| `DATA/TEXT/ENGLISH/...` resources | Yes | Loose game/dialog text files are present |
| `credits.txt` | No | Not found loose and not found in inspected `MASTER.DAT`; distribution-path question remains open |
| `quotes.txt` | No | Not found loose and not found in inspected `MASTER.DAT`; distribution-path question remains open |
| premade `.gcd` files | No | Not found loose, but confirmed present in `MASTER.DAT` under `PREMADE\\*.GCD` |
| `data\\party.txt` | No | Not found loose and confirmed absent from inspected `MASTER.DAT`; `../fallout1-ce` does not load this file, so CE party init needs special FO1 handling |

## Risk Register

The highest current risks are:

- hidden contamination in the local FO1 test folder causing false confidence
- porting too much from `fallout1-runtime-bringup` and reintroducing overlay assumptions
- confusing path-resolution failures with schema incompatibility failures
- letting save/load ABI work creep back into scope
- scattering FO1 conditionals across the codebase instead of centralizing variant decisions

## `fo1_base` Classification Heuristic

Anything previously supplied by `fo1_base` should be classified before it is reintroduced in any form.

Use these buckets:

- `Loader fix`
  - stock FO1 already has the resource, CE just needs to reach it correctly
- `Tiny bridge asset`
  - stock FO1 does not expose the CE-needed data shape directly, but the missing piece is small and declarative
- `Engine/runtime work`
  - the mismatch is behavioral, ABI-level, or too rich to encode safely as a tiny data file
- `Reject as compatibility-mod content`
  - large replacement content that would recreate the forbidden overlay dependency

Current classification direction:

- `scripts\\scripts.lst`
  - `Loader fix`
  - confirmed present in `MASTER.DAT`
- `data\\vault13.gam`
  - `Loader fix`
  - confirmed present in `MASTER.DAT`
- `proto\\*\\*.lst`
  - `Loader fix`
  - confirmed present in stock archives
- `art\\*\\*.lst`
  - `Loader fix`
  - confirmed present in stock archives
- `premade\\player.gcd`
  - `Loader fix`
  - confirmed present in `MASTER.DAT`
- `data\\worldmap.txt`
  - `Tiny bridge asset`
  - strong candidate if kept derived and minimal
- `data\\party.txt`
  - `Tiny bridge asset`
  - strong candidate if it starts as default/generic FO1 companion metadata
- `credits.txt`
  - `Tiny bridge asset` or distribution/import step
  - FO1 expects it, but it was not found in inspected `MASTER.DAT`
- `quotes.txt`
  - `Tiny bridge asset` or distribution/import step
  - same classification as `credits.txt`
- player default native look selection
  - `Loader/config/variant default fix`, not a bridge asset
  - CE already knows both FO2-era native looks (`tribal`, `jumpsuit`)
  - FO1 should default to the vault-suit/jumpsuit presentation from new game, not the FO2 tribal progression
- PID-specific engine hacks
  - `Engine/runtime work`
  - both codebases contain hardcoded PID checks, and FO1 has behavior that cannot be recovered just from generic proto/script loading
  - this needs explicit audit and bucketing, not ad hoc fixes as regressions appear
- broad `text/english/game/*.msg` replacement
  - usually `Reject as compatibility-mod content`
  - exception: a very small surgical patch file set if specific CE-required entries prove missing
- large map/world content under `fo1_base/maps/*`
  - `Reject as compatibility-mod content`
  - this is full replacement content, not a bridge
- large proto/art replacement trees under `fo1_base/proto/*` and similar
  - usually `Reject as compatibility-mod content`
  - unless a very small subset is proven to be unavoidable and directly tied to an isolated FO1-vs-CE schema gap

Planning consequence:

- do not evaluate `fo1_base` by file count alone
- a single giant replacement tree is worse than a handful of tiny explicit bridge assets
- do not assume "stock assets loaded correctly" implies PID-special behavior will match FO1

## FO1 Features Dropped Or Reshaped In FO2

This track should be investigated in parallel with runtime bringup so visible FO1 behavior does not get silently flattened into FO2 defaults.

These are not all startup blockers. They are a feature-compatibility inventory that must be explicitly phased.

### Phase bucket rule

Use these buckets consistently:

- `Phase 1 core blocker`
  - blocks boot, basic progression, or functioning core systems
  - examples: startup/new-game flow, combat that works, worldmap travel, encounters, core quest progression
- `Phase 2-4 core correctness`
  - does not block immediate playability, but materially affects FO1 rules or progression correctness
  - examples: quest-state semantics, worldmap urgency rules, deeper combat behavior drift, endgame trigger correctness
- `Phase 5 secondary UX / flavor`
  - visible FO1-specific presentation, informational UI, or non-blocking flavor behavior
  - examples: Pip-Boy countdown presentation, special help/information flows, menu/archive polish

Decision bias:

- if it affects combat, worldmap travel, encounters, or quest completion logic, it belongs no earlier than `Phase 2-4 core correctness`
- if it is primarily informational or presentation-only, it belongs in `Phase 5 secondary UX / flavor`

### Pip-Boy status model

FO1 and CE/FO2 differ substantially in how Pip-Boy status content is modeled.

Current findings:

- FO1 status content is largely hardwired:
  - quest/location structure is encoded in built-in arrays like `sthreads`
  - holodisk presence is encoded in built-in `holodisks`
  - special status output includes explicit countdown rendering via `pip_days_left(...)`
  - example: FO1 renders the Vault 13 water countdown from `GVAR_VAULT_WATER`
- CE/FO2 Pip-Boy is more data-driven:
  - quests come from `data\\quests.txt`
  - holodisks come from `data\\holodisk.txt`
  - location names are resolved through FO2-style message/data lookups

Implication:

- FO1 support needs an explicit decision on whether to:
  - emulate FO1 Pip-Boy status behavior via tiny bridge data files
  - or add a small FO1-specific status rendering path
- this is where features like "time left" belong conceptually
- this is not purely cosmetic:
  - FO1 does not just render different strings
  - FO1 groups quests by built-in town buckets via `sthreads`
  - FO1 special-cases `GVAR_FIND_WATER_CHIP`
  - FO1 countdown/status output is partly hardcoded from globals like `GVAR_VAULT_WATER`
  - CE expects FO2-style external quest metadata with display/completion thresholds

Working conclusion:

- treat FO1 Pip-Boy support as two separate problems:
  - core quest-state visibility and correctness
  - later FO1-specific presentation polish
- the safest initial direction is:
  - keep CE's generic Pip-Boy shell/window
  - add an FO1-mode quest/holodisk provider that can be backed by tiny bridge files or built-in FO1 tables
  - only add FO1-specific countdown/status rendering once core runtime behavior is stable
- do not try to force FO1 semantics entirely through FO2 `quests.txt` conventions unless testing shows the mapping is truly lossless

Initial bucket:

- `Phase 5 secondary UX / flavor`
- exception: promote only the parts that demonstrably affect quest visibility or core progression understanding

### FO1-specific visible feature inventory to investigate

Seed list with initial buckets:

- default player native look / starting art presentation
  - initial bucket: `Phase 1 core blocker`
  - rationale: not a gameplay-system blocker, but it is a tiny, high-visibility FO1 identity issue that should be corrected as part of basic FO1-mode startup
  - technical direction:
    - prefer a variant-aware default in code/config
    - do not treat this as a missing art-asset problem
- Pip-Boy "time left" / water-chip countdown presentation
  - initial bucket: `Phase 5 secondary UX / flavor`
  - rationale: presentation of urgency is not the same as underlying timer/world logic
- Pip-Boy quest/status grouping and wording
  - initial bucket: `Phase 5 secondary UX / flavor`
  - promote only if FO2-style status modeling hides essential quest state
- "Tell me about" style UI/help flows and where they are exposed in FO1 source/data
  - initial bucket: `Phase 5 secondary UX / flavor`
  - current evidence: treat as informational UI/help behavior, not a core runtime blocker
- FO1 movie/archive availability and sequencing compared with FO2 video archives
  - initial bucket: split
  - startup/new-game movie gating: `Phase 1 core blocker`
  - archive/menu polish: `Phase 5 secondary UX / flavor`
- FO1 death/endgame/worldmap urgency messaging tied to water timer and mutant invasion pressure
  - initial bucket: split
  - underlying timer, consequences, and failure conditions: `Phase 2-4 core correctness`
  - messaging/presentation: `Phase 5 secondary UX / flavor`
- FO1 quest journal semantics versus FO2's `quests.txt`-driven model
  - initial bucket: `Phase 2-4 core correctness`
  - rationale: this can affect player-visible quest correctness, not just wording

Investigation rule:

- every such feature should be classified as:
  - `Phase 1 core blocker`
  - `Phase 2-4 core correctness`
  - `Phase 5 secondary UX / flavor`

### Explicit phase bucket inventory

#### Phase 1 core blockers

- startup/new-game flow in FO1 mode
- stock FO1 archive access
- first map load and script execution
- correct default player native look for FO1 new game
- combat that does not fail on enum/proc mismatches
- worldmap travel that functions
- encounters that can generate and resolve
- quest progression plumbing sufficient for early FO1 progression

#### Phase 2-4 core correctness

- FO1 quest-state semantics where CE/FO2 data models differ materially
- FO1 midnight timer/event processing for water countdown, failure movies, and world-state consequences
- worldmap urgency rules tied to water timer and invasion pressure
- deeper combat-rule differences that do not block basic fights but change FO1 behavior materially
- companion behavior correctness beyond a minimal default `party.txt` bridge
- endgame trigger and failure-condition correctness
- PID-specific engine hacks that affect gameplay behavior, combat, map aging, object use, or scripted special cases

#### Phase 5 secondary UX / flavor

- Pip-Boy countdown / "time left" presentation
- Pip-Boy status wording/grouping
- "Tell me about" and similar informational/help flows
- credits/quotes/menu polish questions
- non-blocking movie/archive presentation differences
- tertiary FO1-vs-FO2 text/UI differences

## PID-Specific Engine Hacks

This should be treated as its own audit track.

Why it matters:

- both engines contain hardcoded PID checks in addition to generic proto/script logic
- these checks are often silent gameplay rules, not loader issues
- if left unaudited, FO1 mode can appear mostly functional while still missing important one-off behavior

Current finding:

- CE and FO1 share some long-standing PID special cases
- FO1 also contains PID-specific behavior that is not obviously represented in CE's FO2-centric special cases
- therefore "generic FO1 asset loading" is not sufficient for correctness here
- some older FO1 PID hacks appear to have been partially generalized in CE into critter flags or helper predicates
  - this means the audit should look for semantic coverage, not just literal PID-equality copies

Observed examples from `../fallout1-ce` worth auditing against CE:

- combat special cases in `actions.cc` / `combat.cc` for specific critter PIDs
- map-aging and object-aging rules in `map.cc` keyed by specific critter or misc-item PIDs
- script/runtime object handling in `scripts.cc` for specific scenery PIDs
- object/proto handling around exit-grid and special misc-item ranges

Observed CE-side examples that may be FO2-specific or need FO1 review:

- weapon/item special cases such as `PROTO_ID_SOLAR_SCORCHER`, `PROTO_ID_JET_ANTIDOTE`, `PROTO_ID_MEGA_POWER_FIST`
- combat and AI checks for specific critter PIDs in `combat.cc` and `combat_ai.cc`
- scenery/elevator/dialog special cases in `scripts.cc`, `automap.cc`, and `game_dialog.cc`

First concrete comparison findings:

- map corpse-aging logic shows one useful migration pattern:
  - FO1 uses explicit critter PID exclusions in `map.cc` when deciding corpse drops/blood handling
  - CE uses `critterFlagCheck(pid, CRITTER_NO_DROP)` and `critterFlagCheck(pid, CRITTER_NO_HEAL)` in the corresponding paths
  - implication:
    - some FO1 PID hacks may be satisfiable by ensuring FO1 critter flags are loaded/interpreted correctly
    - do not blindly port old literal PID checks if the CE abstraction already expresses the same rule
- explosive/flare/book handling is mostly shared family behavior:
  - FO1 `protinst.cc` and CE `proto_instance.cc` still special-case flares, explosives, books, doctor bags/first-aid kits
  - implication:
    - many item PID hacks are inherited/shared and are lower-risk
    - priority should go to places where CE added clearly FO2-only item or car behaviors
- combat contains clear per-critter special cases in both engines:
  - FO1 has explicit critter PID behavior around death handling and damage immunity paths
  - CE has its own critter PID special cases in combat start/over and safety logic
  - implication:
    - combat PID audit belongs in `Phase 2-4 core correctness`, with promotion to Phase 1 only if an early-game critter is affected

Initial triage buckets for PID hacks:

- `Shared/inherited`
  - likely already safe or close; verify but do not prioritize first
- `Generalized in CE`
  - FO1 literal PID logic may map to CE flags/helpers instead of requiring new PID checks
- `FO1-only likely needed`
  - candidate for variant-aware runtime handling
- `FO2-only must be gated`
  - CE special cases that likely should not apply in FO1 mode
- `Unknown impact`
  - track and defer until a failing case or high-risk subsystem review

### First structured PID inventory

This is a first-pass checklist from the highest-value gameplay files. It is not exhaustive, but it is now concrete enough to drive follow-up work.

#### Shared/inherited

- elevator-door scenery PIDs in script/elevator handling
  - FO1: `scripts.cc` uses specific scenery PIDs `0x2000099`, `0x20001A5`, `0x20001D6`
  - CE: same elevator-door PID family still appears in `src/scripts.cc`
  - classification: `Shared/inherited`
  - likely phase: verify during Phase 1, but low risk unless FO1 elevator content differs
- flare / lit-flare transitions
  - FO1 `protinst.cc` and CE `proto_instance.cc` both special-case flare activation and lit flare light emission
  - classification: `Shared/inherited`
  - likely phase: later verification unless an early FO1 map depends on it
- explosives / timed explosives / books / medkits
  - both engines still special-case dynamite/plastic explosives, books, and doctor/first-aid kits
  - classification: `Shared/inherited`
  - likely phase: verify, but not a likely FO1-only blocker by itself
- charged misc items
  - stealth boys, geiger counters, motion sensor appear as charged-item PID special cases in both engines
  - classification: `Shared/inherited`

#### Generalized in CE

- corpse aging / corpse drop / healing rules on old maps
  - FO1 `map.cc` uses explicit critter PID exclusions for blood/drop behavior on some corpses
  - CE `map.cc` instead relies on `CRITTER_NO_DROP` and `CRITTER_NO_HEAL`
  - classification: `Generalized in CE`
  - implication:
    - first verify whether FO1 critter proto flags reproduce the old behavior
    - only reintroduce literal FO1 PID checks if flag-driven semantics do not match
- special-death / no-knockback / invulnerable / no-steal style combat flags
  - CE pushes several old one-off combat behaviors behind `critterFlagCheck(...)`
  - classification: `Generalized in CE`
  - implication:
    - the audit should compare FO1 critter flags and meanings before adding new variant branches
- death / corpse flattening behavior in actions/combat
  - FO1 `actions.cc` uses literal critter PID checks to suppress flattening, special-death drops, and some destruction cases
  - CE `actions.cc` / `combat.cc` now use `CRITTER_FLAT`, `CRITTER_SPECIAL_DEATH`, `CRITTER_NO_DROP`, `CRITTER_NO_KNOCKBACK`, `CRITTER_INVULNERABLE`
  - classification: `Generalized in CE`
  - implication:
    - combat correctness may depend on FO1 critter flags/proto interpretation more than literal PID copying

#### FO1-only likely needed

- explicit critter PID handling in FO1 combat/actions around death and destruction behavior
  - FO1 `actions.cc` / `combat.cc` still reference specific critter PIDs such as `16777224`, `16777239`, `16777265`, `16777266`
  - CE no longer mirrors these as literal checks in the same way, or replaces them with different abstractions
  - classification: `FO1-only likely needed`
  - likely phase: `Phase 2-4 core correctness`
- FO1 map-aging blood/corpse presentation for specific corpse families
  - FO1 uses explicit corpse PIDs `213`, `214` and specific blood PID selection rules
  - CE collapses this into more generic drop/blood handling
  - classification: `FO1-only likely needed` or `Generalized in CE`, depending on visual mismatch severity
  - likely phase: `Phase 5 secondary UX / flavor` if only visual, promote if gameplay-relevant
- FO1-specific critical/death exceptions tied to exact critter identities
  - FO1 `combat.cc` has explicit special handling for defender/attacker PID `16777224` in critical-success/failure, death checks, result application, and damage application
  - CE does not preserve this as a literal PID path and instead uses generalized invulnerability/drop checks plus FO2-specific critter PID hooks elsewhere
  - classification: `FO1-only likely needed`
  - likely phase: `Phase 2-4 core correctness`

#### FO2-only must be gated

- car-specific object use and recharge behavior
  - CE `proto_instance.cc` special-cases car usage and charging with energy cells
  - this is FO2 content and should not leak into FO1 mode
  - classification: `FO2-only must be gated`
  - likely phase: `Phase 1 core blocker` only if reachable in FO1 mode; otherwise Phase 2 cleanup
- FO2-only items and combat hooks
  - CE `item.cc` special-cases `PROTO_ID_SOLAR_SCORCHER`, `PROTO_ID_JET_ANTIDOTE`, `PROTO_ID_MEGA_POWER_FIST`, `PROTO_ID_SUPER_CATTLE_PROD`
  - classification: `FO2-only must be gated`
  - likely phase: `Phase 2-4 core correctness`
- FO2-specific critter PID behavior in combat begin/over
  - CE `combat.cc` references critter PID `0x1000098` in combat start/finish handling
  - classification: `FO2-only must be gated`
  - likely phase: `Phase 2-4 core correctness`
- FO2-era combat/item extensions layered on top of shared combat paths
  - CE `actions.cc` / `combat.cc` add FO2/sfall-era behavior such as Molotov/Pyromaniac/Flameboy death animation tuning, enhanced-knockout handling, and additional combat hook cleanup
  - classification: `FO2-only must be gated` or `Unknown impact`
  - likely phase: `Phase 2-4 core correctness`

#### Unknown impact

- FO1 `actions.cc` force-field / special-attack PID handling using `PROTO_ID_0x20001EB`, `FID_0x20001F5`, and several hardcoded critter PIDs
  - needs content-side identification before deciding whether FO1 mode must preserve it
- CE scenery/dialog special cases in `game_dialog.cc`, `automap.cc`, and `scripts.cc`
  - some may be harmless shared engine baggage, others may be FO2-specific behavior leaks
- CE combat behavior fixes versus vanilla FO1 quirks
  - CE fixes some long-standing engine issues such as exit-grid knockback/fire-dance traversal and death-animation fallback behavior
  - likely safe to keep, but each fix should be distinguished from true FO2-only behavior changes

### PID audit execution order

1. `combat.cc` and `actions.cc`
   - highest gameplay-risk density
2. `map.cc`
   - aging, corpse, and persistence semantics
3. `scripts.cc`
   - elevator/scenery transitions and scripted world interactions
4. `item.cc` / `proto_instance.cc`
   - item use and FO2-only object behavior
5. `game_dialog.cc` / `automap.cc`
   - lower priority unless a concrete FO1 regression appears

### Combat/actions conclusions from the current pass

- not every FO1 literal PID check should be ported
  - several old cases correspond to modern CE critter flags and helper predicates
- combat correctness depends on three separate layers:
  - literal PID special cases that still matter
  - critter/proto flags that may already encode the intended behavior
  - CE/FO2 extra combat features that need variant review or gating
- practical next step for this area:
  - identify the FO1 critters behind the old literal PIDs
  - compare their proto flags against CE's generalized checks
  - only add new FO1-mode PID branches where the generalized path does not reproduce FO1 semantics

Planning rule:

- do not try to solve PID-specific drift by shipping replacement assets
- maintain an explicit table of:
  - file/function
  - PID(s)
  - behavior
  - FO1-only, FO2-only, or shared
  - target phase

Initial bucket:

- default to `Phase 2-4 core correctness`
- promote to `Phase 1 core blocker` only if a specific PID hack blocks startup, early progression, or makes a core system unusable

## Recommended Order of Code Recovery From `fallout1-runtime-bringup`

Recover in this order:

1. `src/game_variant.*`
2. `src/fo1db/*`
3. `src/dfile.*` changes required for FO1 backend support
4. `src/main.cc` variant startup changes
5. `src/game_config.*` variant config-name/install-root changes
6. `src/game.cc` variant resource/bootstrap changes
7. `src/settings.*` only as needed for correct path resolution
8. `src/xfile.cc` only the stock-FO1-safe parts

Avoid recovering these blindly:

- mod-loading assumptions that pull in `mods/fo1_base`
- broad logging noise that was only useful during the initial spike
- any overlay-first behavior that hides stock content failures

## Format Compatibility Matrix

This section answers a narrower question:

- for each important on-disk/runtime format, can FO2 CE consume stock FO1 data directly?

Verdict meanings:

- `Compatible`
  - same practical layout or already safe to share
- `Compatible with defaults`
  - layout is nearly identical, but FO2 CE must synthesize missing FO2-only fields
- `Same file family, different interpretation`
  - bytes are close enough, but enum tables or semantics differ and must be variant-aware
- `Separate FO1 path required`
  - CE cannot safely treat the FO1 format as the FO2 format without a dedicated reader or branch

### Proto `.pro` records

Verdict:

- `Compatible with defaults`

Why:

- item proto payload ordering is effectively the same
- critter proto payload is almost the same, but FO2 CE appends `damageType`
- scenery proto payload is close, but ladder/generic/stairs semantics drifted
- FO2 CE also names and uses the fields more aggressively

Evidence:

- FO2 CE critter proto reader explicitly tries to read `damageType` and falls back to `DAMAGE_TYPE_MELEE` on missing data:
  - [src/critter.cc](/Users/klaas/game/fallout2-ce/src/critter.cc)
- FO1 proto readers/writers:
  - `../fallout1-ce/src/game/proto.cc`
- FO2 CE proto readers/writers:
  - [src/proto.cc](/Users/klaas/game/fallout2-ce/src/proto.cc)

Mitigation:

- keep a shared proto reader shape
- on FO1 reads:
  - default `CritterProtoData.damageType`
  - apply FO1-specific scenery interpretation for ladder/generic cases
- do not assume FO2 enum values beyond FO1 ranges are meaningful for loaded FO1 content

### Proto update data inside save/load object state

Verdict:

- `Separate FO1 path required`

Why:

- FO1 has a dedicated `proto_read_protoUpdateData` / `proto_write_protoUpdateData` path
- ladder object update data differs:
  - FO1 stores only `destinationBuiltTile`
  - FO2 CE object-side ladder data contains `destinationMap` and `destinationBuiltTile`
- critter/object update semantics differ enough that save compatibility should not be shared blindly

Evidence:

- FO1:
  - `../fallout1-ce/src/game/proto.cc`
- FO2 CE:
  - [src/object.cc](/Users/klaas/game/fallout2-ce/src/object.cc)
  - [src/obj_types.h](/Users/klaas/game/fallout2-ce/src/obj_types.h)

Mitigation:

- do not target FO1 save compatibility
- if FO1 saves are detected, reject them with a variant-specific error instead of attempting a reader path

### Runtime `Object` serialization in savegames

Verdict:

- `Separate FO1 path required`

Why:

- the runtime `Object` layout is close, but object serialization is coupled to:
  - proto update data
  - script serialization
  - map/savegame handler ordering
- FO2 CE object save/load includes `scriptIndex` and FO2-era object-data semantics

Evidence:

- FO2 CE object reader/writer:
  - [src/object.cc](/Users/klaas/game/fallout2-ce/src/object.cc)
- FO1 object reader/writer:
  - `../fallout1-ce/src/game/object.cc`

Mitigation:

- explicitly separate FO1 and FO2 savegame compatibility goals
- treat stock FO1 runtime support as "new game from FO1 assets" first
- reject unsupported FO1 savegame loads rather than trying to deserialize them

### Script table metadata and script save records

Verdict:

- `Separate FO1 path required`

Why:

- FO1 `Script` struct uses `SCRIPT_PROC_COUNT == 24`
- FO2 CE widened this to 28 and added new proc categories
- serialized script records store proc arrays and runtime fields tied to that width
- even where field order is similar, array width is not

Evidence:

- FO2 CE:
  - [src/scripts.h](/Users/klaas/game/fallout2-ce/src/scripts.h)
  - [src/scripts.cc](/Users/klaas/game/fallout2-ce/src/scripts.cc)
- FO1:
  - `../fallout1-ce/src/game/scripts.h`
  - `../fallout1-ce/src/game/scripts.cc`

Mitigation:

- do not share script savegame serialization
- for stock FO1 gameplay, the important case is loading compiled map/item/critter scripts, not FO1 savegame script state
- keep runtime execution variant-aware where CE expects FO2-only procs/hooks
- fail early on detected FO1 save-record loads with a clear unsupported-save message

### `scripts.lst`

Verdict:

- `Same file family, different interpretation`

Why:

- path and purpose are the same
- but runtime expectations differ because FO2 CE exposes additional proc slots and hook opportunities

Mitigation:

- loading the list itself is probably fine
- proc discovery and invocation must remain variant-aware

### Global vars bootstrap files like `vault13.gam`

Verdict:

- `Same file family, different interpretation`

Why:

- parser is simple text-based `name=value` style loading
- but meaning depends entirely on expected variable count/order and the content source
- the bigger problem is path selection and choosing the right FO1 source of truth

Evidence:

- [src/game.cc](/Users/klaas/game/fallout2-ce/src/game.cc)

Mitigation:

- likely keep the parser
- make source-path and expected semantics variant-aware
- verify stock FO1 provides the canonical file needed, instead of relying on an overlay replacement

Containment note:

- this should be implemented as a tiny variant bootstrap module, not custom FO1 logic inside `src/game.cc`
- the parser can stay shared
- FO1/FO2 should choose bootstrap path and validation rules through a narrow provider hook

### `worldmap.txt`

Verdict:

- `CE-side replacement surface, not a stock FO1 asset`

Why:

- stock FO1 does not load `data\\worldmap.txt`
- FO1 worldmap behavior is encoded mostly in compiled tables and runtime/save state inside `worldmap.cc`
- important FO1 built-in tables include:
  - `WorldTerraTable`
  - `WorldEcounTable`
  - `city_location`
  - `TownHotSpots`
  - `RandEnctNames`
  - `SpclEncRange`
  - `xlate_town_table`
- FO1 also keeps worldmap progression in runtime/save structures such as:
  - `WorldGrid`
  - `TwnSelKnwFlag`
  - `first_visit_flag`
  - `encounter_specials`
  - current world position / town / section state
- CE's `data\\worldmap.txt` is therefore not "the FO1 file with a slightly different schema"; it is a CE/FO2-style data-driven substitute for logic that FO1 kept in code plus save state

Mitigation:

- stop treating stock FO1 support as a file-path problem here
- FO1 mode needs a separate worldmap data source strategy:
  - preferred first step: a shipped or generated FO1 `worldmap.txt` bridge asset derived from FO1 semantics
  - fallback: a variant-aware built-in FO1 dataset derived from FO1 source/static tables
- even if CE keeps `worldmap.txt` internally as its runtime format, that file should be understood as a compatibility layer for FO1 mode, not an original asset
- practical source options for the bridge include:
  - FO1 source/static tables as the authoritative baseline
  - `Fo1in2` as a reference/input for equivalent data, without making it a runtime dependency

Additional finding:

- the save/runtime model behind the parser has diverged heavily
- FO1 worldmap save state is compact and grid-oriented:
  - `WorldGrid`
  - `TwnSelKnwFlag`
  - first-visit/special-encounter/current-town/current-section/world-position ints
- FO2 CE worldmap save state serializes a much richer runtime:
  - city entrance states
  - per-subtile state
  - random encounter table counters
  - car ownership/fuel state
  - FO2-specific globals like `gDidMeetFrankHorrigan`

Evidence:

- FO2 CE:
  - [src/worldmap.cc](/Users/klaas/game/fallout2-ce/src/worldmap.cc)
- FO1:
  - `../fallout1-ce/src/game/worldmap.cc`

Implication:

- `worldmap.txt` syntax compatibility is irrelevant for stock FO1 assets, because there is no stock FO1 `worldmap.txt`
- FO1 worldmap support should be treated as:
  - a data-model port from FO1 static/runtime definitions into CE's worldmap runtime
  - separate save/runtime compatibility for persisted world state

### Party system

Verdict:

- `CE-side replacement surface, not a stock FO1 asset`

Why:

- FO1 party handling is a relatively small runtime list of recruited companions
- stock FO1 does not load `data\\party.txt`
- FO1 party code is centered on `partyMemberList`, object/script bookkeeping, and save-time object/script preservation
- FO1 companion behavior comes from:
  - recruited critter objects and their PIDs
  - critter scripts
  - combat AI packet assignment
  - generic critter/proto/stat systems
  - `data\\ai.txt` for AI packet definitions
- FO2 CE party handling is now partly data-driven from `data\\party.txt`
- CE expects FO2-era companion capability tables such as:
  - area attack mode support
  - run away mode support
  - best weapon mode support
  - distance mode support
  - attack-who mode support
  - chem-use mode support
  - disposition support
  - multi-pid level-up metadata
- CE also threads these descriptions into other systems like perk lookup and save/load bookkeeping

Evidence:

- CE party initialization loads `data\\party.txt` and allocates descriptor tables:
  - [src/party_member.cc](/Users/klaas/game/fallout2-ce/src/party_member.cc)
- FO1 party code has no corresponding `party.txt`-driven description layer:
  - `../fallout1-ce/src/game/party.cc`
  - `../fallout1-ce/src/game/party.h`
- FO1 combat AI still depends on `data\\ai.txt`:
  - `../fallout1-ce/src/game/combatai.cc`
- CE perk code consults `gPartyMemberPids` for party-member perk rank storage:
  - [src/perk.cc](/Users/klaas/game/fallout2-ce/src/perk.cc)

Mitigation:

- do not treat `data\\party.txt` as a simple missing-path problem
- FO1 mode should try this order first:
  - a minimal shipped/generated `party.txt` bridge with mostly default companion behavior
  - a variant-aware minimal built-in descriptor table only if a bridge file proves too awkward
  - an FO1-native companion rules path only if the data-driven bridge cannot cover required behavior
- the bridge must cover more than UI toggles:
  - CE perk storage for companions keys off `gPartyMemberPids`
  - if an FO1 recruitable companion is missing from the descriptor source, perk rank lookup falls back incorrectly
  - this makes `party.txt` a hidden runtime contract for companion persistence/progression, not just an interface contract
- companion controls exposed by CE should be reviewed feature-by-feature:
  - features backed only by FO2 `party.txt` metadata may need to be hidden, downgraded, or replaced in FO1 mode
  - features backed by generic AI packet/object state can stay, provided they map to FO1 behavior
- keep FO1 save compatibility out of scope initially, because CE party save/load is tied to this richer descriptor model

Recommended minimum FO1 bridge for party:

- include every FO1 recruitable companion PID in the descriptor source even if behavior options are mostly defaulted
- keep level-up metadata minimal and explicit rather than trying to mirror FO2 multi-form evolution rules
- if a companion has no meaningful FO2-style control matrix, prefer a conservative always-valid option set instead of bespoke code
- gate or hide CE companion controls that cannot be represented honestly for FO1

### Combat data and critical-hit tables

Verdict:

- `Same file family, different interpretation`

Why:

- the core combat loop still descends from the same engine family
- but CE widened important table/index spaces and added optional combat rule systems
- the most obvious schema drift is kill-type handling:
  - FO2 CE keeps `KILL_TYPE_COUNT`
  - then doubles critical-hit table capacity with `SFALL_KILL_TYPE_COUNT`
- CE combat also layers in:
  - sfall callbacks and script hooks
  - alternate damage-calculation modes
  - extended unarmed and burst-mod support

Evidence:

- CE critical hit tables are sized by `SFALL_KILL_TYPE_COUNT`:
  - [src/combat.cc](/Users/klaas/game/fallout2-ce/src/combat.cc)
  - [src/proto_types.h](/Users/klaas/game/fallout2-ce/src/proto_types.h)
- FO1 combat is materially smaller and does not share CE’s widened extension surface:
  - `../fallout1-ce/src/game/combat.cc`

Mitigation:

- FO1 mode must clamp loaded/content-derived kill types to the FO1-valid range
- any CE code iterating `SFALL_KILL_TYPE_COUNT` should be reviewed for FO1 mode so it does not imply missing FO1 content tables
- loader compatibility alone is not enough here; this is a behavior-and-table-bounds problem

Additional schema drift in the same area:

- animation enum and critter FRM encoding appear structurally compatible between FO1 and CE
  - both codebases still use the same `ANIM_*` range and critter FID packing shape
  - this lowers the risk of a broad "FO1 has fewer animation types" failure
- the subtler risk is not animation slots, but supporting enum spaces around them:
  - FO1 has fewer kill types
  - FO1 has fewer calibers and item/proto IDs
  - CE carries FO2-only weapon/item identifiers and related special handling

Practical implication:

- art loading is likely not the primary blocker once the default dude look is fixed
- combat and inventory correctness should focus more on enum bounds and FO2-only item behavior than on missing critter animation codes

### Script proc ABI and FO2-only proc usage

Verdict:

- `Same runtime family, but variant-aware proc availability required`

Why:

- FO1 and FO2 CE script metadata are close enough that one widened in-memory `Script` model is still plausible
- but CE actively uses FO2-only proc slots in engine code
- that means "just store fewer procs" is not enough unless all engine call sites handle missing FO1-only entries safely

Evidence:

- proc-count drift:
  - FO1 `SCRIPT_PROC_COUNT == 24`
  - CE `SCRIPT_PROC_COUNT == 28`
- CE call sites using FO2-only procs:
  - `SCRIPT_PROC_PUSH` in [src/actions.cc](/Users/klaas/game/fallout2-ce/src/actions.cc)
  - `SCRIPT_PROC_IS_DROPPING` in [src/proto_instance.cc](/Users/klaas/game/fallout2-ce/src/proto_instance.cc)
  - combat request handling tied to FO2-era proc surface and request flags in [src/scripts.cc](/Users/klaas/game/fallout2-ce/src/scripts.cc)

Recommended handling:

- keep the CE-sized in-memory `Script` struct
- on FO1 script metadata load:
  - populate the first 24 proc slots from FO1 data
  - initialize CE-only proc slots to "missing" (`-1`)
- implement proc availability checks centrally:
  - `scriptHasProc`
  - proc lookup/discovery
  - any helper that translates script metadata into callable proc indices
- treat FO2-only proc invocations as clean no-ops when the proc is absent

Important nuance:

- "add no-op procs" should mean engine-level no-op dispatch for missing proc slots
- it should not mean synthesizing fake wrapper procedures into every FO1 script binary
- central dispatch/defaulting is lower-risk and keeps the compatibility rule in one place

### Script request flags

Verdict:

- `Compatible names, different semantics pressure`

Why:

- request flags mostly line up structurally between FO1 and CE
- but CE naming obscures that `SCRIPT_REQUEST_0x40` is FO1’s `SCRIPT_REQUEST_NO_INITIAL_COMBAT_STATE`
- CE still routes that flag through widened FO2-era combat request handling

Evidence:

- CE:
  - [src/scripts.h](/Users/klaas/game/fallout2-ce/src/scripts.h)
  - [src/scripts.cc](/Users/klaas/game/fallout2-ce/src/scripts.cc)
- FO1:
  - `../fallout1-ce/src/game/scripts.h`

Mitigation:

- rename/comment this flag more explicitly during FO1 work so the behavior is understandable in both variants
- keep request handling centralized in scripts/combat transition code rather than scattering variant checks at call sites

### Map files `.map`

Verdict:

- `Unknown, likely separate FO1 path required for some cases`

Why:

- the loader stack around maps is strongly entangled with:
  - map headers
  - local/global vars
  - scripts
  - object update data
- the map family is likely related enough to start loading, but full interchangeability is not something to assume

Evidence:

- FO2 CE map load/save:
  - [src/map.cc](/Users/klaas/game/fallout2-ce/src/map.cc)
- FO1 map load/save:
  - `../fallout1-ce/src/game/map.cc`

Mitigation:

- treat stock map load as a validation milestone
- if failures appear, separate them into:
  - header/layout mismatch
  - object serialization mismatch
  - script-state mismatch
  - worldmap/transition semantic mismatch

Refined finding after code comparison:

- raw map container compatibility is better than many other subsystems:
  - `MapHeader` layout is still effectively shared
  - both loaders allocate map globals/locals the same way
  - both load squares, scripts, and objects in the same broad order
  - both load per-map `.GAM` sidecar globals for non-saved maps
- the bigger risk is semantic drift around the loader, not the `.map` body itself:
  - CE accepts map version `19` and `20`, FO1 expects `19`
  - CE map lifecycle is wired into newer follow-up helpers and CE save/runtime extras
  - FO1 script/runtime state carries story-specific state like `water_movie_play_flag`

Architecture direction:

- do not fork `src/map.cc` wholesale unless testing proves it necessary
- prefer a small FO1 compatibility module with hook points such as:
  - `fo1MapNormalizeHeader(...)`
  - `fo1MapLoadSidecarGlobals(...)`
  - `fo1MapPostLoadSetup(...)`
  - `fo1MapPreAreaEventCheck(...)`
- let shared `mapLoad(...)` continue to own the generic binary read sequence
- move FO1 semantic differences to explicit hook points before/after that sequence

### Savegame handler stack

Verdict:

- `Separate FO1 path required`

Why:

- both repos use a handler list model
- but the handler lists encode savegame ABI by ordering and by per-subsystem data layouts
- subsystem payloads already differ enough that this should not be shared

Evidence:

- FO2 CE:
  - [src/loadsave.cc](/Users/klaas/game/fallout2-ce/src/loadsave.cc)
- FO1:
  - `../fallout1-ce/src/game/loadsave.cc`

Mitigation:

- do not promise FO1 save compatibility
- detect unsupported FO1 saves and stop with a relevant error
- if CE ever writes FO1-mode saves in the future, keep them clearly separated from FO2 saves

Containment note:

- the existing CE save stack should remain FO2/CE-owned
- any future FO1-mode save support should register its own handler path rather than inserting conditionals through the FO2 handler list

### Movies

Verdict:

- `Separate FO1 path required`

Why:

- movie enum spaces are completely different
- this is not just filenames; numeric IDs and story flow differ

Evidence:

- [src/game_movie.h](/Users/klaas/game/fallout2-ce/src/game_movie.h)
- `../fallout1-ce/src/game/gmovie.h`

Mitigation:

- introduce symbolic variant-aware movie descriptors
- never share movie numeric IDs across variants

Additional derisking note:

- FO1 urgency/event movies are not just a table-mapping issue
- FO1 midnight processing in `../fallout1-ce/src/game/scripts.cc` directly decrements `GVAR_VAULT_WATER` and triggers `MOVIE_BOIL1/2/3`
- FO1 worldmap event checks in `../fallout1-ce/src/game/worldmap.cc` also enforce death/failure and VATS countdown consequences
- CE `src/scripts.cc` currently drives FO2 Arroyo timer movies (`MOVIE_ARTIMER1..4`) from FO2 globals
- therefore FO1 mode needs a variant-native timed-event path, not merely different movie IDs

Execution rule:

- do not try to reuse FO2 timer/movie logic with remapped constants
- port or recreate the FO1 midnight-event/state-transition logic explicitly behind FO1 mode
- keep archive/video-list presentation as a separate later concern from story-state timer processing

### Endgame slideshow and ending movie flow

Verdict:

- `FO1-native runtime path required`

Why:

- CE endgame support is data-driven from `data\\endgame.txt` and `data\\enddeath.txt`
- FO1 endgame logic is largely hardcoded around specific FO1 global vars, narrator clips, and slide order
- FO1 also plays gender-specific ending walk movies before credits
- this is not just presentation polish; story-state interpretation differs

Evidence:

- CE:
  - [src/endgame.cc](/Users/klaas/game/fallout2-ce/src/endgame.cc)
- FO1:
  - `../fallout1-ce/src/game/endgame.cc`

Mitigation:

- do not assume FO1 can be represented cleanly by dropping in tiny `endgame.txt` and `enddeath.txt` bridge files
- preferred order:
  - first, add an FO1-mode endgame decision path that reproduces FO1 slide/ending selection semantics
  - second, decide whether any data extraction/bridge layer is still useful for narration metadata
  - third, restore FO1-specific ending movie flow including gender-specific walk movie selection
- treat credits/movie playback polish separately from core ending-condition correctness

Containment note:

- a dedicated `fo1_endgame` module is preferable to teaching CE's `endgame.txt` path about all FO1 story rules
- the shared engine only needs narrow dispatch points such as:
  - `variantEndgamePlaySlideshow()`
  - `variantEndgamePlayMovie()`
  - `variantEndgameInitDeathEndingRules()`

### Timers and world-state urgency

Verdict:

- `FO1-native runtime path required`

Why:

- FO1 urgency mechanics are split across multiple systems:
  - midnight queue processing decrements vault water and triggers warning/failure movies
  - worldmap checks enforce death and later VATS/endgame consequences
  - Pip-Boy status renders parts of that state directly from globals
- CE's nearest equivalent logic is FO2-specific Arroyo timer handling and should not be treated as interchangeable

Evidence:

- FO1 midnight timer logic:
  - `../fallout1-ce/src/game/scripts.cc`
- FO1 worldmap failure/event checks:
  - `../fallout1-ce/src/game/worldmap.cc`
- CE FO2 timer path:
  - [src/scripts.cc](/Users/klaas/game/fallout2-ce/src/scripts.cc)

Mitigation:

- define a small FO1 timed-event checklist and port it intentionally:
  - decrement `GVAR_VAULT_WATER`
  - play the FO1 warning/failure movies only once at the correct thresholds
  - trigger worldmap death/failure when the timer reaches zero and the chip is not returned
  - preserve FO1 VATS/endgame countdown consequences
- keep the timer state in runtime globals, not in bridge assets
- bucket this as core correctness, not presentation, even though some symptoms are movie/UI visible

Containment note:

- this is a strong candidate for a dedicated `fo1_story_state` or `fo1_timers` module
- shared engine code should call into it at a few lifecycle points instead of embedding FO1 story logic into generic script/worldmap code

### Art and animation surface

Verdict:

- `Mostly compatible, but do not confuse art compatibility with gameplay-enum compatibility`

Why:

- the underlying critter animation enum surface appears effectively shared between FO1 and CE
- art filename building and FID packing also remain in the same family
- this means the FO1-only runtime is less likely to fail because CE expects entirely new animation classes
- however, CE layers FO2-era content assumptions around that shared art surface:
  - dual native player looks
  - extra kill types
  - extra calibers
  - extra FO2-only item/proto IDs with bespoke engine behavior

Evidence:

- animation enum alignment:
  - [src/animation.h](/Users/klaas/game/fallout2-ce/src/animation.h)
  - `../fallout1-ce/src/game/anim.h`
- art/FID handling remains very close:
  - [src/art.cc](/Users/klaas/game/fallout2-ce/src/art.cc)
  - `../fallout1-ce/src/game/art.cc`
- proto enum drift is around supported content categories, not basic FRM addressing:
  - [src/proto_types.h](/Users/klaas/game/fallout2-ce/src/proto_types.h)
  - `../fallout1-ce/src/game/proto_types.h`

Mitigation:

- treat the player default-look fix as its own small Phase 1 task
- do not spend early effort inventing animation bridge assets
- instead, review CE code that assumes FO2-only kill types, calibers, or special weapon/item proto IDs around otherwise-compatible art usage

### Stats

Verdict:

- `Same file family, different interpretation`

Why:

- base stat enum is mostly stable
- FO2 CE increases `PC_LEVEL_MAX` to 99, while FO1 is 21
- this matters for rules and progression, not usually raw file parsing

Evidence:

- [src/stat_defs.h](/Users/klaas/game/fallout2-ce/src/stat_defs.h)
- `../fallout1-ce/src/game/stat_defs.h`

Mitigation:

- variant-aware gameplay rules
- likely no separate serialized stat schema needed for startup/content loading

Additional note:

- some CE stat calculations already read FO2-only perk ids directly from `src/stat.cc`
- that means FO1 mode cannot rely on "out-of-range perks simply never appear"
- stat rule evaluation itself must become variant-aware where FO2-only perks are consulted

### Skills

Verdict:

- `Compatible`

Why:

- skill enum/count appears stable across the two codebases
- critter proto skill array width matches

Mitigation:

- treat as shared unless a content-specific mismatch appears

### Traits

Verdict:

- `Same file family, different interpretation`

Why:

- the trait count is the same, but the set is not identical
- FO1 uses `TRAIT_NIGHT_PERSON`
- CE uses the FO2-era `TRAIT_SEX_APPEAL` in the same slot
- FO1 trait rule logic also differs in behavior details:
  - FO1 `Skilled` grants a skill bonus in trait skill adjustment
  - FO1 `Night Person` adjusts Perception and Intelligence by time of day
- CE trait code currently reflects FO2 assumptions, not FO1 ones

Evidence:

- CE:
  - [src/trait_defs.h](/Users/klaas/game/fallout2-ce/src/trait_defs.h)
  - [src/trait.cc](/Users/klaas/game/fallout2-ce/src/trait.cc)
- FO1:
  - `../fallout1-ce/src/game/trait.h`
  - `../fallout1-ce/src/game/trait.cc`

Mitigation:

- do not treat traits as a shared semantic table
- keep the save/runtime shape shared if convenient:
  - two selected trait slots remain fine
- but make trait identity and rule evaluation variant-aware:
  - names/descriptions/art
  - stat modifiers
  - skill modifiers
  - any downstream code that assumes FO2-specific trait effects
- this is runtime rules work, not a bridge-asset problem

Containment note:

- avoid remapping trait slots ad hoc throughout gameplay code
- centralize FO1/FO2 trait semantics behind a small rules layer:
  - `variantTraitGetMetadata(...)`
  - `variantTraitGetStatModifier(...)`
  - `variantTraitGetSkillModifier(...)`

### Perks

Verdict:

- `Same file family, different interpretation`

Why:

- FO2 CE has a much wider perk enum space
- FO1 content will only legitimately reference the smaller FO1 range
- any CE logic that iterates full FO2 perk tables can leak FO2 assumptions into FO1 gameplay
- CE also directly consults FO2-only perks in generic runtime code:
  - stat calculations
  - combat modifiers
  - worldmap behavior
  - skill modifiers
  - script opcodes validating perk ids against `PERK_COUNT`

Evidence:

- [src/perk_defs.h](/Users/klaas/game/fallout2-ce/src/perk_defs.h)
- `../fallout1-ce/src/game/perk_defs.h`
- examples of FO2-only perk usage in CE runtime:
  - [src/stat.cc](/Users/klaas/game/fallout2-ce/src/stat.cc)
  - [src/perk.cc](/Users/klaas/game/fallout2-ce/src/perk.cc)
  - [src/worldmap.cc](/Users/klaas/game/fallout2-ce/src/worldmap.cc)
  - [src/combat.cc](/Users/klaas/game/fallout2-ce/src/combat.cc)

Mitigation:

- do not stop at bounds checks
- FO1 mode needs a variant-native perk rules table:
  - valid perk ids
  - names/descriptions
  - level/requirement metadata
  - effect application and derived-stat hooks
- script opcode validation should use variant-valid perk ranges, not the raw CE `PERK_COUNT`
- review runtime call sites that currently bake in FO2-only perks:
  - some can remain harmless no-ops if unreachable in FO1
  - others change generic formulas and should branch explicitly by variant
- this is core correctness work, not a tiny bridge-asset candidate

Containment note:

- handle this as a rules-table problem, not ad hoc guards
- preferred structure:
  - shared perk storage/save shape if possible
  - variant-owned perk metadata/effect tables
  - narrow integration calls from stat/combat/worldmap/item code

### Map/bootstrap lifecycle

Verdict:

- `Mostly shared container flow, but variant-owned setup/policy hooks required`

Why:

- startup, script init, map load, worldmap entry, and story-state updates form one lifecycle chain
- FO1 and CE are close in raw order, but differ in a few policy/state details that should not be hardcoded into shared FO2 paths

Evidence:

- CE:
  - [src/game.cc](/Users/klaas/game/fallout2-ce/src/game.cc)
  - [src/map.cc](/Users/klaas/game/fallout2-ce/src/map.cc)
  - [src/scripts.cc](/Users/klaas/game/fallout2-ce/src/scripts.cc)
- FO1:
  - `../fallout1-ce/src/game/game.cc`
  - `../fallout1-ce/src/game/map.cc`
  - `../fallout1-ce/src/game/scripts.cc`

Concrete drift points:

- script-game-init lifecycle:
  - FO1 resets and persists `water_movie_play_flag`
  - CE script init/save path lacks that FO1-specific story-state byte
- map load:
  - raw loader order is broadly shared
  - post-load follow-up and tolerated runtime state differ
- worldmap/game-area checks:
  - CE runs FO2-era area-event processing after map load
  - FO1 ties more of its urgency/failure behavior to separate timed/worldmap checks

Recommended isolation strategy:

- add a small variant lifecycle facade and call it from existing subsystem boundaries
- keep shared sequencing in CE where possible
- move variant policy into modules such as:
  - `fo1_bootstrap`
  - `fo1_map_compat`
  - `fo1_story_state`
  - `fo1_endgame`

Minimal desired integration points:

1. startup bootstrap selection
2. script-game-init / script-save-state extension hook
3. map post-load hook
4. worldmap/game-area event hook
5. endgame dispatch hook

### Calibers / kill types / proto ids

Verdict:

- `Same file family, different interpretation`

Why:

- these are enum/value-space issues, not necessarily different file containers
- FO2 CE widened them significantly

Mitigation:

- clamp FO1 content to FO1-valid ranges
- provide variant-aware defaults and lookup tables
- audit array indexing carefully, especially for combat and critter-death logic

## Priority Implications

The matrix suggests this execution order:

1. boot/runtime variant support
2. native FO1 DAT access
3. stock FO1 proto loading with explicit FO1 defaults
4. stock FO1 startup/worldmap/global-var content loading
5. map load validation
6. script proc/defaulting layer for FO1 metadata
7. minimal replacement surfaces for startup blockers:
   - `worldmap.txt`
   - `party.txt`
   - startup movie/default-look gating
8. variant-native timed-event and quest-state rules:
   - water timer
   - FO1 Pip-Boy quest visibility semantics
   - worldmap/endgame trigger correctness
9. trait/perk rule-table divergence:
   - FO1 `Night Person`/`Skilled` semantics
   - FO1-valid perk table and runtime hooks
10. deeper party/worldmap/combat behavior divergence
11. endgame slideshow/movie sequencing restoration
12. unsupported-FO1-save detection and error path

The key scope-control decision is:

- new-game runtime from stock FO1 assets is a reasonable near-term target
- FO1 savegame compatibility is a separate project and should not be on the critical path

## Suggested Tracking Format

Keep a live markdown tracker in this repo with at least these sections:

- Environment
- Boot blockers
- Stock-vs-overlay dependency inventory
- Variant behavior mismatches
- Completed fixes
- Open questions

Recommended file:

- `todo/fallout1-original-assets-bringup-log.md`

## Immediate Next Actions

1. Create a pristine FO1 test directory and stop using the current mixed folder as the acceptance environment.
2. Cherry-pick or manually port `GameVariant` and FO1 DAT support from `fallout1-runtime-bringup`.
3. Verify stock FO1 boot to main menu with no `mods/` dependency.
4. Build an explicit dependency inventory of every file/path that still only works when `fo1_base` is mounted.
5. Convert that inventory into native FO1 loader and behavior work items.

## Bottom Line

The shortest credible route is not "finish the old branch". The shortest credible route is:

- salvage the old branch's runtime infrastructure
- throw away its overlay dependency as a success condition
- then systematically replace each FO2-shaped assumption with a stock-FO1-native path

If the project is executed in that order, it is realistic. If it starts by leaning on `fo1_base` for too long, it will produce a hybrid runtime that is hard to finish and hard to reason about.

## Session Handoff

This section is intentionally short and operational. A new session should be able to start here, then use the rest of the document as reference detail.

### Locked assumptions

- target end state uses original FO1 assets only
- FO2 assets, `Fo1in2`, `mods/fo1_base`, or any other FO2-derived overlay are not acceptable runtime dependencies
- original FO1 save compatibility is out of scope
- if original FO1 saves are encountered, preferred behavior is clear detection plus an unsupported-save error
- minimal bridge assets are acceptable when they are small, declarative, and lower-risk than large engine forks
- bridge assets are not the answer for script ABI, trait/perk semantics, timer/story-state logic, or save ABI

### Preferred architectural guardrails

- preserve shared CE loader/container code where raw formats are still compatible
- put FO1-only policy in separate `fo1_*` modules
- add thin integration points at subsystem boundaries
- avoid scattering FO1 conditionals throughout generic gameplay code
- only fork full subsystem paths when semantic flow is genuinely different end-to-end

### Most likely first implementation slice

1. recover `GameVariant` and FO1 DAT support from `fallout1-runtime-bringup`
2. stand up a pristine FO1 validation folder with no overlay content
3. boot to main menu in FO1 mode without `mods/`
4. get stock FO1 map/proto/script assets loading with no `fo1_base`
5. add only the smallest startup-blocker surfaces needed for first playable new game:
   - startup map/movie gating
   - default FO1 dude look
   - `worldmap.txt` bridge if still needed for CE runtime shape
   - `party.txt` bridge if still needed for init-time CE assumptions

### Most important unresolved risk buckets

- timed story-state logic:
  - water countdown
  - warning/failure movies
  - worldmap death/failure conditions
- trait/perk rules:
  - FO1 `Night Person`
  - FO1 `Skilled`
  - FO1-valid perk table and runtime effects
- endgame logic:
  - FO1 hardcoded slide selection
  - gender-specific ending movie flow
- worldmap/encounter semantics beyond bare loading
- map/bootstrap lifecycle hooks around script init, sidecar `.GAM` globals, and post-load area checks

### Suggested first modules to create

- `fo1_bootstrap`
- `fo1_map_compat`
- `fo1_story_state`
- `fo1_endgame`
- later, if needed:
  - `fo1_trait_rules`
  - `fo1_perk_rules`
  - `fo1_pipboy_data`

### If a new session needs a concrete starting task

Start with:

- port `GameVariant` and FO1 DAT support
- verify stock FO1 `vault13.gam`, `.lst`, proto, art, and script assets resolve with no overlay
- then test whether CE's shared map loader can load stock FO1 maps before adding any deeper FO1 runtime rules

Do not start with:

- broad gameplay rewrites
- endgame polish
- FO1 save support
- large in-place branching across CE combat/stat/item code
