cmake_minimum_required(VERSION 3.15)
project(NativeAudioEngine)

# Enable automatic processing of Qt files (moc, uic, rcc)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 1. Find Python and Pybind11
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 REQUIRED)

# 2. Find Qt6 components
find_package(Qt6 COMPONENTS Core Widgets Network Sql REQUIRED)

# 3. Add current directory for header lookup
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# 4. Find RubberBand
find_package(PkgConfig REQUIRED)
pkg_check_modules(RUBBERBAND REQUIRED rubberband)

# 5. Create the Python Extension Module with ALL files
pybind11_add_module(native_audio 
    # ALL Header Files (Required for AUTOMOC)
    AdvancedEqualizer.h
    AlbumArtDownloader.h
    AlbumArtWidget.h
    AudioEngine.h
    CrossfadeEngine.h
    LyricsDisplay.h
    MainWindow.h
    MusicDatabase.h
    PitchShifter.h
    ReplayGain.h
    SmartPlaylist.h
    SocialSharing.h
    ThemeManager.h
    WaveformWidget.h

    # ALL Source Files
    AdvancedEqualizer.cpp
    AlbumArtDownloader.cpp
    AlbumArtWidget.cpp
    AudioEngine.cpp
    CrossfadeEngine.cpp
    LyricsDisplay.cpp
    MainWindow.cpp
    MusicDatabase.cpp
    native_audio.cpp
    PitchShifter.cpp
    ReplayGain.cpp
    SmartPlaylist.cpp
    SocialSharing.cpp
    ThemeManager.cpp
    WaveformWidget.cpp
)

# 6. Link libraries
target_link_libraries(native_audio PRIVATE 
    RUBBERBAND::rubberband
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
)

# Platform-specific libraries
if (WIN32)
    target_link_libraries(native_audio PRIVATE ole32) # Required by miniaudio
elseif (APPLE)
    target_link_libraries(native_audio PRIVATE "-framework CoreAudio" "-framework AudioToolbox")
elseif (UNIX)
    target_link_libraries(native_audio PRIVATE pthread dl m)
endif()

# 7. Output the compiled native_audio.pyd directly next to your Python files
set_target_properties(native_audio PROPERTIES 
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
