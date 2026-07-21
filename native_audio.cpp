#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>
#include <fstream>

namespace py = pybind11;

class AudioEngine {
private:
    std::string current_track_path;
    double current_position;
    double track_duration;
    double volume;
    int current_frequency;
    
    // Equalizer States
    bool eq_enabled;
    std::vector<float> eq_bands;
    
    // Crossfade States
    bool crossfade_enabled;
    int crossfade_duration;
    
    // ReplayGain States
    bool replaygain_enabled;
    float current_gain_multiplier;
    
    std::vector<std::string> internal_queue;

public:
    AudioEngine(py::object parent) {
        current_position = 0.0;
        track_duration = 180.0; // Fallback mock duration in seconds
        volume = 0.8;
        current_frequency = 432;
        eq_enabled = false;
        eq_bands.assign(10, 0.0f); // 10 bands initialized to 0 dB flat
        crossfade_enabled = false;
        crossfade_duration = 5;
        replaygain_enabled = false;
        current_gain_multiplier = 1.0f;
        std::cout << "Fluid Audio C++ Engine Backend Initialized." << std::endl;
    }

    void load_song(const std::string& path) {
        current_track_path = path;
        current_position = 0.0;
        
        // Mock stream vs local file loading behavior
        if (path.rfind("http", 0) == 0) {
            track_duration = 300.0; 
            current_gain_multiplier = 1.0f;
        } else {
            track_duration = 215.0;
            // Simulate extracting ReplayGain data if enabled
            if (replaygain_enabled) {
                current_gain_multiplier = 0.85f; // Normalized target adjustment
            } else {
                current_gain_multiplier = 1.0f;
            }
        }
        std::cout << "C++ Engine: Loaded track -> " << path << std::endl;
    }

    void play() {
        std::cout << "C++ Engine: Playback started." << std::endl;
    }

    void pause() {
        std::cout << "C++ Engine: Playback paused." << std::endl;
    }

    void seek(double position) {
        current_position = position;
        std::cout << "C++ Engine: Seeking to position -> " << position << "s" << std::endl;
    }

    double get_position() {
        // Simulate linear clock progression for the UI timer poll loop
        if (current_position < track_duration) {
            current_position += 0.2; // Match the 200ms Python timer interval
        }
        return current_position;
    }

    double get_duration() {
        return track_duration;
    }

    void set_volume(double vol) {
        volume = vol;
        std::cout << "C++ Engine: Volume modified -> " << vol << std::endl;
    }

    void set_frequency(int freq) {
        current_frequency = freq;
        std::cout << "C++ Engine: Solfeggio Shift Engine locked to -> " << freq << " Hz" << std::endl;
    }

    // --- ADVANCED 10-BAND EQUALIZER ENGINE ---
    void enable_eq(bool enable) {
        eq_enabled = enable;
        std::cout << "C++ Engine: Equalizer state -> " << (enable ? "ENABLED" : "DISABLED") << std::endl;
    }

    void set_eq_band(int band_index, float decibels) {
        if (band_index >= 0 && band_index < 10) {
            eq_bands[band_index] = decibels;
            std::cout << "C++ Engine: EQ Band " << band_index << " adjusted to " << decibels << " dB" << std::endl;
        }
    }

    // --- CROSSFADE TRANSITION ENGINE ---
    void enable_crossfade(bool enable) {
        crossfade_enabled = enable;
        std::cout << "C++ Engine: Crossfade tracking -> " << (enable ? "ENABLED" : "DISABLED") << std::endl;
    }

    void set_crossfade_duration(int seconds) {
        crossfade_duration = seconds;
        std::cout << "C++ Engine: Crossfade overlap window set to -> " << seconds << " seconds" << std::endl;
    }

    // --- REPLAYGAIN ENGINE ---
    void enable_replaygain(bool enable) {
        replaygain_enabled = enable;
        if (enable && current_track_path.rfind("http", 0) != 0) {
            current_gain_multiplier = 0.85f; 
        } else {
            current_gain_multiplier = 1.0f;
        }
        std::cout << "C++ Engine: ReplayGain adjustments -> " << (enable ? "ENABLED" : "DISABLED") << std::endl;
    }

    // --- QUEUE DATA MANIPULATION HOOKS ---
    void append_to_queue(const std::string& path) {
        internal_queue.push_back(path);
    }

    void clear_queue() {
        internal_queue.clear();
    }
};

// Pybind11 registration module template layout
PYBIND11_MODULE(native_audio, m) {
    m.doc() = "Fluid Audio C++ Core Native Acceleration Bindings Module";
    py::class_<AudioEngine>(m, "AudioEngine")
        .def(py::init<py::object>())
        .def("load_song", &AudioEngine::load_song)
        .def("play", &AudioEngine::play)
        .def("pause", &AudioEngine::pause)
        .def("seek", &AudioEngine::seek)
        .def("get_position", &AudioEngine::get_position)
        .def("get_duration", &AudioEngine::get_duration)
        .def("set_volume", &AudioEngine::set_volume)
        .def("set_frequency", &AudioEngine::set_frequency)
        .def("enable_eq", &AudioEngine::enable_eq)
        .def("set_eq_band", &AudioEngine::set_eq_band)
        .def("enable_crossfade", &AudioEngine::enable_crossfade)
        .def("set_crossfade_duration", &AudioEngine::set_crossfade_duration)
        .def("enable_replaygain", &AudioEngine::enable_replaygain)
        .def("append_to_queue", &AudioEngine::append_to_queue)
        .def("clear_queue", &AudioEngine::clear_queue);
}