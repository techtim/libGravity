/**
 * @file channel.h
 * @brief A single output channel: a euclidean pattern whose gate length, phase
 *        offset and swing are all adjustable. This one generator subsumes the
 *        old "probability" channel - set STEPS = HITS = 1 and every beat fires,
 *        gated only by PROB / DUTY / OFFSET / SWING.
 *
 * There is only one channel behaviour, so there is no func union or dispatch:
 * the six parameters below are plain, named fields addressed through a small
 * enum for the generic UI / CV-routing code.
 *
 * The ISR path (processClockTick) is kept tight: a per-channel phase counter
 * avoids a 32-bit divide, the clock-mod pulse count is cached, and the four
 * gate edge phases are precomputed on every parameter change (finalize()).
 */
#ifndef GRAVITYPLUS_CHANNEL_H
#define GRAVITYPLUS_CHANNEL_H

#include <Arduino.h>

#include "clock_mod.h"
#include "digital_output.h"

static const uint8_t MAX_PATTERN_STEPS = 32; // pattern bitmap is a uint32_t

// The six per-channel parameters, in UI order. Doubles as the index into the
// base_/live_ value arrays.
enum GateParam : uint8_t {
  GATE_STEPS,  // euclidean pattern length         (1..32)
  GATE_HITS,   // active hits spread over the steps (1..steps)
  GATE_PROB,   // per-hit trigger probability       (0..100 %)
  GATE_DUTY,   // gate length as % of the step      (1..99 %)
  GATE_OFFSET, // gate phase offset within the step (0..99 %)
  GATE_SWING,  // swing delay applied to odd steps  (50..95 %)
  GATE_PARAM_COUNT,
};

// CV routing targets. The per-parameter targets are laid out contiguously right
// after CV_CLOCK_MOD so that (target - CV_STEPS) is the GateParam it drives.
enum CvTarget : uint8_t {
  CV_NONE,
  CV_CLOCK_MOD,
  CV_STEPS,
  CV_HITS,
  CV_PROB,
  CV_DUTY,
  CV_OFFSET,
  CV_SWING,
  CV_TARGET_COUNT,
};
static_assert(CV_STEPS + GATE_PARAM_COUNT == CV_TARGET_COUNT,
              "CvTarget param entries must match GateParam");

class Channel {
public:
  Channel() { Init(); }

  void Init() {
    base_[GATE_STEPS] = 1;
    base_[GATE_HITS] = 1;
    base_[GATE_PROB] = 100; // every hit fires by default
    base_[GATE_DUTY] = 50;  // 50% gate
    base_[GATE_OFFSET] = 0; // fire on the step boundary
    base_[GATE_SWING] = 50; // no swing
    base_clock_mod_ = DEFAULT_CLOCK_MOD_INDEX;
    live_clock_mod_ = base_clock_mod_;
    cv1_ = CV_NONE;
    cv2_ = CV_NONE;
    mute_ = false;
    choke_ = 0;
    step_ = 0;
    phase_ = 0;
    beat_ = 0;
    last_tick_ = 0;
    last_mod_ = 0; // 0 != any real mod_pulses -> recompute on the first tick
    syncLive();
    refreshModPulses();
    finalize();
  }

  // --- Parameters (base = persisted, live = CV-modulated for playback) ---
  static uint8_t paramCount() { return GATE_PARAM_COUNT; }

  static const __FlashStringHelper *paramLabel(uint8_t i) {
    switch (i) {
    case GATE_STEPS: return F("STEPS");
    case GATE_HITS: return F("HITS");
    case GATE_PROB: return F("PROB");
    case GATE_DUTY: return F("DUTY");
    case GATE_OFFSET: return F("OFFSET");
    default: return F("SWING");
    }
  }

  int getBase(uint8_t i) const { return base_[i]; }
  int getLive(uint8_t i) const { return live_[i]; }
  // Value to show for param i: the modulated value when a CV drives it (and not
  // editing), otherwise the base value.
  int paramValue(uint8_t i, bool withCvMod) const {
    return (withCvMod && targetsParam(i)) ? live_[i] : base_[i];
  }

  void editParam(uint8_t i, int delta) {
    base_[i] = clampParam(i, base_[i] + delta, base_[GATE_STEPS]);
    if (i == GATE_STEPS && base_[GATE_HITS] > base_[GATE_STEPS])
      base_[GATE_HITS] = base_[GATE_STEPS];
    if (!targetsParam(i))
      live_[i] = base_[i];
    finalize();
  }

  // Bipolar CV maps into these ranges. Hits scales to the current step count.
  void cvRange(uint8_t i, int &lo, int &hi) const {
    switch (i) {
    case GATE_STEPS: lo = 0; hi = MAX_PATTERN_STEPS; break;
    case GATE_HITS: lo = 0; hi = live_[GATE_STEPS]; break;
    case GATE_SWING: lo = -25; hi = 25; break;
    default: lo = -50; hi = 50; break; // PROB / DUTY / OFFSET
    }
  }

  // --- Clock mod ---
  void setClockMod(int index) {
    base_clock_mod_ = constrain(index, 0, MOD_CHOICE_SIZE - 1);
    if (!targetsClockMod()) {
      live_clock_mod_ = base_clock_mod_;
      refreshModPulses();
      finalize();
    }
  }
  int getClockModIndex(bool withCvMod = false) const {
    return (withCvMod && targetsClockMod()) ? live_clock_mod_ : base_clock_mod_;
  }
  int getClockMod(bool withCvMod = false) const {
    return clockModValue(getClockModIndex(withCvMod));
  }

  // --- CV routing ---
  void setCv1Target(CvTarget t) { cv1_ = t; }
  void setCv2Target(CvTarget t) { cv2_ = t; }
  CvTarget getCv1Target() const { return cv1_; }
  CvTarget getCv2Target() const { return cv2_; }
  bool isCvActive() const { return cv1_ != CV_NONE || cv2_ != CV_NONE; }

  // --- Mute ---
  void toggleMute() { mute_ = !mute_; }
  void setMute(bool m) { mute_ = m; }
  bool isMuted() const { return mute_; }

  // --- Choke ---
  // 0 = off, otherwise the 1-based channel number whose gate silences this one
  // (applied in the clock ISR, see HandleIntClockTick).
  void setChoke(uint8_t source) { choke_ = source; }
  uint8_t getChoke() const { return choke_; }

  // --- Pattern view (for the UI) ---
  uint8_t patternSteps() const { return live_[GATE_STEPS]; }
  bool patternHit(uint8_t i) const { return (pattern_ & (1UL << i)) != 0; }

  /**
   * @brief Apply CV modulation. Called from the main loop when a CV is routed.
   * Rebuilds the live clock-mod index and each modulated parameter from base +
   * CV, then recomputes derived state.
   */
  void applyCvMod(int cv1_val, int cv2_val) {
    // Clock mod.
    int mod = 0;
    if (cv1_ == CV_CLOCK_MOD)
      mod += bipolarMod(cv1_val, -(MOD_CHOICE_SIZE / 2), MOD_CHOICE_SIZE / 2);
    if (cv2_ == CV_CLOCK_MOD)
      mod += bipolarMod(cv2_val, -(MOD_CHOICE_SIZE / 2), MOD_CHOICE_SIZE / 2);
    live_clock_mod_ = constrain(base_clock_mod_ + mod, 0, MOD_CHOICE_SIZE - 1);
    refreshModPulses();

    // Parameters: start from base, then add each routed CV contribution. STEPS
    // is resolved first (index 0) so HITS can clamp to the modulated step count.
    syncLive();
    for (uint8_t i = 0; i < GATE_PARAM_COUNT; i++) {
      int lo, hi;
      cvRange(i, lo, hi);
      int amt = 0;
      if (cv1_ == (CvTarget)(CV_STEPS + i))
        amt += bipolarMod(cv1_val, lo, hi);
      if (cv2_ == (CvTarget)(CV_STEPS + i))
        amt += bipolarMod(cv2_val, lo, hi);
      if (amt != 0)
        live_[i] = clampParam(i, base_[i] + amt, live_[GATE_STEPS]);
    }
    finalize();
  }

  /**
   * @brief Process a clock tick. Called from the internal clock ISR - keep tight.
   *
   * Maintains phase_/beat_ incrementally instead of a 32-bit tick % / tick /
   * per call. uClock emits tick == 0 on every start/reset, so that also restarts
   * the pattern at step 0.
   */
  void processClockTick(uint32_t tick, DigitalOutput &output) {
    if (mute_) {
      output.Low();
      return;
    }
    const uint16_t mod_pulses = mod_pulses_; // cached; refreshed off hot path
    if (tick == last_tick_ + 1 && mod_pulses == last_mod_) {
      if (++phase_ >= mod_pulses) {
        phase_ = 0;
        ++beat_;
      }
    } else if (tick == 0) {
      // A discontinuity: external/MIDI resync, a clock-mod change, or a reset.
      // tick == 0 is a start/reset, so restart the pattern too.
      step_ = 0;
      phase_ = 0;
      beat_ = 0;
    } else {
      phase_ = tick % mod_pulses;
      beat_ = tick / mod_pulses;
    }
    last_tick_ = tick;
    last_mod_ = mod_pulses;

    // Swing delays the gate on odd beats/steps.
    const bool swing = (swing_pulses_ > 0) && (beat_ & 1);
    const uint16_t high_phase = swing ? high_phase_sw_ : high_phase_;
    const uint16_t low_phase = swing ? low_phase_sw_ : low_phase_;

    if (!output.On()) {
      if (phase_ == high_phase) {
        const uint8_t prob = live_[GATE_PROB];
        if (nextStep() && (prob >= 100 || prob > (uint8_t)random(0, 100)))
          output.High();
      }
    }
    if (phase_ == low_phase)
      output.Low();
  }

  // --- Persistence (base params + routing) ---
  void save(byte *p) const {
    p[0] = base_clock_mod_;
    p[1] = (byte)cv1_;
    p[2] = (byte)cv2_;
    p[3] = mute_ ? 0x01 : 0x00;
    p[4] = choke_;
    for (uint8_t i = 0; i < GATE_PARAM_COUNT; i++)
      p[5 + i] = base_[i];
  }
  void load(const byte *p) {
    base_clock_mod_ = constrain((int)p[0], 0, MOD_CHOICE_SIZE - 1);
    cv1_ = (CvTarget)constrain((int)p[1], 0, CV_TARGET_COUNT - 1);
    cv2_ = (CvTarget)constrain((int)p[2], 0, CV_TARGET_COUNT - 1);
    mute_ = (p[3] & 0x01) != 0;
    choke_ = p[4];
    // Clamp in STEPS -> HITS order so HITS can bound to the loaded step count.
    for (uint8_t i = 0; i < GATE_PARAM_COUNT; i++)
      base_[i] = clampParam(i, (int)p[5 + i], base_[GATE_STEPS]);
    syncLive();
    live_clock_mod_ = base_clock_mod_;
    refreshModPulses();
    finalize();
  }
  static const uint8_t SAVE_BYTES = 5 + GATE_PARAM_COUNT;

private:
  // Clamp a raw value to param i's range. HITS is bounded by `steps`.
  static int clampParam(uint8_t i, int v, int steps) {
    switch (i) {
    case GATE_STEPS: return constrain(v, 1, MAX_PATTERN_STEPS);
    case GATE_HITS: return constrain(v, 1, steps);
    case GATE_PROB: return constrain(v, 0, 100);
    case GATE_DUTY: return constrain(v, 1, 99);
    case GATE_OFFSET: return constrain(v, 0, 99);
    default: return constrain(v, 50, 95); // SWING
    }
  }

  bool targetsClockMod() const {
    return cv1_ == CV_CLOCK_MOD || cv2_ == CV_CLOCK_MOD;
  }
  bool targetsParam(uint8_t i) const {
    return cv1_ == (CvTarget)(CV_STEPS + i) || cv2_ == (CvTarget)(CV_STEPS + i);
  }

  void syncLive() {
    for (uint8_t i = 0; i < GATE_PARAM_COUNT; i++)
      live_[i] = base_[i];
  }

  // Cache clockModPulses(live_clock_mod_) so the ISR skips a PROGMEM read.
  void refreshModPulses() { mod_pulses_ = clockModPulses(live_clock_mod_); }

  // Advance one euclidean step, returning whether the step we left was a hit.
  bool nextStep() {
    const uint8_t steps = live_[GATE_STEPS];
    const bool hit = (pattern_ & (1UL << step_)) != 0;
    step_ = (step_ < steps - 1) ? step_ + 1 : 0;
    return hit;
  }

  // Recompute the euclidean pattern and the four gate edge phases. Called on
  // every parameter / clock-mod change (never on the hot path).
  void finalize() {
    // Bresenham euclidean pattern into the bitmap (step 0 always a hit).
    const uint8_t steps = live_[GATE_STEPS];
    const uint8_t hits = live_[GATE_HITS];
    pattern_ = 1UL;
    uint8_t bucket = 0;
    for (uint8_t i = 1; i < steps; i++) {
      bucket += hits;
      if (bucket >= steps) {
        bucket -= steps;
        pattern_ |= (1UL << i);
      }
    }
    if (step_ >= steps)
      step_ = 0;

    // Gate edge phases. The gate opens `offset` into the step and closes after
    // `duty` of the step; swing pushes both later on odd steps. Precomputing
    // these makes process() a pair of phase compares (no 32-bit modulo).
    const int32_t mod = mod_pulses_;
    const uint16_t duty_pulses =
        max((int32_t)(mod * (100 - live_[GATE_DUTY]) / 100), (int32_t)1);
    const uint16_t offset_pulses =
        (uint16_t)(mod * (100 - live_[GATE_OFFSET]) / 100);
    const uint8_t swing = live_[GATE_SWING];
    swing_pulses_ =
        (swing > 50) ? (uint16_t)(mod * (100 - (swing - 50)) / 100) : 0;

    const uint16_t m = mod_pulses_;
    high_phase_ = (uint16_t)((m - (offset_pulses % m)) % m);
    high_phase_sw_ = (uint16_t)((m - ((offset_pulses + swing_pulses_) % m)) % m);
    low_phase_ = (uint16_t)((m - ((duty_pulses + offset_pulses) % m)) % m);
    low_phase_sw_ = (uint16_t)(
        (m - ((duty_pulses + offset_pulses + swing_pulses_) % m)) % m);
  }

  // Parameters.
  uint8_t base_[GATE_PARAM_COUNT];
  uint8_t live_[GATE_PARAM_COUNT];
  byte base_clock_mod_;
  byte live_clock_mod_;
  CvTarget cv1_;
  CvTarget cv2_;

  // Playback / pattern state.
  uint32_t pattern_; // euclidean hit bitmap

  // Precomputed gate edge phases ("_sw" = the swung, odd-step variant).
  uint16_t high_phase_, high_phase_sw_;
  uint16_t low_phase_, low_phase_sw_;
  uint16_t swing_pulses_;

  // Per-channel clock-mod phase counter (avoids 32-bit tick % / divide).
  uint16_t phase_;
  uint32_t last_tick_;
  uint16_t beat_;
  uint16_t last_mod_;
  uint16_t mod_pulses_; // cached clockModPulses(live_clock_mod_)

  uint8_t step_;     // current step index

  bool mute_;
  uint8_t choke_; // 0 = off, else 1-based source channel that silences this one
};

#endif // GRAVITYPLUS_CHANNEL_H
