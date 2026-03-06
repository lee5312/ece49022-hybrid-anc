
#include "stm32f0xx.h"
#include <math.h>   // for M_PI
#include <stdint.h>
#include <stdio.h>

#define SAMPLE_RATE 48000 
uint32_t adc_sample = 2048;

void setup_adc(void) {
    // Enable GPIOA clock and set pin for ADC input(PA1-ADC_IN1)
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA->MODER |= 0xC;

    // Enable ADC1 clock
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    // Enable HSI14 (14 MHz) for ADC
    RCC->CR2 |= RCC_CR2_HSI14ON;
    while (!(RCC->CR2 & RCC_CR2_HSI14RDY)); //wait for 14Mhz clock to be ready

    // Enable ADC 
    ADC1->CR |= ADC_CR_ADEN;
    while (!(ADC1->ISR & ADC_ISR_ADRDY)); //wait for ADC to be ready

    // Select channel 1 ADC_IN1 (PA1)
    ADC1->CHSELR = ADC_CHSELR_CHSEL1;
    while (!(ADC1->ISR & ADC_ISR_ADRDY)); //wait for ADC to be ready
}


///////////////////////
// TIM2 for ADC
///////////////////////
void TIM2_IRQHandler() {
    // Acknowledge the interrupt.
    // Start the ADC by turning on the ADSTART bit in the CR.
    // Wait until the EOC bit is set in the ISR.

    TIM2->SR &= ~TIM_SR_UIF;
    ADC1->CR |= ADC_CR_ADSTART;
    while (!(ADC1->ISR & ADC_ISR_EOC));
    adc_sample = ADC1->DR; //store ADC lastest sample
  }


void init_tim2(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    // Timer clock = 48 MHz
    // PSC * ARR / timer_clk = sample period
    // Example: PSC= 9, ARR=99 => 48MHz/(99+10)= 48 kHz
    TIM2->PSC = 10 - 1;
    TIM2->ARR = 100 - 1;

    TIM2->DIER |= TIM_DIER_UIE;         // enable update interrupt
    TIM2->CR1 |= TIM_CR1_CEN;           // enable counter
    NVIC->ISER[0] |= 1 << TIM2_IRQn;    // enable TIM2 IRQ
}


void setup_dac(void) {
    //Enable GPIOA and set PA4 - DAC_OUT1
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA->MODER |= 3 << (2*4); 

    // Enable clock for DAC
    RCC->APB1ENR |= RCC_APB1ENR_DACEN;
    DAC->CR &= ~DAC_CR_TSEL1; //wait for the clock to be ready

    // Enable trigger and enable DAC
    DAC->CR |= DAC_CR_TEN1;
    DAC->CR |= DAC_CR_EN1;
}


///////////////////////
// TIM6 for DAC
///////////////////////
void TIM6_IRQHandler() {

    // clear interrupt flag
    TIM6->SR &= ~TIM_SR_UIF;

    // Testing DAC without ADC samples
    // test += 50;
    // DAC->DHR12R1 = test;

    // output lastest ADC sample
    DAC->DHR12R1 = adc_sample;
  }


void init_tim6(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;
    // Timer clock = 48 MHz
    // PSC * ARR / timer_clk = sample period
    // Example: PSC= 9, ARR=99 => 48MHz/(99+10)= 48 kHz
    TIM6->PSC = 10 - 1;
    TIM6->ARR = 100 - 1;
    
    TIM6->DIER |= TIM_DIER_UIE; // enable update interrupt
    TIM6->CR2 |= TIM_CR2_MMS_1; 
    TIM6->CR1 |= TIM_CR1_CEN;   // enable counter

    NVIC->ISER[0] |= 1 << TIM6_IRQn;  // enable TIM6 IRQ
}

int main(void) {

    setup_adc();
    setup_dac();
    init_tim2();
    init_tim6();

    while(1){
        
    }

}

