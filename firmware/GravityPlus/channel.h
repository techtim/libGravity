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

static constexpr uint8_t MAX_PATTERN_STEPS = 16; // pattern bitmap fits a uint16_t

// One enum for every channel-page item, in UI order: clock mod, the seven
// pattern/gate params (STEPS..SWING, the ones stored per channel in base_/live_),
// the choke source, then the two CV routing targets. Used as the base_/live_
// array index and the UI / CV item index alike.
enum ChannelPageParam : uint8_t {
  CP_CLOCK_MOD,
  CP_STEPS,  // euclidean pattern length         (1..16)
  CP_HITS,   // active hits spread over the steps (1..steps)
  CP_ROTATE, // cyclic shift of the pattern       (0..steps-1)
  CP_PROB,   // per-hit trigger probability       (0..100 %)
  CP_DUTY,   // gate length as % of the step      (1..99 %)
  CP_OFFSET, // gate phase offset within the step (0..99 %)
  CP_SWING,  // swing delay applied to odd steps  (50..95 %)
  CP_CHOKE,
  CP_CV1A,
  CP_CV1B,
  CP_CV2A,
  CP_CV2B,
  CP_PARAM_COUNT,
};

// The stored gate params are the contiguous block CP_STEPS..CP_SWING.
static constexpr uint8_t CP_MOD_FIRST = CP_STEPS;
static constexpr uint8_t CP_MOD_LAST = CP_SWING;
static constexpr uint8_t CP_MOD_COUNT = CP_MOD_LAST - CP_MOD_FIRST + 1;

// Four CV modulation slots per channel: CV1-A, CV1-B (both read CV1) and
// CV2-A, CV2-B (both read CV2). Each has a destination + signed amount.
static constexpr uint8_t CVMOD_SLOTS = 4;

// CV routing targets. CV_STEPS..CV_SWING align 1:1 with CP_STEPS..CP_SWING, so
// the target for gate param cp is CV_STEPS + (cp - GATE_FIRST).
enum CvTarget : uint8_t {
  CV_NONE,
  CV_CLOCK_MOD,
  CV_STEPS,
  CV_HITS,
  CV_ROTATE,
  CV_PROB,
  CV_DUTY,
  CV_OFFSET,
  CV_SWING,
  CV_TARGET_COUNT,
};
static_assert(CV_STEPS + CP_MOD_COUNT == CV_TARGET_COUNT,
              "CvTarget param entries must match the gate params");
// Channel::targetsParam() maps a ChannelPageParam to its CvTarget with cp + 1.
static_assert(CP_CLOCK_MOD + 1 == CV_CLOCK_MOD && CP_STEPS + 1 == CV_STEPS &&
                  CP_SWING + 1 == CV_SWING,
              "CvTarget must be ChannelPageParam shifted by one (CV_NONE)");

// A channel-page item that is a stored gate param (STEPS..SWING). These index
// base_/live_ directly (see ChannelPageParam in channel.h).
inline bool pageParamIsGate(uint8_t page_param) {
  return page_param >= CP_STEPS && page_param <= CP_SWING;
}

class Channel {
public:
  Channel() { Init(); }

  void Init() {
    base_[CP_STEPS] = 1;
    base_[CP_HITS] = 1;
    base_[CP_PROB] = 100; // every hit fires by default
    base_[CP_DUTY] = 50;  // 50% gate
    base_[CP_OFFSET] = 0; // fire on the step boundary
    base_[CP_SWING] = 50; // no swing
    base_[CP_ROTATE] = 0; // no pattern rotation
    base_clock_mod_ = DEFAULT_CLOCK_MOD_INDEX;
    live_clock_mod_ = base_clock_mod_;
    for (uint8_t s = 0; s < CVMOD_SLOTS; s++) {
      cvdest_[s] = CV_NONE;
      cvamt_[s] = 100; // full depth once a destination is picked
    }
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

  // Value to show for param i: the modulated value when a CV drives it (and not editing), otherwise the base value.
  uint8_t paramValue(uint8_t i, bool withCvMod) const {
    return (withCvMod && targetsParam((ChannelPageParam)i)) ? live_[i] : base_[i];
  }

  void editParam(uint8_t i, int delta) {
    base_[i] = clampParam(i, base_[i] + delta, base_[CP_STEPS]);
    if (i == CP_STEPS) {
      // STEPS also bounds HITS and ROTATE - clamp them and sync their live
      // copies too, else a shrink-then-grow leaves live_ stale (e.g. HITS).
      base_[CP_HITS] = base_[CP_HITS] > base_[CP_STEPS] ? base_[CP_STEPS] : base_[CP_HITS];
      base_[CP_ROTATE] = base_[CP_ROTATE] > base_[CP_STEPS] - 1 ? base_[CP_STEPS] - 1 : base_[CP_ROTATE];
      if (!targetsParam(CP_HITS))
        live_[CP_HITS] = base_[CP_HITS];
      if (!targetsParam(CP_ROTATE))
        live_[CP_ROTATE] = base_[CP_ROTATE];
    }
    if (!targetsParam((ChannelPageParam)i))
      live_[i] = base_[i];
    finalize();
  }

  // --- Clock mod ---
  void setClockMod(int index) {
    base_clock_mod_ = constrain(index, 0, MOD_CHOICE_SIZE - 1);
    if (!targetsParam(CP_CLOCK_MOD)) {
      live_clock_mod_ = base_clock_mod_;
      refreshModPulses();
      finalize();
    }
  }
  int getClockModIndex(bool withCvMod = false) const {
    return (withCvMod && targetsParam(CP_CLOCK_MOD)) ? live_clock_mod_ : base_clock_mod_;
  }
  int getClockMod(bool withCvMod = false) const {
    return clockModValue(getClockModIndex(withCvMod));
  }

  // --- CV routing: 4 slots (CV1-A, CV1-B, CV2-A, CV2-B). Slots 0/1 read CV1,
  // 2/3 read CV2. Each has a destination param + signed amount (< 0 inverts). ---
  void setCvDest(uint8_t s, CvTarget t) { cvdest_[s] = t; }
  CvTarget getCvDest(uint8_t s) const { return cvdest_[s]; }
  void setCvAmount(uint8_t s, int a) { cvamt_[s] = (int8_t)constrain(a, -100, 100); }
  int8_t getCvAmount(uint8_t s) const { return cvamt_[s]; }
  bool isCvActive() const {
    for (uint8_t s = 0; s < CVMOD_SLOTS; s++)
      if (cvdest_[s] != CV_NONE)
        return true;
    return false;
  }

  // --- Mute ---
  void toggleMute() { mute_ = !mute_; }
  void setMute(bool m) { mute_ = m; }
  bool isMuted() const { return mute_; }

  // --- Choke ---
  // 0 = off, otherwise the 1-based channel number whose gate silences this one
  // (applied in the clock ISR, see HandleIntClockTick).
  void setChoke(uint8_t source) { choke_ = source; }
  uint8_t getChoke() const { return choke_; }

  uint8_t patternSteps() const { return live_[CP_STEPS]; }
  bool patternHit(uint8_t i) const { return (pattern_ & (1U << i)) != 0; }

  /**
   * @brief Apply CV modulation. Called from the main loop when a CV is routed.
   * Rebuilds the live clock-mod index and each modulated parameter from base +
   * CV, then recomputes derived state.
   */
  // cv1_val/cv2_val are the reduced CV readings (-127..127, i.e. Read() >> 2).
  // A slot's contribution is reading * amount / 128 (amount = offset at full CV;
  // negative inverts). Kept to int8 so reading * amount fits a 16-bit int.
  void applyCvMod(int8_t cv1_val, int8_t cv2_val) {
    // Per-slot input: slots 0/1 = CV1, slots 2/3 = CV2.
    const int8_t in[CVMOD_SLOTS] = {cv1_val, cv1_val, cv2_val, cv2_val};

    // Clock mod. Scaled to +-(MOD_CHOICE_SIZE/2) index steps, not the +-100 used
    // for params (the index range is only ~25 wide).
    int mod = 0;
    for (uint8_t s = 0; s < CVMOD_SLOTS; s++) {
      if (cvdest_[s] == CV_CLOCK_MOD) {
        mod += in[s] * cvamt_[s] / 128 * (MOD_CHOICE_SIZE / 2) / 100;
      }
    }

    live_clock_mod_ = constrain(base_clock_mod_ + mod, 0, MOD_CHOICE_SIZE - 1);
    refreshModPulses();

    // Parameters: start from base, then add each routed slot. STEPS is resolved
    // first so HITS can clamp to the modulated step count.
    syncLive();
    for (uint8_t i = CP_MOD_FIRST; i <= CP_MOD_LAST; ++i) {
      int amt = 0;
      for (uint8_t s = 0; s < CVMOD_SLOTS; ++s) {
        if (cvdest_[s] == (CvTarget)(i + 1)) { // to map CP to CV skipping CV_NONE == 0
          amt += (cvdest_[s] == CV_STEPS || cvdest_[s] == CV_HITS || cvdest_[s] == CV_ROTATE)
              ? (static_cast<int>(in[s] >> 3) * cvamt_[s]) / 100 // 100% * (127 >> 3 == 15) 
              : (static_cast<int>(in[s]) * cvamt_[s]) / 128;
        }
      }
      if (amt != 0) {
        live_[i] = clampParam(i, base_[i] + amt, live_[CP_STEPS]);
      }
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
        if (nextStep() && (live_[CP_PROB] >= 100 || live_[CP_PROB] > (uint8_t)random(0, 100)))
          output.High();
      }
    }
    if (phase_ == low_phase)
      output.Low();
  }

  // --- Persistence (base params + routing) ---
  // Byte layout: [0]=clock mod, [1]=mute, [2]=choke, [3..3+N)=CV dests,
  // [3+N..3+2N)=CV amounts, then the gate params. N = CVMOD_SLOTS.
  static const uint8_t CVMOD_BASE = 3;
  static const uint8_t GATE_BASE = CVMOD_BASE + 2 * CVMOD_SLOTS;
  void save(byte *p) const {
    p[0] = base_clock_mod_;
    p[1] = mute_ ? 0x01 : 0x00;
    p[2] = choke_;
    for (uint8_t s = 0; s < CVMOD_SLOTS; s++) {
      p[CVMOD_BASE + s] = (byte)cvdest_[s];
      p[CVMOD_BASE + CVMOD_SLOTS + s] = (byte)cvamt_[s];
    }
    for (uint8_t i = CP_MOD_FIRST; i <= CP_MOD_LAST; i++)
      p[GATE_BASE + (i - CP_MOD_FIRST)] = base_[i];
  }

  void load(const byte *p) {
    base_clock_mod_ = constrain((int)p[0], 0, MOD_CHOICE_SIZE - 1);
    mute_ = (p[1] & 0x01) != 0;
    choke_ = p[2];
    for (uint8_t s = 0; s < CVMOD_SLOTS; s++) {
      cvdest_[s] = (CvTarget)constrain((int)p[CVMOD_BASE + s], 0, CV_TARGET_COUNT - 1);
      cvamt_[s] = (int8_t)constrain((int)(int8_t)p[CVMOD_BASE + CVMOD_SLOTS + s], -100, 100);
    }
    // Clamp in STEPS -> HITS order so HITS can bound to the loaded step count.
    for (uint8_t i = CP_MOD_FIRST; i <= CP_MOD_LAST; i++)
      base_[i] = clampParam(i, (int)p[GATE_BASE + (i - CP_MOD_FIRST)], base_[CP_STEPS]);
    syncLive();
    live_clock_mod_ = base_clock_mod_;
    refreshModPulses();
    finalize();
  }
  static const uint8_t SAVE_BYTES = GATE_BASE + CP_MOD_COUNT;

private:
  // Phase at which a gate edge with the given pulse `shift` occurs, i.e.
  // (mod - shift % mod) % mod, reduced by subtraction instead of a 32-bit
  // modulo. The final wrap to 0 is essential: when shift is a whole number of
  // steps the edge belongs on phase 0, and `mod` itself is never reached (phase
  // only runs 0..mod-1), which would leave the gate stuck. duty/offset/swing
  // pulses are each <= mod, so the loop runs at most 3 times.
  static uint16_t edgePhase(uint32_t shift, uint16_t mod) {
    while (shift >= mod)
      shift -= mod;
    return shift ? (uint16_t)(mod - shift) : 0;
  }

  // Clamp a raw value to param i's range. HITS is bounded by `steps`.
  static int clampParam(uint8_t i, int v, int8_t steps) {
    switch (i) {
    case CP_STEPS: return constrain(v, 1, MAX_PATTERN_STEPS);
    case CP_HITS: return constrain(v, 1, steps);
    case CP_PROB: return constrain(v, 0, 100);
    case CP_DUTY: return constrain(v, 1, 99);
    case CP_OFFSET: return constrain(v, 0, 99);
    case CP_ROTATE: return steps > 1 ? constrain(v, 0, steps - 1) : 0;
    default: return constrain(v, 50, 95); // SWING
    }
  }

  bool targetsParam(ChannelPageParam cp) const {
    for (uint8_t s = 0; s < CVMOD_SLOTS; ++s)
      if (cvdest_[s] == (CvTarget)(cp + 1))
        return true;
    return false;
  }

  void syncLive() {
    for (uint8_t i = CP_MOD_FIRST; i <= CP_MOD_LAST; i++)
      live_[i] = base_[i];
  }

  // Cache clockModPulses(live_clock_mod_) so the ISR skips a PROGMEM read.
  void refreshModPulses() { mod_pulses_ = clockModPulses(live_clock_mod_); }

  // Advance one euclidean step, returning whether the step we left was a hit.
  bool nextStep() {
    const uint8_t steps = live_[CP_STEPS];
    const bool hit = (pattern_ & (1U << step_)) != 0;
    step_ = (step_ < steps - 1) ? step_ + 1 : 0;
    return hit;
  }

  // Recompute the euclidean pattern and the four gate edge phases. Called on
  // every parameter / clock-mod change (never on the hot path).
  void finalize() {
    // Bresenham euclidean pattern into the bitmap (step 0 always a hit).
    const uint8_t steps = live_[CP_STEPS];
    pattern_ = 1U;
    uint8_t bucket = 0;
    for (uint8_t i = 1; i < steps; i++) {
      bucket += live_[CP_HITS];
      if (bucket >= steps) {
        bucket -= steps;
        pattern_ |= (1U << i);
      }
    }
    // Cyclic rotation: a hit at base index j moves to (j + rotate) % steps.
    if (live_[CP_ROTATE]) {
      uint16_t rotated = 0;
      for (uint8_t i = 0; i < steps; i++)
        if (pattern_ & (1U << ((i + steps - live_[CP_ROTATE]) % steps)))
          rotated |= (1U << i);
      pattern_ = rotated;
    }

    // Gate edge phases. The gate opens `offset` into the step and closes after swing pushes both later on odd steps.
    // Precomputing these makes process() a pair of phase compares (no 32-bit modulo).
    const uint16_t duty_pulses = max(static_cast<int32_t>(mod_pulses_) * (100 - live_[CP_DUTY]) / 100, 1);
    const uint16_t offset_pulses = static_cast<int32_t>(mod_pulses_) * (100 - live_[CP_OFFSET]) / 100;
    swing_pulses_ =
        (live_[CP_SWING] > 50) ? static_cast<int32_t>(mod_pulses_) * (100 - (live_[CP_SWING] - 50)) / 100 : 0;

    high_phase_ = edgePhase(offset_pulses, mod_pulses_);
    high_phase_sw_ = edgePhase((uint32_t)offset_pulses + swing_pulses_, mod_pulses_);
    low_phase_ = edgePhase((uint32_t)duty_pulses + offset_pulses, mod_pulses_);
    low_phase_sw_ = edgePhase((uint32_t)duty_pulses + offset_pulses + swing_pulses_, mod_pulses_);
  }

  // Parameters (indexed by ChannelPageParam; only the gate block is used).
  uint8_t base_[CP_MOD_LAST + 1];
  uint8_t live_[CP_MOD_LAST + 1];
  byte base_clock_mod_;
  byte live_clock_mod_;
  CvTarget cvdest_[CVMOD_SLOTS];
  int8_t cvamt_[CVMOD_SLOTS];

  // Playback / pattern state.
  uint16_t pattern_; // euclidean hit bitmap (<= 16 steps)

  // Precomputed gate edge phases ("_sw" = the swung, odd-step variant).
  uint16_t high_phase_, high_phase_sw_;
  uint16_t low_phase_, low_phase_sw_;
  uint16_t swing_pulses_;

  // Per-channel clock-mod phase counter (avoids 32-bit tick % / divide).
  uint32_t last_tick_;
  uint16_t phase_;
  uint16_t beat_;
  uint16_t last_mod_;
  uint16_t mod_pulses_; // cached clockModPulses(live_clock_mod_)

  uint8_t step_;     // current step index
  uint8_t choke_; // 0 = off, else 1-based source channel that silences this one
  bool mute_;
};

#endif // GRAVITYPLUS_CHANNEL_H
