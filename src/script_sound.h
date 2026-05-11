#ifndef FALLOUT_SCRIPT_SOUND_H_
#define FALLOUT_SCRIPT_SOUND_H_

namespace fallout {

enum ScriptSoundMode {
    SCRIPT_SOUND_MODE_SINGLE = 0,
    SCRIPT_SOUND_MODE_LOOP = 1,
    SCRIPT_SOUND_MODE_MUSIC = 2,
    SCRIPT_SOUND_MODE_SPEECH = 3,
};

int scriptSoundPlay(const char* path, int mode);
void scriptSoundStop(int id);
void scriptSoundReset();
void scriptSoundExit();

} // namespace fallout

#endif /* FALLOUT_SCRIPT_SOUND_H_ */
