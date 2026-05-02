#ifndef FALLOUT_SFALL_SOUND_H_
#define FALLOUT_SFALL_SOUND_H_

namespace fallout {

enum SfallSoundMode {
    SFALL_SOUND_MODE_SINGLE = 0,
    SFALL_SOUND_MODE_LOOP = 1,
    SFALL_SOUND_MODE_MUSIC = 2,
    SFALL_SOUND_MODE_SPEECH = 3,
};

int sfallSoundPlay(const char* path, int mode);
void sfallSoundStop(int id);
void sfallSoundReset();
void sfallSoundExit();

} // namespace fallout

#endif /* FALLOUT_SFALL_SOUND_H_ */
