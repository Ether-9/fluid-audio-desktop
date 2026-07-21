cmake_minimum_required(VERSION 3.16)
project(NativeAudioEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable AUTOMOC for Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Python
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

# Find pybind11
find_package(pybind11 CONFIG REQUIRED)

# Find Qt6
find_package(Qt6 COMPONENTS Core Widgets Network Sql REQUIRED)

# Find RubberBand
find_package(PkgConfig REQUIRED)
pkg_check_modules(RUBBERBAND REQUIRED rubberband)

# Create the module
pybind11_add_module(native_audio
    # Source files
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

# Include directories
target_include_directories(native_audio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Python3_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(native_audio PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    ${RUBBERBAND_LIBRARIES}
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(native_audio PRIVATE ole32)
elseif(APPLE)
    target_link_libraries(native_audio PRIVATE 
        "-framework CoreAudio" 
        "-framework AudioToolbox"
        "-framework CoreFoundation"
    )
else()
    target_link_libraries(native_audio PRIVATE pthread dl m)
endif()

# Set output directory
set_target_properties(native_audio PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
