#include "AdvancedEqualizer.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QStandardPaths>
#include <QSaveFile>
#include <QtMath>
#include <cmath>

AdvancedEqualizer::AdvancedEqualizer(QWidget *parent)
    : QWidget(parent), m_currentSampleRate(44100) {
    
    m_presetsDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/presets";
    ensurePresetsDirExists();
    
    loadDefaultBands();
    updateAllCoefficients(m_currentSampleRate);
}

AdvancedEqualizer::~AdvancedEqualizer() {}

void AdvancedEqualizer::loadDefaultBands() {
    QMutexLocker locker(&m_mutex);
    const QVector<float> defaultFreqs = {32.0f, 64.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f};
    
    m_bands.clear();
    m_coefficients.clear();
    m_states.clear();

    for (float freq : defaultFreqs) {
        BandConfig band{freq, 0.0f, 1.0f, FilterType::Peaking};
        m_bands.append(band);
        m_coefficients.append(BiquadCoefficients());
        m_states.append(QVector<FilterState>()); // Dynamically sized on processAudio channels
    }
}

void AdvancedEqualizer::setBandGain(int bandIndex, float gainDb) {
    {
        QMutexLocker locker(&m_mutex);
        if (bandIndex < 0 || bandIndex >= m_bands.size()) return;
        m_bands[bandIndex].gain = qBound(-24.0f, gainDb, 24.0f);
        recalculateCoefficients(bandIndex);
    }
    emit equalizerChanged();
}

float AdvancedEqualizer::getBandGain(int bandIndex) const {
    QMutexLocker locker(&m_mutex);
    if (bandIndex < 0 || bandIndex >= m_bands.size()) return 0.0f;
    return m_bands[bandIndex].gain;
}

void AdvancedEqualizer::setBandConfig(int bandIndex, float freq, float q, FilterType type) {
    {
        QMutexLocker locker(&m_mutex);
        if (bandIndex < 0 || bandIndex >= m_bands.size()) return;
        m_bands[bandIndex].frequency = qBound(20.0f, freq, 22000.0f);
        m_bands[bandIndex].qFactor = qBound(0.1f, q, 10.0f);
        m_bands[bandIndex].type = type;
        recalculateCoefficients(bandIndex);
    }
    emit equalizerChanged();
}

QVector<AdvancedEqualizer::BandConfig> AdvancedEqualizer::getBands() const {
    QMutexLocker locker(&m_mutex);
    return m_bands;
}

void AdvancedEqualizer::recalculateCoefficients(int bandIndex) {
    // Expects caller to hold m_mutex lock context
    if (bandIndex < 0 || bandIndex >= m_bands.size()) return;

    const auto &band = m_bands[bandIndex];
    auto &coeff = m_coefficients[bandIndex];

    float omega = 2.0f * M_PI * band.frequency / static_cast<float>(m_currentSampleRate);
    float sinOmega = std::sin(omega);
    float cosOmega = std::cos(omega);
    float alpha = sinOmega / (2.0f * band.qFactor);
    float A = std::pow(10.0f, band.gain / 40.0f);

    float a0 = 1.0f;

    switch (band.type) {
        case Peaking: {
            coeff.b0 = 1.0f + alpha * A;
            coeff.b1 = -2.0f * cosOmega;
            coeff.b2 = 1.0f - alpha * A;
            a0       = 1.0f + alpha / A;
            coeff.a1 = -2.0f * cosOmega;
            coeff.a2 = 1.0f - alpha / A;
            break;
        }
        case LowPass: {
            coeff.b0 = (1.0f - cosOmega) / 2.0f;
            coeff.b1 = 1.0f - cosOmega;
            coeff.b2 = (1.0f - cosOmega) / 2.0f;
            a0       = 1.0f + alpha;
            coeff.a1 = -2.0f * cosOmega;
            coeff.a2 = 1.0f - alpha;
            break;
        }
        case HighPass: {
            coeff.b0 = (1.0f + cosOmega) / 2.0f;
            coeff.b1 = -(1.0f + cosOmega);
            coeff.b2 = (1.0f + cosOmega) / 2.0f;
            a0       = 1.0f + alpha;
            coeff.a1 = -2.0f * cosOmega;
            coeff.a2 = 1.0f - alpha;
            break;
        }
        default: // Fallback safety
            coeff.b0 = 1.0f; coeff.b1 = 0.0f; coeff.b2 = 0.0f;
            a0 = 1.0f; coeff.a1 = 0.0f; coeff.a2 = 0.0f;
            break;
    }

    // Normalize coefficients safely
    if (std::abs(a0) > 1e-6f) {
        coeff.b0 /= a0; coeff.b1 /= a0; coeff.b2 /= a0;
        coeff.a1 /= a0; coeff.a2 /= a0;
    }
}

void AdvancedEqualizer::updateAllCoefficients(int sampleRate) {
    m_currentSampleRate = sampleRate;
    for (int i = 0; i < m_bands.size(); ++i) {
        recalculateCoefficients(i);
    }
}

void AdvancedEqualizer::processAudio(AudioBuffer &buffer) {
    QMutexLocker locker(&m_mutex);
    
    if (buffer.samples.isEmpty() || buffer.channels <= 0) return;

    // Step down parameters safely if sample-rate conversion shifts mid-playback
    if (buffer.sampleRate != m_currentSampleRate && buffer.sampleRate > 0) {
        updateAllCoefficients(buffer.sampleRate);
    }

    int totalBands = m_bands.size();
    int channels = buffer.channels;
    int numFrames = buffer.samples.size() / channels;

    // Prevent cross-channel drift memory leaking by aligning filter matrices
    for (int b = 0; b < totalBands; ++b) {
        if (m_states[b].size() < channels) {
            m_states[b].resize(channels);
        }
    }

    // High performance localized floating point calculations
    const float denormalThreshold = 1e-25f;

    for (int b = 0; b < totalBands; ++b) {
        const auto &c = m_coefficients[b];
        auto &channelStates = m_states[b];

        for (int ch = 0; ch < channels; ++ch) {
            auto &state = channelStates[ch];
            
            for (int s = 0; s < numFrames; ++s) {
                int index = s * channels + ch;
                if (index >= buffer.samples.size()) break;

                float input = buffer.samples[index];
                
                // Direct Form II Transposed implementation loop
                float output = c.b0 * input + state.z1;
                state.z1 = c.b1 * input - c.a1 * output + state.z2;
                state.z2 = c.b2 * input - c.a2 * output;

                // Crash Mitigation: Anti-denormal treatment avoids microcode overhead
                if (std::abs(state.z1) < denormalThreshold) state.z1 = 0.0f;
                if (std::abs(state.z2) < denormalThreshold) state.z2 = 0.0f;

                buffer.samples[index] = output;
            }
        }
    }
}

bool AdvancedEqualizer::savePreset(const QString &presetName) {
    QMutexLocker locker(&m_mutex);
    if (presetName.isEmpty()) return false;

    QJsonArray bandsArray;
    for (const auto &band : m_bands) {
        QJsonObject bObj;
        bObj["frequency"] = band.frequency;
        bObj["gain"] = band.gain;
        bObj["qFactor"] = band.qFactor;
        bObj["type"] = static_cast<int>(band.type);
        bandsArray.append(bObj);
    }

    QJsonObject root;
    root["bands"] = bandsArray;

    // Failure Recovery Fix: Use QSaveFile to guard against power-cut truncation corruption
    QSaveFile file(getPresetFilePath(presetName));
    if (!file.open(QIODevice::WriteOnly)) {
        emit errorOccurred("Failed to open preset target file for structural saving.");
        return false;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson());
    return file.commit();
}

bool AdvancedEqualizer::loadPreset(const QString &presetName) {
    QMutexLocker locker(&m_mutex);
    QFile file(getPresetFilePath(presetName));
    if (!file.open(QIODevice::ReadOnly)) return false;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject()) return false;

    QJsonObject root = doc.object();
    QJsonArray bandsArray = root["bands"].toArray();
    
    int limit = qMin(bandsArray.size(), m_bands.size());
    for (int i = 0; i < limit; ++i) {
        QJsonObject bObj = bandsArray[i].toObject();
        m_bands[i].frequency = bObj["frequency"].toDouble();
        m_bands[i].gain = bObj["gain"].toDouble();
        m_bands[i].qFactor = bObj["qFactor"].toDouble();
        m_bands[i].type = static_cast<FilterType>(bObj["type"].toInt());
        recalculateCoefficients(i);
    }

    emit presetLoaded(presetName);
    emit equalizerChanged();
    return true;
}

QStringList AdvancedEqualizer::getAvailablePresets() const {
    QDir dir(m_presetsDirectory);
    return dir.entryList(QStringList() << "*.json", QDir::Files).replaceInStrings(".json", "");
}

void AdvancedEqualizer::ensurePresetsDirExists() {
    QDir dir(m_presetsDirectory);
    if (!dir.exists()) dir.mkpath(".");
}

QString AdvancedEqualizer::getPresetFilePath(const QString &presetName) const {
    return m_presetsDirectory + "/" + presetName + ".json";
}
