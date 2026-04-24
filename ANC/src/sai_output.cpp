#include "sai_output.h"

// Initialize static instance pointers
SAIOutput* SAIOutput::instances[3] = {nullptr, nullptr, nullptr};

SAIOutput::SAIOutput(IMXRT_SAI_t* sai_p, bool isMaster) : sai(sai_p), masterMode(isMaster) {
    buffer_ptr_a = dma_buffer;
    buffer_ptr_b = dma_buffer + (AUDIO_BLOCK * AUDIO_CHANNELS_OUT);

    if (sai == &I2S1) instances[0] = this;
    if (sai == &I2S2) instances[1] = this;
    if (sai == &I2S3) instances[2] = this;
}

void SAIOutput::begin() {
    configDMA();
    configSAI();
    dma.enable();
}

void SAIOutput::configSAI() {
    sai->TCSR = 0; // Disable first, then enable with DMA
    sai->TCR1 = I2S_TCR1_RFW(0);

    if (masterMode) {
        // SAI1 is master
        sai->TCR2 = I2S_TCR2_SYNC(0) | I2S_TCR2_BCP | I2S_TCR2_BCD | I2S_TCR2_DIV(0);
        sai->TCR4 = I2S_TCR4_FSP | I2S_TCR4_FSD | I2S_TCR4_FRSZ(1);
        sai->TCSR = I2S_TCSR_BCE | I2S_TCSR_FRDE | I2S_TCSR_TE;
    } else {
        // SAI2/3 are slaves
        sai->TCR2 = I2S_TCR2_SYNC(1) | I2S_TCR2_BCP;
        sai->TCR4 = I2S_TCR4_FSP | I2S_TCR4_FSD | I2S_TCR4_FRSZ(1);
        sai->TCSR = I2S_TCSR_FRDE | I2S_TCSR_TE;
    }

    sai->TCR3 = I2S_TCR3_TCE;
    sai->TCR5 = I2S_TCR5_WNW(15) | I2S_TCR5_W0W(15) | I2S_TCR5_FBT(15);
    sai->TMR = 0;
}

void SAIOutput::configDMA() {
    dma.begin(true);
    dma.destination((volatile uint32_t&)sai->TDR[0]);
    dma.sourceBuffer(dma_buffer, AUDIO_BLOCK * AUDIO_CHANNELS_OUT * 2);
    dma.transferSize(4); // 32-bit transfers

    if (sai == &I2S1) {
        dma.triggerAtHardwareEvent(DMAMUX_SOURCE_SAI1_TX);
        dma.attachInterrupt(dmaISR1);
    } else if (sai == &I2S2) {
        dma.triggerAtHardwareEvent(DMAMUX_SOURCE_SAI2_TX);
        dma.attachInterrupt(dmaISR2);
    } else if (sai == &I2S3) {
        dma.triggerAtHardwareEvent(DMAMUX_SOURCE_SAI3_TX);
        dma.attachInterrupt(dmaISR3);
    }
    
    dma.interruptAtHalf();
    dma.interruptAtCompletion();
}

void SAIOutput::write(const int16_t* data) {
    if ((uint32_t)dma.sourceAddress() < (uint32_t)buffer_ptr_b) {
        // DMA is reading from the first half, so we can write to the second
        memcpy(buffer_ptr_b, data, AUDIO_BLOCK * AUDIO_CHANNELS_OUT * sizeof(int16_t));
    } else {
        // DMA is reading from the second half, so we can write to the first
        memcpy(buffer_ptr_a, data, AUDIO_BLOCK * AUDIO_CHANNELS_OUT * sizeof(int16_t));
    }
}

// Static ISR functions that call the correct instance's ISR
void SAIOutput::dmaISR1() { if (instances[0]) instances[0]->isr(); }
void SAIOutput::dmaISR2() { if (instances[1]) instances[1]->isr(); }
void SAIOutput::dmaISR3() { if (instances[2]) instances[2]->isr(); }

void SAIOutput::isr() {
    dma.clearInterrupt();
    // No need to do anything else, the DMA hardware handles the buffer switching
}
