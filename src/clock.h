/**
 * @file clock.h
 * @author Adam Wonak (https://github.com/awonak)
 * @brief Wrapper Class for clock timing functions.
 * @version 0.1
 * @date 2025-05-04
 *
 * @copyright MIT - (c) 2025 - Adam Wonak - adam.wonak@gmail.com
 *
 */

#ifndef CLOCK_H
#define CLOCK_H

#include <NeoHWSerial.h>

#include "peripherials.h"
#include "uClock/uClock.h"

// MIDI clock, start, stop, and continue byte definitions - based on MIDI 1.0
// Standards.
#define MIDI_CLOCK 0xF8
#define MIDI_START 0xFA
#define MIDI_STOP 0xFC
#define MIDI_CONTINUE 0xFB

static void serialEventNoop(uint8_t msg, uint8_t status) {}

class Clock {
public:
  static constexpr int DEFAULT_TEMPO = 120;

  enum Source {
    SOURCE_INTERNAL,
    SOURCE_EXTERNAL_PPQN_24,
    SOURCE_EXTERNAL_PPQN_4,
    SOURCE_EXTERNAL_PPQN_2,
    SOURCE_EXTERNAL_PPQN_1,
    SOURCE_EXTERNAL_MIDI,
    SOURCE_LAST,
  };

  enum Pulse {
    PULSE_NONE,
    PULSE_PPQN_1,
    PULSE_PPQN_4,
    PULSE_PPQN_24,
    PULSE_LAST,
  };

  void Init() {
    NeoSerial.begin(31250);

    // Initialize the clock library
    uClock.init();
    uClock.setClockMode(uClock.INTERNAL_CLOCK);
    uClock.setOutputPPQN(uClock.PPQN_96);
    uClock.setTempo(DEFAULT_TEMPO);

    // MIDI events.
    uClock.setOnClockStart(sendMIDIStart);
    uClock.setOnClockStop(sendMIDIStop);
    uClock.setOnSync24(sendMIDIClock);

    uClock.start();
  }

  // Handle external clock tick and call user callback when receiving clock
  // trigger (PPQN_4, PPQN_24, or MIDI).
  void AttachExtHandler(void (*callback)()) {
    // Configure the EXT input. The original firmware ran on this same hardware
    // with INPUT_PULLUP + FALLING edge (the input idles HIGH via its input
    // conditioning and a clock/reset pulse pulls it LOW). Match that here.
    // (If your build drives EXT high-active with an external pull-down, switch
    // to INPUT + RISING instead.)
    pinMode(EXT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(EXT_PIN), callback, FALLING);
  }

  // Internal PPQN96 callback for all clock timer operations.
  void AttachIntHandler(void (*callback)(uint32_t)) {
    uClock.setOnOutputPPQN(callback);
  }

  // Set the source of the clock mode.
  void SetSource(Source source) {
    bool was_playing = !IsPaused();
    uClock.stop();
    // If we are changing the source from MIDI, disable the serial interrupt
    // handler.
    if (source_ == SOURCE_EXTERNAL_MIDI) {
      NeoSerial.attachInterrupt(serialEventNoop);
    }
    source_ = source;
    switch (source) {
    case SOURCE_INTERNAL:
      uClock.setClockMode(uClock.INTERNAL_CLOCK);
      break;
    case SOURCE_EXTERNAL_PPQN_24:
      uClock.setClockMode(uClock.EXTERNAL_CLOCK);
      uClock.setInputPPQN(uClock.PPQN_24);
      break;
    case SOURCE_EXTERNAL_PPQN_4:
      uClock.setClockMode(uClock.EXTERNAL_CLOCK);
      uClock.setInputPPQN(uClock.PPQN_4);
      break;
    case SOURCE_EXTERNAL_PPQN_2:
      uClock.setClockMode(uClock.EXTERNAL_CLOCK);
      uClock.setInputPPQN(uClock.PPQN_2);
      break;
    case SOURCE_EXTERNAL_PPQN_1:
      uClock.setClockMode(uClock.EXTERNAL_CLOCK);
      uClock.setInputPPQN(uClock.PPQN_1);
      break;
    case SOURCE_EXTERNAL_MIDI:
      uClock.setClockMode(uClock.EXTERNAL_CLOCK);
      uClock.setInputPPQN(uClock.PPQN_24);
      NeoSerial.attachInterrupt(onSerialEvent);
      break;
    case SOURCE_LAST:
    default:
      break;
    }
    if (was_playing) {
      uClock.start();
    }
  }

  // Return true if the current selected source is externl (PPQN_4, PPQN_24, or MIDI).
  bool ExternalSource() {
    return uClock.getClockMode() == uClock.EXTERNAL_CLOCK;
  }

  // Return true if the current selected source is the internal master clock.
  bool InternalSource() {
    return uClock.getClockMode() == uClock.INTERNAL_CLOCK;
  }

  // Returns the current BPM tempo.
  int Tempo() { return uClock.getTempo(); }

  // Set the clock tempo to a int between 1 and 400.
  void SetTempo(int tempo) { return uClock.setTempo(tempo); }

  // Record an external clock tick received to process external/internal syncronization.
  void Tick() { uClock.clockMe(); }

  // Start the internal clock.
  void Start() { uClock.start(); }

  // Stop internal clock clock.
  void Stop() { uClock.stop(); }

  // Reset all clock counters to 0.
  void Reset() { uClock.resetCounters(); }

  // Returns true if the clock is not running.
  bool IsPaused() { return uClock.clock_state == uClock.PAUSED; }

private:
  Source source_ = SOURCE_INTERNAL;

  static void onSerialEvent(uint8_t msg, uint8_t status) {
    // Note: uClock.start()/stop() already echo MIDI Start/Stop via the clock start/stop callbacks,
    switch (msg) {
    case MIDI_CLOCK:
      // Advance the external clock on every incoming MIDI clock pulse.
      // The EXT hardware pin continues to act as reset in MIDI mode 
      uClock.clockMe();
      break;
    case MIDI_STOP:
      uClock.stop();
      break;
    case MIDI_START:
    case MIDI_CONTINUE:
      // uClock has no separate "continue"; both start the clock
      uClock.start();
      break;
    }
  }

  static void sendMIDIStart() { NeoSerial.write(MIDI_START); }

  static void sendMIDIStop() { NeoSerial.write(MIDI_STOP); }

  static void sendMIDIClock(uint32_t tick) { NeoSerial.write(MIDI_CLOCK); }
};

#endif