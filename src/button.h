/**
 * @file button.h
 * @author Adam Wonak (https://github.com/awonak)
 * @brief Wrapper class for interacting with trigger / gate inputs.
 * @version 0.1
 * @date 2025-04-20
  *
 * @copyright MIT - (c) 2025 - Adam Wonak - adam.wonak@gmail.com
 *
 */
#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

const uint8_t DEBOUNCE_MS = 10;
const uint16_t LONG_PRESS_DURATION_MS = 750;

class Button {
   protected:
    typedef void (*CallbackFunction)(void);

   public:
    // Enum constants for active change in button state.
    enum ButtonChange {
        CHANGE_UNCHANGED,
        CHANGE_PRESSED,
        CHANGE_RELEASED,
        CHANGE_RELEASED_LONG,
    };

    Button() {}
    Button(int pin) { Init(pin); }
    ~Button() {}

    /**
     * Initializes a CV Input object.
     *
     * @param pin gpio pin for the cv output.
     */
    void Init(uint8_t pin) {
        pinMode(pin, INPUT_PULLUP);
        pin_ = pin;
        old_read_ = digitalRead(pin_);
    }

    /**
     * Provide a handler function for executing when button is pressed.
     *
     * @param f Callback function to attach push behavior to this button.
     */
    void AttachPressHandler(CallbackFunction f) {
        on_press_ = f;
    }
    /**
     * Provide a handler function for executing when button is pressed.
     *
     * @param f Callback function to attach push behavior to this button.
     */
    void AttachLongPressHandler(CallbackFunction f) {
        on_long_press_ = f;
    }

    /**
     * Read the state of the cv input.
     */
    void Process() {
        int read = digitalRead(pin_);
        bool debounced = (millis() - last_press_) > DEBOUNCE_MS;
        bool pressed = read == 0 && old_read_ == 1 && debounced;
        bool released = read == 1 && old_read_ == 0 && debounced;

        // Determine current clock input state.
        change_ = CHANGE_UNCHANGED;
        if (pressed) {
            change_ = CHANGE_PRESSED;
        } else if (released) {
            // Call appropriate button press handler upon release.
            if (last_press_ + LONG_PRESS_DURATION_MS > millis()) {
                change_ = CHANGE_RELEASED;
                if (on_press_ != NULL) on_press_();
            } else {
                change_ = CHANGE_RELEASED_LONG;
                if (on_long_press_ != NULL) on_long_press_();
            }
        }
        
        // Update variables for next loop
        last_press_ = (pressed || released) ? millis() : last_press_;
        old_read_ = read;
    }

    /**
     * Get the state change for the button.
     *
     * @return ButtonChange
     */
    inline ButtonChange Change() { return change_; }

    /**
     * Current cv state represented as a bool.
     *
     * @return true if cv signal is high, false if cv signal is low
     */
    inline bool On() { return digitalRead(pin_) == 0; }

   private:
    uint8_t pin_;
    uint8_t old_read_ = 1;
    unsigned long last_press_ = 0;
    ButtonChange change_ = CHANGE_UNCHANGED;
    CallbackFunction on_press_ = nullptr;
    CallbackFunction on_long_press_ = nullptr;
};

#endif
