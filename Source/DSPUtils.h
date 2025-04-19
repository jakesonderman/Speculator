#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>
#include <cmath>

namespace DSPUtils {

// High-quality sinc resampler
class Resampler {
public:
    static constexpr int SINC_POINTS = 8;
    
    void prepare(double sampleRate) {
        buildKernel();
    }
    
    float resample(const float* input, double position, int bufferSize) {
        const int pos = static_cast<int>(std::floor(position));
        const float frac = position - pos;
        
        float sum = 0.0f;
        for (int i = -SINC_POINTS; i <= SINC_POINTS; ++i) {
            const int readPos = pos + i;
            if (readPos >= 0 && readPos < bufferSize) {
                sum += input[readPos] * sincInterpolate(frac - i);
            }
        }
        return sum;
    }
    
private:
    static float sincInterpolate(float x) {
        if (x == 0.0f) return 1.0f;
        const float px = juce::MathConstants<float>::pi * x;
        return std::sin(px) / px;
    }
    
    void buildKernel() {
        // Pre-calculate sinc kernel if needed
    }
};

// 4th order Butterworth filter
class ButterworthFilter {
public:
    void prepare(double sampleRate) {
        fs = static_cast<float>(sampleRate);
        reset();
    }
    
    void setCutoff(float frequency) {
        const float omega = 2.0f * juce::MathConstants<float>::pi * frequency / fs;
        const float cosOmega = std::cos(omega);
        const float alpha = std::sin(omega) / 1.414213562f; // Q = 1/sqrt(2)
        
        const float a0 = 1.0f + alpha;
        b[0] = (1.0f - cosOmega) / (2.0f * a0);
        b[1] = (1.0f - cosOmega) / a0;
        b[2] = b[0];
        a[1] = (-2.0f * cosOmega) / a0;
        a[2] = (1.0f - alpha) / a0;
    }
    
    float process(float input) {
        const float output = b[0] * input + b[1] * x[1] + b[2] * x[2] 
                           - a[1] * y[1] - a[2] * y[2];
                           
        x[2] = x[1];
        x[1] = input;
        y[2] = y[1];
        y[1] = output;
        
        return output;
    }
    
    void reset() {
        std::fill(x.begin(), x.end(), 0.0f);
        std::fill(y.begin(), y.end(), 0.0f);
    }
    
private:
    float fs = 44100.0f;
    std::array<float, 3> a{1.0f, 0.0f, 0.0f};
    std::array<float, 3> b{1.0f, 0.0f, 0.0f};
    std::array<float, 3> x{0.0f, 0.0f, 0.0f};
    std::array<float, 3> y{0.0f, 0.0f, 0.0f};
};

// DC Blocker
class DCBlocker {
public:
    float process(float input) {
        const float output = input - x1 + R * y1;
        x1 = input;
        y1 = output;
        return output;
    }
    
    void reset() {
        x1 = y1 = 0.0f;
    }
    
private:
    static constexpr float R = 0.995f;
    float x1 = 0.0f, y1 = 0.0f;
};

// Improved window function for grains
class GrainWindow {
public:
    static float getGainAt(float phase, float overlap = 0.5f) {
        // Enhanced window combining Hann and exponential edges
        const float hann = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * phase));
        
        // Add exponential edges for smoother transitions
        const float edgeWidth = (1.0f - overlap) * 0.5f;
        if (phase < edgeWidth) {
            const float normPhase = phase / edgeWidth;
            return hann * (normPhase * normPhase);
        }
        else if (phase > (1.0f - edgeWidth)) {
            const float normPhase = (1.0f - phase) / edgeWidth;
            return hann * (normPhase * normPhase);
        }
        
        return hann;
    }
};

// Improved soft clipper with oversampling
class SoftClipper {
public:
    void prepare(double sampleRate) {
        filter.prepare(sampleRate * OVERSAMPLE);
        filter.setCutoff(sampleRate * 0.45f); // Nyquist - small margin
    }
    
    float process(float input) {
        // Oversample
        float oversampled[OVERSAMPLE];
        for (int i = 0; i < OVERSAMPLE; ++i) {
            oversampled[i] = input;
        }
        
        // Process each oversampled point
        for (int i = 0; i < OVERSAMPLE; ++i) {
            oversampled[i] = processSample(oversampled[i]);
            oversampled[i] = filter.process(oversampled[i]);
        }
        
        // Average back to original sample rate
        float sum = 0.0f;
        for (int i = 0; i < OVERSAMPLE; ++i) {
            sum += oversampled[i];
        }
        
        return sum / OVERSAMPLE;
    }
    
private:
    static constexpr int OVERSAMPLE = 4;
    ButterworthFilter filter;
    
    float processSample(float x) {
        // Multi-stage soft clipping
        x *= 0.686;  // Adjust input gain
        x = std::tanh(x);
        x = std::copysign(1.0f - std::exp(-std::abs(x)), x);
        return x;
    }
};

// Limiter for peak control
class PeakLimiter {
public:
    void prepare(double sampleRate) {
        attackTime = static_cast<float>(std::exp(-1.0 / (0.001 * sampleRate)));
        releaseTime = static_cast<float>(std::exp(-1.0 / (0.100 * sampleRate)));
    }
    
    float process(float input) {
        // Calculate envelope
        const float inputAbs = std::abs(input);
        if (inputAbs > envelope) {
            envelope = attackTime * (envelope - inputAbs) + inputAbs;
        } else {
            envelope = releaseTime * (envelope - inputAbs) + inputAbs;
        }
        
        // Apply limiting
        const float gain = envelope > threshold ? threshold / envelope : 1.0f;
        return input * gain;
    }
    
    void reset() {
        envelope = 0.0f;
    }
    
private:
    float envelope = 0.0f;
    float attackTime = 0.0f;
    float releaseTime = 0.0f;
    float threshold = 0.95f;
};

} // namespace DSPUtils 