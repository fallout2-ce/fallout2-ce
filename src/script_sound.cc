#include "script_sound.h"

#include <vector>

#include "audio_channels.h"
#include "debug.h"
#include "game_sound.h"
#include "sound.h"

namespace fallout {

enum ScriptSoundFlags {
    SCRIPT_SOUND_FLAG_LOOPING = 0x10000000,
    SCRIPT_SOUND_FLAG_RESTORE = 0x40000000,
};

// One channel of the script sound pool. Looping sounds keep an [id] so
// scripts can stop them later. One-shots have no id and free their channel
// when they finish. [serial] orders sounds by start time for eviction.
struct ScriptSoundSlot {
    Sound* sound = nullptr;
    int id = 0;
    bool restoreBackground = false;
    unsigned int serial = 0;
};

// Sized once in [scriptSoundInit] so slot addresses stay valid as callback
// data.
static std::vector<ScriptSoundSlot> scriptSoundSlots;
static unsigned int scriptSoundNextSerial = 0;
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

static ScriptSoundSlot* scriptSoundFindLoopingSlotById(int id)
{
    for (ScriptSoundSlot& slot : scriptSoundSlots) {
        if (slot.sound != nullptr && slot.id == id) {
            return &slot;
        }
    }

    return nullptr;
}

static void scriptSoundSlotCallback(void* userData, int event)
{
    if (event == SOUND_CALLBACK_EVENT_DONE) {
        static_cast<ScriptSoundSlot*>(userData)->sound = nullptr;
    }
}

static Sound* scriptSoundCreate(const char* path, bool looping, int volume, ScriptSoundSlot* slot)
{
    GameSoundLoadOptions loadOptions = {
        GSOUND_LIMIT_AFTER,
        GSOUND_STREAM,
        looping ? GSOUND_LOOP : GSOUND_NO_LOOP,
        0,
        scriptSoundSlotCallback,
        slot,
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

static void scriptSoundStopSlot(ScriptSoundSlot* slot, bool restoreBackground)
{
    Sound* sound = slot->sound;
    int id = slot->id;
    bool shouldRestoreBackground = restoreBackground && slot->restoreBackground;

    *slot = ScriptSoundSlot();

    if (id != 0 && currentMusicId == id) {
        currentMusicId = 0;
    }

    if (sound != nullptr) {
        if (id != 0 && soundIsPlaying(sound)) {
            soundStop(sound);
        }

        soundDelete(sound);
    }

    if (shouldRestoreBackground) {
        backgroundSoundRestart(GSOUND_LIMIT_AFTER);
    }
}

// Returns a free channel. If every channel is busy, the oldest one-shot is
// stopped to make room. Looping sounds are never evicted, since the script
// still owns them, so this fails when every channel holds a loop.
static ScriptSoundSlot* scriptSoundAcquireSlot()
{
    ScriptSoundSlot* oldestOneShot = nullptr;
    for (ScriptSoundSlot& slot : scriptSoundSlots) {
        if (slot.sound == nullptr) {
            return &slot;
        }

        if (slot.id == 0 && (oldestOneShot == nullptr || slot.serial < oldestOneShot->serial)) {
            oldestOneShot = &slot;
        }
    }

    if (oldestOneShot != nullptr) {
        scriptSoundStopSlot(oldestOneShot, false);
    }

    return oldestOneShot;
}

void scriptSoundInit()
{
    scriptSoundSlots.assign(audioChannelsGetCount(AUDIO_CHANNEL_SCRIPT), ScriptSoundSlot());
    scriptSoundNextSerial = 0;
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
            ScriptSoundSlot* existing = scriptSoundFindLoopingSlotById(currentMusicId);
            if (existing != nullptr) {
                scriptSoundStopSlot(existing, false);
            }
        } else {
            backgroundSoundDelete();
        }
    }

    ScriptSoundSlot* slot = scriptSoundAcquireSlot();
    Sound* sound = slot != nullptr ? scriptSoundCreate(path, looping, volume, slot) : nullptr;
    if (sound == nullptr) {
        if (mode == SCRIPT_SOUND_MODE_MUSIC) {
            backgroundSoundRestart(GSOUND_LIMIT_AFTER);
        }
        debugPrint("scriptSoundPlay: failed to play %s\n", path);
        return 0;
    }

    slot->sound = sound;
    slot->serial = scriptSoundNextSerial++;

    if (!looping) {
        return 0;
    }

    int id = ++scriptLoopId;
    id |= SCRIPT_SOUND_FLAG_LOOPING;
    if (mode == SCRIPT_SOUND_MODE_MUSIC) {
        id |= SCRIPT_SOUND_FLAG_RESTORE;
    }

    slot->id = id;
    slot->restoreBackground = mode == SCRIPT_SOUND_MODE_MUSIC;

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

    ScriptSoundSlot* slot = scriptSoundFindLoopingSlotById(id);
    if (slot == nullptr) {
        return;
    }

    scriptSoundStopSlot(slot, true);
}

void scriptSoundReset()
{
    scriptSoundExit();
}

void scriptSoundExit()
{
    for (ScriptSoundSlot& slot : scriptSoundSlots) {
        if (slot.sound != nullptr) {
            scriptSoundStopSlot(&slot, false);
        }
    }

    scriptLoopId = 0;
    currentMusicId = 0;
}

} // namespace fallout
