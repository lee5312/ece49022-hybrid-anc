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
    Serial.println("Enabling SAIInput");
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
    // FIFO WATERMARK CONFIG
    // -------------------------
    // RFW = when FIFO triggers DMA request
    I2S1_RCR1 = I2S_RCR1_RFW(0); // Set FIFO watermark to 1 for lowest latency

    // -------------------------
    // CLOCK + FRAME CONFIG
    // -------------------------
    I2S1_RCR2 =
        I2S_RCR2_SYNC(0) |   // asynchronous mode (SAI generates its own timing)
        I2S_RCR2_BCP |       // bit clock polarity (data sampled on opposite edge)     May remmove this if clock not working
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
        I2S_RCR4_FRSZ(1) |   // frame size = 2 words (stereo frame)
        I2S_RCR4_SYWD(31) |  // sync width = 32-bit word
        I2S_RCR4_MF |       // MSB first format
        I2S_RCR4_FSP ;     // frame sync pulse = active low and early (data starts on falling edge of frame sync)

    // -------------------------
    // WORD CONFIGURATION
    // -------------------------
    I2S1_RCR5 =
        I2S_RCR5_WNW(23) |   // word N width = 32 bits
        I2S_RCR5_W0W(23) |   // word 0 width = 32 bits
        I2S_RCR5_FBT(23);    // first bit transmitted = MSB

    // -------------------------
    // ENABLE RECEIVER + CLOCK
    // -------------------------
    I2S1_RCSR = I2S_RCSR_FRDE; // Enable FIFO Request DMA

    // Enable the first 4 word slots in the TDM frame
    I2S1_RMR = 0x0003;
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
    
    dma.clearInterrupt(); // Always clear the interrupt flag first.

    // This is an efficient way to check the DMA destination address.
    // It determines which half of the circular buffer has just been filled.
    uint32_t* current_dma_dest = (uint32_t*)dma.destinationAddress();
    uint32_t* buffer_midpoint = dma_buffer + (AUDIO_BLOCK * AUDIO_CHANNELS_IN);

    if (current_dma_dest >= buffer_midpoint) {
        // DMA is currently writing to the second half (buffer B),
        // so the first half (buffer A) is ready for processing.
        readBuffer = dma_buffer;
    } else {
        // DMA is currently writing to the first half (buffer A),
        // so the second half (buffer B) is ready for processing.
        readBuffer = buffer_midpoint;
    }
    
    bufferReady = true; // Set the flag to notify the main loop.

    // This is not needed for circular DMA. The hardware handles it.
    // dma.enable(); << REMOVE
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
const int32_t* SAIInput::read() {

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