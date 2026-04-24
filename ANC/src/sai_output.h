#pragma once

#include <Arduino.h>
#include "DMAChannel.h"
#include "audio_config.h"
#include "imxrt.h"

// Define I2S peripherals for convenience
#define I2S1 IMXRT_SAI1
#define I2S2 IMXRT_SAI2
#define I2S3 IMXRT_SAI3

class SAIOutput {
public:
    SAIOutput(IMXRT_SAI_t* sai_p, bool isMaster);
    void begin();
    void write(const int16_t* data);

    // Made public for ISR
    void isr();

private:

    int16_t dma_buffer[AUDIO_BLOCK * AUDIO_CHANNELS_OUT * 2];
    volatile bool dmaDone = true;

    // Pointers to the two halves of the dma_buffer
    int16_t* buffer_ptr_a;
    int16_t* buffer_ptr_b;
    const int16_t* nextBufferToWrite = nullptr;

    DMAChannel dma;
    IMXRT_SAI_t* sai;
    bool masterMode;

    static SAIOutput* instances[3];
    static void dmaISR1();
    static void dmaISR2();
    static void dmaISR3();

    static void dmaISR(void* arg);

    void configSAI();
    void configDMA();
};
