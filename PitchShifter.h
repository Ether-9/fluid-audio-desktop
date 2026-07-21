#ifndef PITCHSHIFTER_H
#define PITCHSHIFTER_H

#include <rubberband/RubberBandStretcher.h>
#include <vector>

class PitchShifter
{
public:
    PitchShifter();
    ~PitchShifter();

    void initStretcher(int sampleRate, int channels);
    void setTargetFrequency(float freq, float originalFreq = 440.0f);
    void reset();
    
    // Streaming FIFO API
    void feed(const float* interleavedInput, int numFrames);
    int getAvailableFrames() const;
    int retrieve(float* interleavedOutput, int numFrames);

    bool isActive() const { return m_ratio != 1.0f && m_ratio > 0.01f; }
    float getRatio() const { return m_ratio; }

private:
    RubberBand::RubberBandStretcher* m_stretcher;
    float m_targetFreq;
    float m_originalFreq;
    float m_ratio;
    bool m_initialized;
    int m_sampleRate;
    int m_channels;

    std::vector<std::vector<float>> m_outputFifo;
    
    // [UPDATE]: Pre-allocated buffers to prevent real-time memory allocation stutters
    std::vector<std::vector<float>> m_inputChannels;
    std::vector<std::vector<float>> m_retrievedData;
    std::vector<const float*> m_processPointers;
    std::vector<float*> m_retrievePointers;

    void clearFifos();
};

#endif // PITCHSHIFTER_H