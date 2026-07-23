/**
 * @file analog_input.h
 * @author Adam Wonak (https://github.com/awonak)
 * @brief Class for interacting with analog inputs.
 * @version 0.1
 * @date 2025-05-23
 *
 * @copyright MIT - (c) 2025 - Adam Wonak - adam.wonak@gmail.com
 *
 */
#ifndef ANALOG_INPUT_H
#define ANALOG_INPUT_H

// Named ADC_MAX_INPUT rather than MAX_INPUT to avoid colliding with the POSIX
// <sys/syslimits.h> MAX_INPUT macro when compiled on a host (e.g. unit tests).
const int ADC_MAX_INPUT = (1 << 10) - 1; // Max 10 bit analog read resolution.

// estimated default calibration value
const int CALIBRATED_LOW = -566;
const int CALIBRATED_HIGH = 512;

class AnalogInput {
public:
  static const int GATE_THRESHOLD = 0;

  AnalogInput() {}
  ~AnalogInput() {}

  /**
   * Initializes a analog input object.
   *
   * @param pin gpio pin for the analog input.
   */
  void Init(uint8_t pin) {
    pinMode(pin, INPUT);
    pin_ = pin;
  }

  /**
   * Read the value of the analog input and set instance state.
   *
   */
  void Process() {
    old_read_ = read_;
    int raw = analogRead(pin_);
    read_ = map(raw, 0, ADC_MAX_INPUT, low_, high_);
    // Cast to long to avoid AVR 16-bit integer overflow prior to constraining
    read_ = constrain((long)read_ - (long)offset_, -512, 512);
    if (inverted_)
      read_ = -read_;
  }

  // Set calibration values.

  void AdjustCalibrationLow(int amount) { low_ += amount; }

  void AdjustCalibrationHigh(int amount) { high_ += amount; }

  void SetCalibrationLow(int low) { low_ = low; }

  void SetCalibrationHigh(int high) { high_ = high; }

  int GetCalibrationLow() const { return low_; }

  int GetCalibrationHigh() const { return high_; }

  void SetOffset(float percent) { offset_ = -(percent) * 512; }

  void AdjustOffset(int amount) { offset_ += amount; }

  int GetOffset() const { return offset_; }

  void SetAttenuation(float percent) {
    low_ = abs(percent) * CALIBRATED_LOW;
    high_ = abs(percent) * CALIBRATED_HIGH;
    inverted_ = percent < 0;
  }

  /**
   * Get the current value of the analog input within a range of +/-512.
   *
   * @return read value within a range of +/-512.
   *
   */
  inline int16_t Read() { return read_; }

  /**
   * Return the analog read value as voltage.
   *
   * @return A float representing the voltage (-5.0 to +5.0).
   *
   */
  inline float Voltage() { return ((read_ / 512.0) * 5.0); }

  /**
   * Checks for a rising edge transition across a threshold.
   *
   * @param threshold The value that the input must cross.
   * @return True if the value just crossed the threshold from below, false
   * otherwise.
   */
  inline bool IsRisingEdge(int16_t threshold) const {
    bool was_high = old_read_ > threshold;
    bool is_high = read_ > threshold;
    return is_high && !was_high;
  }

private:
  uint8_t pin_;
  int16_t read_ = 0;
  // Must be signed and match read_: the reading is bipolar (-512..512), so a
  // uint16_t here would wrap negative values and break IsRisingEdge().
  int16_t old_read_ = 0;
  // calibration values.
  int offset_ = 0;
  int low_ = CALIBRATED_LOW;
  int high_ = CALIBRATED_HIGH;
  bool inverted_ = false;
};

#endif
