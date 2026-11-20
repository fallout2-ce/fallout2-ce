#ifndef SFALL_EXT_H
#define SFALL_EXT_H

struct XFile;
typedef XFile File;

namespace fallout {

void sfallLoadMods();
bool sfallSaveGameData(File* stream);
bool sfallLoadGameData(File* stream);

} // namespace fallout

#endif /* SFALL_EXT_H */
