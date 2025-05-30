# Testing sFall scripting features

## Basic info

This folder contains scripts which can be used for sFall opcodes.

## Build script using sFall Script Editor

1. Get "Fallout sFall Script Editor v4.1.7.RC1", for example from https://nuclear-grot.ucoz.net/forum/12-20-1

2. Run script editor, add "sfall_headers" into "Location folder of headers files" in the options

3. Open any script here and compile it

4. This shoud create compiled script <file.int>


## Run test script

1. Move compiled .int file into game folder as `mods/tests/SCRIPTS/OBJ_DUDE.INT` (case-sensitive!)

2. Change `mods/mods_order.txt` file: add a line `tests` there

3. Run game, check that game displays message about tests

4. Comment tests line in `mods/mods_order.txt` in order to enable game back



## TODO

- Use custom global script instead of `obj_dude`
- Add note about compile.exe and https://marketplace.visualstudio.com/items?itemName=BGforge.bgforge-mls
