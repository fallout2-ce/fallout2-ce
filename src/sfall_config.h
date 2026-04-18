#ifndef SFALL_CONFIG_H
#define SFALL_CONFIG_H

#include "config.h"

namespace fallout {

#define SFALL_CONFIG_FILE_NAME "ddraw.ini"

#define SFALL_CONFIG_MAIN_KEY "Main"
#define SFALL_CONFIG_MISC_KEY "Misc"
#define SFALL_CONFIG_SCRIPTS_KEY "Scripts"

#define SFALL_CONFIG_OVERRIDE_CRITICALS_MODE_KEY "OverrideCriticalTable"
#define SFALL_CONFIG_OVERRIDE_CRITICALS_FILE_KEY "OverrideCriticalFile"
#define SFALL_CONFIG_BOOKS_FILE_KEY "BooksFile"
#define SFALL_CONFIG_ELEVATORS_FILE_KEY "ElevatorsFile"
#define SFALL_CONFIG_PREMADE_CHARACTERS_FILE_NAMES_KEY "PremadePaths"
#define SFALL_CONFIG_PREMADE_CHARACTERS_FACE_FIDS_KEY "PremadeFIDs"
#define SFALL_CONFIG_UNARMED_FILE_KEY "UnarmedFile"
#define SFALL_CONFIG_EXTRA_MESSAGE_LISTS_KEY "ExtraGameMsgFileList"
#define SFALL_CONFIG_INI_CONFIG_FOLDER "IniConfigFolder"
#define SFALL_CONFIG_CONFIG_FILE "ConfigFile"
#define SFALL_CONFIG_PATCH_FILE "PatchFile"

extern bool gSfallConfigInitialized;
extern Config gSfallConfig;

bool sfallConfigInit(int argc, char** argv);
void sfallConfigExit();

} // namespace fallout

#endif /* SFALL_CONFIG_H */
