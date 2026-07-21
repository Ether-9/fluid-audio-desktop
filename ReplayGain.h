#ifndef REPLAYGAIN_H
#define REPLAYGAIN_H

#include <vector>
#include <QObject>

class ReplayGain : public QObject
{
    Q_OBJECT

public:
    explicit ReplayGain(QObject* parent = nullptr);
    ~ReplayGain() override;

    void setEnabled(bool enabled);
    void setTargetLoudness(float loudness); // in LUFS
    bool isEnabled() const { return m_enabled; }
    float getTargetLoudness() const { return m_targetLoudness; }
    
    float analyzeLoudness(const std::vector<float>& audio, int sampleRate);
    float calculateGain(const std::vector<float>& audio, int sampleRate);
    std::vector<float> applyGain(const std::vector<float>& audio, float gain);
    void processAudio(std::vector<float>& audio, int sampleRate);

signals:
    void gainCalculated(float gain);
    void loudnessAnalyzed(float loudness);

private:
    bool m_enabled;
    float m_targetLoudness; // Target loudness in LUFS (default: -18 LUFS)
    float m_currentGain;
    
    // EBU R128 calculation helpers
    float calculateRMS(const std::vector<float>& audio);
    float calculateLUFS(const std::vector<float>& audio, int sampleRate);
    void applyPreFilter(std::vector<float>& audio);
    void applyRLBFilter(std::vector<float>& audio);
};

#endif // REPLAYGAIN_H
