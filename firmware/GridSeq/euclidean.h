/**
 * @file euclidean.h
 * @author Adam Wonak (https://github.com/awonak/)
 * @brief Alt firmware version of Gravity by Sitka Instruments.
 * @version 2.0.1
 * @date 2025-07-04
 *
 * @copyright MIT - (c) 2025 - Adam Wonak - adam.wonak@gmail.com
 *
 */

#ifndef EUCLIDEAN_H
#define EUCLIDEAN_H

#define MAX_PATTERN_LEN 32

struct EuclideanState {
    uint8_t steps;
    uint8_t hits;
    uint8_t offset;
    uint8_t padding;
};

const EuclideanState DEFAULT_PATTERN = {1, 1};

class Euclidean {
   public:
    Euclidean() {}
    ~Euclidean() {}

    enum Step : uint8_t {
        REST,
        HIT,
    };

    void Init(EuclideanState state) {
        steps_ = constrain(state.steps, 1, MAX_PATTERN_LEN);
        hits_ = constrain(state.hits, 1, steps_);
        updatePattern();
    }

    EuclideanState GetState() const { return {steps_, hits_}; }

    Step GetCurrentStep(byte i) {
        if (i >= MAX_PATTERN_LEN) return REST;
        return (pattern_bitmap_ & (1UL << i)) ? HIT : REST;
    }

    void SetSteps(int steps) {
        steps_ = constrain(steps, 1, MAX_PATTERN_LEN);
        hits_ = min(hits_, steps_);
        updatePattern();
    }

    void SetHits(int hits) {
        hits_ = constrain(hits, 1, steps_);
        updatePattern();
    }

    void Reset() { step_index_ = 0; }

    uint8_t GetSteps() const { return steps_; }
    uint8_t GetHits() const { return hits_; }
    uint8_t GetStepIndex() const { return step_index_; }

    Step NextStep() {
        if (steps_ == 0) return REST;

        Step value = GetCurrentStep(step_index_);
        step_index_ = (step_index_ < steps_ - 1) ? step_index_ + 1 : 0;
        return value;
    }

   private:
    uint8_t steps_ = 0;
    uint8_t hits_ = 0;
    volatile uint8_t step_index_ = 0;
    uint32_t pattern_bitmap_ = 0;

    // Update the euclidean rhythm pattern using bitmap
    void updatePattern() {
        pattern_bitmap_ = 0;  // Clear the bitmap

        if (steps_ == 0) return;

        byte bucket = 0;
        // Set the first bit (index 0) if it's a HIT
        pattern_bitmap_ |= (1UL << 0);

        for (int i = 1; i < steps_; i++) {
            bucket += hits_;
            if (bucket >= steps_) {
                bucket -= steps_;
                pattern_bitmap_ |= (1UL << i);
            }
        }
    }
};

#endif