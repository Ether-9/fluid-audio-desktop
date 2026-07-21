#include "PitchShifter.h"
#include <cmath>
#include <cstring>
#include <QDebug>
#include <algorithm>

PitchShifter::PitchShifter()
    : m_stretcher(nullptr)
    , m_targetFreq(528.0f)
    , m_originalFreq(440.0f)
    , m_ratio(1.2f)
    , m_initialized(false)
    , m_sampleRate(44100)
    , m_channels(2)
{
}

// Destructor
PitchShifter::~PitchShifter()
{
    if (m_stretcher) {
        delete m_stretcher;
    }
}

void PitchShifter::clearFifos()
{
    for (auto& chan : m_outputFifo) {
        chan.clear();
    }
}

void PitchShifter::initStretcher(int sampleRate, int channels)
{
    if (m_stretcher) {
        delete m_stretcher;
        m_stretcher = nullptr;
    }

    m_sampleRate = sampleRate;
    m_channels = channels;
    
    m_outputFifo.assign(m_channels, std::vector<float>());

    // [UPDATE]: Initialize and pre-allocate reusable buffers to prevent allocation on the audio thread
    m_inputChannels.assign(m_channels, std::vector<float>());
    m_retrievedData.assign(m_channels, std::vector<float>());
    for (int ch = 0; ch < m_channels; ++ch) {
        m_inputChannels[ch].reserve(8192); // Reserve generous capacity ahead of time
        m_retrievedData[ch].reserve(8192);
    }
    m_processPointers.resize(m_channels, nullptr);
    m_retrievePointers.resize(m_channels, nullptr);

    try {
        // Studio-quality options optimized for master tracks containing complex vocals + instrumentals:
        // - OptionProcessRealTime: Allows dynamic pitch ratio updates while running.
        // - OptionEngineFiner: Employs standard high-fidelity crispness engine (superior spectral resolution).
        // - OptionDetectorCompound: Blends transient response (bass/percussion) with phase stability (vocals).
        // - OptionThreadingNever: Keeps processing synchronous inside audio thread.
        int options = RubberBand::RubberBandStretcher::OptionProcessRealTime |
                      RubberBand::RubberBandStretcher::OptionEngineFiner |
                      RubberBand::RubberBandStretcher::OptionDetectorCompound | 
                      RubberBand::RubberBandStretcher::OptionThreadingNever;

        m_stretcher = new RubberBand::RubberBandStretcher(
            sampleRate, channels, options, 1.0f, m_ratio
        );
        m_stretcher->setMaxProcessSize(4096);

        // --- LATENCY COMPENSATION (PRIMING) ---
        size_t latency = m_stretcher->getStartDelay();
        if (latency > 0) {
            std::vector<std::vector<float>> silence(channels, std::vector<float>(latency, 0.0f));
            std::vector<const float*> silencePointers(channels);
            for (int ch = 0; ch < channels; ++ch) {
                silencePointers[ch] = silence[ch].data();
            }
            // Prime with absolute silence so output frames are immediately retrievable
            m_stretcher->process(silencePointers.data(), latency, false);
        }

        m_initialized = true;
        qDebug() << "RubberBand initialized & primed. Latency compensation:" << latency << "frames";
    } catch (const std::exception& e) {
        qDebug() << "ERROR: RubberBand init failed:" << e.what();
        m_initialized = false;
    }
}

void PitchShifter::setTargetFrequency(float freq, float originalFreq)
{
    if (freq <= 0 || originalFreq <= 0) return;
    
    m_targetFreq = freq;
    m_originalFreq = originalFreq;
    m_ratio = freq / originalFreq;
    
    // 0.1f to 10.0f flawlessly covers the 174Hz to 963Hz Solfeggio scale bounds
    if (m_ratio < 0.1f || m_ratio > 10.0f) {
        m_ratio = 1.0f;
        m_targetFreq = m_originalFreq;
    }
    
    if (m_stretcher && m_initialized) {
        m_stretcher->setPitchScale(m_ratio);
    }
}

void PitchShifter::feed(const float* interleavedInput, int numFrames)
{
    if (!m_initialized || !m_stretcher || numFrames <= 0) return;

    // [UPDATE]: Using pre-allocated buffers instead of allocating new vectors every frame.
    // .resize() does not allocate memory if capacity is sufficient (which we ensured in initStretcher).
    for (int ch = 0; ch < m_channels; ++ch) {
        m_inputChannels[ch].resize(numFrames);
    }

    // 1. De-interleave the channels
    for (int i = 0; i < numFrames; ++i) {
        for (int ch = 0; ch < m_channels; ++ch) {
            m_inputChannels[ch][i] = interleavedInput[i * m_channels + ch];
        }
    }

    // 2. Submit to Rubber Band
    for (int ch = 0; ch < m_channels; ++ch) {
        m_processPointers[ch] = m_inputChannels[ch].data();
    }
    m_stretcher->process(m_processPointers.data(), numFrames, false);

    // 3. Pull newly-synthesized frames directly into the output FIFO
    int available = m_stretcher->available();
    if (available > 0) {
        for (int ch = 0; ch < m_channels; ++ch) {
            m_retrievedData[ch].resize(available);
            m_retrievePointers[ch] = m_retrievedData[ch].data();
        }
        
        int retrieved = m_stretcher->retrieve(m_retrievePointers.data(), available);
        for (int ch = 0; ch < m_channels; ++ch) {
            m_outputFifo[ch].insert(m_outputFifo[ch].end(), 
                                    m_retrievedData[ch].begin(), 
                                    m_retrievedData[ch].begin() + retrieved);
        }
    }
}

int PitchShifter::getAvailableFrames() const
{
    if (m_outputFifo.empty() || m_outputFifo[0].empty()) return 0;
    return static_cast<int>(m_outputFifo[0].size());
}

int PitchShifter::retrieve(float* interleavedOutput, int numFrames)
{
    int available = getAvailableFrames();
    int toWrite = std::min(numFrames, available);
    if (toWrite <= 0) return 0;

    // Interleave the frames back to the hardware device format
    for (int i = 0; i < toWrite; ++i) {
        for (int ch = 0; ch < m_channels; ++ch) {
            interleavedOutput[i * m_channels + ch] = m_outputFifo[ch][i];
        }
    }

    // Clean consumed frames from our FIFO
    for (int ch = 0; ch < m_channels; ++ch) {
        m_outputFifo[ch].erase(m_outputFifo[ch].begin(), m_outputFifo[ch].begin() + toWrite);
    }

    return toWrite;
}

void PitchShifter::reset()
{
    if (m_stretcher) {
        m_stretcher->reset();
    }
    clearFifos();
}