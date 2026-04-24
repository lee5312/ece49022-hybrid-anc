#include "sai_input.h"
#include "imxrt.h"

// ======================================================
// GLOBAL POINTER FOR ISR CALLBACK
// ======================================================
// DMA interrupts are "C-style" functions (no class context),
// so we store a pointer to the active class instance.
SAIInput* SAIInput::instance_ptr = nullptr;


SAIInput::SAIInput() {
    instance_ptr = this;

    // Set up the initial buffer pointers.
    // DMA will start by writing to bufferA.
    writeBuffer = dma_buffer;
    readBuffer = dma_buffer + (AUDIO_BLOCK * AUDIO_CHANNELS_IN);
}


// ======================================================
// BEGIN: INITIALIZE ENTIRE AUDIO INPUT SYSTEM
// ======================================================
void SAIInput::begin() {

    // Configure hardware: SAI peripheral (I2S) and DMA engine to move data from SAI → memory
    configSAI1();
    configDMA();
    
    // IMPORTANT: Enable the SAI receiver AFTER the DMA is ready.
    // Also, enable the transmitter to generate clocks.
    I2S1_RCSR |= I2S_RCSR_RE;
    I2S1_TCSR |= I2S_TCSR_TE | I2S_TCSR_BCE; // TX provides the master clock

    // Enable the DMA channel to start receiving data
    dma.enable();
}


// ======================================================
// SAI1 CONFIGURATION (AUDIO SERIAL INTERFACE)
// ======================================================
// This configures the Teensy hardware I2S peripheral (SAI1)
// which receives audio data from PCM1808 ADCs.
void SAIInput::configSAI1() {

    // Enable clock gate to SAI1 peripheral
    // (Without this, SAI1 hardware is OFF)
    CCM_CCGR5 |= CCM_CCGR5_SAI1(CCM_CCGR_ON);

    // -------------------------
    // DISABLE RECEIVER BEFORE CONFIG
    // -------------------------
    // We must disable SAI before changing its settings
    I2S1_TCSR = 0;
    I2S1_RCSR = 0;

    // -------------------------
    // == TRANSMITTER CONFIGURATION (Clock Master) ==
    // -------------------------
    // The TX side will generate the master BCLK and LRCLK for the entire audio bus.
    I2S1_TCR2 = I2S_TCR2_SYNC(0) |  // Asynchronous mode
                I2S_TCR2_BCP |     // Bit clock is provided by SAI
                I2S_TCR2_BCD |  // Bit clock is output from SAI
                I2S_TCR2_DIV(0);   // BCLK = MCLK / 2
    I2S1_TCR4 = I2S_TCR4_FSP |     // Frame sync polarity
                I2S_TCR4_FSD |  // Frame sync is output from SAI
                I2S_TCR4_FRSZ(AUDIO_CHANNELS_IN - 1); // ***FIXED: Frame is 4 words long for TDM4***

    // -------------------------
    // FIFO WATERMARK CONFIG
    // -------------------------
    // RFW = when FIFO triggers DMA request
    I2S1_RCR1 = I2S_RCR1_RFW(0); // Set FIFO watermark to 1 for lowest latency

    // -------------------------
    // CLOCK + FRAME CONFIG
    // -------------------------
    I2S1_RCR2 =
        I2S_RCR2_SYNC(0) |   // asynchronous mode (SAI generates its own timing)
        I2S_RCR2_BCP |       // bit clock polarity (data sampled on opposite edge)
        I2S_RCR2_MSEL(1);    // clock source = PLL (Audio PLL)

    // -------------------------
    // ENABLE RECEIVER CHANNEL
    // -------------------------
    // RCE = Receiver Channel Enable
    I2S1_RCR3 = I2S_RCR3_RCE;

    // -------------------------
    // FRAME FORMAT CONFIGURATION
    // -------------------------
    I2S1_RCR4 =
        I2S_RCR4_FRSZ(AUDIO_CHANNELS_IN - 1) |   // frame size = 2 words (stereo frame)
        I2S_RCR4_SYWD(0) |  // sync width = 1-bit word
        I2S_RCR4_MF;         // MSB first format

    // -------------------------
    // WORD CONFIGURATION
    // -------------------------
    I2S1_RCR5 =
        I2S_RCR5_WNW(15) |   // word N width = 32 bits
        I2S_RCR5_W0W(15) |   // word 0 width = 32 bits
        I2S_RCR5_FBT(15);    // first bit transmitted = MSB

    // -------------------------
    // ENABLE RECEIVER + CLOCK
    // -------------------------
    I2S1_RCSR = I2S_RCSR_FRDE; // Enable FIFO Request DMA

    // Enable the first 4 word slots in the TDM frame
    I2S1_RMR = 0x000F;
}


// ======================================================
// DMA CONFIGURATION (DATA MOVEMENT ENGINE)
// ======================================================
// DMA = Direct Memory Access controller
// It moves audio data from SAI hardware → RAM without CPU load
void SAIInput::configDMA() {
    // Allocate DMA channel
    dma.begin(true);

    // -------------------------
    // SOURCE: SAI1 RX FIFO
    // -------------------------
    // This is where incoming audio samples arrive
    dma.source((volatile uint32_t&)I2S1_RDR0);


    // -------------------------
    // DESTINATION: CIRCULAR BUFFER
    // -------------------------
    // Instead of swapping buffers manually, DMA loops continuously
    dma.destinationBuffer(dma_buffer, AUDIO_BLOCK * AUDIO_CHANNELS_IN * 2);

    // -------------------------
    // Configure the DMA transfer properties
    // -------------------------
    dma.transferSize(4);          // 32-bit transfers
    //dma.transferCount(AUDIO_BLOCK * AUDIO_CHANNELS_IN); // Total samples in one block

    // -------------------------
    // HARDWARE TRIGGER SOURCE
    // -------------------------
    // DMA is triggered every time SAI receives new audio data
    dma.triggerAtHardwareEvent(DMAMUX_SOURCE_SAI1_RX);

    // -------------------------
    // INTERRUPT CALLBACK
    // -------------------------
    // This runs when DMA completes a transfer cycle
    dma.attachInterrupt(dmaISR);
    dma.interruptAtHalf();
    dma.interruptAtCompletion();

}


// ======================================================
// STATIC DMA INTERRUPT SERVICE ROUTINE (ISR)
// ======================================================
void SAIInput::dmaISR(void* arg) {
    // Cast the argument back to our class instance and call the member ISR
    if (instance_ptr) {
        instance_ptr->isr();
    }
}


// ======================================================
// INSTANCE-SPECIFIC ISR
// ======================================================
void SAIInput::isr() {

    uint32_t dma_csr = dma.TCD->CSR; // get the CSR to check which interrupt occurred
    // Clear interrupt flag so DMA can continue
    dma.clearInterrupt();

    if (dma_csr & DMA_TCD_CSR_DREQ) { // if we are just disabling.. return
        return;
    }

   if ( (dma_csr & DMA_TCD_CSR_INTMAJOR) || (dma_csr & DMA_TCD_CSR_INTHALF) ) {
        int16_t* in = (int16_t*)dma.destinationAddress();
        int16_t* out = (int16_t*)dma.sourceAddress();

        if ( in >= dma_buffer + (AUDIO_BLOCK * AUDIO_CHANNELS_IN) ) {
            // DMA is writing to the second half, so we can read from the first
            readBuffer = dma_buffer;
        } else {
            // DMA is writing to the first half, so we can read from the second
            readBuffer = dma_buffer + (AUDIO_BLOCK * AUDIO_CHANNELS_IN);
        }
        bufferReady = true;
    }

    // Enable DMA channel
    dma.enable();
}


// ======================================================
// PUBLIC API: CHECK IF NEW AUDIO IS READY
// ======================================================
bool SAIInput::available() const {
    return bufferReady;
}


// ======================================================
// PUBLIC API: READ AUDIO BUFFER
// ======================================================
// Returns pointer to raw interleaved audio samples
const int16_t* SAIInput::read() {

    if (!bufferReady) {
        return nullptr; // No new data, return null
    }

    // Acknowledge that we are consuming the buffer
    noInterrupts(); // Safely clear the flag
    bufferReady = false;
    interrupts();

    // Return pointer to DMA-filled buffer
    return readBuffer;
}