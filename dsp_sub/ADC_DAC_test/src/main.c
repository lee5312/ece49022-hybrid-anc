#include "stm32f0xx.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "lms_filter.h" // Include your LMS filter header
#include "anti_noise_filter.h" // Include your anti-noise filter header

// --- LMS Filter Configuration ---
#define LMS_TAPS 1
#define LMS_MU   0.01f

// --- Global Variables ---
LMSFilter* my_lms_filter;           // Pointer to our filter instance
AntiNoiseFilter* my_anti_noise_filter;   // Pointer to our anti-noise filter instance

uint32_t reference_input = 2048;        // The newest sample from the ADC
uint32_t measured_signal = 2048;        // The newest sample from the ADC
float y = 0;
float z = 0;

uint32_t previous_ref_input = 2048; // For debugging purposes, to track changes in the reference input

void setup_adc(void) {
    // Enable GPIOA clock and set pin for ADC input(PA1-ADC_IN1)
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA->MODER |= 0xC | GPIO_MODER_MODER5; // PA1-ADC_IN1, PA5 for debugging output
    // Enable ADC1 clock
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    // Enable HSI14 (14 MHz) for ADC
    RCC->CR2 |= RCC_CR2_HSI14ON;
    while (!(RCC->CR2 & RCC_CR2_HSI14RDY)); //wait for 14Mhz clock to be ready
    // Enable ADC 
    ADC1->CR |= ADC_CR_ADEN;
    while (!(ADC1->ISR & ADC_ISR_ADRDY)); //wait for ADC to be ready
    // Select channel 1 ADC_IN1 (PA1)
    // ADC1->CHSELR = ADC_CHSELR_CHSEL1;
    // while (!(ADC1->ISR & ADC_ISR_ADRDY)); //wait for ADC to be ready
}

///////////////////////
// TIM2 for ADC
///////////////////////
void TIM2_IRQHandler() {
    // 1. Acknowledge the interrupt
    TIM2->SR &= ~TIM_SR_UIF;

    // 2. Acquire the ADC sample (your blocking method)
    ADC1->CHSELR = ADC_CHSELR_CHSEL1; // Select channel 1
    ADC1->CR |= ADC_CR_ADSTART;
    while (!(ADC1->ISR & ADC_ISR_EOC));
    reference_input = ADC1->DR;

    // --- Read Channel 5 (PA5) ---
    ADC1->CHSELR = ADC_CHSELR_CHSEL5; // Select channel 5
    ADC1->CR |= ADC_CR_ADSTART;      // Start conversion
    while (!(ADC1->ISR & ADC_ISR_EOC)); // Wait for it to finish
    measured_signal = ADC1->DR; // Read the result

    // 3. --- NORMALIZE THE ADC VALUES (THE FIX) ---
    // Convert the 0-4095 integer range to a -1.0 to +1.0 float range.
    // 2047.5f is the midpoint (half of 4095).
    const float v_norm = 0.2f;

    float x_norm = ((float)reference_input - 2047.5f) / 2047.5f;

    // 3b. Create the simulated measured signal 'x + v'.
    //    This will be the 'measured_signal' for the filter.
    float measured_signal_norm = ((float)measured_signal - 2047.5f) / 2047.5f;
    // y = measured_signal_norm; // For debugging/monitoring purposes

    // 4. --- LMS FILTER PROCESSING ---
    // Run the filter using the SMALL, NORMALIZED floating-point values.
    // lms_filter_process(my_lms_filter, x_norm, measured_signal_norm);

    // 5. Get the normalized anti-noise output from the filter
    // This value will be in the approximate range of -1.0 to +1.0.
    // float anti_noise_norm = my_lms_filter->anti_noise;
    // float anti_noise_90_norm = my_lms_filter->anti_noise_90;
    // float e_norm = my_lms_filter->e;

    float anti_noise_norm = anti_noise_filter_process(my_anti_noise_filter, x_norm, measured_signal_norm);

    z = anti_noise_norm; // For debugging/monitoring purposes

    // 6. --- DENORMALIZE THE OUTPUT FOR THE DAC (THE FIX) ---
    // Convert the -1.0 to +1.0 float range back to the 0-4095 DAC range.
    int32_t dac_output = (int32_t)(anti_noise_norm * 2047.5f + 2047.5f);

    // 7. Clamp/saturate the output to the valid 12-bit range as a safety measure
    if (dac_output > 4095) dac_output = 4095;
    if (dac_output < 0) dac_output = 0;
    
    int32_t f = measured_signal; // For debugging/monitoring purposes
    // 8. Load the final integer result into the DAC's holding register
    DAC->DHR12R1 = (uint32_t)dac_output;
    // DAC->DHR12R1 = reference_input; // For testing, output the reference input directly to the DAC
    // nano_wait(100000); // Short delay to ensure DAC has time to update (optional, depends on your timing requirements)
    previous_ref_input = reference_input; // Update previous input for the next iteration
}

void init_tim2(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    // Your timer settings that result in ~41kHz effective rate
    TIM2->PSC = 2 - 1;
    TIM2->ARR = 100 - 1;
    TIM2->DIER |= TIM_DIER_UIE;        // enable update interrupt
    TIM2->CR1 |= TIM_CR1_CEN;          // enable counter
    NVIC->ISER[0] |= 1 << TIM2_IRQn;   // enable TIM2 IRQ
}

void setup_dac(void) {
    //Enable GPIOA and set PA4 - DAC_OUT1
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA->MODER |= 3 << (2*4); 
    // Enable clock for DAC
    RCC->APB1ENR |= RCC_APB1ENR_DACEN;
    // Set for HW trigger from TIM6
    DAC->CR &= ~DAC_CR_TSEL1;
    DAC->CR |= DAC_CR_TEN1;  // Enable trigger
    DAC->CR |= DAC_CR_EN1;   // Enable DAC
}

///////////////////////
// TIM6 for DAC
///////////////////////
void init_tim6(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;
    // Match the timer settings to the ADC timer for synchronization
    TIM6->PSC = 2 - 1;
    TIM6->ARR = 100 - 1;
    
    // Configure TIM6 to generate a trigger on its update event
    TIM6->CR2 &= ~TIM_CR2_MMS;  // Clear the MMS bits first
    TIM6->CR2 |= TIM_CR2_MMS_1; // Set MMS to 010: Update Event
    
    TIM6->CR1 |= TIM_CR1_CEN;   // Enable counter
}

int main(void) {
    // --- Create and Initialize the LMS Filter ---
    // my_lms_filter = lms_filter_create(LMS_TAPS, LMS_MU);
    my_anti_noise_filter = anti_noise_filter_create(5 + 1); // +1 for the delay line
    anti_noise_filter_set_delay(my_anti_noise_filter, 2); // Set initial delay to 2 samples
    
    // Halt if memory allocation fails
    // if (my_lms_filter == NULL) {
    //     while(1);
    // }
    // Halt if memory allocation fails
    if (my_anti_noise_filter == NULL) {
        while(1);
    }
    
    // --- Initialize Hardware ---
    setup_adc();
    setup_dac();
    init_tim2();
    init_tim6();

    // --- Main Loop ---
    // The program will now run entirely within the TIM2 interrupt.
    while(1){
        // This space is free for low-priority background tasks.
    }
}
