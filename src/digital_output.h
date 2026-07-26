/**
 * @file digital_output.h
 * @author Adam Wonak (https://github.com/awonak)
 * @brief Class for interacting with trigger / gate outputs.
 * @version 0.1
 * @date 2025-04-17
 *
 * @copyright MIT - (c) 2025 - Adam Wonak - adam.wonak@gmail.com
 *
 */
#ifndef DIGITAL_OUTPUT_H
#define DIGITAL_OUTPUT_H

#include <Arduino.h>

const byte DEFAULT_TRIGGER_DURATION_MS = 5;

class DigitalOutput {
public:
  /**
   * Initializes an CV Output paired object.
   *
   * @param cv_pin gpio pin for the cv output
   */
  void Init(uint8_t cv_pin) {
    pinMode(cv_pin, OUTPUT); // Gate/Trigger Output
    cv_pin_ = cv_pin;
    trigger_duration_ = DEFAULT_TRIGGER_DURATION_MS;
    on_ = false;
    last_triggered_ = 0;
  }

  /**
   * Set the trigger duration in miliseconds.
   *
   * @param duration_ms trigger duration in miliseconds
   */
  void SetTriggerDuration(uint8_t duration_ms) {
    trigger_duration_ = duration_ms;
  }

  /**
   * Turn the CV and LED on or off according to the input state.
   *
   * @param state Arduino digital HIGH or LOW values.
   */
  inline void Update(uint8_t state) {
    if (state == HIGH)
      High(); // Rising
    if (state == LOW)
      Low(); // Falling
  }

  // Sets the cv output HIGH to about 5v.
  inline void High() { update(HIGH); }

  // Sets the cv output LOW to 0v.
  inline void Low() { update(LOW); }

  /**
   * Deferred write mode. When enabled, High()/Low()/Update() only track the
   * logical state; the physical pin is written by Flush(). This lets a firmware
   * decide all of its outputs first and then write them together, minimizing the
   * skew between channels. The trigger auto-low (Process) is disabled while
   * deferred, since deferred outputs are gate-managed by the caller.
   */
  void SetDeferred(bool deferred) { deferred_ = deferred; }

  // Write the current logical state to the pin (used in deferred mode).
  inline void Flush() { digitalWrite(cv_pin_, on_ ? HIGH : LOW); }

  /**
   * Begin a Trigger period for this output.
   */
  inline void Trigger() {
    update(HIGH);
    last_triggered_ = millis();
  }

  /**
   * Return a bool representing the on/off state of the output.
   */
  inline void Process() {
    // Deferred (gate) outputs manage their own low edge, so skip the trigger
    // auto-low to avoid fighting an externally-driven gate.
    if (deferred_)
      return;
    // If trigger is HIGH and the trigger duration time has elapsed, set the
    // output low.
    if (on_ && (millis() - last_triggered_) >= trigger_duration_) {
      update(LOW);
    }
  }

  /**
   * Return a bool representing the on/off state of the output.
   *
   * @return true if current cv state is high, false if current cv state is low
   */
  inline bool On() { return on_; }

private:
  unsigned long last_triggered_;
  uint8_t trigger_duration_;
  uint8_t cv_pin_;
  bool on_;
  bool deferred_ = false;

  void update(uint8_t state) {
    on_ = state == HIGH;
    if (!deferred_)
      digitalWrite(cv_pin_, state);
  }
};

#endif
