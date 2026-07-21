#include "AudioEngine.h"

// Guard pybind11 includes from Qt's "slots" keyword conflict
#if defined(slots)
#  define QT_RE_DEFINE_SLOTS
#  pragma push_macro("slots")
#  undef slots
#endif

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#if defined(QT_RE_DEFINE_SLOTS)
#  pragma pop_macro("slots")
#  undef QT_RE_DEFINE_SLOTS
#endif

#include <QWidget>
#include <QString>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "PitchShifter.h" 

#include <iostream>
#include <QMutexLocker>
#include <deque>
#include <algorithm>
#include <string>
#include <chrono>
#include <iostream>

class Engine {
private:
    bool is_premium_unlocked = false;
    bool is_trial_active = true;
    std::string secret_app_hash = "FLUID_AUDIO_SECURE_HASH_2026";

public:
    Engine() : is_premium_unlocked(false), is_trial_active(true) {}

    // Unlock permanently via Lemon Squeezy Key
    bool unlock_premium(const std::string& passed_hash) {
        if (passed_hash == secret_app_hash) {
            is_premium_unlocked = true;
            return true;
        }
        return false;
    }

    // Set trial status from Python calculation
    void set_trial_status(bool active) {
        is_trial_active = active;
    }

    bool can_play_audio() {
        return is_premium_unlocked || is_trial_active;
    }

    void processAudio() {
        if (is_premium_unlocked) {
            std::cout << "[PREMIUM] Processing full frequency shift." << std::endl;
        } else if (is_trial_active) {
            std::cout << "[TRIAL ACTIVE] Processing full audio (Free Trial)." << std::endl;
        } else {
            std::cout << "[EXPIRED] Trial ended. Please enter your Lemon Squeezy license key." << std::endl;
            return; // Block processing or mute audio
        }
    }
};

namespace py = pybind11;

static std::deque<std::string> playbackQueue;

// The streaming feed-retrieve callback loop that completely removes static underruns
void AudioEngine::data_callback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount) {
    AudioEngine* engine = static_cast<AudioEngine*>(pDevice->pUserData);
    if (!engine || !engine->isDecoderInitialized) return;

    QMutexLocker lock(&engine->audioMutex);

    float* outBuffer = static_cast<float*>(pOutput);
    int channels = pDevice->playback.channels;
    int framesWritten = 0;

    // Loop until we fully satisfy miniaudio's hardware output request
    while (framesWritten < static_cast<int>(frameCount)) {
        int available = engine->pPitchShifter->getAvailableFrames();

        // 1. If we have enough frames in our FIFO to satisfy the rest of the buffer
        if (available >= static_cast<int>(frameCount) - framesWritten) {
            int written = engine->pPitchShifter->retrieve(outBuffer + (framesWritten * channels), 
                                                          static_cast<int>(frameCount) - framesWritten);
            framesWritten += written;
            break;
        }

        // 2. Otherwise, drain the FIFO's contents first
        if (available > 0) {
            int written = engine->pPitchShifter->retrieve(outBuffer + (framesWritten * channels), available);
            framesWritten += written;
        }

        // 3. Read a solid block size to keep Rubber Band's internal window filters happy
        // and completely prevent the tight loop "tatatatatata" frame starving stutter
        ma_uint32 standardBlockSize = 1024; 
        size_t totalSamplesNeeded = standardBlockSize * channels;
        
        // [UPDATE]: Because we pre-allocated dspInputBuffer in loadSong(), this condition 
        // will safely adapt without allocating new memory in the real-time thread.
        if (engine->dspInputBuffer.size() < totalSamplesNeeded) {
            engine->dspInputBuffer.resize(totalSamplesNeeded);
        }

        ma_uint64 framesRead = 0;
        ma_result result = ma_decoder_read_pcm_frames(engine->pDecoder, 
                                                      engine->dspInputBuffer.data(), 
                                                      standardBlockSize, 
                                                      &framesRead);

        if (result == MA_SUCCESS && framesRead > 0) {
            engine->pPitchShifter->feed(engine->dspInputBuffer.data(), static_cast<int>(framesRead));
            engine->currentPlayPosition += framesRead;
        } else {
            // EOF: Align play position with duration frames to avoid timeline calculation issues
            engine->currentPlayPosition = engine->totalDurationFrames;

            // Pad any remaining empty frames of the callback with silence and break
            int remainingFrames = static_cast<int>(frameCount) - framesWritten;
            std::fill(outBuffer + (framesWritten * channels), 
                      outBuffer + (frameCount * channels), 
                      0.0f);
            framesWritten += remainingFrames;
            break;
        }
    }

    (void)pInput;
}

AudioEngine::AudioEngine(QObject* parent) : QObject(parent) {
    pDecoder = new ma_decoder();
    pDevice = new ma_device();
    pPitchShifter = new PitchShifter();
    targetFrequency = 440.0f; // Explicitly initialized baseline frequency
    currentPitchFactor = 1.0f;
}

AudioEngine::~AudioEngine() {
    if (isDeviceInitialized) {
        ma_device_stop(pDevice);
        ma_device_uninit(pDevice);
    }
    if (isDecoderInitialized) {
        ma_decoder_uninit(pDecoder);
    }
    delete pDecoder;
    delete pDevice;
    delete pPitchShifter;
}

void AudioEngine::setCrossfade(int seconds) {
    (void)seconds; 
}

void AudioEngine::loadSong(const std::string& filePath) {
    if (isDeviceInitialized) {
        ma_device_uninit(pDevice);
        isDeviceInitialized = false;
    }
    if (isDecoderInitialized) {
        ma_decoder_uninit(pDecoder);
        isDecoderInitialized = false;
    }

    ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 2, 44100); 
    ma_result result;

#ifdef _WIN32
    // Windows requires UTF-16 (wchar_t) string conversion to preserve file paths with emoji titles
    std::wstring wPath = QString::fromStdString(filePath).toStdWString();
    result = ma_decoder_init_file_w(wPath.c_str(), &decoderConfig, pDecoder);
#else
    // macOS / Linux handle UTF-8 std::string paths natively
    result = ma_decoder_init_file(filePath.c_str(), &decoderConfig, pDecoder);
#endif
        
    if (result != MA_SUCCESS) {
        return;
    }
    isDecoderInitialized = true;
    m_currentSong = QString::fromStdString(filePath);

    // Track frame positions
    ma_uint64 durationFrames = 0;
    ma_decoder_get_length_in_pcm_frames(pDecoder, &durationFrames);
    totalDurationFrames = static_cast<int64_t>(durationFrames);
    currentPlayPosition = 0;

    // [UPDATE]: Pre-allocate the DSP buffer to prevent lock-stutters during high CPU load
    size_t maxSamplesNeeded = 4096 * pDecoder->outputChannels;
    dspInputBuffer.reserve(maxSamplesNeeded);
    dspInputBuffer.resize(maxSamplesNeeded);

    // Configure PitchShifter
    pPitchShifter->initStretcher(pDecoder->outputSampleRate, pDecoder->outputChannels);
    pPitchShifter->setTargetFrequency(targetFrequency, 440.0f);

    // Set up playback device
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = ma_format_f32;
    deviceConfig.playback.channels = pDecoder->outputChannels;
    deviceConfig.sampleRate        = pDecoder->outputSampleRate;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = this;

    // [UPDATE]: Hardware buffer tuning. 
    // Increasing period size and using 3 periods (triple buffering) absorbs 
    // OS scheduling delays from ACPI interrupts (plugging in charger) & CPU load.
    deviceConfig.periodSizeInFrames = 2048; 
    deviceConfig.periods = 3; 

    if (ma_device_init(NULL, &deviceConfig, pDevice) != MA_SUCCESS) {
        ma_decoder_uninit(pDecoder);
        isDecoderInitialized = false;
        return;
    }
    isDeviceInitialized = true;
}

void AudioEngine::play() {
    if (isDeviceInitialized) {
        ma_device_start(pDevice);
        m_isPlaying = true;
    }
}

void AudioEngine::pause() {
    if (isDeviceInitialized) {
        ma_device_stop(pDevice);
        m_isPlaying = false;
    }
}

void AudioEngine::stop() {
    if (isDeviceInitialized) {
        ma_device_stop(pDevice);
        m_isPlaying = false;
        QMutexLocker lock(&audioMutex);
        ma_decoder_seek_to_pcm_frame(pDecoder, 0);
        pPitchShifter->reset();
        currentPlayPosition = 0;
    }
}

void AudioEngine::setFrequency(int freq) {
    QMutexLocker lock(&audioMutex);
    targetFrequency = static_cast<float>(freq);
    currentPitchFactor = targetFrequency / 440.0f;
    pPitchShifter->setTargetFrequency(targetFrequency, 440.0f);
}

double AudioEngine::getDuration() {
    if (!isDecoderInitialized) return 0.0;
    ma_uint64 length;
    ma_decoder_get_length_in_pcm_frames(pDecoder, &length);
    return (double)length / pDecoder->outputSampleRate;
}

double AudioEngine::getPosition() {
    if (!isDecoderInitialized) return 0.0;
    ma_uint64 cursor;
    ma_decoder_get_cursor_in_pcm_frames(pDecoder, &cursor);
    return (double)cursor / pDecoder->outputSampleRate;
}

void AudioEngine::seek(float seconds) {
    QMutexLocker lock(&audioMutex);
    if (isDecoderInitialized) {
        ma_uint64 targetFrame = static_cast<ma_uint64>(seconds * pDecoder->outputSampleRate);
        ma_decoder_seek_to_pcm_frame(pDecoder, targetFrame);
        pPitchShifter->reset();
        currentPlayPosition = targetFrame;
    }
}

void AudioEngine::setReplayGain(bool enabled) { (void)enabled; }
void AudioEngine::setEQEnabled(bool enabled) { (void)enabled; }
void AudioEngine::setSeeking(bool isSeeking) { (void)isSeeking; }
void AudioEngine::setVolume(float volume) { (void)volume; }
void AudioEngine::setEQBand(int band, float gain) { (void)band; (void)gain; }

bool AudioEngine::isFinished() {
    return (currentPlayPosition >= totalDurationFrames);
}

void AudioEngine::insertIntoQueueNext(const std::string& filePath) {
    QMutexLocker lock(&audioMutex);
    playbackQueue.push_front(filePath);
}

void AudioEngine::appendToQueue(const std::string& filePath) {
    QMutexLocker lock(&audioMutex);
    playbackQueue.push_back(filePath);
}

void AudioEngine::clearQueue() {
    QMutexLocker lock(&audioMutex);
    playbackQueue.clear();
}

// =========================================================================
// pybind11 Custom Type Casters
// =========================================================================
namespace pybind11 { namespace detail {
    template <> struct type_caster<QString> {
    public:
        PYBIND11_TYPE_CASTER(QString, _("str"));
        bool load(handle src, bool) {
            if (!src) return false;
            PyObject* tmp = PyUnicode_AsUTF8String(src.ptr());
            if (!tmp) return false;
            value = QString::fromUtf8(PyBytes_AsString(tmp));
            Py_DECREF(tmp);
            return true;
        }
        static handle cast(const QString& src, return_value_policy, handle) {
            QByteArray bytes = src.toUtf8();
            return PyUnicode_FromStringAndSize(bytes.data(), bytes.size());
        }
    };
}} // namespace pybind11::detail

// =========================================================================
// Unified PYBIND11 Module Definitions (AudioEngine Only)
// =========================================================================
PYBIND11_MODULE(native_audio, m) {
    m.doc() = "Native Audio Engine Core Library";

    py::class_<AudioEngine>(m, "AudioEngine")
        .def(py::init([](py::object parent) {
            QObject* cpp_parent = nullptr;
            if (!parent.is_none()) {
                try {
                    py::module_ shiboken = py::module_::import("shiboken6");
                    py::tuple address_tuple = shiboken.attr("getCppPointer")(parent).cast<py::tuple>();
                    size_t address = address_tuple[0].cast<size_t>();
                    cpp_parent = reinterpret_cast<QObject*>(address);
                } catch (...) {
                    try {
                        cpp_parent = reinterpret_cast<QObject*>(parent.attr("winId")().cast<uintptr_t>());
                    } catch (...) {}
                }
            }
            return new AudioEngine(cpp_parent);
        }), py::arg("parent") = py::none())
        .def("load_song", &AudioEngine::loadSong)
        .def("play", &AudioEngine::play)
        .def("pause", &AudioEngine::pause)
        .def("stop", &AudioEngine::stop)
        .def("set_frequency", &AudioEngine::setFrequency)
        .def("get_duration", &AudioEngine::getDuration)
        .def("get_position", &AudioEngine::getPosition)
        .def("seek", &AudioEngine::seek)
        .def("set_replay_gain", &AudioEngine::setReplayGain)
        .def("set_eq_enabled", &AudioEngine::setEQEnabled)
        .def("set_seeking", &AudioEngine::setSeeking)
        .def("set_volume", &AudioEngine::setVolume)
        .def("set_eq_band", &AudioEngine::setEQBand)
        .def("insert_into_queue_next", &AudioEngine::insertIntoQueueNext)
        .def("append_to_queue", &AudioEngine::appendToQueue)
        .def("clear_queue", &AudioEngine::clearQueue)
        .def("is_finished", &AudioEngine::isFinished);
}

PYBIND11_MODULE(AudioEngine, m) {
    pybind11::class_<Engine>(m, "Engine")
        .def(pybind11::init<>())
        .def("unlock_premium", &Engine::unlock_premium)
        .def("set_trial_status", &Engine::set_trial_status)
        .def("can_play_audio", &Engine::can_play_audio)
        .def("processAudio", &Engine::processAudio);
}

