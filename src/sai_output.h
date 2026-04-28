#pragma once

#include <Arduino.h>
#include "DMAChannel.h"
#include "audio_config.h"
#include "imxrt.h"

// Define I2S peripherals for convenience
#define I2S1 IMXRT_SAI1


// Define how many stereo pairs we are outputting
#define NUM_STEREO_PAIRS 3 

class SAIOutput {
public:
    // The constructor now only needs the SAI peripheral, as it's always master
    SAIOutput(IMXRT_SAI_t* sai_p);
    void begin();

    // The write function is now completely different.
    // It must take pointers to all three of the final processed buffers.
    void write(const int32_t* dac1_data, const int32_t* dac2_data, const int32_t* dac3_data);

    void isr();

private:
    // The DMA buffer must now be large enough to hold interleaved data for all channels.
    // Size = samples_per_block * num_stereo_pairs * channels_per_pair * num_halves
    // Size = 128 * 3 * 2 * 2 = 1536
    int32_t dma_buffer[AUDIO_BLOCK * NUM_STEREO_PAIRS * 2]; 

    int32_t* buffer_ptr_a;
    int32_t* buffer_ptr_b;

    DMAChannel dma;
    IMXRT_SAI_t* sai;

    // We only have one instance now
    static SAIOutput* instance;
    static void dmaISR();

    void configSAI();
    void configDMA();
};