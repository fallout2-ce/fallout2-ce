#ifndef FALLOUT_SCRIPT_SOUND_H_
#define FALLOUT_SCRIPT_SOUND_H_

namespace fallout {

enum ScriptSoundMode : int {
    SCRIPT_SOUND_MODE_SINGLE = 0,
    SCRIPT_SOUND_MODE_LOOP = 1,
    SCRIPT_SOUND_MODE_MUSIC = 2,
    SCRIPT_SOUND_MODE_SPEECH = 3,
};

constexpr inline bool scriptSoundModeIsValid(int mode)
{
    return mode >= SCRIPT_SOUND_MODE_SINGLE && mode <= SCRIPT_SOUND_MODE_SPEECH;
}

// Sizes the script channel pool, see `script_channels` in fallout2.cfg.
void scriptSoundInit();
int scriptSoundPlay(const char* path, ScriptSoundMode mode, int volumeReduction = 0);
void scriptSoundStop(int id);
void scriptSoundReset();
void scriptSoundExit();

} // namespace fallout

#endif /* FALLOUT_SCRIPT_SOUND_H_ */
