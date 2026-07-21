#pragma once

#include <QObject>
#include <QString>
#include <QMutex>
#include <vector>
#include <cstdint>
#include <string>

struct ma_decoder;
struct ma_device;
class PitchShifter; // Forward declaration

class AudioEngine : public QObject {
    Q_OBJECT
private:
    struct ma_decoder* pDecoder = nullptr;
    struct ma_device* pDevice = nullptr;
    PitchShifter* pPitchShifter = nullptr; // Pointer to updated high-fidelity component
    
    bool isDecoderInitialized = false;
    bool isDeviceInitialized = false;
    bool m_isPlaying = false; 
    QString m_currentSong;    
    float targetFrequency = 440.0f; 
    float currentPitchFactor = 1.0f;
    
    int64_t currentPlayPosition = 0;
    int64_t totalDurationFrames = 0;

    mutable QMutex audioMutex;
    std::vector<float> dspInputBuffer;

    static void data_callback(struct ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount);

signals:
    void positionChanged();
    void playbackFinished();

public:
    explicit AudioEngine(QObject* parent = nullptr);
    ~AudioEngine();

    void setCrossfade(int seconds);
    void loadSong(const std::string& filePath);
    void play();
    void pause();
    void stop();
    void insertIntoQueueNext(const std::string& filePath);
    void appendToQueue(const std::string& filePath);
    void clearQueue();
    bool isFinished();

    bool isPlaying() const { return m_isPlaying; }
    QString getCurrentSong() const { return m_currentSong; }
    int getCurrentFrequency() const { return static_cast<int>(targetFrequency); }

    void setFrequency(int freq);
    double getDuration();
    double getPosition();
    void seek(float seconds);
    
    void setReplayGain(bool enabled);
    void setEQEnabled(bool enabled);
    void setSeeking(bool isSeeking);
    void setVolume(float volume);
    void setEQBand(int band, float gain);
};