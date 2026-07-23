/**
 * @file channel.h
 * @author Adam Wonak (https://github.com/awonak/)
 * @brief Grid Sequencer.
 * @version 1.0.0
 * @date 2025-08-12
 *
 * @copyright MIT - (c) 2025 - Adam Wonak - adam.wonak@gmail.com
 *
 */

#ifndef CHANNEL_H
#define CHANNEL_H

#include <Arduino.h>
#include <libGravity.h>

#include "euclidean.h"

// Enums for CV Mod destination
enum CvDestination : uint8_t {
    CV_DEST_NONE,
    CV_DEST_MODE,
    CV_DEST_LENGTH,
    CV_DEST_DIV,
    CV_DEST_PROB,
    CV_DEST_DENSITY,
    CV_DEST_LAST,
};

// Enums for GridSeq modes
enum Mode : uint8_t {
    MODE_SEQ,
    MODE_EUCLIDEAN,
    MODE_PATTERN,
    MODE_LAST,
};


class Channel {
   public:
    Channel() {
        Init();
    }

    void Init() {
        base_probability = 100;

        cv1_dest = CV_DEST_NONE;
        cv2_dest = CV_DEST_NONE;
    }

    bool isCvModActive() const { return cv1_dest != CV_DEST_NONE || cv2_dest != CV_DEST_NONE; }

    // Setters (Set the BASE value)

    void setProbability(int prob) {
        base_probability = constrain(prob, 0, 100);
    }

    void setCv1Dest(CvDestination dest) {
        cv1_dest = dest;
    }
    void setCv2Dest(CvDestination dest) {
        cv2_dest = dest;
    }
    CvDestination getCv1Dest() const { return cv1_dest; }
    CvDestination getCv2Dest() const { return cv2_dest; }

    // Getters (Get the BASE value for editing or cv modded value for display)
    int getProbability() const { return base_probability; }

    // Getters that calculate the value with CV modulation applied.
    int getProbabilityWithMod(int cv1_val, int cv2_val) {
        int prob_mod = _calculateMod(CV_DEST_PROB, cv1_val, cv2_val, -50, 50);
        return constrain(base_probability + prob_mod, 0, 100);
    }

    void toggleMute() { mute = !mute; }

    /**
     * @brief Processes a clock tick and determines if the output should be high or low.
     * Note: this method is called from an ISR and must be kept as simple as possible.
     * @param tick The current clock tick count.
     * @param output The output object to be modified.
     */
    void processClockTick(uint32_t tick, DigitalOutput& output) {
        // Mute check
        if (mute) {
            output.Low();
            return;
        }

        int cv1 = gravity.cv1.Read();
        int cv2 = gravity.cv2.Read();
        int cvmod_probability = getProbabilityWithMod(cv1, cv2);

        // Duty cycle high check logic
        if (!output.On()) {
            // Step check
            bool hit = cvmod_probability >= random(0, 100);
            if (hit) {
                output.Trigger();
            }
        }

    }

   private:
    int _calculateMod(CvDestination dest, int cv1_val, int cv2_val, int min_range, int max_range) {
        int mod1 = (cv1_dest == dest) ? map(cv1_val, -512, 512, min_range, max_range) : 0;
        int mod2 = (cv2_dest == dest) ? map(cv2_val, -512, 512, min_range, max_range) : 0;
        return mod1 + mod2;
    }

    // User-settable base values.
    byte base_probability;

    // CV mod configuration
    CvDestination cv1_dest;
    CvDestination cv2_dest;

    // Mute channel flag
    bool mute;

    uint16_t _duty_pulses;
};

#endif  // CHANNEL_H