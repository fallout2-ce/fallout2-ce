#include "sfall_sound.h"

#include <string.h>

#include <vector>

#include "audio.h"
#include "debug.h"
#include "game_sound.h"
#include "platform_compat.h"
#include "sound.h"

namespace fallout {

namespace {

enum SfallSoundFlags {
    SFALL_SOUND_FLAG_LOOPING = 0x10000000,
    SFALL_SOUND_FLAG_RESTORE = 0x40000000,
};

struct SfallManagedSound {
    int id;
    Sound* sound;
    bool restoreBackground;
};

std::vector<SfallManagedSound> sfallLoopingSounds;
int sfallLoopId = 0;
int sfallBackgroundReplacementId = 0;

bool sfallSoundPathIsValid(const char* path)
{
    if (path == nullptr) {
        return false;
    }

    size_t length = strlen(path);
    if (length <= 3 || length >= COMPAT_MAX_PATH) {
        return false;
    }

    for (const char* ch = path; *ch != '\0'; ch++) {
        if (*ch == ':') {
            return false;
        }

        if (*ch == '.' && ch[1] == '.') {
            return false;
        }
    }

    return true;
}

int sfallSoundGetBaseVolume(int mode)
{
    switch (mode) {
    case SFALL_SOUND_MODE_MUSIC:
        return backgroundSoundGetVolume();
    case SFALL_SOUND_MODE_SPEECH:
        return speechGetVolume();
    case SFALL_SOUND_MODE_SINGLE:
    case SFALL_SOUND_MODE_LOOP:
    default:
        return soundEffectsGetVolume();
    }
}

int sfallSoundClampVolume(int volume)
{
    if (volume < VOLUME_MIN) {
        return VOLUME_MIN;
    }

    if (volume > VOLUME_MAX) {
        return VOLUME_MAX;
    }

    return volume;
}

int sfallSoundFindLoopingSoundIndexById(int id)
{
    for (size_t index = 0; index < sfallLoopingSounds.size(); index++) {
        if (sfallLoopingSounds[index].id == id) {
            return static_cast<int>(index);
        }
    }

    return -1;
}

Sound* sfallSoundCreate(const char* path, bool looping, int volume)
{
    int soundFlags = SOUND_FLAG_0x02 | SOUND_16BIT;
    int type = SOUND_TYPE_STREAMING;
    if (!looping) {
        type |= SOUND_TYPE_FIRE_AND_FORGET;
    } else {
        soundFlags |= SOUND_LOOPING;
    }

    Sound* sound = soundAllocate(type, soundFlags);
    if (sound == nullptr) {
        return nullptr;
    }

    int rc = soundSetFileIO(sound,
        audioOpen,
        audioClose,
        audioRead,
        audioWrite,
        audioSeek,
        audioTell,
        audioGetSize);
    if (rc != SOUND_NO_ERROR) {
        soundDelete(sound);
        return nullptr;
    }

    if (looping) {
        rc = soundSetLooping(sound, 0xFFFF);
        if (rc != SOUND_NO_ERROR) {
            soundDelete(sound);
            return nullptr;
        }
    }

    char pathCopy[COMPAT_MAX_PATH];
    snprintf(pathCopy, sizeof(pathCopy), "%s", path);

    rc = soundLoad(sound, pathCopy);
    if (rc != SOUND_NO_ERROR) {
        soundDelete(sound);
        return nullptr;
    }

    soundSetVolume(sound, sfallSoundClampVolume(volume));

    rc = soundPlay(sound);
    if (rc != SOUND_NO_ERROR) {
        soundDelete(sound);
        return nullptr;
    }

    return sound;
}

void sfallSoundStopTrackedIndex(int index, bool restoreBackground)
{
    Sound* sound = sfallLoopingSounds[index].sound;
    int id = sfallLoopingSounds[index].id;
    bool shouldRestoreBackground = restoreBackground && sfallLoopingSounds[index].restoreBackground;

    sfallLoopingSounds.erase(sfallLoopingSounds.begin() + index);

    if (sfallBackgroundReplacementId == id) {
        sfallBackgroundReplacementId = 0;
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

} // namespace

int sfallSoundPlay(const char* path, int mode)
{
    if (mode < 0 || !sfallSoundPathIsValid(path)) {
        return 0;
    }

    int volumeReduction = (mode & 0x7FFF0000) >> 16;
    mode &= 0xF;
    if (mode > SFALL_SOUND_MODE_SPEECH) {
        mode = SFALL_SOUND_MODE_MUSIC;
    }

    bool looping = mode == SFALL_SOUND_MODE_LOOP || mode == SFALL_SOUND_MODE_MUSIC;
    int volume = sfallSoundGetBaseVolume(mode) - volumeReduction;

    if (mode == SFALL_SOUND_MODE_MUSIC) {
        if (sfallBackgroundReplacementId != 0) {
            int existingIndex = sfallSoundFindLoopingSoundIndexById(sfallBackgroundReplacementId);
            if (existingIndex != -1) {
                sfallSoundStopTrackedIndex(existingIndex, false);
            }
        } else {
            backgroundSoundDelete();
        }
    }

    Sound* sound = sfallSoundCreate(path, looping, volume);
    if (sound == nullptr) {
        debugPrint("sfallSoundPlay: failed to play %s\n", path);
        return 0;
    }

    if (!looping) {
        return 0;
    }

    int id = ++sfallLoopId;
    id |= SFALL_SOUND_FLAG_LOOPING;
    if (mode == SFALL_SOUND_MODE_MUSIC) {
        id |= SFALL_SOUND_FLAG_RESTORE;
    }

    sfallLoopingSounds.push_back({
        id,
        sound,
        mode == SFALL_SOUND_MODE_MUSIC,
    });

    if (mode == SFALL_SOUND_MODE_MUSIC) {
        sfallBackgroundReplacementId = id;
    }

    return id;
}

void sfallSoundStop(int id)
{
    if ((id & 0xFFFFFF) == 0) {
        return;
    }

    int index = sfallSoundFindLoopingSoundIndexById(id);
    if (index == -1) {
        return;
    }

    sfallSoundStopTrackedIndex(index, true);
}

void sfallSoundReset()
{
    sfallLoopingSounds.clear();
    sfallLoopId = 0;
    sfallBackgroundReplacementId = 0;
}

void sfallSoundExit()
{
    while (!sfallLoopingSounds.empty()) {
        sfallSoundStopTrackedIndex(static_cast<int>(sfallLoopingSounds.size() - 1), false);
    }

    sfallLoopId = 0;
    sfallBackgroundReplacementId = 0;
}

} // namespace fallout
