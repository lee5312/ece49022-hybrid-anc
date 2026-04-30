#pragma once

#define AUDIO_SAMPLE_RATE          48000
#define AUDIO_BLOCK                128

#define AUDIO_CHANNELS_IN           4  // FINAL: 4 logical channels
#define AUDIO_CHANNELS_OUT          2  // start simple (stereo)

// 3 DACS, so may need 6 audio channels out

// Channel mapping
#define CH_CAL_L  0
#define CH_FB_L   1
#define CH_CAL_R  2
#define CH_FB_R   3