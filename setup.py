from setuptools import setup, Extension
import pybind11
import os

# ALL your C++ source files (16 files including native_audio.cpp)
cpp_sources = [
    'AdvancedEqualizer.cpp',
    'AlbumArtDownloader.cpp',
    'AlbumArtWidget.cpp',
    'AudioEngine.cpp',
    'CrossfadeEngine.cpp',
    'LyricsDisplay.cpp',
    'MainWindow.cpp',
    'MusicDatabase.cpp',
    'native_audio.cpp',          # ← IMPORTANT: Your pybind11 module
    'PitchShifter.cpp',
    'ReplayGain.cpp',
    'SmartPlaylist.cpp',
    'SocialSharing.cpp',
    'ThemeManager.cpp',
    'WaveformWidget.cpp'
]

# ALL your header files (15 headers including miniaudio.h)
header_files = [
    'AdvancedEqualizer.h',
    'AlbumArtDownloader.h',
    'AlbumArtWidget.h',
    'AudioEngine.h',
    'CrossfadeEngine.h',
    'LyricsDisplay.h',
    'MainWindow.h',
    'MusicDatabase.h',
    'miniaudio.h',      # ← 4.11 MB header - INCLUDED
    'PitchShifter.h',
    'ReplayGain.h',
    'SmartPlaylist.h',
    'SocialSharing.h',
    'ThemeManager.h',
    'WaveformWidget.h'
]

native_module = Extension(
    'native_audio',
    sources=cpp_sources,
    include_dirs=[
        pybind11.get_include(),
        '.',  # Current directory for headers
    ],
    language='c++',
    extra_compile_args=['-std=c++17', '-O3'],
    extra_link_args=[],
    libraries=['rubberband'],
)

setup(
    name='native_audio',
    ext_modules=[native_module],
    zip_safe=False,
    install_requires=[
        'pybind11',
        'PySide6',
        'rubberband',
        'requests',
        'keyring',
        'cryptography',
        'numpy',
    ],
)
