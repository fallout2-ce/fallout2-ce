#ifndef GAME_SOUND_H
#define GAME_SOUND_H

#include "animation_defs.h"
#include "art_defs.h"
#include "combat_defs.h"
#include "obj_types.h"
#include "sound.h"

namespace fallout {

enum class CharacterSoundEffect : int {
    Unused,
    KnockDown,
    PassOut,
    Die,
    Contact
};

constexpr inline bool characterSoundEffectIsValid(int soundEffect)
{
    return soundEffect >= static_cast<int>(CharacterSoundEffect::Unused) && soundEffect <= static_cast<int>(CharacterSoundEffect::Contact);
}

enum class WeaponSoundEffect : int {
    Ready,
    Attack,
    OutOfAmmo,
    AmmoFlying,
    Hit
};

constexpr inline bool weaponSoundEffectIsValid(int effect)
{
    return effect >= static_cast<int>(WeaponSoundEffect::Ready) && effect <= static_cast<int>(WeaponSoundEffect::Hit);
}

enum class ScenerySoundEffect : int {
    Open,
    Closed,
    Locked,
    Unlocked,
    Used
};

constexpr inline bool scenerySoundEffectIsValid(int effect)
{
    return effect >= static_cast<int>(ScenerySoundEffect::Open) && effect <= static_cast<int>(ScenerySoundEffect::Used);
}

enum class SoundEffectActionType : int {
    Active,
    Passive,
};

constexpr inline bool soundEffectActionTypeIsValid(int actionType)
{
    return actionType == static_cast<int>(SoundEffectActionType::Active) || actionType == static_cast<int>(SoundEffectActionType::Passive);
}

enum class GameSoundReadLimitMode : int {
    LoadNoPlay = 10,
    LimitBefore = 11,
    LimitAfter = 12,
};

enum class GameSoundStorageType : int {
    Invalid = -1,
    Memory = 13,
    Stream = 14,
};

enum class GameSoundLoopingMode : int {
    Invalid = -1,
    NoLoop = 15,
    Loop = 16,
};

typedef struct GameSoundLoadOptions {
    GameSoundReadLimitMode readLimitMode;
    GameSoundStorageType storageType;
    GameSoundLoopingMode loopingMode;
    int channels;
    SoundCallback* callback;
    void* callbackUserData;
} GameSoundLoadOptions;

typedef void(SoundEndCallback)();

extern int gMusicVolume;
extern const SoundFileIO gGameSoundAudioIO;

int gameSoundInit();
void gameSoundReset();
int gameSoundExit();
int gameSoundSetMasterVolume(int value);
int gameSoundGetMasterVolume();
int soundEffectsSetVolume(int value);
int soundEffectsGetVolume();
int backgroundSoundIsEnabled();
void backgroundSoundSetVolume(int value);
int backgroundSoundGetVolume();
int _gsound_background_volume_get_set(int volume);
void backgroundSoundSetEndCallback(SoundEndCallback* callback);
int backgroundSoundLoad(const char* fileName, GameSoundReadLimitMode readLimitMode, GameSoundStorageType storageType, GameSoundLoopingMode loopingMode);
int _gsound_background_play_level_music(const char* fileName, GameSoundReadLimitMode readLimitMode);
const char* gameSoundGetMusicOverride(const char* key, const char* defaultValue);
void backgroundSoundDelete();
void backgroundSoundRestart(GameSoundReadLimitMode readLimitMode);
void backgroundSoundPause();
void backgroundSoundResume();
bool backgoundSoundIsPlaying();
int gameSoundLoadSound(Sound** soundPtr, const char* path, const SoundFileIO* fileIO, const GameSoundLoadOptions* options);
int speechIsEnabled();
void speechSetVolume(int value);
int speechGetVolume();
void speechSetEndCallback(SoundEndCallback* callback);
int speechGetDuration();
int speechLoad(const char* fileName, GameSoundReadLimitMode readLimitMode, GameSoundStorageType storageType, GameSoundLoopingMode loopingMode);
int _gsound_speech_play_preloaded();
void speechDelete();

// Plays a voiced float line (speech file name without extension) on the float
// channel pool at speech volume. Evicts the oldest float when every channel is
// busy. Falls back to sound\sfx\ when the file is not in the speech folders,
// like sfall.
// [speaker] (optional) keeps one line per object: a new line from the same
// speaker replaces the one still playing.
int floatSoundPlay(const char* fileName, Object* speaker = nullptr);
void floatSoundStopAll();

// Plays a voiced Pip-Boy line (file name in sound\pipboy\ without extension,
// e.g. a holodisk narration) on the Pip-Boy channel pool at speech volume.
int pipboySoundPlay(const char* fileName);
void pipboySoundStop();

int _gsound_play_sfx_file_volume(const char* name, int volume);
Sound* soundEffectLoad(const char* name, Object* object);
Sound* soundEffectLoadWithVolume(const char* name, Object* object, int volume);
void soundEffectDelete(Sound* sound);
int _gsnd_anim_sound(Sound* sound, void* objectPtr);
int soundEffectPlay(Sound* sound);
int _gsound_compute_relative_volume(Object* obj);
char* sfxBuildCharName(Object* object, AnimationType anim, CharacterSoundEffect soundEffect);
char* gameSoundBuildAmbientSoundEffectName(const char* name);
char* gameSoundBuildInterfaceName(const char* name);
char* sfxBuildWeaponName(WeaponSoundEffect effectType, Object* weapon, HitMode hitMode, Object* target);
char* sfxBuildSceneryName(SoundEffectActionType actionType, ScenerySoundEffect action, const char* name);
char* sfxBuildOpenName(Object* object, ScenerySoundEffect action);
void _gsound_red_butt_press(int btn, int keyCode);
void _gsound_red_butt_release(int btn, int keyCode);
void _gsound_toggle_butt_press_(int btn, int keyCode);
void _gsound_med_butt_press(int btn, int keyCode);
void _gsound_med_butt_release(int btn, int keyCode);
void _gsound_lrg_butt_press(int btn, int keyCode);
void _gsound_lrg_butt_release(int btn, int keyCode);
int soundPlayFile(const char* name);
int _gsound_sfx_q_start();
int ambientSoundEffectEventProcess(Object* object, void* data);

} // namespace fallout

#endif /* GAME_SOUND_H */
