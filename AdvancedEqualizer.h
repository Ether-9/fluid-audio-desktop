#ifndef ADVANCED_EQUALIZER_H
#define ADVANCED_EQUALIZER_H

#include <QWidget> // Change from <QObject> to <QWidget>
#include <QString>
#include <QVector>
#include <QMap>
#include <QJsonObject>
#include <QMutex>

// Struct matching your existing architecture for buffer transfer
struct AudioBuffer {
    QVector<float> samples;
    int channels;
    int sampleRate;
};

class AdvancedEqualizer : public QWidget { // Change public QObject to public QWidget
    Q_OBJECT

public:
    enum FilterType {
        Peaking,
        LowPass,
        HighPass,
        BandPass,
        LowShelf,
        HighShelf,
        Notch
    };

    struct BandConfig {
        float frequency;
        float gain;
        float qFactor;
        FilterType type;
    };

    explicit AdvancedEqualizer(QWidget *parent = nullptr); // Change QObject* to QWidget*
    ~AdvancedEqualizer();

    // Preserved public API for seamless linking
    void setBandGain(int bandIndex, float gainDb);
    float getBandGain(int bandIndex) const;
    
    void setBandConfig(int bandIndex, float freq, float q, FilterType type);
    QVector<BandConfig> getBands() const;

    bool loadPreset(const QString &presetName);
    bool savePreset(const QString &presetName);
    QStringList getAvailablePresets() const;

    // Thread-safe high-performance audio processing pipeline
    void processAudio(AudioBuffer &buffer);

signals:
    void equalizerChanged();
    void presetLoaded(const QString &presetName);
    void errorOccurred(const QString &errorMessage);

private:
    struct BiquadCoefficients {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
    };

    struct FilterState {
        float z1 = 0.0f;
        float z2 = 0.0f;
    };

    void recalculateCoefficients(int bandIndex);
    void updateAllCoefficients(int sampleRate);
    void loadDefaultBands();
    void ensurePresetsDirExists();
    QString getPresetFilePath(const QString &presetName) const;

    mutable QMutex m_mutex; // Protects configuration structures across threads
    QVector<BandConfig> m_bands;
    QVector<BiquadCoefficients> m_coefficients; // Per-channel serialized structures
    
    // History states tracking: m_states[bandIndex][channelIndex]
    QVector<QVector<FilterState>> m_states; 
    
    int m_currentSampleRate;
    QString m_presetsDirectory;
};

#endif // ADVANCED_EQUALIZER_H