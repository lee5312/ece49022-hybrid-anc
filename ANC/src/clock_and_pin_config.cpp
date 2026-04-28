#include "clock_and_pin_config.h"
#include "imxrt.h"
#include "Arduino.h"

void init_audio_clocks_and_pins() {
    // ----------------------------------------------------------------
    // 1. CONFIGURE MASTER CLOCK (MCLK) SOURCE FROM AUDIO PLL
    // ----------------------------------------------------------------

    uint32_t pll_audio_register_value = CCM_ANALOG_PLL_AUDIO;
    Serial.printf("Value (HEX): %08x\n", pll_audio_register_value);
    
    // Power down PLL4 to change settings
    CCM_ANALOG_PLL_AUDIO_SET = CCM_ANALOG_PLL_AUDIO_POWERDOWN;
    CCM_ANALOG_PLL_AUDIO_CLR = CCM_ANALOG_PLL_AUDIO_ENABLE | CCM_ANALOG_PLL_AUDIO_BYPASS; // Ensure PLL is disabled before configuring

    pll_audio_register_value = CCM_ANALOG_PLL_AUDIO;
    Serial.printf("Value (HEX): %08x\n", pll_audio_register_value);
    

    // Set the fractional loop divider
    CCM_ANALOG_PLL_AUDIO_NUM = 774;
    CCM_ANALOG_PLL_AUDIO_DENOM = 1000;

    CCM_ANALOG_PLL_AUDIO_SET = CCM_ANALOG_PLL_AUDIO_DIV_SELECT(32)
                            | CCM_ANALOG_PLL_AUDIO_POST_DIV_SELECT(1);

    CCM_ANALOG_PLL_AUDIO_CLR = CCM_ANALOG_PLL_AUDIO_POWERDOWN; // Clear powerdown to enable PLL
    CCM_ANALOG_PLL_AUDIO_SET = CCM_ANALOG_PLL_AUDIO_ENABLE; // Enable the PLL

    pll_audio_register_value = CCM_ANALOG_PLL_AUDIO;
    Serial.printf("Value (HEX): %08x\n", pll_audio_register_value);

    // Wait for the PLL to lock
    while (!(CCM_ANALOG_PLL_AUDIO & CCM_ANALOG_PLL_AUDIO_LOCK)) {
        // Wait...
    }

    pll_audio_register_value = CCM_ANALOG_PLL_AUDIO;
    Serial.printf("Value (HEX): %08x\n", pll_audio_register_value);

    // Enable clock gates for IOMUXC, CCM, and SAI1
    CCM_CCGR4 |= CCM_CCGR4_IOMUXC(CCM_CCGR_ON) | CCM_CCGR4_IOMUXC_GPR(CCM_CCGR_ON); 
    CCM_CCGR5 |= CCM_CCGR5_SAI1(CCM_CCGR_ON) |
                CCM_CCGR5_SAI2(CCM_CCGR_ON) |
                CCM_CCGR5_SAI3(CCM_CCGR_ON) |
                CCM_CCGR5_DMA(CCM_CCGR_ON);
    // Configure Audio PLL (PLL4)
    // Target: Generate a stable PLL frequency. Often around 393.216 MHz for 48kHz audio rates.
    // NOTE: This is often already handled by the Teensy core library, but we can ensure it's set.
    // PLL_AUDIO_CTRL = 0x...
    // Route the Audio PLL to the SAI1 clock root

    // CCM_CSCMR1[SAI1_CLK_SEL] -> 2 selects PLL4
    CCM_CSCMR1 = (CCM_CSCMR1 & ~CCM_CSCMR1_SAI1_CLK_SEL_MASK) | CCM_CSCMR1_SAI1_CLK_SEL(2);
    

    // Set the post-divider for SAI1 clock. This creates the MCLK.
  // MCLK = (PLL4 Freq) / (SAI1_CLK_PODF + 1)
  // MCLK = 393.288 MHz / (16 + 1) = 23.134 MHz; actual: 1.3729MHz
  // This is 524.58 * 44.1 kHz, which is a common oversampling ratio.
    CCM_CS1CDR &= ~(CCM_CS1CDR_SAI1_CLK_PODF_MASK | CCM_CS1CDR_SAI1_CLK_PRED_MASK); // Clear existing divider settings
    CCM_CS1CDR |= CCM_CS1CDR_SAI1_CLK_PRED(5) // Divide by 6
            | CCM_CS1CDR_SAI1_CLK_PODF(3); // Divide by 4
    
    // ----------------------------------------------------------------
    // 2. CONFIGURE PIN MUXING & DIRECTION
    // ----------------------------------------------------------------
    pinMode(20, INPUT);
    pinMode(21, INPUT);

    // MCLK on Pin 23 (GPIO_AD_B1_09)
    // Set MUX control to ALT 3 for SAI1_MCLK
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_09 = 0x3; // SAI1_MCLK

    // BCLK on Pin 26 (GPIO_AD_B1_14)
    // Set MUX control to ALT 3 for SAI1_TX_BCLK
    IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_02 = 0x3; // SAI1_TX_BCLK

    // LRCLK on Pin 27 (GPIO_AD_B1_15)
    // Set MUX control to ALT 3 for SAI1_TX_SYNC
    IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_03 = 0x3; // SAI1_TX_SYNC (LRCLK)

    // Set SAI1 MCLK direction to OUTPUT
    // This is a special register for MCLK direction control.
    IOMUXC_GPR_GPR1 = (IOMUXC_GPR_GPR1 & ~(IOMUXC_GPR_GPR1_SAI1_MCLK1_SEL_MASK | IOMUXC_GPR_GPR1_SAI1_MCLK2_SEL_MASK)) // 1. Clears the SEL field
                  | IOMUXC_GPR_GPR1_SAI1_MCLK_DIR                       // 2. Adds the DIR bit
                  | IOMUXC_GPR_GPR1_SAI1_MCLK1_SEL(0);                   // 3. Adds the new SEL value

    IOMUXC_SAI1_MCLK2_SELECT_INPUT = 1; // Select input from GPIO_B1_02 pad (MCLK pin)
    IOMUXC_SAI1_TX_BCLK_SELECT_INPUT = 2; // Select input from GPIO_B1_02 pad
    IOMUXC_SAI1_TX_SYNC_SELECT_INPUT = 2; // Select input from GPIO_B1_03 pad
    
    // ----------------------------------------------------------------
    // 3. --- Data Lines for Each DAC --- DAC Configuration 
    // ----------------------------------------------------------------
    // 
    // We need to configure the data pin for SAI1 as well. Let's use Pin 22.
    // DAC 1 Data (SAI1) on Pin 7 (Processor Pad: GPIO_B1_01)
    IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_01 = 3; // ALT3 = SAI1_TX_DATA00
    IOMUXC_SW_PAD_CTL_PAD_GPIO_B1_01 = 0x10B0; // Set drive strength and enable pull-up if needed

    // DAC 2 Data (SAI2) on Pin 9 (Processor Pad: GPIO_AD_B0_11)
    IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_11 = 3; // ALT3 = SAI2_TX_DATA02

    // DAC 3 Data (SAI3) on Pin 32 (Processor Pad: GPIO_AD_B0_12)
    IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_12 = 3; // ALT3 = SAI3_TX_DATA01

    // IOMUXC_SAI1_TX_DATA0_SELECT_INPUT = 1; // For Pin 7
    // IOMUXC_SAI1_TX_DATA1_SELECT_INPUT = 1; // For Pin 32
    // IOMUXC_SAI1_TX_DATA2_SELECT_INPUT = 1; // For Pin 9
}

// void configure_sai_for_clock_generation() {
//     // Configure the transmitter for master mode.
//     // BCD=1 (Bit Clock is output), BCE=1 (Bit Clock is enabled)
//     // --- Configure TCR2: Bit Clock Configuration ---
//     // MCLK is the input. BCLK = MCLK / (DIV + 1)
//     // We want to control clock generation ourselves, so we set it to master mode.
//     // Let's target a 3.072 MHz BCLK for 48kHz, 32-bit, stereo.
//     // BCLK = 3.072 MHz MHz. MCLK = 23.134 MHz.
//     // Divisor = (23.134 / 3.072) - 1 = 6.55... so we'll use 7 (DIV=6)
//     // This will result in BCLK = 23.134 / 7 = 3.29 MHz, which is close.
//     I2S1_TCSR = 0;

//     I2S1_TCR2 = I2S_TCR2_BCD 
//                 | I2S_TCR2_BCP
//                 | I2S_TCR2_MSEL(1) // Clock source = PLL (Audio PLL)
//                 | I2S_TCR2_DIV(1); // BCP for clock polarity

//                   // --- Configure TCR3: Word Select (LRCLK) Configuration ---
//     // Disable word flag during config
//     I2S1_TCR3 &= ~I2S_TCR3_TCE; // Disable Transmit Channel Enable to configure
//     I2S1_TCR3 = 0; // Clear it

//     I2S1_TCR4 = I2S_TCR4_FSP |     // Frame sync polarity
//                 I2S_TCR4_FSD |  // Frame sync is output from SAI
//                 I2S_TCR4_SYWD(31) | // Sync width = 32 bits (full frame)
//                 I2S_TCR4_FRSZ(1); // Frame is 2 words long (stereo frame)

//     I2S1_TCR5 = I2S_TCR5_WNW(31) |   // Word N width = 32 bits
//                 I2S_TCR5_W0W(31) |   // Word 0 width = 32 bits
//                 I2S_TCR5_FBT(31);    // First bit transmitted = MSB (bit 31)
    
//     // Enable the Transmitter (TE) and Bit Clock (BCE)
//     I2S1_TCSR = I2S_TCSR_TE | I2S_TCSR_BCE;
// }
