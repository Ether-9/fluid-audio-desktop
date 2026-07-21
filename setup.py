from setuptools import setup, Extension
import pybind11
import os

# ALL your C++ source files
cpp_sources = [
    'AdvancedEqualizer.cpp',
    'AlbumArtDownloader.cpp',
    'AlbumArtWidget.cpp',
    'AudioEngine.cpp',
    'CrossfadeEngine.cpp',
    'LyricsDisplay.cpp',
    'MainWindow.cpp',
    'MusicDatabase.cpp',
    'native_audio.cpp',
    'PitchShifter.cpp',
    'ReplayGain.cpp',
    'SmartPlaylist.cpp',
    'SocialSharing.cpp',
    'ThemeManager.cpp',
    'WaveformWidget.cpp'
]

native_module = Extension(
    'native_audio',
    sources=cpp_sources,
    include_dirs=[
        pybind11.get_include(),
        '.',
    ],
    language='c++',
    extra_compile_args=['-std=c++17', '-O3'],
    libraries=['rubberband'],  # Link RubberBand library
)

setup(
    name='native_audio',
    ext_modules=[native_module],
    zip_safe=False,
)
