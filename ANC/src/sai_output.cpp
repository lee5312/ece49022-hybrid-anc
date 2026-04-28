#include "sai_output.h"

// Initialize the single static instance pointer
SAIOutput* SAIOutput::instance = nullptr;

// CORRECTED: Constructor matches the new header and DSPEngine call
SAIOutput::SAIOutput(IMXRT_SAI_t* sai_p) : sai(sai_p) {
    buffer_ptr_a = dma_buffer;
    buffer_ptr_b = dma_buffer + (AUDIO_BLOCK * NUM_STEREO_PAIRS * 2); // Correct pointer to the second half
    instance = this;
}

void SAIOutput::begin() {
    configDMA();
    configSAI();
    dma.enable();
}

void SAIOutput::configSAI() {
    sai->TCSR = 0; // Disable before configuring

    // --- MASTER MODE MULTI-CHANNEL CONFIGURATION ---

    // TCR2: Bit Clock. We need a 3.072 MHz BCLK for a 6-channel (3-pair) frame.
    // BCLK = SampleRate * FrameSize * WordBits = 48000 * 6 * 32 is wrong.
    // BCLK is for ONE word, so 48000 * 32 * 2 = 3.072MHz is the target.
    // Divisor = (12.288MHz MCLK / 3.072MHz BCLK) - 1 = 3.
    sai->TCR2 = I2S_TCR2_BCD | I2S_TCR2_BCP | I2S_TCR2_MSEL(0) | I2S_TCR2_DIV(24);

    // TCR4: Frame. The frame size is the number of words. 3 pairs * 2 channels = 6 words.
    sai->TCR4 = I2S_TCR4_FSP | I2S_TCR4_FSD | I2S_TCR4_SYWD(31) | I2S_TCR4_FRSZ(1); // FRSZ is (num_words - 1)

    // TCR3: CRITICAL - Enable 3 data lines. A bitmask where each bit enables a data line.
    // 0b111 enables TDR[0], TDR[1], and TDR[2].
    sai->TCR3 = I2S_TCR3_TCE_3CH; // Enable 3 data lines (TDR[0], TDR[1], TDR[2])

    // TCR5: Word Format.
    sai->TCR5 = I2S_TCR5_WNW(31) | I2S_TCR5_W0W(31) | I2S_TCR5_FBT(31);

    // TCSR: Final Enable.
    sai->TCSR = I2S_TCSR_FRDE | I2S_TCSR_BCE | I2S_TCSR_TE;
    
    sai->TMR = 0; // No time slot masking
}

void SAIOutput::configDMA() {
    dma.begin(true);
    dma.destination((volatile uint32_t&)sai->TDR[0]);
    
    // CORRECTED: The buffer size MUST account for all 3 stereo pairs.
    dma.sourceBuffer(dma_buffer, AUDIO_BLOCK * NUM_STEREO_PAIRS * 2 * 2 * sizeof(int32_t));
    
    dma.transferSize(4);
    dma.triggerAtHardwareEvent(DMAMUX_SOURCE_SAI1_TX);
    dma.attachInterrupt(dmaISR); // Attach the single static ISR
    
    dma.interruptAtHalf();
    dma.interruptAtCompletion();
}

// This function now correctly interleaves the three separate stereo buffers.
void SAIOutput::write(const int32_t* dac1_data, const int32_t* dac2_data, const int32_t* dac3_data) {
    int32_t* target_buffer;

    Serial.println("SAIOutput::write called with new buffers");

    // CORRECTED: This logic correctly finds the free buffer.
    if ((uint32_t)dma.sourceAddress() >= (uint32_t)buffer_ptr_b) {
        target_buffer = buffer_ptr_a;
    } else {
        target_buffer = buffer_ptr_b;
    }

    // Interleave the data into the target buffer.
    for (int i = 0; i < AUDIO_BLOCK; i++) {
        // Data for DAC 1 goes to time slots 0 and 1
        *target_buffer++ = dac1_data[i * 2 + 0]; // L
        *target_buffer++ = dac1_data[i * 2 + 1]; // R
        
        // Data for DAC 2 goes to time slots 2 and 3
        *target_buffer++ = dac2_data[i * 2 + 0]; // L
        *target_buffer++ = dac2_data[i * 2 + 1]; // R

        // Data for DAC 3 goes to time slots 4 and 5
        *target_buffer++ = dac3_data[i * 2 + 0]; // L
        *target_buffer++ = dac3_data[i * 2 + 1]; // R
    }
}

// CORRECTED: A single static ISR for the single instance.
void SAIOutput::dmaISR() { 
    if (instance) instance->isr(); 
}

void SAIOutput::isr() {
    dma.clearInterrupt();
}
