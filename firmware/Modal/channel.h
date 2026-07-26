/**
 * @file channel.h
 * @brief Unified channel: shared clock-mod + CV routing + gate timing, with a
 *        per-channel func selected at runtime from a tagged union.
 *
 * The func-specific behavior is dispatched with a switch generated from
 * FUNC_LIST (funcs.h) - no virtual calls, so the ISR path is a single jump into
 * inlined func code.
 */
#ifndef MODAL_CHANNEL_H
#define MODAL_CHANNEL_H

#include <Arduino.h>

#include "clock_mod.h"
#include "digital_output.h"
#include "funcs.h"

class Channel {
public:
  Channel() { Init(); }

  void Init() {
    func_ = FUNC_PROBABILITY;
    base_clock_mod_index_ = DEFAULT_CLOCK_MOD_INDEX;
    cvmod_clock_mod_index_ = base_clock_mod_index_;
    cv1_target_ = CV_NONE;
    cv2_target_ = CV_NONE;
    mute_ = false;
    phase_ = 0;
    beat_ = 0;
    last_tick_ = 0;
    last_mod_ = 0; // 0 != any real mod_pulses -> recompute on the first tick
    _refreshModPulses();
    _funcReset();
    _funcFinalize(mod_pulses_);
  }

  // --- Func selection (runtime) ---
  ChannelFunc getFunc() const { return func_; }
  void setFunc(ChannelFunc m) {
    if (m >= FUNC_LAST || m == func_)
      return;
    func_ = m;
    // Param semantics change with the func, so drop any CV routing to avoid a
    // stale target pointing at a now-nonexistent parameter.
    cv1_target_ = CV_NONE;
    cv2_target_ = CV_NONE;
    _funcReset();
    _funcFinalize(mod_pulses_);
  }

  const __FlashStringHelper *funcName(bool full = false) const {
    switch (func_) {
#define X(E, M, T) case E: return state_.M.funcName(full);
      FUNC_LIST(X)
#undef X
    default: return F("");
    }
  }

  // --- Clock mod (shared across funcs) ---
  void setClockMod(int index) {
    base_clock_mod_index_ = constrain(index, 0, MOD_CHOICE_SIZE - 1);
    if (!_targetsClockMod()) {
      cvmod_clock_mod_index_ = base_clock_mod_index_;
      _refreshModPulses();
      _funcFinalize(mod_pulses_);
    }
  }
  int getClockModIndex(bool withCvMod = false) const {
    return (withCvMod && _targetsClockMod()) ? cvmod_clock_mod_index_
                                             : base_clock_mod_index_;
  }
  int getClockMod(bool withCvMod = false) const {
    return clockModValue(getClockModIndex(withCvMod));
  }

  // --- CV routing ---
  void setCv1Target(CvTarget t) { cv1_target_ = t; }
  void setCv2Target(CvTarget t) { cv2_target_ = t; }
  CvTarget getCv1Target() const { return cv1_target_; }
  CvTarget getCv2Target() const { return cv2_target_; }
  bool isCvActive() const { return cv1_target_ != CV_NONE || cv2_target_ != CV_NONE; }

  // --- Func parameters (for the UI) ---
  uint8_t paramCount() const {
    switch (func_) {
#define X(E, M, T) case E: return state_.M.paramCount();
      FUNC_LIST(X)
#undef X
    default: return 0;
    }
  }
  const __FlashStringHelper *paramLabel(uint8_t i) const {
    switch (func_) {
#define X(E, M, T) case E: return state_.M.paramLabel(i);
      FUNC_LIST(X)
#undef X
    default: return F("");
    }
  }
  // Value to show for param i: param when a CV is routed to it (and not
  // editing), otherwise the base value.
  int paramValue(uint8_t i, bool withCvMod) const {
    return (withCvMod && _targetsParam(i)) ? _funcGetParam(i)
                                           : _funcGetBase(i);
  }
  void editParam(uint8_t i, int delta) {
    _funcSetBase(i, _funcGetBase(i) + delta);
    if (!_targetsParam(i))
      _funcSetParam(i, _funcGetBase(i));
    _funcFinalize(mod_pulses_);
  }

  void toggleMute() { mute_ = !mute_; }
  void setMute(bool m) { mute_ = m; }
  bool isMuted() const { return mute_; }

  /**
   * @brief Apply CV modulation. Called from the main loop when any CV is routed.
   * Recomputes the clock-mod index and each modulated func parameter, then lets
   * the func precompute its derived state.
   */
  void applyCvMod(int cv1_val, int cv2_val) {
    // Clock mod.
    int mod = 0;
    if (cv1_target_ == CV_CLOCK_MOD)
      mod += bipolarMod(cv1_val, -(MOD_CHOICE_SIZE / 2), MOD_CHOICE_SIZE / 2);
    if (cv2_target_ == CV_CLOCK_MOD)
      mod += bipolarMod(cv2_val, -(MOD_CHOICE_SIZE / 2), MOD_CHOICE_SIZE / 2);
    cvmod_clock_mod_index_ =
        constrain(base_clock_mod_index_ + mod, 0, MOD_CHOICE_SIZE - 1);
    _refreshModPulses();

    // Func params: start from base, then add each routed CV contribution.
    _funcSyncParam();
    uint8_t n = paramCount();
    for (uint8_t i = 0; i < n; i++) {
      int lo, hi;
      _funcCvRange(i, lo, hi);
      int amt = 0;
      if (cv1_target_ == CV_PARAM_0 + i)
        amt += bipolarMod(cv1_val, lo, hi);
      if (cv2_target_ == CV_PARAM_0 + i)
        amt += bipolarMod(cv2_val, lo, hi);
      if (amt != 0)
        _funcSetParam(i, _funcGetBase(i) + amt);
    }
    _funcFinalize(mod_pulses_);
  }

  /**
   * @brief Process a clock tick. Called from the internal clock ISR - keep tight.
   *
   * Maintains phase_/beat_ incrementally instead of doing a 32-bit
   * tick % mod_pulses / tick / mod_pulses per call. Alignment with the master
   * clock is preserved without extra reset plumbing: uClock emits tick == 0 on
   * every start/reset, which re-zeros the counter, so phase_ == tick % mod_pulses
   * and beat_ == tick / mod_pulses hold exactly (for a constant mod_pulses).
   */
  void processClockTick(uint32_t tick, DigitalOutput &output) {
    if (mute_) {
      output.Low();
      return;
    }
    const uint16_t mod_pulses = mod_pulses_; // cached; refreshed off the hot path
    // Compute phase = tick % mod_pulses and beat = tick / mod_pulses without a
    // 32-bit divide on the hot path. The INTERNAL clock advances the tick by
    // exactly 1 each call, so we step the cached counter. EXTERNAL / MIDI clocks
    // jump the tick to resync (and a reset restarts it), and the clock-mod can
    // change, so on any discontinuity we recompute from the tick. This is
    // exactly equivalent to tick % / tick / , just cheaper while free-running.
    if (tick == last_tick_ + 1 && mod_pulses == last_mod_) {
      if (++phase_ >= mod_pulses) {
        phase_ = 0;
        ++beat_;
      }
    } else {
      // uClock emits tick == 0 on every start/reset, so treat it as a RESTART:
      // return the func's playback to the beginning (e.g. euclidean step 0), not
      // just re-align the clock phase.
      if (tick == 0)
        _funcResetPlayback();
      phase_ = tick % mod_pulses;
      beat_ = tick / mod_pulses;
    }
    last_tick_ = tick;
    last_mod_ = mod_pulses;

    StepContext ctx{phase_, mod_pulses, beat_, output};
    switch (func_) {
#define X(E, M, T) case E: state_.M.process(ctx); break;
      FUNC_LIST(X)
#undef X
    default: break;
    }
  }

  // --- Persistence (base params only) ---
  void saveFunc(byte *payload) const {
    switch (func_) {
#define X(E, M, T) case E: state_.M.save(payload); break;
      FUNC_LIST(X)
#undef X
    default: break;
    }
  }
  void loadFunc(const byte *payload) {
    switch (func_) {
#define X(E, M, T) case E: state_.M.load(payload); break;
      FUNC_LIST(X)
#undef X
    default: break;
    }
    _funcSyncParam();
    _funcFinalize(mod_pulses_);
  }

private:
  bool _targetsClockMod() const {
    return cv1_target_ == CV_CLOCK_MOD || cv2_target_ == CV_CLOCK_MOD;
  }
  bool _targetsParam(uint8_t i) const {
    return cv1_target_ == CV_PARAM_0 + i || cv2_target_ == CV_PARAM_0 + i;
  }

  // Func dispatch helpers (generated from FUNC_LIST).
  void _funcReset() {
    switch (func_) {
#define X(E, M, T) case E: state_.M.reset(); break;
      FUNC_LIST(X)
#undef X
    default: break;
    }
  }
  void _funcFinalize(uint16_t mp) {
    switch (func_) {
#define X(E, M, T) case E: state_.M.finalize(mp); break;
      FUNC_LIST(X)
#undef X
    default: break;
    }
  }
  void _funcSyncParam() {
    switch (func_) {
#define X(E, M, T) case E: state_.M.syncParam(); break;
      FUNC_LIST(X)
#undef X
    default: break;
    }
  }
  void _funcResetPlayback() {
    switch (func_) {
#define X(E, M, T) case E: state_.M.resetPlayback(); break;
      FUNC_LIST(X)
#undef X
    default: break;
    }
  }
  // Cache clockModPulses(cvmod_clock_mod_index_) so the hot path doesn't do a
  // PROGMEM read every tick. Call after any change to cvmod_clock_mod_index_.
  void _refreshModPulses() { mod_pulses_ = clockModPulses(cvmod_clock_mod_index_); }
  int _funcGetBase(uint8_t i) const {
    switch (func_) {
#define X(E, M, T) case E: return state_.M.getBase(i);
      FUNC_LIST(X)
#undef X
    default: return 0;
    }
  }
  void _funcSetBase(uint8_t i, int v) {
    switch (func_) {
#define X(E, M, T) case E: state_.M.setBase(i, v); break;
      FUNC_LIST(X)
#undef X
    default: break;
    }
  }
  int _funcGetParam(uint8_t i) const {
    switch (func_) {
#define X(E, M, T) case E: return state_.M.getParam(i);
      FUNC_LIST(X)
#undef X
    default: return 0;
    }
  }
  void _funcSetParam(uint8_t i, int v) {
    switch (func_) {
#define X(E, M, T) case E: state_.M.setParam(i, v); break;
      FUNC_LIST(X)
#undef X
    default: break;
    }
  }
  void _funcCvRange(uint8_t i, int &lo, int &hi) const {
    switch (func_) {
#define X(E, M, T) case E: state_.M.cvRange(i, lo, hi); return;
      FUNC_LIST(X)
#undef X
    default: lo = 0; hi = 0; return;
    }
  }

  ChannelFunc func_;
  byte base_clock_mod_index_;
  byte cvmod_clock_mod_index_;
  CvTarget cv1_target_;
  CvTarget cv2_target_;
  bool mute_;
  // Per-channel clock-mod phase counter (avoids 32-bit tick % / divide on the
  // hot path). last_tick_/last_mod_ detect discontinuities that force a recompute.
  uint16_t phase_;
  uint16_t beat_;
  uint32_t last_tick_;
  uint16_t last_mod_;
  uint16_t mod_pulses_; // cached clockModPulses(cvmod_clock_mod_index_)
  FuncState state_;
};

#endif // MODAL_CHANNEL_H
