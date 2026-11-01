# Testing sFall scripting features

## Basic info

This folder contains scripts which can be used for sFall opcodes.

## Building

Get latest `compile.exe` from [modders pack](https://sourceforge.net/projects/sfall/files/Modders%20pack/) or from [repository](https://github.com/sfall-team/sslc) and run it this way:

```sh
compile.exe -q -p -l -O2 -d -s -n -I<sfall_headers_id> <script_name.ssl>
```

(sfall headers can be found in modderspack)


### Build script using sFall Script Editor

- Get "Fallout sFall Script Editor v4.1.7.RC1", for example from https://nuclear-grot.ucoz.net/forum/12-20-1

- Run script editor, add "sfall_headers" into "Location folder of headers files" in the options

- Open any script here and compile it

- This shoud create compiled script <file.int>


### Build script using VSCode extension

- Install [VSCode Extension](https://marketplace.visualstudio.com/items?itemName=BGforge.bgforge-mls)

## Run test script

1. Move compiled `.int` file into game folder as `scripts/gl_<script_name>.int`

2. Run game, check that game displays message about tests



### NPC level integration test

`gl_test_npc_level.ssl` runs automatically after loading a save. It requires
vanilla Fallout 2 `party.txt` and Myron's four level prototypes, plus a disposable
save where Myron has never joined the party. The test creates a temporary Myron,
checks PID and case-insensitive name calls, dismissed-member behavior, all four
stages' copied stats and skills, healing, and the maximum level, then exits.
Do not save afterward: it changes Myron's prototype and party-level state in
memory. Remove the installed test script before normal play.

The standalone `tests/npc_level_test.cc` additionally checks one-stage and
ten-stage definitions, forced-level bookkeeping, subsequent automatic leveling,
disabled leveling, failed prototype loads, and reset behavior. Its compile
command is at the top of that file.
