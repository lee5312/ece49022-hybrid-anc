#pragma once

#include <Arduino.h>
#include "DMAChannel.h"
#include "audio_config.h"
#include "imxrt.h"

class SAIInput {
public:
    SAIInput();

    void begin();
    bool available() const;
    const int32_t* read();

    void isr();

private:

    // =========================
    // 4-channel interleaved buffer
    // =========================
    // Explicit ping-pong buffers for clarity and safety.
    // DMA writes to one while the CPU reads from the other.
    int32_t dma_buffer[AUDIO_BLOCK * AUDIO_CHANNELS_IN * 2];
    int32_t bufferA[AUDIO_BLOCK * AUDIO_CHANNELS_IN];
    int32_t bufferB[AUDIO_BLOCK * AUDIO_CHANNELS_IN];

    // Pointers to manage the buffer swap
    int32_t* writeBuffer;
    int32_t* readBuffer;

    // Flag set by ISR when a buffer is ready for processing
    volatile bool bufferReady = false;

    // =========================
    // DMA
    // =========================
    DMAChannel dma;
    static void dmaISR(void *arg);

     static SAIInput* instance_ptr;

    void configSAI1();
    void configDMA();
};