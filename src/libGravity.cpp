/**
 * @file libGravity.cpp
 * @author Adam Wonak (https://github.com/awonak)
 * @brief Library for building custom scripts for the Sitka Instruments Gravity module.
 * @version 0.1
 * @date 2025-04-19
 * 
 * @copyright MIT - (c) 2025 - Adam Wonak - adam.wonak@gmail.com
 *
 */

#include "libGravity.h"

// Initialize the static pointer for the EncoderDir class to null. We want to
// have a static pointer to decouple the ISR from the global gravity object.
Encoder* Encoder::_instance = nullptr;

void Gravity::Init() {
    initClock();
    initInputs();
    initOutputs();
    initDisplay();
}

void Gravity::initClock() {
    clock.Init();
}

void Gravity::initInputs() {
    shift_button.Init(SHIFT_BTN_PIN);
    play_button.Init(PLAY_BTN_PIN);

    cv1.Init(CV1_PIN);
    cv2.Init(CV2_PIN);

    // Pin Change Interrupts for Encoder.
    // Thanks to https://dronebotworkshop.com/interrupts/

    // Enable both PCIE2 Bit3 (Port D), and PCIE1 Bit2 (Port C).
    PCICR |= B00000110;
    // Select PCINT23 Bit4 = 1 (Pin D4)
    PCMSK2 |= B00010000;
    // Select PCINT11 Bit3 (Pin D17/A3)
    PCMSK1 |= B00001000;
}

void Gravity::initOutputs() {
    // Initialize each of the outputs with it's GPIO pins and probability.
    outputs[0].Init(OUT_CH1);
    outputs[1].Init(OUT_CH2);
    outputs[2].Init(OUT_CH3);
    outputs[3].Init(OUT_CH4);
    outputs[4].Init(OUT_CH5);
    outputs[5].Init(OUT_CH6);
    // Expansion Pulse Output
    pulse.Init(PULSE_OUT_PIN);
}
void Gravity::initDisplay() {
    // OLED Display configuration.
    display.begin();
}

void Gravity::Process() {
    // Read peripherials for changes.
    shift_button.Process();
    play_button.Process();
    encoder.Process();
    cv1.Process();
    cv2.Process();

    // Update Output states.
    for (uint8_t i; i < OUTPUT_COUNT; i++) {
        outputs[i].Process();
    }
}

// Pin Change Interrupt on Port D (D4).
ISR(PCINT2_vect) {
    Encoder::isr();
};
// Pin Change Interrupt on Port C (D17/A3).
ISR(PCINT1_vect) {
    Encoder::isr();
};

// Global instance
Gravity gravity;
