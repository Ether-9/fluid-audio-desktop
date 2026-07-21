#include "CrossfadeEngine.h"
#include <cmath>
#include <algorithm>
#include <QDebug>

CrossfadeEngine::CrossfadeEngine(QObject* parent)
    : QObject(parent), m_duration(2.0f), m_enabled(false), 
      m_sampleRate(44100), m_channels(2)
{
}

CrossfadeEngine::~CrossfadeEngine()
{
}

void CrossfadeEngine::setDuration(float seconds)
{
    m_duration = qBound(0.0f, seconds, 12.0f);
    qDebug() << "Crossfade duration set to:" << m_duration << "seconds";
}

void CrossfadeEngine::setEnabled(bool enabled)
{
    m_enabled = enabled;
    qDebug() << "Crossfade" << (enabled ? "enabled" : "disabled");
}

std::vector<float> CrossfadeEngine::processCrossfade(
    const std::vector<float>& currentSong,
    const std::vector<float>& nextSong,
    int sampleRate,
    int channels)
{
    // Fix: If validation checks fail, securely clone the fallback audio stream to avoid unexpected silencing
    if (!m_enabled || m_duration <= 0.0f || currentSong.empty() || nextSong.empty()) {
        return nextSong;
    }
    
    m_sampleRate = sampleRate;
    m_channels = channels;
    
    // Track multi-channel block sizes precisely to prevent audio framing drift
    int fadeSamples = static_cast<int>(m_duration * sampleRate * channels);
    
    int currentSize = static_cast<int>(currentSong.size());
    int nextSize = static_cast<int>(nextSong.size());
    
    int fadeStart = std::max(0, currentSize - fadeSamples);
    int actualFadeSamples = currentSize - fadeStart;
    
    if (actualFadeSamples <= 0) {
        return nextSong;
    }
    
    // Fix: Guard buffer iteration bounds against both files simultaneously to avoid segmentation fault index crashes
    int crossfadeEnd = std::min(actualFadeSamples, nextSize);
    
    // Allocate processing space cleanly
    std::vector<float> output(nextSize, 0.0f);
    std::copy(nextSong.begin(), nextSong.end(), output.begin());
    
    // Process mixing calculations
    for (int i = 0; i < crossfadeEnd; ++i) {
        // Fix: Sample index stepping calculations take total multi-channel framing into account
        float progress = static_cast<float>(i) / static_cast<float>(actualFadeSamples);
        
        // Equal-power crossfade equation (sin/cos curve) preserves steady root-mean-square loudness levels
        float currentGain = std::cos(progress * M_PI_2);
        float nextGain = std::sin(progress * M_PI_2);
        
        int currentIdx = fadeStart + i;
        int nextIdx = i;
        
        if (currentIdx < currentSize && nextIdx < nextSize) {
            output[nextIdx] = (currentSong[currentIdx] * currentGain) + (nextSong[nextIdx] * nextGain);
        }
    }
    
    emit crossfadeProgress(1.0f);
    return output;
}

std::vector<float> CrossfadeEngine::applyFadeIn(const std::vector<float>& audio, float startPos, float duration)
{
    if (audio.empty() || duration <= 0.0f) return audio;
    
    std::vector<float> result = audio;
    int startSample = static_cast<int>(startPos * m_sampleRate * m_channels);
    int durationSamples = static_cast<int>(duration * m_sampleRate * m_channels);
    int endSample = std::min(startSample + durationSamples, static_cast<int>(result.size()));
    
    for (int i = startSample; i < endSample; ++i) {
        float progress = static_cast<float>(i - startSample) / durationSamples;
        result[i] *= (progress * progress * (3.0f - 2.0f * progress)); // Smooth cubic fade-in interpolation
    }
    return result;
}

std::vector<float> CrossfadeEngine::applyFadeOut(const std::vector<float>& audio, float startPos, float duration)
{
    if (audio.empty() || duration <= 0.0f) return audio;
    
    std::vector<float> result = audio;
    int startSample = static_cast<int>(startPos * m_sampleRate * m_channels);
    int durationSamples = static_cast<int>(duration * m_sampleRate * m_channels);
    int endSample = std::min(startSample + durationSamples, static_cast<int>(result.size()));
    
    for (int i = startSample; i < endSample; ++i) {
        float progress = static_cast<float>(i - startSample) / durationSamples;
        result[i] *= (1.0f - (progress * progress * (3.0f - 2.0f * progress))); // Smooth cubic fade-out interpolation
    }
    return result;
}

float CrossfadeEngine::crossfadeGain(float position, float totalLength)
{
    if (totalLength <= 0.0f) return 0.0f;
    float t = qBound(0.0f, position / totalLength, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
