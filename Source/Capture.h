#pragma once
#include <atomic>
#include <vector>
#include <algorithm>
#include <cmath>
// Single audio producer, single UI consumer. No allocation, locks or network on audio thread.
class Capture {
public:
    enum State { idle, recording, ready };
    Capture() : samples(24'000 * 30 + 1024, 0.0f) {}
    bool start(double inputRate) {
        if (state.load(std::memory_order_acquire) != idle || inputRate <= 0) return false;
        stride = std::max(1, (int) std::ceil(inputRate / 24000.0));
        rate = inputRate / stride; limit = std::min((int)samples.size(), (int)(rate * 30));
        count = phase = 0; accumulator = 0; stop.store(false); state.store(recording, std::memory_order_release); return true;
    }
    void requestStop() { stop.store(true, std::memory_order_release); }
    // Only call from a host/device lifecycle callback after its audio producer has stopped.
    void finishAfterProducerStopped() {
        if (state.load(std::memory_order_acquire) == recording)
            state.store(ready, std::memory_order_release);
    }
    void push(const float* const* channels, int numChannels, int numSamples) {
        if (state.load(std::memory_order_acquire) != recording) return;
        for (int i = 0; i < numSamples && count < limit && !stop.load(std::memory_order_relaxed); ++i) {
            float value = 0; int valid = 0;
            for(int c=0;c<numChannels;++c) if(channels[c]) { value += channels[c][i]; ++valid; }
            accumulator += valid ? value / (float)valid : 0;
            if (++phase == stride) { samples[(size_t)count++] = accumulator / (float)stride; phase=0; accumulator=0; }
        }
        if(count >= limit || stop.load(std::memory_order_acquire)) state.store(ready, std::memory_order_release);
    }
    bool take(std::vector<float>& out, double& sampleRate) {
        if(state.load(std::memory_order_acquire) != ready) return false;
        out.assign(samples.begin(), samples.begin()+count); sampleRate=rate;
        state.store(idle, std::memory_order_release); return true;
    }
    State status() const { return state.load(std::memory_order_acquire); }
private:
    std::vector<float> samples;
    std::atomic<State> state{idle}; std::atomic<bool> stop{false};
    float accumulator=0;
    int count=0, phase=0, stride=1, limit=0; double rate=24000;
};
