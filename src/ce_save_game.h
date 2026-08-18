#ifndef CE_SAVE_GAME_H
#define CE_SAVE_GAME_H

namespace fallout {

bool ceSaveGameData(const char* path);
void ceLoadGameData(const char* path);

} // namespace fallout

#endif /* CE_SAVE_GAME_H */
