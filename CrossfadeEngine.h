#ifndef CROSSFADEENGINE_H
#define CROSSFADEENGINE_H

#include <vector>
#include <QObject>

class CrossfadeEngine : public QObject
{
    Q_OBJECT

public:
    explicit CrossfadeEngine(QObject* parent = nullptr);
    ~CrossfadeEngine();

    void setDuration(float seconds);
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }
    float getDuration() const { return m_duration; }
    
    std::vector<float> processCrossfade(
        const std::vector<float>& currentSong,
        const std::vector<float>& nextSong,
        int sampleRate,
        int channels
    );

signals:
    void crossfadeProgress(float progress);

private:
    float m_duration;     // Crossfade duration in seconds
    bool m_enabled;
    int m_sampleRate;
    int m_channels;
    
    float crossfadeGain(float position, float totalLength);
    std::vector<float> applyFadeIn(const std::vector<float>& audio, float startPos, float duration);
    std::vector<float> applyFadeOut(const std::vector<float>& audio, float startPos, float duration);
};

#endif // CROSSFADEENGINE_H
