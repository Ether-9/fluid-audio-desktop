#include "ReplayGain.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <QDebug>

ReplayGain::ReplayGain(QObject* parent)
    : QObject(parent), m_enabled(false), 
      m_targetLoudness(-18.0f), m_currentGain(1.0f)
{
}

ReplayGain::~ReplayGain()
{
}

void ReplayGain::setEnabled(bool enabled)
{
    m_enabled = enabled;
    qDebug() << "ReplayGain state set to:" << (enabled ? "enabled" : "disabled");
}

void ReplayGain::setTargetLoudness(float loudness)
{
    m_targetLoudness = qBound(-30.0f, loudness, -10.0f);
    qDebug() << "Target loudness set to:" << m_targetLoudness << "LUFS";
}

float ReplayGain::analyzeLoudness(const std::vector<float>& audio, int sampleRate)
{
    if (audio.empty()) return -60.0f;
    
    float lufs = calculateLUFS(audio, sampleRate);
    emit loudnessAnalyzed(lufs);
    return lufs;
}

float ReplayGain::calculateGain(const std::vector<float>& audio, int sampleRate)
{
    if (!m_enabled || audio.empty()) return 1.0f;
    
    float lufs = calculateLUFS(audio, sampleRate);
    float gain = std::pow(10.0f, (m_targetLoudness - lufs) / 20.0f);
    
    // Guard rails to prevent extreme amplification issues
    gain = qBound(0.10f, gain, 5.0f);
    
    m_currentGain = gain;
    emit gainCalculated(gain);
    
    return gain;
}

std::vector<float> ReplayGain::applyGain(const std::vector<float>& audio, float gain)
{
    if (!m_enabled || audio.empty()) return audio;
    
    std::vector<float> result(audio.size());
    for (size_t i = 0; i < audio.size(); ++i) {
        float sample = audio[i] * gain;
        
        // Fix: Smooth sigmoid soft-clipping function to prevent digital clipping click artifacts
        if (sample > 0.95f) {
            sample = 0.95f + (1.0f - 0.95f) * std::tanh((sample - 0.95f) / (1.0f - 0.95f));
        } else if (sample < -0.95f) {
            sample = -0.95f + (-1.0f - (-0.95f)) * std::tanh((sample - (-0.95f)) / (-1.0f - (-0.95f)));
        }
        
        result[i] = qBound(-1.0f, sample, 1.0f);
    }
    return result;
}

void ReplayGain::processAudio(std::vector<float>& audio, int sampleRate)
{
    if (!m_enabled || audio.empty()) return;
    
    float gain = calculateGain(audio, sampleRate);
    
    // Fix: In-place multiplication bypasses expensive allocation and memory copying
    for (size_t i = 0; i < audio.size(); ++i) {
        float sample = audio[i] * gain;
        
        // Perform soft-clipping inline on the source vector
        if (sample > 0.95f) {
            sample = 0.95f + 0.05f * std::tanh((sample - 0.95f) / 0.05f);
        } else if (sample < -0.95f) {
            sample = -0.95f + 0.05f * std::tanh((sample + 0.95f) / 0.05f);
        }
        audio[i] = qBound(-1.0f, sample, 1.0f);
    }
}

float ReplayGain::calculateRMS(const std::vector<float>& audio)
{
    if (audio.empty()) return 0.0f;
    
    // Fix: Use std::accumulate with double precision to prevent float underflow/overflow accumulator bugs
    double sumOfSquares = std::accumulate(audio.begin(), audio.end(), 0.0, 
        [](double sum, float sample) {
            return sum + (static_cast<double>(sample) * sample);
        });
        
    return static_cast<float>(std::sqrt(sumOfSquares / audio.size()));
}

float ReplayGain::calculateLUFS(const std::vector<float>& audio, int sampleRate)
{
    // Fix: Filter copy allocations are avoided if data sizes are too small
    if (audio.size() < 128) return -60.0f;
    
    // EBU R128 loudness calculation involves running pre-filters (K-Weighting filtering)
    std::vector<float> filteredAudio = audio;
    applyPreFilter(filteredAudio);
    applyRLBFilter(filteredAudio);
    
    float rms = calculateRMS(filteredAudio);
    if (rms < 0.000001f) return -70.0f; // Standard EBU noise floor floor value
    
    // Apply -0.691 dB gain compensation according to the ITU-R BS.1770-4 spec
    return 20.0f * std::log10(rms) - 0.691f;
}

void ReplayGain::applyPreFilter(std::vector<float>& audio)
{
    // ITU-R BS.1770 K-Weighting First-Stage High Shelving Filter
    // Pre-calculated coefficients assuming standard sample rate of 44100 Hz
    const double b0 = 1.53512485958697;
    const double b1 = -2.69169618940638;
    const double b2 = 1.19839281083701;
    const double a1 = -1.69065929318241;
    const double a2 = 0.73248077420001;
    
    double x1 = 0.0, x2 = 0.0;
    double y1 = 0.0, y2 = 0.0;
    
    for (size_t i = 0; i < audio.size(); ++i) {
        double x0 = audio[i];
        double y0 = b0*x0 + b1*x1 + b2*x2 - a1*y1 - a2*y2;
        
        x2 = x1;
        x1 = x0;
        y2 = y1;
        y1 = y0;
        
        audio[i] = static_cast<float>(y0);
    }
}

void ReplayGain::applyRLBFilter(std::vector<float>& audio)
{
    // ITU-R BS.1770 K-Weighting Second-Stage High-Pass Filter (RLB Curve)
    // Pre-calculated coefficients assuming standard sample rate of 44100 Hz
    const double b0 = 1.0;
    const double b1 = -2.0;
    const double b2 = 1.0;
    const double a1 = -1.99004745410009;
    const double a2 = 0.99007225034137;
    
    double x1 = 0.0, x2 = 0.0;
    double y1 = 0.0, y2 = 0.0;
    
    for (size_t i = 0; i < audio.size(); ++i) {
        double x0 = audio[i];
        double y0 = b0*x0 + b1*x1 + b2*x2 - a1*y1 - a2*y2;
        
        x2 = x1;
        x1 = x0;
        y2 = y1;
        y1 = y0;
        
        audio[i] = static_cast<float>(y0);
    }
}
