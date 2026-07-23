/**
 * @file encoder_dir.h
 * @author Adam Wonak (https://github.com/awonak)
 * @brief Class for interacting with encoders.
 * @version 0.1
 * @date 2025-04-19
 * 
 * @copyright MIT - (c) 2025 - Adam Wonak - adam.wonak@gmail.com
 *
 */
#ifndef ENCODER_DIR_H
#define ENCODER_DIR_H

#include <RotaryEncoder.h>

#include "button.h"
#include "peripherials.h"

class Encoder {
   protected:
    typedef void (*CallbackFunction)(void);
    typedef void (*RotateCallbackFunction)(int val);
    CallbackFunction on_press;
    CallbackFunction on_long_press;
    RotateCallbackFunction on_press_rotate;
    RotateCallbackFunction on_rotate;
    int change;

   public:
    Encoder() : encoder_(ENCODER_PIN1, ENCODER_PIN2, RotaryEncoder::LatchMode::FOUR3),
                   button_(ENCODER_SW_PIN) {
        _instance = this;
    }
    ~Encoder() {}

    // Set to true if the encoder read direction should be reversed.
    void SetReverseDirection(bool reversed) {
        reversed_ = reversed;
    }
    void AttachPressHandler(CallbackFunction f) {
        on_press = f;
    }

    void AttachLongPressHandler(CallbackFunction f) {
        on_long_press = f;
    }

    void AttachRotateHandler(RotateCallbackFunction f) {
        on_rotate = f;
    }

    void AttachPressRotateHandler(RotateCallbackFunction f) {
        on_press_rotate = f;
    }

    void Process() {
        // Get encoder position change amount.
        int encoder_rotated = _rotate_change() != 0;
        bool button_pressed = button_.On();
        button_.Process();

        // Handle encoder position change and button press.
        if (button_pressed && encoder_rotated) {
            rotated_while_held_ = true;
            if (on_press_rotate != NULL) on_press_rotate(change);
        } else if (!button_pressed && encoder_rotated) {
            if (on_rotate != NULL) on_rotate(change);
        } else if (button_.Change() == Button::CHANGE_RELEASED && !rotated_while_held_) {
            if (on_press != NULL) on_press();
        } else if (button_.Change() == Button::CHANGE_RELEASED_LONG && !rotated_while_held_) {
            if (on_long_press != NULL) on_long_press();
        }

        // Reset rotate while held state.
        if (button_.Change() == Button::CHANGE_RELEASED && rotated_while_held_) {
            rotated_while_held_ = false;
        }
    }

    static void isr() {
        // If the instance has been created, call its tick() method.
        if (_instance) {
            _instance->encoder_.tick();
        }
    }

   private:
    static Encoder* _instance;

    int previous_pos_;
    bool rotated_while_held_;
    bool reversed_ = false;
    RotaryEncoder encoder_;
    Button button_;

    // Return the number of ticks change since last polled.
    int _rotate_change() {
        int position = encoder_.getPosition();
        unsigned long ms = encoder_.getMillisBetweenRotations();

        // Validation (TODO: add debounce check).
        if (previous_pos_ == position) {
            return 0;
        }

        // Update state variables.
        change = position - previous_pos_;
        previous_pos_ = position;

        // Encoder rotate acceleration.
        if (ms < 16) {
            change *= 3;
        } else if (ms < 32) {
            change *= 2;
        }

        if (reversed_) {
            change = -(change);
        }
        return change;
    }
};

#endif