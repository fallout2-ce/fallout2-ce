#include "sfall_ext.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "db.h"
#include "debug.h"
#include "file_find.h"
#include "item.h"
#include "platform_compat.h"
#include "scripts.h"
#include "sfall_arrays.h"
#include "sfall_global_vars.h"

namespace fallout {

/**
 * Load mods from the mod directory
 */
void sfallLoadMods()
{
    // SFALL: additional mods from the mods directory / mods_order.txt
    const char* modsPath = "mods";
    const char* loadOrderFilename = "mods_order.txt";

    char loadOrderFilepath[COMPAT_MAX_PATH];
    compat_makepath(loadOrderFilepath, nullptr, modsPath, loadOrderFilename, nullptr);

    // If the mods folder does not exist, create it.
    compat_mkdir(modsPath);

    // If load order file does not exist, initialize it automatically with mods already in the mods folder.
    if (compat_access(loadOrderFilepath, 0) != 0) {
        debugPrint("Generating Mods Order file based on the contents of Mods folder: %s\n", loadOrderFilepath);

        File* stream = fileOpen(loadOrderFilepath, "wt");
        if (stream != nullptr) {
            // SFALL: the .dat files and folders of the mods folder (sfall
            // loads folders named *.dat too), sorted.
            std::vector<std::string> mods;
            char pattern[COMPAT_MAX_PATH];
            compat_makepath(pattern, nullptr, modsPath, "*.dat", nullptr);
            compat_windows_path_to_native(pattern);
            DirectoryFileFindData findData;
            if (fileFindFirst(pattern, &findData)) {
                do {
                    std::string name = fileFindGetName(&findData);
                    // A longer extension the pattern lets through (Windows
                    // matches "*.dat" with short names of "*.data").
                    size_t dot = name.find_last_of('.');
                    if (dot != std::string::npos && name.length() - dot <= 4) {
                        mods.push_back(name);
                    }
                } while (fileFindNext(&findData));
                findFindClose(&findData);
            }
            std::sort(mods.begin(), mods.end());

            for (const std::string& mod : mods) {
                fileWriteString(mod.c_str(), stream);
                fileWriteString("\n", stream);
            }
            fileClose(stream);
        }
    }

    // Add mods from load order file.
    File* stream = fileOpen(loadOrderFilepath, "r");
    if (stream != nullptr) {
        int numMods = 0;
        char mod[COMPAT_MAX_PATH];
        while (fileReadString(mod, COMPAT_MAX_PATH, stream)) {
            std::string modPath { mod };

            if (modPath.find_first_of(";#") != std::string::npos)
                continue; // skip comments

            // ltrim
            modPath.erase(modPath.begin(), std::find_if(modPath.begin(), modPath.end(), [](unsigned char ch) {
                return !isspace(ch);
            }));

            // rtrim
            modPath.erase(std::find_if(modPath.rbegin(), modPath.rend(), [](unsigned char ch) {
                return !isspace(ch);
            }).base(),
                modPath.end());

            if (modPath.empty())
                continue; // skip empty lines

            char normalizedModPath[COMPAT_MAX_PATH];
            compat_makepath(normalizedModPath, nullptr, modsPath, modPath.c_str(), nullptr);

            if (compat_access(normalizedModPath, 0) == 0) {
                debugPrint("Loading mod %s\n", normalizedModPath);
                if (dbOpen(normalizedModPath, nullptr) != -1) {
                    numMods++;
                } else {
                    debugPrint("Error opening mod %s\n", normalizedModPath);
                }
            } else {
                debugPrint("Skipping invalid mod entry %s in %s\n", normalizedModPath, loadOrderFilepath);
            }
        }
        fileClose(stream);
        debugPrint("Loaded %d mods from %s\n", numMods, loadOrderFilepath);
    } else {
        debugPrint("Error opening %s for read\n", loadOrderFilepath);
    }
}

// SFALL: sfall writes the drug pids with raw 4-byte writes, so they are
// little-endian (unlike `fileWriteInt32`).
static bool sfallWriteInt32LE(File* stream, int32_t value)
{
    uint32_t bits = static_cast<uint32_t>(value);
    unsigned char bytes[4] = {
        static_cast<unsigned char>(bits & 0xFF),
        static_cast<unsigned char>((bits >> 8) & 0xFF),
        static_cast<unsigned char>((bits >> 16) & 0xFF),
        static_cast<unsigned char>((bits >> 24) & 0xFF),
    };
    return fileWrite(bytes, sizeof(bytes), 1, stream) == 1;
}

static bool sfallReadInt32LE(File* stream, int32_t* valuePtr)
{
    unsigned char bytes[4];
    if (fileRead(bytes, sizeof(bytes), 1, stream) != 1) {
        return false;
    }

    *valuePtr = static_cast<int32_t>(static_cast<uint32_t>(bytes[0])
        | (static_cast<uint32_t>(bytes[1]) << 8)
        | (static_cast<uint32_t>(bytes[2]) << 16)
        | (static_cast<uint32_t>(bytes[3]) << 24));
    return true;
}

// Binary layout of sfallgv.sav (must match sfall's SaveGame2 / LoadGame_Before order):
//   global vars | nextObjectId(4) | addedYears(4) | fakeTraitsCount(4) |
//   fakePerksCount(4) | fakeSelectablePerksCount(4) | arrays | drugPidsCount(4)
//
// Sections that CE doesn't implement are written/read as zero.

bool sfallSaveGameData(File* stream)
{
    if (!sfall_gl_vars_save(stream)) {
        debugPrint("LOADSAVE (SFALL): ** Error saving global vars **\n");
        return false;
    }

    if (fileWriteInt32(stream, scriptsGetUniqueObjectIdCounter()) == -1) {
        debugPrint("LOADSAVE (SFALL): ** Error saving next object id **\n");
        return false;
    }

    // Write zeros for CE-unimplemented fields: addedYears, fakeTraitsCount,
    // fakePerksCount, fakeSelectablePerksCount
    int32_t zero = 0;
    for (int32_t i = 0; i < 4; i++) {
        if (fileWrite(&zero, sizeof(zero), 1, stream) != 1) {
            debugPrint("LOADSAVE (SFALL): ** Error saving stub fields **\n");
            return false;
        }
    }

    if (!sfallArraysSave(stream)) {
        debugPrint("LOADSAVE (SFALL): ** Error saving arrays **\n");
        return false;
    }

    // SFALL: The drug pids of the drug effect events (see
    // `drugEffectEventsRestorePids`).
    std::vector<int> drugPids = drugEffectEventsGetPids();
    if (!sfallWriteInt32LE(stream, static_cast<int32_t>(drugPids.size()))) {
        debugPrint("LOADSAVE (SFALL): ** Error saving drug pids **\n");
        return false;
    }

    for (int drugPid : drugPids) {
        if (!sfallWriteInt32LE(stream, drugPid)) {
            debugPrint("LOADSAVE (SFALL): ** Error saving drug pids **\n");
            return false;
        }
    }

    return true;
}

bool sfallLoadGameData(File* stream)
{
    if (!sfall_gl_vars_load(stream)) {
        debugPrint("LOADSAVE (SFALL): ** Error loading global vars **\n");
        return false;
    }

    int32_t nextObjectId;
    if (fileReadInt32(stream, &nextObjectId) == -1) {
        scriptsRestoreUniqueObjectIdCounter(OBJECT_ID_UNIQUE_START);
        return true; // old save, stop gracefully
    }

    // Skip sections CE doesn't implement: addedYears, fakeTraitsCount,
    // fakePerksCount, fakeSelectablePerksCount
    int32_t ignored;
    for (int32_t i = 0; i < 4; i++) {
        if (fileRead(&ignored, sizeof(ignored), 1, stream) != 1) {
            scriptsRestoreUniqueObjectIdCounter(nextObjectId);
            return true; // old save, stop gracefully
        }
    }

    if (!sfallArraysLoad(stream)) {
        // Corrupted save.
        debugPrint("LOADSAVE (SFALL): ** Error loading arrays **\n");
        return false;
    }

    scriptsRestoreUniqueObjectIdCounter(nextObjectId);

    // SFALL: The drug pids of the drug effect events (see
    // `drugEffectEventsRestorePids`). Older saves don't have them. A list
    // cut short is dropped as a whole: the pids are matched to the events by
    // their order.
    int32_t drugPidsCount;
    if (sfallReadInt32LE(stream, &drugPidsCount) && drugPidsCount > 0) {
        std::vector<int> drugPids;
        for (int32_t i = 0; i < drugPidsCount; i++) {
            int32_t drugPid;
            if (!sfallReadInt32LE(stream, &drugPid)) {
                debugPrint("LOADSAVE (SFALL): ** Drug pids cut short (%d of %d), ignored **\n", i, drugPidsCount);
                drugPids.clear();
                break;
            }
            drugPids.push_back(drugPid);
        }
        drugEffectEventsSetLoadedPids(drugPids);
    }

    return true;
}

} // namespace fallout
