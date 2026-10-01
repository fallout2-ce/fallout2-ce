#include "audio_channels.h"

#include <algorithm>

#include "settings.h"

namespace fallout {

static int audioChannelsGetConfiguredCount(AudioChannelType type)
{
    switch (type) {
    case AUDIO_CHANNEL_MUSIC:
        return settings.sound.music_channels;
    case AUDIO_CHANNEL_SPEECH:
        return settings.sound.speech_channels;
    case AUDIO_CHANNEL_SFX:
        return settings.sound.sfx_channels;
    case AUDIO_CHANNEL_MOVIE:
        return settings.sound.movie_channels;
    case AUDIO_CHANNEL_SCRIPT:
        return settings.sound.script_channels;
    case AUDIO_CHANNEL_FLOAT:
        return settings.sound.float_channels;
    case AUDIO_CHANNEL_PIPBOY:
        return settings.sound.pipboy_channels;
    default:
        return 0;
    }
}

int audioChannelsGetCount(AudioChannelType type)
{
    if (type < 0 || type >= AUDIO_CHANNEL_TYPE_COUNT) {
        return 0;
    }

    const AudioChannelLimits& limits = kAudioChannelLimits[type];
    return std::clamp(audioChannelsGetConfiguredCount(type), limits.min, limits.max);
}

int audioChannelsGetTotal()
{
    int total = 0;
    for (int type = 0; type < AUDIO_CHANNEL_TYPE_COUNT; type++) {
        total += audioChannelsGetCount(static_cast<AudioChannelType>(type));
    }
    return total;
}

} // namespace fallout
