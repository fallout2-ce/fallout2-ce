#ifndef GAME_SOUND_H
#define GAME_SOUND_H

#include "obj_types.h"
#include "sound.h"

namespace fallout {

typedef enum WeaponSoundEffect {
    WEAPON_SOUND_EFFECT_READY,
    WEAPON_SOUND_EFFECT_ATTACK,
    WEAPON_SOUND_EFFECT_OUT_OF_AMMO,
    WEAPON_SOUND_EFFECT_AMMO_FLYING,
    WEAPON_SOUND_EFFECT_HIT,
    WEAPON_SOUND_EFFECT_COUNT,
} WeaponSoundEffect;

typedef enum ScenerySoundEffect {
    SCENERY_SOUND_EFFECT_OPEN,
    SCENERY_SOUND_EFFECT_CLOSED,
    SCENERY_SOUND_EFFECT_LOCKED,
    SCENERY_SOUND_EFFECT_UNLOCKED,
    SCENERY_SOUND_EFFECT_USED,
    SCENERY_SOUND_EFFECT_COUNT,
} ScenerySoundEffect;

typedef enum CharacterSoundEffect {
    CHARACTER_SOUND_EFFECT_UNUSED,
    CHARACTER_SOUND_EFFECT_KNOCKDOWN,
    CHARACTER_SOUND_EFFECT_PASS_OUT,
    CHARACTER_SOUND_EFFECT_DIE,
    CHARACTER_SOUND_EFFECT_CONTACT,
} CharacterSoundEffect;

typedef enum GameSoundReadLimitMode {
    GSOUND_LOAD_NO_PLAY = 10,
    GSOUND_LIMIT_BEFORE = 11,
    GSOUND_LIMIT_AFTER = 12,
} GameSoundReadLimitMode;

typedef enum GameSoundStorageType {
    GSOUND_STORAGE_INVALID = -1,
    GSOUND_MEMORY = 13,
    GSOUND_STREAM = 14,
} GameSoundStorageType;

typedef enum GameSoundLoopingMode {
    GSOUND_LOOPING_INVALID = -1,
    GSOUND_NO_LOOP = 15,
    GSOUND_LOOP = 16,
} GameSoundLoopingMode;

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
extern const SoundFileIO gGameSoundAudioFileIO;
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
int _gsound_play_sfx_file_volume(const char* name, int volume);
Sound* soundEffectLoad(const char* name, Object* object);
Sound* soundEffectLoadWithVolume(const char* name, Object* object, int volume);
void soundEffectDelete(Sound* sound);
int _gsnd_anim_sound(Sound* sound, void* objectPtr);
int soundEffectPlay(Sound* sound);
int _gsound_compute_relative_volume(Object* obj);
char* sfxBuildCharName(Object* object, int anim, int weaponAnimationCode);
char* gameSoundBuildAmbientSoundEffectName(const char* name);
char* gameSoundBuildInterfaceName(const char* name);
char* sfxBuildWeaponName(int effectType, Object* weapon, int hitMode, Object* target);
char* sfxBuildSceneryName(int actionType, int action, const char* name);
char* sfxBuildOpenName(Object* object, int action);
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
