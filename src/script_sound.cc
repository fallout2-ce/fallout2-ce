#include "script_sound.h"

#include <vector>

#include "debug.h"
#include "game_sound.h"
#include "sound.h"

namespace fallout {

enum ScriptSoundFlags {
    SCRIPT_SOUND_FLAG_LOOPING = 0x10000000,
    SCRIPT_SOUND_FLAG_RESTORE = 0x40000000,
};

struct ScriptManagedSound {
    int id;
    Sound* sound;
    bool restoreBackground;
};

static std::vector<ScriptManagedSound> scriptLoopingSounds;
static int scriptLoopId = 0;
static int currentMusicId = 0;

static int scriptSoundGetBaseVolume(int mode)
{
    switch (mode) {
    case SCRIPT_SOUND_MODE_MUSIC:
        return backgroundSoundGetVolume();
    case SCRIPT_SOUND_MODE_SPEECH:
        return speechGetVolume();
    case SCRIPT_SOUND_MODE_SINGLE:
    case SCRIPT_SOUND_MODE_LOOP:
    default:
        return soundEffectsGetVolume();
    }
}

static int scriptSoundClampVolume(int volume)
{
    if (volume < VOLUME_MIN) {
        return VOLUME_MIN;
    }

    if (volume > VOLUME_MAX) {
        return VOLUME_MAX;
    }

    return volume;
}

static int scriptSoundFindLoopingSoundIndexById(int id)
{
    for (size_t index = 0; index < scriptLoopingSounds.size(); index++) {
        if (scriptLoopingSounds[index].id == id) {
            return static_cast<int>(index);
        }
    }

    return -1;
}

static Sound* scriptSoundCreate(const char* path, bool looping, int volume)
{
    GameSoundLoadOptions loadOptions = {
        GSOUND_LIMIT_AFTER,
        GSOUND_STREAM,
        looping ? GSOUND_LOOP : GSOUND_NO_LOOP,
        0,
        nullptr,
        nullptr,
    };

    Sound* sound = nullptr;
    if (gameSoundLoadSound(&sound, path, &gGameSoundAudioIO, &loadOptions) != 0) {
        return nullptr;
    }

    soundSetVolume(sound, scriptSoundClampVolume(volume));

    int rc;
    rc = soundPlay(sound);
    if (rc != SOUND_NO_ERROR) {
        soundDelete(sound);
        return nullptr;
    }

    return sound;
}

static void scriptSoundStopTrackedIndex(int index, bool restoreBackground)
{
    Sound* sound = scriptLoopingSounds[index].sound;
    int id = scriptLoopingSounds[index].id;
    bool shouldRestoreBackground = restoreBackground && scriptLoopingSounds[index].restoreBackground;

    scriptLoopingSounds.erase(scriptLoopingSounds.begin() + index);

    if (currentMusicId == id) {
        currentMusicId = 0;
    }

    if (sound != nullptr) {
        if (soundIsPlaying(sound)) {
            soundStop(sound);
        }

        soundDelete(sound);
    }

    if (shouldRestoreBackground) {
        backgroundSoundRestart(GSOUND_LIMIT_AFTER);
    }
}

int scriptSoundPlay(const char* path, int mode)
{
    if (mode < 0 || path == nullptr || path[0] == '\0') {
        return 0;
    }

    int volumeReduction = (mode & 0x7FFF0000) >> 16;
    mode &= 0xF;
    if (mode > SCRIPT_SOUND_MODE_SPEECH) {
        mode = SCRIPT_SOUND_MODE_MUSIC;
    }

    bool looping = mode == SCRIPT_SOUND_MODE_LOOP || mode == SCRIPT_SOUND_MODE_MUSIC;
    int volume = scriptSoundGetBaseVolume(mode) - volumeReduction;

    if (mode == SCRIPT_SOUND_MODE_MUSIC) {
        if (currentMusicId != 0) {
            int existingIndex = scriptSoundFindLoopingSoundIndexById(currentMusicId);
            if (existingIndex != -1) {
                scriptSoundStopTrackedIndex(existingIndex, false);
            }
        } else {
            backgroundSoundDelete();
        }
    }

    Sound* sound = scriptSoundCreate(path, looping, volume);
    if (sound == nullptr) {
        if (mode == SCRIPT_SOUND_MODE_MUSIC) {
            backgroundSoundRestart(GSOUND_LIMIT_AFTER);
        }
        debugPrint("scriptSoundPlay: failed to play %s\n", path);
        return 0;
    }

    if (!looping) {
        return 0;
    }

    int id = ++scriptLoopId;
    id |= SCRIPT_SOUND_FLAG_LOOPING;
    if (mode == SCRIPT_SOUND_MODE_MUSIC) {
        id |= SCRIPT_SOUND_FLAG_RESTORE;
    }

    scriptLoopingSounds.push_back({
        id,
        sound,
        mode == SCRIPT_SOUND_MODE_MUSIC,
    });

    if (mode == SCRIPT_SOUND_MODE_MUSIC) {
        currentMusicId = id;
    }

    return id;
}

void scriptSoundStop(int id)
{
    if ((id & 0xFFFFFF) == 0) {
        return;
    }

    int index = scriptSoundFindLoopingSoundIndexById(id);
    if (index == -1) {
        return;
    }

    scriptSoundStopTrackedIndex(index, true);
}

void scriptSoundReset()
{
    scriptSoundExit();
}

void scriptSoundExit()
{
    while (!scriptLoopingSounds.empty()) {
        scriptSoundStopTrackedIndex(static_cast<int>(scriptLoopingSounds.size() - 1), false);
    }

    scriptLoopId = 0;
    currentMusicId = 0;
}

} // namespace fallout
