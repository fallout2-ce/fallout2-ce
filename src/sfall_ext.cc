#include "sfall_ext.h"

#include <algorithm>
#include <string>

#include "db.h"
#include "debug.h"
#include "platform_compat.h"
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
            char** fileList;
            int fileListLength = fileNameListInit("mods\\*.dat", &fileList);

            for (int index = 0; index < fileListLength; index++) {
                fileWriteString(fileList[index], stream);
                fileWriteString("\n", stream);
            }
            fileClose(stream);
            fileNameListFree(&fileList, 0);
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

    // Write zeros for CE-unimplemented fields: nextObjectId, addedYears,
    // fakeTraitsCount, fakePerksCount, fakeSelectablePerksCount
    int zero = 0;
    for (int i = 0; i < 5; i++) {
        if (fileWrite(&zero, sizeof(zero), 1, stream) != 1) {
            debugPrint("LOADSAVE (SFALL): ** Error saving stub fields **\n");
            return false;
        }
    }

    if (!sfallArraysSave(stream)) {
        debugPrint("LOADSAVE (SFALL): ** Error saving arrays **\n");
        return false;
    }

    if (fileWrite(&zero, sizeof(zero), 1, stream) != 1) { // drugPidsCount
        debugPrint("LOADSAVE (SFALL): ** Error saving drug pids **\n");
        return false;
    }

    return true;
}

bool sfallLoadGameData(File* stream)
{
    if (!sfall_gl_vars_load(stream)) {
        debugPrint("LOADSAVE (SFALL): ** Error loading global vars **\n");
        return false;
    }

    // Skip sections CE doesn't implement: nextObjectId, addedYears,
    // fakeTraitsCount, fakePerksCount, fakeSelectablePerksCount
    int ignored;
    for (int i = 0; i < 5; i++) {
        if (fileRead(&ignored, sizeof(ignored), 1, stream) != 1) return true; // old save, stop gracefully
    }

    if (!sfallArraysLoad(stream)) {
        // Corrupted save.
        debugPrint("LOADSAVE (SFALL): ** Error loading arrays **\n");
        return false;
    }

    return true;
}

} // namespace fallout
