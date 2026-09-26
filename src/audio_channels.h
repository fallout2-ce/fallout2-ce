#ifndef AUDIO_CHANNELS_H
#define AUDIO_CHANNELS_H

namespace fallout {

// Every sound the game plays occupies one audio engine channel. Channels are
// split into pools by type, and the audio engine allocates exactly as many
// channels as all pools together (see `audioChannelsGetTotal`).
//
// To add a new pool: append a type here, add its limits to
// `kAudioChannelLimits`, add a `<name>_channels` key to `SoundSettings`, and
// map it in `audioChannelsGetConfiguredCount`.
enum AudioChannelType {
    AUDIO_CHANNEL_MUSIC,
    AUDIO_CHANNEL_SPEECH,
    AUDIO_CHANNEL_SFX,
    AUDIO_CHANNEL_MOVIE,
    AUDIO_CHANNEL_SCRIPT,
    AUDIO_CHANNEL_FLOAT,
    AUDIO_CHANNEL_PIPBOY,
    AUDIO_CHANNEL_TYPE_COUNT,
};

struct AudioChannelLimits {
    int min;
    int max;
};

// Allowed channel count per pool. Values outside these bounds in
// fallout2.cfg are clamped. Pools with `min == max` are fixed for now, but
// can be opened up later by raising `max`.
constexpr AudioChannelLimits kAudioChannelLimits[AUDIO_CHANNEL_TYPE_COUNT] = {
    /* AUDIO_CHANNEL_MUSIC  */ { 1, 1 },
    /* AUDIO_CHANNEL_SPEECH */ { 1, 1 },
    /* AUDIO_CHANNEL_SFX    */ { 4, 16 },
    /* AUDIO_CHANNEL_MOVIE  */ { 1, 1 },
    /* AUDIO_CHANNEL_SCRIPT */ { 1, 16 },
    /* AUDIO_CHANNEL_FLOAT  */ { 1, 8 },
    /* AUDIO_CHANNEL_PIPBOY */ { 1, 1 },
};

int audioChannelsGetCount(AudioChannelType type);
int audioChannelsGetTotal();

} // namespace fallout

#endif /* AUDIO_CHANNELS_H */
