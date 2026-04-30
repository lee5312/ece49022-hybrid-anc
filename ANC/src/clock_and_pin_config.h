#pragma once

// Initializes the core audio clocks (MCLK, BCLK, LRCLK) and configures
// the specific hardware pins for their output functions.
// This must be called before any SAI peripherals are initialized.
void init_audio_clocks_and_pins();
void configure_sai_for_clock_generation();