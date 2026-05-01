#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/pio_instructions.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "pico/multicore.h"
#include "pico/time.h"

namespace {

constexpr uint MIC_PIN_BCLK = 0;
constexpr uint MIC_PIN_LRCLK = 1;
constexpr uint MIC_PIN_DOUT = 2;

constexpr uint DEFAULT_SAMPLE_RATE_HZ = 8000;
constexpr uint DEFAULT_FRAMES_PER_REPORT = 1024;
constexpr uint DEFAULT_WAVE_DECIMATION = 16;
constexpr uint DEFAULT_PCM_MS = 5000;
constexpr uint PCM_PROBE_FRAMES = 1024;
constexpr uint MAX_PCM_MS = 10000;
constexpr uint MAX_DIFF_PCM_MS = 600000;
constexpr uint MIN_SAMPLE_RATE_HZ = 8000;
constexpr uint MAX_SAMPLE_RATE_HZ = 48000;
constexpr uint MIN_FRAMES_PER_REPORT = 64;
constexpr uint MAX_FRAMES_PER_REPORT = 4096;
constexpr uint MIN_WAVE_DECIMATION = 1;
constexpr uint MAX_WAVE_DECIMATION = 256;
constexpr uint DEFAULT_OUTPUT_STREAM_DECIMATION = 16;
constexpr uint MIN_OUTPUT_STREAM_DECIMATION = 4;
constexpr uint MAX_OUTPUT_STREAM_DECIMATION = 256;
constexpr uint SPEAKER_PWM_PIN = 10;
constexpr uint SPEAKER_PWM_WRAP = 1023;
constexpr uint SPEAKER_FFT_N = 128;
constexpr uint SPEAKER_FFT_HOP = 64;
constexpr uint SPEAKER_FFT_BINS = (SPEAKER_FFT_N / 2) + 1;
constexpr uint SPEAKER_FFT_MASK = SPEAKER_FFT_N - 1;
constexpr uint SPEAKER_OUTPUT_FIFO_LEN = 512;
constexpr uint SPEAKER_OUTPUT_FIFO_MASK = SPEAKER_OUTPUT_FIFO_LEN - 1;
constexpr uint SPEAKER_PLAYBACK_FIFO_LEN = 1024;
constexpr uint SPEAKER_PLAYBACK_FIFO_MASK = SPEAKER_PLAYBACK_FIFO_LEN - 1;
constexpr uint SPEAKER_PLAYBACK_PREBUFFER = 320;
constexpr uint32_t SPEAKER_STARTUP_FADE_FRAMES = DEFAULT_SAMPLE_RATE_HZ / 10;
constexpr float PI_F = 3.14159265358979323846f;
constexpr float SPEAKER_GAIN_DEFAULT = 8.0f;
constexpr float SPEAKER_INPUT_HIGHPASS_HZ = 80.0f;
constexpr float SPEAKER_VOICE_HIGHPASS_HZ = 150.0f;
constexpr float SPEAKER_VOICE_LOWPASS_HZ = 2800.0f;
constexpr float SPEAKER_FFT_MU_DEFAULT = 0.60f;
constexpr float SPEAKER_FFT_REF_BIN_FLOOR = 8.0f;
constexpr float SPEAKER_FFT_EPS_SCALE = 20.0f;
constexpr float SPEAKER_FFT_PRIMARY_RATIO_GATE = 0.02f;
constexpr float SPEAKER_MASK_RATIO_START = 0.03f;
constexpr float SPEAKER_MASK_STRENGTH = 5.0f;
constexpr float SPEAKER_MASK_MIN = 0.06f;
constexpr float SPEAKER_MASK_RELEASE = 0.06f;
constexpr float SPEAKER_SOFT_LIMIT_KNEE = 22000.0f;
constexpr float SPEAKER_SOFT_LIMIT_MAX = 32000.0f;
constexpr uint SPEAKER_ENV_SHIFT = 7;
constexpr uint USB_SPK_DEFAULT_MS = 600000;
constexpr uint USB_SPK_MAX_MS = 600000;
constexpr uint USB_SPK_RETURN_TIMEOUT_FRAMES = DEFAULT_SAMPLE_RATE_HZ * 2;
constexpr uint USB_SPK_RETURN_BUFFER_LEN = 4096;
constexpr uint USB_SPK_PREBUFFER_FRAMES = DEFAULT_SAMPLE_RATE_HZ / 20;
constexpr uint DUAL_RING_FRAMES = 8192;
constexpr uint DUAL_RING_MASK = DUAL_RING_FRAMES - 1;
constexpr uint DUAL_USB_CHUNK_FRAMES = 128;
constexpr uint I2S_BITS_PER_SLOT = 32;
constexpr uint I2S_SLOTS_PER_FRAME = 2;
constexpr uint PIO_CYCLES_PER_FRAME =
    (I2S_BITS_PER_SLOT * I2S_SLOTS_PER_FRAME * 2) + I2S_SLOTS_PER_FRAME;
constexpr double SAMPLE_FULL_SCALE = 8388608.0;

constexpr size_t CMD_BUFFER_LEN = 96;

enum ProgramIns : uint {
    INS_LEFT_SET_X = 0,
    INS_LEFT_LOOP = 1,
    INS_LEFT_LOW = 2,
    INS_RIGHT_SET_X = 3,
    INS_RIGHT_LOOP = 4,
    INS_RIGHT_LOW = 5,
    INS_WRAP = 5,
    INS_COUNT = 6,
};

uint16_t s_i2s_program_instructions[INS_COUNT];

const pio_program_t s_i2s_program = {
    s_i2s_program_instructions,
    INS_COUNT,
    -1,
    0,
#if PICO_PIO_VERSION > 0
    1,
#endif
};

PIO s_pio = pio0;
uint s_sm = 0;
uint s_program_offset = 0;
uint s_sample_rate_hz = DEFAULT_SAMPLE_RATE_HZ;
uint s_frames_per_report = DEFAULT_FRAMES_PER_REPORT;
uint s_decode_shift = 7;
bool s_auto_report = false;
char s_cmd_buffer[CMD_BUFFER_LEN] = {};
size_t s_cmd_len = 0;

struct WaveStreamState {
    bool running;
    uint decimation;
    uint32_t seq;
};

WaveStreamState s_wave = { false, DEFAULT_WAVE_DECIMATION, 0 };

struct SlotStats {
    int32_t min_sample;
    int32_t max_sample;
    uint32_t peak_abs;
    int64_t sum;
    uint64_t sum_sq;
};

struct HighPassState {
    float alpha;
    float prev_x;
    float prev_y;
};

struct LowPassState {
    float alpha;
    float y;
};

enum SpeakerMode {
    SPEAKER_MODE_CANCEL = 0,
    SPEAKER_MODE_PRIMARY,
    SPEAKER_MODE_REFERENCE,
    SPEAKER_MODE_DIFF,
};

struct SpeakerOutputState {
    bool running;
    bool pwm_configured;
    SpeakerMode mode;
    float gain;
    float mu;
    float ref_bin_floor;
    float eps_scale;
    float primary_ratio_gate;
    float mask_ratio_start;
    float mask_strength;
    float mask_min;
    float mask_release;
    bool output_streaming;
    uint output_stream_decimation;
    uint output_stream_counter;
    uint32_t output_stream_seq;
    uint32_t seq;
    uint32_t update_count;
    uint32_t primary_peak;
    uint32_t reference_peak;
    uint32_t output_peak;
    uint32_t underflows;
    uint32_t freeze_count;
    uint32_t primary_env;
    uint32_t reference_env;
    uint32_t output_env;
    uint32_t quiet_count;
    uint32_t decay_count;
    uint32_t block_count;
    uint32_t bin_update_count;
    uint32_t output_fifo_overflows;
    HighPassState primary_hp;
    HighPassState reference_hp;
    HighPassState voice_hp;
    LowPassState voice_lp;
    LowPassState voice_lp2;
    float window[SPEAKER_FFT_N];
    float primary_ring[SPEAKER_FFT_N];
    float reference_ring[SPEAKER_FFT_N];
    float primary_re[SPEAKER_FFT_N];
    float primary_im[SPEAKER_FFT_N];
    float spectral_mask[SPEAKER_FFT_BINS];
    float output_overlap[SPEAKER_FFT_N];
    float output_fifo[SPEAKER_OUTPUT_FIFO_LEN];
    uint fft_pos;
    uint fft_fill;
    uint hop_count;
    uint output_read;
    uint output_write;
    uint output_count;
};

SpeakerOutputState s_speaker = {
    false,
    false,
    SPEAKER_MODE_CANCEL,
    SPEAKER_GAIN_DEFAULT,
    SPEAKER_FFT_MU_DEFAULT,
    SPEAKER_FFT_REF_BIN_FLOOR,
    SPEAKER_FFT_EPS_SCALE,
    SPEAKER_FFT_PRIMARY_RATIO_GATE,
    SPEAKER_MASK_RATIO_START,
    SPEAKER_MASK_STRENGTH,
    SPEAKER_MASK_MIN,
    SPEAKER_MASK_RELEASE,
    false,
    DEFAULT_OUTPUT_STREAM_DECIMATION,
    0,
    0,
};

struct UsbSpeakerBridgeState {
    bool running;
    uint32_t remaining_frames;
    bool continuous;
    bool playback_started;
    uint8_t rx_state;
    uint8_t sample_low;
    int16_t return_buffer[USB_SPK_RETURN_BUFFER_LEN];
    uint16_t return_read;
    uint16_t return_write;
    uint16_t return_count;
    uint32_t frames_without_return;
    uint32_t frames_sent;
    uint32_t samples_received;
    uint32_t samples_played;
    uint32_t underflows;
};

UsbSpeakerBridgeState s_usb_spk = {
    false,
    0,
    false,
    false,
    0,
    0,
    {},
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

uint32_t s_dual_ring[DUAL_RING_FRAMES] = {};
volatile bool s_dual_ring_active = false;
volatile uint32_t s_dual_ring_read = 0;
volatile uint32_t s_dual_ring_write = 0;
volatile uint32_t s_dual_ring_overflows = 0;

uint16_t s_speaker_playback_fifo[SPEAKER_PLAYBACK_FIFO_LEN] = {};
volatile uint32_t s_speaker_playback_read = 0;
volatile uint32_t s_speaker_playback_write = 0;
volatile uint32_t s_speaker_playback_overflows = 0;
volatile bool s_speaker_playback_started = false;
repeating_timer_t s_speaker_playback_timer = {};
bool s_speaker_playback_timer_active = false;

void serial_printf(const char *fmt, ...)
{
    char buffer[768];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    printf("%s", buffer);
    fflush(stdout);
}

void build_i2s_program(void)
{
    constexpr uint SIDE_BCLK_HIGH = 0x1;
    constexpr uint SIDE_WS_HIGH = 0x2;

    s_i2s_program_instructions[INS_LEFT_SET_X] =
        pio_encode_set(pio_x, I2S_BITS_PER_SLOT - 1) | pio_encode_sideset(2, 0);
    s_i2s_program_instructions[INS_LEFT_LOOP] =
        pio_encode_in(pio_pins, 1) | pio_encode_sideset(2, SIDE_BCLK_HIGH);
    s_i2s_program_instructions[INS_LEFT_LOW] =
        pio_encode_jmp_x_dec(INS_LEFT_LOOP) | pio_encode_sideset(2, 0);
    s_i2s_program_instructions[INS_RIGHT_SET_X] =
        pio_encode_set(pio_x, I2S_BITS_PER_SLOT - 1) | pio_encode_sideset(2, SIDE_WS_HIGH);
    s_i2s_program_instructions[INS_RIGHT_LOOP] =
        pio_encode_in(pio_pins, 1) | pio_encode_sideset(2, SIDE_WS_HIGH | SIDE_BCLK_HIGH);
    s_i2s_program_instructions[INS_RIGHT_LOW] =
        pio_encode_jmp_x_dec(INS_RIGHT_LOOP) | pio_encode_sideset(2, SIDE_WS_HIGH);
}

float pio_clkdiv_for_rate(uint sample_rate_hz)
{
    const uint32_t sys_hz = clock_get_hz(clk_sys);
    const float pio_hz = static_cast<float>(sample_rate_hz * PIO_CYCLES_PER_FRAME);
    return static_cast<float>(sys_hz) / pio_hz;
}

void apply_sample_rate(uint sample_rate_hz)
{
    s_sample_rate_hz = sample_rate_hz;
    pio_sm_set_clkdiv(s_pio, s_sm, pio_clkdiv_for_rate(sample_rate_hz));
}

void restart_capture_sm(void)
{
    pio_sm_set_enabled(s_pio, s_sm, false);
    pio_sm_clear_fifos(s_pio, s_sm);
    pio_sm_restart(s_pio, s_sm);
    pio_sm_exec(s_pio, s_sm, pio_encode_jmp(s_program_offset + INS_LEFT_SET_X));
    pio_sm_set_enabled(s_pio, s_sm, true);
}

void stop_capture_sm(void)
{
    pio_sm_set_enabled(s_pio, s_sm, false);
    pio_sm_clear_fifos(s_pio, s_sm);
    gpio_put(MIC_PIN_BCLK, 0);
    gpio_put(MIC_PIN_LRCLK, 0);
}

void set_wave_stream(bool running)
{
    if (running) {
        s_auto_report = false;
        s_wave.running = true;
        s_wave.seq = 0;
        restart_capture_sm();
        serial_printf("[WAVE] on decim=%lu stream_hz=%lu\r\n",
                      static_cast<unsigned long>(s_wave.decimation),
                      static_cast<unsigned long>(s_sample_rate_hz / s_wave.decimation));
    } else {
        s_wave.running = false;
        stop_capture_sm();
        serial_printf("[WAVE] off\r\n");
    }
}

void init_i2s_pio(void)
{
    build_i2s_program();

    s_program_offset = static_cast<uint>(pio_add_program(s_pio, &s_i2s_program));
    s_sm = static_cast<uint>(pio_claim_unused_sm(s_pio, true));

    pio_gpio_init(s_pio, MIC_PIN_BCLK);
    pio_gpio_init(s_pio, MIC_PIN_LRCLK);
    pio_gpio_init(s_pio, MIC_PIN_DOUT);

    gpio_pull_down(MIC_PIN_DOUT);
    pio_sm_set_consecutive_pindirs(s_pio, s_sm, MIC_PIN_BCLK, 2, true);
    pio_sm_set_consecutive_pindirs(s_pio, s_sm, MIC_PIN_DOUT, 1, false);
    pio_sm_set_pins_with_mask(s_pio, s_sm, 0, (1u << MIC_PIN_BCLK) | (1u << MIC_PIN_LRCLK));

    pio_sm_config config = pio_get_default_sm_config();
    sm_config_set_sideset(&config, 2, false, false);
    sm_config_set_sideset_pins(&config, MIC_PIN_BCLK);
    sm_config_set_in_pins(&config, MIC_PIN_DOUT);
#if PICO_PIO_VERSION > 0
    sm_config_set_in_pin_count(&config, 1);
#endif
    // ICS43434 sends MSB-first. Shift left so the first sampled bit remains at
    // the MSB side of the pushed FIFO word instead of bit-reversing the slot.
    sm_config_set_in_shift(&config, false, true, 32);
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_RX);
    sm_config_set_wrap(&config, s_program_offset + INS_LEFT_SET_X, s_program_offset + INS_WRAP);
    sm_config_set_clkdiv(&config, pio_clkdiv_for_rate(s_sample_rate_hz));

    pio_sm_init(s_pio, s_sm, s_program_offset + INS_LEFT_SET_X, &config);
    stop_capture_sm();
}

int32_t sign_extend_24(uint32_t sample)
{
    sample &= 0x00FFFFFFu;
    if (sample & 0x00800000u) {
        sample |= 0xFF000000u;
    }
    return static_cast<int32_t>(sample);
}

int32_t sample_from_i2s_word(uint32_t word)
{
    // ICS43434 outputs I2S, 24-bit two's-complement, MSB-first. The MSB is
    // delayed by one SCK from the start of each half-frame, so a 32-bit slot
    // captured MSB-first should contain: 1 dummy bit, 24 data bits, then 7
    // high-Z bits. The default shift is therefore 7; make it tunable so raw
    // captures can be checked against the datasheet without reflashing.
    return sign_extend_24((word >> s_decode_shift) & 0x00FFFFFFu);
}

int32_t sample_from_shifted_word(uint32_t word, uint shift)
{
    return sign_extend_24((word >> shift) & 0x00FFFFFFu);
}

uint32_t abs_sample(int32_t sample)
{
    if (sample == INT32_MIN) {
        return 0x80000000u;
    }
    return static_cast<uint32_t>(sample < 0 ? -sample : sample);
}

void stats_reset(SlotStats &stats)
{
    stats.min_sample = INT32_MAX;
    stats.max_sample = INT32_MIN;
    stats.peak_abs = 0;
    stats.sum = 0;
    stats.sum_sq = 0;
}

void stats_add(SlotStats &stats, int32_t sample)
{
    if (sample < stats.min_sample) {
        stats.min_sample = sample;
    }
    if (sample > stats.max_sample) {
        stats.max_sample = sample;
    }
    const uint32_t abs_value = abs_sample(sample);
    if (abs_value > stats.peak_abs) {
        stats.peak_abs = abs_value;
    }
    stats.sum += sample;
    stats.sum_sq += static_cast<uint64_t>(static_cast<int64_t>(sample) * sample);
}

void print_slot_stats(const char *name, const SlotStats &stats, uint frame_count)
{
    const double mean = static_cast<double>(stats.sum) / static_cast<double>(frame_count);
    const double rms = sqrt(static_cast<double>(stats.sum_sq) / static_cast<double>(frame_count));
    const double rms_pct = (100.0 * rms) / SAMPLE_FULL_SCALE;
    const double peak_pct = (100.0 * static_cast<double>(stats.peak_abs)) / SAMPLE_FULL_SCALE;

    serial_printf("%s min=%ld max=%ld dc=%.1f rms=%.1f (%5.2f%%FS) peak=%lu (%5.2f%%FS)",
                  name,
                  static_cast<long>(stats.min_sample),
                  static_cast<long>(stats.max_sample),
                  mean,
                  rms,
                  rms_pct,
                  static_cast<unsigned long>(stats.peak_abs),
                  peak_pct);
}

void capture_stats(uint frame_count, SlotStats &left, SlotStats &right)
{
    stats_reset(left);
    stats_reset(right);
    restart_capture_sm();

    for (uint i = 0; i < PCM_PROBE_FRAMES; i++) {
        (void)pio_sm_get_blocking(s_pio, s_sm);
        (void)pio_sm_get_blocking(s_pio, s_sm);
    }

    for (uint i = 0; i < frame_count; i++) {
        const uint32_t left_word = pio_sm_get_blocking(s_pio, s_sm);
        const uint32_t right_word = pio_sm_get_blocking(s_pio, s_sm);
        stats_add(left, sample_from_i2s_word(left_word));
        stats_add(right, sample_from_i2s_word(right_word));
    }

    stop_capture_sm();
}

void print_stats_report(uint frame_count)
{
    SlotStats left;
    SlotStats right;
    capture_stats(frame_count, left, right);

    serial_printf("[I2S] rate=%lu Hz frames=%lu  ",
                  static_cast<unsigned long>(s_sample_rate_hz),
                  static_cast<unsigned long>(frame_count));
    print_slot_stats("L", left, frame_count);
    serial_printf("  ");
    print_slot_stats("R", right, frame_count);
    serial_printf("\r\n");
}

void print_raw_frames(uint frame_count)
{
    if (frame_count == 0) {
        frame_count = 8;
    }
    if (frame_count > 32) {
        frame_count = 32;
    }

    uint32_t left_words[32] = {};
    uint32_t right_words[32] = {};

    restart_capture_sm();
    for (uint i = 0; i < PCM_PROBE_FRAMES; i++) {
        (void)pio_sm_get_blocking(s_pio, s_sm);
        (void)pio_sm_get_blocking(s_pio, s_sm);
    }
    for (uint i = 0; i < frame_count; i++) {
        left_words[i] = pio_sm_get_blocking(s_pio, s_sm);
        right_words[i] = pio_sm_get_blocking(s_pio, s_sm);
    }
    stop_capture_sm();

    serial_printf("[RAW] %lu frames\r\n", static_cast<unsigned long>(frame_count));
    for (uint i = 0; i < frame_count; i++) {
        const uint32_t left_word = left_words[i];
        const uint32_t right_word = right_words[i];
        serial_printf("%02lu L raw=0x%08lX sample=%ld  R raw=0x%08lX sample=%ld\r\n",
                      static_cast<unsigned long>(i),
                      static_cast<unsigned long>(left_word),
                      static_cast<long>(sample_from_i2s_word(left_word)),
                      static_cast<unsigned long>(right_word),
                      static_cast<long>(sample_from_i2s_word(right_word)));
    }
}

void scan_alignment(uint frame_count)
{
    if (frame_count < 64) {
        frame_count = 64;
    }
    if (frame_count > MAX_FRAMES_PER_REPORT) {
        frame_count = MAX_FRAMES_PER_REPORT;
    }

    SlotStats stats[9];
    for (uint shift = 0; shift <= 8; shift++) {
        stats_reset(stats[shift]);
    }

    restart_capture_sm();
    for (uint i = 0; i < PCM_PROBE_FRAMES; i++) {
        (void)pio_sm_get_blocking(s_pio, s_sm);
        (void)pio_sm_get_blocking(s_pio, s_sm);
    }
    for (uint i = 0; i < frame_count; i++) {
        const uint32_t left_word = pio_sm_get_blocking(s_pio, s_sm);
        (void)pio_sm_get_blocking(s_pio, s_sm);
        for (uint shift = 0; shift <= 8; shift++) {
            stats_add(stats[shift], sample_from_shifted_word(left_word, shift));
        }
    }
    stop_capture_sm();

    serial_printf("[SCAN] left-slot %lu frames\r\n", static_cast<unsigned long>(frame_count));
    for (uint shift = 0; shift <= 8; shift++) {
        serial_printf("shift=%lu ", static_cast<unsigned long>(shift));
        print_slot_stats("", stats[shift], frame_count);
        serial_printf("\r\n");
    }
}

const char *choose_active_slot(void)
{
    SlotStats left;
    SlotStats right;
    capture_stats(PCM_PROBE_FRAMES, left, right);
    return (left.peak_abs >= right.peak_abs) ? "left" : "right";
}

int16_t pcm16_from_sample(int32_t sample_24)
{
    int32_t sample_16 = sample_24 >> 8;
    if (sample_16 > 32767) {
        sample_16 = 32767;
    } else if (sample_16 < -32768) {
        sample_16 = -32768;
    }

    return static_cast<int16_t>(sample_16);
}

int16_t pcm16_from_diff(int32_t first_sample_24, int32_t second_sample_24)
{
    const int32_t diff_24 = first_sample_24 - second_sample_24;
    int32_t sample_16 = diff_24 / 512;
    if (sample_16 > 32767) {
        sample_16 = 32767;
    } else if (sample_16 < -32768) {
        sample_16 = -32768;
    }

    return static_cast<int16_t>(sample_16);
}

void write_pcm16_value(int16_t sample)
{
    const uint16_t packed = static_cast<uint16_t>(sample);
    putchar_raw(static_cast<int>(packed & 0xFFu));
    putchar_raw(static_cast<int>((packed >> 8) & 0xFFu));
}

void write_raw_bytes(const uint8_t *data, uint length)
{
    if (length == 0) {
        return;
    }
    stdio_put_string(reinterpret_cast<const char *>(data), static_cast<int>(length), false, false);
}

void append_pcm16(uint8_t *buffer, uint &offset, int16_t sample)
{
    const uint16_t packed = static_cast<uint16_t>(sample);
    buffer[offset++] = static_cast<uint8_t>(packed & 0xFFu);
    buffer[offset++] = static_cast<uint8_t>((packed >> 8) & 0xFFu);
}

uint32_t pack_dual_frame(int16_t primary, int16_t reference)
{
    return static_cast<uint16_t>(primary) | (static_cast<uint32_t>(static_cast<uint16_t>(reference)) << 16);
}

void unpack_dual_frame(uint32_t packed, int16_t &primary, int16_t &reference)
{
    primary = static_cast<int16_t>(packed & 0xFFFFu);
    reference = static_cast<int16_t>((packed >> 16) & 0xFFFFu);
}

void capture_core1_main(void)
{
    while (true) {
        if (!s_dual_ring_active) {
            tight_loop_contents();
            continue;
        }

        if (pio_sm_get_rx_fifo_level(s_pio, s_sm) < 2) {
            tight_loop_contents();
            continue;
        }

        const uint32_t first_word = pio_sm_get(s_pio, s_sm);
        const uint32_t second_word = pio_sm_get(s_pio, s_sm);
        const uint32_t packed = pack_dual_frame(
            pcm16_from_sample(sample_from_i2s_word(first_word)),
            pcm16_from_sample(sample_from_i2s_word(second_word)));

        const uint32_t write = s_dual_ring_write;
        const uint32_t next = (write + 1u) & DUAL_RING_MASK;
        if (next == s_dual_ring_read) {
            s_dual_ring_read = (s_dual_ring_read + 1u) & DUAL_RING_MASK;
            s_dual_ring_overflows++;
        }

        s_dual_ring[write] = packed;
        __dmb();
        s_dual_ring_write = next;
    }
}

void start_dual_ring_capture(void)
{
    s_dual_ring_active = false;
    s_dual_ring_read = 0;
    s_dual_ring_write = 0;
    s_dual_ring_overflows = 0;

    restart_capture_sm();
    for (uint i = 0; i < PCM_PROBE_FRAMES; i++) {
        (void)pio_sm_get_blocking(s_pio, s_sm);
        (void)pio_sm_get_blocking(s_pio, s_sm);
    }

    s_dual_ring_active = true;
}

void stop_dual_ring_capture(void)
{
    s_dual_ring_active = false;
    sleep_us(100);
    stop_capture_sm();
}

bool dual_ring_pop(uint32_t &packed)
{
    const uint32_t read = s_dual_ring_read;
    if (read == s_dual_ring_write) {
        return false;
    }

    packed = s_dual_ring[read];
    __dmb();
    s_dual_ring_read = (read + 1u) & DUAL_RING_MASK;
    return true;
}

void stop_speaker_output(bool announce);

const char *speaker_mode_name(SpeakerMode mode)
{
    switch (mode) {
    case SPEAKER_MODE_CANCEL:
        return "cancel";
    case SPEAKER_MODE_PRIMARY:
        return "primary";
    case SPEAKER_MODE_REFERENCE:
        return "reference";
    case SPEAKER_MODE_DIFF:
        return "diff";
    default:
        return "unknown";
    }
}

void stream_pcm_direct(uint32_t sample_count, bool use_left, const char *slot)
{
    serial_printf("$PCM,%lu,%lu,%s,16le\r\n",
                  static_cast<unsigned long>(s_sample_rate_hz),
                  static_cast<unsigned long>(sample_count),
                  slot);
    stdio_flush();

    stdio_set_translate_crlf(&stdio_usb, false);
    restart_capture_sm();
    for (uint i = 0; i < PCM_PROBE_FRAMES; i++) {
        (void)pio_sm_get_blocking(s_pio, s_sm);
        (void)pio_sm_get_blocking(s_pio, s_sm);
    }
    for (uint32_t i = 0; i < sample_count; i++) {
        const uint32_t left_word = pio_sm_get_blocking(s_pio, s_sm);
        const uint32_t right_word = pio_sm_get_blocking(s_pio, s_sm);
        const int32_t sample = use_left ? sample_from_i2s_word(left_word) : sample_from_i2s_word(right_word);
        write_pcm16_value(pcm16_from_sample(sample));
    }
    stop_capture_sm();
    stdio_flush();
    stdio_set_translate_crlf(&stdio_usb, true);
    serial_printf("\r\n$PCM_DONE\r\n");
}

bool diff_stream_stop_requested(void)
{
    const int ch = getchar_timeout_us(0);
    return ch == 'q' || ch == 'Q' || ch == 3;
}

void stream_diff_pcm(uint duration_ms, bool continuous)
{
    if (!continuous) {
        if (duration_ms == 0) {
            duration_ms = DEFAULT_PCM_MS;
        }
        if (duration_ms > MAX_DIFF_PCM_MS) {
            duration_ms = MAX_DIFF_PCM_MS;
        }
    }

    if (s_speaker.running) {
        stop_speaker_output(false);
    }
    if (s_wave.running) {
        s_wave.running = false;
        stop_capture_sm();
    }
    s_auto_report = false;

    const uint32_t sample_count = continuous
        ? 0u
        : (static_cast<uint32_t>(s_sample_rate_hz) * static_cast<uint32_t>(duration_ms)) / 1000u;

    serial_printf("$DIFFPCM,%lu,%lu,mic1-minus-mic2,16le\r\n",
                  static_cast<unsigned long>(s_sample_rate_hz),
                  static_cast<unsigned long>(sample_count));
    stdio_flush();

    stdio_set_translate_crlf(&stdio_usb, false);
    restart_capture_sm();
    for (uint i = 0; i < PCM_PROBE_FRAMES; i++) {
        (void)pio_sm_get_blocking(s_pio, s_sm);
        (void)pio_sm_get_blocking(s_pio, s_sm);
    }

    uint32_t sent = 0;
    while (continuous || sent < sample_count) {
        if (diff_stream_stop_requested()) {
            break;
        }

        const uint32_t first_word = pio_sm_get_blocking(s_pio, s_sm);
        const uint32_t second_word = pio_sm_get_blocking(s_pio, s_sm);
        const int32_t first_sample = sample_from_i2s_word(first_word);
        const int32_t second_sample = sample_from_i2s_word(second_word);
        write_pcm16_value(pcm16_from_diff(first_sample, second_sample));
        sent++;
    }

    stop_capture_sm();
    stdio_flush();
    stdio_set_translate_crlf(&stdio_usb, true);
    serial_printf("\r\n$DIFF_DONE,%lu\r\n", static_cast<unsigned long>(sent));
}

void stream_dual_pcm(uint duration_ms, bool continuous)
{
    if (!continuous) {
        if (duration_ms == 0) {
            duration_ms = DEFAULT_PCM_MS;
        }
        if (duration_ms > MAX_DIFF_PCM_MS) {
            duration_ms = MAX_DIFF_PCM_MS;
        }
    }

    if (s_speaker.running) {
        stop_speaker_output(false);
    }
    if (s_wave.running) {
        s_wave.running = false;
        stop_capture_sm();
    }
    s_auto_report = false;

    const uint32_t sample_count = continuous
        ? 0u
        : (static_cast<uint32_t>(s_sample_rate_hz) * static_cast<uint32_t>(duration_ms)) / 1000u;

    serial_printf("$DUALPCM,%lu,%lu,mic1,mic2,16le\r\n",
                  static_cast<unsigned long>(s_sample_rate_hz),
                  static_cast<unsigned long>(sample_count));
    stdio_flush();

    stdio_set_translate_crlf(&stdio_usb, false);
    if (continuous) {
        uint8_t out[DUAL_USB_CHUNK_FRAMES * 4] = {};
        uint out_len = 0;
        uint32_t sent = 0;

        start_dual_ring_capture();
        while (true) {
            if (diff_stream_stop_requested()) {
                break;
            }

            uint32_t packed_frame = 0;
            if (!dual_ring_pop(packed_frame)) {
                tight_loop_contents();
                continue;
            }

            int16_t primary = 0;
            int16_t reference = 0;
            unpack_dual_frame(packed_frame, primary, reference);
            append_pcm16(out, out_len, primary);
            append_pcm16(out, out_len, reference);
            sent++;

            if (out_len >= sizeof(out)) {
                write_raw_bytes(out, out_len);
                out_len = 0;
            }
        }

        if (out_len > 0) {
            write_raw_bytes(out, out_len);
        }

        stop_dual_ring_capture();
        stdio_flush();
        stdio_set_translate_crlf(&stdio_usb, true);
        serial_printf("\r\n$DUAL_DONE,%lu,%lu\r\n",
                      static_cast<unsigned long>(sent),
                      static_cast<unsigned long>(s_dual_ring_overflows));
        return;
    }

    restart_capture_sm();
    for (uint i = 0; i < PCM_PROBE_FRAMES; i++) {
        (void)pio_sm_get_blocking(s_pio, s_sm);
        (void)pio_sm_get_blocking(s_pio, s_sm);
    }

    uint32_t sent = 0;
    while (continuous || sent < sample_count) {
        if (diff_stream_stop_requested()) {
            break;
        }

        const uint32_t first_word = pio_sm_get_blocking(s_pio, s_sm);
        const uint32_t second_word = pio_sm_get_blocking(s_pio, s_sm);
        write_pcm16_value(pcm16_from_sample(sample_from_i2s_word(first_word)));
        write_pcm16_value(pcm16_from_sample(sample_from_i2s_word(second_word)));
        sent++;
    }

    stop_capture_sm();
    stdio_flush();
    stdio_set_translate_crlf(&stdio_usb, true);
    serial_printf("\r\n$DUAL_DONE,%lu\r\n", static_cast<unsigned long>(sent));
}

float highpass_alpha_for_rate(uint sample_rate_hz, float cutoff_hz)
{
    const float rc = 1.0f / (2.0f * PI_F * cutoff_hz);
    const float dt = 1.0f / static_cast<float>(sample_rate_hz);
    return rc / (rc + dt);
}

void reset_highpass(HighPassState &state, float cutoff_hz)
{
    state.alpha = highpass_alpha_for_rate(s_sample_rate_hz, cutoff_hz);
    state.prev_x = 0.0f;
    state.prev_y = 0.0f;
}

float highpass_step(HighPassState &state, float sample)
{
    const float y = state.alpha * (state.prev_y + sample - state.prev_x);
    state.prev_x = sample;
    state.prev_y = y;
    return y;
}

float lowpass_alpha_for_rate(uint sample_rate_hz, float cutoff_hz)
{
    const float rc = 1.0f / (2.0f * PI_F * cutoff_hz);
    const float dt = 1.0f / static_cast<float>(sample_rate_hz);
    return dt / (rc + dt);
}

void reset_lowpass(LowPassState &state, float cutoff_hz)
{
    state.alpha = lowpass_alpha_for_rate(s_sample_rate_hz, cutoff_hz);
    state.y = 0.0f;
}

float lowpass_step(LowPassState &state, float sample)
{
    state.y += state.alpha * (sample - state.y);
    return state.y;
}

uint32_t abs_i32(int32_t value)
{
    return static_cast<uint32_t>(value < 0 ? -value : value);
}

uint32_t smooth_envelope_shift(uint32_t current, uint32_t sample_abs, uint shift)
{
    if (sample_abs > current) {
        const uint32_t delta = sample_abs - current;
        const uint32_t step = delta >> shift;
        current += step == 0 ? 1 : step;
    } else {
        const uint32_t delta = current - sample_abs;
        if (delta > 0) {
            const uint32_t step = delta >> shift;
            current -= step == 0 ? 1 : step;
        }
    }
    return current;
}

uint32_t smooth_envelope(uint32_t current, uint32_t sample_abs)
{
    return smooth_envelope_shift(current, sample_abs, SPEAKER_ENV_SHIFT);
}

int32_t clamp_i32(int32_t value, int32_t low, int32_t high)
{
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

uint16_t clamp_pwm_level(int32_t value)
{
    if (value < 0) {
        return 0;
    }
    if (value > static_cast<int32_t>(SPEAKER_PWM_WRAP)) {
        return static_cast<uint16_t>(SPEAKER_PWM_WRAP);
    }
    return static_cast<uint16_t>(value);
}

uint16_t pwm_level_from_audio(float sample_16)
{
    constexpr int32_t kPwmMid = (SPEAKER_PWM_WRAP + 1U) / 2U;
    const int32_t scaled = static_cast<int32_t>((sample_16 * static_cast<float>(SPEAKER_PWM_WRAP + 1U)) / 65536.0f);
    return clamp_pwm_level(kPwmMid + scaled);
}

void configure_speaker_pwm(void)
{
    if (s_speaker.pwm_configured) {
        return;
    }

    gpio_set_function(SPEAKER_PWM_PIN, GPIO_FUNC_PWM);
    const uint slice = pwm_gpio_to_slice_num(SPEAKER_PWM_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, SPEAKER_PWM_WRAP);
    pwm_config_set_clkdiv(&config, 1.0f);
    pwm_init(slice, &config, true);
    pwm_set_gpio_level(SPEAKER_PWM_PIN, (SPEAKER_PWM_WRAP + 1U) / 2U);
    s_speaker.pwm_configured = true;
}

uint32_t speaker_playback_fifo_count(void)
{
    const uint32_t write = s_speaker_playback_write;
    const uint32_t read = s_speaker_playback_read;
    return (write - read) & SPEAKER_PLAYBACK_FIFO_MASK;
}

void clear_speaker_playback_fifo(void)
{
    const uint32_t irq_state = save_and_disable_interrupts();
    s_speaker_playback_read = 0;
    s_speaker_playback_write = 0;
    s_speaker_playback_overflows = 0;
    s_speaker_playback_started = false;
    memset(s_speaker_playback_fifo, 0, sizeof(s_speaker_playback_fifo));
    restore_interrupts(irq_state);
}

void speaker_playback_enqueue(uint16_t pwm_level)
{
    uint32_t write = s_speaker_playback_write;
    const uint32_t next = (write + 1u) & SPEAKER_PLAYBACK_FIFO_MASK;
    if (next == s_speaker_playback_read) {
        s_speaker_playback_read = (s_speaker_playback_read + 1u) & SPEAKER_PLAYBACK_FIFO_MASK;
        s_speaker_playback_overflows++;
    }
    s_speaker_playback_fifo[write] = pwm_level;
    __dmb();
    s_speaker_playback_write = next;
}

bool speaker_playback_timer_callback(repeating_timer_t *)
{
    if (!s_speaker.running) {
        pwm_set_gpio_level(SPEAKER_PWM_PIN, (SPEAKER_PWM_WRAP + 1U) / 2U);
        return true;
    }

    const uint32_t read = s_speaker_playback_read;
    const uint32_t write = s_speaker_playback_write;
    const uint32_t available = (write - read) & SPEAKER_PLAYBACK_FIFO_MASK;
    if (!s_speaker_playback_started) {
        if (available < SPEAKER_PLAYBACK_PREBUFFER) {
            pwm_set_gpio_level(SPEAKER_PWM_PIN, (SPEAKER_PWM_WRAP + 1U) / 2U);
            return true;
        }
        s_speaker_playback_started = true;
    }

    if (read == write) {
        s_speaker.underflows++;
        s_speaker_playback_started = false;
        pwm_set_gpio_level(SPEAKER_PWM_PIN, (SPEAKER_PWM_WRAP + 1U) / 2U);
        return true;
    }

    const uint16_t level = s_speaker_playback_fifo[read];
    s_speaker_playback_read = (read + 1u) & SPEAKER_PLAYBACK_FIFO_MASK;
    pwm_set_gpio_level(SPEAKER_PWM_PIN, level);
    return true;
}

void stop_speaker_playback_timer(void)
{
    if (s_speaker_playback_timer_active) {
        cancel_repeating_timer(&s_speaker_playback_timer);
        s_speaker_playback_timer_active = false;
    }
    pwm_set_gpio_level(SPEAKER_PWM_PIN, (SPEAKER_PWM_WRAP + 1U) / 2U);
}

void start_speaker_playback_timer(void)
{
    stop_speaker_playback_timer();
    clear_speaker_playback_fifo();
    const int64_t period_us = -static_cast<int64_t>(1000000u / s_sample_rate_hz);
    s_speaker_playback_timer_active =
        add_repeating_timer_us(period_us, speaker_playback_timer_callback, nullptr, &s_speaker_playback_timer);
    if (!s_speaker_playback_timer_active) {
        serial_printf("[ERR] speaker playback timer failed\r\n");
    }
}

void init_speaker_fft_window(void)
{
    for (uint i = 0; i < SPEAKER_FFT_N; i++) {
        s_speaker.window[i] = sinf((PI_F * (static_cast<float>(i) + 0.5f)) /
                                   static_cast<float>(SPEAKER_FFT_N));
    }
}

void reset_speaker_canceller(void)
{
    reset_highpass(s_speaker.primary_hp, SPEAKER_INPUT_HIGHPASS_HZ);
    reset_highpass(s_speaker.reference_hp, SPEAKER_INPUT_HIGHPASS_HZ);
    reset_highpass(s_speaker.voice_hp, SPEAKER_VOICE_HIGHPASS_HZ);
    reset_lowpass(s_speaker.voice_lp, SPEAKER_VOICE_LOWPASS_HZ);
    reset_lowpass(s_speaker.voice_lp2, SPEAKER_VOICE_LOWPASS_HZ);
    init_speaker_fft_window();
    memset(s_speaker.primary_ring, 0, sizeof(s_speaker.primary_ring));
    memset(s_speaker.reference_ring, 0, sizeof(s_speaker.reference_ring));
    memset(s_speaker.primary_re, 0, sizeof(s_speaker.primary_re));
    memset(s_speaker.primary_im, 0, sizeof(s_speaker.primary_im));
    for (uint bin = 0; bin < SPEAKER_FFT_BINS; bin++) {
        s_speaker.spectral_mask[bin] = 1.0f;
    }
    memset(s_speaker.output_overlap, 0, sizeof(s_speaker.output_overlap));
    memset(s_speaker.output_fifo, 0, sizeof(s_speaker.output_fifo));
    s_speaker.seq = 0;
    s_speaker.update_count = 0;
    s_speaker.primary_peak = 0;
    s_speaker.reference_peak = 0;
    s_speaker.output_peak = 0;
    s_speaker.underflows = 0;
    s_speaker.freeze_count = 0;
    s_speaker.primary_env = 0;
    s_speaker.reference_env = 0;
    s_speaker.output_env = 0;
    s_speaker.quiet_count = 0;
    s_speaker.decay_count = 0;
    s_speaker.block_count = 0;
    s_speaker.bin_update_count = 0;
    s_speaker.output_fifo_overflows = 0;
    s_speaker.fft_pos = 0;
    s_speaker.fft_fill = 0;
    s_speaker.hop_count = 0;
    s_speaker.output_read = 0;
    s_speaker.output_write = 0;
    s_speaker.output_count = 0;
    s_speaker.output_stream_counter = 0;
    s_speaker.output_stream_seq = 0;
    clear_speaker_playback_fifo();
}

void fft_complex(float *re, float *im, uint n, bool inverse)
{
    uint j = 0;
    for (uint i = 1; i < n; i++) {
        uint bit = n >> 1;
        while ((j & bit) != 0) {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;
        if (i < j) {
            const float tr = re[i];
            const float ti = im[i];
            re[i] = re[j];
            im[i] = im[j];
            re[j] = tr;
            im[j] = ti;
        }
    }

    for (uint len = 2; len <= n; len <<= 1) {
        const float angle = (inverse ? 2.0f : -2.0f) * PI_F / static_cast<float>(len);
        const float wlen_re = cosf(angle);
        const float wlen_im = sinf(angle);
        const uint half = len >> 1;
        for (uint i = 0; i < n; i += len) {
            float wr = 1.0f;
            float wi = 0.0f;
            for (uint k = 0; k < half; k++) {
                const uint even = i + k;
                const uint odd = even + half;
                const float ur = re[even];
                const float ui = im[even];
                const float vr = (re[odd] * wr) - (im[odd] * wi);
                const float vi = (re[odd] * wi) + (im[odd] * wr);
                re[even] = ur + vr;
                im[even] = ui + vi;
                re[odd] = ur - vr;
                im[odd] = ui - vi;

                const float next_wr = (wr * wlen_re) - (wi * wlen_im);
                wi = (wr * wlen_im) + (wi * wlen_re);
                wr = next_wr;
            }
        }
    }

    if (inverse) {
        const float inv_n = 1.0f / static_cast<float>(n);
        for (uint i = 0; i < n; i++) {
            re[i] *= inv_n;
            im[i] *= inv_n;
        }
    }
}

bool fft_bin_in_voice_band(uint bin)
{
    const float freq =
        (static_cast<float>(bin) * static_cast<float>(s_sample_rate_hz)) /
        static_cast<float>(SPEAKER_FFT_N);
    return freq >= SPEAKER_VOICE_HIGHPASS_HZ && freq <= SPEAKER_VOICE_LOWPASS_HZ;
}

float target_mask_from_ratio(float ratio)
{
    if (ratio <= s_speaker.mask_ratio_start) {
        return 1.0f;
    }
    float mask = 1.0f / (1.0f + (s_speaker.mask_strength * (ratio - s_speaker.mask_ratio_start)));
    if (mask < s_speaker.mask_min) {
        mask = s_speaker.mask_min;
    } else if (mask > 1.0f) {
        mask = 1.0f;
    }
    return mask;
}

float soft_limit_audio(float sample)
{
    const float sign = sample < 0.0f ? -1.0f : 1.0f;
    float mag = sample < 0.0f ? -sample : sample;
    if (mag <= SPEAKER_SOFT_LIMIT_KNEE) {
        return sample;
    }
    const float range = SPEAKER_SOFT_LIMIT_MAX - SPEAKER_SOFT_LIMIT_KNEE;
    const float over = mag - SPEAKER_SOFT_LIMIT_KNEE;
    mag = SPEAKER_SOFT_LIMIT_KNEE + ((range * over) / (over + range));
    if (mag > 32767.0f) {
        mag = 32767.0f;
    }
    return sign * mag;
}

void speaker_output_enqueue(float sample)
{
    if (s_speaker.output_count >= SPEAKER_OUTPUT_FIFO_LEN) {
        s_speaker.output_read = (s_speaker.output_read + 1u) & SPEAKER_OUTPUT_FIFO_MASK;
        s_speaker.output_count--;
        s_speaker.output_fifo_overflows++;
    }
    s_speaker.output_fifo[s_speaker.output_write] = sample;
    s_speaker.output_write = (s_speaker.output_write + 1u) & SPEAKER_OUTPUT_FIFO_MASK;
    s_speaker.output_count++;
}

float speaker_output_pop(void)
{
    if (s_speaker.output_count == 0) {
        if (s_speaker.fft_fill >= SPEAKER_FFT_N) {
            s_speaker.underflows++;
        }
        return 0.0f;
    }

    const float sample = s_speaker.output_fifo[s_speaker.output_read];
    s_speaker.output_read = (s_speaker.output_read + 1u) & SPEAKER_OUTPUT_FIFO_MASK;
    s_speaker.output_count--;
    return sample;
}

void process_fft_canceller_block(void)
{
    const uint start = s_speaker.fft_pos;
    for (uint i = 0; i < SPEAKER_FFT_N; i++) {
        const uint src = (start + i) & SPEAKER_FFT_MASK;
        const float w = s_speaker.window[i];
        s_speaker.primary_re[i] = s_speaker.primary_ring[src] * w;
        s_speaker.primary_im[i] = s_speaker.reference_ring[src] * w;
    }

    fft_complex(s_speaker.primary_re, s_speaker.primary_im, SPEAKER_FFT_N, false);

    const float ref_floor_mag = s_speaker.ref_bin_floor * static_cast<float>(SPEAKER_FFT_N);
    const float ref_floor_power = ref_floor_mag * ref_floor_mag;
    const float eps_mag = s_speaker.eps_scale * static_cast<float>(SPEAKER_FFT_N);
    const float eps_power = eps_mag * eps_mag;
    uint updated_bins = 0;
    bool quiet_decay = false;

    for (uint bin = 0; bin < SPEAKER_FFT_BINS; bin++) {
        float dr = 0.0f;
        float di = 0.0f;
        float xr = 0.0f;
        float xi = 0.0f;
        if (bin == 0 || bin == (SPEAKER_FFT_N / 2)) {
            dr = s_speaker.primary_re[bin];
            xr = s_speaker.primary_im[bin];
        } else {
            const float cr = s_speaker.primary_re[bin];
            const float ci = s_speaker.primary_im[bin];
            const float mr = s_speaker.primary_re[SPEAKER_FFT_N - bin];
            const float mi = s_speaker.primary_im[SPEAKER_FFT_N - bin];
            dr = 0.5f * (cr + mr);
            di = 0.5f * (ci - mi);
            xr = 0.5f * (ci + mi);
            xi = 0.5f * (mr - cr);
        }
        const float x_power = (xr * xr) + (xi * xi);
        const float d_power = (dr * dr) + (di * di);
        float er = dr;
        float ei = di;
        const bool in_voice_band = fft_bin_in_voice_band(bin);

        if (in_voice_band) {
            const bool has_reference = x_power > ref_floor_power;
            const bool reference_is_relevant =
                x_power > (d_power * s_speaker.primary_ratio_gate);
            if (has_reference && reference_is_relevant) {
                const float ratio = x_power / (d_power + eps_power);
                const float target_mask = target_mask_from_ratio(ratio);
                s_speaker.spectral_mask[bin] +=
                    (target_mask - s_speaker.spectral_mask[bin]) * s_speaker.mu;
                updated_bins++;
            } else {
                s_speaker.spectral_mask[bin] +=
                    (1.0f - s_speaker.spectral_mask[bin]) * s_speaker.mask_release;
                if (!has_reference) {
                    quiet_decay = true;
                }
            }
        } else {
            s_speaker.spectral_mask[bin] +=
                (1.0f - s_speaker.spectral_mask[bin]) * s_speaker.mask_release;
        }

        if (in_voice_band) {
            er = dr * s_speaker.spectral_mask[bin];
            ei = di * s_speaker.spectral_mask[bin];
        }

        s_speaker.primary_re[bin] = er;
        s_speaker.primary_im[bin] = ei;
        if (bin > 0 && bin < (SPEAKER_FFT_N / 2)) {
            s_speaker.primary_re[SPEAKER_FFT_N - bin] = er;
            s_speaker.primary_im[SPEAKER_FFT_N - bin] = -ei;
        }
    }
    s_speaker.primary_im[0] = 0.0f;
    s_speaker.primary_im[SPEAKER_FFT_N / 2] = 0.0f;

    fft_complex(s_speaker.primary_re, s_speaker.primary_im, SPEAKER_FFT_N, true);

    for (uint i = 0; i < SPEAKER_FFT_N; i++) {
        s_speaker.output_overlap[i] += s_speaker.primary_re[i] * s_speaker.window[i];
    }
    for (uint i = 0; i < SPEAKER_FFT_HOP; i++) {
        speaker_output_enqueue(s_speaker.output_overlap[i]);
    }
    for (uint i = 0; i < SPEAKER_FFT_N - SPEAKER_FFT_HOP; i++) {
        s_speaker.output_overlap[i] = s_speaker.output_overlap[i + SPEAKER_FFT_HOP];
    }
    for (uint i = SPEAKER_FFT_N - SPEAKER_FFT_HOP; i < SPEAKER_FFT_N; i++) {
        s_speaker.output_overlap[i] = 0.0f;
    }

    s_speaker.block_count++;
    s_speaker.bin_update_count += updated_bins;
    if (updated_bins > 0) {
        s_speaker.update_count++;
        s_speaker.quiet_count = 0;
    } else {
        s_speaker.freeze_count++;
        s_speaker.quiet_count++;
    }
    if (quiet_decay) {
        s_speaker.decay_count++;
    }
}

float adaptive_cancel_step(float primary_16, float reference_16)
{
    const int32_t d_q15 = clamp_i32(static_cast<int32_t>(primary_16), -32768, 32767);
    const int32_t x_q15 = clamp_i32(static_cast<int32_t>(reference_16), -32768, 32767);
    const uint32_t d_abs = abs_i32(d_q15);
    const uint32_t x_abs = abs_i32(x_q15);
    s_speaker.primary_env = smooth_envelope(s_speaker.primary_env, d_abs);
    s_speaker.reference_env = smooth_envelope(s_speaker.reference_env, x_abs);

    s_speaker.primary_ring[s_speaker.fft_pos] = static_cast<float>(d_q15);
    s_speaker.reference_ring[s_speaker.fft_pos] = static_cast<float>(x_q15);
    s_speaker.fft_pos = (s_speaker.fft_pos + 1u) & SPEAKER_FFT_MASK;
    if (s_speaker.fft_fill < SPEAKER_FFT_N) {
        s_speaker.fft_fill++;
    }
    s_speaker.hop_count++;
    if (s_speaker.fft_fill >= SPEAKER_FFT_N && s_speaker.hop_count >= SPEAKER_FFT_HOP) {
        process_fft_canceller_block();
        s_speaker.hop_count = 0;
    }

    return speaker_output_pop();
}

void stop_speaker_output(bool announce)
{
    if (!s_speaker.running) {
        return;
    }

    s_speaker.running = false;
    stop_speaker_playback_timer();
    stop_dual_ring_capture();
    configure_speaker_pwm();
    pwm_set_gpio_level(SPEAKER_PWM_PIN, (SPEAKER_PWM_WRAP + 1U) / 2U);
    if (announce) {
        serial_printf("[SPK] off, GP%u centered\r\n", SPEAKER_PWM_PIN);
    }
}

void start_speaker_output(void)
{
    if (s_wave.running) {
        s_wave.running = false;
        stop_capture_sm();
    }

    s_auto_report = false;
    configure_speaker_pwm();
    reset_speaker_canceller();
    start_dual_ring_capture();
    s_speaker.running = true;
    start_speaker_playback_timer();
    serial_printf("[SPK] on GP%u PWM timer, gain=%.2fx, mode=%s\r\n",
                  SPEAKER_PWM_PIN,
                  static_cast<double>(s_speaker.gain),
                  speaker_mode_name(s_speaker.mode));
}

void print_speaker_status(void)
{
    serial_printf("[SPK] %s GP%u rate=%luHz mode=%s timer=%s play=%lu/%u pre=%u gain=%.2fx mu=%.3f fft=%u hop=%u block=%.1fms prebuf=%.1fms mask=%.2f..1 voice=%u-%uHz viz=%s/%u frames=%lu blocks=%lu suppress_blocks=%lu suppress_bins=%lu blocked=%lu decays=%lu fft_fifo=%u/%u fft_ovf=%lu play_ovf=%lu p_peak=%lu r_peak=%lu out_peak=%lu p_env=%lu r_env=%lu out_env=%lu cap_overflows=%lu underflows=%lu\r\n",
                  s_speaker.running ? "on" : "off",
                  SPEAKER_PWM_PIN,
                  static_cast<unsigned long>(s_sample_rate_hz),
                  speaker_mode_name(s_speaker.mode),
                  s_speaker_playback_timer_active ? "on" : "off",
                  static_cast<unsigned long>(speaker_playback_fifo_count()),
                  SPEAKER_PLAYBACK_FIFO_LEN,
                  SPEAKER_PLAYBACK_PREBUFFER,
                  static_cast<double>(s_speaker.gain),
                  static_cast<double>(s_speaker.mu),
                  SPEAKER_FFT_N,
                  SPEAKER_FFT_HOP,
                  static_cast<double>((1000.0f * static_cast<float>(SPEAKER_FFT_N)) / static_cast<float>(s_sample_rate_hz)),
                  static_cast<double>((1000.0f * static_cast<float>(SPEAKER_PLAYBACK_PREBUFFER)) / static_cast<float>(s_sample_rate_hz)),
                  static_cast<double>(s_speaker.mask_min),
                  static_cast<unsigned>(SPEAKER_VOICE_HIGHPASS_HZ),
                  static_cast<unsigned>(SPEAKER_VOICE_LOWPASS_HZ),
                  s_speaker.output_streaming ? "on" : "off",
                  s_speaker.output_stream_decimation,
                  static_cast<unsigned long>(s_speaker.seq),
                  static_cast<unsigned long>(s_speaker.block_count),
                  static_cast<unsigned long>(s_speaker.update_count),
                  static_cast<unsigned long>(s_speaker.bin_update_count),
                  static_cast<unsigned long>(s_speaker.freeze_count),
                  static_cast<unsigned long>(s_speaker.decay_count),
                  s_speaker.output_count,
                  SPEAKER_OUTPUT_FIFO_LEN,
                  static_cast<unsigned long>(s_speaker.output_fifo_overflows),
                  static_cast<unsigned long>(s_speaker_playback_overflows),
                  static_cast<unsigned long>(s_speaker.primary_peak),
                  static_cast<unsigned long>(s_speaker.reference_peak),
                  static_cast<unsigned long>(s_speaker.output_peak),
                  static_cast<unsigned long>(s_speaker.primary_env),
                  static_cast<unsigned long>(s_speaker.reference_env),
                  static_cast<unsigned long>(s_speaker.output_env),
                  static_cast<unsigned long>(s_dual_ring_overflows),
                  static_cast<unsigned long>(s_speaker.underflows));
}

void print_speaker_params(void)
{
    serial_printf("[PARAM] gain %.4f\r\n", static_cast<double>(s_speaker.gain));
    serial_printf("[PARAM] mu %.4f\r\n", static_cast<double>(s_speaker.mu));
    serial_printf("[PARAM] refloor %.4f\r\n", static_cast<double>(s_speaker.ref_bin_floor));
    serial_printf("[PARAM] eps %.4f\r\n", static_cast<double>(s_speaker.eps_scale));
    serial_printf("[PARAM] gate %.4f\r\n", static_cast<double>(s_speaker.primary_ratio_gate));
    serial_printf("[PARAM] start %.4f\r\n", static_cast<double>(s_speaker.mask_ratio_start));
    serial_printf("[PARAM] strength %.4f\r\n", static_cast<double>(s_speaker.mask_strength));
    serial_printf("[PARAM] min %.4f\r\n", static_cast<double>(s_speaker.mask_min));
    serial_printf("[PARAM] release %.4f\r\n", static_cast<double>(s_speaker.mask_release));
}

void set_speaker_output_stream(bool running, uint decimation)
{
    if (decimation < MIN_OUTPUT_STREAM_DECIMATION) {
        decimation = MIN_OUTPUT_STREAM_DECIMATION;
    } else if (decimation > MAX_OUTPUT_STREAM_DECIMATION) {
        decimation = MAX_OUTPUT_STREAM_DECIMATION;
    }
    s_speaker.output_stream_decimation = decimation;
    s_speaker.output_stream_counter = 0;
    s_speaker.output_stream_seq = 0;
    s_speaker.output_streaming = running;
    serial_printf("[OUT] %s decim=%u stream_hz=%lu\r\n",
                  running ? "on" : "off",
                  decimation,
                  static_cast<unsigned long>(s_sample_rate_hz / decimation));
}

void stream_speaker_output_sample(int16_t output,
                                  int16_t primary,
                                  int16_t reference,
                                  int16_t primary_raw,
                                  int16_t reference_raw)
{
    if (!s_speaker.output_streaming) {
        return;
    }
    s_speaker.output_stream_counter++;
    if (s_speaker.output_stream_counter < s_speaker.output_stream_decimation) {
        return;
    }
    s_speaker.output_stream_counter = 0;
    serial_printf("$OUT,%lu,%d,%d,%d,%lu,%lu,%lu,%lu,%lu,%d,%d\r\n",
                  static_cast<unsigned long>(s_speaker.output_stream_seq++),
                  static_cast<int>(output),
                  static_cast<int>(primary),
                  static_cast<int>(reference),
                  static_cast<unsigned long>(s_speaker.primary_env),
                  static_cast<unsigned long>(s_speaker.reference_env),
                  static_cast<unsigned long>(s_speaker.output_env),
                  static_cast<unsigned long>(s_speaker.update_count),
                  static_cast<unsigned long>(s_speaker.bin_update_count),
                  static_cast<int>(primary_raw),
                  static_cast<int>(reference_raw));
}

void play_speaker_tone(uint duration_ms, uint tone_hz)
{
    const bool restart_after = s_speaker.running;
    stop_speaker_output(false);
    configure_speaker_pwm();

    if (duration_ms == 0) {
        duration_ms = 1500;
    }
    if (duration_ms > 10000) {
        duration_ms = 10000;
    }
    if (tone_hz < 50 || tone_hz > 5000) {
        tone_hz = 1000;
    }

    const uint32_t sample_count =
        (static_cast<uint32_t>(s_sample_rate_hz) * static_cast<uint32_t>(duration_ms)) / 1000u;
    const float phase_step = (2.0f * PI_F * static_cast<float>(tone_hz)) / static_cast<float>(s_sample_rate_hz);
    float phase = 0.0f;
    constexpr float amplitude = static_cast<float>(SPEAKER_PWM_WRAP) * 0.42f;
    constexpr float center = static_cast<float>((SPEAKER_PWM_WRAP + 1U) / 2U);

    serial_printf("[SPK] tone %lu Hz for %lu ms on GP%u\r\n",
                  static_cast<unsigned long>(tone_hz),
                  static_cast<unsigned long>(duration_ms),
                  SPEAKER_PWM_PIN);
    for (uint32_t i = 0; i < sample_count; i++) {
        const int32_t level = static_cast<int32_t>(center + amplitude * sinf(phase));
        pwm_set_gpio_level(SPEAKER_PWM_PIN, clamp_pwm_level(level));
        phase += phase_step;
        if (phase >= 2.0f * PI_F) {
            phase -= 2.0f * PI_F;
        }
        sleep_us(1000000u / s_sample_rate_hz);
    }
    pwm_set_gpio_level(SPEAKER_PWM_PIN, (SPEAKER_PWM_WRAP + 1U) / 2U);
    serial_printf("[SPK] tone done\r\n");

    if (restart_after) {
        start_speaker_output();
    }
}

void stop_usb_speaker_bridge(bool announce)
{
    if (!s_usb_spk.running) {
        return;
    }

    s_usb_spk.running = false;
    stop_capture_sm();
    configure_speaker_pwm();
    pwm_set_gpio_level(SPEAKER_PWM_PIN, (SPEAKER_PWM_WRAP + 1U) / 2U);
    stdio_flush();
    stdio_set_translate_crlf(&stdio_usb, true);
    if (announce) {
        serial_printf("\r\n$USBSPK_DONE,%lu,%lu,%lu,%lu\r\n",
                      static_cast<unsigned long>(s_usb_spk.frames_sent),
                      static_cast<unsigned long>(s_usb_spk.samples_received),
                      static_cast<unsigned long>(s_usb_spk.samples_played),
                      static_cast<unsigned long>(s_usb_spk.underflows));
    }
}

void usb_speaker_apply_return_sample(int16_t sample)
{
    if (s_usb_spk.return_count >= USB_SPK_RETURN_BUFFER_LEN) {
        s_usb_spk.return_read++;
        if (s_usb_spk.return_read >= USB_SPK_RETURN_BUFFER_LEN) {
            s_usb_spk.return_read = 0;
        }
        s_usb_spk.return_count--;
    }

    s_usb_spk.return_buffer[s_usb_spk.return_write] = sample;
    s_usb_spk.return_write++;
    if (s_usb_spk.return_write >= USB_SPK_RETURN_BUFFER_LEN) {
        s_usb_spk.return_write = 0;
    }
    s_usb_spk.return_count++;
    s_usb_spk.samples_received++;
    s_usb_spk.frames_without_return = 0;
}

bool usb_speaker_pop_return_sample(int16_t &sample)
{
    if (s_usb_spk.return_count == 0) {
        return false;
    }

    sample = s_usb_spk.return_buffer[s_usb_spk.return_read];
    s_usb_spk.return_read++;
    if (s_usb_spk.return_read >= USB_SPK_RETURN_BUFFER_LEN) {
        s_usb_spk.return_read = 0;
    }
    s_usb_spk.return_count--;
    return true;
}

void usb_speaker_parse_byte(uint8_t byte)
{
    if (s_usb_spk.rx_state == 0) {
        s_usb_spk.sample_low = byte;
        s_usb_spk.rx_state = 1;
    } else {
        const uint16_t packed =
            static_cast<uint16_t>(s_usb_spk.sample_low) | (static_cast<uint16_t>(byte) << 8);
        usb_speaker_apply_return_sample(static_cast<int16_t>(packed));
        s_usb_spk.rx_state = 0;
    }
}

void pump_usb_speaker_return_audio(void)
{
    for (uint i = 0; i < 16 && s_usb_spk.running; i++) {
        const int ch = getchar_timeout_us(0);
        if (ch == PICO_ERROR_TIMEOUT) {
            break;
        }
        usb_speaker_parse_byte(static_cast<uint8_t>(ch & 0xFF));
    }
}

void start_usb_speaker_bridge(uint duration_ms)
{
    if (s_speaker.running) {
        stop_speaker_output(false);
    }
    if (s_wave.running) {
        s_wave.running = false;
        stop_capture_sm();
    }

    if (duration_ms > USB_SPK_MAX_MS) {
        duration_ms = USB_SPK_MAX_MS;
    }

    s_auto_report = false;
    configure_speaker_pwm();
    pwm_set_gpio_level(SPEAKER_PWM_PIN, (SPEAKER_PWM_WRAP + 1U) / 2U);

    const bool continuous = (duration_ms == 0);
    const uint32_t frame_count = continuous
        ? 0u
        : (static_cast<uint32_t>(s_sample_rate_hz) * static_cast<uint32_t>(duration_ms)) / 1000u;

    s_usb_spk.running = true;
    s_usb_spk.remaining_frames = frame_count;
    s_usb_spk.continuous = continuous;
    s_usb_spk.playback_started = false;
    s_usb_spk.rx_state = 0;
    s_usb_spk.sample_low = 0;
    s_usb_spk.return_read = 0;
    s_usb_spk.return_write = 0;
    s_usb_spk.return_count = 0;
    s_usb_spk.frames_sent = 0;
    s_usb_spk.samples_received = 0;
    s_usb_spk.samples_played = 0;
    s_usb_spk.underflows = 0;

    restart_capture_sm();
    for (uint i = 0; i < PCM_PROBE_FRAMES; i++) {
        (void)pio_sm_get_blocking(s_pio, s_sm);
        (void)pio_sm_get_blocking(s_pio, s_sm);
    }

    serial_printf("$USBSPK,%lu,%lu,mic1,mic2,16le,return=raw16le\r\n",
                  static_cast<unsigned long>(s_sample_rate_hz),
                  static_cast<unsigned long>(frame_count));
    stdio_flush();
    stdio_set_translate_crlf(&stdio_usb, false);
}

void service_usb_speaker_bridge(void)
{
    if (!s_usb_spk.running) {
        return;
    }

    const uint32_t primary_word = pio_sm_get_blocking(s_pio, s_sm);
    const uint32_t reference_word = pio_sm_get_blocking(s_pio, s_sm);
    write_pcm16_value(pcm16_from_sample(sample_from_i2s_word(primary_word)));
    write_pcm16_value(pcm16_from_sample(sample_from_i2s_word(reference_word)));
    s_usb_spk.frames_sent++;

    pump_usb_speaker_return_audio();
    if (!s_usb_spk.running) {
        return;
    }

    if (!s_usb_spk.playback_started) {
        if (s_usb_spk.return_count >= USB_SPK_PREBUFFER_FRAMES) {
            s_usb_spk.playback_started = true;
        } else {
            pwm_set_gpio_level(SPEAKER_PWM_PIN, (SPEAKER_PWM_WRAP + 1U) / 2U);
        }
    }

    int16_t returned_sample = 0;
    if (s_usb_spk.playback_started && usb_speaker_pop_return_sample(returned_sample)) {
        pwm_set_gpio_level(SPEAKER_PWM_PIN, pwm_level_from_audio(static_cast<float>(returned_sample)));
        s_usb_spk.samples_played++;
    } else if (s_usb_spk.playback_started) {
        s_usb_spk.playback_started = false;
        pwm_set_gpio_level(SPEAKER_PWM_PIN, (SPEAKER_PWM_WRAP + 1U) / 2U);
        s_usb_spk.underflows++;
    }

    s_usb_spk.frames_without_return++;
    if (s_usb_spk.samples_received > 0 &&
        s_usb_spk.frames_without_return > USB_SPK_RETURN_TIMEOUT_FRAMES) {
        stop_usb_speaker_bridge(true);
        return;
    }

    if (!s_usb_spk.continuous) {
        if (s_usb_spk.remaining_frames > 0) {
            s_usb_spk.remaining_frames--;
        }
        if (s_usb_spk.remaining_frames == 0) {
            stop_usb_speaker_bridge(true);
        }
    }
}

void service_speaker_output(void)
{
    uint32_t packed_frame = 0;
    if (!dual_ring_pop(packed_frame)) {
        return;
    }

    int16_t primary_pcm = 0;
    int16_t reference_pcm = 0;
    unpack_dual_frame(packed_frame, primary_pcm, reference_pcm);

    const float primary = highpass_step(
        s_speaker.primary_hp,
        static_cast<float>(primary_pcm));
    const float reference = highpass_step(
        s_speaker.reference_hp,
        static_cast<float>(reference_pcm));

    float cleaned = primary;
    if (s_speaker.mode == SPEAKER_MODE_CANCEL) {
        cleaned = adaptive_cancel_step(primary, reference);
    } else if (s_speaker.mode == SPEAKER_MODE_REFERENCE) {
        cleaned = reference;
    } else if (s_speaker.mode == SPEAKER_MODE_DIFF) {
        cleaned = primary - reference;
    }
    float output_sample = highpass_step(s_speaker.voice_hp, cleaned);
    output_sample = lowpass_step(s_speaker.voice_lp, output_sample);
    output_sample = lowpass_step(s_speaker.voice_lp2, output_sample);
    float output = output_sample * s_speaker.gain;
    if (s_speaker.seq < SPEAKER_STARTUP_FADE_FRAMES) {
        output *= static_cast<float>(s_speaker.seq) / static_cast<float>(SPEAKER_STARTUP_FADE_FRAMES);
    }
    output = soft_limit_audio(output);
    if (output > 32767.0f) {
        output = 32767.0f;
    } else if (output < -32768.0f) {
        output = -32768.0f;
    }

    speaker_playback_enqueue(pwm_level_from_audio(output));

    const uint32_t primary_abs = abs_i32(static_cast<int32_t>(primary));
    const uint32_t reference_abs = abs_i32(static_cast<int32_t>(reference));
    const uint32_t output_abs = abs_i32(static_cast<int32_t>(output));
    s_speaker.output_env = smooth_envelope(s_speaker.output_env, output_abs);
    stream_speaker_output_sample(static_cast<int16_t>(output),
                                 static_cast<int16_t>(primary),
                                 static_cast<int16_t>(reference),
                                 primary_pcm,
                                 reference_pcm);
    if (primary_abs > s_speaker.primary_peak) {
        s_speaker.primary_peak = primary_abs;
    }
    if (reference_abs > s_speaker.reference_peak) {
        s_speaker.reference_peak = reference_abs;
    }
    if (output_abs > s_speaker.output_peak) {
        s_speaker.output_peak = output_abs;
    }
    s_speaker.seq++;
}

void stream_pcm_capture(uint duration_ms)
{
    if (duration_ms == 0) {
        duration_ms = DEFAULT_PCM_MS;
    }
    if (duration_ms > MAX_PCM_MS) {
        duration_ms = MAX_PCM_MS;
    }

    if (s_wave.running) {
        s_wave.running = false;
        stop_capture_sm();
    }
    s_auto_report = false;

    const char *slot = choose_active_slot();
    const bool use_left = (strcmp(slot, "left") == 0);
    const uint32_t sample_count =
        (static_cast<uint32_t>(s_sample_rate_hz) * static_cast<uint32_t>(duration_ms)) / 1000u;

    int16_t *pcm_buffer = static_cast<int16_t *>(malloc(sample_count * sizeof(int16_t)));
    if (!pcm_buffer) {
        stream_pcm_direct(sample_count, use_left, slot);
        return;
    }

    restart_capture_sm();
    for (uint i = 0; i < PCM_PROBE_FRAMES; i++) {
        (void)pio_sm_get_blocking(s_pio, s_sm);
        (void)pio_sm_get_blocking(s_pio, s_sm);
    }
    for (uint32_t i = 0; i < sample_count; i++) {
        const uint32_t left_word = pio_sm_get_blocking(s_pio, s_sm);
        const uint32_t right_word = pio_sm_get_blocking(s_pio, s_sm);
        const int32_t sample = use_left ? sample_from_i2s_word(left_word) : sample_from_i2s_word(right_word);
        pcm_buffer[i] = pcm16_from_sample(sample);
    }
    stop_capture_sm();

    serial_printf("$PCM,%lu,%lu,%s,16le\r\n",
                  static_cast<unsigned long>(s_sample_rate_hz),
                  static_cast<unsigned long>(sample_count),
                  slot);
    stdio_flush();

    stdio_set_translate_crlf(&stdio_usb, false);
    for (uint32_t i = 0; i < sample_count; i++) {
        write_pcm16_value(pcm_buffer[i]);
    }
    stdio_flush();
    stdio_set_translate_crlf(&stdio_usb, true);
    free(pcm_buffer);
    serial_printf("\r\n$PCM_DONE\r\n");
}

void print_pins(void)
{
    serial_printf("[PINS] Mic 1 3V -> Proton 3V3, Mic 2 3V -> Proton 3V3\r\n");
    serial_printf("[PINS] Mic 1 GND -> Proton GND, Mic 2 GND -> Proton GND\r\n");
    serial_printf("[PINS] Mic 1 BCLK + Mic 2 BCLK -> Proton GP%u\r\n", MIC_PIN_BCLK);
    serial_printf("[PINS] Mic 1 LRCL/WS + Mic 2 LRCL/WS -> Proton GP%u\r\n", MIC_PIN_LRCLK);
    serial_printf("[PINS] Mic 1 DOUT + Mic 2 DOUT -> Proton GP%u\r\n", MIC_PIN_DOUT);
    serial_printf("[PINS] Mic 1 SEL -> GND, Mic 2 SEL -> 3V3\r\n");
    serial_printf("[PINS] Add one 100 kOhm pull-down from the shared DOUT line to GND if the breakout does not already have one\r\n");
    serial_printf("[PINS] Speaker/amp input -> Proton GP%u PWM, speaker/amp GND -> Proton GND\r\n", SPEAKER_PWM_PIN);
    serial_printf("[PINS] GP%u is PWM audio, not a power driver; use an amplifier or active speaker input\r\n", SPEAKER_PWM_PIN);
}

void print_help(void)
{
    serial_printf("[HELP] help | pins | once | raw [frames] | scan [frames] | decode [0..8] | rate <8000..48000> | frames <64..4096> | auto on/off | wave on [decim] | wave off | pcm [ms] | diffpcm [ms] | diffstream | dualpcm [ms] | dualstream | usbspk [ms] | speaker on/off/status/params/viz/gain/mu/refloor/eps/gate/start/strength/min/release/mode/reset/tone\r\n");
}

uint parse_uint_or_default(const char *text, uint default_value)
{
    if (!text || *text == '\0') {
        return default_value;
    }
    char *end = nullptr;
    const unsigned long value = strtoul(text, &end, 0);
    if (!end || *end != '\0') {
        return default_value;
    }
    return static_cast<uint>(value);
}

float parse_float_or_default(const char *text, float default_value)
{
    if (!text || *text == '\0') {
        return default_value;
    }
    char *end = nullptr;
    const float value = strtof(text, &end);
    if (!end || *end != '\0') {
        return default_value;
    }
    return value;
}

void to_lower_inplace(char *text)
{
    while (text && *text) {
        *text = static_cast<char>(tolower(static_cast<unsigned char>(*text)));
        ++text;
    }
}

void process_command(char *line)
{
    char *argv[4] = {};
    size_t argc = 0;
    char *token = strtok(line, " \t");
    while (token && argc < 4) {
        to_lower_inplace(token);
        argv[argc++] = token;
        token = strtok(nullptr, " \t");
    }
    if (argc == 0) {
        return;
    }

    if (strcmp(argv[0], "help") == 0) {
        print_help();
    } else if (strcmp(argv[0], "pins") == 0) {
        print_pins();
    } else if (strcmp(argv[0], "once") == 0) {
        stop_speaker_output(false);
        print_stats_report(s_frames_per_report);
    } else if (strcmp(argv[0], "raw") == 0) {
        stop_speaker_output(false);
        print_raw_frames((argc >= 2) ? parse_uint_or_default(argv[1], 8) : 8);
    } else if (strcmp(argv[0], "scan") == 0) {
        stop_speaker_output(false);
        scan_alignment((argc >= 2) ? parse_uint_or_default(argv[1], s_frames_per_report) : s_frames_per_report);
    } else if (strcmp(argv[0], "decode") == 0) {
        if (argc < 2) {
            serial_printf("[I2S] decode shift=%lu\r\n", static_cast<unsigned long>(s_decode_shift));
        } else {
            const uint shift = parse_uint_or_default(argv[1], s_decode_shift);
            if (shift > 8) {
                serial_printf("[ERR] decode shift must be 0..8\r\n");
            } else {
                s_decode_shift = shift;
                serial_printf("[I2S] decode shift set to %lu\r\n", static_cast<unsigned long>(s_decode_shift));
            }
        }
    } else if (strcmp(argv[0], "rate") == 0) {
        const uint rate = (argc >= 2) ? parse_uint_or_default(argv[1], s_sample_rate_hz) : s_sample_rate_hz;
        if (rate < MIN_SAMPLE_RATE_HZ || rate > MAX_SAMPLE_RATE_HZ) {
            serial_printf("[ERR] rate must be %u..%u Hz\r\n", MIN_SAMPLE_RATE_HZ, MAX_SAMPLE_RATE_HZ);
        } else {
            const bool restart_speaker = s_speaker.running;
            if (restart_speaker) {
                stop_speaker_output(false);
            }
            apply_sample_rate(rate);
            serial_printf("[I2S] sample rate set to %lu Hz\r\n", static_cast<unsigned long>(s_sample_rate_hz));
            if (restart_speaker) {
                start_speaker_output();
            }
        }
    } else if (strcmp(argv[0], "frames") == 0) {
        const uint frames = (argc >= 2) ? parse_uint_or_default(argv[1], s_frames_per_report) : s_frames_per_report;
        if (frames < MIN_FRAMES_PER_REPORT || frames > MAX_FRAMES_PER_REPORT) {
            serial_printf("[ERR] frames must be %u..%u\r\n", MIN_FRAMES_PER_REPORT, MAX_FRAMES_PER_REPORT);
        } else {
            s_frames_per_report = frames;
            serial_printf("[I2S] report frames set to %lu\r\n", static_cast<unsigned long>(s_frames_per_report));
        }
    } else if (strcmp(argv[0], "auto") == 0) {
        if (argc < 2) {
            serial_printf("[I2S] auto is %s\r\n", s_auto_report ? "on" : "off");
        } else if (strcmp(argv[1], "on") == 0) {
            stop_speaker_output(false);
            s_auto_report = true;
            serial_printf("[I2S] auto on\r\n");
        } else if (strcmp(argv[1], "off") == 0) {
            s_auto_report = false;
            serial_printf("[I2S] auto off\r\n");
        } else {
            serial_printf("[ERR] auto expects on or off\r\n");
        }
    } else if (strcmp(argv[0], "wave") == 0) {
        if (argc < 2) {
            serial_printf("[WAVE] %s decim=%lu stream_hz=%lu\r\n",
                          s_wave.running ? "on" : "off",
                          static_cast<unsigned long>(s_wave.decimation),
                          static_cast<unsigned long>(s_sample_rate_hz / s_wave.decimation));
        } else if (strcmp(argv[1], "on") == 0) {
            stop_speaker_output(false);
            const uint decimation =
                (argc >= 3) ? parse_uint_or_default(argv[2], s_wave.decimation) : s_wave.decimation;
            if (decimation < MIN_WAVE_DECIMATION || decimation > MAX_WAVE_DECIMATION) {
                serial_printf("[ERR] wave decim must be %u..%u\r\n", MIN_WAVE_DECIMATION, MAX_WAVE_DECIMATION);
            } else {
                s_wave.decimation = decimation;
                set_wave_stream(true);
            }
        } else if (strcmp(argv[1], "off") == 0) {
            set_wave_stream(false);
        } else {
            serial_printf("[ERR] wave expects on or off\r\n");
        }
    } else if (strcmp(argv[0], "pcm") == 0) {
        const uint duration_ms = (argc >= 2) ? parse_uint_or_default(argv[1], DEFAULT_PCM_MS) : DEFAULT_PCM_MS;
        stream_pcm_capture(duration_ms);
    } else if (strcmp(argv[0], "diffpcm") == 0) {
        const uint duration_ms = (argc >= 2) ? parse_uint_or_default(argv[1], DEFAULT_PCM_MS) : DEFAULT_PCM_MS;
        stream_diff_pcm(duration_ms, false);
    } else if (strcmp(argv[0], "diffstream") == 0) {
        stream_diff_pcm(0, true);
    } else if (strcmp(argv[0], "dualpcm") == 0) {
        const uint duration_ms = (argc >= 2) ? parse_uint_or_default(argv[1], DEFAULT_PCM_MS) : DEFAULT_PCM_MS;
        stream_dual_pcm(duration_ms, false);
    } else if (strcmp(argv[0], "dualstream") == 0) {
        stream_dual_pcm(0, true);
    } else if (strcmp(argv[0], "usbspk") == 0) {
        const uint duration_ms = (argc >= 2) ? parse_uint_or_default(argv[1], USB_SPK_DEFAULT_MS) : USB_SPK_DEFAULT_MS;
        start_usb_speaker_bridge(duration_ms);
    } else if (strcmp(argv[0], "speaker") == 0) {
        if (argc < 2 || strcmp(argv[1], "status") == 0) {
            print_speaker_status();
        } else if (strcmp(argv[1], "params") == 0) {
            print_speaker_params();
        } else if (strcmp(argv[1], "on") == 0) {
            start_speaker_output();
        } else if (strcmp(argv[1], "off") == 0) {
            stop_speaker_output(true);
        } else if (strcmp(argv[1], "viz") == 0) {
            if (argc < 3 || strcmp(argv[2], "status") == 0) {
                serial_printf("[OUT] %s decim=%u stream_hz=%lu\r\n",
                              s_speaker.output_streaming ? "on" : "off",
                              s_speaker.output_stream_decimation,
                              static_cast<unsigned long>(s_sample_rate_hz / s_speaker.output_stream_decimation));
            } else if (strcmp(argv[2], "off") == 0) {
                set_speaker_output_stream(false, s_speaker.output_stream_decimation);
            } else if (strcmp(argv[2], "on") == 0) {
                const uint decimation =
                    (argc >= 4) ? parse_uint_or_default(argv[3], s_speaker.output_stream_decimation)
                                : s_speaker.output_stream_decimation;
                set_speaker_output_stream(true, decimation);
            } else {
                const uint decimation = parse_uint_or_default(argv[2], s_speaker.output_stream_decimation);
                set_speaker_output_stream(true, decimation);
            }
        } else if (strcmp(argv[1], "gain") == 0) {
            if (argc < 3) {
                serial_printf("[SPK] gain=%.2fx\r\n", static_cast<double>(s_speaker.gain));
            } else {
                const float gain = parse_float_or_default(argv[2], s_speaker.gain);
                if (gain < 0.0f || gain > 256.0f) {
                    serial_printf("[ERR] speaker gain must be 0..256\r\n");
                } else {
                    s_speaker.gain = gain;
                    serial_printf("[SPK] gain set to %.2fx\r\n", static_cast<double>(s_speaker.gain));
                }
            }
        } else if (strcmp(argv[1], "mu") == 0) {
            if (argc < 3) {
                serial_printf("[SPK] mu=%.3f\r\n", static_cast<double>(s_speaker.mu));
            } else {
                const float mu = parse_float_or_default(argv[2], s_speaker.mu);
                if (mu < 0.0f || mu > 1.0f) {
                    serial_printf("[ERR] speaker mu must be 0..1\r\n");
                } else {
                    s_speaker.mu = mu;
                    serial_printf("[SPK] mu set to %.3f\r\n", static_cast<double>(s_speaker.mu));
                }
            }
        } else if (strcmp(argv[1], "refloor") == 0) {
            if (argc < 3) {
                serial_printf("[SPK] refloor=%.3f\r\n", static_cast<double>(s_speaker.ref_bin_floor));
            } else {
                const float value = parse_float_or_default(argv[2], s_speaker.ref_bin_floor);
                if (value < 0.0f || value > 200.0f) {
                    serial_printf("[ERR] speaker refloor must be 0..200\r\n");
                } else {
                    s_speaker.ref_bin_floor = value;
                    serial_printf("[SPK] refloor set to %.3f\r\n", static_cast<double>(s_speaker.ref_bin_floor));
                }
            }
        } else if (strcmp(argv[1], "eps") == 0) {
            if (argc < 3) {
                serial_printf("[SPK] eps=%.3f\r\n", static_cast<double>(s_speaker.eps_scale));
            } else {
                const float value = parse_float_or_default(argv[2], s_speaker.eps_scale);
                if (value < 0.0f || value > 500.0f) {
                    serial_printf("[ERR] speaker eps must be 0..500\r\n");
                } else {
                    s_speaker.eps_scale = value;
                    serial_printf("[SPK] eps set to %.3f\r\n", static_cast<double>(s_speaker.eps_scale));
                }
            }
        } else if (strcmp(argv[1], "gate") == 0) {
            if (argc < 3) {
                serial_printf("[SPK] gate=%.4f\r\n", static_cast<double>(s_speaker.primary_ratio_gate));
            } else {
                const float value = parse_float_or_default(argv[2], s_speaker.primary_ratio_gate);
                if (value < 0.0f || value > 10.0f) {
                    serial_printf("[ERR] speaker gate must be 0..10\r\n");
                } else {
                    s_speaker.primary_ratio_gate = value;
                    serial_printf("[SPK] gate set to %.4f\r\n", static_cast<double>(s_speaker.primary_ratio_gate));
                }
            }
        } else if (strcmp(argv[1], "start") == 0) {
            if (argc < 3) {
                serial_printf("[SPK] start=%.4f\r\n", static_cast<double>(s_speaker.mask_ratio_start));
            } else {
                const float value = parse_float_or_default(argv[2], s_speaker.mask_ratio_start);
                if (value < 0.0f || value > 10.0f) {
                    serial_printf("[ERR] speaker start must be 0..10\r\n");
                } else {
                    s_speaker.mask_ratio_start = value;
                    serial_printf("[SPK] start set to %.4f\r\n", static_cast<double>(s_speaker.mask_ratio_start));
                }
            }
        } else if (strcmp(argv[1], "strength") == 0) {
            if (argc < 3) {
                serial_printf("[SPK] strength=%.3f\r\n", static_cast<double>(s_speaker.mask_strength));
            } else {
                const float value = parse_float_or_default(argv[2], s_speaker.mask_strength);
                if (value < 0.0f || value > 100.0f) {
                    serial_printf("[ERR] speaker strength must be 0..100\r\n");
                } else {
                    s_speaker.mask_strength = value;
                    serial_printf("[SPK] strength set to %.3f\r\n", static_cast<double>(s_speaker.mask_strength));
                }
            }
        } else if (strcmp(argv[1], "min") == 0) {
            if (argc < 3) {
                serial_printf("[SPK] min=%.3f\r\n", static_cast<double>(s_speaker.mask_min));
            } else {
                const float value = parse_float_or_default(argv[2], s_speaker.mask_min);
                if (value < 0.0f || value > 1.0f) {
                    serial_printf("[ERR] speaker min must be 0..1\r\n");
                } else {
                    s_speaker.mask_min = value;
                    serial_printf("[SPK] min set to %.3f\r\n", static_cast<double>(s_speaker.mask_min));
                }
            }
        } else if (strcmp(argv[1], "release") == 0) {
            if (argc < 3) {
                serial_printf("[SPK] release=%.3f\r\n", static_cast<double>(s_speaker.mask_release));
            } else {
                const float value = parse_float_or_default(argv[2], s_speaker.mask_release);
                if (value < 0.0f || value > 1.0f) {
                    serial_printf("[ERR] speaker release must be 0..1\r\n");
                } else {
                    s_speaker.mask_release = value;
                    serial_printf("[SPK] release set to %.3f\r\n", static_cast<double>(s_speaker.mask_release));
                }
            }
        } else if (strcmp(argv[1], "mode") == 0) {
            if (argc < 3) {
                serial_printf("[SPK] mode=%s\r\n", speaker_mode_name(s_speaker.mode));
            } else if (strcmp(argv[2], "cancel") == 0) {
                s_speaker.mode = SPEAKER_MODE_CANCEL;
                reset_speaker_canceller();
                serial_printf("[SPK] mode set to cancel\r\n");
            } else if (strcmp(argv[2], "primary") == 0) {
                s_speaker.mode = SPEAKER_MODE_PRIMARY;
                reset_speaker_canceller();
                serial_printf("[SPK] mode set to primary\r\n");
            } else if (strcmp(argv[2], "reference") == 0) {
                s_speaker.mode = SPEAKER_MODE_REFERENCE;
                reset_speaker_canceller();
                serial_printf("[SPK] mode set to reference\r\n");
            } else if (strcmp(argv[2], "diff") == 0) {
                s_speaker.mode = SPEAKER_MODE_DIFF;
                reset_speaker_canceller();
                serial_printf("[SPK] mode set to diff\r\n");
            } else {
                serial_printf("[ERR] speaker mode expects cancel, primary, reference, or diff\r\n");
            }
        } else if (strcmp(argv[1], "reset") == 0) {
            reset_speaker_canceller();
            serial_printf("[SPK] adaptive filter reset\r\n");
        } else if (strcmp(argv[1], "tone") == 0) {
            const uint duration_ms = (argc >= 3) ? parse_uint_or_default(argv[2], 1500) : 1500;
            play_speaker_tone(duration_ms, 1000);
        } else {
            serial_printf("[ERR] speaker expects on, off, status, params, viz, gain, mu, refloor, eps, gate, start, strength, min, release, mode, reset, or tone\r\n");
        }
    } else {
        serial_printf("[ERR] unknown command: %s\r\n", argv[0]);
        print_help();
    }
}

void service_wave_stream(void)
{
    if (!s_wave.running) {
        return;
    }

    int32_t left_sample = 0;
    int32_t right_sample = 0;
    for (uint i = 0; i < s_wave.decimation; i++) {
        const uint32_t left_word = pio_sm_get_blocking(s_pio, s_sm);
        const uint32_t right_word = pio_sm_get_blocking(s_pio, s_sm);
        left_sample = sample_from_i2s_word(left_word);
        right_sample = sample_from_i2s_word(right_word);
    }

    serial_printf("$W,%lu,%ld,%ld\r\n",
                  static_cast<unsigned long>(s_wave.seq++),
                  static_cast<long>(left_sample),
                  static_cast<long>(right_sample));
}

void pump_stdio(void)
{
    while (true) {
        const int ch = getchar_timeout_us(0);
        if (ch == PICO_ERROR_TIMEOUT) {
            return;
        }

        if (ch == '\r' || ch == '\n') {
            if (s_cmd_len == 0) {
                continue;
            }
            s_cmd_buffer[s_cmd_len] = '\0';
            process_command(s_cmd_buffer);
            s_cmd_len = 0;
            continue;
        }

        if (s_cmd_len + 1 < sizeof(s_cmd_buffer)) {
            s_cmd_buffer[s_cmd_len++] = static_cast<char>(ch);
        }
    }
}

} // namespace

int main(void)
{
    stdio_usb_init();
    sleep_ms(1500);

    init_i2s_pio();
    multicore_launch_core1(capture_core1_main);

    serial_printf("\r\n=== Proton ICS43434 I2S Mic Test ===\r\n");
    print_pins();
    print_help();
    serial_printf("[I2S] default rate=%lu Hz, report frames=%lu\r\n",
                  static_cast<unsigned long>(s_sample_rate_hz),
                  static_cast<unsigned long>(s_frames_per_report));
    serial_printf("[SPK] standalone auto-start: A_7_6 live visual tuning -> GP%u\r\n", SPEAKER_PWM_PIN);
    start_speaker_output();
    while (true) {
        if (s_usb_spk.running) {
            service_usb_speaker_bridge();
        } else {
            pump_stdio();
        }
        if (s_usb_spk.running) {
            continue;
        } else if (s_speaker.running) {
            service_speaker_output();
        } else if (s_wave.running) {
            service_wave_stream();
        } else if (s_auto_report) {
            print_stats_report(s_frames_per_report);
            sleep_ms(100);
        } else {
            sleep_ms(1);
        }
    }
}
