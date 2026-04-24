#include "clock_and_pin_config.h"
#include "imxrt.h"
#include "Arduino.h"

void init_audio_clocks_and_pins() {
    // ----------------------------------------------------------------
    // 1. CONFIGURE MASTER CLOCK (MCLK) SOURCE FROM AUDIO PLL
    // ----------------------------------------------------------------

    // Enable clock gates for IOMUXC, CCM, and SAI1
    CCM_CCGR4 |= CCM_CCGR4_IOMUXC(CCM_CCGR_ON) ; 
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
    

    // Configure the SAI1 clock root dividers to generate the desired MCLK frequency.
    // MCLK = (PLL4 Freq / (PRED + 1)) / (PODF + 1)
    // Example for 12.288 MHz MCLK from 393.216 MHz PLL4: (393.216 / 4) / 8 = 12.288
    CCM_CS1CDR = (CCM_CS1CDR & ~(CCM_CS1CDR_SAI1_CLK_PRED_MASK | CCM_CS1CDR_SAI1_CLK_PODF_MASK))
               | CCM_CS1CDR_SAI1_CLK_PRED(3)   // PRED: Divide by 4
               | CCM_CS1CDR_SAI1_CLK_PODF(7);  // PODF: Divide by 8
    
    // ----------------------------------------------------------------
    // 2. CONFIGURE PIN MUXING & DIRECTION
    // ----------------------------------------------------------------

    // MCLK on Pin 23 (GPIO_B1_01)
    // Set MUX control to ALT 3 for SAI1_MCLK
    IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_01 = 3; // SAI1_MCLK

    // BCLK on Pin 21 (GPIO_B0_12)
    // Set MUX control to ALT 3 for SAI1_TX_BCLK
    IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_12 = 3; // SAI1_TX_BCLK

    // LRCLK on Pin 20 (GPIO_B0_13)
    // Set MUX control to ALT 3 for SAI1_TX_SYNC
    IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_13 = 3; // SAI1_TX_SYNC (LRCLK)

    // Set SAI1 MCLK direction to OUTPUT
    // This is a special register for MCLK direction control.
    IOMUXC_GPR_GPR1 |= IOMUXC_GPR_GPR1_SAI1_MCLK_DIR; // 1 = Output, 0 = Input
    
    // ----------------------------------------------------------------
    // 3. SET PIN DRIVE STRENGTH AND SPEED (Optional but good practice)
    // ----------------------------------------------------------------
    // For high-frequency clocks, it's good to set a higher drive strength
    // and slew rate to ensure clean signals.
    
    // Pad control register values (Example: 100MHz speed, 60 Ohm drive strength)
    uint32_t fast_clock_pad_config = IOMUXC_PAD_SPEED(2) | IOMUXC_PAD_DSE(6);

    IOMUXC_SW_PAD_CTL_PAD_GPIO_B1_01 = fast_clock_pad_config; // MCLK
    IOMUXC_SW_PAD_CTL_PAD_GPIO_B0_12 = fast_clock_pad_config; // BCLK
    IOMUXC_SW_PAD_CTL_PAD_GPIO_B0_13 = fast_clock_pad_config; // LRCLK
}
